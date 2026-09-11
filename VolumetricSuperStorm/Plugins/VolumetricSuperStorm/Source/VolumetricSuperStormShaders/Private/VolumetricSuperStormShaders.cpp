// Copyright 2026 GoroGoro. All Rights Reserved.

/**
 * @file VolumetricSuperStormShaders.cpp
 * @brief Registers the plugin shader source directory.
 */

#include "VolumetricSuperStormShaders.h"
#include "ShaderCore.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "FVolumetricSuperStormShadersModule"

void FVolumetricSuperStormShadersModule::StartupModule()
{
	FString PluginShaderDir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("VolumetricSuperStorm"))->GetBaseDir(), TEXT("Shaders"));
	AddShaderSourceDirectoryMapping(TEXT("/VolumetricSuperStormShaders"), PluginShaderDir);
}

void FVolumetricSuperStormShadersModule::ShutdownModule()
{}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FVolumetricSuperStormShadersModule, VolumetricSuperStormShaders)