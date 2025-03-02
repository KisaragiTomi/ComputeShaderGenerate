// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CSEditorProcess : ModuleRules
{
	public CSEditorProcess(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core", 
				"GeometryFramework",
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"EditorFramework",
				"Engine",
				"Landscape",
				"RenderCore",
				"ComputeShaderGenerator", 
				"EditorScriptingUtilities",
				"Renderer",
				"Projects", 
				"GeometryScriptExtra",
				"GeometryScriptingCore",
				"DynamicMesh",
				"GeometryCore", 
				"RHI",
				"Core", 
				"InputCore",
				"SlateCore",
				"AssetRegistry",
				"Core",
				"CoreUObject",
				"EditorStyle",
				"LevelEditor",
				"UnrealEd",
				
				// ... add private dependencies that you statically link with here ...	
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
