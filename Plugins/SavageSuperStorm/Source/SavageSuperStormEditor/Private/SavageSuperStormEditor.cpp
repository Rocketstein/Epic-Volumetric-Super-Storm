#include "SavageSuperStormEditor.h"

#include "Customization/SSSActorDetailCustomization.h"
#include "Actors/VolumetricSuperStormActor.h"
#include "Assets/StormPresetDataAsset.h"
#include "Components/StormVerticalProfileToolComponent.h"
#include "Components/StormFlowMapComponent.h"
#include "Components/StormMaterialBinderComponent.h"
#include "Visualization/StormMotionComponentVisualizer.h"
#include "Widgets/Profile/SStormProfilePainter.h"
#include "Widgets/Profile/StormProfileAssetEditorUtils.h"
#include "Widgets/Flowmap/SStormFlowMapEditor.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Brushes/SlateImageBrush.h"
#include "ContentBrowserModule.h"
#include "Editor.h"
#include "Editor/Transactor.h"
#include "Editor/UnrealEdEngine.h"
#include "IContentBrowserSingleton.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/MessageDialog.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "PropertyEditorModule.h"
#include "ScopedTransaction.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "UnrealEdGlobals.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/SWindow.h"
#include "Framework/Application/SlateApplication.h"

DEFINE_LOG_CATEGORY_STATIC(LogSavageSuperStormEditor, Log, All);

IMPLEMENT_MODULE(FSavageSuperStormEditorModule, SavageSuperStormEditor)

namespace
{
TSharedPtr<FSlateStyleSet> SavageSuperStormStyle;

void RegisterSavageSuperStormStyle()
{
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("SavageSuperStorm"));
	if (!Plugin)
	{
		UE_LOG(
			LogSavageSuperStormEditor,
			Warning,
			TEXT("Could not register the storm actor icon because the plugin was not found."));
		return;
	}

	SavageSuperStormStyle = MakeShared<FSlateStyleSet>(TEXT("SavageSuperStormStyle"));
	SavageSuperStormStyle->SetContentRoot(
		FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources")));

	const FString IconPath =
		SavageSuperStormStyle->RootToContentDir(TEXT("StormActorIcon"), TEXT(".png"));
	SavageSuperStormStyle->Set(
		TEXT("ClassIcon.VolumetricSuperStormActor"),
		new FSlateImageBrush(IconPath, FVector2D(16.0f, 16.0f)));
	SavageSuperStormStyle->Set(
		TEXT("ClassThumbnail.VolumetricSuperStormActor"),
		new FSlateImageBrush(IconPath, FVector2D(64.0f, 64.0f)));

	FSlateStyleRegistry::RegisterSlateStyle(*SavageSuperStormStyle);
}

void UnregisterSavageSuperStormStyle()
{
	if (SavageSuperStormStyle)
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*SavageSuperStormStyle);
		SavageSuperStormStyle.Reset();
	}
}
}

void FSavageSuperStormEditorModule::StartupModule()
{
	RegisterSavageSuperStormStyle();

    FPropertyEditorModule& PropertyModule =
        FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
    PropertyModule.RegisterCustomClassLayout(
        AVolumetricSuperStormActor::StaticClass()->GetFName(),
        FOnGetDetailCustomizationInstance::CreateStatic(&FSSSActorDetailCustomization::MakeInstance));
    PropertyModule.NotifyCustomizationModuleChanged();

	if (GUnrealEd)
	{
		StormMotionComponentVisualizer =
			MakeShared<FStormMotionComponentVisualizer>();
		GUnrealEd->RegisterComponentVisualizer(
			UStormMaterialBinderComponent::StaticClass()->GetFName(),
			StormMotionComponentVisualizer);
		StormMotionComponentVisualizer->OnRegister();
	}

}

void FSavageSuperStormEditorModule::ShutdownModule()
{
	UnregisterSavageSuperStormStyle();

	if (GUnrealEd)
	{
		GUnrealEd->UnregisterComponentVisualizer(
			UStormMaterialBinderComponent::StaticClass()->GetFName());
	}
	StormMotionComponentVisualizer.Reset();

    if (FPropertyEditorModule* PropertyModule =
        FModuleManager::GetModulePtr<FPropertyEditorModule>("PropertyEditor"))
    {
        PropertyModule->UnregisterCustomClassLayout(
            AVolumetricSuperStormActor::StaticClass()->GetFName());
        PropertyModule->NotifyCustomizationModuleChanged();
    }

	// A popup opened from one of the tool windows (a details-panel dropdown, say) lives in
	// its own menu window rather than as a child of ours, so Slate's own
	// MenuStack::OnWindowDestroyed will not take it down with the owner. Its content still
	// holds refs into the widget tree we are about to detach, so close menus first.
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().DismissAllMenus();
	}

    if (TSharedPtr<SWindow> Window = ActiveWindow.Pin())
    {
        Window->RequestDestroyWindow();
    }
	if (TSharedPtr<SWindow> Window = ActiveFlowMapWindow.Pin())
	{
		Window->RequestDestroyWindow();
	}
}

void FSavageSuperStormEditorModule::SavePresetAs(
	AVolumetricSuperStormActor* Actor)
{
	if (!Actor)
	{
		return;
	}

	// Capture before opening the dialog so cancellation cannot leave an asset or actor
	// partially modified.
	FStormPresetData CapturedData;
	Actor->BuildPresetData(CapturedData);
	FSaveAssetDialogConfig SaveConfig;
	SaveConfig.DialogTitleOverride = NSLOCTEXT(
		"SavageSuperStorm",
		"SavePresetAsTitle",
		"Save Storm Preset As");
	SaveConfig.DefaultPath = TEXT("/SavageSuperStorm/Presets");
	SaveConfig.DefaultAssetName = TEXT("DA_StormPreset");
	SaveConfig.AssetClassNames.Add(
		UStormPresetDataAsset::StaticClass()->GetClassPathName());
	SaveConfig.ExistingAssetPolicy =
		ESaveAssetDialogExistingAssetPolicy::AllowButWarn;

	FContentBrowserModule& ContentBrowserModule =
		FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	const FString SaveObjectPath =
		ContentBrowserModule.Get().CreateModalSaveAssetDialog(SaveConfig);
	if (SaveObjectPath.IsEmpty())
	{
		return;
	}

	const FString PackageName = SaveObjectPath.Contains(TEXT("."))
		? FPackageName::ObjectPathToPackageName(SaveObjectPath)
		: SaveObjectPath;
	const FString AssetName =
		FPackageName::GetLongPackageAssetName(PackageName);
	if (!FPackageName::IsValidLongPackageName(PackageName) ||
		AssetName.IsEmpty())
	{
		UE_LOG(
			LogSavageSuperStormEditor,
			Warning,
			TEXT("SavePresetAs: invalid preset package name '%s'."),
			*PackageName);
		return;
	}

	UPackage* Package = CreatePackage(*PackageName);
	if (!Package)
	{
		UE_LOG(
			LogSavageSuperStormEditor,
			Warning,
			TEXT("SavePresetAs: failed to create package '%s'."),
			*PackageName);
		return;
	}
	Package->FullyLoad();

	const FString ObjectPath = PackageName + TEXT(".") + AssetName;
	UObject* ExistingObject =
		StaticLoadObject(UObject::StaticClass(), nullptr, *ObjectPath);
	UStormPresetDataAsset* PresetAsset =
		Cast<UStormPresetDataAsset>(ExistingObject);
	if (ExistingObject && !PresetAsset)
	{
		UE_LOG(
			LogSavageSuperStormEditor,
			Warning,
			TEXT("SavePresetAs: '%s' already exists and is not a storm preset."),
			*ObjectPath);
		return;
	}

	const bool bCreatedNew = PresetAsset == nullptr;
	if (bCreatedNew)
	{
		PresetAsset = NewObject<UStormPresetDataAsset>(
			Package,
			*AssetName,
			RF_Public | RF_Standalone | RF_Transactional);
	}
	if (!PresetAsset)
	{
		UE_LOG(
			LogSavageSuperStormEditor,
			Warning,
			TEXT("SavePresetAs: failed to create preset asset '%s'."),
			*ObjectPath);
		return;
	}

	PresetAsset->Modify();
	PresetAsset->PresetData = CapturedData;
	// Route the raw struct assignment through the normal change path so an already-open
	// Details panel / preset editor refreshes and any property-change listeners fire.
	PresetAsset->PostEditChange();
	if (bCreatedNew)
	{
		FAssetRegistryModule::AssetCreated(PresetAsset);
	}
	PresetAsset->MarkPackageDirty();

	// One-shot save: the actor already holds exactly this state (captured above), so nothing
	// is applied back and no reference to the asset is stored on the actor.

	UE_LOG(
		LogSavageSuperStormEditor,
		Log,
		TEXT("SavePresetAs: %s storm preset '%s'."),
		bCreatedNew ? TEXT("created") : TEXT("updated"),
		*ObjectPath);
}

void FSavageSuperStormEditorModule::LoadPresetInto(
	AVolumetricSuperStormActor* Actor)
{
	if (!Actor)
	{
		return;
	}

	FOpenAssetDialogConfig OpenConfig;
	OpenConfig.DialogTitleOverride = NSLOCTEXT(
		"SavageSuperStorm",
		"LoadPresetTitle",
		"Load Storm Preset");
	OpenConfig.DefaultPath = TEXT("/SavageSuperStorm/Presets");
	OpenConfig.AssetClassNames.Add(
		UStormPresetDataAsset::StaticClass()->GetClassPathName());
	OpenConfig.bAllowMultipleSelection = false;

	FContentBrowserModule& ContentBrowserModule =
		FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	const TArray<FAssetData> SelectedAssets =
		ContentBrowserModule.Get().CreateModalOpenAssetDialog(OpenConfig);
	if (SelectedAssets.Num() == 0)
	{
		return;
	}

	UStormPresetDataAsset* PresetAsset =
		Cast<UStormPresetDataAsset>(SelectedAssets[0].GetAsset());
	if (!PresetAsset)
	{
		UE_LOG(
			LogSavageSuperStormEditor,
			Warning,
			TEXT("LoadPreset: selected asset '%s' is not a storm preset."),
			*SelectedAssets[0].GetObjectPathString());
		return;
	}

	UStormVerticalProfileToolComponent* ProfileTool =
		Actor->GetVerticalProfileTool();
	UStormFlowMapComponent* FlowMapComponent = Actor->GetFlowMapComponent();
	if (ProfileTool &&
		!PrepareToReplaceProfileDocument(ProfileTool))
	{
		return;
	}

	// Applying a preset replaces the transient profile and flow-map documents as
	// well as the actor settings. Keep the whole apply in one real transaction so opening it
	// first discards any redo tail, then place a barrier after it. The barrier makes
	// the replacement explicitly irreversible and prevents older profile-history
	// transactions from being consumed against the new registry.
	{
		FScopedTransaction Transaction(
			NSLOCTEXT("SavageSuperStorm", "LoadPresetApply", "Load Storm Preset"));
		Actor->Modify();
		if (ProfileTool)
		{
			ProfileTool->AdvanceProfileEditSession();
		}
		Actor->ApplyPreset(PresetAsset);
		Actor->PostEditChange();
	}
	if ((ProfileTool || FlowMapComponent) && GEditor && GEditor->Trans)
	{
		GEditor->Trans->SetUndoBarrier();
	}

	if (ProfileTool)
	{
		if (TSharedPtr<SStormProfilePainter> Painter = ActivePainter.Pin())
		{
			if (Painter->IsEditingTool(ProfileTool))
			{
				Painter->SetTarget(ProfileTool);
			}
		}
	}

	UE_LOG(
		LogSavageSuperStormEditor,
		Log,
		TEXT("LoadPreset: applied storm preset '%s' to '%s'."),
		*PresetAsset->GetName(),
		*Actor->GetName());
}

bool FSavageSuperStormEditorModule::PrepareToReplaceProfileDocument(
	UStormVerticalProfileToolComponent* Tool)
{
	if (!Tool || !Tool->IsUnsavedToAsset())
	{
		return true;
	}

	if (TSharedPtr<SStormProfilePainter> Painter = ActivePainter.Pin())
	{
		if (Painter->IsEditingTool(Tool))
		{
			return Painter->PrepareToReplaceProfileDocument(
				NSLOCTEXT(
					"SavageSuperStorm",
					"LoadPresetProfileTitle",
					"Load Storm Preset"),
				NSLOCTEXT(
					"SavageSuperStorm",
					"LoadPresetProfilePrompt",
					"The current vertical profile has live key edits.\n\n"
					"Save them before loading the preset?"));
		}
	}

	// Profile authoring normally has an open painter, but a component can retain
	// live working sets after that window closes. Preserve the same save/discard/
	// cancel policy for an existing asset even without the widget.
	if (Tool->PersistentProfileAsset)
	{
		const EAppReturnType::Type Choice = FMessageDialog::Open(
			EAppMsgType::YesNoCancel,
			NSLOCTEXT(
				"SavageSuperStorm",
				"LoadPresetDetachedProfilePrompt",
				"The current vertical profile has live key edits.\n\n"
				"Save them before loading the preset?"),
			NSLOCTEXT(
				"SavageSuperStorm",
				"LoadPresetDetachedProfileTitle",
				"Load Storm Preset"));
		if (Choice == EAppReturnType::Cancel)
		{
			return false;
		}
		if (Choice == EAppReturnType::Yes)
		{
			const bool bSaved =
				SavageSuperStorm::ProfileEditor::CommitToolToAllKeys(
					Tool,
					Tool->PersistentProfileAsset,
					Tool->GetActiveKeyId());
			if (!bSaved)
			{
				FMessageDialog::Open(
					EAppMsgType::Ok,
					NSLOCTEXT(
						"SavageSuperStorm",
						"LoadPresetProfileSaveFailed",
						"The vertical profile could not be saved. The preset was not loaded."),
					NSLOCTEXT(
						"SavageSuperStorm",
						"LoadPresetProfileSaveFailedTitle",
						"Load Storm Preset"));
			}
			return bSaved;
		}
		return true;
	}

	// A detached scratch document has no asset to save into and no painter from
	// which to run Save As. Do not silently discard it: the user can cancel, open
	// the profile editor, and save it first.
	return FMessageDialog::Open(
		EAppMsgType::YesNo,
		NSLOCTEXT(
			"SavageSuperStorm",
			"LoadPresetScratchProfilePrompt",
			"The current vertical profile has live edits but has not been saved as an asset.\n\n"
			"Discard those edits and load the preset?"),
		NSLOCTEXT(
			"SavageSuperStorm",
			"LoadPresetScratchProfileTitle",
			"Load Storm Preset")) == EAppReturnType::Yes;
}

void FSavageSuperStormEditorModule::OpenProfilePainter(UStormVerticalProfileToolComponent* Component)
{
    if (!Component)
    {
        return;
    }

    // One painter at a time: focus the existing window and retarget it.
    if (TSharedPtr<SWindow> Existing = ActiveWindow.Pin())
    {
        Existing->BringToFront();
        if (TSharedPtr<SStormProfilePainter> Painter = ActivePainter.Pin())
        {
            Painter->SetTarget(Component);
        }
        return;
    }

    TSharedRef<SStormProfilePainter> Painter = SNew(SStormProfilePainter);
    Painter->SetTarget(Component);

    TSharedRef<SWindow> Window = SNew(SWindow)
        .Title(INVTEXT("Storm Profile Editor"))
        .ClientSize(FVector2D(1100.f, 720.f))
        [
            Painter
        ];

    // Intercept the close request so the painter can warn about changes that are not
    // saved to the asset. The override is responsible for the actual destruction.
    TWeakPtr<SStormProfilePainter> WeakPainter = Painter;
    Window->SetRequestDestroyWindowOverride(FRequestDestroyWindowOverride::CreateLambda(
        [WeakPainter](const TSharedRef<SWindow>& WindowToDestroy)
        {
            if (TSharedPtr<SStormProfilePainter> PinnedPainter = WeakPainter.Pin())
            {
                if (!PinnedPainter->ConfirmClose())
                {
                    return; // user cancelled the close
                }
            }
            // Close any popup still referencing the painter's widgets before detaching
            // its tree; Slate only unhooks windows that are themselves in the menu stack.
            FSlateApplication::Get().DismissAllMenus();
            FSlateApplication::Get().DestroyWindowImmediately(WindowToDestroy);
        }));

    // Weak refs: FSlateApplication owns the window, so closing it auto-invalidates
    // these and the "already open" guard above resolves to a fresh window.
    ActiveWindow = Window;
    ActivePainter = Painter;

    FSlateApplication::Get().AddWindow(Window);
}

void FSavageSuperStormEditorModule::OpenFlowMapPainter(
	UStormFlowMapComponent* Component)
{
	if (!Component)
	{
		return;
	}

	if (TSharedPtr<SWindow> Existing = ActiveFlowMapWindow.Pin())
	{
		if (TSharedPtr<SStormFlowMapEditor> Painter = ActiveFlowMapPainter.Pin())
		{
			if (!Painter->ConfirmClose())
			{
				return;
			}
			Painter->SetTarget(Component);
		}
		Existing->BringToFront();
		return;
	}

	TSharedRef<SStormFlowMapEditor> Painter = SNew(SStormFlowMapEditor)
		.FlowMapComponent(Component);
	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(INVTEXT("Storm Flow Map Editor"))
		.ClientSize(FVector2D(1000.0f, 760.0f))
		[
			Painter
		];

	TWeakPtr<SStormFlowMapEditor> WeakPainter = Painter;
	Window->SetRequestDestroyWindowOverride(
		FRequestDestroyWindowOverride::CreateLambda(
			[WeakPainter](const TSharedRef<SWindow>& WindowToDestroy)
			{
				if (TSharedPtr<SStormFlowMapEditor> PinnedPainter =
					WeakPainter.Pin())
				{
					if (!PinnedPainter->ConfirmClose())
					{
						return;
					}
				}
				// See OpenProfilePainter: dismiss menus before detaching the tree.
				FSlateApplication::Get().DismissAllMenus();
				FSlateApplication::Get().DestroyWindowImmediately(WindowToDestroy);
			}));

	ActiveFlowMapWindow = Window;
	ActiveFlowMapPainter = Painter;
	FSlateApplication::Get().AddWindow(Window);
}
