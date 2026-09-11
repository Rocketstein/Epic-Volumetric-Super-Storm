// Copyright 2026 GoroGoro. All Rights Reserved.

/**
 * @file StormFlowmapParams.h
 * @brief Defines editable and render-ready flow-map parameters.
 */

#pragma once

#include "CoreMinimal.h"
#include "StormFlowmapParams.generated.h"

/** @brief Identifies one of the three editable flow-map layers. */
UENUM(BlueprintType)
enum class EStormFlowMapLayer : uint8
{
	Lower,
	Middle,
	Upper,
};

/** @brief Defines editable layer heights, strengths, and playback timing for flow maps. */
USTRUCT(BlueprintType)
struct FStormFlowmapParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Storm|Flow Map", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LowerLayerHeight = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Storm|Flow Map", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MiddleLayerHeight = 0.50f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Storm|Flow Map", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float UpperLayerHeight = 0.85f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Storm|Flow Map", meta = (ClampMin = "0.0", UIMax = "16.0"))
	FVector3f UVWStrength = FVector3f(3.f, 3.f, 3.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Storm|Flow Map", meta = (ClampMin = "0.1", UIMin = "1.0", UIMax = "30.0"))
	float CycleDurationSeconds = 12.0f;
};

namespace VolumetricSuperStorm::FlowMap
{
	inline constexpr float MinLayerHeightGap = 1.0e-4f;

	VOLUMETRICSUPERSTORMRUNTIME_API FVector3f SanitizeLayerHeights(const FVector3f& LayerHeights);

	VOLUMETRICSUPERSTORMRUNTIME_API FVector3f EvaluateLayerWeights(float HLocal, const FVector3f& LayerHeights);

	VOLUMETRICSUPERSTORMRUNTIME_API FVector3f GetLayerHeights(const FStormFlowmapParams& Params);
}
