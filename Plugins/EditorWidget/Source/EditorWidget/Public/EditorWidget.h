// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Math/Vector.h"
#include "Misc/Optional.h"
#include "Widgets/Views/STreeView.h"

class FToolBarBuilder;
class FMenuBuilder;

namespace ETransformField2
{
	enum Type
	{
		Location,
		Rotation,
		Scale
	};
}

struct FVariableMappingInfo
{
public:
	// This is the property that is the most shallow type
	// It will be Transform.Translation.X
	FName PropertyName; 
	// Display Name
	FString DisplayName;
	// List of Children
	// in theory, this actually shouldn't be active if you have children
	// but it represent each raw nonetheless
	// this is to map curve which is always float
	TArray<TSharedPtr<FVariableMappingInfo>> Children;

	/** Static function for creating a new item, but ensures that you can only have a TSharedRef to one */
	static TSharedRef<FVariableMappingInfo> Make(const FName InPathName)
	{
		return MakeShareable(new FVariableMappingInfo(InPathName));
	}

	FName GetPathName() const
	{
		return PropertyName;
	}

	FString GetDisplayName() const
	{
		return DisplayName;
	}

public:
	/** Hidden constructor, always use Make above */
	FVariableMappingInfo(const FName InPathName)
		: PropertyName(InPathName)
	{
		FString PathString = InPathName.ToString();
		int32 Found = PathString.Find(TEXT("."), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
		DisplayName = (Found != INDEX_NONE) ? PathString.RightChop(Found + 1) : PathString;
	}

	/** Hidden constructor, always use Make above */
	FVariableMappingInfo() {}
};

typedef TSharedPtr< FVariableMappingInfo > FVariableMappingInfoPtr;

class SVariableMappingTreeRow
	: public STableRow< TSharedPtr<FVariableMappingInfoPtr> >
{
public:
	
	//SLATE_BEGIN_ARGS(SVariableMappingTreeRow) {}

	/** The item for this row **/
	//SLATE_ARGUMENT(FVariableMappingInfoPtr, Item)
	//SLATE_EVENT(FOnVariableMappingChanged, OnVariableMappingChanged)
	//SLATE_EVENT(FOnGetVariableMapping, OnGetVariableMapping)
	//SLATE_EVENT(FOnGetAvailableMapping, OnGetAvailableMapping)
	//SLATE_EVENT(FOnGetFilteredText, OnGetFilteredText)
	//SLATE_EVENT(FOnVarOptionAvailable, OnVariableOptionAvailable);
	//SLATE_EVENT(FOnPinCheckStateChanged, OnPinCheckStateChanged)
	//SLATE_EVENT(FOnPinGetCheckState, OnPinGetCheckState)
	//SLATE_EVENT(FOnPinIsCheckEnabled, OnPinIsEnabledCheckState)
	//SLATE_END_ARGS()

	//void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTableView);

private:

	/** Widget used to display the list of variable option list*/
	//TSharedPtr<SSearchableComboBox>	VarOptionComboBox;
	TArray< TSharedPtr< FString > >	VariableOptionList;

	/** The name and weight of the variable option*/
	FVariableMappingInfoPtr	Item;

	// Curve combo box options
	//FReply OnClearButtonClicked();
	//FText GetFilterText() const;

	//TSharedRef<SWidget> MakeVarOptionComboWidget(TSharedPtr<FString> InItem);
	//void OnVarOptionSourceChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
	//FText GetVarOptionComboBoxContent() const;
	//FText GetVarOptionComboBoxToolTip() const;
	//void OnVarOptionComboOpening();
	//bool IsVarOptionEnabled() const;
	//TSharedPtr<FString> GetVarOptionString(FName VarOptionName) const;

	//ECheckBoxState IsPinChecked() const;
	//void OnPinCheckStatusChanged(ECheckBoxState NewState);
	//bool IsPinEnabled() const;

	//FOnVariableMappingChanged	OnVariableMappingChanged;
	//FOnGetVariableMapping		OnGetVariableMapping;
	//FOnGetAvailableMapping		OnGetAvailableMapping;
	//FOnGetFilteredText			OnGetFilteredText;
	//FOnVarOptionAvailable		OnVariableOptionAvailable;
	//FOnPinCheckStateChanged		OnPinCheckStateChanged;
	//FOnPinGetCheckState			OnPinGetCheckState;
	//FOnPinIsCheckEnabled		OnPinIsEnabledCheckState;
};

typedef STreeView< TSharedPtr<FVariableMappingInfo> > SVariableMappingTreeView;

class FEditorWidgetModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	
	/** This function will be bound to Command. */
	void PluginButtonClicked();
	void PluginLayoutButtonClicked();
private:

	void RegisterMenus();
	void RegisterMenus2();
	void RegisterLayoutEnum();
public:
	TSharedRef<SDockTab> OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs);
	TSharedRef<SWidget> OnGetFilterTestContent();
	TSharedRef<SBox> CreateWidgets();
	void OnFilterTestChange(int32 Index);

	FText SComboButtonText;
	ECheckBoxState CurrentCheckBoxState = ECheckBoxState::Unchecked;

	FText SEditableTextBoxText;
	FText SMultiLineEditableTextBoxText;
	float SNumericEntryBoxValue = 0;


	FRotator SNumericRotatorInputBoxValue;

	FText SSearchBoxFilterText;
	bool SSegmentedControl_bInIsDestinationDisplayView = true;
	float SSlider_Value = 0;
	double SSpinBox_Value = 0;

	FText SSuggestionTextBox_Text;
	FText SSuggestionTextBox_TextTooltip;
	FText SSuggestionTextBox_TextHint;

	TArray< TSharedPtr<FString> > GroupNodeComboOptions;

	template <typename NumericType>
	struct FOptionalVector
	{
		/**
		 * Sets the value from an FVector                   
		 */
		void Set( const FVector& InVec )
		{
			X = InVec.X;
			Y = InVec.Y;
			Z = InVec.Z;
		}

		/**
		 * Sets the value from an FRotator
		 */
		void Set(const FRotator& InRot)
		{
			X = InRot.Roll;
			Y = InRot.Pitch;
			Z = InRot.Yaw;
		}
		
		/** @return Whether or not the value is set */
		bool IsSet() const
		{
			// The vector is set if all values are set
			return X.IsSet() && Y.IsSet() && Z.IsSet();
		}

		TOptional<NumericType> X;
		TOptional<NumericType> Y;
		TOptional<NumericType> Z;
	};
	FOptionalVector<FVector::FReal> CachedLocation;

	float SVolumeControl_Value = 0;
	bool	 SVolumeControl_bMuted = false;

	struct FSharedRotatorValue
	{
		FSharedRotatorValue() : bInitialized(false) {}

		void Reset()
		{
			bInitialized = false;
		}

		bool IsValid() const { return bInitialized; }

		void Add(const FRotator& R)
		{
			if (!bInitialized)
			{
				Roll = R.Roll;
				Pitch = R.Pitch;
				Yaw = R.Yaw;
				bInitialized = true;
			}
			else
			{
				if (Roll.IsSet() && R.Roll != Roll.GetValue()) { Roll.Reset(); }
				if (Pitch.IsSet() && R.Pitch != Pitch.GetValue()) { Pitch.Reset(); }
				if (Yaw.IsSet() && R.Yaw != Yaw.GetValue()) { Yaw.Reset(); }
			}
		}

		TOptional<float> Roll;
		TOptional<float> Pitch;
		TOptional<float> Yaw;
		bool bInitialized;
	};
	FSharedRotatorValue SRotatorInputBox_Value;

	struct FSharedVectorValue
	{
		FSharedVectorValue() : bInitialized(false) {}

		void Reset()
		{
			bInitialized = false;
		}

		bool IsValid() const { return bInitialized; }

		void Add(const FVector& V)
		{
			if (!bInitialized)
			{
				X = V.X;
				Y = V.Y;
				Z = V.Z;
				bInitialized = true;
			}
			else
			{
				if (X.IsSet() && V.X != X.GetValue()) { X.Reset(); }
				if (Y.IsSet() && V.Y != Y.GetValue()) { Y.Reset(); }
				if (Z.IsSet() && V.Z != Z.GetValue()) { Z.Reset(); }
			}
		}

		TOptional<float> X;
		TOptional<float> Y;
		TOptional<float> Z;
		bool bInitialized;
	};
	FSharedVectorValue Position;
	int32 SWidgetSwitcherIndex = 0;

	void GetContentBrowserSubMenu(UToolMenu* Menu, TSharedRef<FWorkspaceItem> ContentBrowserGroup);

	TSharedRef<SBox> CreatLayouts();
	TSharedRef<SWidget> CreateSTreeView();

	TSharedPtr<SVariableMappingTreeView> VariableMappingTreeView;
	TArray< TSharedPtr<FVariableMappingInfo> > VariableMappingList;

	TSharedRef<SWidget> CreateSListView();
	TArray<TSharedPtr<FString>> AccessSpecifierStrings;
	TSharedRef<SWidget> CreateGridPanel();

	typedef TArray< TWeakObjectPtr<class UWidgetBlueprint> > FWeakBlueprintPointerArray;
	void ExecuteRun(FWeakBlueprintPointerArray InObjects);
	virtual void PostLoadCallback() override;
private:
	TSharedPtr<class FUICommandList> PluginCommands;
 
};
