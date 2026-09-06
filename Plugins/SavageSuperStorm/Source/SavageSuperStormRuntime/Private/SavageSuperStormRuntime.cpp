/**
 * @file SavageSuperStormRuntime.cpp
 * @brief Starts the runtime module and registers runtime diagnostics.
 */

#include "SavageSuperStormRuntime.h"

DEFINE_LOG_CATEGORY(LogSavageSuperStormRuntime);

#define LOCTEXT_NAMESPACE "FSavageSuperStormRuntimeModule"

void FSavageSuperStormRuntimeModule::StartupModule()
{}

void FSavageSuperStormRuntimeModule::ShutdownModule()
{}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FSavageSuperStormRuntimeModule, SavageSuperStormRuntime)