// Copyright 2026 GoroGoro. All Rights Reserved.

/**
 * @file VolumetricSuperStormRuntime.h
 * @brief Declares the runtime module and log category.
 */

#pragma once

#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogVolumetricSuperStormRuntime, Log, All);

class FVolumetricSuperStormRuntimeModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};