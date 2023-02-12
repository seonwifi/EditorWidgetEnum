// Copyright Epic Games, Inc. All Rights Reserved.

#include "EditorWidget.h"
#include "EditorWidgetStyle.h"
#include "EditorWidgetCommands.h"

#include "Misc/MessageDialog.h"
#include "ToolMenus.h"
#include "Widgets/Input/SHyperlink.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Input/SRotatorInputBox.h"
#include "Widgets/Input/NumericTypeInterface.h"
#include "Widgets/Input/NumericUnitTypeInterface.inl"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Input/SSegmentedControl.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Input/SSuggestionTextBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/STextComboBox.h"
#include "Widgets/Input/STextEntryPopup.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Input/SVectorInputBox.h"
#include "Widgets/Input/SVolumeControl.h"
#include "Widgets/Input/SRotatorInputBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Views/STreeView.h"
#include "Widgets/Layout/SGridPanel.h"
 
#include <Math/UnitConversion.h>
#include <ContentBrowserModule.h>
#include <Subsystems/EditorAssetSubsystem.h>
#include "WidgetBlueprint.h"
#include "EditorUtilityWidget.h"
#include "EditorUtilityWidgetBlueprint.h"
#include "EditorUtilitySubsystem.h"
#include <EditorScriptingHelpers.h>
#include <AssetRegistryModule.h>
#include <Experimental/Async/LazyEvent.h>
 
//#include "EditorUtilityWidget.h"

//Artificial Intelligence
//Replace Selected Actors with

static const FName EditorWidgetTabName("EditorWidget");
static const FName EditorLayoutTabName("EditorLayout");

#define LOCTEXT_NAMESPACE "FEditorWidgetModule"


TValueOrError<UObject*, FString> LoadAssetFromData(const FAssetData& AssetData)
{
	if (!AssetData.IsValid())
	{
		return MakeError("Asset Data is not valid.");
	}

	UObject* FoundObject = AssetData.GetAsset();
	if (!IsValid(FoundObject))
	{
		return MakeError(FString::Printf(TEXT("The asset '%s' exists but was not able to be loaded."), *AssetData.GetObjectPathString()));
	}
	else if (!FoundObject->IsAsset())
	{
		return MakeError(FString::Printf(TEXT("'%s' is not a valid asset."), *AssetData.GetObjectPathString()));
	}
	return MakeValue(FoundObject);
}

TValueOrError<FAssetData, FString> FindAssetDataFromAnyPath(const FString& AnyAssetPath)
{
	FString FailureReason;
	FString ObjectPath = EditorScriptingHelpers::ConvertAnyPathToSubObjectPath(AnyAssetPath, FailureReason);
	if (ObjectPath.IsEmpty())
	{
		return MakeError(FailureReason);
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	FAssetData AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(FSoftObjectPath(ObjectPath));
	if (!AssetData.IsValid())
	{
		ObjectPath = EditorScriptingHelpers::ConvertAnyPathToObjectPath(AnyAssetPath, FailureReason);
		if (ObjectPath.IsEmpty())
		{
			return MakeError(FailureReason);
		}

		AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(FSoftObjectPath(ObjectPath));
		if (!AssetData.IsValid())
		{
			return MakeError(FString::Printf(TEXT("The AssetData '%s' could not be found in the Asset Registry."), *ObjectPath));
		}
	}
		
	return MakeValue(AssetData);
}


TValueOrError<UObject*, FString> LoadAssetFromPath(const FString& AssetPath)
{
	TValueOrError<FAssetData, FString> AssetDataResult = FindAssetDataFromAnyPath(AssetPath);
	if (AssetDataResult.HasError())
	{
		return MakeError(AssetDataResult.StealError());
	}
	return LoadAssetFromData(AssetDataResult.GetValue());
}

UObject* LoadAsset(const FString& AssetPath)
{ 
	
	TValueOrError<UObject*, FString> LoadedAssetResult = LoadAssetFromPath(AssetPath);
	if (LoadedAssetResult.HasError())
	{
		//UE_LOG(LogEditorAssetSubsystem, Error, TEXT("LoadAsset failed: %s"), *LoadedAssetResult.GetError());
		return nullptr;
	}
	return LoadedAssetResult.GetValue();
}

bool CheckIfInEditorAndPIE()
{
	if (!IsInGameThread())
	{
		//UE_LOG(LogUtils, Error, TEXT("You are not on the main thread."));
		return false;
	}
	if (!GIsEditor)
	{
		//UE_LOG(LogUtils, Error, TEXT("You are not in the Editor."));
		return false;
	}
	if (GEditor->PlayWorld || GIsPlayInEditorWorld)
	{
		//UE_LOG(LogUtils, Error, TEXT("The Editor is currently in a play mode."));
		return false;
	}
	return true;
}

namespace UE::EditorAssetUtils
{
	struct ErrorTag{};
}

template<typename T>
using TError = TValueOrError<UE::EditorAssetUtils::ErrorTag, T>;

bool EnsureAssetsLoaded()
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	if (IAssetRegistry& AssetRegistry = AssetRegistryModule.Get(); AssetRegistry.IsLoadingAssets())
	{
		// Event is used like a condition variable here
		UE::FLazyEvent AssetRegistryLoadEvent{EEventMode::ManualReset};
		FDelegateHandle DelegateHandle = AssetRegistry.OnFilesLoaded().AddLambda([&AssetRegistryLoadEvent]
		{
			AssetRegistryLoadEvent.Trigger();
		});
		// open some message here
		AssetRegistryLoadEvent.Wait();

		AssetRegistry.OnFilesLoaded().Remove(DelegateHandle);
	}
	return true;
}
 
TError<FString> EnumerateAssetsInDirectory(const FString& AnyPathDirectoryPath, bool bRecursive, TArray<FAssetData>& OutResult, FString& OutDirectoryPath)
{
	OutResult.Reset();
	OutDirectoryPath.Reset();

	FString FailureReason;
	OutDirectoryPath = EditorScriptingHelpers::ConvertAnyPathToLongPackagePath(AnyPathDirectoryPath, FailureReason);
	if (OutDirectoryPath.IsEmpty())
	{
		return MakeError(FailureReason);
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	if (!AssetRegistryModule.Get().GetAssetsByPath(*OutDirectoryPath, OutResult, bRecursive))
	{
		return MakeError(FString::Printf(TEXT("Could not get assets from path '%s'"), *OutDirectoryPath));
	}
		
	return MakeValue();
}

TArray<FString> ListAssets(const FString& DirectoryPath, bool bRecursive = true, bool bIncludeFolder = false)
{
	//TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	TArray<FString> AssetPaths;
	if (!CheckIfInEditorAndPIE() || EnsureAssetsLoaded())
	{
		return AssetPaths;
	}

	TArray<FAssetData> AssetDatas;
	FString DirectoryPackageName;
	
	// if there is a valid asset data (i.e. path belongs to a file)
	if (TValueOrError<FAssetData, FString> AssetDataResult = FindAssetDataFromAnyPath(DirectoryPath); AssetDataResult.HasValue() && AssetDataResult.GetValue().IsValid())
	{
		AssetDatas.Add(AssetDataResult.GetValue());
	}
	// path may belong to a directory
	else if (TError<FString> Result = EnumerateAssetsInDirectory(DirectoryPath, bRecursive, AssetDatas, DirectoryPackageName); Result.HasError())
	{
		//UE_LOG(LogEditorAssetSubsystem, Error, TEXT("ListAssets failed: Could not enumerate assets in directory '%s'. %s"), *DirectoryPath, *Result.GetError());
		return AssetPaths;
	}

	if (AssetDatas.Num() > 0)
	{
		AssetDatas.Reserve(AssetDatas.Num());
		for (const FAssetData& AssetData : AssetDatas)
		{
			AssetPaths.Add(AssetData.GetObjectPathString());
		}
	}

	if (bIncludeFolder && !DirectoryPackageName.IsEmpty())
	{
		TArray<FString> SubPaths;
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(FName("AssetRegistry"));
		AssetRegistryModule.Get().GetSubPaths(DirectoryPackageName, SubPaths, bRecursive);

		for (const FString& SubPath : SubPaths)
		{
			if (SubPath.Contains(DirectoryPath) && SubPath != DirectoryPath)
			{
				AssetPaths.Add(SubPath + TEXT('/'));
			}
		}
	}

	AssetPaths.Sort();
	return AssetPaths;
}

void YourAssetExtenderFunc(FMenuBuilder& MenuBuilder, const TArray<FAssetData> SelectedAssets)
{
    //MenuBuilder.BeginSection("Your Asset Context", LOCTEXT("ASSET_CONTEXT", "Your Asset Context"));
    //{
    //    // Add Menu Entry Here
    //}
    //MenuBuilder.EndSection();
	MenuBuilder.AddMenuEntry(
	LOCTEXT("ButtonName", "ButtonName"),
	LOCTEXT("Button ToolTip", "Button ToolTip"),
	FSlateIcon(FEditorWidgetStyle::GetStyleSetName(), "Linter.Toolbar.Icon"),
	FUIAction(FExecuteAction::CreateLambda([SelectedAssets]()
	{
			for (auto& item : SelectedAssets)
			{
				UE_LOG(LogTemp, Log, TEXT("SelectedAssets ObjectPath: %s"), *item.ObjectPath.ToString());
			}
		// Do work here. 
		
	})),
	NAME_None,
	EUserInterfaceActionType::Button);
}

TSharedRef<FExtender> OnExtendContentBrowserAssetSelectionMenu(const TArray<FAssetData>& SelectedAssets)
{
	//TSharedRef<FExtender> Extender = MakeShared<FExtender>();

	//if (SelectedAssets.Num() == 1)
	//{
	//	if (IsClassHeaderViewSupported(SelectedAssets[0].GetClass()))
	//	{
	//		Extender->AddMenuExtension("GetAssetActions", EExtensionHook::After, nullptr, FMenuExtensionDelegate::CreateLambda(
	//			[SelectedAssets](FMenuBuilder& MenuBuilder) {
	//				MenuBuilder.AddMenuEntry(
	//					LOCTEXT("OpenHeaderView", "Preview Equivalent C++ Header"),
	//					LOCTEXT("OpenHeaderViewTooltip", "Provides a preview of what this class could look like in C++"),
	//					FSlateIcon(HeaderViewStyleSet->GetStyleSetName(), "Icons.HeaderView"),
	//					FUIAction(FExecuteAction::CreateStatic(&FBlueprintHeaderViewModule::OpenHeaderViewForAsset, SelectedAssets[0]))
	//					);
	//			})
	//		);
	//	}
	//}
	TSharedRef<FExtender> Extender = MakeShared<FExtender>();
	Extender->AddMenuExtension(
		"CommonAssetActions",
		EExtensionHook::After,
		nullptr,
		FMenuExtensionDelegate::CreateStatic(&YourAssetExtenderFunc, SelectedAssets)
	);
	return Extender;
}

void FEditorWidgetModule::ExecuteRun(FWeakBlueprintPointerArray InObjects)
{
	//UEditorAssetLibrary::
	//for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	//{
	//	if (UWidgetBlueprint* Blueprint = Cast<UWidgetBlueprint>(*ObjIt))
	//	{
	//		if (Blueprint->GeneratedClass->IsChildOf(UEditorUtilityWidget::StaticClass()))
	//		{
	//			UEditorUtilityWidgetBlueprint* EditorWidget = Cast<UEditorUtilityWidgetBlueprint>(Blueprint);
	//			if (EditorWidget)
	//			{
	//				UEditorUtilitySubsystem* EditorUtilitySubsystem = GEditor->GetEditorSubsystem<UEditorUtilitySubsystem>();
	//				EditorUtilitySubsystem->SpawnAndRegisterTab(EditorWidget);
	//			}
	//		}
	//	}
	//}
}

void FEditorWidgetModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	
	FEditorWidgetStyle::Initialize();
	FEditorWidgetStyle::ReloadTextures();

	FEditorWidgetCommands::Register();
	
	PluginCommands = MakeShareable(new FUICommandList);
 
	//메뉴 또는 에디터 툴바 아이콘 클릭 이벤트 생성
	PluginCommands->MapAction( FEditorWidgetCommands::Get().PluginAction, FExecuteAction::CreateRaw(this, &FEditorWidgetModule::PluginButtonClicked), FCanExecuteAction());
	PluginCommands->MapAction( FEditorWidgetCommands::Get().PluginActionLayout, FExecuteAction::CreateRaw(this, &FEditorWidgetModule::PluginLayoutButtonClicked), FCanExecuteAction());

	//메뉴 등록
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FEditorWidgetModule::RegisterMenus));
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FEditorWidgetModule::RegisterMenus2));
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FEditorWidgetModule::RegisterLayoutEnum));

	//패널창 등록
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(EditorWidgetTabName, FOnSpawnTab::CreateRaw(this, &FEditorWidgetModule::OnSpawnPluginTab))
		.SetDisplayName(LOCTEXT(" FEditorWidgetTitle", " Editor Widget"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(EditorLayoutTabName, FOnSpawnTab::CreateLambda([this](const FSpawnTabArgs& SpawnTabArgs) -> TSharedRef<SDockTab> 
	{ 
		return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			// Put your tab content here!
			CreatLayouts()
		];
	}))
	.SetDisplayName(LOCTEXT("LayoutsKey", "Editor Layouts"))
	.SetMenuType(ETabSpawnerMenuType::Hidden);


	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
	TArray<FContentBrowserMenuExtender_SelectedAssets>& CBMenuAssetExtenderDelegates = ContentBrowserModule.GetAllAssetViewContextMenuExtenders(); 
	CBMenuAssetExtenderDelegates.Add(FContentBrowserMenuExtender_SelectedAssets::CreateStatic(&OnExtendContentBrowserAssetSelectionMenu));
	
}

void FEditorWidgetModule::PostLoadCallback()
{

}

void FEditorWidgetModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	UToolMenus::UnRegisterStartupCallback(this);

	UToolMenus::UnregisterOwner(this);

	FEditorWidgetStyle::Shutdown();

	FEditorWidgetCommands::Unregister(); 
}

void FEditorWidgetModule::PluginButtonClicked()
{
	//패널창 화면에 보이기
	FGlobalTabmanager::Get()->TryInvokeTab(EditorWidgetTabName);

	//// Put your "OnButtonClicked" stuff here
	//FText DialogText = FText::Format(
	//						LOCTEXT("PluginButtonDialogText", "Add code to {0} in {1} to override this button's actions"),
	//						FText::FromString(TEXT("FEditorWidgetModule::PluginButtonClicked()")),
	//						FText::FromString(TEXT("EditorWidget.cpp"))
	//				   );
	//FMessageDialog::Open(EAppMsgType::Ok, DialogText);
}

void FEditorWidgetModule::PluginLayoutButtonClicked()
{
	//패널창 화면에 보이기
	FGlobalTabmanager::Get()->TryInvokeTab(EditorLayoutTabName);

	//// Put your "OnButtonClicked" stuff here
	//FText DialogText = FText::Format(
	//						LOCTEXT("PluginButtonDialogText", "Add code to {0} in {1} to override this button's actions"),
	//						FText::FromString(TEXT("FEditorWidgetModule::PluginButtonClicked()")),
	//						FText::FromString(TEXT("EditorWidget.cpp"))
	//				   );
	//FMessageDialog::Open(EAppMsgType::Ok, DialogText);
}

#define LOCTEXT2(InKey, InTextLiteral) FInternationalization::ForUseOnlyByLocMacroAndGraphNodeTextLiterals_CreateText(TEXT(InTextLiteral), TEXT(LOCTEXT_NAMESPACE), InKey)
void FEditorWidgetModule::RegisterLayoutEnum()
{
	// Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
 
	FToolMenuOwnerScoped OwnerScoped(this);
	{
		UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.PlayToolBar");
		{
			FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("PluginTools");
			{
				FToolMenuEntry& Entry = Section.AddEntry(FToolMenuEntry::InitToolBarButton(FEditorWidgetCommands::Get().PluginActionLayout));
 
				Entry.SetCommandList(PluginCommands);
			}
		}
	}
	
	{
		UToolMenus* ToolMenus = UToolMenus::Get();
 
		FName ParentName = "";
		const FName MenuName = "LevelEditor.MainMenu.Window"; 
		UToolMenu* MenuBar = ToolMenus->ExtendMenu("MainFrame.MainMenu");

		if (MenuBar)
		{
			MenuBar->AddSubMenu(
				"MainMenu",
				NAME_None,
				"CustomCategory",
				LOCTEXT("CustomCategory", "Custom Category"),
				LOCTEXT("CustomCategory_ToolTip", "Open the custom category")
			);
		}
		//"LevelEditor.OpenAddContent.Background", NAME_None, "LevelEditor.OpenAddContent.Overlay"
		UToolMenu* Menu = UToolMenus::Get()->RegisterMenu("MainFrame.MainMenu.CustomCategory");
		{
			//FToolMenuSection& Section = Menu->FindOrAddSection("Programming");
			FToolMenuSection& Section = Menu->AddSection("CustomCategorySection", LOCTEXT("CustomCategorySection", "Custom category section"));
			Section.AddMenuEntryWithCommandList(FEditorWidgetCommands::Get().PluginActionLayout, PluginCommands);

			//sub menu
			Section.AddSubMenu("WindowSubMenuContent", LOCTEXT("WindowSubMenuContent_Key", "Window SubMenu Content"), LOCTEXT("WindowSubMenuContent_Tooltip", "Window Sub MenuContent Tooltip"),
			FNewToolMenuDelegate::CreateLambda([this](UToolMenu* Menu) 
			{
				FToolMenuSection& Section = Menu->FindOrAddSection("WindowContent");
 
				for (int i = 0; i < 10; ++i)
				{
					FName MenuEntryName = *FString::Printf(TEXT("WindowMenuEntryKey%d"), i);
					FString tempStrkey1 = FString::Printf(TEXT("WindowOpenBridgeTab_Label%d"), i);
					FString tempStrkey2 = FString::Printf(TEXT("WindowOpenBridgeTab_Desc%d"), i);
		 
					Section.AddMenuEntry(MenuEntryName,
						LOCTEXT2(*tempStrkey1, "WindowEditorLayout"),
						LOCTEXT2(*tempStrkey2, "WindowEditorLayout."),
						FSlateIcon(FEditorWidgetStyle::GetStyleSetName(), "EditorWidget.PluginAction"),
						FUIAction(FExecuteAction::CreateLambda([this, i]() 
						{  
							UEditorAssetSubsystem* EditorAssetSubsystem = GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
							TArray<FString> DirectoryList = EditorAssetSubsystem->ListAssets(TEXT("/Game/EditorUtilityCollection/Parts/IndependentTools"));
							UObject* loadAsset =  EditorAssetSubsystem->LoadAsset(TEXT("/Game/EditorUtilityCollection/Parts/IndependentTools/EUW_GetPath"));
							//IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
							FString loadAssetPath;
							if (loadAsset)
							{
								UWidgetBlueprint* Blueprint = Cast<UWidgetBlueprint>(loadAsset);
								if (Blueprint)
								{
									if (Blueprint->GeneratedClass->IsChildOf(UEditorUtilityWidget::StaticClass()))
									{
										UEditorUtilityWidgetBlueprint* EditorWidget = Cast<UEditorUtilityWidgetBlueprint>(Blueprint);
										if (EditorWidget)
										{
											UEditorUtilitySubsystem* EditorUtilitySubsystem = GEditor->GetEditorSubsystem<UEditorUtilitySubsystem>();
											EditorUtilitySubsystem->SpawnAndRegisterTab(EditorWidget);
										}
									}
								}

								loadAssetPath = loadAsset->GetPathName();
							}
							FMessageDialog::Open(EAppMsgType::Ok, FText::Format(LOCTEXT("WindowSubMenuContentDialogText", "Window Sub MenuContent Dialog {0} AssetName {1}"), FText::FromString(FString::Printf(TEXT("%d"), i)), FText::FromString(*loadAssetPath) ));
						}), FCanExecuteAction())
					);
				}

			})
			,false, FSlateIcon(FAppStyle::Get().GetStyleSetName(), "LevelEditor.OpenContentBrowser"));
		}

		//퀵 메뉴에 추가 등록
		//"LevelEditor.LevelEditorToolBar.AddQuickMenu"
			// Adding Bridge entry to Quick Content menu.
		UToolMenu* AddMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.AddQuickMenu");
		FToolMenuSection& Section = AddMenu->FindOrAddSection("Content");
 
		Section.AddMenuEntry("OpenEditorLayout",
			LOCTEXT("OpenBridgeTab_Label2", "EditorLayout"),
			LOCTEXT("OpenBridgeTab_Desc2", "EditorLayout."),
			FSlateIcon(FEditorWidgetStyle::GetStyleSetName(), "EditorWidget.PluginAction"),
			FUIAction(FExecuteAction::CreateLambda([this]() {}), FCanExecuteAction())
		).InsertPosition = FToolMenuInsert("ImportContent", EToolMenuInsertType::After);


		/// <summary>
		/// 서브 메뉴 추가 카테고리
		/// </summary>
		//참조
			//Section.AddSubMenu("ContentBrowser", LOCTEXT("ContentBrowserMenu", "Content Browser"), LOCTEXT("ContentBrowserTooltip", "Actions related to the Content Browser"),
			//			FNewToolMenuDelegate::CreateRaw(this, &FContentBrowserSingleton::GetContentBrowserSubMenu, ContentBrowserGroup), false, 
			//			FSlateIcon(FAppStyle::Get().GetStyleSetName(), "LevelEditor.OpenContentBrowser"))
			//			.InsertPosition = FToolMenuInsert("OpenMarketplace", EToolMenuInsertType::After);
							  // FNewToolMenuDelegate::CreateLambda([this](UToolMenu* Menu, TSharedRef<FWorkspaceItem> ContentBrowserGroup) 
						   //{

						   //})
		
		Section.AddSubMenu("SubMenuContent", LOCTEXT("SubMenuContent_Key", "SubMenu Content"), LOCTEXT("SubMenuContent_Tooltip", "Sub MenuContent Tooltip"),
		FNewToolMenuDelegate::CreateLambda([this](UToolMenu* Menu) 
		{
			FToolMenuSection& Section = Menu->FindOrAddSection("Content2");
 
			for (int i = 0; i < 10; ++i)
			{
				FName MenuEntryName = *FString::Printf(TEXT("MenuEntryKey%d"), i);
				FString tempStrkey1 = FString::Printf(TEXT("OpenBridgeTab_Label%d"), i);
				FString tempStrkey2 = FString::Printf(TEXT("OpenBridgeTab_Desc%d"), i);
		 
				Section.AddMenuEntry(MenuEntryName,
					LOCTEXT2(*tempStrkey1, "EditorLayout"),
					LOCTEXT2(*tempStrkey2, "EditorLayout."),
					FSlateIcon(FEditorWidgetStyle::GetStyleSetName(), "EditorWidget.PluginAction"),
					FUIAction(FExecuteAction::CreateLambda([this, i]() 
					{ 
							FMessageDialog::Open(EAppMsgType::Ok, FText::Format(LOCTEXT("SubMenuContentDialogText", "SubMenuContentDialog {0}"), FText::FromString(FString::Printf(TEXT("%d"), i))));
					}), FCanExecuteAction())
				);
			}

		})
		,false, FSlateIcon(FAppStyle::Get().GetStyleSetName(), "LevelEditor.OpenContentBrowser")).InsertPosition = FToolMenuInsert("OpenMarketplace", EToolMenuInsertType::After);
	}
}

void FEditorWidgetModule::GetContentBrowserSubMenu(UToolMenu* Menu, TSharedRef<FWorkspaceItem> ContentBrowserGroup)
{

}

void FEditorWidgetModule::RegisterMenus()
{
	// Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
	FToolMenuOwnerScoped OwnerScoped(this);

	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
		{
			FToolMenuSection& Section = Menu->FindOrAddSection("WindowLayout");
			Section.AddMenuEntryWithCommandList(FEditorWidgetCommands::Get().PluginAction, PluginCommands);
		}
	}
}

void FEditorWidgetModule::RegisterMenus2()
{
	// Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
	FToolMenuOwnerScoped OwnerScoped(this);
 
	{
		UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.PlayToolBar");
		{
			FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("PluginTools");
			{
				FToolMenuEntry& Entry = Section.AddEntry(FToolMenuEntry::InitToolBarButton(FEditorWidgetCommands::Get().PluginAction));
				Entry.SetCommandList(PluginCommands);
			}
		}
	}
}


TSharedRef<SDockTab> FEditorWidgetModule::OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			// Put your tab content here!
			CreateWidgets()
		];
}

TSharedRef<SBox> FEditorWidgetModule::CreatLayouts()
{
	return SNew(SBox)
	.HAlign(HAlign_Fill)
	.VAlign(VAlign_Fill)
	[
		SNew(SScrollBox)
		+ SScrollBox::Slot()[CreateGridPanel()]
		+ SScrollBox::Slot()[CreateSTreeView()]
		+ SScrollBox::Slot()[CreateSListView()]
		+ SScrollBox::Slot()[SNew(SButton).DesiredSizeScale(FVector2D(0.5f, 3.0f)).HAlign(HAlign_Center).Text(LOCTEXT("Button", "Button"))]
		+ SScrollBox::Slot()[SNew(SButton).Text(LOCTEXT("Button", "Button"))]
		+ SScrollBox::Slot()[SNew(SButton).Text(LOCTEXT("Button", "Button"))]
		+ SScrollBox::Slot()[SNew(SButton).Text(LOCTEXT("Button", "Button"))]
		+ SScrollBox::Slot()[SNew(SButton).Text(LOCTEXT("Button", "Button"))]
		+ SScrollBox::Slot()[SNew(SButton).Text(LOCTEXT("Button", "Button"))]
		+ SScrollBox::Slot()[SNew(SButton).Text(LOCTEXT("Button", "Button"))]
		+ SScrollBox::Slot()[SNew(SButton).Text(LOCTEXT("Button", "Button"))]
		+ SScrollBox::Slot()[SNew(SButton).Text(LOCTEXT("Button", "Button"))]
		+ SScrollBox::Slot()[SNew(SButton).Text(LOCTEXT("Button", "Button"))]
		+ SScrollBox::Slot()[SNew(SButton).Text(LOCTEXT("Button", "Button"))]
		+ SScrollBox::Slot()[SNew(SButton).Text(LOCTEXT("Button", "Button"))]
		+ SScrollBox::Slot()[SNew(SButton).Text(LOCTEXT("Button", "Button"))]
		+ SScrollBox::Slot()[SNew(SButton).Text(LOCTEXT("Button", "Button"))]
		+ SScrollBox::Slot()[SNew(SButton).Text(LOCTEXT("Button", "Button"))]
		+ SScrollBox::Slot()[SNew(SButton).Text(LOCTEXT("Button", "Button"))]
		+ SScrollBox::Slot()[SNew(SButton).Text(LOCTEXT("Button", "Button"))]
		+ SScrollBox::Slot()[SNew(SButton).Text(LOCTEXT("Button", "Button"))]
		+ SScrollBox::Slot()[SNew(SButton).Text(LOCTEXT("Button", "Button"))]
		+ SScrollBox::Slot()[SNew(SButton).Text(LOCTEXT("Button", "Button"))]
		+ SScrollBox::Slot()[SNew(SButton).Text(LOCTEXT("Button", "Button"))]
		+ SScrollBox::Slot()[SNew(SButton).Text(LOCTEXT("Button", "Button"))]
		+ SScrollBox::Slot()[SNew(SButton).Text(LOCTEXT("Button", "Button"))]
		+ SScrollBox::Slot()[SNew(SButton).Text(LOCTEXT("Button", "Button"))]
		+ SScrollBox::Slot()[SNew(SButton).Text(LOCTEXT("Button", "Button"))]
		+ SScrollBox::Slot()[SNew(SButton).Text(LOCTEXT("Button", "Button"))]
		+ SScrollBox::Slot()[SNew(SButton).Text(LOCTEXT("Button", "Button"))]
		+ SScrollBox::Slot()[SNew(SButton).Text(LOCTEXT("Button", "Button"))]
		+ SScrollBox::Slot()[SNew(SButton).Text(LOCTEXT("Button", "Button"))]
		+ SScrollBox::Slot()[SNew(SButton).Text(LOCTEXT("Button", "Button"))]
		+ SScrollBox::Slot()[SNew(SButton).Text(LOCTEXT("Button", "Button"))]
		+ SScrollBox::Slot()[SNew(SButton).Text(LOCTEXT("Button", "Button"))]
		+ SScrollBox::Slot()[SNew(SButton).Text(LOCTEXT("Button", "Button"))]
		+ SScrollBox::Slot()[SNew(SButton).Text(LOCTEXT("Button", "Button"))]
		+ SScrollBox::Slot()[SNew(SButton).Text(LOCTEXT("Button", "Button"))]
		+ SScrollBox::Slot()[SNew(SButton).Text(LOCTEXT("Button", "Button"))]
		+ SScrollBox::Slot()[SNew(SButton).Text(LOCTEXT("Button", "Button"))]
		+ SScrollBox::Slot()[SNew(SButton).Text(LOCTEXT("Button", "Button"))]
		+ SScrollBox::Slot()[SNew(SButton).Text(LOCTEXT("Button", "Button"))]
		+ SScrollBox::Slot()[SNew(SButton).Text(LOCTEXT("Button", "Button"))]
		+ SScrollBox::Slot()[SNew(SButton).Text(LOCTEXT("Button", "Button"))]
		+ SScrollBox::Slot()[SNew(SButton).Text(LOCTEXT("Button", "Button"))]
		+ SScrollBox::Slot()[SNew(SButton).Text(LOCTEXT("Button", "Button"))]
		+ SScrollBox::Slot()[SNew(SButton).Text(LOCTEXT("Button", "Button"))]
		+ SScrollBox::Slot()[SNew(SButton).Text(LOCTEXT("Button", "Button"))]
		+ SScrollBox::Slot()[SNew(SButton).Text(LOCTEXT("Button", "Button"))]
		+ SScrollBox::Slot()[SNew(SButton).Text(LOCTEXT("Button", "Button"))]
		+ SScrollBox::Slot()[SNew(SButton).Text(LOCTEXT("Button", "Button"))]
		+ SScrollBox::Slot()[SNew(SButton).Text(LOCTEXT("Button", "Button"))]
		+ SScrollBox::Slot()[SNew(SButton).Text(LOCTEXT("Button", "Button"))]
		+ SScrollBox::Slot()[CreateWidgets()] 
		 
	];
}
 
TSharedRef<SWidget> FEditorWidgetModule::CreateSTreeView()
{
	TSharedPtr<FVariableMappingInfo> AAC = MakeShareable( new FVariableMappingInfo("AAC"));
	AAC->Children.Add(MakeShareable( new FVariableMappingInfo("AACA")));
	AAC->Children.Add(MakeShareable( new FVariableMappingInfo("AACB")));
	AAC->Children.Add(MakeShareable( new FVariableMappingInfo("AACC")));

	TSharedPtr<FVariableMappingInfo> AA = MakeShareable( new FVariableMappingInfo("AA"));
	AA->Children.Add(MakeShareable( new FVariableMappingInfo("AAA")));
	AA->Children.Add(MakeShareable( new FVariableMappingInfo("AAB")));
	AA->Children.Add( AAC);

	 TSharedPtr<FVariableMappingInfo> A = MakeShareable( new FVariableMappingInfo("A"));
	A->Children.Add(AA);
	A->Children.Add(MakeShareable( new FVariableMappingInfo("AB")));
	A->Children.Add(MakeShareable( new FVariableMappingInfo("AC")));

	VariableMappingList.Add(A);
	VariableMappingList.Add(MakeShareable( new FVariableMappingInfo("B")));
	VariableMappingList.Add(MakeShareable( new FVariableMappingInfo("C")));

	return SAssignNew(VariableMappingTreeView, SVariableMappingTreeView)
	.TreeItemsSource(&VariableMappingList)
 
	.OnGenerateRow_Lambda([this](TSharedPtr<FVariableMappingInfo> InInfo, const TSharedRef<STableViewBase>& OwnerTable) -> TSharedRef<ITableRow> 
	{
		//return SNew(SVariableMappingTreeRow, OwnerTable)
		return SNew(STableRow< TSharedPtr<FName> >, OwnerTable)
			 
			[
				SNew(STextBlock).Text(FText::FromName(InInfo->GetPathName()))

			];
	})
	.OnGetChildren_Lambda([this](TSharedPtr<FVariableMappingInfo> InInfo, TArray< TSharedPtr<FVariableMappingInfo> >& OutChildren) 
						  {
							  OutChildren = InInfo->Children;
						  })
	.ItemHeight(22.0f);
}

TSharedRef<SWidget> FEditorWidgetModule::CreateGridPanel()
{
	return SNew(SGridPanel)
					.FillColumn(0, 1.0f)
					.FillRow(0, 1.0f)
					+SGridPanel::Slot(0, 0)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("Grid00", "Grid00:"))
					]
					+SGridPanel::Slot(1, 0)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("Grid10", "Grid10:"))
					]
					+SGridPanel::Slot(0, 1)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("Grid01", "Grid01:"))
					];
}

TSharedRef<SWidget> FEditorWidgetModule::CreateSListView()
{
 
	AccessSpecifierStrings.Add(TSharedPtr<FString>(new FString(TEXT("SListView 1"))));
	AccessSpecifierStrings.Add(TSharedPtr<FString>(new FString(TEXT("SListView 2"))));
	AccessSpecifierStrings.Add(TSharedPtr<FString>(new FString(TEXT("SListView 3"))));
	AccessSpecifierStrings.Add(TSharedPtr<FString>(new FString(TEXT("SListView 4"))));
	AccessSpecifierStrings.Add(TSharedPtr<FString>(new FString(TEXT("SListView 5"))));
	AccessSpecifierStrings.Add(TSharedPtr<FString>(new FString(TEXT("SListView 6"))));
	AccessSpecifierStrings.Add(TSharedPtr<FString>(new FString(TEXT("SListView 7"))));
	AccessSpecifierStrings.Add(TSharedPtr<FString>(new FString(TEXT("SListView 8"))));

	return  SNew(SListView<TSharedPtr<FString> >)
                    .ListItemsSource( &AccessSpecifierStrings )
		.OnGenerateRow_Lambda([this](TSharedPtr<FString> SpecifierName, const TSharedRef<STableViewBase>& OwnerTable) ->TSharedRef<ITableRow>  
							  {
								  	return SNew(STableRow< TSharedPtr<FString> >, OwnerTable)
									.Content()
									[
										SNew( STextBlock ) 
											.Text(FText::FromString(*SpecifierName.Get()) )
									];
							  })
		.OnSelectionChanged_Lambda([](TSharedPtr<FString> SpecifierName, ESelectInfo::Type SelectInfo) 
								   { 
									   UE_LOG(LogCore, Log, TEXT( "OnSelectionChanged_Lambda: %s"),  **SpecifierName);
								   });
                    //.OnGenerateRow(this, &FControlRigGraphDetails::HandleGenerateRowAccessSpecifier)
                    //.OnSelectionChanged(this, &FControlRigGraphDetails::OnAccessSpecifierSelected)
}
 

//SMenuAnchor
TSharedRef<SBox> FEditorWidgetModule::CreateWidgets()
{
		FText WidgetText = FText::Format(
		LOCTEXT("WindowWidgetText", "Add code to {0} in {1} to override this window's contents"),
		FText::FromString(TEXT("FEditorStandaloneWindowModule::OnSpawnPluginTab")),
		FText::FromString(TEXT("EditorStandaloneWindow.cpp"))
		);
		
	//SComboButton 은 초기 선택이 없다. 메뉴 리스트도 버튼 클릭 할때 만들어 진다. 초기 선택은 ButtonContent 에 초기 선택 글자를 세팅 해주면 된다.
	this->SComboButtonText =  FText::Format(LOCTEXT("DefaultSComboButtonText", "SComboButtonText {0}"), FText::AsNumber(0));

	TSharedPtr<SComboButton> SComboButtonExample  = 	SNew(SComboButton)
					//.OnGetMenuContent(FOnGetContent::CreateSP(this,  &FEditorWidgetModule::OnGetFilterTestContent))
					//.OnGetMenuContent(this, &FEditorWidgetModule::OnGetFilterTestContent)
					.OnGetMenuContent(FOnGetContent::CreateLambda([this]()
					{
						//FMenuBuilder MenuBuilder(true, TSharedPtr<FUICommandList>(), NULL);

						//MenuBuilder.BeginSection(FName("FilterPicker"));
						//{
						//	//MenuBuilder.AddWidget(SessionFilterService->GetFilterPickerWidget(FOnFilterClassPicked::CreateLambda(OnActorFilterClassPicked)), FText::GetEmpty(), true, false);
						//}
						//MenuBuilder.EndSection();

						return this->OnGetFilterTestContent();//MenuBuilder.MakeWidget();
					}))
					.ContentPadding(FMargin( 2.0f, 2.0f ))
					.ButtonContent()
					[
						SNew(STextBlock) 
						.Text_Lambda([this]()
									 {
										 //초기 글자가 초기 선택이다.
										 return this->SComboButtonText;
									 })
						//.Text(this, &FEditorWidgetModule::GetCurrentFilterTestDesc)
						//Text( LOCTEXT("GetCurrentFilterTestDescKey", "GetCurrentFilterTestDesc"))
					];

	TSharedPtr<INumericTypeInterface<FRotator::FReal>> TypeInterface = MakeShareable( new TNumericUnitTypeInterface<FRotator::FReal>(EUnit::Degrees) );
	TSharedPtr<INumericTypeInterface<FRotator::FReal>> TypeInterfaceVector = MakeShareable( new TNumericUnitTypeInterface<FRotator::FReal>(EUnit::Meters) );
 

	SSuggestionTextBox_TextTooltip = LOCTEXT("SSuggestionTextBox_TextTooltip", "SSuggestionTextBox_TextTooltip");
	SSuggestionTextBox_TextHint = LOCTEXT("SSuggestionTextBox_TextHint", "SSuggestionTextBox_TextHint");

	GroupNodeComboOptions.Add( MakeShareable(new FString(TEXT("Item1"))));
	GroupNodeComboOptions.Add( MakeShareable(new FString(TEXT("Item2"))));
	GroupNodeComboOptions.Add( MakeShareable(new FString(TEXT("Item3"))));
	TSharedPtr<FString> ItemToSelect = GroupNodeComboOptions.Last();

	CachedLocation.Set(FVector(1,2,3));

	SRotatorInputBox_Value.Add(FRotator(0,10,20));

	Position.Add(FVector(1,2,3));


	return SNew(SBox)
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Top)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("STextBlockWindowWidgetTextTitle", "STextBlock:"))

				]
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Top)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("STextBlockWindowWidgetTextDesc", " Text UI"))
				]
			] 
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Top)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("SBorderTitle", "SBorder:"))

				]
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Top)
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("Concert.MultiUser"))
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Fill)
					.Visibility(EVisibility::Visible)
					//.Padding(FMargin(0.0f, 0.0f, 0.0f, 0.0f))
				]
			] 
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Top)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("TitleSButton", "SButton:"))

				]
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Top)
				[
					SNew(SButton) 
					.Text(LOCTEXT("SButtonText", "SButton"))
					.OnClicked_Lambda([]()
						{
							//다이얼로그 박스 띄움
							FText DialogText = FText::Format(	LOCTEXT("PluginButtonDialogText", "Add code to {0} in {1} to override this button's actions"),
															FText::FromString(TEXT("FEditorWidgetModule::PluginButtonClicked()")),
															FText::FromString(TEXT("EditorWidget.cpp")));
							FMessageDialog::Open(EAppMsgType::Ok, DialogText);
 
							return FReply::Handled();
						})
				]
			] 
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Top)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("TitleSComboButton", "SComboButton:"))

				]
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Top)
				[
					SComboButtonExample.ToSharedRef()
				]
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Top)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("TitleSCheckBox", "SCheckBox:"))

				]
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Top)
				[
					SNew(SCheckBox)
					.IsChecked_Lambda([this]() -> ECheckBoxState
									  {
										  return this->CurrentCheckBoxState;
									  }) 
					.OnCheckStateChanged_Lambda([this](ECheckBoxState InCheckBoxState) 
												{
													this->CurrentCheckBoxState = InCheckBoxState;
												})
					[
						SNew(STextBlock)
						.Text(LOCTEXT("ProfilerDetailsOverlay2","Show details for current query"))
					]
				]
			]  
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Top)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("TitleSEditableText", "SEditableText:"))

				]
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Top)
				[
 					SNew( SEditableText )
					.IsReadOnly( false )
					.Text_Lambda([this]() -> FText 
								 {
									 return FText::FromString( "test SEditableText");
								 }) 
					.ToolTipText_Lambda([this]()
										{
											return FText::FromString( "test SEditableText Tooltip");
										})
				]
			]

			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Top)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("TitleSEditableTextBox", "SEditableTextBox:"))

				]
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Top)
				[
  					SNew(SEditableTextBox)
					//.Font(IDetailLayoutBuilder::GetDetailFont())
					//.Text(this, &FControlRigGraphDetails::GetNodeCategory)
					//.OnTextCommitted(this, &FControlRigGraphDetails::SetNodeCategory)
					.Text_Lambda([this]() -> FText 
					{
						return this->SEditableTextBoxText;// FText::FromString( "test SEditableText");
					}) 
					.OnTextCommitted_Lambda([this](const FText& InNewText, ETextCommit::Type InCommitType)
					{
						FString strInNewText = InNewText.ToString();
						UE_LOG(LogCore, Log, TEXT("OnTextCommitted_Lambda: %s"), * strInNewText);

						this->SEditableTextBoxText = InNewText;
					})
					
					.OnVerifyTextChanged_Lambda([&](const FText& InNewText, FText& OutErrorMessage) -> bool
					{
						FString strInNewText = InNewText.ToString();
						UE_LOG(LogCore, Log, TEXT("OnVerifyTextChanged_Lambda: %s"), * strInNewText);
						 
						return true;
					})
				]
			]
			
			///////////////////////////////////////////////////////////////
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Top)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("TitleSHyperlink", "SHyperlink:"))

				]
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Top)
				[
					SNew(SHyperlink)
					.Visibility_Lambda([this]() { return EVisibility::Visible; })
					.Text_Lambda([this]() ->FText 
					{
						return FText::FromString("SHyperlink");
					})
					.ToolTipText_Lambda([this]() ->FText 
					{
						return FText::FromString("SHyperlink Tooltip");
					})
						 
					.OnNavigate_Lambda([this]() 
					{
						FPlatformProcess::ExploreFolder(TEXT("c:/"));
					}) 
					// 
					//.Text(this, &SActiveSessionDetailsRow::GetLevel)
					//.ToolTipText(this, &SActiveSessionDetailsRow::GetOtherLevelTooltip)
					//.OnNavigate(this, &SActiveSessionDetailsRow::OnOtherLevelClicked)
 
				]
			]

			//SMultiLineEditableTextBox ----------------------------------------------------------------
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Top)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Title:SMultiLineEditableTextBox", "SMultiLineEditableTextBox:"))

				]
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Top)
				[
					SNew(SMultiLineEditableTextBox)
					//SAssignNew(RichEditableTextBox, SMultiLineEditableTextBox)
					//.Font(FTestStyle::Get().GetWidgetStyle<FTextBlockStyle>("RichText.Editor.Text").Font)
					//.Text(this, &SRichTextEditTest::GetRichEditableText)
					//.OnTextChanged(this, &SRichTextEditTest::HandleRichEditableTextChanged)
					//.OnTextCommitted(this, &SRichTextEditTest::HandleRichEditableTextCommitted)
					//.OnCursorMoved(this, &SRichTextEditTest::HandleRichEditableTextCursorMoved)
					//.Marshaller(RichTextMarshaller)
					//.ClearTextSelectionOnFocusLoss(false)
					//.AutoWrapText(true)
					//.Margin(4)
					//.LineHeightPercentage(1.1f)
					.Text_Lambda([this]() -> FText 
					{
						return this->SMultiLineEditableTextBoxText;// FText::FromString( "test SEditableText");
					}) 
					.OnTextCommitted_Lambda([this](const FText& InNewText, ETextCommit::Type InCommitType)
					{
						FString strInNewText = InNewText.ToString();
						UE_LOG(LogCore, Log, TEXT("OnTextCommitted_Lambda: %s"), * strInNewText);

						this->SMultiLineEditableTextBoxText = InNewText;
					})
					.OnTextChanged_Lambda([this](const FText& InText) 
					{
						FString strInNewText = InText.ToString();
						UE_LOG(LogCore, Log, TEXT("OnTextChanged_Lambda: %s"), * strInNewText);

						this->SMultiLineEditableTextBoxText = InText;
					})
					.OnVerifyTextChanged_Lambda([&](const FText& InNewText, FText& OutErrorMessage) -> bool
					{
						FString strInNewText = InNewText.ToString();
						UE_LOG(LogCore, Log, TEXT("OnVerifyTextChanged_Lambda: %s"), * strInNewText);
						 
						return true;
					})
				]
			]
			
			//SNumericEntryBox ----------------------------------------------------------------
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Top)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Title:SNumericEntryBox", "SNumericEntryBox:"))

				]
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Top)
				[
					SNew(SNumericEntryBox<float>)
					//.Font(StandardFont)
					.AllowSpin(true)
					.MinValue(0.0f)
					.MaxValue(65536.0f)
					.MaxSliderValue(4096.0f)
					.MinDesiredValueWidth(50.0f)
					.SliderExponent(3.0f)
					.Value_Lambda([this]
					{
						return this->SNumericEntryBoxValue;
					})
					.OnValueChanged_Lambda([this](float Value)
					{
						this->SNumericEntryBoxValue = Value;
					})
				]
			]

			//SNumericRotatorInputBox ----------------------------------------------------------------
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Top)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Title:SNumericRotatorInputBox", "SNumericRotatorInputBox:"))

				]
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Top)
				[
					SNew( SNumericRotatorInputBox<FRotator::FReal> )
					//.AllowSpin( SelectedObjects.Num() == 1 ) 
					//.Roll( this, &FComponentTransformDetails::GetRotationX )
					//.Pitch( this, &FComponentTransformDetails::GetRotationY )
					//.Yaw( this, &FComponentTransformDetails::GetRotationZ )
					//.bColorAxisLabels( true )
					//.IsEnabled( this, &FComponentTransformDetails::GetIsEnabled )
					//.OnBeginSliderMovement( this, &FComponentTransformDetails::OnBeginRotationSlider )
					//.OnEndSliderMovement( this, &FComponentTransformDetails::OnEndRotationSlider )
					//.OnRollChanged( this, &FComponentTransformDetails::OnSetTransformAxis, ETextCommit::Default, ETransformField::Rotation, EAxisList::X, false )
					//.OnPitchChanged( this, &FComponentTransformDetails::OnSetTransformAxis, ETextCommit::Default, ETransformField::Rotation, EAxisList::Y, false )
					//.OnYawChanged( this, &FComponentTransformDetails::OnSetTransformAxis, ETextCommit::Default, ETransformField::Rotation, EAxisList::Z, false )
					//.OnRollCommitted( this, &FComponentTransformDetails::OnSetTransformAxis, ETransformField::Rotation, EAxisList::X, true )
					//.OnPitchCommitted( this, &FComponentTransformDetails::OnSetTransformAxis, ETransformField::Rotation, EAxisList::Y, true )
					//.OnYawCommitted( this, &FComponentTransformDetails::OnSetTransformAxis, ETransformField::Rotation, EAxisList::Z, true )
					//.TypeInterface( TypeInterface )
					//.Font( FontInfo )
					.AllowSpin( true )
					.Roll_Lambda([this]() -> double
					{
						return this->SNumericRotatorInputBoxValue.Roll;
					})
					.Pitch_Lambda([this]() -> double
					{
						return this->SNumericRotatorInputBoxValue.Pitch;
					})
					.Yaw_Lambda([this]() -> double
					{
						return this->SNumericRotatorInputBoxValue.Yaw;
					})
					.bColorAxisLabels( true )
					
					.OnBeginSliderMovement_Lambda([this]() 
					{
						UE_LOG(LogCore, Log, TEXT("OnBeginSliderMovement_Lambda:"));
					})
					.OnEndSliderMovement_Lambda([this](FRotator::FReal NewValue) 
					{
						UE_LOG(LogCore, Log, TEXT("OnEndSliderMovement_Lambda: %lf"), NewValue);
					})
					.OnRollChanged_Lambda([this](FVector::FReal NewValue, ETextCommit::Type CommitInfo, ETransformField2::Type TransformField, EAxisList::Type Axis, bool bCommitted) 
					{
						UE_LOG(LogCore, Log, TEXT("OnRollChanged_Lambda: %lf"), NewValue);
						this->SNumericRotatorInputBoxValue.Roll = NewValue;
					}, ETextCommit::Default, ETransformField2::Rotation, EAxisList::X, false)

					.OnPitchChanged_Lambda([this](FVector::FReal NewValue, ETextCommit::Type CommitInfo, ETransformField2::Type TransformField, EAxisList::Type Axis, bool bCommitted) 
					{
						UE_LOG(LogCore, Log, TEXT("OnPitchChanged_Lambda: %lf"), NewValue);
						this->SNumericRotatorInputBoxValue.Pitch = NewValue;
					}, ETextCommit::Default, ETransformField2::Rotation, EAxisList::Y, false)
					
					.OnYawChanged_Lambda([this](FVector::FReal NewValue, ETextCommit::Type CommitInfo, ETransformField2::Type TransformField, EAxisList::Type Axis, bool bCommitted) 
					{
						UE_LOG(LogCore, Log, TEXT("OnYawChanged_Lambda: %lf"), NewValue);
						this->SNumericRotatorInputBoxValue.Yaw = NewValue;
					}, ETextCommit::Default, ETransformField2::Rotation, EAxisList::Z, false)
 
					.OnRollCommitted_Lambda([this](FVector::FReal NewValue, ETextCommit::Type CommitInfo, ETransformField2::Type TransformField) 
					{
						UE_LOG(LogCore, Log, TEXT("OnRollCommitted_Lambda: %lf"), NewValue);
						this->SNumericRotatorInputBoxValue.Roll = NewValue;
					}, ETransformField2::Rotation)

					.OnPitchCommitted_Lambda([this](FVector::FReal NewValue, ETextCommit::Type CommitInfo, ETransformField2::Type TransformField) 
					{
						UE_LOG(LogCore, Log, TEXT("OnPitchCommitted_Lambda: %lf"), NewValue);
						this->SNumericRotatorInputBoxValue.Pitch = NewValue;
					}, ETransformField2::Rotation)

					.OnYawCommitted_Lambda([this](FVector::FReal NewValue, ETextCommit::Type CommitInfo, ETransformField2::Type TransformField) 
					{
						UE_LOG(LogCore, Log, TEXT("OnYawCommitted_Lambda: %lf"), NewValue);
						this->SNumericRotatorInputBoxValue.Yaw = NewValue;
					}, ETransformField2::Rotation)
					.TypeInterface( TypeInterface )
				]
			]
			
			//SSearchBox ----------------------------------------------------------------
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Top)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Title:SSearchBox", "SSearchBox:"))

				]
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Top)
				[ 
					SNew(SSearchBox)
					//.SelectAllTextWhenFocused(true)
					//.OnTextChanged( this, &SIKRigRetargetChainList::OnFilterTextChanged )
					//.HintText( LOCTEXT( "SearchBoxHint", "Filter Chain List...") )
					.SelectAllTextWhenFocused(true)
					.OnTextChanged_Lambda([this](const FText& InText) 
					{
						FString strInNewText = InText.ToString();
						UE_LOG(LogCore, Log, TEXT("OnTextChanged_Lambda: %s"), *strInNewText);
						this->SSearchBoxFilterText = InText;
					})
					.OnTextCommitted_Lambda([this](const FText& InText, ETextCommit::Type InCommitType) 
					{
						FString strInNewText = InText.ToString();
						UE_LOG(LogCore, Log, TEXT("OnTextCommitted_Lambda: %s"), *strInNewText);
						this->SSearchBoxFilterText = InText;
					})
					.IsSearching_Lambda([this]()  -> bool 
					{
						return false;
					})
					.HintText( LOCTEXT( "SearchBoxHint", "Filter Chain List...") )
				]
			]

			//SSegmentedControl ----------------------------------------------------------------
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Top)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Title:SSegmentedControl<bool>", "SSegmentedControl<bool>:"))

				]
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Top)
				[ 
					SNew(SSegmentedControl<bool>)
					.Value_Lambda([this]()
					{
						//UE_LOG(LogCore, Log, TEXT("Value_Lambda: %s"), SSegmentedControl_bInIsDestinationDisplayView == true? TEXT("True"):TEXT("False"));
						return SSegmentedControl_bInIsDestinationDisplayView;
					})
					.OnValueChanged_Lambda([this](bool bInIsDestinationDisplayView) 
					{
						SSegmentedControl_bInIsDestinationDisplayView = bInIsDestinationDisplayView;
						UE_LOG(LogCore, Log, TEXT("OnValueChanged_Lambda: %s"), SSegmentedControl_bInIsDestinationDisplayView == true? TEXT("True"):TEXT("False"));
					})
 
					+ SSegmentedControl<bool>::Slot(false)
					.Text(LOCTEXT("ColorSpace", "Color Space"))
					.ToolTip(LOCTEXT("ColorSpace_ToolTip",
						"Select this if you want to use a color space destination."))

					+ SSegmentedControl<bool>::Slot(true)
					.Text(LOCTEXT("DisplayView", "Display-View"))
					.ToolTip(LOCTEXT("DisplayView_ToolTip",
						"Select this if you want to use a display-view destination."))
				]
			]
			//SSlider ----------------------------------------------------------------
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Top)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Title:SSlider", "SSlider:"))

				]
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Top)
				[ 
					SNew(SSlider)
					.Value_Lambda([this]() -> float 
					{
						//UE_LOG(LogCore, Log, TEXT("Value_Lambda"));
						return this->SSlider_Value;
					})
					.OnValueChanged_Lambda([this](float NewValue, bool bFromSlider) 
					{
						UE_LOG(LogCore, Log, TEXT("OnValueChanged_Lambda: %lf"), NewValue);
						this->SSlider_Value = NewValue;
					}, true) 
					.SliderBarColor(FLinearColor(0.48f, 0.48f, 0.48f))
					//.Style(FComposureEditorStyle::Get(), "ComposureTree.AlphaScrubber")
					.MouseUsesStep(true)
					.StepSize(0.01)
					.OnMouseCaptureEnd_Lambda([]() 
					{
						UE_LOG(LogCore, Log, TEXT("OnMouseCaptureEnd_Lambda"));
					})
				]
			]
			//SSpinBox ----------------------------------------------------------------
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Top)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Title:SSpinBox", "SSpinBox:"))

				]
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Top)
				[ 
					SNew(SSpinBox<double>)
					.Value_Lambda([this](){ return this->SSpinBox_Value; })
					.ToolTipText(LOCTEXT("SSpinBox_Value", "SSpinBox_Value speed"))
					.OnValueCommitted_Lambda([this](double InValue, ETextCommit::Type InCommitType){ this->SSpinBox_Value = InValue; })
					.MinValue(0.001f)
					.MaxValue(100.0f)
					//.Style(&FGameplayInsightsStyle::Get().GetWidgetStyle<FSpinBoxStyle>("TransportControls.HyperlinkSpinBox"))
					.ClearKeyboardFocusOnCommit(true)
					.Delta(0.01f)
					.LinearDeltaSensitivity(25)
					.TypeInterface(MakeShared<TNumericUnitTypeInterface<double>>(EUnit::Multiplier))
				]
			]
			//SSuggestionTextBox ----------------------------------------------------------------
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Top)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Title:SSuggestionTextBox", "SSuggestionTextBox:"))

				]
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Top)
				[ 
					SNew(SSuggestionTextBox)
					.MinDesiredWidth(50.0f)
					.ForegroundColor(FSlateColor::UseForeground())
					.OnTextCommitted_Lambda([this](const FText& InNewText, ETextCommit::Type InTextCommit) 
					{
						FString strInNewText = InNewText.ToString();
						UE_LOG(LogCore, Log, TEXT("OnTextCommitted_Lambda: %s"), *strInNewText);
						this->SSuggestionTextBox_Text = InNewText;
					})
					.OnTextChanged_Lambda([this](const FText& InNewText) 
					{
						FString strInNewText = InNewText.ToString();
						UE_LOG(LogCore, Log, TEXT("OnTextChanged_Lambda: %s"), *strInNewText);
						this->SSuggestionTextBox_Text = InNewText;
					})
					.Text_Lambda([this]() ->FText 
					{
						return this->SSuggestionTextBox_Text;
					})
					.ToolTipText_Lambda([this]() -> FText
					{
						return this->SSuggestionTextBox_TextTooltip;
					})
					.HintText_Lambda([this]() ->FText 
					{
						return this->SSuggestionTextBox_TextHint;
					})
					.OnShowingSuggestions_Lambda([this](const FString& Text, TArray<FString>& Suggestions) 
					{
						UE_LOG(LogCore, Log, TEXT("OnShowingSuggestions_Lambda: %s"), *Text);
						for (auto& item : Suggestions)
						{
							UE_LOG(LogCore, Log, TEXT("OnShowingSuggestions_Lambda: %s"), *item);
						}
					})
					.OnShowingHistory_Lambda([this](TArray<FString>& Suggestions) 
					{
						for (auto& item : Suggestions)
						{
							UE_LOG(LogCore, Log, TEXT("OnShowingHistory_Lambda: %s"), *item);
						}
					})
				]
			]
			//STextComboBox ----------------------------------------------------------------
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Top)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Title:STextComboBox", "STextComboBox:"))

				]
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Top)
				[ 
					SNew(STextComboBox)
					.OptionsSource(&GroupNodeComboOptions)
					.InitiallySelectedItem(ItemToSelect)
					.OnSelectionChanged_Lambda([this](TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo) 
					{
						FText DialogText = FText::Format(LOCTEXT("OnSelectionChanged_Lambda", "OnSelectionChanged_Lambda Select Cahnged: {0}"), FText::FromString(*Selection));
						FMessageDialog::Open(EAppMsgType::Ok, DialogText);
					}) 
				]
			]
			//STextEntryPopup ----------------------------------------------------------------
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Top)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Title:STextEntryPopup", "STextEntryPopup:"))

				]
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Top)
				[ 
					SNew(STextEntryPopup)
					.Label(LOCTEXT("AppendAnim_AskNumFrames22", "Number of Frames to Append"))
					.OnTextCommitted_Lambda([](const FText& CommittedText, ETextCommit::Type CommitType) 
					{
						FString strInNewText = CommittedText.ToString();
						UE_LOG(LogCore, Log, TEXT("OnTextCommitted_Lambda %s"), *strInNewText);
					})
					//.OnTextCommitted(this, &SAnimTimeline::OnSequenceAppendedCalled, bBegin);
				]
			]
			//SNumericVectorInputBox ----------------------------------------------------------------
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Top)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Title:SNumericVectorInputBox", "SNumericVectorInputBox:"))

				]
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Top)
				[ 
					SNew(SNumericVectorInputBox<FVector::FReal>)
					.X_Lambda([this]() -> TOptional<FVector::FReal> { return this->CachedLocation.X;})
					.Y_Lambda([this]() -> TOptional<FVector::FReal> { return this->CachedLocation.Y;})
					.Z_Lambda([this]() -> TOptional<FVector::FReal> { return this->CachedLocation.Z;})
					.bColorAxisLabels(true)
					.IsEnabled_Lambda([this]() -> bool {		return true;})
					.OnXChanged_Lambda([this](FVector::FReal NewValue, ETextCommit::Type CommitInfo, ETransformField2::Type TransformField, EAxisList::Type Axis, bool bCommitted) 
					{
						this->CachedLocation.X = NewValue;
						UE_LOG(LogCore, Log, TEXT("OnXChanged_Lambda: %lf"), NewValue);
					}, ETextCommit::Default, ETransformField2::Location, EAxisList::X, false)
					.OnYChanged_Lambda([this](FVector::FReal NewValue, ETextCommit::Type CommitInfo, ETransformField2::Type TransformField, EAxisList::Type Axis, bool bCommitted) 
					{
						this->CachedLocation.Y = NewValue;
						UE_LOG(LogCore, Log, TEXT("OnYChanged_Lambda: %lf"), NewValue);
					}, ETextCommit::Default, ETransformField2::Location, EAxisList::Y, false)
					.OnZChanged_Lambda([this](FVector::FReal NewValue, ETextCommit::Type CommitInfo, ETransformField2::Type TransformField, EAxisList::Type Axis, bool bCommitted) 
					{
						this->CachedLocation.Z = NewValue;
						UE_LOG(LogCore, Log, TEXT("OnZChanged_Lambda: %lf"), NewValue);
					}, ETextCommit::Default, ETransformField2::Location, EAxisList::Z, false)
					.OnXCommitted_Lambda([this](FVector::FReal NewValue, ETextCommit::Type CommitInfo, ETransformField2::Type TransformField, EAxisList::Type Axis, bool bCommitted) 
					{
						this->CachedLocation.X = NewValue;
						UE_LOG(LogCore, Log, TEXT("OnXCommitted_Lambda: %lf"), NewValue);
					}, ETransformField2::Location, EAxisList::X, true)
					.OnYCommitted_Lambda([this](FVector::FReal NewValue, ETextCommit::Type CommitInfo, ETransformField2::Type TransformField, EAxisList::Type Axis, bool bCommitted) 
					{
						this->CachedLocation.Y = NewValue;
						UE_LOG(LogCore, Log, TEXT("OnYCommitted_Lambda: %lf"), NewValue);
					}, ETransformField2::Location, EAxisList::Y, true)
					.OnZCommitted_Lambda([this](FVector::FReal NewValue, ETextCommit::Type CommitInfo, ETransformField2::Type TransformField, EAxisList::Type Axis, bool bCommitted) 
					{
						this->CachedLocation.Z = NewValue;
						UE_LOG(LogCore, Log, TEXT("OnZCommitted_Lambda: %lf"), NewValue);
					}, ETransformField2::Location, EAxisList::Z, true)
					//.Font(FontInfo)
					.TypeInterface(TypeInterfaceVector)
					.AllowSpin(true)
					.SpinDelta(1)
					.OnBeginSliderMovement_Lambda([this]() 
					{
						UE_LOG(LogCore, Log, TEXT("OnBeginSliderMovement_Lambda"));
					})
					.OnEndSliderMovement_Lambda([this](FVector::FReal NewValue) 
					{
						UE_LOG(LogCore, Log, TEXT("OnEndSliderMovement_Lambda %lf"), NewValue);
					})
				]
			]
			//SVolumeControl ----------------------------------------------------------------
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Top)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Title:SVolumeControl", "SVolumeControl:"))

				]
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Top)
				[ 
					SNew(SVolumeControl)
					.ToolTipText_Lambda([this]() -> FText
					{
						return LOCTEXT("Title:ToolTipText_Lambda", "SVolumeControl ToolTipText");
					})
					.Volume_Lambda([this]() -> float 
					{
						return this->SVolumeControl_Value;
					})
					.OnVolumeChanged_Lambda([this](float Volume) 
					{
						UE_LOG(LogCore, Log, TEXT("OnVolumeChanged_Lambda: %f"), Volume);
						this->SVolumeControl_Value = Volume;
					})
					.Muted_Lambda([this]() -> bool
					{
						return this->SVolumeControl_bMuted;
					})
					.OnMuteChanged_Lambda([this](bool bMuted) 
					{
						this->SVolumeControl_bMuted = bMuted;
						FText DialogText = FText::Format(LOCTEXT("OnMuteChanged_Lambda", "OnMuteChanged_Lambda Select Cahnged: {0}"), bMuted ? FText::FromString(TEXT("True")) : FText::FromString(TEXT("False")));
						FMessageDialog::Open(EAppMsgType::Ok, DialogText);
					})

					//.ToolTipText_Static(&FLevelEditorActionCallbacks::GetAudioVolumeToolTip)
					//.Volume_Static(&FLevelEditorActionCallbacks::GetAudioVolume)
					//.OnVolumeChanged_Static(&FLevelEditorActionCallbacks::OnAudioVolumeChanged)
					//.Muted_Static(&FLevelEditorActionCallbacks::GetAudioMuted)
					//.OnMuteChanged_Static(&FLevelEditorActionCallbacks::OnAudioMutedChanged)
				]
			]
			//SRotatorInputBox ----------------------------------------------------------------
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Top)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Title:SRotatorInputBox", "SRotatorInputBox:"))
				]
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Top)
				[ 
					SNew(SRotatorInputBox)
					.AllowSpin(true)
					.bColorAxisLabels(true)
					.Roll_Lambda([this]() -> TOptional<float>
					{
						return this->SRotatorInputBox_Value.Roll;
					})
					.Pitch_Lambda([this]() -> TOptional<float>
					{
						return this->SRotatorInputBox_Value.Pitch;
					})
					.Yaw_Lambda([this]() -> TOptional<float>
					{
						return this->SRotatorInputBox_Value.Yaw;
					})
					.OnRollChanged_Lambda([this](float NewValue, ETextCommit::Type CommitInfo, EAxis::Type Axis) 
					{
						UE_LOG(LogCore, Log, TEXT("OnRollChanged_Lambda: %f"), NewValue);
						this->SRotatorInputBox_Value.Roll = NewValue;
					}, ETextCommit::Default, EAxis::X)
					.OnPitchChanged_Lambda([this](float NewValue, ETextCommit::Type CommitInfo, EAxis::Type Axis) 
					{
						UE_LOG(LogCore, Log, TEXT("OnPitchChanged_Lambda: %f"), NewValue);
						this->SRotatorInputBox_Value.Pitch = NewValue;
					}, ETextCommit::Default, EAxis::Y)
					.OnYawChanged_Lambda([this](float NewValue, ETextCommit::Type CommitInfo, EAxis::Type Axis) 
					{
						UE_LOG(LogCore, Log, TEXT("OnYawChanged_Lambda: %f"), NewValue);
						this->SRotatorInputBox_Value.Yaw = NewValue;
					}, ETextCommit::Default, EAxis::Z)
				
					.OnRollCommitted_Lambda([this](float NewValue, ETextCommit::Type CommitInfo, EAxis::Type Axis) 
					{
						UE_LOG(LogCore, Log, TEXT("OnRollCommitted_Lambda: %f"), NewValue);
						this->SRotatorInputBox_Value.Roll = NewValue;
					}, EAxis::X)
					.OnPitchCommitted_Lambda([this](float NewValue, ETextCommit::Type CommitInfo, EAxis::Type Axis) 
					{
						UE_LOG(LogCore, Log, TEXT("OnPitchCommitted_Lambda: %f"), NewValue);
						this->SRotatorInputBox_Value.Pitch = NewValue;
					},  EAxis::Y)
					.OnYawCommitted_Lambda([this](float NewValue, ETextCommit::Type CommitInfo, EAxis::Type Axis) 
					{
						UE_LOG(LogCore, Log, TEXT("OnYawCommitted_Lambda: %f"), NewValue);
						this->SRotatorInputBox_Value.Yaw = NewValue;
					}, EAxis::Z)
				]
			]
			//SVectorInputBox ----------------------------------------------------------------
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Top)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Title:SVectorInputBox", "SVectorInputBox:"))
				]
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Top)
				[ 
 					SNew(SVectorInputBox)
					.X_Lambda([this]() -> TOptional<float> { return this->Position.X;})
					.Y_Lambda([this]() -> TOptional<float> { return this->Position.Y;})
					.Z_Lambda([this]() -> TOptional<float> { return this->Position.Z;})
					.AllowSpin(true)
					.bColorAxisLabels(true)
					.SpinDelta(1.f)
					.OnXChanged_Lambda([this](float NewValue, ETextCommit::Type CommitInfo, EAxis::Type Axis)
					{
						this->Position.X = NewValue;
						UE_LOG(LogCore, Log, TEXT("OnXChanged_Lambda: %f"), NewValue);
					}, ETextCommit::Default, EAxis::X)
					.OnYChanged_Lambda([this](float NewValue, ETextCommit::Type CommitInfo, EAxis::Type Axis)
					{
						this->Position.Y = NewValue;
						UE_LOG(LogCore, Log, TEXT("OnYChanged_Lambda: %f"), NewValue);
					}, ETextCommit::Default, EAxis::Y)
					.OnZChanged_Lambda([this](float NewValue, ETextCommit::Type CommitInfo, EAxis::Type Axis)
					{
						this->Position.Z = NewValue;
						UE_LOG(LogCore, Log, TEXT("OnZChanged_Lambda: %f"), NewValue);
					}, ETextCommit::Default, EAxis::Z)
					.OnXCommitted_Lambda([this](float NewValue, ETextCommit::Type CommitInfo, EAxis::Type Axis)
					{
						this->Position.X = NewValue;
						UE_LOG(LogCore, Log, TEXT("OnXCommitted_Lambda: %f"), NewValue);
					}, EAxis::X)
					.OnYCommitted_Lambda([this](float NewValue, ETextCommit::Type CommitInfo, EAxis::Type Axis)
					{
						this->Position.Y = NewValue;
						UE_LOG(LogCore, Log, TEXT("OnYCommitted_Lambda: %f"), NewValue);
					}, EAxis::Y)
					.OnZCommitted_Lambda([this](float NewValue, ETextCommit::Type CommitInfo, EAxis::Type Axis)
					{
						this->Position.Z = NewValue;
						UE_LOG(LogCore, Log, TEXT("OnZCommitted_Lambda: %f"), NewValue);
					}, EAxis::Z)
					.OnBeginSliderMovement_Lambda([this](){})
					.OnEndSliderMovement_Lambda([this](float NewValue)
					{
						UE_LOG(LogCore, Log, TEXT("OnEndSliderMovement_Lambda: %f"), NewValue);
					})
				]
			]
			//SWidgetSwitcher ----------------------------------------------------------------
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Top)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Title:SWidgetSwitcher", "SWidgetSwitcher:"))
				]
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Top)
				[ 
 					SNew(SWidgetSwitcher)
					.WidgetIndex_Lambda([this]() -> int32 
					{
						if (this->SWidgetSwitcherIndex > 0)
						{
							this->SWidgetSwitcherIndex = 0;
						}
						else
						{
							this->SWidgetSwitcherIndex = 1;
						}
						return this->SWidgetSwitcherIndex;
					})
					+SWidgetSwitcher::Slot()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("Title:A", "A"))
					]
					+SWidgetSwitcher::Slot()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("Title:B", "B"))
					]
				]
			]

		];
}

TSharedRef<SWidget> FEditorWidgetModule::OnGetFilterTestContent()
{
	//BuildFilterTestValues();
	FMenuBuilder MenuBuilder(true, NULL);

	//for (int32 i = 0; i < FilterTestValues.Num(); i++)
	//{
	//	FUIAction ItemAction(FExecuteAction::CreateSP(this, &FEditorWidgetModule::OnFilterTestChange, FilterTestValues[i].Int));
	//	MenuBuilder.AddMenuEntry(FilterTestValues[i].Text, TAttribute<FText>(), FSlateIcon(), ItemAction);
	//}

	for (int32 i = 0; i < 3; i++)
	{
 
		FText SComboButtonTextI = FText::Format(LOCTEXT("hello {0}", "hello {0}"), FText::AsNumber(i));
		//FUIAction ItemAction(FExecuteAction::CreateSP(this, &FEditorWidgetModule::OnFilterTestChange, i));
		
		FUIAction ItemAction( FExecuteAction::CreateLambda([i, this, SComboButtonTextI]()
							{
								 this->SComboButtonText = SComboButtonTextI;
								//UE_LOG(LogCore, Warning, TEXT("hellokey hello Click"));

							}));
		MenuBuilder.AddMenuEntry(SComboButtonTextI, TAttribute<FText>(), FSlateIcon(), ItemAction);
	}

	return MenuBuilder.MakeWidget();
}

void FEditorWidgetModule::OnFilterTestChange(int32 Index)
{
	uint8 EnumValue = Index;
	//FilterTypeHandle->SetValue(EnumValue);
}


#undef LOCTEXT_NAMESPACE
	
 
IMPLEMENT_MODULE(FEditorWidgetModule, EditorWidget)