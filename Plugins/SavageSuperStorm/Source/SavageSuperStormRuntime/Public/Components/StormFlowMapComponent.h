/**
 * @file StormFlowMapComponent.h
 * @brief Declares the three-layer flow-map component.
 */

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/Flowmap/StormFlowMapHistory.h"
#include "Data/StormFlowmapParams.h"
#include "Data/StormRenderData.h"
#include "StormFlowMapComponent.generated.h"

class UStormWindFlowMapDataAsset;
class UStormFlowMapUndoState;
class UTextureRenderTarget2D;
struct FStormFlowMapPaintParameters;

/**
 * @brief Owns and renders the storm's three flow-map layers.
 */
UCLASS(ClassGroup = (Storm), BlueprintType, meta = (BlueprintSpawnableComponent))

class SAVAGESUPERSTORMRUNTIME_API UStormFlowMapComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UStormFlowMapComponent();

	void ApplyFlowMapConfiguration(UStormWindFlowMapDataAsset* InAsset, bool bEnabled, bool bNotifyOwner = true);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Storm|Flow Map")
	TObjectPtr<UStormWindFlowMapDataAsset> PersistentFlowMapAsset = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Flow Map")
	bool bFlowMapEnabled = true;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Storm|Flow Map|Live")
	TObjectPtr<UTextureRenderTarget2D> LowerFlowMapRT = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Storm|Flow Map|Live")
	TObjectPtr<UTextureRenderTarget2D> MiddleFlowMapRT = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Storm|Flow Map|Live")
	TObjectPtr<UTextureRenderTarget2D> UpperFlowMapRT = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Transient, Category = "Storm|Flow Map", meta = (ShowOnlyInnerProperties))
	FStormFlowmapParams WorkingParams;

	/** @brief Loads a persistent flow-map asset into the working surfaces. */
	UFUNCTION(BlueprintCallable, Category = "Storm|Flow Map")
	void SetFlowMapAsset(UStormWindFlowMapDataAsset* InAsset);

	/** @brief Enables or disables flow-map sampling. */
	UFUNCTION(BlueprintCallable, Category = "Storm|Flow Map")
	void SetFlowMapEnabled(bool bEnabled);

	/** @brief Applies editable flow-map layer and playback parameters. */
	UFUNCTION(BlueprintCallable, Category = "Storm|Flow Map")
	void SetWorkingParams(const FStormFlowmapParams& InParams);

	/** @brief Creates missing working flow-map render targets. */
	UFUNCTION(BlueprintCallable, Category = "Storm|Flow Map")
	bool EnsureFlowMapsInitialized();

	/** @brief Restores all working layers from the assigned asset. */
	UFUNCTION(BlueprintCallable, Category = "Storm|Flow Map")
	bool RevertToAsset();

	/**
	 * @brief Replaces the complete working document and discards its edit session.
	 *
	 * Asset changes, explicit reverts, and unsupported resolution changes all pass
	 * through this boundary so flow-map history can be attached without retaining
	 * references to an obsolete document.
	 */
	bool ReplaceFlowMapDocument(UStormWindFlowMapDataAsset* InAsset, bool bNotifyOwner = true);

	/** @brief Clears one working flow-map layer. */
	UFUNCTION(BlueprintCallable, Category = "Storm|Flow Map|Paint")
	bool ClearLayer(EStormFlowMapLayer Layer);

	/** @brief Paints one brush stamp into a working flow-map layer. */
	UFUNCTION(BlueprintCallable, Category = "Storm|Flow Map|Paint")
	bool StampBrush(EStormFlowMapLayer Layer, FVector2D BrushUV, FVector2D DirectionUV, float BrushRadiusUV, float VerticalDirection, float Strength, float Opacity, FLinearColor EncodedRGBA, bool bErase, bool bUseEncodedRGBA);

	/** @brief Opens one transactional flow-map paint gesture. */
	bool BeginStroke();

	/** @brief Seals all dabs in the current gesture into replay history. */
	void EndStroke();

	/** @brief Replays the transactional history bookmark after Undo or Redo. */
	bool ReplayHistoryIfStale(EStormFlowMapLayer* OutAffectedLayer = nullptr);

	/** @brief Returns the working render target for one flow-map layer. */
	UFUNCTION(BlueprintPure, Category = "Storm|Flow Map")
	UTextureRenderTarget2D* GetLayerRenderTarget(EStormFlowMapLayer Layer) const;

	/** @brief Returns the current square working resolution. */
	UFUNCTION(BlueprintPure, Category = "Storm|Flow Map")
	int32 GetWorkingResolution() const;

	void GetFlowMapRenderData(FStormFlowMapRenderData& OutRenderData) const;

	/** @brief Compares the current history content and params with the last bake. */
	bool IsUnsavedToAsset() const;

	/** @brief Moves the non-transactional saved baseline to the current document. */
	void MarkSavedToAsset();

	/**
	 * @brief Makes an exact bake of the working document its new save target.
	 *
	 * Unlike loading an asset, this preserves the working surfaces and edit history.
	 */
	bool AdoptBakedFlowMapAsset(UStormWindFlowMapDataAsset* InAsset);

#if WITH_EDITOR
	/** @brief Returns whether the current pixel history has reached a configured limit. */
	bool IsFlowMapHistoryCheckpointRequired() const;

	/** @brief Snapshots the current surfaces and rebases history behind an undo boundary. */
	bool RebaseFlowMapHistoryWithUndoBoundary(
		TFunctionRef<bool()> EstablishUndoBoundary);

	/** @brief Advances the transactional sentinel used by irreversible history boundaries. */
	void AdvanceFlowMapEditSession();
#endif

protected:
	virtual void OnRegister() override;
	virtual void BeginPlay() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	bool RegenerateRenderTargets();
	bool SeedSurfacesFromAsset();
	bool InitializeFlowMapHistory(bool bCurrentStateIsSaved = false);
	void ResetFlowMapHistory();
	bool IsHistoryStrokeOpen() const;
	void RecordOp(const FStormFlowMapOp& Op);
	void RecordStamp(EStormFlowMapLayer Layer, const FStormFlowMapPaintParameters& PaintParameters);
	void RecordClear(EStormFlowMapLayer Layer);
	bool FlushPendingStrokeEdit();
	FGuid AppendHistoryEdit(const FStormFlowMapEdit& Edit);
	bool ResolveTransitionLayer(const FGuid& FromStateId, const FGuid& ToStateId, EStormFlowMapLayer& OutLayer) const;
	bool ReplayHistoryTo(const FGuid& TargetStateId);
	void MarkOwnerShapeDirty();

	void RefreshOwnerRenderData();
	void NotifyOwnerFlowMapChanged();

	UPROPERTY(Transient, DuplicateTransient)
	bool bFlowMapsInitialized = false;

	UPROPERTY(Transient, DuplicateTransient, NonTransactional)
	TObjectPtr<UTextureRenderTarget2D> HistoryOriginLowerRT = nullptr;

	UPROPERTY(Transient, DuplicateTransient, NonTransactional)
	TObjectPtr<UTextureRenderTarget2D> HistoryOriginMiddleRT = nullptr;

	UPROPERTY(Transient, DuplicateTransient, NonTransactional)
	TObjectPtr<UTextureRenderTarget2D> HistoryOriginUpperRT = nullptr;

	UPROPERTY(Transient, NonTransactional)
	TObjectPtr<UStormFlowMapUndoState> EditUndoState = nullptr;

	UPROPERTY(Transient, NonTransactional)
	FStormFlowMapEdit PendingStrokeEdit;

	FGuid ReplayedHistoryStateId;

	UPROPERTY(Transient, NonTransactional)
	FGuid SavedHistoryContentId;

	UPROPERTY(Transient, NonTransactional)
	FStormFlowmapParams SavedParams;

	UPROPERTY(Transient, NonTransactional)
	bool bHasSavedBaseline = false;

	// Transactional sentinel for an irreversible flow-map document/history boundary.
	// It has no authoring or runtime meaning.
	UPROPERTY(Transient)
	int32 FlowMapEditSessionSerial = 0;
};
