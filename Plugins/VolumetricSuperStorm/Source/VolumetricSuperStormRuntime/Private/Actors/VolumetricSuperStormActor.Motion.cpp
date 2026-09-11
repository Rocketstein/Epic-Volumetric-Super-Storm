// Copyright 2026 GoroGoro. All Rights Reserved.

/**
 * @file VolumetricSuperStormActor.Motion.cpp
 * @brief Integrates ring motion and updates the storm motion phase state.
 */

#include "Actors/VolumetricSuperStormActor.h"

#include "Motion/StormMotionSettingsUtils.h"

namespace
{
	constexpr double StormTwoPi = 2.0 * UE_DOUBLE_PI;

	/** @brief Splits an unwrapped angle into a [-pi, pi) remainder and a whole-turn count. */
	FStormRingPhase MakeRingPhase(double UnwrappedRadians)
	{
		const double    Turns = FMath::FloorToDouble((UnwrappedRadians + UE_DOUBLE_PI) / StormTwoPi);
		FStormRingPhase Phase;
		Phase.WrappedRadians = UnwrappedRadians - Turns * StormTwoPi;
		Phase.TurnCount      = static_cast<int64>(Turns);
		return Phase;
	}

	/** @brief Advances a ring by DeltaRadians, carrying completed turns into TurnCount. */
	void AdvanceRingPhase(FStormRingPhase& Phase, double DeltaRadians)
	{
		const double Unwrapped = Phase.WrappedRadians + DeltaRadians;
		const double Turns     = FMath::FloorToDouble((Unwrapped + UE_DOUBLE_PI) / StormTwoPi);
		Phase.WrappedRadians   = Unwrapped - Turns * StormTwoPi;
		Phase.TurnCount        += static_cast<int64>(Turns);
	}
}

void AVolumetricSuperStormActor::SetStormMotionEnabled(bool bEnabled)
{
	MotionSettings.bEnabled = bEnabled;
	if (bEnabled)
	{
		SetActorTickEnabled(true);
	}
	RebuildRenderData();
}

void AVolumetricSuperStormActor::PauseStormMotion()
{
	bMotionPaused = true;
	RebuildRenderData();
}

void AVolumetricSuperStormActor::ResumeStormMotion()
{
	bMotionPaused = false;
	SetActorTickEnabled(true);
	RebuildRenderData();
}

void AVolumetricSuperStormActor::ResetStormMotion()
{
	MotionElapsedSeconds                       = 0.0;
	const FStormMotionSettings SanitizedMotion = VolumetricSuperStorm::Motion::SanitizeSettings(MotionSettings);
	RingPhases.Init(FStormRingPhase(), SanitizedMotion.RingCount);
	RebuildRenderData();
}

void AVolumetricSuperStormActor::SetStormMotionTime(float TimeSeconds)
{
	MotionElapsedSeconds = FMath::Max(0.0, static_cast<double>(TimeSeconds));
	RebuildMotionPhasesFromTime(MotionElapsedSeconds);
	RebuildRenderData();
}

bool AVolumetricSuperStormActor::TickMotion(float DeltaSeconds)
{
	if (!MotionSettings.bEnabled || bMotionPaused || DeltaSeconds <= 0.0f)
	{
		return false;
	}

	const FStormMotionSettings SanitizedMotion = VolumetricSuperStorm::Motion::SanitizeSettings(MotionSettings);
	SynchronizeMotionPhaseCount(SanitizedMotion);
	const double Direction          = ShapeSettings.bClockwise ? 1.0 : -1.0;
	const double ScaledDeltaSeconds = static_cast<double>(DeltaSeconds) * static_cast<double>(SanitizedMotion.TimeScale);
	MotionElapsedSeconds            += static_cast<double>(DeltaSeconds);

	for (int32 RingIndex = 0; RingIndex < SanitizedMotion.RingCount; ++RingIndex)
	{
		const double RadiansPerSecond = FMath::DegreesToRadians(static_cast<double>(SanitizedMotion.RingAngularSpeedDegrees[RingIndex]));
		AdvanceRingPhase(RingPhases[RingIndex], Direction * RadiansPerSecond * ScaledDeltaSeconds);
	}
	return true;
}

void AVolumetricSuperStormActor::SynchronizeMotionPhaseCount(const FStormMotionSettings& SanitizedMotion)
{
	if (RingPhases.Num() == SanitizedMotion.RingCount)
	{
		return;
	}

	const int32 PreviousPhaseCount = RingPhases.Num();
	RingPhases.SetNum(SanitizedMotion.RingCount);
	if (SanitizedMotion.RingCount <= PreviousPhaseCount)
	{
		return;
	}

	const double Direction = ShapeSettings.bClockwise ? 1.0 : -1.0;
	for (int32 RingIndex = PreviousPhaseCount; RingIndex < SanitizedMotion.RingCount; ++RingIndex)
	{
		const double RadiansPerSecond = FMath::DegreesToRadians(static_cast<double>(SanitizedMotion.RingAngularSpeedDegrees[RingIndex]));
		RingPhases[RingIndex]         = MakeRingPhase(Direction * RadiansPerSecond * static_cast<double>(SanitizedMotion.TimeScale) * MotionElapsedSeconds);
	}
}

void AVolumetricSuperStormActor::RebuildMotionPhasesFromTime(double InTimeSeconds)
{
	const FStormMotionSettings SanitizedMotion = VolumetricSuperStorm::Motion::SanitizeSettings(MotionSettings);
	RingPhases.SetNum(SanitizedMotion.RingCount);
	const double Direction = ShapeSettings.bClockwise ? 1.0 : -1.0;
	for (int32 RingIndex = 0; RingIndex < SanitizedMotion.RingCount; ++RingIndex)
	{
		const double RadiansPerSecond = FMath::DegreesToRadians(static_cast<double>(SanitizedMotion.RingAngularSpeedDegrees[RingIndex]));
		RingPhases[RingIndex]         = MakeRingPhase(Direction * RadiansPerSecond * static_cast<double>(SanitizedMotion.TimeScale) * InTimeSeconds);
	}
}