/**
 * @file SavageSuperStormShaders.cpp
 * @brief Registers the plugin shader source directory.
 */

#include "SavageSuperStormShaders.h"
#include "ShaderCore.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "FSavageSuperStormShadersModule"

void FSavageSuperStormShadersModule::StartupModule()
{
	FString PluginShaderDir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("SavageSuperStorm"))->GetBaseDir(), TEXT("Shaders"));
	AddShaderSourceDirectoryMapping(TEXT("/SavageSuperStormShaders"), PluginShaderDir);
}

void FSavageSuperStormShadersModule::ShutdownModule()
{}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FSavageSuperStormShadersModule, SavageSuperStormShaders)