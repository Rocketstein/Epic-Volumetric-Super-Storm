/**
 * @file StormRenderData.h
 * @brief Defines the runtime payload consumed by rendering and materials.
 */

#pragma once

#include "CoreMinimal.h"
#include "Data/StormTypes.h"
#include "StormRenderData.generated.h"

class UTexture;

/**
 * @brief Stores resolved flow-map textures and material controls.
 */
USTRUCT(BlueprintType)
struct SAVAGESUPERSTORMRUNTIME_API FStormFlowMapRenderData
{
	GENERATED_BODY()

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "Storm|Flow Map")
	TObjectPtr<UTexture> LowerTexture = nullptr;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "Storm|Flow Map")
	TObjectPtr<UTexture> MiddleTexture = nullptr;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "Storm|Flow Map")
	TObjectPtr<UTexture> UpperTexture = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Storm|Flow Map")
	FVector3f LayerHeights = FVector3f(0.15f, 0.50f, 0.85f);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Storm|Flow Map")
	FVector3f UVWStrength = FVector3f(0.03f, 0.03f, 0.01f);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Storm|Flow Map")
	float CycleDurationSeconds = 12.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Storm|Flow Map")
	bool bEnabled = false;
};

/**
 * @brief Stores the complete runtime payload consumed by rendering and materials.
 */
USTRUCT(BlueprintType)
struct SAVAGESUPERSTORMRUNTIME_API FStormRenderData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Storm|Render")
	FVector WorldCenter = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Storm|Render")
	FVector WorldExtent = FVector(800000.0, 800000.0, 2200.0);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Storm|Render")
	FStormShapeSettings Shape;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Storm|Render")
	FVector WindDirection = FVector::ForwardVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Storm|Render")
	float TimeSeconds = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Storm|Render|Motion")
	FStormMotionSettings Motion;

	FLinearColor MDRBoundaries0 = FLinearColor::Black;
	FLinearColor MDRBoundaries1 = FLinearColor::Black;
	FLinearColor MDRSpeeds0     = FLinearColor::Black;
	FLinearColor MDRSpeeds1     = FLinearColor::Black;
	FLinearColor MDRPhases0     = FLinearColor::Black;
	FLinearColor MDRPhases1     = FLinearColor::Black;
	FLinearColor MDRSkews0      = FLinearColor::Black;
	FLinearColor MDRSkews1      = FLinearColor::Black;
	FLinearColor MDRControl0    = FLinearColor::Black;
	FLinearColor MDRControl1    = FLinearColor::Black;

	EStormFormationState FormationState    = EStormFormationState::Mature;
	float                FormationProgress = 1.0f;

	bool bLifecycleWarmup = false;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "Storm|Render")
	TObjectPtr<UTexture> BottomProfileTexture = nullptr;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "Storm|Render")
	TObjectPtr<UTexture> TopProfileTexture = nullptr;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "Storm|Render")
	TObjectPtr<UTexture> AnvilProfileTexture = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Storm|Render")
	FStormFlowMapRenderData FlowMap;
};