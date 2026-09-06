/**
 * @file StormMotionSettingsUtils.cpp
 * @brief Sanitizes ring-motion settings and packs shader parameters.
 */

#include "Motion/StormMotionSettingsUtils.h"

namespace SavageSuperStorm::Motion
{
	namespace
	{
		constexpr float DefaultRingEndRadii01[MaxRingCount - 1] = { 0.10f, 0.22f, 0.36f, 0.54f, 0.74f };

		constexpr float DefaultRingAngularSpeedDegrees[MaxRingCount] = { 3.00f, 2.40f, 1.80f, 1.25f, 0.75f, 0.35f };

		constexpr float DefaultRingSkewDegrees[MaxRingCount] = { 140.0f, 96.0f, 60.0f, 34.0f, 16.0f, 0.0f };

		template <int32 NumDefaults>
		void ResizePreservingValues(TArray<float>& Values, int32 DesiredNum, const float (&Defaults)[NumDefaults])
		{
			check(DesiredNum >= 0 && DesiredNum <= NumDefaults);

			const int32 PreviousNum = Values.Num();
			Values.SetNum(DesiredNum);
			for (int32 Index = PreviousNum; Index < DesiredNum; ++Index)
			{
				Values[Index] = Defaults[Index];
			}
		}
	}

	void SynchronizeRingArrays(FStormMotionSettings& Settings)
	{
		Settings.RingCount = FMath::Clamp(Settings.RingCount, MinRingCount, MaxRingCount);

		ResizePreservingValues(Settings.RingEndRadii01, Settings.RingCount - 1, DefaultRingEndRadii01);
		ResizePreservingValues(Settings.RingAngularSpeedDegrees, Settings.RingCount, DefaultRingAngularSpeedDegrees);
		ResizePreservingValues(Settings.RingSkewDegrees, Settings.RingCount, DefaultRingSkewDegrees);
	}

	FStormMotionSettings SanitizeSettings(const FStormMotionSettings& Settings)
	{
		FStormMotionSettings Sanitized = Settings;
		SynchronizeRingArrays(Sanitized);

		Sanitized.MotionStrength = FMath::Clamp(Settings.MotionStrength, 0.0f, 4.0f);
		Sanitized.TimeScale      = FMath::Clamp(Settings.TimeScale, 0.0f, 100.0f);

		float       PreviousBoundary = 0.0f;
		const int32 BoundaryCount    = Sanitized.RingEndRadii01.Num();
		for (int32 BoundaryIndex = 0; BoundaryIndex < BoundaryCount; ++BoundaryIndex)
		{
			const int32 RemainingBoundaries         = BoundaryCount - BoundaryIndex - 1;
			const float MinimumBoundary             = PreviousBoundary + MinRingWidth01;
			const float MaximumBoundary             = 0.99f - static_cast<float>(RemainingBoundaries) * MinRingWidth01;
			Sanitized.RingEndRadii01[BoundaryIndex] = FMath::Clamp(Sanitized.RingEndRadii01[BoundaryIndex], MinimumBoundary, MaximumBoundary);
			PreviousBoundary                        = Sanitized.RingEndRadii01[BoundaryIndex];
		}

		for (float& SpeedDegrees : Sanitized.RingAngularSpeedDegrees)
		{
			SpeedDegrees = FMath::Clamp(SpeedDegrees, 0.0f, 360.0f);
		}
		for (float& SkewDegrees : Sanitized.RingSkewDegrees)
		{
			SkewDegrees = FMath::Clamp(SkewDegrees, -720.0f, 720.0f);
		}

		Sanitized.RadialShearGain = FMath::Clamp(Settings.RadialShearGain, 0.0f, 2.0f);

		const float LastBoundary    = BoundaryCount > 0 ? Sanitized.RingEndRadii01.Last() : 0.0f;
		Sanitized.MotionRadiusScale = FMath::Clamp(Settings.MotionRadiusScale, LastBoundary + MinRingWidth01, 2.0f);
		Sanitized.RadialFeather01   = FMath::Clamp(Settings.RadialFeather01, 0.0f, 0.5f);

		if (BoundaryCount == 0)
		{
			Sanitized.BoundaryOverlap01 = 0.0f;
		}
		else
		{
			float MinimumRingWidth = Sanitized.RingEndRadii01[0];
			for (int32 BoundaryIndex = 1; BoundaryIndex < BoundaryCount; ++BoundaryIndex)
			{
				MinimumRingWidth = FMath::Min(MinimumRingWidth, Sanitized.RingEndRadii01[BoundaryIndex] - Sanitized.RingEndRadii01[BoundaryIndex - 1]);
			}
			MinimumRingWidth            = FMath::Min(MinimumRingWidth, Sanitized.MotionRadiusScale - Sanitized.RingEndRadii01.Last());
			Sanitized.BoundaryOverlap01 = FMath::Clamp(Settings.BoundaryOverlap01, 0.0f, FMath::Min(0.2f, MinimumRingWidth * 0.9f));
		}

		Sanitized.HeightMin01            = FMath::Clamp(Settings.HeightMin01, 0.0f, 0.999f);
		Sanitized.HeightMax01            = FMath::Clamp(Settings.HeightMax01, Sanitized.HeightMin01 + 0.001f, 1.0f);
		Sanitized.HeightFeather01        = FMath::Clamp(Settings.HeightFeather01, 0.001f, 0.5f);
		Sanitized.LFRotationMultiplier   = FMath::Clamp(Settings.LFRotationMultiplier, 0.0f, 8.0f);
		Sanitized.HFRotationMultiplier   = FMath::Clamp(Settings.HFRotationMultiplier, 0.0f, 8.0f);
		Sanitized.CurlRotationMultiplier = FMath::Clamp(Settings.CurlRotationMultiplier, 0.0f, 8.0f);
		return Sanitized;
	}

	float OuterBrimRadius01ToBodyRadius(float OuterBrimRadius01, float OuterBrimRadiusScale)
	{
		const float SafeOuterBrimRadiusScale = FMath::Max(FMath::Abs(OuterBrimRadiusScale), 1.0e-4f);
		return OuterBrimRadius01 * SafeOuterBrimRadiusScale;
	}

	FStormRingInfluenceRange GetRingInfluenceRange(const FStormMotionSettings& SanitizedSettings, int32 RingIndex)
	{
		const TArray<float>& Boundaries = SanitizedSettings.RingEndRadii01;
		const int32          RingCount  = Boundaries.Num() + 1;
		if (RingIndex < 0 || RingIndex >= RingCount)
		{
			return {};
		}

		const float MotionOuterRadius01 = FMath::Max(0.0f, SanitizedSettings.MotionRadiusScale);
		const float HalfOverlap01       = 0.5f * FMath::Max(0.0f, SanitizedSettings.BoundaryOverlap01);
		const float InnerRadius01       = RingIndex > 0 ? Boundaries[RingIndex - 1] - HalfOverlap01 : 0.0f;
		const float OuterRadius01       = RingIndex < Boundaries.Num() ? Boundaries[RingIndex] + HalfOverlap01 : MotionOuterRadius01;

		FStormRingInfluenceRange Result;
		Result.InnerRadius01 = FMath::Clamp(InnerRadius01, 0.0f, MotionOuterRadius01);
		Result.OuterRadius01 = FMath::Clamp(OuterRadius01, Result.InnerRadius01, MotionOuterRadius01);
		return Result;
	}
}