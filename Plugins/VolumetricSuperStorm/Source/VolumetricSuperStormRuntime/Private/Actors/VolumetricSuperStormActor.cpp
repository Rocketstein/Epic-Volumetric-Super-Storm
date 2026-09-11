// Copyright 2026 GoroGoro. All Rights Reserved.

/**
 * @file VolumetricSuperStormActor.cpp
 * @brief Defines the single supported storm actor and its public controls.
 */

#include "Actors/VolumetricSuperStormActor.h"
#include "Actors/StormDefaultConfiguration.h"
#include "VolumetricSuperStormRuntime.h"

#include "Assets/StormPresetDataAsset.h"
#include "Assets/StormVerticalProfileAsset.h"
#include "Assets/StormWindFlowMapDataAsset.h"
#include "Components/SceneComponent.h"
#include "Components/StormFlowMapComponent.h"
#include "Components/StormLightningComponent.h"
#include "Components/StormMaterialBinderComponent.h"
#include "Components/StormVerticalProfileToolComponent.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Motion/StormMotionSettingsUtils.h"
#include "Subsystems/StormRenderWorldSubsystem.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UnrealType.h"

#if WITH_EDITOR
#include "ImageUtils.h"
#include "Components/BillboardComponent.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#endif

namespace
{
	constexpr float MinFormationAnimationDurationSeconds = 0.1f;

	float SanitizeFormationAnimationDuration(float DurationSeconds)
	{
		return FMath::Max(DurationSeconds, MinFormationAnimationDurationSeconds);
	}
}

AVolumetricSuperStormActor::AVolumetricSuperStormActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick          = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	PrimaryActorTick.TickInterval          = 0.0f;

#if WITH_EDITORONLY_DATA
	bIsSpatiallyLoaded = false;
#endif

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	MaterialBinder      = CreateDefaultSubobject<UStormMaterialBinderComponent>(TEXT("MaterialBinder"));
	LightningComponent  = CreateDefaultSubobject<UStormLightningComponent>(TEXT("LightningComponent"));
	VerticalProfileTool = CreateDefaultSubobject<UStormVerticalProfileToolComponent>(TEXT("VerticalProfileTool"));
	FlowMapComponent    = CreateDefaultSubobject<UStormFlowMapComponent>(TEXT("FlowMapComponent"));

	const VolumetricSuperStorm::Defaults::FStormDefaultConfiguration DefaultConfiguration =
		VolumetricSuperStorm::Defaults::MakeStormDefaultConfiguration();
	ShapeSettings                 = DefaultConfiguration.ShapeSettings;
	MotionSettings                = DefaultConfiguration.MotionSettings;
	LightningSequence             = DefaultConfiguration.LightningSequence;
	FormationDurationSeconds      = DefaultConfiguration.FormationDurationSeconds;
	DissolutionDurationSeconds    = DefaultConfiguration.DissolutionDurationSeconds;
	GroundStrikeProbability       = DefaultConfiguration.GroundStrikeProbability;
	LightningRepeatDelaySeconds   = DefaultConfiguration.LightningRepeatDelaySeconds;

	static ConstructorHelpers::FObjectFinder<UStormPresetDataAsset> DefaultPreset(
		TEXT("/VolumetricSuperStorm/VolumetricSuperStorm/Preset/SP_Default.SP_Default"));
	const FStormPresetData* DefaultPresetData = nullptr;
	if (DefaultPreset.Succeeded())
	{
		DefaultPresetData            = &DefaultPreset.Object->PresetData;
		ShapeSettings                = DefaultPresetData->ShapeSettings;
		MotionSettings               = DefaultPresetData->MotionSettings;
		LightningSequence            = DefaultPresetData->LightningSequence;
		FormationDurationSeconds     = SanitizeFormationAnimationDuration(DefaultPresetData->FormationDurationSeconds);
		DissolutionDurationSeconds   = SanitizeFormationAnimationDuration(DefaultPresetData->DissolutionDurationSeconds);
		GroundStrikeProbability      = FMath::Clamp(DefaultPresetData->GroundStrikeProbability, 0.0f, 1.0f);
		LightningRepeatDelaySeconds  = FMath::Max(0.1f, DefaultPresetData->LightningRepeatDelaySeconds);
		ShapeSettings.ShapeCurves.EnsureNamedCurves();
		VolumetricSuperStorm::Motion::SynchronizeRingArrays(MotionSettings);
	}

	UStormVerticalProfileAsset* DefaultVerticalProfile =
		DefaultPresetData ? DefaultPresetData->VerticalProfile.Get() : nullptr;
	if (!DefaultVerticalProfile)
	{
		static ConstructorHelpers::FObjectFinder<UStormVerticalProfileAsset> FallbackVerticalProfile(
			TEXT("/VolumetricSuperStorm/VolumetricSuperStorm/VerticalProfile/VP_Default.VP_Default"));
		DefaultVerticalProfile = FallbackVerticalProfile.Object;
	}
	if (DefaultVerticalProfile)
	{
		VerticalProfileTool->PersistentProfileAsset = DefaultVerticalProfile;
		if (const FStormProfileKey* PrimaryKey = DefaultVerticalProfile->GetPrimaryKey())
		{
			VerticalProfileTool->StormProfileParams = PrimaryKey->Params;
		}
	}

	UStormWindFlowMapDataAsset* DefaultFlowMap =
		DefaultPresetData ? DefaultPresetData->FlowMap.Get() : nullptr;
	if (!DefaultFlowMap)
	{
		static ConstructorHelpers::FObjectFinder<UStormWindFlowMapDataAsset> FallbackFlowMap(
			TEXT("/VolumetricSuperStorm/VolumetricSuperStorm/FlowMaps/FM_Default.FM_Default"));
		DefaultFlowMap = FallbackFlowMap.Object;
	}
	if (DefaultFlowMap)
	{
		FlowMapComponent->PersistentFlowMapAsset = DefaultFlowMap;
		FlowMapComponent->WorkingParams = DefaultFlowMap->Params;
	}
	FlowMapComponent->bFlowMapEnabled = DefaultPresetData ? DefaultPresetData->bFlowMapEnabled : true;

#if WITH_EDITOR
	CreateEditorIconComponent();
#endif
}

#if WITH_EDITOR
void AVolumetricSuperStormActor::CreateEditorIconComponent()
{
	EditorIcon = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("EditorIcon"), false);
	if (!EditorIcon)
	{
		return;
	}

	EditorIcon->SetupAttachment(SceneRoot);
	EditorIcon->bIsScreenSizeScaled = true;
	EditorIcon->SetUsingAbsoluteScale(true);
	EditorIcon->SetHiddenInGame(true);

	static TWeakObjectPtr<UTexture2D> IconTexture;
	if (IconTexture.IsValid())
	{
		EditorIcon->SetSprite(IconTexture.Get());
		return;
	}

	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("VolumetricSuperStorm"));
	if (!Plugin)
	{
		UE_LOG(LogVolumetricSuperStormRuntime, Warning, TEXT("Could not load the storm actor icon because the plugin was not found."));
		return;
	}

	const FString IconPath = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/StormActorIcon.png"));
	IconTexture            = FImageUtils::ImportFileAsTexture2D(IconPath);
	if (!IconTexture.IsValid())
	{
		UE_LOG(LogVolumetricSuperStormRuntime, Warning, TEXT("Could not load the storm actor icon from '%s'."), *IconPath);
		return;
	}

	EditorIcon->SetSprite(IconTexture.Get());
}
#endif

void AVolumetricSuperStormActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (VerticalProfileTool)
	{
		VerticalProfileTool->EnsureProfilesInitialized();
	}
	if (FlowMapComponent)
	{
		FlowMapComponent->EnsureFlowMapsInitialized();
	}

	VolumetricSuperStorm::Motion::SynchronizeRingArrays(MotionSettings);
	NormalizeFormationStateEndpoints();
	PrepareStormResources();

	ConfigureLightningComponent();
}

void AVolumetricSuperStormActor::Destroyed()
{
	if (MaterialBinder)
	{
		MaterialBinder->ReleaseVolumetricCloudMaterial();
	}
	UnregisterFromRenderSubsystem();
	Super::Destroyed();
}

void AVolumetricSuperStormActor::PrepareStormResources()
{
	if (IsTemplate() || !RegisterWithRenderSubsystem())
	{
		return;
	}

	if (VerticalProfileTool)
	{
		VerticalProfileTool->EnsureProfilesInitialized();
	}
	if (FlowMapComponent)
	{
		FlowMapComponent->EnsureFlowMapsInitialized();
	}

	QueueLifecycleWarmup();
	RequestRenderDataUpdate(EStormRenderUpdateScope::Full);
}

void AVolumetricSuperStormActor::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	ConfigureLightningComponent();
}

void AVolumetricSuperStormActor::BeginPlay()
{
	Super::BeginPlay();

	VolumetricSuperStorm::Motion::SynchronizeRingArrays(MotionSettings);
	NormalizeFormationStateEndpoints();
	PrepareStormResources();
	if (!bRegisteredWithRenderSubsystem)
	{
		return;
	}

	if (UStormLightningComponent* Lightning = ConfigureLightningComponent())
	{
		BindLightningEvents();
		Lightning->InitializeRuntime(MaterialBinder);
	}
}

void AVolumetricSuperStormActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UStormLightningComponent* Lightning = LightningComponent.Get())
	{
		UnbindLightningEvents();
		Lightning->StopLightning();
	}

	if (MaterialBinder)
	{
		MaterialBinder->ReleaseVolumetricCloudMaterial();
	}
	UnregisterFromRenderSubsystem();
	Super::EndPlay(EndPlayReason);
}

#if WITH_EDITOR
void AVolumetricSuperStormActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
	const FName MemberName   = PropertyChangedEvent.GetMemberPropertyName();

	if (PropertyName == GET_MEMBER_NAME_CHECKED(AVolumetricSuperStormActor, LightningRepeatDelaySeconds) || PropertyName == GET_MEMBER_NAME_CHECKED(AVolumetricSuperStormActor, GroundStrikeProbability) || MemberName == GET_MEMBER_NAME_CHECKED(AVolumetricSuperStormActor, LightningSequence))
	{
		ConfigureLightningComponent();
	}

	ShapeSettings.ShapeCurves.EnsureNamedCurves();

	VolumetricSuperStorm::Motion::SynchronizeRingArrays(MotionSettings);
	NormalizeFormationStateEndpoints();
	RebuildRenderData();
}

bool AVolumetricSuperStormActor::ShouldTickIfViewportsOnly() const
{
	const bool bFlowMapPreviewActive = FlowMapComponent && FlowMapComponent->bFlowMapEnabled;
	return bFormationAnimationActive || LifecycleWarmupFramesRemaining > 0 || bFlowMapPreviewActive || (MotionSettings.bEnabled && MotionSettings.bPreviewInEditor && !bMotionPaused);
}
#endif

void AVolumetricSuperStormActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const EStormFormationState PreviousFormationState  = FormationState;
	const bool                 bWarmupWasActive        = LifecycleWarmupFramesRemaining > 0;
	const bool                 bWarmupChanged          = TickLifecycleWarmup();
	const bool                 bFormationChanged       = bWarmupWasActive ? false : TickFormationAnimation(DeltaSeconds);
	const bool                 bMotionChanged          = TickMotion(DeltaSeconds);
	const bool                 bFlowMapAnimationActive = FlowMapComponent && FlowMapComponent->bFlowMapEnabled;
	if (!bWarmupChanged && !bFormationChanged && !bMotionChanged && !bFlowMapAnimationActive)
	{
		return;
	}

	RequestRenderDataUpdate(EStormRenderUpdateScope::Frame);
	BroadcastFormationCompletionEvents(PreviousFormationState, FormationState);
}

UTexture* AVolumetricSuperStormActor::GetBottomTypeProfileTexture() const
{
	return VerticalProfileTool ? VerticalProfileTool->BottomTypeProfileRT.Get() : nullptr;
}

UTexture* AVolumetricSuperStormActor::GetTopTypeProfileTexture() const
{
	return VerticalProfileTool ? VerticalProfileTool->TopTypeProfileRT.Get() : nullptr;
}

void AVolumetricSuperStormActor::ApplyPreset(UStormPresetDataAsset* InPreset)
{
	ApplyPresetConfiguration(InPreset, true);
}

void AVolumetricSuperStormActor::ApplyPresetConfiguration(
	UStormPresetDataAsset* InPreset,
	bool bRebuildRenderData)
{
	if (!InPreset)
	{
		UE_LOG(LogVolumetricSuperStormRuntime, Warning, TEXT("Storm '%s': Data asset is null. Failed to apply preset."), *GetName());
		return;
	}

	FStormPresetData PresetData = InPreset->PresetData;

	ShapeSettings  = PresetData.ShapeSettings;
	MotionSettings = PresetData.MotionSettings;
	VolumetricSuperStorm::Motion::SynchronizeRingArrays(MotionSettings);

	FormationDurationSeconds   = PresetData.FormationDurationSeconds;
	DissolutionDurationSeconds = PresetData.DissolutionDurationSeconds;

	if (VerticalProfileTool)
	{
		if (!VerticalProfileTool->ApplyProfileConfiguration(PresetData.VerticalProfile, false))
		{
			UE_LOG(LogVolumetricSuperStormRuntime, Warning, TEXT("Storm '%s': applied preset has an incomplete vertical profile asset."), *GetName());
		}
	}

	if (FlowMapComponent)
	{
		FlowMapComponent->ApplyFlowMapConfiguration(PresetData.FlowMap, PresetData.bFlowMapEnabled, false);
	}

	LightningSequence           = PresetData.LightningSequence;
	GroundStrikeProbability     = FMath::Clamp(PresetData.GroundStrikeProbability, 0.0f, 1.0f);
	LightningRepeatDelaySeconds = FMath::Max(0.1f, PresetData.LightningRepeatDelaySeconds);
	ConfigureLightningComponent();

	if (bRebuildRenderData)
	{
		RebuildRenderData();
	}
}

void AVolumetricSuperStormActor::StartFormationAnimation()
{
	if (FormationState == EStormFormationState::Dissolving)
	{
		FormationProgress = 1.0f - FMath::Clamp(FormationProgress, 0.0f, 1.0f);
	}
	else if (FormationState != EStormFormationState::Forming)
	{
		FormationProgress = 0.0f;
	}

	FormationState            = FormationProgress >= 1.0f ? EStormFormationState::Mature : EStormFormationState::Forming;
	bFormationAnimationActive = FormationState == EStormFormationState::Forming;
	SetActorTickEnabled(true);
	RequestRenderDataUpdate(EStormRenderUpdateScope::Frame);
}

void AVolumetricSuperStormActor::StartDissolutionAnimation()
{
	if (FormationState == EStormFormationState::Hidden)
	{
		return;
	}

	if (FormationState == EStormFormationState::Forming)
	{
		FormationProgress = 1.0f - FMath::Clamp(FormationProgress, 0.0f, 1.0f);
	}
	else if (FormationState != EStormFormationState::Dissolving)
	{
		FormationProgress = 0.0f;
	}

	FormationState            = EStormFormationState::Dissolving;
	bFormationAnimationActive = true;
	SetActorTickEnabled(true);
	RequestRenderDataUpdate(EStormRenderUpdateScope::Frame);
}

void AVolumetricSuperStormActor::SetFormationAnimationDuration(float DurationSeconds)
{
	FormationDurationSeconds = SanitizeFormationAnimationDuration(DurationSeconds);
}

void AVolumetricSuperStormActor::SetDissolutionAnimationDuration(float DurationSeconds)
{
	DissolutionDurationSeconds = SanitizeFormationAnimationDuration(DurationSeconds);
}

void AVolumetricSuperStormActor::ShowMatureStorm()
{
	FormationState            = EStormFormationState::Mature;
	FormationProgress         = 1.0f;
	bFormationAnimationActive = false;
	RequestRenderDataUpdate(EStormRenderUpdateScope::Frame);
}

void AVolumetricSuperStormActor::HideStormImmediately()
{
	FormationState            = EStormFormationState::Hidden;
	FormationProgress         = 0.0f;
	bFormationAnimationActive = false;
	RequestRenderDataUpdate(EStormRenderUpdateScope::Frame);
}

void AVolumetricSuperStormActor::BuildPresetData(FStormPresetData& OutData) const
{
	OutData.ShapeSettings  = ShapeSettings;
	OutData.MotionSettings = MotionSettings;

	OutData.FormationDurationSeconds   = FormationDurationSeconds;
	OutData.DissolutionDurationSeconds = DissolutionDurationSeconds;

	if (VerticalProfileTool)
	{
		OutData.VerticalProfile = VerticalProfileTool->PersistentProfileAsset;
	}

	if (FlowMapComponent)
	{
		OutData.FlowMap         = FlowMapComponent->PersistentFlowMapAsset;
		OutData.bFlowMapEnabled = FlowMapComponent->bFlowMapEnabled;
	}

	OutData.LightningSequence           = LightningSequence;
	OutData.GroundStrikeProbability     = GroundStrikeProbability;
	OutData.LightningRepeatDelaySeconds = LightningRepeatDelaySeconds;

	VolumetricSuperStorm::Motion::SynchronizeRingArrays(OutData.MotionSettings);
}

UStormMaterialBinderComponent* AVolumetricSuperStormActor::GetMaterialBinder() const
{
	return MaterialBinder;
}

UStormVerticalProfileToolComponent* AVolumetricSuperStormActor::GetVerticalProfileTool() const
{
	return VerticalProfileTool;
}

UStormFlowMapComponent* AVolumetricSuperStormActor::GetFlowMapComponent() const
{
	return FlowMapComponent;
}

UStormLightningComponent* AVolumetricSuperStormActor::GetLightningComponent() const
{
	return LightningComponent.Get();
}

bool AVolumetricSuperStormActor::PlayProfileSequence(bool bRestart)
{
	return VerticalProfileTool
		? VerticalProfileTool->PlayProfileSequence(bRestart)
		: false;
}

bool AVolumetricSuperStormActor::PlayProfileSequenceReverse(bool bRestart)
{
	return VerticalProfileTool
		? VerticalProfileTool->PlayProfileSequenceReverse(bRestart)
		: false;
}

void AVolumetricSuperStormActor::PauseProfileSequence()
{
	if (VerticalProfileTool)
	{
		VerticalProfileTool->PauseProfileSequence();
	}
}

void AVolumetricSuperStormActor::StopProfileSequence()
{
	if (VerticalProfileTool)
	{
		VerticalProfileTool->StopProfileSequence();
	}
}

bool AVolumetricSuperStormActor::SetProfileSequenceTime(float InTimeSeconds)
{
	return VerticalProfileTool
		? VerticalProfileTool->SetProfileSequenceTime(InTimeSeconds)
		: false;
}

void AVolumetricSuperStormActor::SetProfileSequenceLooping(bool bLooping)
{
	if (VerticalProfileTool)
	{
		VerticalProfileTool->SetProfileSequenceLooping(bLooping);
	}
}

float AVolumetricSuperStormActor::GetProfileSequenceTime() const
{
	return VerticalProfileTool
		? VerticalProfileTool->GetProfileSequenceTime()
		: 0.0f;
}

float AVolumetricSuperStormActor::GetProfileSequenceDuration() const
{
	return VerticalProfileTool
		? VerticalProfileTool->GetProfileSequenceDuration()
		: 0.0f;
}

bool AVolumetricSuperStormActor::IsProfileSequencePlaying() const
{
	return VerticalProfileTool &&
		VerticalProfileTool->IsProfileSequencePlaying();
}

bool AVolumetricSuperStormActor::IsProfileSequencePlayingInReverse() const
{
	return VerticalProfileTool &&
		VerticalProfileTool->IsProfileSequencePlaying() &&
		VerticalProfileTool->GetProfileSequencePlaybackDirection() ==
			EStormProfileSequencePlaybackDirection::Reverse;
}

bool AVolumetricSuperStormActor::IsProfileSequencePreviewActive() const
{
	return VerticalProfileTool &&
		VerticalProfileTool->IsProfileSequencePreviewActive();
}

bool AVolumetricSuperStormActor::CanPlayProfileSequence() const
{
	return VerticalProfileTool &&
		VerticalProfileTool->CanPlayProfileSequence();
}

void AVolumetricSuperStormActor::QueueLifecycleWarmup()
{
	if (bLifecycleWarmupComplete || LifecycleWarmupFramesRemaining > 0)
	{
		return;
	}

	LifecycleWarmupFramesRemaining = 2;
	SetActorTickEnabled(true);
}

bool AVolumetricSuperStormActor::TickLifecycleWarmup()
{
	if (LifecycleWarmupFramesRemaining <= 0)
	{
		return false;
	}

	--LifecycleWarmupFramesRemaining;
	if (LifecycleWarmupFramesRemaining == 0)
	{
		bLifecycleWarmupComplete = true;
	}
	return true;
}

bool AVolumetricSuperStormActor::TickFormationAnimation(float DeltaSeconds)
{
	if (!bFormationAnimationActive || DeltaSeconds <= 0.0f)
	{
		return false;
	}

	const float                PreviousProgress = FormationProgress;
	const EStormFormationState PreviousState    = FormationState;
	switch (FormationState)
	{
	case EStormFormationState::Forming:
		FormationProgress += DeltaSeconds / SanitizeFormationAnimationDuration(FormationDurationSeconds);
		if (FormationProgress >= 1.0f)
		{
			FormationState            = EStormFormationState::Mature;
			FormationProgress         = 1.0f;
			bFormationAnimationActive = false;
		}
		break;

	case EStormFormationState::Dissolving:
		FormationProgress += DeltaSeconds / SanitizeFormationAnimationDuration(DissolutionDurationSeconds);
		if (FormationProgress >= 1.0f)
		{
			FormationState            = EStormFormationState::Hidden;
			FormationProgress         = 0.0f;
			bFormationAnimationActive = false;
		}
		break;

	default:
		bFormationAnimationActive = false;
		NormalizeFormationStateEndpoints();
		break;
	}

	FormationProgress   = FMath::Clamp(FormationProgress, 0.0f, 1.0f);
	const bool bChanged = PreviousState != FormationState || !FMath::IsNearlyEqual(PreviousProgress, FormationProgress, 1.0e-6f);
	return bChanged;
}

void AVolumetricSuperStormActor::BroadcastFormationCompletionEvents(const EStormFormationState PreviousState, const EStormFormationState NewState)
{
	const UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return;
	}

	if (PreviousState == EStormFormationState::Forming && NewState == EStormFormationState::Mature)
	{
		OnStormFormationCompleted.Broadcast();
		ReceiveStormFormationCompleted();
	}
	else if (PreviousState == EStormFormationState::Dissolving && NewState == EStormFormationState::Hidden)
	{
		OnStormDissolutionCompleted.Broadcast();
		ReceiveStormDissolutionCompleted();
	}
}

void AVolumetricSuperStormActor::NormalizeFormationStateEndpoints()
{
	switch (FormationState)
	{
	case EStormFormationState::Hidden:
		FormationProgress = 0.0f;
		break;
	case EStormFormationState::Mature:
		FormationProgress = 1.0f;
		break;
	default:
		FormationProgress = FMath::Clamp(FormationProgress, 0.0f, 1.0f);
		break;
	}
}

#if WITH_EDITOR
bool AVolumetricSuperStormActor::SetMotionRingEndRadius01ForEditor(int32 BoundaryIndex, float NewRadius01)
{
	const FStormMotionSettings SanitizedMotion = VolumetricSuperStorm::Motion::SanitizeSettings(MotionSettings);
	if (!SanitizedMotion.RingEndRadii01.IsValidIndex(BoundaryIndex))
	{
		return false;
	}

	const float MinimumRadius01 = BoundaryIndex > 0 ? SanitizedMotion.RingEndRadii01[BoundaryIndex - 1] + VolumetricSuperStorm::Motion::MinRingWidth01 : VolumetricSuperStorm::Motion::MinRingWidth01;
	const float MaximumRadius01 = SanitizedMotion.RingEndRadii01.IsValidIndex(BoundaryIndex + 1) ? SanitizedMotion.RingEndRadii01[BoundaryIndex + 1] - VolumetricSuperStorm::Motion::MinRingWidth01 : FMath::Min(0.99f, SanitizedMotion.MotionRadiusScale - VolumetricSuperStorm::Motion::MinRingWidth01);
	if (MaximumRadius01 < MinimumRadius01)
	{
		return false;
	}

	VolumetricSuperStorm::Motion::SynchronizeRingArrays(MotionSettings);
	const float ClampedRadius01 = FMath::Clamp(NewRadius01, MinimumRadius01, MaximumRadius01);
	if (FMath::IsNearlyEqual(MotionSettings.RingEndRadii01[BoundaryIndex], ClampedRadius01))
	{
		return false;
	}

	Modify();
	MotionSettings.RingEndRadii01[BoundaryIndex] = ClampedRadius01;
	RebuildRenderData();
	return true;
}

void AVolumetricSuperStormActor::CommitMotionRingEndRadiiEditForEditor()
{
	FProperty*            MotionSettingsProperty = FindFProperty<FProperty>(StaticClass(), GET_MEMBER_NAME_CHECKED(AVolumetricSuperStormActor, MotionSettings));
	FPropertyChangedEvent ChangeEvent(MotionSettingsProperty, EPropertyChangeType::ValueSet);
	PostEditChangeProperty(ChangeEvent);
}
#endif
