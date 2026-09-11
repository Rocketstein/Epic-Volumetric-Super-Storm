// Copyright 2026 GoroGoro. All Rights Reserved.

/**
 * @file StormAuthoringPaths.h
 * @brief Declares the shared save/load dialogs that keep storm content in one directory.
 */

#pragma once

#include "CoreMinimal.h"

#include "AssetRegistry/AssetData.h"

namespace VolumetricSuperStorm::AuthoringPaths
{
	/** The three kinds of storm content the painters save, one directory each. */
	enum class ECategory : uint8
	{
		Preset,
		VerticalProfile,
		FlowMap
	};

	/**
	 * Returns the writable directory this category saves into, as a long package path.
	 * Comes from UStormEditorSettings; falls back to the built-in /Game default if that is
	 * empty, names a root that is not mounted, or points into the shipped read-only directory.
	 */
	FString GetAuthoringRoot(ECategory Category);

	/** Returns the plugin's read-only directory of shipped examples for this category. */
	FString GetShippedRoot(ECategory Category);

	/** Whether Asset belongs to this category's shipped plugin content. */
	bool IsShippedAsset(const UObject* Asset, ECategory Category);

	/** Whether an existing asset may be overwritten by an authoring tool. */
	bool CanOverwriteAsset(const UObject* Asset, ECategory Category);

	/** Shared explanation used by disabled overwrite actions. */
	FText GetShippedAssetOverwriteBlockedText();

	/**
	 * Creates the authoring directory on disk and registers it with the asset registry.
	 *
	 * The stock content-browser dialogs silently retarget /Game when their default path
	 * names a folder that does not exist, so every entry point materializes the folder
	 * before opening a dialog. Also makes the folder visible in the Content Browser
	 * before anything has been saved into it.
	 */
	void EnsureAuthoringRootExists(ECategory Category);

	/** Materializes all three authoring directories. */
	void EnsureAuthoringRootsExist();

	/**
	 * Opens a modal picker listing only assets of AssetClass inside this category's
	 * authoring and shipped directories. Returns an invalid FAssetData if cancelled.
	 */
	FAssetData PickAssetModal(ECategory Category, const UClass* AssetClass, const FText& Title);

	/** Typed convenience wrapper around PickAssetModal. Returns nullptr if cancelled. */
	template <typename AssetType>
	AssetType* PickAssetModal(ECategory Category, const FText& Title)
	{
		return Cast<AssetType>(
			PickAssetModal(Category, AssetType::StaticClass(), Title).GetAsset());
	}

	/**
	 * Opens a modal name-only save dialog for this category's authoring directory.
	 *
	 * There is no path picker: the destination is fixed, so a save cannot land outside the
	 * directory. An existing asset of the same class is offered as an overwrite; an existing
	 * asset of any other class blocks the name.
	 *
	 * @param DefaultAssetName Name to seed the field with, sanitized before display.
	 * @param OutPackageName   Long package name to create, valid only when this returns true.
	 * @param OutAssetName     Asset name within that package.
	 * @return true when the user confirmed a usable name.
	 */
	bool PickSaveNameModal(
		ECategory Category,
		const UClass* AssetClass,
		const FString& DefaultAssetName,
		const FText& Title,
		FString& OutPackageName,
		FString& OutAssetName);
}
