// Copyright 2026 GoroGoro. All Rights Reserved.

/**
 * @file StormPresetDataAsset.cpp
 * @brief Validates edited storm preset values.
 */

#include "Assets/StormPresetDataAsset.h"
#include "Motion/StormMotionSettingsUtils.h"

#if WITH_EDITOR
void UStormPresetDataAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	VolumetricSuperStorm::Motion::SynchronizeRingArrays(PresetData.MotionSettings);
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif