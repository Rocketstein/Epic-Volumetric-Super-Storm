// Copyright 2026 GoroGoro. All Rights Reserved.

/**
 * @file StormAuthoringPaths.cpp
 * @brief Implements the shared save/load dialogs for the storm authoring directories.
 */

#include "Assets/StormAuthoringPaths.h"

#include "ContentBrowserDelegates.h"
#include "ContentBrowserModule.h"
#include "Editor.h"
#include "IContentBrowserSingleton.h"
#include "ObjectTools.h"
#include "Settings/StormEditorSettings.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Editor/EditorEngine.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Styling/StyleColors.h"
#include "UObject/Package.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "StormAuthoringPaths"

DEFINE_LOG_CATEGORY_STATIC(LogStormAuthoringPaths, Log, All);

namespace VolumetricSuperStorm::AuthoringPaths
{
namespace
{
	/** Used whenever the configured directory is unusable. Must stay valid long package paths. */
	const TCHAR* DefaultAuthoringRoot(ECategory Category)
	{
		switch (Category)
		{
		case ECategory::Preset:
			return TEXT("/Game/SuperStorm/Presets");
		case ECategory::VerticalProfile:
			return TEXT("/Game/SuperStorm/VerticalProfiles");
		default:
			return TEXT("/Game/SuperStorm/FlowMaps");
		}
	}

	/** Where the plugin's own example assets ship. Read-only as far as the tools are concerned. */
	const TCHAR* ShippedRoot(ECategory Category)
	{
		switch (Category)
		{
		case ECategory::Preset:
			return TEXT("/VolumetricSuperStorm/VolumetricSuperStorm/Preset");
		case ECategory::VerticalProfile:
			return TEXT("/VolumetricSuperStorm/VolumetricSuperStorm/VerticalProfile");
		default:
			return TEXT("/VolumetricSuperStorm/VolumetricSuperStorm/FlowMaps");
		}
	}

	FString ConfiguredRoot(ECategory Category)
	{
		const UStormEditorSettings* Settings = GetDefault<UStormEditorSettings>();
		if (!Settings)
		{
			return FString();
		}

		switch (Category)
		{
		case ECategory::Preset:
			return Settings->PresetDirectory.Path;
		case ECategory::VerticalProfile:
			return Settings->VerticalProfileDirectory.Path;
		default:
			return Settings->FlowMapDirectory.Path;
		}
	}

	/**
	 * Trims a configured path into a usable long package path, or returns false. A path whose
	 * root is not mounted is rejected here rather than at save time, where CreatePackage would
	 * fail with nothing to fall back to.
	 */
	bool NormalizeRoot(const FString& InPath, FString& OutPath)
	{
		OutPath = InPath;
		OutPath.TrimStartAndEndInline();
		while (OutPath.EndsWith(TEXT("/")))
		{
			OutPath.LeftChopInline(1);
		}

		FString UnusedFilename;
		return !OutPath.IsEmpty() &&
			FPackageName::TryConvertLongPackageNameToFilename(OutPath, UnusedFilename);
	}

	bool IsUnderRoot(const FString& PackageName, const FString& Root)
	{
		return PackageName == Root || PackageName.StartsWith(Root + TEXT("/"));
	}

	struct FSaveNameValidation
	{
		bool  bCanSave = false;
		bool  bWillOverwrite = false;
		FText Message;
	};

	FSaveNameValidation ValidateSaveName(
		const FString& Root,
		const UClass* AssetClass,
		const FString& AssetName)
	{
		FSaveNameValidation Validation;
		if (AssetName.IsEmpty())
		{
			Validation.Message = LOCTEXT("EmptyName", "Enter a name for the asset.");
			return Validation;
		}

		FText Reason;
		if (!FName(*AssetName).IsValidObjectName(Reason))
		{
			Validation.Message = Reason;
			return Validation;
		}

		const FString PackageName = Root / AssetName;
		if (!FPackageName::IsValidLongPackageName(PackageName, false, &Reason))
		{
			Validation.Message = Reason;
			return Validation;
		}

		const FAssetData Existing = IAssetRegistry::GetChecked().GetAssetByObjectPath(
			FSoftObjectPath(PackageName + TEXT(".") + AssetName));
		if (Existing.IsValid())
		{
			if (!Existing.IsInstanceOf(AssetClass))
			{
				Validation.Message = FText::Format(
					LOCTEXT("NameTakenByOtherType", "'{0}' already exists and is not a {1}."),
					FText::FromString(AssetName),
					AssetClass->GetDisplayNameText());
				return Validation;
			}

			Validation.bWillOverwrite = true;
			Validation.Message = FText::Format(
				LOCTEXT("NameWillOverwrite", "'{0}' already exists and will be overwritten."),
				FText::FromString(AssetName));
		}

		Validation.bCanSave = true;
		return Validation;
	}
}

FString GetAuthoringRoot(ECategory Category)
{
	FString Root;
	if (NormalizeRoot(ConfiguredRoot(Category), Root) &&
		!IsUnderRoot(Root, ShippedRoot(Category)))
	{
		return Root;
	}

	return DefaultAuthoringRoot(Category);
}

FString GetShippedRoot(ECategory Category)
{
	return ShippedRoot(Category);
}

bool IsShippedAsset(const UObject* Asset, ECategory Category)
{
	if (!Asset)
	{
		return false;
	}

	const UPackage* Package = Asset->GetOutermost();
	return Package && IsUnderRoot(Package->GetName(), ShippedRoot(Category));
}

bool CanOverwriteAsset(const UObject* Asset, ECategory Category)
{
	return Asset && !IsShippedAsset(Asset, Category);
}

FText GetShippedAssetOverwriteBlockedText()
{
	return LOCTEXT(
		"ShippedAssetOverwriteBlocked",
		"Plugin default assets cannot be overwritten. Use Save As... to create an editable copy.");
}

void EnsureAuthoringRootExists(ECategory Category)
{
	const FString Root = GetAuthoringRoot(Category);

	FString Filename;
	if (!FPackageName::TryConvertLongPackageNameToFilename(Root, Filename))
	{
		UE_LOG(
			LogStormAuthoringPaths,
			Warning,
			TEXT("'%s' is not under a mounted content root; storm content cannot be saved there."),
			*Root);
		return;
	}

	if (!IFileManager::Get().DirectoryExists(*Filename) &&
		!IFileManager::Get().MakeDirectory(*Filename, /*Tree=*/true))
	{
		UE_LOG(
			LogStormAuthoringPaths,
			Warning,
			TEXT("Could not create the storm authoring directory '%s'."),
			*Filename);
		return;
	}

	// Without this the folder exists on disk but not in the Content Browser's path tree
	// until the next registry scan, and a path-filtered asset picker finds nothing.
	if (IAssetRegistry* AssetRegistry = IAssetRegistry::Get())
	{
		AssetRegistry->AddPath(Root);
	}
}

void EnsureAuthoringRootsExist()
{
	EnsureAuthoringRootExists(ECategory::Preset);
	EnsureAuthoringRootExists(ECategory::VerticalProfile);
	EnsureAuthoringRootExists(ECategory::FlowMap);
}

FAssetData PickAssetModal(ECategory Category, const UClass* AssetClass, const FText& Title)
{
	if (!AssetClass || !GEditor)
	{
		return FAssetData();
	}

	EnsureAuthoringRootExists(Category);
	const FString AuthoringRoot = GetAuthoringRoot(Category);
	const FString ShippedExamples = GetShippedRoot(Category);

	FAssetData SelectedAsset;
	bool       bConfirmed = false;

	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(Title)
		.ClientSize(FVector2D(520.0f, 520.0f))
		.SupportsMinimize(false)
		.SupportsMaximize(false);
	TWeakPtr<SWindow> WeakWindow = Window;

	FAssetPickerConfig PickerConfig;
	PickerConfig.Filter.ClassPaths.Add(AssetClass->GetClassPathName());
	PickerConfig.Filter.bRecursiveClasses = true;
	// Scope by predicate, not by Filter.PackagePaths. An asset picker does not use
	// PackagePaths as a backend filter: SAssetPicker moves them into the view's content
	// *sources* and resets them on the filter, so they are resolved as virtual paths and a
	// plugin folder is easily lost. A predicate is evaluated against the assets themselves.
	PickerConfig.OnShouldFilterAsset = FOnShouldFilterAsset::CreateLambda(
		[AuthoringRoot, ShippedExamples](const FAssetData& InAsset)
		{
			const FString PackagePath = InAsset.PackagePath.ToString();
			auto IsUnder = [&PackagePath](const FString& Root)
			{
				return PackagePath == Root || PackagePath.StartsWith(Root + TEXT("/"));
			};

			// Returning true filters the asset OUT.
			return !IsUnder(AuthoringRoot) && !IsUnder(ShippedExamples);
		});
	// The shipped examples are plugin content, which the Content Browser hides by default --
	// and a Fab install puts them under Engine/Plugins/Marketplace, where they count as
	// engine content as well. SAssetView::ForceShowPluginFolder needs both flags for an
	// engine plugin; either one missing leaves the picker empty.
	PickerConfig.bForceShowPluginContent = true;
	PickerConfig.bForceShowEngineContent = true;
	PickerConfig.SelectionMode = ESelectionMode::Single;
	PickerConfig.InitialAssetViewType = EAssetViewType::Tile;
	PickerConfig.bFocusSearchBoxWhenOpened = true;
	PickerConfig.OnAssetSelected = FOnAssetSelected::CreateLambda(
		[&SelectedAsset](const FAssetData& InAsset)
		{
			SelectedAsset = InAsset;
		});
	PickerConfig.OnAssetsActivated = FOnAssetsActivated::CreateLambda(
		[&SelectedAsset, &bConfirmed, WeakWindow](
			const TArray<FAssetData>& InAssets, EAssetTypeActivationMethod::Type)
		{
			if (!InAssets.IsEmpty())
			{
				SelectedAsset = InAssets[0];
				bConfirmed = true;
				if (TSharedPtr<SWindow> ActivatedWindow = WeakWindow.Pin())
				{
					ActivatedWindow->RequestDestroyWindow();
				}
			}
		});

	FContentBrowserModule& ContentBrowserModule =
		FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

	Window->SetContent(
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.0f, 8.0f, 8.0f, 0.0f)
		[
			SNew(STextBlock)
			.AutoWrapText(true)
			.ColorAndOpacity(FStyleColors::Foreground)
			.Text(FText::Format(
				LOCTEXT("PickerScope", "Showing {0} and the plugin's shipped examples."),
				FText::FromString(AuthoringRoot)))
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(4.0f)
		[
			ContentBrowserModule.Get().CreateAssetPicker(PickerConfig)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Right)
		.Padding(8.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 8.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("PickerLoad", "Load"))
				.IsEnabled_Lambda([&SelectedAsset]() { return SelectedAsset.IsValid(); })
				.OnClicked_Lambda([&bConfirmed, WeakWindow]()
				{
					bConfirmed = true;
					if (TSharedPtr<SWindow> LoadWindow = WeakWindow.Pin())
					{
						LoadWindow->RequestDestroyWindow();
					}
					return FReply::Handled();
				})
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("PickerCancel", "Cancel"))
				.OnClicked_Lambda([WeakWindow]()
				{
					if (TSharedPtr<SWindow> CancelWindow = WeakWindow.Pin())
					{
						CancelWindow->RequestDestroyWindow();
					}
					return FReply::Handled();
				})
			]
		]);

	GEditor->EditorAddModalWindow(Window);

	return bConfirmed ? SelectedAsset : FAssetData();
}

bool PickSaveNameModal(
	ECategory Category,
	const UClass* AssetClass,
	const FString& DefaultAssetName,
	const FText& Title,
	FString& OutPackageName,
	FString& OutAssetName)
{
	OutPackageName.Reset();
	OutAssetName.Reset();
	if (!AssetClass || !GEditor)
	{
		return false;
	}

	EnsureAuthoringRootExists(Category);
	const FString Root = GetAuthoringRoot(Category);

	// State lives on the stack: EditorAddModalWindow does not return until the window is
	// destroyed, so no widget below outlives it.
	FString             AssetName = ObjectTools::SanitizeObjectName(DefaultAssetName);
	FSaveNameValidation Validation = ValidateSaveName(Root, AssetClass, AssetName);
	bool                bAccepted = false;

	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(Title)
		.ClientSize(FVector2D(460.0f, 180.0f))
		.SizingRule(ESizingRule::FixedSize)
		.SupportsMinimize(false)
		.SupportsMaximize(false);
	TWeakPtr<SWindow> WeakWindow = Window;

	auto Close = [&bAccepted, WeakWindow](bool bInAccepted)
	{
		bAccepted = bInAccepted;
		if (TSharedPtr<SWindow> PinnedWindow = WeakWindow.Pin())
		{
			PinnedWindow->RequestDestroyWindow();
		}
	};

	TSharedPtr<SEditableTextBox> NameBox;

	Window->SetContent(
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(14.0f, 14.0f, 14.0f, 2.0f)
		[
			SNew(STextBlock)
			.AutoWrapText(true)
			.ColorAndOpacity(FStyleColors::Foreground)
			.Text(FText::Format(
				LOCTEXT("SaveDestination", "Saving into {0}"),
				FText::FromString(Root)))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(14.0f, 6.0f, 14.0f, 0.0f)
		[
			SAssignNew(NameBox, SEditableTextBox)
			.Text(FText::FromString(AssetName))
			.SelectAllTextWhenFocused(true)
			.HintText(LOCTEXT("SaveNameHint", "Asset name"))
			.OnTextChanged_Lambda(
				[&AssetName, &Validation, Root, AssetClass](const FText& NewText)
				{
					AssetName = NewText.ToString();
					Validation = ValidateSaveName(Root, AssetClass, AssetName);
				})
			.OnTextCommitted_Lambda(
				[&AssetName, &Validation, &Close, Root, AssetClass](
					const FText& NewText, ETextCommit::Type CommitType)
				{
					AssetName = NewText.ToString();
					Validation = ValidateSaveName(Root, AssetClass, AssetName);
					if (CommitType == ETextCommit::OnEnter && Validation.bCanSave)
					{
						Close(true);
					}
				})
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(14.0f, 6.0f, 14.0f, 0.0f)
		[
			SNew(STextBlock)
			.AutoWrapText(true)
			.Text_Lambda([&Validation]() { return Validation.Message; })
			.ColorAndOpacity_Lambda([&Validation]()
			{
				return Validation.bCanSave ? FStyleColors::Warning : FStyleColors::Error;
			})
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Right)
		.Padding(14.0f, 6.0f, 14.0f, 12.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 8.0f, 0.0f)
			[
				SNew(SButton)
				.Text_Lambda([&Validation]()
				{
					return Validation.bWillOverwrite
						? LOCTEXT("SaveOverwrite", "Overwrite")
						: LOCTEXT("Save", "Save");
				})
				.IsEnabled_Lambda([&Validation]() { return Validation.bCanSave; })
				.OnClicked_Lambda([&Close]()
				{
					Close(true);
					return FReply::Handled();
				})
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("SaveCancel", "Cancel"))
				.OnClicked_Lambda([&Close]()
				{
					Close(false);
					return FReply::Handled();
				})
			]
		]);

	Window->SetWidgetToFocusOnActivate(NameBox);
	GEditor->EditorAddModalWindow(Window);

	if (!bAccepted || !Validation.bCanSave)
	{
		return false;
	}

	OutAssetName = AssetName;
	OutPackageName = Root / AssetName;
	return true;
}
}

#undef LOCTEXT_NAMESPACE
