// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class MeshSynthTarget : TargetRules
{
	public MeshSynthTarget( TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V5;

		bBuildAdditionalConsoleApp = true;

		ExtraModuleNames.AddRange( new string[] { "MeshSynth" } );
	}
}
