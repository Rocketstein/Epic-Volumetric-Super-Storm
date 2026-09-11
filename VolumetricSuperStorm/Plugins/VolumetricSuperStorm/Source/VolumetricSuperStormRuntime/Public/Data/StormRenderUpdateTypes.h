// Copyright 2026 GoroGoro. All Rights Reserved.

/**
 * @file StormRenderUpdateTypes.h
 * @brief Defines backend-neutral storm render update requests.
 */

#pragma once

#include "CoreMinimal.h"

/** Selects how much cached storm render data must be rebuilt and published. */
enum class EStormRenderUpdateScope : uint8
{
	Full,
	Frame,
	Profile
};

/** Identifies render textures whose contents changed without changing identity. */
enum class EStormTextureDirtyFlags : uint16
{
	None = 0,
	Shape = 1u << 0,
	Shape2 = 1u << 1,
	ProfileBottom = 1u << 2,
	ProfileTop = 1u << 3,
	ProfileAnvil = 1u << 4,
	FlowLower = 1u << 5,
	FlowMiddle = 1u << 6,
	FlowUpper = 1u << 7
};

ENUM_CLASS_FLAGS(EStormTextureDirtyFlags);
