#include "Components/StormVerticalProfileToolComponent.h"
#include "SavageSuperStormRuntime.h"
#include "Assets/StormVerticalProfileAsset.h"
#include "Data/Profile/StormProfileUndoState.h"
#include "Shader_SavageSuperStorm.h"

#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "GlobalShader.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/Canvas.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "SceneInterface.h"
#include "TextureResource.h"
#include "RenderingThread.h"
#include "TextureCompiler.h"
#include "UObject/ConstructorHelpers.h"

#include "Actors/VolumetricSuperStormActor.h"

#if WITH_EDITOR
#include "HAL/IConsoleManager.h"
#include "Misc/TransactionObjectEvent.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectIterator.h"

namespace SavageSuperStorm
{
	static TAutoConsoleVariable<bool> CVarLogProfileParamChanges(
		TEXT("SavageStorm.Profile.LogParamChanges"),
		false,
		TEXT("Logs the change type and property of every profile param edit. Use to "
			 "see what a details-panel widget actually reports during a drag."));

	static TAutoConsoleVariable<int32> CVarProfileHistoryMaxDepth(
		TEXT("SavageStorm.Profile.History.MaxDepth"),
		128,
		TEXT("Completed edit nodes allowed from a profile checkpoint. A positive "
			 "limit schedules an undo barrier and prefix rebase when reached; <= 0 "
			 "disables the depth limit."));

	static TAutoConsoleVariable<int32> CVarProfileHistoryMaxReplayCost(
		TEXT("SavageStorm.Profile.History.MaxReplayCost"),
		1024,
		TEXT("Estimated replay passes allowed from a profile checkpoint. A positive "
			 "limit schedules an undo barrier and prefix rebase when reached; <= 0 "
			 "disables the replay-cost limit."));

	static TAutoConsoleVariable<int32> CVarProfileHistoryMaxEditReplayCost(
		TEXT("SavageStorm.Profile.History.MaxEditReplayCost"),
		256,
		TEXT("Estimated replay passes allowed in one internal profile edit chunk. "
			 "Long strokes split into several graph nodes inside the same transaction; "
			 "<= 0 disables chunking."));

	// Largest per-channel absolute difference between two same-size float render
	// targets. Accumulates into InOutMaxDelta so several surface pairs can share
	// one running maximum.
	static bool CompareProfileRenderTargets(
		UTextureRenderTarget2D* A,
		UTextureRenderTarget2D* B,
		float& InOutMaxDelta)
	{
		if (!A || !B || A->SizeX != B->SizeX || A->SizeY != B->SizeY)
		{
			return false;
		}

		FTextureRenderTargetResource* ResourceA =
			A->GameThread_GetRenderTargetResource();
		FTextureRenderTargetResource* ResourceB =
			B->GameThread_GetRenderTargetResource();
		if (!ResourceA || !ResourceB)
		{
			return false;
		}

		TArray<FFloat16Color> PixelsA;
		TArray<FFloat16Color> PixelsB;
		if (!ResourceA->ReadFloat16Pixels(PixelsA) ||
			!ResourceB->ReadFloat16Pixels(PixelsB) ||
			PixelsA.Num() != PixelsB.Num())
		{
			return false;
		}

		for (int32 Index = 0; Index < PixelsA.Num(); ++Index)
		{
			const FFloat16Color& PixelA = PixelsA[Index];
			const FFloat16Color& PixelB = PixelsB[Index];
			InOutMaxDelta = FMath::Max(InOutMaxDelta,
				FMath::Abs(static_cast<float>(PixelA.R) - static_cast<float>(PixelB.R)));
			InOutMaxDelta = FMath::Max(InOutMaxDelta,
				FMath::Abs(static_cast<float>(PixelA.G) - static_cast<float>(PixelB.G)));
			InOutMaxDelta = FMath::Max(InOutMaxDelta,
				FMath::Abs(static_cast<float>(PixelA.B) - static_cast<float>(PixelB.B)));
			InOutMaxDelta = FMath::Max(InOutMaxDelta,
				FMath::Abs(static_cast<float>(PixelA.A) - static_cast<float>(PixelB.A)));
		}
		return true;
	}

	// Asset saves update their embedded textures in place. History restore payloads
	// therefore need private snapshots or a later save would silently rewrite an
	// older Revert node's meaning.
	static UTexture2D* SnapshotProfileHistoryTexture(
		UTexture2D* Source,
		UObject* Outer)
	{
		if (!Source || !Outer)
		{
			return nullptr;
		}
		const FName SnapshotName = MakeUniqueObjectName(
			Outer,
			UTexture2D::StaticClass(),
			FName(TEXT("ProfileHistoryTexture")));
		UTexture2D* Snapshot = DuplicateObject<UTexture2D>(
			Source,
			Outer,
			SnapshotName);
		if (Snapshot)
		{
			Snapshot->SetFlags(RF_Transient);
			Snapshot->ClearFlags(
				RF_Public | RF_Standalone | RF_Transactional);
			Snapshot->UpdateResource();
		}
		return Snapshot;
	}
}
#endif // WITH_EDITOR

UStormVerticalProfileToolComponent::UStormVerticalProfileToolComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	static ConstructorHelpers::FObjectFinder<UCurveFloat> DefaultProfileCurve(
		TEXT("/SavageSuperStorm/VerticalProfile/Internal/"
			"CF_DefaultProfileCurve.CF_DefaultProfileCurve"));

	if (!IsProfileAssetComplete(PersistentProfileAsset) && DefaultProfileCurve.Succeeded())
	{
		DefaultProfileCurveTemplate = DefaultProfileCurve.Object;
		ResetProfileParamsToDefaults();
	}
}

void UStormVerticalProfileToolComponent::ResetProfileParamsToDefaults()
{
	StormProfileParams = FStormProfileParams();
	if (!DefaultProfileCurveTemplate)
	{
		return;
	}

	const FRichCurve& SourceCurve =
		DefaultProfileCurveTemplate->FloatCurve;
	StormProfileParams.VerticalProfileCurve.ExternalCurve = nullptr;
	StormProfileParams.VerticalProfileCurve.EditorCurveData = SourceCurve;
	StormProfileParams.AnvilProfileCurve.ExternalCurve = nullptr;
	StormProfileParams.AnvilProfileCurve.EditorCurveData = SourceCurve;
}

void UStormVerticalProfileToolComponent::OnRegister()
{
	Super::OnRegister();

#if WITH_EDITOR
	// The component remains transactional for ordinary editor properties. Profile
	// params are a nontransactional mirror; each key's current-state bookmark lives
	// on its own transactional UObject, while immutable history data stays in the
	// registry. An edit therefore snapshots neither prior ops nor other keys.
	if (GetWorld() && GetWorld()->WorldType == EWorldType::Editor)
	{
		SetFlags(RF_Transactional);
		EnsureEditUndoState();
	}
#endif

	EnsureProfilesInitialized();
}

bool UStormVerticalProfileToolComponent::EnsureProfilesInitialized()
{
	const int32 Resolution = FMath::Max<int32>(StormProfileParams.StormProfileResolution, 1);
	const bool bHasUsableTargets =
		BottomTypeProfileRT && TopTypeProfileRT && AnvilProfileRT && StashTopRT && StashAnvilRT &&
		BottomCurrentRT && TopCurrentRT && AnvilCurrentRT &&
		BottomTypeProfileRT->GetOuter() == this &&
		TopTypeProfileRT->GetOuter() == this &&
		AnvilProfileRT->GetOuter() == this &&
		StashTopRT->GetOuter() == this &&
		StashAnvilRT->GetOuter() == this &&
		BottomCurrentRT->GetOuter() == this &&
		TopCurrentRT->GetOuter() == this &&
		AnvilCurrentRT->GetOuter() == this &&
		BottomTypeProfileRT->SizeX == Resolution &&
		BottomTypeProfileRT->SizeY == Resolution &&
		TopTypeProfileRT->SizeX == Resolution &&
		TopTypeProfileRT->SizeY == Resolution &&
		AnvilProfileRT->SizeX == Resolution &&
		AnvilProfileRT->SizeY == Resolution &&
		StashTopRT->SizeX == Resolution &&
		StashTopRT->SizeY == Resolution &&
		StashAnvilRT->SizeX == Resolution &&
		StashAnvilRT->SizeY == Resolution &&
		BottomCurrentRT->SizeX == Resolution &&
		BottomCurrentRT->SizeY == Resolution &&
		TopCurrentRT->SizeX == Resolution &&
		TopCurrentRT->SizeY == Resolution &&
		AnvilCurrentRT->SizeX == Resolution &&
		AnvilCurrentRT->SizeY == Resolution;

	if (bProfilesInitialized && bHasUsableTargets)
	{
		return true;
	}

	RegenerateProfileRenderTargets();
	if (!GetWorld() || !BottomTypeProfileRT || !TopTypeProfileRT || !AnvilProfileRT)
	{
		return false;
	}

	const bool bHydratedFromAsset = HasCompletePersistentProfile();
	if (bHydratedFromAsset)
	{
		if (!SeedSurfacesFromAsset())
		{
			return false;
		}
	}
	else
	{
		// Without a complete persistent asset, parameters remain the safe
		// last-resort seed in every world.
		if (!GetWorld()->Scene)
		{
			return false;
		}
		BuildBottomTypeProfile();
		BuildTopTypeProfile(TopTypeProfileRT);
		BuildAnvilProfile();
	}

	bProfilesInitialized = true;

#if WITH_EDITOR
	if (GetWorld()->WorldType == EWorldType::Editor)
	{
		if (!bHydratedFromAsset)
		{
			// No asset key to seed from, so the parametric surfaces above are the
			// working set. Give them an identity now or nothing painted before the
			// first save would be recorded. The hydrated path anchors its own
			// origin inside SeedSurfacesFromKey.
			EnsureScratchWorkingSet();
		}
	}
#endif

	return true;
}

void UStormVerticalProfileToolComponent::BeginPlay()
{
	Super::BeginPlay();
	EnsureProfilesInitialized();
}

void UStormVerticalProfileToolComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	SetComponentTickEnabled(false);
	ProfileSequenceRuntime = FProfileSequenceRuntimeState();
#if WITH_EDITOR
	ResetKeyRenderTargets();
#endif
	TopTypeProfileRT	= nullptr;
	AnvilProfileRT		= nullptr;
	StashTopRT			= nullptr;
	StashAnvilRT		= nullptr;
	BottomTypeProfileRT = nullptr;
	BottomCurrentRT		= nullptr;
	TopCurrentRT		= nullptr;
	AnvilCurrentRT		= nullptr;
	bProfilesInitialized = false;
}

bool UStormVerticalProfileToolComponent::IsProfileAssetComplete(const UStormVerticalProfileAsset* ProfileAsset) const
{
	return ProfileAsset && ProfileAsset->IsKeyComplete(0);
}

bool UStormVerticalProfileToolComponent::ApplyProfileConfiguration(
	UStormVerticalProfileAsset* InProfileAsset,
	bool bNotifyOwner)
{
	if (InProfileAsset && !IsProfileAssetComplete(InProfileAsset))
	{
		return false;
	}

	Modify();
	ProfileSequenceRuntime = FProfileSequenceRuntimeState();
	SetComponentTickEnabled(false);

#if WITH_EDITOR
	// Applying a configuration is an explicit reset. Do not let editor working
	// sets from the previous application shadow the freshly applied asset. Give
	// the replacement document a new registry as well: old transactions can keep
	// the previous registry and its key states alive, but the new document must
	// have no route back to them.
	ResetKeyRenderTargets();
	EditUndoState = nullptr;
	EnsureEditUndoState();
#endif
	PersistentProfileAsset = InProfileAsset;
	if (PersistentProfileAsset)
	{
		// Resolution and parametric fallbacks must match the pixels being loaded.
		StormProfileParams =
			PersistentProfileAsset->GetPrimaryKey()->Params;
	}

	// Force a re-seed even when the same asset is reapplied to already valid RTs.
	bProfilesInitialized = false;
	EnsureProfilesInitialized();
	if (bNotifyOwner)
	{
		NotifyProfileRenderDataChanged();
	}
	return true;
}
bool UStormVerticalProfileToolComponent::PlayProfileSequence(bool bRestart)
{
	return StartProfileSequencePlayback(
		EStormProfileSequencePlaybackDirection::Forward,
		bRestart);
}

bool UStormVerticalProfileToolComponent::PlayProfileSequenceReverse(
	bool bRestart)
{
	return StartProfileSequencePlayback(
		EStormProfileSequencePlaybackDirection::Reverse,
		bRestart);
}

bool UStormVerticalProfileToolComponent::StartProfileSequencePlayback(
	EStormProfileSequencePlaybackDirection Direction,
	bool bRestart)
{
	if (!CanPlayProfileSequence() || !bProfilesInitialized)
	{
		return false;
	}
	const bool bWasPreviewActive =
		ProfileSequenceRuntime.bPreviewActive;

	const float Duration = GetProfileSequenceDuration();
	if (Direction == EStormProfileSequencePlaybackDirection::Forward)
	{
		if (bRestart ||
			ProfileSequenceRuntime.TimeSeconds >= Duration)
		{
			ProfileSequenceRuntime.TimeSeconds = 0.0f;
		}
	}
	else if (bRestart ||
		ProfileSequenceRuntime.TimeSeconds <= 0.0f)
	{
		ProfileSequenceRuntime.TimeSeconds = Duration;
	}
	if (!ProfileSequenceRuntime.bHasLoopingOverride)
	{
		ProfileSequenceRuntime.bLooping =
			PersistentProfileAsset->bLoopByDefault;
	}
	ProfileSequenceRuntime.bPreviewActive = true;
	ProfileSequenceRuntime.Direction = Direction;
	ProfileSequenceRuntime.State =
		EStormProfileSequencePlaybackState::Playing;

#if WITH_EDITOR
	if (!bWasPreviewActive)
	{
		TArray<UTexture*> SequenceTextures;
		for (const FStormProfileKey& Key : PersistentProfileAsset->Keys)
		{
			SequenceTextures.Add(Key.BottomProfile.Get());
			SequenceTextures.Add(Key.TopProfile.Get());
			SequenceTextures.Add(Key.AnvilProfile.Get());
		}
		FTextureCompilingManager::Get().FinishCompilation(
			SequenceTextures);
	}
#endif

	if (!ComposeProfileSequenceAtCurrentTime())
	{
		ResetProfileSequenceRuntime(/*bResetTime=*/false);
		UpdatePlaybackTickEnabled();
		return false;
	}

	UpdatePlaybackTickEnabled();
	NotifyProfileRenderDataChanged();
	return true;
}

void UStormVerticalProfileToolComponent::PauseProfileSequence()
{
	if (ProfileSequenceRuntime.bPreviewActive)
	{
		ProfileSequenceRuntime.State =
			EStormProfileSequencePlaybackState::Paused;
	}
	UpdatePlaybackTickEnabled();
}

void UStormVerticalProfileToolComponent::StopProfileSequence()
{
	const bool bWasPreviewing = ProfileSequenceRuntime.bPreviewActive;
	ResetProfileSequenceRuntime(/*bResetTime=*/true);
	UpdatePlaybackTickEnabled();
	if (bWasPreviewing)
	{
		NotifyProfileRenderDataChanged();
	}
}

bool UStormVerticalProfileToolComponent::SetProfileSequenceTime(
	float InTimeSeconds)
{
	if (!CanPlayProfileSequence() || !bProfilesInitialized ||
		!FMath::IsFinite(InTimeSeconds))
	{
		return false;
	}
	const bool bWasPreviewActive =
		ProfileSequenceRuntime.bPreviewActive;

	ProfileSequenceRuntime.TimeSeconds = FMath::Clamp(
		InTimeSeconds,
		0.0f,
		GetProfileSequenceDuration());
	if (!ProfileSequenceRuntime.bHasLoopingOverride)
	{
		ProfileSequenceRuntime.bLooping =
			PersistentProfileAsset->bLoopByDefault;
	}
	ProfileSequenceRuntime.bPreviewActive = true;
	ProfileSequenceRuntime.State =
		EStormProfileSequencePlaybackState::Paused;

#if WITH_EDITOR
	if (!bWasPreviewActive)
	{
		TArray<UTexture*> SequenceTextures;
		for (const FStormProfileKey& Key : PersistentProfileAsset->Keys)
		{
			SequenceTextures.Add(Key.BottomProfile.Get());
			SequenceTextures.Add(Key.TopProfile.Get());
			SequenceTextures.Add(Key.AnvilProfile.Get());
		}
		FTextureCompilingManager::Get().FinishCompilation(
			SequenceTextures);
	}
#endif

	if (!ComposeProfileSequenceAtCurrentTime())
	{
		ResetProfileSequenceRuntime(/*bResetTime=*/false);
		UpdatePlaybackTickEnabled();
		return false;
	}

	UpdatePlaybackTickEnabled();
	NotifyProfileRenderDataChanged();
	return true;
}

void UStormVerticalProfileToolComponent::SetProfileSequenceLooping(
	bool bLooping)
{
	ProfileSequenceRuntime.bLooping = bLooping;
	ProfileSequenceRuntime.bHasLoopingOverride = true;
}

float UStormVerticalProfileToolComponent::GetProfileSequenceTime() const
{
	return FMath::Clamp(
		ProfileSequenceRuntime.TimeSeconds,
		0.0f,
		GetProfileSequenceDuration());
}

float UStormVerticalProfileToolComponent::GetProfileSequenceDuration() const
{
	return PersistentProfileAsset
		? PersistentProfileAsset->GetDurationSeconds()
		: 0.0f;
}

EStormProfileSequencePlaybackState
UStormVerticalProfileToolComponent::GetProfileSequencePlaybackState() const
{
	return ProfileSequenceRuntime.State;
}

EStormProfileSequencePlaybackDirection
UStormVerticalProfileToolComponent::GetProfileSequencePlaybackDirection() const
{
	return ProfileSequenceRuntime.Direction;
}

bool UStormVerticalProfileToolComponent::IsProfileSequencePlaying() const
{
	return ProfileSequenceRuntime.State ==
		EStormProfileSequencePlaybackState::Playing;
}

bool UStormVerticalProfileToolComponent::IsProfileSequencePreviewActive() const
{
	return ProfileSequenceRuntime.bPreviewActive;
}

bool UStormVerticalProfileToolComponent::CanPlayProfileSequence() const
{
	return PersistentProfileAsset &&
		PersistentProfileAsset->IsSequenceComplete() &&
		PersistentProfileAsset->GetKeyCount() > 1 &&
		PersistentProfileAsset->GetDurationSeconds() > KINDA_SMALL_NUMBER;
}

void UStormVerticalProfileToolComponent::GetProfileRenderData(
	FStormProfileRenderData& OutRenderData) const
{
	OutRenderData = FStormProfileRenderData();

	// Without sequence preview, the cloud samples the live authoring surfaces.
	UTexture* BottomDisplay = BottomTypeProfileRT.Get();
	UTexture* TopDisplay    = TopTypeProfileRT.Get();
	UTexture* AnvilDisplay  = AnvilProfileRT.Get();

	// During sequence preview/playback, hand the cloud the pre-composed surfaces.
	if (IsSamplingCompositedProfile())
	{
		BottomDisplay = BottomCurrentRT.Get();
		TopDisplay    = TopCurrentRT.Get();
		AnvilDisplay  = AnvilCurrentRT.Get();
	}

	OutRenderData.BottomProfile = BottomDisplay;
	OutRenderData.TopProfile    = TopDisplay;
	OutRenderData.AnvilProfile  = AnvilDisplay;
}

bool UStormVerticalProfileToolComponent::IsSamplingCompositedProfile() const
{
	return BottomCurrentRT && TopCurrentRT && AnvilCurrentRT &&
		ProfileSequenceRuntime.bPreviewActive;
}

bool UStormVerticalProfileToolComponent::ResolveProfileSequenceKeySurfaces(
	int32 KeyIndex,
	FProfileSurfaceSet& OutSurfaces) const
{
	OutSurfaces = FProfileSurfaceSet();
	if (!PersistentProfileAsset ||
		!PersistentProfileAsset->Keys.IsValidIndex(KeyIndex))
	{
		return false;
	}

	const FStormProfileKey& Key =
		PersistentProfileAsset->Keys[KeyIndex];

#if WITH_EDITOR
	UTextureRenderTarget2D* BottomRT = nullptr;
	UTextureRenderTarget2D* TopRT = nullptr;
	UTextureRenderTarget2D* AnvilRT = nullptr;
	FStormProfileParams Params;
	if (GetKeyRenderTargetState(
			Key.KeyId,
			BottomRT,
			TopRT,
			AnvilRT,
			Params))
	{
		OutSurfaces.Bottom = BottomRT;
		OutSurfaces.Top = TopRT;
		OutSurfaces.Anvil = AnvilRT;
		return OutSurfaces.IsComplete();
	}
#endif

	OutSurfaces.Bottom = Key.BottomProfile.Get();
	OutSurfaces.Top = Key.TopProfile.Get();
	OutSurfaces.Anvil = Key.AnvilProfile.Get();
	return OutSurfaces.IsComplete();
}

bool UStormVerticalProfileToolComponent::ComposeProfileSurfaces(
	const FProfileSurfaceSet& From,
	const FProfileSurfaceSet& To,
	float Alpha)
{
	UWorld* World = GetWorld();
	if (!From.IsComplete() || !To.IsComplete() ||
		!World || !World->Scene ||
		!BottomCurrentRT || !TopCurrentRT || !AnvilCurrentRT)
	{
		return false;
	}

	auto GetSourceResource = [](UTexture* Texture) -> FTextureResource*
	{
		if (UTextureRenderTarget2D* RenderTarget =
				Cast<UTextureRenderTarget2D>(Texture))
		{
			return RenderTarget->GameThread_GetRenderTargetResource();
		}
		return Texture ? Texture->GetResource() : nullptr;
	};

	FTextureResource* BottomAResource = GetSourceResource(From.Bottom);
	FTextureResource* TopAResource = GetSourceResource(From.Top);
	FTextureResource* AnvilAResource = GetSourceResource(From.Anvil);
	FTextureResource* BottomBResource = GetSourceResource(To.Bottom);
	FTextureResource* TopBResource = GetSourceResource(To.Top);
	FTextureResource* AnvilBResource = GetSourceResource(To.Anvil);
	FTextureRenderTargetResource* BottomCurResource = BottomCurrentRT->GameThread_GetRenderTargetResource();
	FTextureRenderTargetResource* TopCurResource    = TopCurrentRT->GameThread_GetRenderTargetResource();
	FTextureRenderTargetResource* AnvilCurResource  = AnvilCurrentRT->GameThread_GetRenderTargetResource();
	if (!BottomAResource || !TopAResource || !AnvilAResource ||
		!BottomCurResource || !TopCurResource || !AnvilCurResource ||
		!BottomBResource || !TopBResource || !AnvilBResource)
	{
		return false;
	}

	const uint32 Resolution = static_cast<uint32>(BottomCurrentRT->SizeX);
	const float ClampedAlpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
	const ERHIFeatureLevel::Type FeatureLevel = World->Scene->GetFeatureLevel();

	ENQUEUE_RENDER_COMMAND(BlendStormProfilesRT)(
		[BottomAResource, TopAResource, AnvilAResource,
		 BottomCurResource, TopCurResource, AnvilCurResource,
		 BottomBResource, TopBResource, AnvilBResource,
		 Resolution, ClampedAlpha, FeatureLevel]
		(FRHICommandListImmediate& RHICmdList)
		{
			FRHITexture* BottomARHI   = BottomAResource->GetTextureRHI();
			FRHITexture* TopARHI      = TopAResource->GetTextureRHI();
			FRHITexture* AnvilARHI    = AnvilAResource->GetTextureRHI();
			FRHITexture* BottomCurRHI = BottomCurResource->GetRenderTargetTexture();
			FRHITexture* TopCurRHI    = TopCurResource->GetRenderTargetTexture();
			FRHITexture* AnvilCurRHI  = AnvilCurResource->GetRenderTargetTexture();
			FRHITexture* BottomBRHI   = BottomBResource->GetTextureRHI();
			FRHITexture* TopBRHI      = TopBResource->GetTextureRHI();
			FRHITexture* AnvilBRHI    = AnvilBResource->GetTextureRHI();
			if (!BottomARHI || !TopARHI || !AnvilARHI ||
				!BottomCurRHI || !TopCurRHI || !AnvilCurRHI ||
				!BottomBRHI || !TopBRHI || !AnvilBRHI)
			{
				return;
			}

			FRDGBuilder GraphBuilder(RHICmdList);
			FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(FeatureLevel);

			FSavageSuperStormProfileBlendPassParameters PassParameters;
			PassParameters.Resolution = Resolution;
			PassParameters.Alpha = ClampedAlpha;

			FRDGTextureRef BottomA   = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(BottomARHI,   TEXT("StormBottomProfileA")));
			FRDGTextureRef BottomBex = BottomBRHI == BottomARHI
				? BottomA
				: GraphBuilder.RegisterExternalTexture(CreateRenderTarget(BottomBRHI, TEXT("StormBottomProfileB")));
			FRDGTextureRef BottomCur = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(BottomCurRHI, TEXT("StormBottomProfileCurrent")));
			FSavageSuperStormShaderInterface::AddProfileBlendPass_RenderThread(
				GraphBuilder, GlobalShaderMap, PassParameters, BottomA, BottomBex, BottomCur);

			FRDGTextureRef TopA   = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(TopARHI,   TEXT("StormTopProfileA")));
			FRDGTextureRef TopBex = TopBRHI == TopARHI
				? TopA
				: GraphBuilder.RegisterExternalTexture(CreateRenderTarget(TopBRHI, TEXT("StormTopProfileB")));
			FRDGTextureRef TopCur = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(TopCurRHI, TEXT("StormTopProfileCurrent")));
			FSavageSuperStormShaderInterface::AddProfileBlendPass_RenderThread(
				GraphBuilder, GlobalShaderMap, PassParameters, TopA, TopBex, TopCur);

			FRDGTextureRef AnvilA   = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(AnvilARHI,   TEXT("StormAnvilProfileA")));
			FRDGTextureRef AnvilBex = AnvilBRHI == AnvilARHI
				? AnvilA
				: GraphBuilder.RegisterExternalTexture(CreateRenderTarget(AnvilBRHI, TEXT("StormAnvilProfileB")));
			FRDGTextureRef AnvilCur = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(AnvilCurRHI, TEXT("StormAnvilProfileCurrent")));
			FSavageSuperStormShaderInterface::AddProfileBlendPass_RenderThread(
				GraphBuilder, GlobalShaderMap, PassParameters, AnvilA, AnvilBex, AnvilCur);

			GraphBuilder.Execute();
		});
	return true;
}

bool UStormVerticalProfileToolComponent::ComposeProfileSequenceAtCurrentTime()
{
	if (!CanPlayProfileSequence())
	{
		return false;
	}

	int32 FromIndex = INDEX_NONE;
	int32 ToIndex = INDEX_NONE;
	float Alpha = 0.0f;
	if (!PersistentProfileAsset->FindSegment(
			GetProfileSequenceTime(),
			FromIndex,
			ToIndex,
			Alpha))
	{
		return false;
	}

	if (FromIndex != ToIndex &&
		PersistentProfileAsset->Keys[FromIndex].OutgoingEasing ==
			EStormProfileTransitionEasing::SmoothStep)
	{
		Alpha = Alpha * Alpha * (3.0f - 2.0f * Alpha);
	}

	FProfileSurfaceSet From;
	FProfileSurfaceSet To;
	return ResolveProfileSequenceKeySurfaces(FromIndex, From) &&
		ResolveProfileSequenceKeySurfaces(ToIndex, To) &&
		ComposeProfileSurfaces(From, To, Alpha);
}

#if WITH_EDITOR
void UStormVerticalProfileToolComponent::AdvanceProfileSequencePreview(
	float DeltaTime)
{
	TickProfileSequence(DeltaTime);
}
#endif

void UStormVerticalProfileToolComponent::NotifyProfileRenderDataChanged()
{
	if (AVolumetricSuperStormActor* StormActor = Cast<AVolumetricSuperStormActor>(GetOwner()))
	{
		StormActor->RefreshProfileRenderData();
	}
}

void UStormVerticalProfileToolComponent::TickProfileSequence(float DeltaTime)
{
	if (!IsProfileSequencePlaying())
	{
		UpdatePlaybackTickEnabled();
		return;
	}

	if (!CanPlayProfileSequence())
	{
		StopProfileSequence();
		return;
	}

	const float Duration = GetProfileSequenceDuration();
	const float TimeStep = FMath::Max(DeltaTime, 0.0f);
	const bool bPlayingForward =
		ProfileSequenceRuntime.Direction ==
			EStormProfileSequencePlaybackDirection::Forward;
	float NewTime = ProfileSequenceRuntime.TimeSeconds +
		(bPlayingForward ? TimeStep : -TimeStep);
	if (ProfileSequenceRuntime.bLooping)
	{
		NewTime = FMath::Fmod(NewTime, Duration);
		if (NewTime < 0.0f)
		{
			NewTime += Duration;
		}
	}
	else if (bPlayingForward && NewTime >= Duration)
	{
		NewTime = Duration;
		ProfileSequenceRuntime.State =
			EStormProfileSequencePlaybackState::Paused;
	}
	else if (!bPlayingForward && NewTime <= 0.0f)
	{
		NewTime = 0.0f;
		ProfileSequenceRuntime.State =
			EStormProfileSequencePlaybackState::Paused;
	}
	if (!ProfileSequenceRuntime.bLooping)
	{
		NewTime = FMath::Clamp(NewTime, 0.0f, Duration);
	}

	ProfileSequenceRuntime.TimeSeconds = NewTime;
	if (!ComposeProfileSequenceAtCurrentTime())
	{
		StopProfileSequence();
		return;
	}

	UpdatePlaybackTickEnabled();
	NotifyProfileRenderDataChanged();
}

void UStormVerticalProfileToolComponent::UpdatePlaybackTickEnabled()
{
	SetComponentTickEnabled(
		IsProfileSequencePlaying());
}

void UStormVerticalProfileToolComponent::ResetProfileSequenceRuntime(
	bool bResetTime)
{
	ProfileSequenceRuntime.State =
		EStormProfileSequencePlaybackState::Stopped;
	ProfileSequenceRuntime.bPreviewActive = false;
	if (bResetTime)
	{
		ProfileSequenceRuntime.TimeSeconds = 0.0f;
	}
}

void UStormVerticalProfileToolComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	TickProfileSequence(DeltaTime);
}

#if WITH_EDITOR
void UStormVerticalProfileToolComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (GIsTransacting)
	{
		// Undo/redo may restore params authored for a key that is no longer active.
		// The history reconciliation pass will activate and replay the affected key;
		// rebuilding or publishing here would apply those params to the current key.
		return;
	}

	// The bottom profile is purely parametric (never painted), so keep it live with the params.
	BuildBottomTypeProfile();

	// Protect both paintable RTs in Paint mode.
	if (bParameterizeMode)
	{
		BuildTopTypeProfile(TopTypeProfileRT);
		BuildAnvilProfile();
	}

	if ((PropertyChangedEvent.ChangeType & EPropertyChangeType::Interactive) == 0)
	{
		RecordReseedFromParams();
	}

	MarkProfileDirty();
}
#endif // WITH_EDITOR

void UStormVerticalProfileToolComponent::RebuildVerticalProfiles()
{
	RegenerateProfileRenderTargets();
	BuildBottomTypeProfile();
	// Rebuilding seeds the paint surface from the current parametric settings and
	// therefore discards any brushwork (paint is destructive by design).
	BuildTopTypeProfile(TopTypeProfileRT);
	BuildAnvilProfile();
	bProfilesInitialized = GetWorld() && GetWorld()->Scene;
#if WITH_EDITOR
	// Top and Anvil are pure parametric macro again; the log records that as a
	// reseed, which is also what makes HasPaintedProfiles read false from here on.
	RecordReseedFromParams();
	MarkProfileDirty();
#endif
}

bool UStormVerticalProfileToolComponent::RegenerateProfileRenderTargets()
{
	const int32 Resolution = FMath::Max<int32>(StormProfileParams.StormProfileResolution, 1);
	const bool bNeedsRealloc =
		!BottomTypeProfileRT || !TopTypeProfileRT || !AnvilProfileRT || !StashTopRT || !StashAnvilRT ||
		!BottomCurrentRT || !TopCurrentRT || !AnvilCurrentRT ||
		BottomTypeProfileRT->GetOuter() != this ||
		TopTypeProfileRT->GetOuter() != this ||
		AnvilProfileRT->GetOuter() != this ||
		StashTopRT->GetOuter() != this ||
		StashAnvilRT->GetOuter() != this ||
		BottomCurrentRT->GetOuter() != this ||
		TopCurrentRT->GetOuter() != this ||
		AnvilCurrentRT->GetOuter() != this ||
		BottomTypeProfileRT->SizeX != Resolution ||
		BottomTypeProfileRT->SizeY != Resolution ||
		TopTypeProfileRT->SizeX != Resolution ||
		TopTypeProfileRT->SizeY != Resolution ||
		AnvilProfileRT->SizeX != Resolution ||
		AnvilProfileRT->SizeY != Resolution ||
		StashTopRT->SizeX != Resolution ||
		StashTopRT->SizeY != Resolution ||
		StashAnvilRT->SizeX != Resolution ||
		StashAnvilRT->SizeY != Resolution ||
		BottomCurrentRT->SizeX != Resolution ||
		BottomCurrentRT->SizeY != Resolution ||
		TopCurrentRT->SizeX != Resolution ||
		TopCurrentRT->SizeY != Resolution ||
		AnvilCurrentRT->SizeX != Resolution ||
		AnvilCurrentRT->SizeY != Resolution;
	if (!bNeedsRealloc)
	{
		return false;
	}

	BottomTypeProfileRT = CreateProfileRenderTarget(
		TEXT("RT_StormBottomTypeProfile"), Resolution);
	TopTypeProfileRT = CreateProfileRenderTarget(
		TEXT("RT_StormPaintedTopProfile"), Resolution);
	AnvilProfileRT = CreateProfileRenderTarget(
		TEXT("RT_StormAnvilProfile"), Resolution);
	StashTopRT = CreateProfileRenderTarget(
		TEXT("RT_StormStashTopProfile"), Resolution);
	StashAnvilRT = CreateProfileRenderTarget(
		TEXT("RT_StormStashAnvilProfile"), Resolution);
	BottomCurrentRT = CreateProfileRenderTarget(
		TEXT("RT_StormBottomCurrentProfile"), Resolution);
	TopCurrentRT = CreateProfileRenderTarget(
		TEXT("RT_StormTopCurrentProfile"), Resolution);
	AnvilCurrentRT = CreateProfileRenderTarget(
		TEXT("RT_StormAnvilCurrentProfile"), Resolution);
#if WITH_EDITOR
	if (FStormProfileKeyRenderTargetSet* ActiveState =
			KeyRenderTargets.Find(ActiveKeyRenderTargetId))
	{
		ActiveState->Bottom = BottomTypeProfileRT;
		ActiveState->Top = TopTypeProfileRT;
		ActiveState->Anvil = AnvilProfileRT;
		ActiveState->StashTop = StashTopRT;
		ActiveState->StashAnvil = StashAnvilRT;
		ActiveState->Params = StormProfileParams;
	}
	ValidateKeyRenderTargetOwnership();
#endif
	bProfilesInitialized = false;
	return true;
}

UTextureRenderTarget2D*
UStormVerticalProfileToolComponent::CreateProfileRenderTarget(
	const FName& BaseName,
	int32 Resolution)
{
	const FName UniqueName = MakeUniqueObjectName(
		this,
		UTextureRenderTarget2D::StaticClass(),
		BaseName);
	UTextureRenderTarget2D* RT =
		NewObject<UTextureRenderTarget2D>(this, UniqueName);
	RT->ClearColor = FLinearColor::Black;
	RT->bCanCreateUAV = true;
	RT->SRGB = false;
	RT->AddressX = TA_Clamp;
	RT->AddressY = TA_Clamp;
	RT->InitCustomFormat(
		FMath::Max(Resolution, 1),
		FMath::Max(Resolution, 1),
		PF_FloatRGBA,
		true);
	RT->UpdateResourceImmediate(true);
	return RT;
}

void UStormVerticalProfileToolComponent::BuildBottomTypeProfile()
{
	BuildBottomTypeProfile(BottomTypeProfileRT, StormProfileParams);
}

void UStormVerticalProfileToolComponent::BuildBottomTypeProfile(
	UTextureRenderTarget2D* TargetRT,
	const FStormProfileParams& Params)
{
	UWorld* World = GetWorld();
	if (!World || !World->Scene || !TargetRT)
	{
		return;
	}

	FTextureRenderTargetResource* RTResource =
		TargetRT->GameThread_GetRenderTargetResource();
	if (!RTResource)
	{
		return;
	}

	FSavageSuperStormBottomTypePassParameters PassParameters;
	PassParameters.Resolution = static_cast<uint32>(TargetRT->SizeX);
	PassParameters.BottomFade = Params.BottomFade;

	const ERHIFeatureLevel::Type FeatureLevel = World->Scene->GetFeatureLevel();
	ENQUEUE_RENDER_COMMAND(BuildBottomTypeProfileRT)(
		[PassParameters, RTResource, FeatureLevel](FRHICommandListImmediate& RHICmdList)
		{
			FRDGBuilder GraphBuilder(RHICmdList);
			FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(FeatureLevel);
			FRHITexture* TextureRHI = RTResource->GetRenderTargetTexture();
			FRDGTextureRef RDGTexture = GraphBuilder.RegisterExternalTexture(
				CreateRenderTarget(TextureRHI, TEXT("RT_StormBottomTypeProfile")));

			FSavageSuperStormShaderInterface::AddBottomTypeProfilePass_RenderThread(
				GraphBuilder, GlobalShaderMap, PassParameters, RDGTexture);
			GraphBuilder.Execute();
		});
}

void UStormVerticalProfileToolComponent::BuildTopTypeProfile(UTextureRenderTarget2D* TargetRT)
{
	BuildTopTypeProfile(TargetRT, StormProfileParams);
}

void UStormVerticalProfileToolComponent::BuildTopTypeProfile(
	UTextureRenderTarget2D* TargetRT,
	const FStormProfileParams& Params)
{
	UWorld* World = GetWorld();
	if (!World || !World->Scene || !TargetRT)
	{
		return;
	}

	FTextureRenderTargetResource* RTResource = TargetRT->GameThread_GetRenderTargetResource();
	if (!RTResource)
	{
		return;
	}

	TArray<float> LUT;
	if (!SampleVerticalProfileCurve(
		Params.VerticalProfileCurve,
		Params.StormProfileResolution,
		LUT,
		0.0f,
		1.0f))
	{
		UE_LOG(LogSavageSuperStormRuntime, Error,
			TEXT("Top profile curve is unusable; profile rebuild skipped."));
		return;
	}

	FSavageSuperStormTopTypePassParameters PassParameters;
	PassParameters.Resolution = static_cast<uint32>(TargetRT->SizeX);
	PassParameters.TopFade = Params.TopFade;
	PassParameters.TopHeightLUT = MoveTemp(LUT);

	const ERHIFeatureLevel::Type FeatureLevel = World->Scene->GetFeatureLevel();
	ENQUEUE_RENDER_COMMAND(BuildTopTypeProfileRT)(
		[PassParameters, RTResource, FeatureLevel](FRHICommandListImmediate& RHICmdList)
		{
			FRDGBuilder GraphBuilder(RHICmdList);
			FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(FeatureLevel);
			FRHITexture* TextureRHI = RTResource->GetRenderTargetTexture();
			FRDGTextureRef RDGTexture = GraphBuilder.RegisterExternalTexture(
				CreateRenderTarget(TextureRHI, TEXT("RT_StormTopTypeProfile")));

			FSavageSuperStormShaderInterface::AddTopTypeProfilePass_RenderThread(
				GraphBuilder, GlobalShaderMap, PassParameters, RDGTexture);
			GraphBuilder.Execute();
		});
}

void UStormVerticalProfileToolComponent::BuildAnvilProfile()
{
	BuildAnvilProfile(AnvilProfileRT, StormProfileParams);
}

void UStormVerticalProfileToolComponent::BuildAnvilProfile(
	UTextureRenderTarget2D* TargetRT,
	const FStormProfileParams& Params)
{
	UWorld* World = GetWorld();
	if (!World || !World->Scene || !TargetRT)
	{
		return;
	}

	FTextureRenderTargetResource* RTResource = TargetRT->GameThread_GetRenderTargetResource();
	if (!RTResource)
	{
		return;
	}

	const int32 Resolution = FMath::Max(Params.StormProfileResolution, 1);
	TArray<float> LUT;
	if (!SampleVerticalProfileCurve(
		Params.AnvilProfileCurve,
		Resolution,
		LUT,
		0.0f,
		1.0f))
	{
		UE_LOG(LogSavageSuperStormRuntime, Error,
			TEXT("Anvil profile curve is unusable; profile rebuild skipped."));
		return;
	}

	FSavageSuperStormAnvilProfilePassParameters PassParameters;
	PassParameters.Resolution = static_cast<uint32>(Resolution);
	PassParameters.Fade = Params.TopFade;
	PassParameters.HeightLUT = MoveTemp(LUT);

	const ERHIFeatureLevel::Type FeatureLevel = World->Scene->GetFeatureLevel();
	ENQUEUE_RENDER_COMMAND(BuildAnvilProfileRT)(
		[PassParameters, RTResource, FeatureLevel](FRHICommandListImmediate& RHICmdList)
		{
			FRDGBuilder GraphBuilder(RHICmdList);
			FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(FeatureLevel);
			FRHITexture* TextureRHI = RTResource->GetRenderTargetTexture();
			FRDGTextureRef RDGTexture = GraphBuilder.RegisterExternalTexture(
				CreateRenderTarget(TextureRHI, TEXT("RT_StormAnvilProfile")));

			FSavageSuperStormShaderInterface::AddAnvilProfilePass_RenderThread(
				GraphBuilder, GlobalShaderMap, PassParameters, RDGTexture);
			GraphBuilder.Execute();
		});
}

void UStormVerticalProfileToolComponent::StampBrush(
	FVector2D BrushUV,
	float BrushRadiusUV,
	float Strength,
	float Value,
	bool bErase,
	bool bOverwrite,
	bool bPaintAnvil)
{
	UTextureRenderTarget2D* PaintTarget = bPaintAnvil
		? AnvilProfileRT.Get()
		: TopTypeProfileRT.Get();

	if (!StampBrushInto(
			PaintTarget,
			BrushUV,
			BrushRadiusUV,
			Strength,
			Value,
			bErase,
			bOverwrite))
	{
		return;
	}

#if WITH_EDITOR
	FStormProfileOp Op;
	Op.Type = EStormProfileOpType::Stamp;
	Op.bTargetAnvil = bPaintAnvil;
	Op.BrushUV = FVector2f(
		static_cast<float>(BrushUV.X),
		static_cast<float>(BrushUV.Y));
	// Record the clamped values the pass actually consumed, not the raw arguments.
	// Replay feeds these straight back into the same pass, so recording anything
	// the shader would have clamped differently would drift the reproduction.
	Op.BrushRadiusUV = FMath::Max(BrushRadiusUV, 0.f);
	Op.Strength = FMath::Clamp(Strength, 0.f, 1.f);
	Op.Value = FMath::Clamp(Value, 0.f, 1.f);
	Op.bErase = bErase;
	Op.bOverwrite = bOverwrite;
	RecordOp(Op);
	MarkProfileDirty();
#endif
}

bool UStormVerticalProfileToolComponent::StampBrushInto(
	UTextureRenderTarget2D* TargetRT,
	FVector2D BrushUV,
	float BrushRadiusUV,
	float Strength,
	float Value,
	bool bErase,
	bool bOverwrite)
{
	UWorld* World = GetWorld();
	if (!World || !World->Scene || !TargetRT)
	{
		return false;
	}

	FTextureRenderTargetResource* PaintResource = TargetRT->GameThread_GetRenderTargetResource();
	if (!PaintResource)
	{
		return false;
	}

	FSavageSuperStormProfileBrushPassParameters PassParameters;
	PassParameters.Resolution    = static_cast<uint32>(TargetRT->SizeX);
	PassParameters.BrushCenterUV = FVector2f(static_cast<float>(BrushUV.X), static_cast<float>(BrushUV.Y));
	PassParameters.BrushRadiusUV = FMath::Max(BrushRadiusUV, 0.f);
	PassParameters.BrushStrength = FMath::Clamp(Strength, 0.f, 1.f);
	PassParameters.BrushValue    = FMath::Clamp(Value, 0.f, 1.f);
	PassParameters.bErase        = bErase ? 1u : 0u;
	PassParameters.bOverwrite    = bOverwrite ? 1u : 0u;

	const ERHIFeatureLevel::Type FeatureLevel = World->Scene->GetFeatureLevel();
	ENQUEUE_RENDER_COMMAND(StampProfileBrushRT)(
		[PassParameters, PaintResource, FeatureLevel](FRHICommandListImmediate& RHICmdList)
		{
			FRDGBuilder GraphBuilder(RHICmdList);
			FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(FeatureLevel);

			FRDGTextureRef PaintTex = GraphBuilder.RegisterExternalTexture(
				CreateRenderTarget(PaintResource->GetRenderTargetTexture(), TEXT("RT_StormPaintedProfile")));

			FSavageSuperStormShaderInterface::AddProfileBrushPass_RenderThread(
				GraphBuilder, GlobalShaderMap, PassParameters, PaintTex);
			GraphBuilder.Execute();
		});

	return true;
}

bool UStormVerticalProfileToolComponent::SeedSurfacesFromAsset()
{
	if (!PersistentProfileAsset) return false;
	const FStormProfileKey* PrimaryKey =
		PersistentProfileAsset->GetPrimaryKey();
	if (!PrimaryKey)
	{
		return false;
	}

	StormProfileParams = PrimaryKey->Params;
	RegenerateProfileRenderTargets();

#if WITH_EDITOR
	return SeedSurfacesFromKey(*PrimaryKey);
#else
	UTexture2D* TopProfile		= PrimaryKey->TopProfile.Get();
	UTexture2D* BottomProfile	= PrimaryKey->BottomProfile.Get();
	UTexture2D* AnvilProfile	= PrimaryKey->AnvilProfile.Get();

	if (!TopProfile || !BottomProfile ||
		!TopTypeProfileRT || !BottomTypeProfileRT || !AnvilProfileRT)
	{
		return false;
	}

	FlushRenderingCommands();

	if (!BlitToRenderTarget(TopProfile, TopTypeProfileRT) ||
		!BlitToRenderTarget(BottomProfile, BottomTypeProfileRT) ||
		!BlitToRenderTarget(AnvilProfile, AnvilProfileRT))
	{
		return false;
	}
	bProfilesInitialized = true;
	return true;
#endif
}

#if WITH_EDITOR
bool UStormVerticalProfileToolComponent::SeedSurfacesFromKey(
	const FStormProfileKey& ProfileKey)
{
	UTexture2D* TopProfile		= ProfileKey.TopProfile.Get();
	UTexture2D* BottomProfile	= ProfileKey.BottomProfile.Get();
	UTexture2D* AnvilProfile	= ProfileKey.AnvilProfile.Get();

	if (!ProfileKey.KeyId.IsValid() ||
		!TopProfile || !BottomProfile || !AnvilProfile)
	{
		return false;
	}

	// Switching keys always commits any pending stroke: the next dab belongs to a
	// different log and must not extend this key's edit.
	EndStroke();
	// This also covers undo performed while the painter was closed: activation is
	// the final authority that a structurally present key should be live again.
	RestoreTombstonedKeyRenderTargets(ProfileKey.KeyId);

	const FGuid PreviousKeyId = ActiveKeyRenderTargetId;
	if (FStormProfileKeyRenderTargetSet* ExistingState =
			KeyRenderTargets.Find(ProfileKey.KeyId))
	{
		RestoreKeyRenderTargetState(ProfileKey.KeyId, *ExistingState);
		EnsureKeyHistory(ProfileKey.KeyId, ExistingState->Params);
		bProfilesInitialized = true;
		// An undo that moved this key's playhead while another key was active left
		// its surfaces behind. Catch up now rather than on every undo, which keeps
		// undo O(one key) no matter how many working sets are resident.
		ReplayActiveKeyIfStale();
		return true;
	}

	FStormProfileKeyRenderTargetSet& NewState =
		KeyRenderTargets.Add(ProfileKey.KeyId);
	if (!AllocateKeyRenderTargets(
			ProfileKey.KeyId,
			ProfileKey.Params,
			NewState))
	{
		KeyRenderTargets.Remove(ProfileKey.KeyId);
		return false;
	}
	RestoreKeyRenderTargetState(ProfileKey.KeyId, NewState);

	// A texture chosen from the asset picker may be loaded for the first time here.
	// Ensure its render resource has been created before the canvas blit samples it;
	FTextureCompilingManager::Get().FinishCompilation(
		{ TopProfile, BottomProfile, AnvilProfile });

	FlushRenderingCommands();

	if (!BlitToRenderTarget(TopProfile, TopTypeProfileRT) ||
		!BlitToRenderTarget(BottomProfile, BottomTypeProfileRT) ||
		!BlitToRenderTarget(AnvilProfile, AnvilProfileRT))
	{
		ActiveKeyRenderTargetId.Invalidate();
		KeyRenderTargets.Remove(ProfileKey.KeyId);
		if (FStormProfileKeyRenderTargetSet* PreviousState =
				KeyRenderTargets.Find(PreviousKeyId))
		{
			RestoreKeyRenderTargetState(
				PreviousKeyId,
				*PreviousState);
		}
		return false;
	}
	bProfilesInitialized = true;

	// Anchor the origin on the pixels just loaded, so replaying an empty log
	// reproduces the baked key exactly. The log starts empty rather than holding a
	// LoadFromTexture op precisely because the origin already *is* that load.
	FlushRenderingCommands();
	BlitToRenderTarget(TopTypeProfileRT, StashTopRT);
	BlitToRenderTarget(AnvilProfileRT, StashAnvilRT);
	FlushRenderingCommands();

	EnsureKeyHistory(ProfileKey.KeyId, ProfileKey.Params);
	// These surfaces came straight out of the asset, so the empty log is the
	// saved state.
	MarkKeyRenderTargetsSaved(ProfileKey.KeyId);
	return true;
}

bool UStormVerticalProfileToolComponent::DuplicateKeyRenderTargets(
	const FGuid& SourceKeyId,
	const FStormProfileKey& NewKey)
{
	if (!SourceKeyId.IsValid() ||
		!NewKey.KeyId.IsValid() ||
		SourceKeyId == NewKey.KeyId)
	{
		return false;
	}

	const FStormProfileKeyRenderTargetSet* SourceStatePtr =
		KeyRenderTargets.Find(SourceKeyId);
	if (!SourceStatePtr)
	{
		return false;
	}
	// Adding the destination may reallocate the map, so retain a value copy of
	// the source pointers and metadata rather than a pointer into the map.
	const FStormProfileKeyRenderTargetSet SourceState = *SourceStatePtr;
	const UStormProfileKeyUndoState* SourceUndoState =
		FindKeyUndoState(SourceKeyId);
	const FStormProfileKeyHistory* SourceHistoryPtr =
		FindKeyHistory(SourceKeyId);
	const bool bHasSourceHistory =
		SourceUndoState && SourceHistoryPtr;
	const FGuid SourceCurrentStateId = bHasSourceHistory
		? SourceUndoState->CurrentStateId
		: FGuid();
	// Adding the destination history may reallocate the registry map, so copy the
	// immutable graph before creating the destination entry.
	FStormProfileKeyHistory SourceHistoryCopy;
	if (bHasSourceHistory)
	{
		SourceHistoryCopy = *SourceHistoryPtr;
	}

	FStormProfileKeyRenderTargetSet& NewState =
		KeyRenderTargets.Add(NewKey.KeyId);
	if (!AllocateKeyRenderTargets(
			NewKey.KeyId,
			SourceState.Params,
			NewState))
	{
		KeyRenderTargets.Remove(NewKey.KeyId);
		return false;
	}

	FlushRenderingCommands();
	const bool bCopied =
		BlitToRenderTarget(SourceState.Bottom, NewState.Bottom) &&
		BlitToRenderTarget(SourceState.Top, NewState.Top) &&
		BlitToRenderTarget(SourceState.Anvil, NewState.Anvil) &&
		BlitToRenderTarget(SourceState.StashTop, NewState.StashTop) &&
		BlitToRenderTarget(SourceState.StashAnvil, NewState.StashAnvil);
	FlushRenderingCommands();
	if (!bCopied)
	{
		KeyRenderTargets.Remove(NewKey.KeyId);
		return false;
	}

	NewState.Params = SourceState.Params;

	// The duplicate's stash and live surfaces were both copied from the source, so
	// its origin and its bookmark match the source's -- the immutable graph copies
	// across verbatim. Only the saved marker differs: these pixels have never been
	// baked into a key of their own.
	UStormProfileKeyUndoState* NewUndoState =
		EnsureKeyUndoState(NewKey.KeyId);
	if (!NewUndoState || !EditUndoState)
	{
		KeyRenderTargets.Remove(NewKey.KeyId);
		return false;
	}
	FStormProfileKeyHistory& NewHistory =
		EditUndoState->KeyHistories.FindOrAdd(NewKey.KeyId);
	if (bHasSourceHistory)
	{
		NewHistory = MoveTemp(SourceHistoryCopy);
		NewUndoState->CurrentStateId = SourceCurrentStateId;
	}
	else
	{
		NewHistory.OriginParams = SourceState.Params;
		NewHistory.Graph.OriginStateId = FGuid::NewGuid();
		NewHistory.Graph.OriginContentId = NewHistory.Graph.OriginStateId;
		NewUndoState->CurrentStateId = NewHistory.Graph.OriginStateId;
	}
	SavedStatesByKey.Remove(NewKey.KeyId);
	ReplayedStateByKey.Add(
		NewKey.KeyId,
		NewUndoState->CurrentStateId);
	ValidateKeyRenderTargetOwnership();
	return true;
}

void UStormVerticalProfileToolComponent::TombstoneKeyRenderTargets(
	const FGuid& KeyId)
{
	FStormProfileKeyRenderTargetSet* LiveState =
		KeyRenderTargets.Find(KeyId);
	if (!LiveState)
	{
		return;
	}

	if (TombstonedKeyRenderTargets.Contains(KeyId))
	{
		ensureMsgf(false,
			TEXT("Profile key %s had both a live and tombstoned workspace."),
			*KeyId.ToString(EGuidFormats::Digits));
		return;
	}
	TombstonedKeyRenderTargets.Add(KeyId, MoveTemp(*LiveState));
	KeyRenderTargets.Remove(KeyId);

	// History, saved content identity, and replay identity deliberately remain in
	// their registries. The tombstone makes the key inactive without making any
	// state needed by undo unreachable.
	if (ActiveKeyRenderTargetId == KeyId)
	{
		EndStroke();
		ActiveKeyRenderTargetId.Invalidate();
	}
	ValidateKeyRenderTargetOwnership();
}

bool UStormVerticalProfileToolComponent::RestoreTombstonedKeyRenderTargets(
	const FGuid& KeyId)
{
	FStormProfileKeyRenderTargetSet* TombstonedState =
		TombstonedKeyRenderTargets.Find(KeyId);
	if (!TombstonedState)
	{
		return false;
	}
	if (KeyRenderTargets.Contains(KeyId))
	{
		ensureMsgf(false,
			TEXT("Profile key %s had both a live and tombstoned workspace."),
			*KeyId.ToString(EGuidFormats::Digits));
		return false;
	}

	KeyRenderTargets.Add(KeyId, MoveTemp(*TombstonedState));
	TombstonedKeyRenderTargets.Remove(KeyId);
	if (ScratchWorkingSetId == KeyId)
	{
		ScratchWorkingSetId.Invalidate();
	}
	ValidateKeyRenderTargetOwnership();
	return true;
}

bool UStormVerticalProfileToolComponent::ReconcileKeyRenderTargets(
	const UStormVerticalProfileAsset* OwningAsset,
	TArray<FGuid>& OutRestoredKeyIds)
{
	OutRestoredKeyIds.Reset();

	TSet<FGuid> LiveKeyIds;
	if (OwningAsset)
	{
		LiveKeyIds.Reserve(OwningAsset->Keys.Num());
		for (const FStormProfileKey& Key : OwningAsset->Keys)
		{
			LiveKeyIds.Add(Key.KeyId);
		}
	}

	// Undo of a structural removal brings the asset key back first. Restore its
	// exact non-transactional workspace before the painter tries to select it.
	for (const FGuid& KeyId : LiveKeyIds)
	{
		if (RestoreTombstonedKeyRenderTargets(KeyId))
		{
			OutRestoredKeyIds.Add(KeyId);
		}
	}

	bool bDroppedActiveSet = false;
	for (auto It = KeyRenderTargets.CreateIterator(); It; ++It)
	{
		if (LiveKeyIds.Contains(It.Key()))
		{
			// The asset owns this id now, so it is an ordinary key and no longer
			// needs the scratch exemption below.
			if (ScratchWorkingSetId == It.Key())
			{
				ScratchWorkingSetId.Invalidate();
			}
			continue;
		}
		if (ScratchWorkingSetId == It.Key())
		{
			// The pre-asset working set is in no asset by construction. Dropping it
			// here would destroy the log for everything painted before the first
			// save, which is exactly the work an undo is trying to step back
			// through.
			continue;
		}
		if (ActiveKeyRenderTargetId == It.Key())
		{
			// The active pointer views still refer to this set's surfaces until the
			// caller activates a surviving key. Clear the id so those views cannot
			// be mistaken for an active live workspace.
			EndStroke();
			ActiveKeyRenderTargetId.Invalidate();
			bDroppedActiveSet = true;
		}

		if (TombstonedKeyRenderTargets.Contains(It.Key()))
		{
			ensureMsgf(false,
				TEXT("Profile key %s had both a live and tombstoned workspace."),
				*It.Key().ToString(EGuidFormats::Digits));
			continue;
		}
		TombstonedKeyRenderTargets.Add(
			It.Key(),
			MoveTemp(It.Value()));
		It.RemoveCurrent();
	}

	ValidateKeyRenderTargetOwnership();
	return bDroppedActiveSet;
}

void UStormVerticalProfileToolComponent::ResetKeyRenderTargets()
{
	PendingStrokeEdit = FStormProfileEdit();
	KeyRenderTargets.Empty();
	TombstonedKeyRenderTargets.Empty();
	if (EditUndoState)
	{
		EditUndoState->ResetStroke();
		EditUndoState->KeyStates.Empty();
		EditUndoState->KeyHistories.Empty();
	}
	SavedStatesByKey.Empty();
	ReplayedStateByKey.Empty();
	if (EditUndoState)
	{
		EditUndoState->ResetBranchTracking();
	}
	ActiveKeyRenderTargetId.Invalidate();
	ScratchWorkingSetId.Invalidate();
}

void UStormVerticalProfileToolComponent::AdvanceProfileEditSession()
{
	Modify();
	++ProfileEditSessionSerial;
}

bool UStormVerticalProfileToolComponent::IsProfileHistoryCheckpointRequired() const
{
	if (!EditUndoState)
	{
		return false;
	}

	const int64 MaxDepth =
		SavageSuperStorm::CVarProfileHistoryMaxDepth.GetValueOnGameThread();
	const int64 MaxReplayCost =
		SavageSuperStorm::CVarProfileHistoryMaxReplayCost.GetValueOnGameThread();
	if (MaxDepth <= 0 && MaxReplayCost <= 0)
	{
		return false;
	}

	for (const TPair<FGuid, FStormProfileKeyHistory>& Pair :
		EditUndoState->KeyHistories)
	{
		const UStormProfileKeyUndoState* KeyState =
			FindKeyUndoState(Pair.Key);
		if (!KeyState || !Pair.Value.ContainsState(KeyState->CurrentStateId))
		{
			continue;
		}

		const int64 Depth = Pair.Value.GetDepth(KeyState->CurrentStateId);
		const int64 ReplayCost =
			Pair.Value.GetReplayCost(KeyState->CurrentStateId);
		if ((MaxDepth > 0 && Depth >= MaxDepth) ||
			(MaxReplayCost > 0 && ReplayCost >= MaxReplayCost))
		{
			return true;
		}
	}
	return false;
}

bool UStormVerticalProfileToolComponent::RebaseProfileHistoriesWithUndoBoundary(
	TFunctionRef<bool()> EstablishUndoBoundary)
{
	if (IsHistoryStrokeOpen() || !PendingStrokeEdit.Ops.IsEmpty() ||
		!EditUndoState || KeyRenderTargets.IsEmpty())
	{
		return false;
	}

	struct FPreparedCheckpoint
	{
		FGuid KeyId;
		FGuid HeadStateId;
		FGuid ContentId;
		FStormProfileParams Params;
		UTextureRenderTarget2D* NewStashTop = nullptr;
		UTextureRenderTarget2D* NewStashAnvil = nullptr;
	};

	TArray<FPreparedCheckpoint> Prepared;
	Prepared.Reserve(KeyRenderTargets.Num());
	FlushRenderingCommands();
	for (TPair<FGuid, FStormProfileKeyRenderTargetSet>& Pair :
		KeyRenderTargets)
	{
		FStormProfileKeyRenderTargetSet& State = Pair.Value;
		UStormProfileKeyUndoState* KeyState = FindKeyUndoState(Pair.Key);
		FStormProfileKeyHistory* History =
			EditUndoState->KeyHistories.Find(Pair.Key);
		if (!KeyState || !History ||
			!History->ContainsState(KeyState->CurrentStateId) ||
			!State.Bottom || !State.Top || !State.Anvil ||
			!State.StashTop || !State.StashAnvil)
		{
			return false;
		}

		FPreparedCheckpoint& Checkpoint = Prepared.AddDefaulted_GetRef();
		Checkpoint.KeyId = Pair.Key;
		Checkpoint.HeadStateId = KeyState->CurrentStateId;
		Checkpoint.ContentId =
			History->ResolveContentId(Checkpoint.HeadStateId);
		Checkpoint.Params = History->ResolveParamsAt(Checkpoint.HeadStateId);
		if (!Checkpoint.ContentId.IsValid() ||
			History->GetAbsoluteDepth(Checkpoint.HeadStateId) == INDEX_NONE ||
			History->GetCumulativeReplayCost(Checkpoint.HeadStateId) == INDEX_NONE)
		{
			return false;
		}

		const int32 Resolution = FMath::Max(State.Top->SizeX, 1);
		const FString Prefix = FString::Printf(
			TEXT("RT_StormCheckpoint_%s"),
			*Pair.Key.ToString(EGuidFormats::Digits));
		Checkpoint.NewStashTop = CreateProfileRenderTarget(
			FName(*(Prefix + TEXT("_Top"))), Resolution);
		Checkpoint.NewStashAnvil = CreateProfileRenderTarget(
			FName(*(Prefix + TEXT("_Anvil"))), Resolution);
		if (!Checkpoint.NewStashTop || !Checkpoint.NewStashAnvil)
		{
			return false;
		}

		const FGuid* ReflectedState = ReplayedStateByKey.Find(Pair.Key);
		if (!ReflectedState || *ReflectedState != Checkpoint.HeadStateId)
		{
			// An inactive key may still show the state from before an undo. Repair its
			// live surfaces before those pixels become the irreversible checkpoint.
			if (!ReplayKeyInto(
					*History,
					Checkpoint.HeadStateId,
					State.StashTop,
					State.StashAnvil,
					State.Top,
					State.Anvil))
			{
				return false;
			}
		}

		// Preparation may repair an inactive state after Undo. Keep the ordinary
		// non-transactional mirrors honest even if a later key prevents checkpointing.
		State.Params = Checkpoint.Params;
		ReplayedStateByKey.Add(Pair.Key, Checkpoint.HeadStateId);
		if (Pair.Key == ActiveKeyRenderTargetId)
		{
			StormProfileParams = Checkpoint.Params;
		}
		BuildBottomTypeProfile(State.Bottom, Checkpoint.Params);
		if (!BlitToRenderTarget(State.Top, Checkpoint.NewStashTop) ||
			!BlitToRenderTarget(State.Anvil, Checkpoint.NewStashAnvil))
		{
			return false;
		}
	}
	FlushRenderingCommands();

	// Nothing irreversible has changed above. Only after every replacement origin
	// is complete do we let the editor seal the old transactions behind a barrier.
	if (!EstablishUndoBoundary())
	{
		return false;
	}

	ReplayedStateByKey.Empty();
	for (const FPreparedCheckpoint& Checkpoint : Prepared)
	{
		FStormProfileKeyRenderTargetSet* State =
			KeyRenderTargets.Find(Checkpoint.KeyId);
		UStormProfileKeyUndoState* KeyState =
			FindKeyUndoState(Checkpoint.KeyId);
		FStormProfileKeyHistory* History =
			EditUndoState->KeyHistories.Find(Checkpoint.KeyId);
		check(State && KeyState && History);

		const FGuid NewOriginStateId =
			History->RebaseToCheckpoint(Checkpoint.HeadStateId);
		check(NewOriginStateId.IsValid());
		State->StashTop = Checkpoint.NewStashTop;
		State->StashAnvil = Checkpoint.NewStashAnvil;
		State->Params = Checkpoint.Params;
		KeyState->CurrentStateId = NewOriginStateId;
		ReplayedStateByKey.Add(Checkpoint.KeyId, NewOriginStateId);

		if (FStormProfileSavedState* SavedState =
				SavedStatesByKey.Find(Checkpoint.KeyId))
		{
			SavedState->ReplayDepthHint =
				SavedState->ContentId == Checkpoint.ContentId
					? 0
					: INDEX_NONE;
		}
	}

	// Deleted-key tombstones only existed so global Undo could restore them. The
	// barrier makes that impossible, so release their workspaces and histories too.
	TombstonedKeyRenderTargets.Empty();
	for (auto It = EditUndoState->KeyStates.CreateIterator(); It; ++It)
	{
		if (!KeyRenderTargets.Contains(It.Key()))
		{
			It.RemoveCurrent();
		}
	}
	for (auto It = EditUndoState->KeyHistories.CreateIterator(); It; ++It)
	{
		if (!KeyRenderTargets.Contains(It.Key()))
		{
			It.RemoveCurrent();
		}
	}
	for (auto It = SavedStatesByKey.CreateIterator(); It; ++It)
	{
		if (!KeyRenderTargets.Contains(It.Key()))
		{
			It.RemoveCurrent();
		}
	}
	EditUndoState->ResetBranchTracking();

	if (FStormProfileKeyRenderTargetSet* ActiveState =
			KeyRenderTargets.Find(ActiveKeyRenderTargetId))
	{
		BottomTypeProfileRT = ActiveState->Bottom;
		TopTypeProfileRT = ActiveState->Top;
		AnvilProfileRT = ActiveState->Anvil;
		StashTopRT = ActiveState->StashTop;
		StashAnvilRT = ActiveState->StashAnvil;
		StormProfileParams = ActiveState->Params;
	}
	if (ScratchWorkingSetId.IsValid() &&
		!KeyRenderTargets.Contains(ScratchWorkingSetId))
	{
		ScratchWorkingSetId.Invalidate();
	}

	ValidateKeyRenderTargetOwnership();
	UE_LOG(LogSavageSuperStormRuntime, Display,
		TEXT("Profile edit history checkpointed behind an undo barrier (%d live keys)."),
		Prepared.Num());
	return true;
}

bool UStormVerticalProfileToolComponent::ResetToNewProfile()
{
	// This is intentionally outside the transaction system. A new profile is a
	// document boundary, not another state in the profile that was just released.
	ProfileSequenceRuntime = FProfileSequenceRuntimeState();
	SetComponentTickEnabled(false);
	ResetKeyRenderTargets();

	// Give the new session its own registry. Transactions can still retain the old
	// child objects, but nothing in the new workspace can resolve or replay them.
	EditUndoState = nullptr;
	EnsureEditUndoState();

	PersistentProfileAsset = nullptr;
	ResetProfileParamsToDefaults();
	bParameterizeMode = true;
	bProfilesInitialized = false;
	MarkPackageDirty();

	const bool bInitialized = EnsureProfilesInitialized();
	if (bInitialized)
	{
		NotifyProfileRenderDataChanged();
	}
	return bInitialized;
}

bool UStormVerticalProfileToolComponent::GetKeyRenderTargetState(
	const FGuid& KeyId,
	UTextureRenderTarget2D*& OutBottom,
	UTextureRenderTarget2D*& OutTop,
	UTextureRenderTarget2D*& OutAnvil,
	FStormProfileParams& OutParams) const
{
	OutBottom = nullptr;
	OutTop = nullptr;
	OutAnvil = nullptr;

	const FStormProfileKeyRenderTargetSet* State =
		KeyRenderTargets.Find(KeyId);
	if (!State)
	{
		return false;
	}

	OutBottom = State->Bottom;
	OutTop = State->Top;
	OutAnvil = State->Anvil;
	OutParams = State->Params;
	return OutBottom && OutTop && OutAnvil;
}

bool UStormVerticalProfileToolComponent::HasKeyRenderTargets(
	const FGuid& KeyId) const
{
	return KeyRenderTargets.Contains(KeyId);
}

bool UStormVerticalProfileToolComponent::HasAnyKeyRenderTargets() const
{
	return !KeyRenderTargets.IsEmpty();
}

void UStormVerticalProfileToolComponent::MarkKeyRenderTargetsSaved(
	const FGuid& KeyId)
{
	if (KeyId == ActiveKeyRenderTargetId && IsHistoryStrokeOpen())
	{
		// A saved content identity must describe a completed graph state, never the
		// parent underneath a still-pending stroke.
		EndStroke();
	}
	if (!KeyRenderTargets.Contains(KeyId))
	{
		return;
	}

	// Deliberately does NOT re-anchor the origin. The saved content identity lives
	// outside the transacted bookmark, so undo can move the current state without
	// also restoring the pre-save marker. A restore may produce a new graph state with
	// this same content identity.
	if (const FStormProfileKeyHistory* History = FindKeyHistory(KeyId))
	{
		const FGuid CurrentStateId = GetCurrentHistoryStateId(KeyId);
		if (!History->ContainsState(CurrentStateId))
		{
			return;
		}
		FStormProfileSavedState& SavedState =
			SavedStatesByKey.FindOrAdd(KeyId);
		SavedState.ContentId = History->ResolveContentId(CurrentStateId);
		SavedState.ReplayDepthHint = History->GetDepth(CurrentStateId);
	}
}

int32 UStormVerticalProfileToolComponent::GetSavedReplayDepthHint(
	const FGuid& KeyId) const
{
	if (const FStormProfileSavedState* SavedState =
			SavedStatesByKey.Find(KeyId))
	{
		return SavedState->ReplayDepthHint;
	}
	return INDEX_NONE;
}

void UStormVerticalProfileToolComponent::RestoreKeyRenderTargetState(
	const FGuid& KeyId,
	FStormProfileKeyRenderTargetSet& State)
{
	ActiveKeyRenderTargetId = KeyId;
	BottomTypeProfileRT = State.Bottom;
	TopTypeProfileRT = State.Top;
	AnvilProfileRT = State.Anvil;
	StashTopRT = State.StashTop;
	StashAnvilRT = State.StashAnvil;
	StormProfileParams = State.Params;
	ValidateKeyRenderTargetOwnership();
}

void UStormVerticalProfileToolComponent::UpdateActiveKeyParams()
{
	if (FStormProfileKeyRenderTargetSet* State =
			KeyRenderTargets.Find(ActiveKeyRenderTargetId))
	{
		State->Params = StormProfileParams;
	}
}

void UStormVerticalProfileToolComponent::ValidateKeyRenderTargetOwnership() const
{
#if DO_CHECK
	TMap<const UTextureRenderTarget2D*, FGuid> Owners;
	auto Claim = [&Owners](
		const UTextureRenderTarget2D* RenderTarget,
		const FGuid& KeyId,
		const TCHAR* SurfaceName)
	{
		if (!RenderTarget)
		{
			return;
		}

		if (const FGuid* ExistingOwner = Owners.Find(RenderTarget))
		{
			ensureMsgf(*ExistingOwner == KeyId,
				TEXT("Profile render target %s is shared by keys %s and %s (%s)."),
				*GetNameSafe(RenderTarget),
				*ExistingOwner->ToString(EGuidFormats::Digits),
				*KeyId.ToString(EGuidFormats::Digits),
				SurfaceName);
			return;
		}

		Owners.Add(RenderTarget, KeyId);
	};

	for (const TPair<FGuid, FStormProfileKeyRenderTargetSet>& Pair :
			KeyRenderTargets)
	{
		Claim(Pair.Value.Bottom, Pair.Key, TEXT("Bottom"));
		Claim(Pair.Value.Top, Pair.Key, TEXT("Top"));
		Claim(Pair.Value.Anvil, Pair.Key, TEXT("Anvil"));
		Claim(Pair.Value.StashTop, Pair.Key, TEXT("StashTop"));
		Claim(Pair.Value.StashAnvil, Pair.Key, TEXT("StashAnvil"));
	}
	for (const TPair<FGuid, FStormProfileKeyRenderTargetSet>& Pair :
			TombstonedKeyRenderTargets)
	{
		Claim(Pair.Value.Bottom, Pair.Key, TEXT("TombstoneBottom"));
		Claim(Pair.Value.Top, Pair.Key, TEXT("TombstoneTop"));
		Claim(Pair.Value.Anvil, Pair.Key, TEXT("TombstoneAnvil"));
		Claim(Pair.Value.StashTop, Pair.Key, TEXT("TombstoneStashTop"));
		Claim(Pair.Value.StashAnvil, Pair.Key, TEXT("TombstoneStashAnvil"));
	}

	ensureMsgf(
		!TombstonedKeyRenderTargets.Contains(ActiveKeyRenderTargetId),
		TEXT("Active profile key %s is tombstoned."),
		*ActiveKeyRenderTargetId.ToString(EGuidFormats::Digits));

	if (const FStormProfileKeyRenderTargetSet* ActiveState =
			KeyRenderTargets.Find(ActiveKeyRenderTargetId))
	{
		ensureMsgf(
			BottomTypeProfileRT == ActiveState->Bottom &&
			TopTypeProfileRT == ActiveState->Top &&
			AnvilProfileRT == ActiveState->Anvil &&
			StashTopRT == ActiveState->StashTop &&
			StashAnvilRT == ActiveState->StashAnvil,
			TEXT("Active profile render-target views do not match key %s."),
			*ActiveKeyRenderTargetId.ToString(EGuidFormats::Digits));
	}
#endif
}

bool UStormVerticalProfileToolComponent::AllocateKeyRenderTargets(
	const FGuid& KeyId,
	const FStormProfileParams& Params,
	FStormProfileKeyRenderTargetSet& OutState)
{
	if (!KeyId.IsValid())
	{
		return false;
	}

	const int32 Resolution =
		FMath::Max(Params.StormProfileResolution, 1);
	const FString Prefix = FString::Printf(
		TEXT("RT_StormKey_%s"),
		*KeyId.ToString(EGuidFormats::Digits));
	OutState.Bottom = CreateProfileRenderTarget(
		FName(*(Prefix + TEXT("_Bottom"))), Resolution);
	OutState.Top = CreateProfileRenderTarget(
		FName(*(Prefix + TEXT("_Top"))), Resolution);
	OutState.Anvil = CreateProfileRenderTarget(
		FName(*(Prefix + TEXT("_Anvil"))), Resolution);
	OutState.StashTop = CreateProfileRenderTarget(
		FName(*(Prefix + TEXT("_StashTop"))), Resolution);
	OutState.StashAnvil = CreateProfileRenderTarget(
		FName(*(Prefix + TEXT("_StashAnvil"))), Resolution);
	OutState.Params = Params;
	return OutState.Bottom && OutState.Top && OutState.Anvil &&
		OutState.StashTop && OutState.StashAnvil;
}

void UStormVerticalProfileToolComponent::EnsureScratchWorkingSet()
{
	if (ActiveKeyRenderTargetId.IsValid())
	{
		return;
	}
	if (!BottomTypeProfileRT || !TopTypeProfileRT || !AnvilProfileRT ||
		!StashTopRT || !StashAnvilRT)
	{
		return;
	}

	// The working set points at the live RTs rather than allocating its own: these
	// surfaces already exist and the cloud material is bound to them.
	const FGuid ScratchId = FGuid::NewGuid();
	ScratchWorkingSetId = ScratchId;
	FStormProfileKeyRenderTargetSet& State = KeyRenderTargets.Add(ScratchId);
	State.Bottom = BottomTypeProfileRT;
	State.Top = TopTypeProfileRT;
	State.Anvil = AnvilProfileRT;
	State.StashTop = StashTopRT;
	State.StashAnvil = StashAnvilRT;
	State.Params = StormProfileParams;
	ActiveKeyRenderTargetId = ScratchId;

	// Anchor the origin on the parametric surfaces just built, so replaying an
	// empty log reproduces exactly what the user sees before their first edit.
	FlushRenderingCommands();
	BlitToRenderTarget(TopTypeProfileRT, StashTopRT);
	BlitToRenderTarget(AnvilProfileRT, StashAnvilRT);
	FlushRenderingCommands();

	EnsureKeyHistory(ScratchId, StormProfileParams);
	// The parametric surfaces are this document's baseline, exactly as the surfaces
	// loaded from a key are the baseline on the hydrated path. Without this the set
	// reads as unsaved the instant it exists, and closing the painter without ever
	// touching it would ask the user to save changes they never made.
	MarkKeyRenderTargetsSaved(ScratchId);
	ValidateKeyRenderTargetOwnership();
}

void UStormVerticalProfileToolComponent::EnsureEditUndoState()
{
	if (!EditUndoState)
	{
		EditUndoState = NewObject<UStormProfileUndoState>(
			this,
			NAME_None,
			RF_Transient);
	}
	else
	{
		EditUndoState->ClearFlags(RF_Transactional);
	}
}

UStormProfileKeyUndoState*
UStormVerticalProfileToolComponent::EnsureKeyUndoState(const FGuid& KeyId)
{
	EnsureEditUndoState();
	if (!EditUndoState || !KeyId.IsValid())
	{
		return nullptr;
	}

	if (TObjectPtr<UStormProfileKeyUndoState>* Existing =
			EditUndoState->KeyStates.Find(KeyId))
	{
		if (Existing->Get())
		{
			Existing->Get()->SetFlags(RF_Transactional);
			Existing->Get()->HistoryId = KeyId;
		}
		return Existing->Get();
	}

	UStormProfileKeyUndoState* KeyState =
		NewObject<UStormProfileKeyUndoState>(
			EditUndoState,
			NAME_None,
			RF_Transient | RF_Transactional);
	KeyState->HistoryId = KeyId;
	EditUndoState->KeyStates.Add(KeyId, KeyState);
	return KeyState;
}

UStormProfileKeyUndoState*
UStormVerticalProfileToolComponent::FindKeyUndoState(const FGuid& KeyId)
{
	if (!EditUndoState)
	{
		return nullptr;
	}
	const TObjectPtr<UStormProfileKeyUndoState>* State =
		EditUndoState->KeyStates.Find(KeyId);
	return State ? State->Get() : nullptr;
}

const UStormProfileKeyUndoState*
UStormVerticalProfileToolComponent::FindKeyUndoState(const FGuid& KeyId) const
{
	if (!EditUndoState)
	{
		return nullptr;
	}
	const TObjectPtr<UStormProfileKeyUndoState>* State =
		EditUndoState->KeyStates.Find(KeyId);
	return State ? State->Get() : nullptr;
}

UStormProfileKeyUndoState*
UStormVerticalProfileToolComponent::FindActiveKeyUndoState()
{
	return FindKeyUndoState(ActiveKeyRenderTargetId);
}

const UStormProfileKeyUndoState*
UStormVerticalProfileToolComponent::FindActiveKeyUndoState() const
{
	return FindKeyUndoState(ActiveKeyRenderTargetId);
}

bool UStormVerticalProfileToolComponent::IsHistoryStrokeOpen() const
{
	return EditUndoState &&
		EditUndoState->IsStrokeOpen(ActiveKeyRenderTargetId);
}

void UStormVerticalProfileToolComponent::ModifyEditHistory()
{
	if (UStormProfileKeyUndoState* KeyState = FindActiveKeyUndoState();
		KeyState && EditUndoState)
	{
		EditUndoState->BeginStandaloneEdit(*KeyState);
	}
}

FStormProfileKeyHistory& UStormVerticalProfileToolComponent::EnsureKeyHistory(
	const FGuid& KeyId,
	const FStormProfileParams& OriginParams)
{
	UStormProfileKeyUndoState* KeyState = EnsureKeyUndoState(KeyId);
	check(KeyState && EditUndoState);
	FStormProfileKeyHistory& History =
		EditUndoState->KeyHistories.FindOrAdd(KeyId);
	if (History.Graph.OriginStateId.IsValid())
	{
		if (!History.ContainsState(KeyState->CurrentStateId))
		{
			// This moves the bookmark backwards without an undo, so no redo tail was
			// dropped. The next edit therefore lands beside the origin's existing
			// child and looks exactly like a deliberate branch -- which would arm the
			// sweep against the entire graph. Disarm it for that one append.
			UE_LOG(LogSavageSuperStormRuntime, Warning,
				TEXT("Profile key %s: bookmark %s is unreachable and resets to the "
					 "origin. Branch pruning is suppressed for the next edit."),
				*KeyId.ToString(EGuidFormats::Digits),
				*KeyState->CurrentStateId.ToString(EGuidFormats::Digits));
			ensureMsgf(false,
				TEXT("Profile history bookmark was unreachable and reset to origin."));
			KeyState->CurrentStateId = History.Graph.OriginStateId;
			EditUndoState->MarkBookmarkRepaired(KeyId);
		}
		return History;
	}

	History.OriginParams = OriginParams;
	History.Graph.OriginStateId = FGuid::NewGuid();
	History.Graph.OriginContentId = History.Graph.OriginStateId;
	KeyState->CurrentStateId = History.Graph.OriginStateId;
	// Surfaces are at the origin, and so is the log.
	ReplayedStateByKey.Add(KeyId, History.Graph.OriginStateId);
	return History;
}

FStormProfileKeyHistory* UStormVerticalProfileToolComponent::FindActiveKeyHistory()
{
	if (EditUndoState && FindActiveKeyUndoState())
	{
		return EditUndoState->KeyHistories.Find(ActiveKeyRenderTargetId);
	}
	return nullptr;
}

const FStormProfileKeyHistory* UStormVerticalProfileToolComponent::FindKeyHistory(
	const FGuid& KeyId) const
{
	if (EditUndoState && FindKeyUndoState(KeyId))
	{
		return EditUndoState->KeyHistories.Find(KeyId);
	}
	return nullptr;
}

FGuid UStormVerticalProfileToolComponent::GetCurrentHistoryStateId(
	const FGuid& KeyId) const
{
	if (const UStormProfileKeyUndoState* State = FindKeyUndoState(KeyId))
	{
		return State->CurrentStateId;
	}
	return FGuid();
}

bool UStormVerticalProfileToolComponent::IsHistorySaved(
	const FGuid& KeyId,
	const FStormProfileKeyHistory& History) const
{
	const FStormProfileSavedState* SavedState =
		SavedStatesByKey.Find(KeyId);
	const FGuid CurrentStateId = GetCurrentHistoryStateId(KeyId);
	return SavedState && SavedState->ContentId.IsValid() &&
		History.ContainsState(CurrentStateId) &&
		SavedState->ContentId == History.ResolveContentId(CurrentStateId);
}

FGuid UStormVerticalProfileToolComponent::AppendHistoryEdit(
	FStormProfileKeyHistory& History,
	UStormProfileKeyUndoState& KeyState,
	const FStormProfileEdit& Edit,
	const FGuid& RequestedContentId)
{
	if (!EditUndoState)
	{
		return FGuid();
	}
	bool bCreatedBranch = false;
	const FGuid NewStateId = History.Append(
		KeyState.CurrentStateId,
		Edit,
		FGuid(),
		RequestedContentId,
		&bCreatedBranch);
	if (!NewStateId.IsValid())
	{
		return NewStateId;
	}

	EditUndoState->TrackAppendedState(
		KeyState.HistoryId,
		NewStateId,
		bCreatedBranch);
	return NewStateId;
}

void UStormVerticalProfileToolComponent::RecordOp(const FStormProfileOp& Op)
{
	if (GIsTransacting)
	{
		// Undo is restoring the log right now; appending to it would fight the
		// restore. See RecordReseedFromParams for why this guard exists.
		return;
	}

	FStormProfileKeyHistory* History = FindActiveKeyHistory();
	UStormProfileKeyUndoState* KeyState = FindActiveKeyUndoState();
	if (!History || !KeyState ||
		!History->ContainsState(KeyState->CurrentStateId))
	{
		// No key working set yet -- the component is on its pre-asset scratch
		// surfaces, which have nothing to replay against.
		return;
	}

	if (!IsHistoryStrokeOpen())
	{
		ModifyEditHistory();
	}
	else
	{
		const int64 MaxEditReplayCost =
			SavageSuperStorm::CVarProfileHistoryMaxEditReplayCost.GetValueOnGameThread();
		if (MaxEditReplayCost > 0 &&
			!PendingStrokeEdit.Ops.IsEmpty() &&
			PendingStrokeEdit.GetReplayCost() + Op.GetReplayCost() >
				MaxEditReplayCost)
		{
			// This seals an internal replay chunk, not the gesture. BeginStroke called
			// Modify only once, so every chunk remains one atomic Unreal undo step.
			FlushPendingStrokeEdit();
		}
		PendingStrokeEdit.Ops.Add(Op);
		return;
	}

	FStormProfileEdit Edit;
	Edit.Type = EStormProfileEditType::Stroke;
	Edit.Ops.Add(Op);
	const FGuid NewStateId = AppendHistoryEdit(
		*History,
		*KeyState,
		Edit);
	if (!NewStateId.IsValid())
	{
		return;
	}
	KeyState->CurrentStateId = NewStateId;
	// The caller applied this edit to the surfaces before recording it, so the
	// pixels already reflect the new position.
	ReplayedStateByKey.Add(
		ActiveKeyRenderTargetId,
		NewStateId);
}

void UStormVerticalProfileToolComponent::BeginStroke()
{
	UStormProfileKeyUndoState* KeyState = FindActiveKeyUndoState();
	if (FindActiveKeyHistory() && KeyState && EditUndoState &&
		EditUndoState->BeginStroke(*KeyState))
	{
		PendingStrokeEdit = FStormProfileEdit();
		PendingStrokeEdit.Type = EStormProfileEditType::Stroke;
	}
}

void UStormVerticalProfileToolComponent::EndStroke()
{
	if (!IsHistoryStrokeOpen())
	{
		return;
	}
	EditUndoState->EndStroke(ActiveKeyRenderTargetId);
	if (!FlushPendingStrokeEdit())
	{
		ensureMsgf(false,
			TEXT("Could not seal the pending profile stroke into its history."));
		PendingStrokeEdit = FStormProfileEdit();
	}
}

bool UStormVerticalProfileToolComponent::FlushPendingStrokeEdit()
{
	if (PendingStrokeEdit.Ops.IsEmpty())
	{
		return true;
	}

	FStormProfileKeyHistory* History = FindActiveKeyHistory();
	UStormProfileKeyUndoState* KeyState = FindActiveKeyUndoState();
	if (!History || !KeyState ||
		!History->ContainsState(KeyState->CurrentStateId))
	{
		return false;
	}

	const FGuid NewStateId = AppendHistoryEdit(
		*History,
		*KeyState,
		PendingStrokeEdit);
	if (!NewStateId.IsValid())
	{
		return false;
	}
	PendingStrokeEdit = FStormProfileEdit();
	PendingStrokeEdit.Type = EStormProfileEditType::Stroke;
	KeyState->CurrentStateId = NewStateId;
	ReplayedStateByKey.Add(
		ActiveKeyRenderTargetId,
		NewStateId);
	return true;
}

void UStormVerticalProfileToolComponent::RecordReseedFromParams()
{
	// Undo reaches this function. UObject::PostEditUndo calls PostEditChange(),
	// which fabricates an empty FPropertyChangedEvent -- Unspecified change type,
	// null property -- and hands it to PostEditChangeProperty, which is
	// indistinguishable from a real param edit by inspection alone.
	//
	// Recording there would create a new node while the bookmark is being restored
	// and sync the reflected-position shadow, so CollectStaleKeys would report
	// nothing stale and the replay that should restore the pixels would never run.
	if (GIsTransacting)
	{
		return;
	}

	FStormProfileKeyHistory* History = FindActiveKeyHistory();
	UStormProfileKeyUndoState* KeyState = FindActiveKeyUndoState();
	if (!History || !KeyState ||
		!History->ContainsState(KeyState->CurrentStateId))
	{
		return;
	}
	ModifyEditHistory();

	FStormProfileOp Op;
	Op.Type = EStormProfileOpType::ReseedFromParams;
	Op.PayloadId = History->AddParamsPayload(StormProfileParams);
	FStormProfileEdit Edit;
	Edit.Type = EStormProfileEditType::ReseedFromParams;
	Edit.Ops.Add(Op);
	// Completed property changes always extend the current head. This keeps a
	// parameter edit from looking like a branch created after undo.
	const FGuid NewStateId = AppendHistoryEdit(
		*History,
		*KeyState,
		Edit);
	if (!NewStateId.IsValid())
	{
		return;
	}
	KeyState->CurrentStateId = NewStateId;
	ReplayedStateByKey.Add(
		ActiveKeyRenderTargetId,
		NewStateId);
}

bool UStormVerticalProfileToolComponent::ReplayKeyInto(
	const FStormProfileKeyHistory& History,
	const FGuid& TargetStateId,
	UTextureRenderTarget2D* OriginTop,
	UTextureRenderTarget2D* OriginAnvil,
	UTextureRenderTarget2D* OutTop,
	UTextureRenderTarget2D* OutAnvil)
{
	if (!OutTop || !OutAnvil || !OriginTop || !OriginAnvil)
	{
		return false;
	}

	TArray<const FStormReplayHistoryNode*> Path;
	if (!History.BuildPath(TargetStateId, Path))
	{
		return false;
	}

	// The root is the stash pixels this key was anchored on. Passed in rather than
	// read from the live StashTopRT/StashAnvilRT members, because those track the
	// *active* key -- replaying a non-active key off them would silently start from
	// the wrong origin. Resolve the path first so corrupt graph data cannot partly
	// overwrite the live surfaces before replay reports failure.
	if (!BlitToRenderTarget(OriginTop, OutTop) ||
		!BlitToRenderTarget(OriginAnvil, OutAnvil))
	{
		return false;
	}

	for (const FStormReplayHistoryNode* Node : Path)
	{
		if (!Node)
		{
			return false;
		}
		const FStormProfileEdit* Edit = History.FindEdit(*Node);
		if (!Edit)
		{
			return false;
		}
		for (const FStormProfileOp& Op : Edit->Ops)
		{
			UTextureRenderTarget2D* OpTarget =
				Op.bTargetAnvil ? OutAnvil : OutTop;

			switch (Op.Type)
			{
			case EStormProfileOpType::Stamp:
				StampBrushInto(
					OpTarget,
					FVector2D(Op.BrushUV.X, Op.BrushUV.Y),
					Op.BrushRadiusUV,
					Op.Strength,
					Op.Value,
					Op.bErase,
					Op.bOverwrite);
				break;

			case EStormProfileOpType::ReseedFromParams:
				if (const FStormProfileParams* OpParams =
						History.ParamsPayloads.Find(Op.PayloadId))
				{
					BuildTopTypeProfile(OutTop, *OpParams);
					BuildAnvilProfile(OutAnvil, *OpParams);
				}
				break;

			case EStormProfileOpType::LoadFromTexture:
				// A deleted source degrades to leaving the surface untouched rather
				// than failing the whole replay; the verify diff will surface it.
				if (const TObjectPtr<UTexture2D>* Texture =
						History.TexturePayloads.Find(Op.PayloadId))
				{
					if (UTexture2D* Source = Texture->Get())
					{
						BlitToRenderTarget(Source, OpTarget);
					}
				}
				break;

			default:
				break;
			}
		}
	}

	return true;
}

bool UStormVerticalProfileToolComponent::VerifyActiveKeyReplay(float& OutMaxDelta)
{
	OutMaxDelta = 0.f;

	const FStormProfileKeyHistory* History =
		FindKeyHistory(ActiveKeyRenderTargetId);
	if (!History || !TopTypeProfileRT || !AnvilProfileRT)
	{
		return false;
	}

	const int32 Resolution = FMath::Max(TopTypeProfileRT->SizeX, 1);
	// Unreferenced by any UPROPERTY, so these are collected after this returns.
	UTextureRenderTarget2D* ScratchTop = CreateProfileRenderTarget(
		TEXT("RT_StormReplayVerifyTop"), Resolution);
	UTextureRenderTarget2D* ScratchAnvil = CreateProfileRenderTarget(
		TEXT("RT_StormReplayVerifyAnvil"), Resolution);
	if (!ScratchTop || !ScratchAnvil)
	{
		return false;
	}

	FlushRenderingCommands();
	// Safe to use the live stash here: this only ever verifies the active key, and
	// StashTopRT/StashAnvilRT are that key's origin.
	bool bReplayed = ReplayKeyInto(
		*History,
		GetCurrentHistoryStateId(ActiveKeyRenderTargetId),
		StashTopRT,
		StashAnvilRT,
		ScratchTop,
		ScratchAnvil);
	if (bReplayed && IsHistoryStrokeOpen())
	{
		for (const FStormProfileOp& Op : PendingStrokeEdit.Ops)
		{
			if (Op.Type != EStormProfileOpType::Stamp)
			{
				bReplayed = false;
				break;
			}
			UTextureRenderTarget2D* Target =
				Op.bTargetAnvil ? ScratchAnvil : ScratchTop;
			if (!StampBrushInto(
				Target,
				FVector2D(Op.BrushUV.X, Op.BrushUV.Y),
				Op.BrushRadiusUV,
				Op.Strength,
				Op.Value,
				Op.bErase,
				Op.bOverwrite))
			{
				bReplayed = false;
				break;
			}
		}
	}
	FlushRenderingCommands();
	if (!bReplayed)
	{
		return false;
	}

	float MaxDelta = 0.f;
	const bool bCompared =
		SavageSuperStorm::CompareProfileRenderTargets(
			TopTypeProfileRT, ScratchTop, MaxDelta) &&
		SavageSuperStorm::CompareProfileRenderTargets(
			AnvilProfileRT, ScratchAnvil, MaxDelta);

	OutMaxDelta = MaxDelta;
	return bCompared;
}

// Checks the invariant the history design rests on: replaying a key's log over
// its origin must reproduce the live surfaces. Run it after painting, after a
// params edit, and after a mode toggle -- a non-zero delta means the log has
// stopped describing the pixels, which every later phase would silently inherit.
static FAutoConsoleCommand GVerifyProfileReplayCommand(
	TEXT("SavageStorm.Profile.VerifyReplay"),
	TEXT("Replays each active profile key's edit log from its origin and reports "
		 "the largest per-channel difference against the live surfaces."),
	FConsoleCommandDelegate::CreateStatic([]()
	{
		int32 Checked = 0;
		for (TObjectIterator<UStormVerticalProfileToolComponent> It; It; ++It)
		{
			UStormVerticalProfileToolComponent* Tool = *It;
			if (!IsValid(Tool))
			{
				continue;
			}
			if (!Tool->GetActiveKeyId().IsValid())
			{
				// Worth naming rather than skipping silently: losing the active key
				// is itself a bug signature (something pruned the working set), and
				// a bare "none found" gives no way to tell that apart from "no
				// profile tool in the level".
				UE_LOG(LogSavageSuperStormRuntime, Warning,
					TEXT("[%s] has no active working set (transactional=%s). "
						 "Its edit log is unreachable."),
					*Tool->GetPathName(),
					Tool->HasAnyFlags(RF_Transactional) ? TEXT("yes") : TEXT("NO"));
				++Checked;
				continue;
			}

			const FStormProfileKeyHistory* History =
				Tool->FindKeyHistory(Tool->GetActiveKeyId());
			float MaxDelta = 0.f;
			const bool bRan = Tool->VerifyActiveKeyReplay(MaxDelta);
			++Checked;

			// Count the root-to-bookmark path only. Nodes on other branches remain
			// immutable so transactions can redo to them, but they are not applied.
			int32 Stamps = 0;
			int32 Reseeds = 0;
			int32 Loads = 0;
			int32 AppliedOps = 0;
			int32 BranchNodes = 0;
			TArray<const FStormReplayHistoryNode*> AppliedPath;
			if (History)
			{
				History->BuildPath(
					Tool->GetActiveHistoryStateId(),
					AppliedPath);
				for (const FStormReplayHistoryNode* Node : AppliedPath)
				{
					if (!Node)
					{
						continue;
					}
					const FStormProfileEdit* Edit = History->FindEdit(*Node);
					if (!Edit)
					{
						continue;
					}
					for (const FStormProfileOp& Op : Edit->Ops)
					{
						++AppliedOps;
						switch (Op.Type)
						{
						case EStormProfileOpType::Stamp:            ++Stamps;  break;
						case EStormProfileOpType::ReseedFromParams: ++Reseeds; break;
						case EStormProfileOpType::LoadFromTexture:  ++Loads;   break;
						default: break;
						}
					}
				}
				BranchNodes = History->Graph.Nodes.Num() - AppliedPath.Num();
			}

			UE_LOG(LogSavageSuperStormRuntime, Display,
				TEXT("[%s] key %s: %d edits / %d ops applied (%d stamp / %d reseed / %d load), ")
				TEXT("%d branch-only nodes, depth %d, saved-hint %d, %s -- replay %s, max delta %.6f"),
				*Tool->GetPathName(),
				*Tool->GetActiveKeyId().ToString(EGuidFormats::Digits),
				AppliedPath.Num(),
				AppliedOps,
				Stamps, Reseeds, Loads,
				BranchNodes,
				AppliedPath.Num(),
				Tool->GetSavedReplayDepthHint(Tool->GetActiveKeyId()),
				Tool->IsActiveKeyModified() ? TEXT("DIRTY") : TEXT("clean"),
				bRan ? TEXT("ran") : TEXT("FAILED"),
				MaxDelta);
		}

		if (Checked == 0)
		{
			UE_LOG(LogSavageSuperStormRuntime, Display,
				TEXT("No profile tool component with an active key was found."));
		}
	}));
#endif

bool UStormVerticalProfileToolComponent::HasCompletePersistentProfile() const
{
	return IsProfileAssetComplete(PersistentProfileAsset);
}

bool UStormVerticalProfileToolComponent::BlitToRenderTarget(UTexture* Source, UTextureRenderTarget2D* Target)
{
	UWorld* World = GetWorld();
	if (!World || !World->Scene || !Source || !Target)
	{
		return false;
	}

	UCanvas* Canvas = nullptr;
	FVector2D CanvasSize = FVector2D::ZeroVector;
	FDrawToRenderTargetContext Context;

	UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(World, Target, Canvas, CanvasSize, Context);
	const bool bDrew = Canvas != nullptr;
	if (bDrew)
	{
		// Full-target opaque blit. Same-size, clamp-addressed, linear (SRGB off) source
		// and target, so the sampler copies texel-for-texel; the format conversion (e.g.
		// TC_HDR Texture2D -> float RT) is handled by the sampler.
		Canvas->K2_DrawTexture(
			Source,
			FVector2D::ZeroVector, CanvasSize,            // dest rect: whole RT
			FVector2D::ZeroVector, FVector2D::UnitVector, // src UV rect: whole texture
			FLinearColor::White, BLEND_Opaque);
	}
	UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(World, Context);
	return bDrew;
}

#if WITH_EDITOR
bool UStormVerticalProfileToolComponent::IsUnsavedToAsset() const
{
	if (IsHistoryStrokeOpen() && !PendingStrokeEdit.Ops.IsEmpty())
	{
		return true;
	}

	// EnsureScratchWorkingSet normally guarantees a live set even before an asset
	// does, but it bails when the render targets are not up yet. Treat "no live sets"
	// as unsaved rather than clean. Tombstones do not count: their keys are absent
	// from the document and therefore have nothing the current asset can save.
	if (!EditUndoState || KeyRenderTargets.IsEmpty())
	{
		return true;
	}

	// Structurally absent tombstoned keys are intentionally excluded. Their edits
	// become relevant again only if undo restores the key and its workspace.
	for (const TPair<FGuid, FStormProfileKeyRenderTargetSet>& Pair :
			KeyRenderTargets)
	{
		const UStormProfileKeyUndoState* KeyState =
			FindKeyUndoState(Pair.Key);
		const FStormProfileKeyHistory* History =
			FindKeyHistory(Pair.Key);
		if (!KeyState || !History ||
			!IsHistorySaved(Pair.Key, *History))
		{
			return true;
		}
	}
	return false;
}

bool UStormVerticalProfileToolComponent::IsActiveKeyModified() const
{
	if (IsHistoryStrokeOpen() && !PendingStrokeEdit.Ops.IsEmpty())
	{
		return true;
	}
	const FStormProfileKeyHistory* History =
		FindKeyHistory(ActiveKeyRenderTargetId);
	return History &&
		!IsHistorySaved(ActiveKeyRenderTargetId, *History);
}

bool UStormVerticalProfileToolComponent::HasPaintedProfiles() const
{
	if (IsHistoryStrokeOpen() && !PendingStrokeEdit.Ops.IsEmpty())
	{
		return true;
	}
	const FStormProfileKeyHistory* History =
		FindKeyHistory(ActiveKeyRenderTargetId);
	return History && History->HasPaintAfterLastReseed(
		GetCurrentHistoryStateId(ActiveKeyRenderTargetId));
}

void UStormVerticalProfileToolComponent::MarkProfileDirty()
{
	UpdateActiveKeyParams();
	if (ProfileSequenceRuntime.bPreviewActive &&
		ComposeProfileSequenceAtCurrentTime())
	{
		NotifyProfileRenderDataChanged();
	}
}

FGuid UStormVerticalProfileToolComponent::RepairMissingHistoryState(
	const FStormProfileKeyHistory& History,
	const FGuid& MissingStateId) const
{
	// A pruned node takes its parent link with it, so the nearest surviving ancestor
	// cannot be walked to from here. The surfaces answer it instead: whatever they
	// already reflect is a real state whose pixels are correct, so adopting it
	// repairs the disagreement without replaying anything and discards only the
	// transition the caller asked for. The origin is the last resort.
	const FGuid* ReflectedState =
		ReplayedStateByKey.Find(ActiveKeyRenderTargetId);
	const FGuid Fallback =
		(ReflectedState && History.ContainsState(*ReflectedState))
			? *ReflectedState
			: History.Graph.OriginStateId;

	UE_LOG(LogSavageSuperStormRuntime, Error,
		TEXT("Profile key %s: history state %s is unreachable, so its bookmark falls "
			 "back to %s. A branch was reclaimed while undo still referenced it."),
		*ActiveKeyRenderTargetId.ToString(EGuidFormats::Digits),
		*MissingStateId.ToString(EGuidFormats::Digits),
		*Fallback.ToString(EGuidFormats::Digits));
	ensureMsgf(false,
		TEXT("Profile history state was pruned while undo still referenced it."));

	return History.ContainsState(Fallback) ? Fallback : FGuid();
}

bool UStormVerticalProfileToolComponent::ReplayActiveKeyTo(
	const FGuid& TargetStateId)
{
	FStormProfileKeyHistory* History = FindActiveKeyHistory();
	UStormProfileKeyUndoState* KeyState = FindActiveKeyUndoState();
	if (!History || !KeyState)
	{
		return false;
	}

	// Sweeping branches made states removable, so a restored bookmark can now name
	// one that is gone. Returning false here would leave the bookmark and the pixels
	// permanently disagreeing with nothing reporting it, because the undo path
	// discards this result -- so resolve to a surviving position instead.
	const FGuid ResolvedStateId = History->ContainsState(TargetStateId)
		? TargetStateId
		: RepairMissingHistoryState(*History, TargetStateId);
	if (!ResolvedStateId.IsValid())
	{
		return false;
	}

	FlushRenderingCommands();
	// Replays into the live surfaces, never repointing them -- the cloud material
	// is bound to these objects.
	const bool bReplayed = ReplayKeyInto(
		*History,
		ResolvedStateId,
		StashTopRT,
		StashAnvilRT,
		TopTypeProfileRT,
		AnvilProfileRT);
	FlushRenderingCommands();
	if (!bReplayed)
	{
		return false;
	}
	KeyState->CurrentStateId = ResolvedStateId;
	// Bottom is parametric, so rebuild it from whatever params are in effect at the
	// restored position rather than leaving it on the newer ones.
	StormProfileParams = History->ResolveParamsAt(ResolvedStateId);
	BuildBottomTypeProfile();
	// The surfaces now reflect this stable state identity; record that so a later
	// undo can distinguish every immutable branch.
	ReplayedStateByKey.Add(
		ActiveKeyRenderTargetId,
		ResolvedStateId);
	UpdateActiveKeyParams();

	if (ProfileSequenceRuntime.bPreviewActive &&
		ComposeProfileSequenceAtCurrentTime())
	{
		NotifyProfileRenderDataChanged();
	}
	return true;
}

bool UStormVerticalProfileToolComponent::RevertActiveKeyToSaved()
{
	FStormProfileKeyHistory* History = FindActiveKeyHistory();
	UStormProfileKeyUndoState* KeyState = FindActiveKeyUndoState();
	if (!History || !KeyState ||
		!History->ContainsState(KeyState->CurrentStateId))
	{
		return false;
	}

	const FStormProfileSavedState* SavedState =
		SavedStatesByKey.Find(ActiveKeyRenderTargetId);
	if (SavedState)
	{
		if (SavedState->ContentId ==
			History->ResolveContentId(KeyState->CurrentStateId))
		{
			return false;
		}
		ModifyEditHistory();
		return AppendSavedAssetRestore(*SavedState);
	}

	// A scratch key has no asset state yet, so append an absolute reseed that
	// reproduces its parametric origin. This remains a normal forward edit: undo
	// returns to the pre-revert head rather than teleporting within the graph.
	const FGuid OriginContentId =
		History->ResolveContentId(History->Graph.OriginStateId);
	if (OriginContentId ==
		History->ResolveContentId(KeyState->CurrentStateId))
	{
		return false;
	}
	ModifyEditHistory();

	FStormProfileOp ReseedOp;
	ReseedOp.Type = EStormProfileOpType::ReseedFromParams;
	ReseedOp.PayloadId = History->AddParamsPayload(History->OriginParams);
	FStormProfileEdit RestoreEdit;
	RestoreEdit.Type = EStormProfileEditType::RestoreOrigin;
	RestoreEdit.Ops.Add(ReseedOp);
	const FGuid RestoreStateId = AppendHistoryEdit(
		*History,
		*KeyState,
		RestoreEdit,
		OriginContentId);
	return RestoreStateId.IsValid() &&
		ReplayActiveKeyTo(RestoreStateId);
}

bool UStormVerticalProfileToolComponent::AppendSavedAssetRestore(
	const FStormProfileSavedState& SavedState)
{
	if (!PersistentProfileAsset ||
		!ActiveKeyRenderTargetId.IsValid() ||
		!SavedState.ContentId.IsValid())
	{
		return false;
	}

	const int32 KeyIndex =
		PersistentProfileAsset->Keys.IndexOfByPredicate(
			[this](const FStormProfileKey& Key)
			{
				return Key.KeyId == ActiveKeyRenderTargetId;
			});
	if (!PersistentProfileAsset->Keys.IsValidIndex(KeyIndex) ||
		!PersistentProfileAsset->IsKeyComplete(KeyIndex))
	{
		return false;
	}

	const FStormProfileKey& Key = PersistentProfileAsset->Keys[KeyIndex];
	UTexture2D* TopProfile = Key.TopProfile.Get();
	UTexture2D* BottomProfile = Key.BottomProfile.Get();
	UTexture2D* AnvilProfile = Key.AnvilProfile.Get();
	if (!TopProfile || !BottomProfile || !AnvilProfile ||
		!TopTypeProfileRT || !BottomTypeProfileRT || !AnvilProfileRT ||
		!StashTopRT || !StashAnvilRT)
	{
		return false;
	}

	FTextureCompilingManager::Get().FinishCompilation(
		{ TopProfile, BottomProfile, AnvilProfile });

	FStormProfileKeyHistory* History = FindActiveKeyHistory();
	UStormProfileKeyUndoState* KeyState = FindActiveKeyUndoState();
	if (!History || !KeyState ||
		!History->ContainsState(KeyState->CurrentStateId))
	{
		return false;
	}
	UTexture2D* TopSnapshot =
		SavageSuperStorm::SnapshotProfileHistoryTexture(
			TopProfile,
			EditUndoState);
	UTexture2D* AnvilSnapshot =
		SavageSuperStorm::SnapshotProfileHistoryTexture(
			AnvilProfile,
			EditUndoState);
	if (!TopSnapshot || !AnvilSnapshot)
	{
		return false;
	}
	FTextureCompilingManager::Get().FinishCompilation(
		{ TopSnapshot, AnvilSnapshot });

	// One atomic restore edit reseeds the saved params and then replaces Top and
	// Anvil with the exact baked pixels. Its graph state remains unique while its
	// content identity matches the asset, so dirty checking becomes clean.
	FStormProfileEdit RestoreEdit;
	RestoreEdit.Type = EStormProfileEditType::RestoreFromAsset;

	FStormProfileOp ReseedOp;
	ReseedOp.Type = EStormProfileOpType::ReseedFromParams;
	ReseedOp.PayloadId = History->AddParamsPayload(Key.Params);
	RestoreEdit.Ops.Add(ReseedOp);

	FStormProfileOp TopLoadOp;
	TopLoadOp.Type = EStormProfileOpType::LoadFromTexture;
	TopLoadOp.PayloadId = History->AddTexturePayload(TopSnapshot);
	RestoreEdit.Ops.Add(TopLoadOp);

	FStormProfileOp AnvilLoadOp;
	AnvilLoadOp.Type = EStormProfileOpType::LoadFromTexture;
	AnvilLoadOp.bTargetAnvil = true;
	AnvilLoadOp.PayloadId = History->AddTexturePayload(AnvilSnapshot);
	RestoreEdit.Ops.Add(AnvilLoadOp);

	const FGuid RestoreStateId = AppendHistoryEdit(
		*History,
		*KeyState,
		RestoreEdit,
		SavedState.ContentId);
	if (!RestoreStateId.IsValid() ||
		!ReplayActiveKeyTo(RestoreStateId))
	{
		return false;
	}

	// Bottom is also baked. Replay rebuilt it from the saved params; use the asset
	// pixels here so the immediate reverted state is an exact asset match.
	FlushRenderingCommands();
	if (!BlitToRenderTarget(BottomProfile, BottomTypeProfileRT))
	{
		return false;
	}
	FlushRenderingCommands();
	bProfilesInitialized = true;

	if (ProfileSequenceRuntime.bPreviewActive &&
		ComposeProfileSequenceAtCurrentTime())
	{
		NotifyProfileRenderDataChanged();
	}
	return true;
}

void UStormVerticalProfileToolComponent::CollectStaleKeys(
	TArray<FGuid>& OutStaleKeys) const
{
	if (!EditUndoState)
	{
		return;
	}
	// Only live workspaces own pixels that can be stale. Tombstones retain their
	// replay identities but must not pull selection toward a structurally absent key.
	for (const TPair<FGuid, FStormProfileKeyRenderTargetSet>& Pair :
			KeyRenderTargets)
	{
		const UStormProfileKeyUndoState* KeyState =
			FindKeyUndoState(Pair.Key);
		if (!KeyState)
		{
			continue;
		}
		const FGuid* ReflectedState = ReplayedStateByKey.Find(Pair.Key);
		if (!ReflectedState ||
			*ReflectedState != KeyState->CurrentStateId)
		{
			OutStaleKeys.Add(Pair.Key);
		}
	}
}

bool UStormVerticalProfileToolComponent::ReplayActiveKeyIfStale()
{
	const FStormProfileKeyHistory* History =
		FindKeyHistory(ActiveKeyRenderTargetId);
	if (!History)
	{
		return false;
	}

	const FGuid TargetStateId =
		GetCurrentHistoryStateId(ActiveKeyRenderTargetId);
	const FGuid* ReflectedState =
		ReplayedStateByKey.Find(ActiveKeyRenderTargetId);
	if (ReflectedState &&
		*ReflectedState == TargetStateId)
	{
		return false;
	}
	return ReplayActiveKeyTo(TargetStateId);
}

void UStormVerticalProfileToolComponent::SetParameterizeMode(bool bEnable)
{
	if (bParameterizeMode == bEnable)
	{
		return;
	}

	bParameterizeMode = bEnable;

	if (bEnable)
	{
		// Paint -> Parameterize: rebuild Top and Anvil, discarding their brushwork.
		BuildTopTypeProfile(TopTypeProfileRT);
		BuildAnvilProfile();
		RecordReseedFromParams();
		MarkProfileDirty();
	}
	// Parameterize -> Paint: the current macro becomes the paint base; no reseed needed.
}
#endif // WITH_EDITOR

bool UStormVerticalProfileToolComponent::SampleVerticalProfileCurve(
	const FRuntimeFloatCurve& Curve,
	uint16 NumSamples,
	TArray<float>& Out,
	float MinT,
	float MaxT) const
{
	Out.Reset();
	const FRichCurve* RichCurve = Curve.GetRichCurveConst();

	if (!RichCurve || RichCurve->GetNumKeys() < 2 || NumSamples < 2 || MinT >= MaxT)
	{
		return false;
	}

	Out.Reserve(NumSamples);
	const float TimeStep = (MaxT - MinT) / (float)(NumSamples - 1);
	for (int32 i = 0; i < NumSamples; ++i)
	{
		Out.Add(FMath::Clamp(RichCurve->Eval(MinT + TimeStep * i), 0.f, 1.f));
	}
	return true;
}

