// Copyright 2026 GoroGoro. All Rights Reserved.

/**
 * @file StormRenderTargetResolution.h
 * @brief Defines the fixed resolutions shared by storm render-target producers and consumers.
 */

#pragma once

#include "CoreMinimal.h"

namespace VolumetricSuperStorm::RenderTargetResolution
{
	inline constexpr int32 Shape = 512;
	inline constexpr int32 Profile = 128;
	inline constexpr int32 FlowMap = 256;
}
