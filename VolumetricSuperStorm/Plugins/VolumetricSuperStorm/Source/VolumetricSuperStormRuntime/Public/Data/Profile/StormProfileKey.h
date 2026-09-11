// Copyright 2026 GoroGoro. All Rights Reserved.

/**
 * @file StormProfileKey.h
 * @brief Declares one key of a vertical profile sequence.
 */

#pragma once

#include "CoreMinimal.h"
#include "Data/Profile/StormProfileParams.h"
#include "Data/Profile/StormProfileTransitionTypes.h"
#include "StormProfileKey.generated.h"

class UTexture2D;

/**
 * One authored pose on a vertical-profile timeline.
 *
 * The three baked surfaces are the renderable endpoints used during runtime
 * playback. OutgoingEasing controls the segment that begins at this key and
 * ends at the next key.
 */
USTRUCT(BlueprintType)
struct VOLUMETRICSUPERSTORMRUNTIME_API FStormProfileKey
{
	GENERATED_BODY()

	/** Stable identifier used to associate editor working surfaces with this key. */
	UPROPERTY()
	FGuid KeyId;

	/** Absolute position of this pose on the sequence timeline, in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Storm|Profile|Timeline",
		meta = (ClampMin = "0.0"))
	float TimeSeconds = 0.0f;

	/** Parametric settings captured when this pose was authored. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Storm|Profile|Timeline")
	FStormProfileParams Params;

	/** Easing applied from this key to the next key in the timeline. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Storm|Profile|Timeline")
	EStormProfileTransitionEasing OutgoingEasing =
		EStormProfileTransitionEasing::Linear;

	/** Baked bottom-density surface used as a transition endpoint. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Storm|Profile|Timeline")
	TObjectPtr<UTexture2D> BottomProfile = nullptr;

	/** Baked top-density surface used as a transition endpoint. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Storm|Profile|Timeline")
	TObjectPtr<UTexture2D> TopProfile = nullptr;

	/** Baked anvil-density surface used as a transition endpoint. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Storm|Profile|Timeline")
	TObjectPtr<UTexture2D> AnvilProfile = nullptr;
};
