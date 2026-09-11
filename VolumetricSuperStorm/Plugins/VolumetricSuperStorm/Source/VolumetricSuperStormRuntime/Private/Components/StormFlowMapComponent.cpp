// Copyright 2026 GoroGoro. All Rights Reserved.

/**
 * @file StormFlowMapComponent.cpp
 * @brief Owns flow-map layers, render targets, and persistent assets.
 */

#include "Components/StormFlowMapComponent.h"

#include "VolumetricSuperStormRuntime.h"
#include "RenderingThread.h"
#include "TextureCompiler.h"
#include "Actors/VolumetricSuperStormActor.h"
#include "Assets/StormWindFlowMapDataAsset.h"
#include "Data/Flowmap/StormFlowMapUndoState.h"
#include "Data/StormRenderTargetResolution.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Rendering/StormFlowMapRenderTargetUtils.h"
#include "Subsystems/StormRenderWorldSubsystem.h"

#if WITH_EDITOR
#include "HAL/IConsoleManager.h"
#endif

using namespace VolumetricSuperStorm;

#if WITH_EDITOR
namespace VolumetricSuperStorm
{
	static TAutoConsoleVariable<int32> CVarFlowMapHistoryMaxDepth(
		TEXT("VolumetricSuperStorm.FlowMap.History.MaxDepth"),
		128,
		TEXT("Completed edit nodes allowed from a flow-map checkpoint. A positive "
			 "limit schedules an undo barrier and prefix rebase when reached; <= 0 "
			 "disables the depth limit."));

	static TAutoConsoleVariable<int32> CVarFlowMapHistoryMaxReplayCost(
		TEXT("VolumetricSuperStorm.FlowMap.History.MaxReplayCost"),
		1024,
		TEXT("Estimated replay passes allowed from a flow-map checkpoint. A positive "
			 "limit schedules an undo barrier and prefix rebase when reached; <= 0 "
			 "disables the replay-cost limit."));

	static TAutoConsoleVariable<int32> CVarFlowMapHistoryMaxEditReplayCost(
		TEXT("VolumetricSuperStorm.FlowMap.History.MaxEditReplayCost"),
		256,
		TEXT("Estimated replay passes allowed in one internal flow-map edit chunk. "
			 "Long strokes split into several graph nodes inside the same transaction; "
			 "<= 0 disables chunking."));
}
#endif

namespace
{
	void NormalizeFlowMapParams(FStormFlowmapParams& Params)
	{
		const FVector3f LayerHeights = FlowMap::SanitizeLayerHeights(FlowMap::GetLayerHeights(Params));
		Params.LowerLayerHeight      = LayerHeights.X;
		Params.MiddleLayerHeight     = LayerHeights.Y;
		Params.UpperLayerHeight      = LayerHeights.Z;
		Params.UVWStrength.X         = FMath::Max(Params.UVWStrength.X, 0.0f);
		Params.UVWStrength.Y         = FMath::Max(Params.UVWStrength.Y, 0.0f);
		Params.UVWStrength.Z         = FMath::Max(Params.UVWStrength.Z, 0.0f);
		Params.CycleDurationSeconds  = FMath::Max(Params.CycleDurationSeconds, 0.1f);
	}

	bool AreFlowMapParamsEqual(
		const FStormFlowmapParams& A,
		const FStormFlowmapParams& B)
	{
		return A.LowerLayerHeight == B.LowerLayerHeight &&
			A.MiddleLayerHeight == B.MiddleLayerHeight &&
			A.UpperLayerHeight == B.UpperLayerHeight &&
			A.UVWStrength == B.UVWStrength &&
			A.CycleDurationSeconds == B.CycleDurationSeconds;
	}
}

UStormFlowMapComponent::UStormFlowMapComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UStormFlowMapComponent::ApplyFlowMapConfiguration(UStormWindFlowMapDataAsset* InAsset, bool bEnabled, bool bNotifyOwner)
{
	Modify();
	bFlowMapEnabled = bEnabled;
	ReplaceFlowMapDocument(InAsset, bNotifyOwner);
}

void UStormFlowMapComponent::SetFlowMapAsset(UStormWindFlowMapDataAsset* InAsset)
{
	if (PersistentFlowMapAsset == InAsset && bFlowMapsInitialized)
	{
		return;
	}

	ReplaceFlowMapDocument(InAsset);
}

void UStormFlowMapComponent::SetFlowMapEnabled(bool bEnabled)
{
	if (bFlowMapEnabled == bEnabled)
	{
		return;
	}
	bFlowMapEnabled = bEnabled;
	NotifyOwnerFlowMapChanged();
}

void UStormFlowMapComponent::SetWorkingParams(const FStormFlowmapParams& InParams)
{
	WorkingParams = InParams;
	NormalizeFlowMapParams(WorkingParams);

	const int32 Resolution                = RenderTargetResolution::FlowMap;
	const bool  bRenderTargetsNeedRefresh = !LowerFlowMapRT || !MiddleFlowMapRT || !UpperFlowMapRT || LowerFlowMapRT->SizeX != Resolution || LowerFlowMapRT->SizeY != Resolution || MiddleFlowMapRT->SizeX != Resolution || MiddleFlowMapRT->SizeY != Resolution || UpperFlowMapRT->SizeX != Resolution || UpperFlowMapRT->SizeY != Resolution;
	if (bRenderTargetsNeedRefresh)
	{
		bFlowMapsInitialized = false;
		EnsureFlowMapsInitialized();
	}

	RefreshOwnerRenderData();
}

bool UStormFlowMapComponent::EnsureFlowMapsInitialized()
{
	NormalizeFlowMapParams(WorkingParams);

	const bool bHasUsableTargets = LowerFlowMapRT && MiddleFlowMapRT && UpperFlowMapRT && LowerFlowMapRT->GetOuter() == this && MiddleFlowMapRT->GetOuter() == this && UpperFlowMapRT->GetOuter() == this && LowerFlowMapRT->SizeX == RenderTargetResolution::FlowMap && LowerFlowMapRT->SizeY == RenderTargetResolution::FlowMap && MiddleFlowMapRT->SizeX == RenderTargetResolution::FlowMap && MiddleFlowMapRT->SizeY == RenderTargetResolution::FlowMap && UpperFlowMapRT->SizeX == RenderTargetResolution::FlowMap && UpperFlowMapRT->SizeY == RenderTargetResolution::FlowMap;
	if (bFlowMapsInitialized && bHasUsableTargets)
	{
#if WITH_EDITOR
		return InitializeFlowMapHistory();
#else
		return true;
#endif
	}

	RegenerateRenderTargets();
	if (!LowerFlowMapRT || !MiddleFlowMapRT || !UpperFlowMapRT)
	{
		return false;
	}

	MarkOwnerShapeDirty();
	if (!SeedSurfacesFromAsset())
	{
		return false;
	}
	bFlowMapsInitialized = true;
#if WITH_EDITOR
	return InitializeFlowMapHistory(true);
#else
	SavedParams = WorkingParams;
	bHasSavedBaseline = true;
	return true;
#endif
}

bool UStormFlowMapComponent::RevertToAsset()
{
	return ReplaceFlowMapDocument(PersistentFlowMapAsset);
}

bool UStormFlowMapComponent::ReplaceFlowMapDocument(UStormWindFlowMapDataAsset* InAsset, bool bNotifyOwner)
{
	Modify();
	++FlowMapEditSessionSerial;
	ResetFlowMapHistory();
	PersistentFlowMapAsset = InAsset;
	WorkingParams          = PersistentFlowMapAsset ? PersistentFlowMapAsset->Params : FStormFlowmapParams();
	NormalizeFlowMapParams(WorkingParams);
	bFlowMapsInitialized = false;
	const bool bInitialized = EnsureFlowMapsInitialized();

	if (bNotifyOwner)
	{
		NotifyOwnerFlowMapChanged();
	}
	return bInitialized;
}

bool UStormFlowMapComponent::ClearLayer(EStormFlowMapLayer Layer)
{
	if (!EnsureFlowMapsInitialized())
	{
		return false;
	}

	const bool bCleared = FlowMapRenderTargetUtils::ClearSurface(GetLayerRenderTarget(Layer));
	if (bCleared)
	{
		RecordClear(Layer);
		if (Layer == EStormFlowMapLayer::Upper)
		{
			MarkOwnerShapeDirty();
		}
		NotifyOwnerFlowMapTextureUpdated(Layer);
	}
	return bCleared;
}

bool UStormFlowMapComponent::StampBrush(EStormFlowMapLayer Layer, FVector2D BrushUV, FVector2D DirectionUV, float BrushRadiusUV, float VerticalDirection, float Strength, float Opacity, FLinearColor EncodedRGBA, bool bErase, bool bUseEncodedRGBA)
{
	if (!EnsureFlowMapsInitialized())
	{
		return false;
	}

	FVector2f HorizontalDirection(static_cast<float>(DirectionUV.X), static_cast<float>(DirectionUV.Y));
	if (!HorizontalDirection.Normalize())
	{
		HorizontalDirection = FVector2f(1.0f, 0.0f);
	}
	const float Z               = FMath::Clamp(VerticalDirection, -1.0f, 1.0f);
	const float HorizontalScale = FMath::Sqrt(FMath::Max(1.0f - Z * Z, 0.0f));

	FStormFlowMapPaintParameters PaintParameters;
	PaintParameters.BrushCenterUV     = FVector2f(FMath::Clamp(static_cast<float>(BrushUV.X), 0.0f, 1.0f), FMath::Clamp(static_cast<float>(BrushUV.Y), 0.0f, 1.0f));
	PaintParameters.BrushRadiusUV     = FMath::Max(BrushRadiusUV, 0.0f);
	PaintParameters.BrushDirectionUVW = FVector3f(HorizontalDirection.X * HorizontalScale, HorizontalDirection.Y * HorizontalScale, Z);
	PaintParameters.BrushEncodedRGBA  = EncodedRGBA;
	PaintParameters.BrushStrength     = FMath::Clamp(Strength, 0.0f, 1.0f);
	PaintParameters.BrushOpacity      = FMath::Clamp(Opacity, 0.0f, 1.0f);
	PaintParameters.bErase            = bErase;
	PaintParameters.bUseEncodedRGBA   = bUseEncodedRGBA;

	const bool bStamped = FlowMapRenderTargetUtils::StampSurface(GetLayerRenderTarget(Layer), PaintParameters);
	if (bStamped)
	{
		RecordStamp(Layer, PaintParameters);
		if (Layer == EStormFlowMapLayer::Upper)
		{
			MarkOwnerShapeDirty();
		}
		NotifyOwnerFlowMapTextureUpdated(Layer);
	}
	return bStamped;
}

bool UStormFlowMapComponent::BeginStroke()
{
#if WITH_EDITOR
	if (!EnsureFlowMapsInitialized() || !EditUndoState || !EditUndoState->Bookmark)
	{
		return false;
	}
	if (EditUndoState->BeginStroke(*EditUndoState->Bookmark))
	{
		PendingStrokeEdit = FStormFlowMapEdit();
		return true;
	}
#endif
	return false;
}

void UStormFlowMapComponent::EndStroke()
{
#if WITH_EDITOR
	if (!IsHistoryStrokeOpen() || !EditUndoState || !EditUndoState->Bookmark)
	{
		return;
	}
	EditUndoState->EndStroke(EditUndoState->Bookmark->HistoryId);
	if (!FlushPendingStrokeEdit())
	{
		ensureMsgf(false, TEXT("Could not seal the pending flow-map stroke into its history."));
		PendingStrokeEdit = FStormFlowMapEdit();
		bHasSavedBaseline = false;
	}
#endif
}

bool UStormFlowMapComponent::ReplayHistoryIfStale(
	EStormFlowMapLayer* OutAffectedLayer)
{
#if WITH_EDITOR
	if (!EnsureFlowMapsInitialized() || !EditUndoState || !EditUndoState->Bookmark)
	{
		return false;
	}
	const FGuid TargetStateId = EditUndoState->Bookmark->CurrentStateId;
	if (TargetStateId == ReplayedHistoryStateId)
	{
		return false;
	}
	EStormFlowMapLayer AffectedLayer = EStormFlowMapLayer::Lower;
	const bool bHasAffectedLayer = ResolveTransitionLayer(
		ReplayedHistoryStateId,
		TargetStateId,
		AffectedLayer);
	if (!ReplayHistoryTo(TargetStateId))
	{
		return false;
	}
	if (OutAffectedLayer && bHasAffectedLayer)
	{
		*OutAffectedLayer = AffectedLayer;
	}
	return true;
#else
	return false;
#endif
}

UTextureRenderTarget2D* UStormFlowMapComponent::GetLayerRenderTarget(EStormFlowMapLayer Layer) const
{
	switch (Layer)
	{
	case EStormFlowMapLayer::Lower:
		return LowerFlowMapRT;
	case EStormFlowMapLayer::Middle:
		return MiddleFlowMapRT;
	case EStormFlowMapLayer::Upper:
		return UpperFlowMapRT;
	default:
		return nullptr;
	}
}

int32 UStormFlowMapComponent::GetWorkingResolution() const
{
	return RenderTargetResolution::FlowMap;
}

bool UStormFlowMapComponent::IsUnsavedToAsset() const
{
	if (!bHasSavedBaseline)
	{
		return true;
	}

	FStormFlowmapParams CurrentParams = WorkingParams;
	FStormFlowmapParams BaselineParams = SavedParams;
	NormalizeFlowMapParams(CurrentParams);
	NormalizeFlowMapParams(BaselineParams);
	if (!AreFlowMapParamsEqual(CurrentParams, BaselineParams))
	{
		return true;
	}

#if WITH_EDITOR
	if (!EditUndoState || !EditUndoState->Bookmark)
	{
		return true;
	}
	if (!PendingStrokeEdit.Ops.IsEmpty())
	{
		return true;
	}
	const FGuid CurrentContentId = EditUndoState->History.Graph.ResolveContentId(
		EditUndoState->Bookmark->CurrentStateId);
	return !CurrentContentId.IsValid() ||
		CurrentContentId != SavedHistoryContentId;
#else
	return false;
#endif
}

void UStormFlowMapComponent::MarkSavedToAsset()
{
	SavedParams = WorkingParams;
	NormalizeFlowMapParams(SavedParams);

#if WITH_EDITOR
	if (!InitializeFlowMapHistory() || !EditUndoState || !EditUndoState->Bookmark)
	{
		SavedHistoryContentId.Invalidate();
		bHasSavedBaseline = false;
		return;
	}
	SavedHistoryContentId = EditUndoState->History.Graph.ResolveContentId(
		EditUndoState->Bookmark->CurrentStateId);
	bHasSavedBaseline = SavedHistoryContentId.IsValid();
#else
	bHasSavedBaseline = true;
#endif
}

bool UStormFlowMapComponent::AdoptBakedFlowMapAsset(
	UStormWindFlowMapDataAsset* InAsset)
{
	if (!InAsset || !EnsureFlowMapsInitialized())
	{
		return false;
	}

	// Save As changes persistence identity, not working-document identity. Keep the
	// live surfaces, replay graph, bookmark, and redo tail exactly where they are.
	PersistentFlowMapAsset = InAsset;
	MarkSavedToAsset();
	MarkPackageDirty();
	return !IsUnsavedToAsset();
}

void UStormFlowMapComponent::GetFlowMapRenderData(FStormFlowMapRenderData& OutRenderData) const
{
	OutRenderData                      = FStormFlowMapRenderData();
	OutRenderData.LowerTexture         = LowerFlowMapRT.Get();
	OutRenderData.MiddleTexture        = MiddleFlowMapRT.Get();
	OutRenderData.UpperTexture         = UpperFlowMapRT.Get();
	OutRenderData.LayerHeights         = FlowMap::SanitizeLayerHeights(FlowMap::GetLayerHeights(WorkingParams));
	OutRenderData.UVWStrength          = WorkingParams.UVWStrength;
	OutRenderData.CycleDurationSeconds = FMath::Max(WorkingParams.CycleDurationSeconds, 0.1f);
	OutRenderData.bEnabled             = bFlowMapEnabled && LowerFlowMapRT && MiddleFlowMapRT && UpperFlowMapRT;
}

void UStormFlowMapComponent::OnRegister()
{
	Super::OnRegister();
	if (!bFlowMapsInitialized && PersistentFlowMapAsset)
	{
		WorkingParams = PersistentFlowMapAsset->Params;
	}
	EnsureFlowMapsInitialized();
}

void UStormFlowMapComponent::BeginPlay()
{
	Super::BeginPlay();
	EnsureFlowMapsInitialized();
}

bool UStormFlowMapComponent::InitializeFlowMapHistory(
	bool bCurrentStateIsSaved)
{
#if !WITH_EDITOR
	return true;
#else
	const bool bHasOrigins =
		HistoryOriginLowerRT && HistoryOriginMiddleRT && HistoryOriginUpperRT &&
		HistoryOriginLowerRT->GetOuter() == this &&
		HistoryOriginMiddleRT->GetOuter() == this &&
		HistoryOriginUpperRT->GetOuter() == this &&
		HistoryOriginLowerRT->SizeX == RenderTargetResolution::FlowMap &&
		HistoryOriginLowerRT->SizeY == RenderTargetResolution::FlowMap &&
		HistoryOriginMiddleRT->SizeX == RenderTargetResolution::FlowMap &&
		HistoryOriginMiddleRT->SizeY == RenderTargetResolution::FlowMap &&
		HistoryOriginUpperRT->SizeX == RenderTargetResolution::FlowMap &&
		HistoryOriginUpperRT->SizeY == RenderTargetResolution::FlowMap;
	if (EditUndoState && EditUndoState->Bookmark && bHasOrigins &&
		EditUndoState->History.Graph.ContainsState(
			EditUndoState->Bookmark->CurrentStateId))
	{
		return true;
	}

	if (!LowerFlowMapRT || !MiddleFlowMapRT || !UpperFlowMapRT || !GetWorld())
	{
		return false;
	}

	ResetFlowMapHistory();
	HistoryOriginLowerRT = FlowMapRenderTargetUtils::CreateSurface(
		this, TEXT("RT_StormFlowMapHistoryOriginLower"), RenderTargetResolution::FlowMap);
	HistoryOriginMiddleRT = FlowMapRenderTargetUtils::CreateSurface(
		this, TEXT("RT_StormFlowMapHistoryOriginMiddle"), RenderTargetResolution::FlowMap);
	HistoryOriginUpperRT = FlowMapRenderTargetUtils::CreateSurface(
		this, TEXT("RT_StormFlowMapHistoryOriginUpper"), RenderTargetResolution::FlowMap);
	if (!HistoryOriginLowerRT || !HistoryOriginMiddleRT || !HistoryOriginUpperRT)
	{
		ResetFlowMapHistory();
		return false;
	}

	FlushRenderingCommands();
	if (!FlowMapRenderTargetUtils::BlitToSurface(
			GetWorld(), LowerFlowMapRT, HistoryOriginLowerRT) ||
		!FlowMapRenderTargetUtils::BlitToSurface(
			GetWorld(), MiddleFlowMapRT, HistoryOriginMiddleRT) ||
		!FlowMapRenderTargetUtils::BlitToSurface(
			GetWorld(), UpperFlowMapRT, HistoryOriginUpperRT))
	{
		ResetFlowMapHistory();
		return false;
	}
	FlushRenderingCommands();

	EditUndoState = NewObject<UStormFlowMapUndoState>(
		this, NAME_None, RF_Transient);
	if (!EditUndoState)
	{
		ResetFlowMapHistory();
		return false;
	}
	EditUndoState->ClearFlags(RF_Transactional);
	EditUndoState->Bookmark = NewObject<UStormFlowMapUndoBookmark>(
		EditUndoState,
		NAME_None,
		RF_Transient | RF_Transactional);
	if (!EditUndoState->Bookmark)
	{
		ResetFlowMapHistory();
		return false;
	}

	EditUndoState->Bookmark->HistoryId = FGuid::NewGuid();
	EditUndoState->History.Graph.OriginStateId = FGuid::NewGuid();
	EditUndoState->History.Graph.OriginContentId =
		EditUndoState->History.Graph.OriginStateId;
	EditUndoState->Bookmark->CurrentStateId =
		EditUndoState->History.Graph.OriginStateId;
	ReplayedHistoryStateId = EditUndoState->History.Graph.OriginStateId;
	if (bCurrentStateIsSaved)
	{
		SavedHistoryContentId =
			EditUndoState->History.Graph.OriginContentId;
		SavedParams = WorkingParams;
		NormalizeFlowMapParams(SavedParams);
		bHasSavedBaseline = SavedHistoryContentId.IsValid();
	}
	return true;
#endif
}

#if WITH_EDITOR
bool UStormFlowMapComponent::IsFlowMapHistoryCheckpointRequired() const
{
	if (!EditUndoState || !EditUndoState->Bookmark)
	{
		return false;
	}

	const int64 MaxDepth =
		VolumetricSuperStorm::CVarFlowMapHistoryMaxDepth.GetValueOnGameThread();
	const int64 MaxReplayCost =
		VolumetricSuperStorm::CVarFlowMapHistoryMaxReplayCost.GetValueOnGameThread();
	if (MaxDepth <= 0 && MaxReplayCost <= 0)
	{
		return false;
	}

	const FStormReplayHistory& Graph = EditUndoState->History.Graph;
	const FGuid CurrentStateId = EditUndoState->Bookmark->CurrentStateId;
	if (!Graph.ContainsState(CurrentStateId))
	{
		return false;
	}

	const int64 Depth = Graph.GetDepth(CurrentStateId);
	const int64 ReplayCost = Graph.GetReplayCost(CurrentStateId);
	return (MaxDepth > 0 && Depth >= MaxDepth) ||
		(MaxReplayCost > 0 && ReplayCost >= MaxReplayCost);
}

void UStormFlowMapComponent::AdvanceFlowMapEditSession()
{
	Modify();
	++FlowMapEditSessionSerial;
}

bool UStormFlowMapComponent::RebaseFlowMapHistoryWithUndoBoundary(
	TFunctionRef<bool()> EstablishUndoBoundary)
{
	if (IsHistoryStrokeOpen() || !PendingStrokeEdit.Ops.IsEmpty() ||
		!EditUndoState || !EditUndoState->Bookmark ||
		!HistoryOriginLowerRT || !HistoryOriginMiddleRT || !HistoryOriginUpperRT ||
		!LowerFlowMapRT || !MiddleFlowMapRT || !UpperFlowMapRT || !GetWorld())
	{
		return false;
	}

	FStormFlowMapHistory& History = EditUndoState->History;
	UStormFlowMapUndoBookmark& Bookmark = *EditUndoState->Bookmark;
	const FGuid HeadStateId = Bookmark.CurrentStateId;
	if (!History.Graph.ContainsState(HeadStateId))
	{
		return false;
	}

	const FGuid CheckpointContentId = History.Graph.ResolveContentId(HeadStateId);
	if (!CheckpointContentId.IsValid() ||
		History.Graph.GetAbsoluteDepth(HeadStateId) == INDEX_NONE ||
		History.Graph.GetCumulativeReplayCost(HeadStateId) == INDEX_NONE)
	{
		return false;
	}

	if (ReplayedHistoryStateId != HeadStateId && !ReplayHistoryTo(HeadStateId))
	{
		return false;
	}

	UTextureRenderTarget2D* NewOriginLower =
		FlowMapRenderTargetUtils::CreateSurface(
			this, TEXT("RT_StormFlowMapCheckpointLower"), RenderTargetResolution::FlowMap);
	UTextureRenderTarget2D* NewOriginMiddle =
		FlowMapRenderTargetUtils::CreateSurface(
			this, TEXT("RT_StormFlowMapCheckpointMiddle"), RenderTargetResolution::FlowMap);
	UTextureRenderTarget2D* NewOriginUpper =
		FlowMapRenderTargetUtils::CreateSurface(
			this, TEXT("RT_StormFlowMapCheckpointUpper"), RenderTargetResolution::FlowMap);
	if (!NewOriginLower || !NewOriginMiddle || !NewOriginUpper)
	{
		return false;
	}

	FlushRenderingCommands();
	if (!FlowMapRenderTargetUtils::BlitToSurface(
			GetWorld(), LowerFlowMapRT, NewOriginLower) ||
		!FlowMapRenderTargetUtils::BlitToSurface(
			GetWorld(), MiddleFlowMapRT, NewOriginMiddle) ||
		!FlowMapRenderTargetUtils::BlitToSurface(
			GetWorld(), UpperFlowMapRT, NewOriginUpper))
	{
		return false;
	}
	FlushRenderingCommands();

	// Preparation above is reversible. Seal the old Unreal transactions only after
	// every replacement origin has been allocated and populated successfully.
	if (!EstablishUndoBoundary())
	{
		return false;
	}

	const FGuid NewOriginStateId =
		History.Graph.RebaseToCheckpoint(HeadStateId);
	check(NewOriginStateId.IsValid());
	check(History.Graph.ResolveContentId(NewOriginStateId) == CheckpointContentId);
	History.SweepEditsToGraph();
	HistoryOriginLowerRT = NewOriginLower;
	HistoryOriginMiddleRT = NewOriginMiddle;
	HistoryOriginUpperRT = NewOriginUpper;
	Bookmark.CurrentStateId = NewOriginStateId;
	ReplayedHistoryStateId = NewOriginStateId;
	EditUndoState->ResetBranchTracking();

	UE_LOG(
		LogVolumetricSuperStormRuntime,
		Display,
		TEXT("Flow-map edit history checkpointed behind an undo barrier."));
	return true;
}
#endif

void UStormFlowMapComponent::ResetFlowMapHistory()
{
	PendingStrokeEdit = FStormFlowMapEdit();
	ReplayedHistoryStateId.Invalidate();
	SavedHistoryContentId.Invalidate();
	bHasSavedBaseline = false;
	if (EditUndoState)
	{
		EditUndoState->ResetStroke();
		EditUndoState->ResetBranchTracking();
	}
	EditUndoState = nullptr;
	HistoryOriginLowerRT = nullptr;
	HistoryOriginMiddleRT = nullptr;
	HistoryOriginUpperRT = nullptr;
}

bool UStormFlowMapComponent::IsHistoryStrokeOpen() const
{
	return EditUndoState && EditUndoState->Bookmark &&
		EditUndoState->IsStrokeOpen(EditUndoState->Bookmark->HistoryId);
}

void UStormFlowMapComponent::RecordOp(const FStormFlowMapOp& Op)
{
#if WITH_EDITOR
	if (GIsTransacting || !EditUndoState || !EditUndoState->Bookmark)
	{
		return;
	}

	if (IsHistoryStrokeOpen())
	{
		const int64 MaxEditReplayCost =
			VolumetricSuperStorm::CVarFlowMapHistoryMaxEditReplayCost.GetValueOnGameThread();
		if (MaxEditReplayCost > 0 && !PendingStrokeEdit.Ops.IsEmpty() &&
			PendingStrokeEdit.GetReplayCost() + Op.GetReplayCost() > MaxEditReplayCost)
		{
			// This seals an internal replay chunk, not the gesture. BeginStroke modified
			// the bookmark once, so every chunk remains one atomic Unreal undo step.
			if (!FlushPendingStrokeEdit())
			{
				bHasSavedBaseline = false;
			}
		}
		PendingStrokeEdit.Ops.Add(Op);
		return;
	}

	EditUndoState->BeginStandaloneEdit(*EditUndoState->Bookmark);
	FStormFlowMapEdit Edit;
	Edit.Ops.Add(Op);
	const FGuid NewStateId = AppendHistoryEdit(Edit);
	if (NewStateId.IsValid())
	{
		EditUndoState->Bookmark->CurrentStateId = NewStateId;
		ReplayedHistoryStateId = NewStateId;
	}
	else
	{
		bHasSavedBaseline = false;
	}
#endif
}

void UStormFlowMapComponent::RecordStamp(
	EStormFlowMapLayer Layer,
	const FStormFlowMapPaintParameters& PaintParameters)
{
	FStormFlowMapOp Op;
	Op.Type = EStormFlowMapOpType::Stamp;
	Op.Layer = Layer;
	Op.BrushCenterUV = PaintParameters.BrushCenterUV;
	Op.BrushRadiusUV = PaintParameters.BrushRadiusUV;
	Op.BrushDirectionUVW = PaintParameters.BrushDirectionUVW;
	Op.BrushEncodedRGBA = PaintParameters.BrushEncodedRGBA;
	Op.BrushStrength = PaintParameters.BrushStrength;
	Op.BrushOpacity = PaintParameters.BrushOpacity;
	Op.bErase = PaintParameters.bErase;
	Op.bUseEncodedRGBA = PaintParameters.bUseEncodedRGBA;
	RecordOp(Op);
}

void UStormFlowMapComponent::RecordClear(EStormFlowMapLayer Layer)
{
	FStormFlowMapOp Op;
	Op.Type = EStormFlowMapOpType::ClearLayer;
	Op.Layer = Layer;
	RecordOp(Op);
}

bool UStormFlowMapComponent::FlushPendingStrokeEdit()
{
	if (PendingStrokeEdit.Ops.IsEmpty())
	{
		return true;
	}
	if (!EditUndoState || !EditUndoState->Bookmark)
	{
		return false;
	}

	const FGuid NewStateId = AppendHistoryEdit(PendingStrokeEdit);
	if (!NewStateId.IsValid())
	{
		return false;
	}
	PendingStrokeEdit = FStormFlowMapEdit();
	EditUndoState->Bookmark->CurrentStateId = NewStateId;
	ReplayedHistoryStateId = NewStateId;
	return true;
}

FGuid UStormFlowMapComponent::AppendHistoryEdit(const FStormFlowMapEdit& Edit)
{
	if (!EditUndoState || !EditUndoState->Bookmark)
	{
		return FGuid();
	}

	FStormFlowMapHistory& History = EditUndoState->History;
	UStormFlowMapUndoBookmark& Bookmark = *EditUndoState->Bookmark;
	if (!History.Graph.ContainsState(Bookmark.CurrentStateId))
	{
		Bookmark.CurrentStateId = History.Graph.OriginStateId;
		EditUndoState->MarkBookmarkRepaired(Bookmark.HistoryId);
	}

	bool bCreatedBranch = false;
	const FGuid NewStateId = History.Append(
		Bookmark.CurrentStateId,
		Edit,
		&bCreatedBranch);
	if (NewStateId.IsValid())
	{
		EditUndoState->TrackAppendedState(
			Bookmark.HistoryId,
			NewStateId,
			bCreatedBranch);
	}
	return NewStateId;
}

bool UStormFlowMapComponent::ResolveTransitionLayer(
	const FGuid& FromStateId,
	const FGuid& ToStateId,
	EStormFlowMapLayer& OutLayer) const
{
	if (!EditUndoState)
	{
		return false;
	}

	const auto ResolveNodeLayer =
		[this, &OutLayer](const FStormReplayHistoryNode* Node)
		{
			const FStormFlowMapEdit* Edit =
				Node ? EditUndoState->History.FindEdit(*Node) : nullptr;
			if (!Edit || Edit->Ops.IsEmpty())
			{
				return false;
			}
			OutLayer = Edit->Ops.Last().Layer;
			return true;
		};

	if (const FStormReplayHistoryNode* FromNode =
			EditUndoState->History.Graph.FindNode(FromStateId);
		FromNode && FromNode->ParentStateId == ToStateId)
	{
		return ResolveNodeLayer(FromNode);
	}

	if (const FStormReplayHistoryNode* ToNode =
			EditUndoState->History.Graph.FindNode(ToStateId);
		ToNode && ToNode->ParentStateId == FromStateId)
	{
		return ResolveNodeLayer(ToNode);
	}
	return false;
}

bool UStormFlowMapComponent::ReplayHistoryTo(const FGuid& TargetStateId)
{
	if (!EditUndoState || !EditUndoState->Bookmark ||
		!HistoryOriginLowerRT || !HistoryOriginMiddleRT || !HistoryOriginUpperRT ||
		!LowerFlowMapRT || !MiddleFlowMapRT || !UpperFlowMapRT || !GetWorld())
	{
		return false;
	}

	TArray<const FStormReplayHistoryNode*> Path;
	if (!EditUndoState->History.BuildPath(TargetStateId, Path))
	{
		return false;
	}

	FlushRenderingCommands();
	if (!FlowMapRenderTargetUtils::BlitToSurface(
			GetWorld(), HistoryOriginLowerRT, LowerFlowMapRT) ||
		!FlowMapRenderTargetUtils::BlitToSurface(
			GetWorld(), HistoryOriginMiddleRT, MiddleFlowMapRT) ||
		!FlowMapRenderTargetUtils::BlitToSurface(
			GetWorld(), HistoryOriginUpperRT, UpperFlowMapRT))
	{
		return false;
	}

	for (const FStormReplayHistoryNode* Node : Path)
	{
		const FStormFlowMapEdit* Edit =
			Node ? EditUndoState->History.FindEdit(*Node) : nullptr;
		if (!Edit)
		{
			return false;
		}
		for (const FStormFlowMapOp& Op : Edit->Ops)
		{
			UTextureRenderTarget2D* Target = GetLayerRenderTarget(Op.Layer);
			if (Op.Type == EStormFlowMapOpType::ClearLayer)
			{
				if (!FlowMapRenderTargetUtils::ClearSurface(Target))
				{
					return false;
				}
				continue;
			}

			FStormFlowMapPaintParameters PaintParameters;
			PaintParameters.BrushCenterUV = Op.BrushCenterUV;
			PaintParameters.BrushRadiusUV = Op.BrushRadiusUV;
			PaintParameters.BrushDirectionUVW = Op.BrushDirectionUVW;
			PaintParameters.BrushEncodedRGBA = Op.BrushEncodedRGBA;
			PaintParameters.BrushStrength = Op.BrushStrength;
			PaintParameters.BrushOpacity = Op.BrushOpacity;
			PaintParameters.bErase = Op.bErase;
			PaintParameters.bUseEncodedRGBA = Op.bUseEncodedRGBA;
			if (!FlowMapRenderTargetUtils::StampSurface(Target, PaintParameters))
			{
				return false;
			}
		}
	}

	FlushRenderingCommands();
	ReplayedHistoryStateId = TargetStateId;
	NotifyOwnerFlowMapChanged();
	return true;
}

bool UStormFlowMapComponent::RegenerateRenderTargets()
{
	const int32 Resolution         = RenderTargetResolution::FlowMap;
	const bool  bNeedsReallocation = !LowerFlowMapRT || !MiddleFlowMapRT || !UpperFlowMapRT || LowerFlowMapRT->GetOuter() != this || MiddleFlowMapRT->GetOuter() != this || UpperFlowMapRT->GetOuter() != this || LowerFlowMapRT->SizeX != Resolution || LowerFlowMapRT->SizeY != Resolution || MiddleFlowMapRT->SizeX != Resolution || MiddleFlowMapRT->SizeY != Resolution || UpperFlowMapRT->SizeX != Resolution || UpperFlowMapRT->SizeY != Resolution;
	if (!bNeedsReallocation)
	{
		return false;
	}

	FlushRenderingCommands();
	ResetFlowMapHistory();
	LowerFlowMapRT       = FlowMapRenderTargetUtils::CreateSurface(this, TEXT("RT_StormFlowMapLower"), Resolution);
	MiddleFlowMapRT      = FlowMapRenderTargetUtils::CreateSurface(this, TEXT("RT_StormFlowMapMiddle"), Resolution);
	UpperFlowMapRT       = FlowMapRenderTargetUtils::CreateSurface(this, TEXT("RT_StormFlowMapUpper"), Resolution);
	bFlowMapsInitialized = false;
	return true;
}

bool UStormFlowMapComponent::SeedSurfacesFromAsset()
{
	if (!LowerFlowMapRT || !MiddleFlowMapRT || !UpperFlowMapRT)
	{
		return false;
	}

	UWorld*                 World      = GetWorld();
	UTexture2D*             Sources[3] = { PersistentFlowMapAsset ? PersistentFlowMapAsset->BottomFlowMap.Get() : nullptr, PersistentFlowMapAsset ? PersistentFlowMapAsset->MiddleFlowMap.Get() : nullptr, PersistentFlowMapAsset ? PersistentFlowMapAsset->TopFlowMap.Get() : nullptr, };
	UTextureRenderTarget2D* Targets[3] = { LowerFlowMapRT, MiddleFlowMapRT, UpperFlowMapRT, };

#if WITH_EDITOR
	TArray<UTexture*> TexturesToFinish;
	for (UTexture2D* Source : Sources)
	{
		if (Source)
		{
			TexturesToFinish.Add(Source);
		}
	}
	if (!TexturesToFinish.IsEmpty())
	{
		FTextureCompilingManager::Get().FinishCompilation(TexturesToFinish);
	}
#endif

	for (int32 Index = 0; Index < 3; ++Index)
	{
		if (Sources[Index])
		{
			if (!FlowMapRenderTargetUtils::BlitToSurface(World, Sources[Index], Targets[Index]))
			{
				return false;
			}
		}
		else if (!FlowMapRenderTargetUtils::ClearSurface(Targets[Index]))
		{
			return false;
		}
	}
	return true;
}

void UStormFlowMapComponent::MarkOwnerShapeDirty()
{
	AVolumetricSuperStormActor* StormActor = Cast<AVolumetricSuperStormActor>(GetOwner());
	if (!StormActor)
	{
		return;
	}

	if (UWorld* World = StormActor->GetWorld())
	{
		if (UStormRenderWorldSubsystem* Subsystem = World->GetSubsystem<UStormRenderWorldSubsystem>())
		{
			Subsystem->MarkStormShapeDirty(StormActor);
		}
	}
}

void UStormFlowMapComponent::NotifyOwnerFlowMapTextureUpdated(EStormFlowMapLayer Layer)
{
	if (AVolumetricSuperStormActor* StormActor = Cast<AVolumetricSuperStormActor>(GetOwner()))
	{
		EStormTextureDirtyFlags DirtyTexture = EStormTextureDirtyFlags::None;
		switch (Layer)
		{
		case EStormFlowMapLayer::Lower: DirtyTexture = EStormTextureDirtyFlags::FlowLower; break;
		case EStormFlowMapLayer::Middle: DirtyTexture = EStormTextureDirtyFlags::FlowMiddle; break;
		case EStormFlowMapLayer::Upper: DirtyTexture = EStormTextureDirtyFlags::FlowUpper; break;
		default: break;
		}
		StormActor->RequestTextureContentUpdate(DirtyTexture);
	}
}

void UStormFlowMapComponent::RefreshOwnerRenderData()
{
	if (AVolumetricSuperStormActor* StormActor = Cast<AVolumetricSuperStormActor>(GetOwner()))
	{
		StormActor->RebuildRenderData();
	}
}

void UStormFlowMapComponent::NotifyOwnerFlowMapChanged()
{
	MarkOwnerShapeDirty();
	RefreshOwnerRenderData();
}

#if WITH_EDITOR
void UStormFlowMapComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName       = PropertyChangedEvent.GetPropertyName();
	const FName MemberPropertyName = PropertyChangedEvent.GetMemberPropertyName();
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UStormFlowMapComponent, PersistentFlowMapAsset))
	{
		ReplaceFlowMapDocument(PersistentFlowMapAsset);
		return;
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UStormFlowMapComponent, WorkingParams) || MemberPropertyName == GET_MEMBER_NAME_CHECKED(UStormFlowMapComponent, WorkingParams))
	{
		const FStormFlowmapParams EditedParams = WorkingParams;
		SetWorkingParams(EditedParams);
		return;
	}
	NotifyOwnerFlowMapChanged();
}
#endif
