// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "EditorWidgetStyle.h"

class FEditorWidgetCommands : public TCommands<FEditorWidgetCommands>
{
public:

	FEditorWidgetCommands()// TEXT("EditorWidget") ������ �̸����� ���ȴ�.
		: TCommands<FEditorWidgetCommands>(TEXT("EditorWidget"), NSLOCTEXT("Contexts", "EditorWidget", "EditorWidget Plugin"), NAME_None, FEditorWidgetStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	//���� �̸��� ������ �̸����� ���ȴ�.
	TSharedPtr< FUICommandInfo > PluginAction;
	TSharedPtr< FUICommandInfo > PluginActionLayout;
};
