/**
 * @file SavageSuperStormShaders.h
 * @brief Declares the shader module.
 */

#pragma once

#include "Modules/ModuleManager.h"

class FSavageSuperStormShadersModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};