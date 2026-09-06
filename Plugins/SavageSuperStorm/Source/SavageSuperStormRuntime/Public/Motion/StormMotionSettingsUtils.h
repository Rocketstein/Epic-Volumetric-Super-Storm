/**
 * @file StormMotionSettingsUtils.h
 * @brief Declares motion sanitization and shader-packing helpers.
 */

#pragma once

#include "CoreMinimal.h"
#include "Data/StormTypes.h"

namespace SavageSuperStorm::Motion
{
	inline constexpr int32 MinRingCount   = 1;
	inline constexpr int32 MaxRingCount   = 6;
	inline constexpr float MinRingWidth01 = 0.01f;

	struct FStormRingInfluenceRange
	{
		float InnerRadius01 = 0.0f;
		float OuterRadius01 = 0.0f;
	};

	SAVAGESUPERSTORMRUNTIME_API void SynchronizeRingArrays(FStormMotionSettings& Settings);

	SAVAGESUPERSTORMRUNTIME_API FStormMotionSettings SanitizeSettings(const FStormMotionSettings& Settings);

	SAVAGESUPERSTORMRUNTIME_API float OuterBrimRadius01ToBodyRadius(float OuterBrimRadius01, float OuterBrimRadiusScale);

	SAVAGESUPERSTORMRUNTIME_API FStormRingInfluenceRange GetRingInfluenceRange(const FStormMotionSettings& SanitizedSettings, int32 RingIndex);
}