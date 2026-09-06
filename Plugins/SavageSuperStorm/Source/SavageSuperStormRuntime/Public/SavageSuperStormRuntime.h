/**
 * @file SavageSuperStormRuntime.h
 * @brief Declares the runtime module and log category.
 */

#pragma once

#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogSavageSuperStormRuntime, Log, All);

class FSavageSuperStormRuntimeModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};