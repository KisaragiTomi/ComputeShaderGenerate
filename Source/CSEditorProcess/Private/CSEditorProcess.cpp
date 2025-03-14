// Copyright Epic Games, Inc. All Rights Reserved.

#include "CSEditorProcess.h"

#include "Interfaces/IPluginManager.h"

#define LOCTEXT_NAMESPACE "FCSEditorProcessModule"

void FCSEditorProcessModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	// const FString ShaderDirectory = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("HLODGenerator"))->GetBaseDir(), TEXT("Shaders/Private"));
	// AddShaderSourceDirectoryMapping("/OoOShaders", ShaderDirectory);
}

void FCSEditorProcessModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FCSEditorProcessModule, CSEditorProcess)