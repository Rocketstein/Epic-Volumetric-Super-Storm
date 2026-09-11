// Copyright 2026 GoroGoro. All Rights Reserved.

/**
 * @file VolumetricSuperStormShaders.h
 * @brief Declares the shader module.
 */

#pragma once

#include "Modules/ModuleManager.h"

class FVolumetricSuperStormShadersModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};