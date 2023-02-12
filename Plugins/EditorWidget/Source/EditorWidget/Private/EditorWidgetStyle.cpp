// Copyright Epic Games, Inc. All Rights Reserved.

#include "EditorWidgetStyle.h"
#include "EditorWidget.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/SlateStyleRegistry.h"
#include "Slate/SlateGameResources.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyleMacros.h"

#define RootToContentDir Style->RootToContentDir

TSharedPtr<FSlateStyleSet> FEditorWidgetStyle::StyleInstance = nullptr;

void FEditorWidgetStyle::Initialize()
{
	if (!StyleInstance.IsValid())
	{
		StyleInstance = Create();
		FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
	}
}

void FEditorWidgetStyle::Shutdown()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
	ensure(StyleInstance.IsUnique());
	StyleInstance.Reset();
}

FName FEditorWidgetStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("EditorWidgetStyle"));
	return StyleSetName;
}


const FVector2D Icon16x16(16.0f, 16.0f);
const FVector2D Icon20x20(20.0f, 20.0f);

TSharedRef< FSlateStyleSet > FEditorWidgetStyle::Create()
{
	TSharedRef< FSlateStyleSet > Style = MakeShareable(new FSlateStyleSet("EditorWidgetStyle"));
	Style->SetContentRoot(IPluginManager::Get().FindPlugin("EditorWidget")->GetBaseDir() / TEXT("Resources"));

	Style->Set("EditorWidget.PluginAction", new IMAGE_BRUSH_SVG(TEXT("game-svgrepo-com"), Icon20x20));
	Style->Set("EditorWidget.PluginActionLayout", new IMAGE_BRUSH_SVG(TEXT("safety-svgrepo-com"), Icon20x20));

	return Style;
}

void FEditorWidgetStyle::ReloadTextures()
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
	}
}

const ISlateStyle& FEditorWidgetStyle::Get()
{
	return *StyleInstance;
}
