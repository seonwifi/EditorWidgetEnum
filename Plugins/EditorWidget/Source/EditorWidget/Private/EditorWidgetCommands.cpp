// Copyright Epic Games, Inc. All Rights Reserved.

#include "EditorWidgetCommands.h"

#define LOCTEXT_NAMESPACE "FEditorWidgetModule"

void FEditorWidgetCommands::RegisterCommands()
{
	UI_COMMAND(PluginAction, "EditorWidget", "Execute EditorWidget action", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(PluginActionLayout, "EditorWidgetLayout", "Execute EditorWidget Layout action", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE
