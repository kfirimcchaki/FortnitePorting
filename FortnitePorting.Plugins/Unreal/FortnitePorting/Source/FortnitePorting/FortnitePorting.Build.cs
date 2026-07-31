// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class FortnitePorting : ModuleRules
{
	public FortnitePorting(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			[
				// ... add public include paths required here ...
			]
		);

		PrivateIncludePaths.AddRange(
			[
				// ... add other private include paths required here ...
			]
		);

		PublicDependencyModuleNames.AddRange(
			[
				"Core", "JsonUtilities", "Json", "PluginUtils", "UEFormat",
				"Projects", "UnrealEd", "EditorScriptingUtilities", "Sockets", "Networking",
				"InterchangeCore", "InterchangeEngine", "InterchangeImport", "InterchangeFactoryNodes", "InterchangePipelines",
				// UE5 extended import support — runtime modules
				"Niagara",
				"UMG",
				"AssetTools",
				"AssetRegistry",
				"LevelSequence",
				"MovieScene",
				"MovieSceneTracks",
				"PhysicsCore",
				"Engine",
				"CoreUObject",
				"Slate",
				"SlateCore",
				"InputCore",
				"EngineSettings",
				"AnimGraphRuntime",
				"RenderCore",
				"RHI",
				"GameplayTags",
				"PhysicsUtilities",
				"HTTP",
				"Landscape",
				"EngineAssetDefinitions"
			]
		);

		// Editor-only modules needed for asset factory calls. These are brought in
		// as PrivateDependencyModuleNames so that monolithic builds can exclude them.
		PrivateDependencyModuleNames.AddRange(
			[
				"Blutility",
				"ContentBrowser",
				"MaterialEditor",
				"Kismet",
				"KismetCompiler",
				"ToolMenus",
				"EditorStyle",
				"NiagaraEditor",
				"UMGEditor",
				"LevelSequenceEditor",
				"BlueprintGraph",
				"GraphEditor",
				"KismetWidgets",
				"EditorWidgets",
				"MainFrame",
				"WorkspaceMenuStructure",
				"CoreUObject"
			]
		);

		DynamicallyLoadedModuleNames.AddRange(
			[
				// ... add any modules that your module loads dynamically here ...
			]
		);
	}
}
