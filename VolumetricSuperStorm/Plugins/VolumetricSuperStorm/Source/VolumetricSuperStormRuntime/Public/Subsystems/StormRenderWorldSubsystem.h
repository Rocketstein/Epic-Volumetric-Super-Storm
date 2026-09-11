// Copyright 2026 GoroGoro. All Rights Reserved.

/**
 * @file StormRenderWorldSubsystem.h
 * @brief Owns shape render targets for the single storm registered in a world.
 */

#pragma once

#include "CoreMinimal.h"
#include "Data/StormRenderData.h"
#include "Subsystems/WorldSubsystem.h"
#include "StormRenderWorldSubsystem.generated.h"

class AVolumetricSuperStormActor;
class UCanvas;
class UTextureRenderTarget2D;

/**
 * @brief Owns render resources for the single storm registered in a world.
 * @details Additional storm actors are rejected instead of replacing or sharing
 * mutable shape data.
 */
UCLASS()
class VOLUMETRICSUPERSTORMRUNTIME_API UStormRenderWorldSubsystem final : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:

	/** @brief Registers the only storm allowed in this world. */
	bool RegisterStorm(AVolumetricSuperStormActor* StormActor);

	/** @brief Releases the registered storm and all transient shape resources. */
	void UnregisterStorm(const AVolumetricSuperStormActor* StormActor);

	/** @brief Returns whether the actor owns this world's storm state. */
	bool IsRegisteredStorm(const AVolumetricSuperStormActor* StormActor) const;

	/** @brief Returns the primary world-owned shape texture. */
	UFUNCTION(BlueprintPure, Category = "Storm|Render")
	UTextureRenderTarget2D* GetShapeRenderTarget() const;

	/** @brief Returns the secondary world-owned shape texture. */
	UFUNCTION(BlueprintPure, Category = "Storm|Render")
	UTextureRenderTarget2D* GetShapeRenderTarget2() const;

	/** @brief Requests a new shape bake for the registered storm. */
	void MarkStormShapeDirty(const AVolumetricSuperStormActor* StormActor);

	/** @brief Stores complete render data and schedules a shape bake when required. */
	bool SubmitStormRenderData(
		AVolumetricSuperStormActor* StormActor,
		const FStormRenderData& RenderData);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;
	virtual bool IsTickableInEditor() const override;
	virtual bool IsTickableWhenPaused() const override;

private:
	bool EnsureShapeRenderTargets();
	bool EnsureShapeRenderTarget();
	bool EnsureShapeRenderTarget2();
	bool EnsureRenderTarget(
		TObjectPtr<UTextureRenderTarget2D>& RenderTarget,
		const TCHAR* RenderTargetName);
	bool ValidateRenderTarget(
		const UTextureRenderTarget2D* RenderTarget,
		const TCHAR* RenderTargetName) const;
	bool BuildShapeRenderTargets();
	void ResetStormState();
	void DrawStatsOverlay(UCanvas* Canvas, APlayerController* PlayerController);

	FDelegateHandle StatsDrawHandle;
	FDelegateHandle StatsDrawEditorHandle;
	double LastTickMs = 0.0;
	double LastSubmitMs = 0.0;
	double LastBuildShapeMs = 0.0;
	int32 ShapeBakeCount = 0;
	bool bShapeDirty = true;
	uint32 PendingShapeBakeHash = 0;
	uint32 LastBakedShapeHash = 0;
	bool bHasPendingShapeBake = false;
	bool bHasLastBakedShapeHash = false;
	TArray<FVector4f> CoverageStrengthCurveLUT;
	TArray<FVector4f> TypeStrengthCurveLUT;
	TArray<FVector4f> LayerHeightCurveLUT;

	TWeakObjectPtr<AVolumetricSuperStormActor> RegisteredStorm;

	UPROPERTY(Transient)
	FStormRenderData StormRenderData;

	UPROPERTY(Transient)
	bool bHasStormRenderData = false;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> ShapeRenderTarget = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> ShapeRenderTarget2 = nullptr;
};
