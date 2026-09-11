// Copyright 2026 GoroGoro. All Rights Reserved.

#pragma once

#include "Data/StormLightningTypes.h"
#include "Data/StormTypes.h"

namespace VolumetricSuperStorm::Defaults
{
	/** Native class defaults authored from SP_Default without retaining a preset reference. */
	struct FStormDefaultConfiguration
	{
		FStormShapeSettings ShapeSettings;
		FStormMotionSettings MotionSettings;
		FStormLightningSequenceSettings LightningSequence;
		float FormationDurationSeconds = 13.0f;
		float DissolutionDurationSeconds = 10.0f;
		float GroundStrikeProbability = 1.0f;
		float LightningRepeatDelaySeconds = 2.0f;
	};

	FStormDefaultConfiguration MakeStormDefaultConfiguration();
}
