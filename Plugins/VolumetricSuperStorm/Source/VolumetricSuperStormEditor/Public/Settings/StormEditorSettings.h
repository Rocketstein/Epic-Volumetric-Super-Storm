// Copyright 2026 GoroGoro. All Rights Reserved.

/**
 * @file StormEditorSettings.h
 * @brief Declares the project settings that locate the storm authoring directories.
 */

#pragma once

#include "Engine/DeveloperSettings.h"

#include "StormEditorSettings.generated.h"

/**
 * @brief Locates the content directories the storm painters save into.
 *
 * The defaults live under /Game rather than the plugin's own content root. Storm content the
 * user authors is project content: a Fab install can place the plugin in a read-only engine
 * directory, and a plugin update replaces everything the plugin ships. Assets saved into the
 * plugin folder would be lost either way.
 */
UCLASS(config = Editor, defaultconfig, meta = (DisplayName = "Volumetric Super Storm"))
class UStormEditorSettings final : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UStormEditorSettings();

	virtual FName GetCategoryName() const override;

	/** Destination for Save Preset As, and one of the two folders the preset browser lists. */
	UPROPERTY(EditAnywhere, config, Category = "Authoring Directories", meta = (ContentDir))
	FDirectoryPath PresetDirectory;

	/** Destination for the profile painter's Save / Save As. */
	UPROPERTY(EditAnywhere, config, Category = "Authoring Directories", meta = (ContentDir))
	FDirectoryPath VerticalProfileDirectory;

	/** Destination for the flow-map editor's Save / Save As. */
	UPROPERTY(EditAnywhere, config, Category = "Authoring Directories", meta = (ContentDir))
	FDirectoryPath FlowMapDirectory;
};
