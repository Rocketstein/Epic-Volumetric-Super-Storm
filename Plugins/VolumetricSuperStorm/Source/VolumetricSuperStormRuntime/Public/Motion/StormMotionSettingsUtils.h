// Copyright 2026 GoroGoro. All Rights Reserved.

/**
 * @file StormMotionSettingsUtils.h
 * @brief Declares motion sanitization and shader-packing helpers.
 */

#pragma once

#include "CoreMinimal.h"
#include "Data/StormTypes.h"

namespace VolumetricSuperStorm::Motion
{
	inline constexpr int32 MinRingCount   = 1;
	inline constexpr int32 MaxRingCount   = 6;
	inline constexpr float MinRingWidth01 = 0.01f;

	struct FStormRingInfluenceRange
	{
		float InnerRadius01 = 0.0f;
		float OuterRadius01 = 0.0f;
	};

	VOLUMETRICSUPERSTORMRUNTIME_API void SynchronizeRingArrays(FStormMotionSettings& Settings);

	VOLUMETRICSUPERSTORMRUNTIME_API FStormMotionSettings SanitizeSettings(const FStormMotionSettings& Settings);

	VOLUMETRICSUPERSTORMRUNTIME_API float OuterBrimRadius01ToBodyRadius(float OuterBrimRadius01, float OuterBrimRadiusScale);

	VOLUMETRICSUPERSTORMRUNTIME_API FStormRingInfluenceRange GetRingInfluenceRange(const FStormMotionSettings& SanitizedSettings, int32 RingIndex);
}