// Copyright 2026 GoroGoro. All Rights Reserved.

/**
 * @file StormEditorSettings.cpp
 * @brief Implements the defaults for the storm authoring directories.
 */

#include "Settings/StormEditorSettings.h"

UStormEditorSettings::UStormEditorSettings()
{
	PresetDirectory.Path = TEXT("/Game/SuperStorm/Presets");
	VerticalProfileDirectory.Path = TEXT("/Game/SuperStorm/VerticalProfiles");
	FlowMapDirectory.Path = TEXT("/Game/SuperStorm/FlowMaps");
}

FName UStormEditorSettings::GetCategoryName() const
{
	return TEXT("Plugins");
}
