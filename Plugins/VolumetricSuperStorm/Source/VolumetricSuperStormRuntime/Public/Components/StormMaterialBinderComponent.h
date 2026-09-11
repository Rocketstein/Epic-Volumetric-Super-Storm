// Copyright 2026 GoroGoro. All Rights Reserved.

/**
 * @file StormMaterialBinderComponent.h
 * @brief Binds storm render data through a global MPC or dynamic material instance.
 */

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Containers/StaticArray.h"
#include "Data/StormLightningTypes.h"
#include "Data/StormRenderData.h"
#include "Data/StormRenderUpdateTypes.h"
#include "Engine/EngineTypes.h"
#include "StormMaterialBinderComponent.generated.h"

class UMaterialInterface;
class UMaterialInstanceDynamic;
class UMaterialParameterCollection;
class UMaterialParameterCollectionInstance;
class UTexture;
class UTextureRenderTarget2D;
class UVolumetricCloudComponent;
namespace VolumetricSuperStorm
{
	enum class EStormMaterialTextureSlot : uint8;
	struct FStormMaterialPayload;
	struct FStormMaterialTextureSources;
}

/** Selects the material parameter source authored into the cloud material. */
UENUM(BlueprintType)
enum class EStormMaterialBindingBackend : uint8
{
	GlobalMPC UMETA(DisplayName = "Global MPC"),
	DynamicMaterialInstance UMETA(DisplayName = "Dynamic Material Instance")
};

/**
 * @brief Applies one storm actor's render data through the selected material backend.
 * @details Global MPC mode leaves the cloud material untouched. MID mode accepts
 * only materials implementing the storm material contract and refuses ambiguous
 * automatic cloud selection.
 */
UCLASS(ClassGroup = (Storm), meta = (BlueprintSpawnableComponent))
class VOLUMETRICSUPERSTORMRUNTIME_API UStormMaterialBinderComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UStormMaterialBinderComponent();

	/** @brief Resolves the explicitly configured or uniquely discovered cloud. */
	UFUNCTION(BlueprintCallable, Category = "Storm|Material")
	UVolumetricCloudComponent* ResolveTargetCloud() const;

	/** @brief Uploads the complete material payload after a static change. */
	UFUNCTION(BlueprintCallable, Category = "Storm|Material")
	void PushStormRenderData(const FStormRenderData& RenderData);

	/** @brief Uploads the render-data subset selected by a backend-neutral scope. */
	void PushStormData(const FStormRenderData& RenderData, EStormRenderUpdateScope Scope);

	/** @brief Coalesces dirty texture bindings into one global-anchor sync per frame. */
	void QueueStormTexturePayload(const VolumetricSuperStorm::FStormMaterialPayload& Payload);

	/** @brief Releases the active backend, restoring an MID-bound cloud when necessary. */
	void ReleaseVolumetricCloudMaterial();

	/** @brief Creates a fresh MID when using the MID backend; otherwise refreshes global state. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Storm|Volumetric Cloud")
	UMaterialInstanceDynamic* ApplyVolumetricCloudMaterial();

	/** @brief Uploads a complete state refresh through the selected backend. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Storm|Volumetric Cloud")
	void UpdateVolumetricCloudMaterial();

	/** @brief Returns the binder-owned dynamic material instance. */
	UFUNCTION(BlueprintPure, Category = "Storm|Volumetric Cloud")
	UMaterialInstanceDynamic* GetDynamicCloudMaterial() const;

	/** @brief Selects the backend matching the material function authored into the cloud material. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Material")
	EStormMaterialBindingBackend BindingBackend = EStormMaterialBindingBackend::GlobalMPC;

	/** @brief Uploads the complete lightning material state. */
	UFUNCTION(BlueprintCallable, Category = "Storm|Material|Lightning")
	void ApplyLightningMaterialState(const FStormLightningMaterialState& State);

	/** @brief Updates the lightning pulse scalar. */
	UFUNCTION(BlueprintCallable, Category = "Storm|Material|Lightning")
	void SetLightningPulse(float Pulse);

	/** @brief Clears all lightning material values. */
	UFUNCTION(BlueprintCallable, Category = "Storm|Material|Lightning")
	void ClearLightningMaterialState();

protected:
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void OnComponentCreated() override;
	virtual void BeginPlay() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	/** @brief Explicit cloud target; automatic binding succeeds only when exactly one cloud exists. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Storm|Volumetric Cloud", meta = (UseComponentPicker, AllowAnyActor, AllowedClasses = "/Script/Engine.VolumetricCloudComponent", EditCondition = "BindingBackend == EStormMaterialBindingBackend::DynamicMaterialInstance", EditConditionHides))
	FComponentReference TargetCloudReference;

	/** @brief Parent material required to implement the storm material contract. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Volumetric Cloud", meta = (EditCondition = "BindingBackend == EStormMaterialBindingBackend::DynamicMaterialInstance", EditConditionHides))
	TObjectPtr<UMaterialInterface> BaseCloudMaterial;

#if WITH_EDITORONLY_DATA
	/** @brief Tracks external material reassignment while editing. */
	UPROPERTY(EditAnywhere, Category = "Storm|Volumetric Cloud", meta = (EditCondition = "BindingBackend == EStormMaterialBindingBackend::DynamicMaterialInstance", EditConditionHides))
	bool bTrackTargetCloudMaterialChange = true;

#endif

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DynamicCloudMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> PreviousCloudMaterial;

private:
	bool                       IsTargetCloudReferenceSet() const;
	UVolumetricCloudComponent* FindUniqueWorldCloud() const;
	void                       TryAutoResolveTargetCloud();

	void RefreshCloudMaterialBinding(bool bReapplyCloudMaterial);

	UMaterialInstanceDynamic* EnsureDynamicCloudMaterial(bool bForceRecreate = false);

	/** @brief Activates and validates the currently selected material backend. */
	bool ActivateSelectedBackend(bool bForceRecreateMID = false);

	/** @brief Releases the active backend and restores any replaced cloud material. */
	void DeactivateCurrentBackend();

	/** @brief Returns whether the selected backend can accept incremental updates. */
	bool IsReadyForIncrementalUpload() const;

	/** @brief Resolves the shape textures supplied by the world render subsystem. */
	VolumetricSuperStorm::FStormMaterialTextureSources ResolveTextureSources() const;

	/** @brief Dispatches one payload exclusively to the selected backend. */
	bool ApplyPayload(const VolumetricSuperStorm::FStormMaterialPayload& Payload);

	/** @brief Applies one payload to the binder-owned dynamic material instance. */
	bool ApplyPayloadToMID(const VolumetricSuperStorm::FStormMaterialPayload& Payload);

	/** @brief Applies values to the global MPC and synchronizes texture anchors. */
	bool ApplyPayloadToMPC(const VolumetricSuperStorm::FStormMaterialPayload& Payload);

	/** @brief Resolves this world's instance of the global storm MPC. */
	UMaterialParameterCollectionInstance* ResolveGlobalParameterCollectionInstance() const;

	/** @brief Resolves the fixed render-target asset for a texture payload slot. */
	UTextureRenderTarget2D* ResolveGlobalTextureAnchor(VolumetricSuperStorm::EStormMaterialTextureSlot Slot) const;

	/** @brief Rejects invalid sources and reports when a usable source will be resampled. */
	bool ValidateGlobalTextureAnchorDimensions(UTextureRenderTarget2D* Anchor, const UTexture* SourceTexture) const;

	/** @brief Copies a source texture into its fixed global render-target anchor. */
	bool SynchronizeGlobalTextureAnchor(VolumetricSuperStorm::EStormMaterialTextureSlot Slot, UTexture* SourceTexture) const;

	/** @brief Acquires exclusive process-wide ownership of the texture anchors. */
	bool AcquireGlobalTextureAnchors();

	/** @brief Resizes the acquired anchors once and waits for their render resources. */
	void BeginGlobalTextureAnchorInitialization();

	/** @brief Invalidates any pending asynchronous anchor initialization callback. */
	void CancelGlobalTextureAnchorInitialization();

	/** @brief Releases this binder's ownership of the global texture anchors. */
	void ReleaseGlobalTextureAnchors();

	/** @brief Clears all global anchors to their slot-specific neutral values. */
	void ClearGlobalTextureAnchors() const;

	/** @brief Disables visible storm state in this world's MPC instance. */
	void DisableGlobalMaterialState() const;

	/** @brief Returns whether this binder currently owns the global anchors. */
	bool HasGlobalTextureAnchorOwnership() const;

	/** @brief Flushes texture bindings dirtied since the previous frame. */
	void FlushPendingTexturePayload();

	/** @brief Discards queued slots superseded by a broader update. */
	void DiscardPendingTextureSlots(uint16 SlotMask);

#if WITH_EDITOR
	void HandleObjectPropertyChanged(UObject* Object, FPropertyChangedEvent& PropertyChangedEvent);
	void QueueTargetCloudMaterialRebind();

	FDelegateHandle ObjectPropertyChangedHandle;
	bool            bCloudMaterialRebindPending = false;
#endif

private:
	TWeakObjectPtr<UMaterialInstanceDynamic> LastFullyUploadedMaterial;
	EStormMaterialBindingBackend ActiveBindingBackend = EStormMaterialBindingBackend::GlobalMPC;
	bool bHasActiveBindingBackend = false;
	bool bHasUploadedFullPayload = false;
	bool bOwnsGlobalTextureAnchors = false;
	bool bGlobalTextureAnchorsReady = false;
	uint32 GlobalTextureAnchorInitializationGeneration = 0;
	static constexpr int32 MaterialTextureSlotCount = 8;
	TStaticArray<TWeakObjectPtr<UTexture>, MaterialTextureSlotCount> PendingTextures;
	uint16 PendingTextureMask = 0;
	bool bTextureFlushScheduled = false;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialParameterCollection> GlobalParameterCollection;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> GlobalShapeAnchor;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> GlobalShape2Anchor;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> GlobalProfileBottomAnchor;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> GlobalProfileTopAnchor;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> GlobalProfileAnvilAnchor;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> GlobalFlowBottomAnchor;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> GlobalFlowMiddleAnchor;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> GlobalFlowTopAnchor;

	mutable TWeakObjectPtr<UVolumetricCloudComponent> ResolvedTargetCloud;
	mutable bool                                      bReportedAmbiguousCloudTarget     = false;
	bool                                              bReportedInvalidMaterial          = false;
	bool                                              bReportedMissingGlobalResources   = false;
	bool                                              bReportedGlobalOwnershipConflict  = false;
	mutable bool                                      bReportedUnexpectedAnchorSize     = false;
	bool                                              bHasCapturedPreviousCloudMaterial = false;
};
