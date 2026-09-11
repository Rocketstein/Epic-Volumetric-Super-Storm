// Copyright 2026 GoroGoro. All Rights Reserved.

/**
 * @file StormProfileTransitionTypes.h
 * @brief Declares the easing modes used between vertical profile keys.
 */

#pragma once

#include "CoreMinimal.h"
#include "StormProfileTransitionTypes.generated.h"

/** Easing applied to the normalized transition between adjacent profile keys. */
UENUM(BlueprintType)
enum class EStormProfileTransitionEasing : uint8
{
	/** Interpolates the profile surfaces at a constant rate. */
	Linear,
	/** Smoothly accelerates from the source key and decelerates into the target key. */
	SmoothStep,
};
