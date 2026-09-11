// Copyright 2026 GoroGoro. All Rights Reserved.

/**
 * @file StormVerticalProfileAsset.h
 * @brief Declares the baked vertical profile asset.
 */

#pragma once

#include "CoreMinimal.h"
#include "Data/Profile/StormProfileKey.h"
#include "Engine/DataAsset.h"
#include "Engine/Texture2D.h"
#include "StormVerticalProfileAsset.generated.h"

/**
 * Ordered, baked vertical-profile poses that can be sampled as a timeline.
 *
 * Runtime playback requires at least two structurally valid keys, beginning at
 * time zero, with complete bottom, top, and anvil surfaces on every key.
 */
UCLASS(BlueprintType)
class VOLUMETRICSUPERSTORMRUNTIME_API UStormVerticalProfileAsset : public UDataAsset
{
    GENERATED_BODY()

public:
    static constexpr int32 MaxProfileKeys = 16;

    /**
     * Upper bound on any key's TimeSeconds, and therefore on sequence duration.
     * Every authoring entry point -- the painter's time spin box, a timeline key
     * drag, and right-click add -- clamps or rejects against this one value, so
     * no path can author a duration the others would consider out of range.
     */
    static constexpr float MaxKeyTimeSeconds = 3600.0f;

	/**
	 * Load-only compatibility for assets authored before profile sequencing.
	 * These fields are intentionally retained until the legacy assets have been
	 * opened, migrated into Keys[0], and resaved in the asset phase.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Profile|Legacy", meta = (DeprecatedProperty, DeprecationMessage = "Migrated into Keys[0] on load."))
	TObjectPtr<UTexture2D> TopProfile = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Profile|Legacy", meta = (DeprecatedProperty, DeprecationMessage = "Migrated into Keys[0] on load."))
	TObjectPtr<UTexture2D> BottomProfile = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Profile|Legacy", meta = (DeprecatedProperty, DeprecationMessage = "Migrated into Keys[0] on load."))
	TObjectPtr<UTexture2D> AnvilProfile = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Profile|Legacy", meta = (DeprecatedProperty, DeprecationMessage = "Migrated into Keys[0] on load."))
	FStormProfileParams Params;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Profile|Timeline")
    TArray<FStormProfileKey> Keys;

    /** Whether playback starts with looping enabled when no runtime override is set. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Profile|Timeline")
    bool bLoopByDefault = false;

public:
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(
		class FDataValidationContext& Context) const override;
#endif

    /** Returns the number of authored poses in the timeline. */
    int32 GetKeyCount() const;

    /** Returns the key at Index, or nullptr when Index is outside the timeline. */
    const FStormProfileKey* GetKey(int32 Index) const;

    /** Returns the first key, which is the profile's initial playback pose. */
    const FStormProfileKey* GetPrimaryKey() const;

    /** Returns the timeline duration in seconds, based on the final key's time. */
    float GetDurationSeconds() const;

    /**
     * Resolves the keys surrounding TimeSeconds and returns an uneased,
     * normalized interpolation alpha.
     *
     * Exact key times resolve to that key on both sides. Time outside the
     * authored range is clamped to the first or last key.
     *
     * @param TimeSeconds Timeline position to sample.
     * @param OutFromIndex Receives the source key index.
     * @param OutToIndex Receives the destination key index.
     * @param OutAlpha Receives the normalized alpha between the two keys.
     * @return true when the timeline is structurally valid and a segment was found.
     */
    bool FindSegment(
		float TimeSeconds,
		int32& OutFromIndex,
		int32& OutToIndex,
		float& OutAlpha) const;

    /** Returns whether the key has all three baked profile surfaces required for playback. */
    bool IsKeyComplete(int32 Index) const;

    /**
     * Performs runtime-safe structural validation for sequence playback.
     *
     * Editor data validation performs additional ownership and format checks
     * on the embedded textures.
     */
    bool IsSequenceComplete() const;

private:
	void MigrateLegacyProfileToPrimaryKey();
};
