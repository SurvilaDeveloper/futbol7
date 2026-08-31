// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ThirdPersonCpp : ModuleRules
{
	public ThirdPersonCpp(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "HeadMountedDisplay", "AIModule", "NavigationSystem", "UMG", "Slate", "SlateCore", "AssetRegistry" });
		PrivateDependencyModuleNames.AddRange(new string[] { "Json", "JsonUtilities" });
	}
}
