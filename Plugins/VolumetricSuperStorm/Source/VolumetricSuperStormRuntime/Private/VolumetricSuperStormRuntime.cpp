// Copyright 2026 GoroGoro. All Rights Reserved.

/**
 * @file VolumetricSuperStormRuntime.cpp
 * @brief Starts the runtime module and registers runtime diagnostics.
 */

#include "VolumetricSuperStormRuntime.h"

DEFINE_LOG_CATEGORY(LogVolumetricSuperStormRuntime);

#define LOCTEXT_NAMESPACE "FVolumetricSuperStormRuntimeModule"

void FVolumetricSuperStormRuntimeModule::StartupModule()
{}

void FVolumetricSuperStormRuntimeModule::ShutdownModule()
{}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FVolumetricSuperStormRuntimeModule, VolumetricSuperStormRuntime)