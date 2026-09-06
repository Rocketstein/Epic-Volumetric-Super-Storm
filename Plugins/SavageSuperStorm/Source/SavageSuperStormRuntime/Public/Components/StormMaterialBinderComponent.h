/**
 * @file StormMaterialBinderComponent.h
 * @brief Binds storm render data to one volumetric cloud material instance.
 */

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/StormLightningTypes.h"
#include "Data/StormRenderData.h"
#include "Engine/EngineTypes.h"
#include "StormMaterialBinderComponent.generated.h"

class UMaterialInterface;
class UMaterialInstanceDynamic;
class UVolumetricCloudComponent;

/**
 * @brief Applies one storm actor's render data to one volumetric cloud.
 * @details The component accepts only materials implementing the storm material
 * contract and refuses ambiguous automatic cloud selection.
 */
UCLASS(ClassGroup = (Storm), meta = (BlueprintSpawnableComponent))
class SAVAGESUPERSTORMRUNTIME_API UStormMaterialBinderComponent : public UActorComponent
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

	/** @brief Uploads only per-frame spatial, motion, and lifecycle values. */
	void PushStormFrameData(const FStormRenderData& RenderData);

	/** @brief Uploads only vertical profile textures. */
	void PushStormProfileData(const FStormRenderData& RenderData);

	/** @brief Restores the cloud material that was active before binding. */
	void ReleaseVolumetricCloudMaterial();

	/** @brief Creates a fresh dynamic cloud material and uploads state. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Storm|Volumetric Cloud")
	UMaterialInstanceDynamic* ApplyVolumetricCloudMaterial();

	/** @brief Refreshes the current dynamic cloud material. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Storm|Volumetric Cloud")
	void UpdateVolumetricCloudMaterial();

	/** @brief Returns the binder-owned dynamic material instance. */
	UFUNCTION(BlueprintPure, Category = "Storm|Volumetric Cloud")
	UMaterialInstanceDynamic* GetDynamicCloudMaterial() const;

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
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Storm|Volumetric Cloud", meta = (UseComponentPicker, AllowAnyActor, AllowedClasses = "/Script/Engine.VolumetricCloudComponent"))
	FComponentReference TargetCloudReference;

	/** @brief Parent material required to implement the storm material contract. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Volumetric Cloud")
	TObjectPtr<UMaterialInterface> BaseCloudMaterial;

#if WITH_EDITORONLY_DATA
	/** @brief Tracks external material reassignment while editing. */
	UPROPERTY(EditAnywhere, Category = "Storm|Volumetric Cloud")
	bool bTrackTargetCloudMaterialChange = true;

#endif

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DynamicCloudMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> PreviousCloudMaterial;

private:
	void CaptureScaleReferenceIfNeeded(const FStormRenderData& RenderData);

	bool                       IsTargetCloudReferenceSet() const;
	UVolumetricCloudComponent* FindUniqueWorldCloud() const;
	void                       TryAutoResolveTargetCloud();

	void RefreshCloudMaterialBinding(bool bReapplyCloudMaterial);

	UMaterialInstanceDynamic* EnsureDynamicCloudMaterial(bool bForceRecreate = false);

#if WITH_EDITOR
	void HandleObjectPropertyChanged(UObject* Object, FPropertyChangedEvent& PropertyChangedEvent);
	void QueueTargetCloudMaterialRebind();

	FDelegateHandle ObjectPropertyChangedHandle;
	bool            bCloudMaterialRebindPending = false;
#endif

	void UploadAllParamsToMID(const FStormRenderData& RenderData) const;

	void UploadTexturesToMID(const FStormRenderData& RenderData) const;

	void UploadLayerParametersToMID(const FStormRenderData& RenderData) const;

	void UploadFrameSpatialParamsToMID(const FStormRenderData& RenderData) const;

	void UploadShapeSettingsToMID(const FStormRenderData& RenderData) const;

	void UploadMotionSettingsToMID(const FStormRenderData& RenderData) const;

	void UploadPerFrameParamsToMID(const FStormRenderData& RenderData) const;

private:
	float ScaleReferenceRadius = 0.0f;

	TWeakObjectPtr<UMaterialInstanceDynamic> LastFullyUploadedMaterial;

	mutable TWeakObjectPtr<UVolumetricCloudComponent> ResolvedTargetCloud;
	mutable bool                                      bReportedAmbiguousCloudTarget     = false;
	bool                                              bReportedInvalidMaterial          = false;
	bool                                              bHasCapturedPreviousCloudMaterial = false;
};