#pragma once

#include "CoreMinimal.h"

class UStormVerticalProfileAsset;
class UStormVerticalProfileToolComponent;

namespace SavageSuperStorm::ProfileEditor
{
	/** Finds a key by stable ID so selection survives timeline reordering. */
	int32 FindKeyIndexById(
		const UStormVerticalProfileAsset* ProfileAsset,
		const FGuid& KeyId);

	/** Suggests an available time immediately after SourceIndex. */
	float SuggestTimeAfter(
		const UStormVerticalProfileAsset* ProfileAsset,
		int32 SourceIndex);

	/** Bakes the component's live surfaces and parameters into one timeline key. */
	bool CommitToolToKey(
		UStormVerticalProfileToolComponent* Tool,
		UStormVerticalProfileAsset* ProfileAsset,
		int32 KeyIndex);

	/**
	 * Bakes every resident per-key working set into the asset.
	 *
	 * Keys that were never activated already match their embedded textures and
	 * do not require a GPU readback.
	 */
	bool CommitToolToAllKeys(
		UStormVerticalProfileToolComponent* Tool,
		UStormVerticalProfileAsset* ProfileAsset,
		const FGuid& ActiveKeyId);

	/** Creates an independently editable copy of SourceIndex after the source key. */
	bool DuplicateKeyAfter(
		UStormVerticalProfileAsset* ProfileAsset,
		int32 SourceIndex,
		FGuid& OutNewKeyId);

	/**
	 * Creates an independent copy at an explicit time and keeps the key array sorted.
	 * The requested time must not overlap an existing key.
	 */
	bool DuplicateKeyAtTime(
		UStormVerticalProfileAsset* ProfileAsset,
		int32 SourceIndex,
		float TimeSeconds,
		FGuid& OutNewKeyId);

	/** Deep-copies a timeline into another asset, including its embedded textures. */
	bool CopyKeysToAsset(
		const UStormVerticalProfileAsset* SourceAsset,
		UStormVerticalProfileAsset* DestinationAsset);

	/**
	 * Removes one key while preserving the invariant that an asset has at least one.
	 * Unreferenced embedded textures remain transaction-safe until the package saves.
	 */
	bool RemoveKey(
		UStormVerticalProfileAsset* ProfileAsset,
		int32 KeyIndex);
}
