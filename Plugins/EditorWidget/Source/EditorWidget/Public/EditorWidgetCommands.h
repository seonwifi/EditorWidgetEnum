// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "EditorWidgetStyle.h"

class FEditorWidgetCommands : public TCommands<FEditorWidgetCommands>
{
public:

	FEditorWidgetCommands()// TEXT("EditorWidget") 아이콘 이름으로 사용된다.
		: TCommands<FEditorWidgetCommands>(TEXT("EditorWidget"), NSLOCTEXT("Contexts", "EditorWidget", "EditorWidget Plugin"), NAME_None, FEditorWidgetStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	//변수 이름이 아이콘 이름으로 사용된다.
	TSharedPtr< FUICommandInfo > PluginAction;
	TSharedPtr< FUICommandInfo > PluginActionLayout;
};
