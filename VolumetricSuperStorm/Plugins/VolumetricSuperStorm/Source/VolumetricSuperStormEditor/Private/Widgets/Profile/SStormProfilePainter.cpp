// Copyright 2026 GoroGoro. All Rights Reserved.

/**
 * @file SStormProfilePainter.cpp
 * @brief Implements the vertical profile painter window.
 */

#include "Widgets/Profile/SStormProfilePainter.h"
#include "Widgets/Profile/StormProfileAssetEditorUtils.h"

#include "Actors/VolumetricSuperStormActor.h"
#include "Assets/StormAuthoringPaths.h"
#include "Assets/StormVerticalProfileAsset.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/StormVerticalProfileToolComponent.h"
#include "Data/StormRenderTargetResolution.h"
#include "DetailsViewArgs.h"
#include "Editor.h"
#include "Editor/Transactor.h"
#include "Misc/MessageDialog.h"
#include "Widgets/SWindow.h"
#include "Engine/Texture.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "IDetailsView.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "ScopedTransaction.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "UObject/Package.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Profile/SStormProfileCompositePreview.h"
#include "Widgets/Profile/SStormProfileTimeline.h"
#include "Widgets/Profile/SProfilePaintSurface.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
    FString MakeProfileObjectPath(const FString& PackageName, const FString& AssetName)
    {
        return PackageName + TEXT(".") + AssetName;
    }

    UStormVerticalProfileAsset* CreateOrLoadProfileAsset(const FString& PackageName, const FString& AssetName)
    {
        UPackage* Package = CreatePackage(*PackageName);
        if (!Package)
        {
            return nullptr;
        }
        Package->FullyLoad();

        const FString ObjectPath = MakeProfileObjectPath(PackageName, AssetName);
        UObject* ExistingObject = StaticLoadObject(UObject::StaticClass(), nullptr, *ObjectPath);
        UStormVerticalProfileAsset* ProfileAsset = Cast<UStormVerticalProfileAsset>(ExistingObject);
        if (ExistingObject && !ProfileAsset)
        {
            return nullptr;
        }

        if (!ProfileAsset)
        {
            ProfileAsset = NewObject<UStormVerticalProfileAsset>(Package, *AssetName, RF_Public | RF_Standalone | RF_Transactional);
            FAssetRegistryModule::AssetCreated(ProfileAsset);
            Package->MarkPackageDirty();
        }

        return ProfileAsset;
    }

    // Session-scoped suppression for the Paint -> Parameterize warning. Static so it survives
    // painter reopen within the editor process (= "this session"); resets on editor restart.
    static bool GbSuppressParamModeWarning = false;

    // Modal confirm for switching Paint -> Parameterize (which discards brushwork). Returns
    // true if the user chose Proceed; sets bOutDontShowAgain from the checkbox.
    bool ShowSwitchToParameterizeWarning(bool& bOutDontShowAgain)
    {
        bool bProceed = false;
        bOutDontShowAgain = false;

        TSharedRef<SWindow> Window = SNew(SWindow)
            .Title(INVTEXT("Switch to Parameterize Mode"))
            .ClientSize(FVector2D(430.f, 175.f))
            .SupportsMinimize(false)
            .SupportsMaximize(false);
        TWeakPtr<SWindow> WeakWindow = Window;

        Window->SetContent(
            SNew(SVerticalBox)
            + SVerticalBox::Slot()
            .FillHeight(1.f)
            .Padding(14.f, 14.f, 14.f, 8.f)
            [
                SNew(STextBlock)
                .AutoWrapText(true)
                .Text(INVTEXT("Switching to Parameterize mode rebuilds the Top and Anvil profiles from their parameters and discards your current brushwork.\n\nProceed?"))
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(14.f, 0.f, 14.f, 8.f)
            [
                SNew(SCheckBox)
                .OnCheckStateChanged_Lambda([&bOutDontShowAgain](ECheckBoxState S) { bOutDontShowAgain = (S == ECheckBoxState::Checked); })
                [
                    SNew(STextBlock).Text(INVTEXT("Don't show this again this session"))
                ]
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .HAlign(HAlign_Right)
            .Padding(14.f, 0.f, 14.f, 12.f)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(0.f, 0.f, 8.f, 0.f)
                [
                    SNew(SButton)
                    .Text(INVTEXT("Proceed"))
                    .OnClicked_Lambda([&bProceed, WeakWindow]()
                    {
                        bProceed = true;
                        if (TSharedPtr<SWindow> W = WeakWindow.Pin()) { W->RequestDestroyWindow(); }
                        return FReply::Handled();
                    })
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SNew(SButton)
                    .Text(INVTEXT("Cancel"))
                    .OnClicked_Lambda([WeakWindow]()
                    {
                        if (TSharedPtr<SWindow> W = WeakWindow.Pin()) { W->RequestDestroyWindow(); }
                        return FReply::Handled();
                    })
                ]
            ]);

        GEditor->EditorAddModalWindow(Window);
        return bProceed;
    }
}

void SStormProfilePainter::Construct(const FArguments& InArgs)
{
    TSharedPtr<SVerticalBox> CenterPanel;
    TSharedPtr<SBorder> TimelinePanel;

    FDetailsViewArgs DetailsViewArgs;
    DetailsViewArgs.bAllowSearch = true;
    DetailsViewArgs.bHideSelectionTip = true;
    DetailsViewArgs.bShowObjectLabel = false;
    DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;

    FPropertyEditorModule& PropertyModule =
        FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
    DetailsView = PropertyModule.CreateDetailView(DetailsViewArgs);

    ChildSlot
        [
            SNew(SSplitter)
            .Orientation(Orient_Horizontal)
            + SSplitter::Slot()
            .Value(0.28f)
            [
                SNew(SSplitter)
                .Orientation(Orient_Vertical)
                + SSplitter::Slot()
                .Value(0.34f)
                [
                    SNew(SBox)
                    .Padding(6.f)
                    .HAlign(HAlign_Fill)
                    .VAlign(VAlign_Fill)
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot()
                        .FillHeight(1.f)
                        [
                            SNew(SBox)
                            .HAlign(HAlign_Fill)
                            .VAlign(VAlign_Fill)
                            .MinAspectRatio(1.f)
                            .MaxAspectRatio(1.f)
                            [
                                SAssignNew(CompositePreview, SCompositeProfilePreview)
                                .TopProfile_Lambda([this]() -> UTexture*
                                    { return GetDisplayTopProfile(); })
                                .BottomProfile_Lambda([this]() -> UTexture*
                                    { return GetDisplayBottomProfile(); })
                                .AnvilProfile_Lambda([this]() -> UTexture*
                                    { return GetDisplayAnvilProfile(); })
                            ]
                        ]
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(0.f, 4.f, 0.f, 0.f)
                        [
                            SNew(SHorizontalBox)
                            + SHorizontalBox::Slot()
                            .AutoWidth()
                            [
                                SNew(SButton)
                                .Text(INVTEXT("<"))
                                .ToolTipText(INVTEXT("Previous profile preview"))
                                .ContentPadding(FMargin(6.f, 2.f))
                                .IsEnabled_Lambda([this]()
                                {
                                    return CompositePreview.IsValid() &&
                                        CompositePreview->CanCyclePreview(-1);
                                })
                                .OnClicked_Lambda([this]()
                                {
                                    if (CompositePreview.IsValid())
                                    {
                                        CompositePreview->CyclePreview(-1);
                                    }
                                    return FReply::Handled();
                                })
                            ]
                            + SHorizontalBox::Slot()
                            .FillWidth(1.f)
                            .HAlign(HAlign_Center)
                            .VAlign(VAlign_Center)
                            [
                                SNew(STextBlock)
                                .Text_Lambda([this]()
                                {
                                    return CompositePreview.IsValid()
                                        ? CompositePreview->GetPreviewModeText()
                                        : FText::GetEmpty();
                                })
                            ]
                            + SHorizontalBox::Slot()
                            .AutoWidth()
                            [
                                SNew(SButton)
                                .Text(INVTEXT(">"))
                                .ToolTipText(INVTEXT("Next profile preview"))
                                .ContentPadding(FMargin(6.f, 2.f))
                                .IsEnabled_Lambda([this]()
                                {
                                    return CompositePreview.IsValid() &&
                                        CompositePreview->CanCyclePreview(1);
                                })
                                .OnClicked_Lambda([this]()
                                {
                                    if (CompositePreview.IsValid())
                                    {
                                        CompositePreview->CyclePreview(1);
                                    }
                                    return FReply::Handled();
                                })
                            ]
                        ]
                    ]
                ]
                + SSplitter::Slot()
                .Value(0.66f)
                [
                    SNew(SBox)
                    .MinDesiredWidth(260.f)
                    [
                        DetailsView.ToSharedRef()
                    ]
                ]
            ]
            + SSplitter::Slot()
            .Value(0.52f)
            [
                SAssignNew(CenterPanel, SVerticalBox)
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(8.f, 8.f, 8.f, 4.f)
                [
                    SAssignNew(TimelinePanel, SBorder)
                    .Padding(6.f)
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        [
                            SNew(SHorizontalBox)
                            + SHorizontalBox::Slot()
                            .FillWidth(1.f)
                            .VAlign(VAlign_Center)
                            [
                                SNew(STextBlock)
                                .Text_Lambda([this]()
                                {
                                    const UStormVerticalProfileAsset* Asset =
                                        GetEditedProfileAsset();
                                    return Asset
                                        ? FText::Format(
                                            INVTEXT("Timeline - {0} ({1}/{2} keys)"),
                                            FText::FromString(Asset->GetName()),
                                            FText::AsNumber(Asset->GetKeyCount()),
                                            FText::AsNumber(
                                                UStormVerticalProfileAsset::MaxProfileKeys))
                                        : INVTEXT("Timeline - save or load a profile asset first");
                                })
                            ]
                            + SHorizontalBox::Slot()
                            .AutoWidth()
                            .Padding(4.f, 0.f)
                            [
                                SNew(SCheckBox)
                                .IsEnabled_Lambda([this]()
                                {
                                    return VolumetricSuperStorm::AuthoringPaths::CanOverwriteAsset(
                                        GetEditedProfileAsset(),
                                        VolumetricSuperStorm::AuthoringPaths::ECategory::VerticalProfile);
                                })
                                .ToolTipText_Lambda([this]()
                                {
                                    const UStormVerticalProfileAsset* Asset = GetEditedProfileAsset();
                                    return Asset && !VolumetricSuperStorm::AuthoringPaths::CanOverwriteAsset(
                                            Asset,
                                            VolumetricSuperStorm::AuthoringPaths::ECategory::VerticalProfile)
                                        ? INVTEXT(
                                            "Looping is part of this profile's key timeline, which is "
                                            "read-only on a plugin default. Use Save As... to create an "
                                            "editable copy.")
                                        : INVTEXT("Whether this profile's playback loops by default.");
                                })
                                .IsChecked(this, &SStormProfilePainter::GetSequenceLoopState)
                                .OnCheckStateChanged(
                                    this,
                                    &SStormProfilePainter::OnSequenceLoopChanged)
                                [
                                    SNew(STextBlock).Text(INVTEXT("Loop"))
                                ]
                            ]
                            + SHorizontalBox::Slot()
                            .AutoWidth()
                            .Padding(4.f, 0.f)
                            [
                                SNew(SButton)
                                .Text(INVTEXT("Add Key"))
                                .ToolTipText(INVTEXT(
                                    "Duplicate the selected key, including independent "
                                    "embedded textures, and select the new key."))
                                .IsEnabled(this, &SStormProfilePainter::CanAddProfileKey)
                                .OnClicked(this, &SStormProfilePainter::OnAddProfileKey)
                            ]
                            + SHorizontalBox::Slot()
                            .AutoWidth()
                            [
                                SNew(SButton)
                                .Text(INVTEXT("Delete"))
                                .ToolTipText(INVTEXT("Delete the selected profile key."))
                                .IsEnabled(
                                    this,
                                    &SStormProfilePainter::CanDeleteSelectedKey)
                                .OnClicked(
                                    this,
                                    &SStormProfilePainter::OnDeleteSelectedKey)
                            ]
                        ]
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(0.f, 6.f, 0.f, 4.f)
                        [
                            SNew(SBox)
                            .HeightOverride(112.0f)
                            [
                                SAssignNew(
                                    TimelineWidget,
                                    SStormProfileTimeline)
                                .ProfileAsset_Lambda([this]()
                                {
                                    return GetEditedProfileAsset();
                                })
                                .SelectedKeyId_Lambda([this]()
                                {
                                    return SelectedKeyId;
                                })
                                .PlayheadTime_Lambda([this]()
                                {
                                    const UStormVerticalProfileToolComponent* Tool =
                                        GetTool();
                                    return Tool
                                        ? Tool->GetProfileSequenceTime()
                                        : 0.0f;
                                })
                                .PlayheadActive(
                                    this,
                                    &SStormProfilePainter::IsProfileSequencePreviewActive)
                                .ScrubbingEnabled(
                                    this,
                                    &SStormProfilePainter::CanPreviewProfileSequence)
                                .AddingEnabled(
                                    this,
                                    &SStormProfilePainter::CanAddProfileKey)
                                .OnKeySelected(
                                    this,
                                    &SStormProfilePainter::OnTimelineKeySelected)
                                .OnKeyTimeCommitted(
                                    this,
                                    &SStormProfilePainter::OnTimelineKeyTimeCommitted)
                                .OnScrubbed(
                                    this,
                                    &SStormProfilePainter::OnTimelineScrubbed)
                                .OnAddKeyRequested(
                                    this,
                                    &SStormProfilePainter::OnTimelineAddKeyRequested)
                            ]
                        ]
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(0.f, 4.f, 0.f, 0.f)
                        [
                            SNew(SHorizontalBox)
                            + SHorizontalBox::Slot()
                            .AutoWidth()
                            .Padding(0.f, 0.f, 4.f, 0.f)
                            [
                                SNew(SButton)
                                .Text(INVTEXT("Forward"))
                                .ToolTipText(INVTEXT(
                                    "Play the profile-key sequence forward from the current playhead."))
                                .IsEnabled(this, &SStormProfilePainter::CanPreviewProfileSequence)
                                .OnClicked(this, &SStormProfilePainter::OnPlayProfileSequence)
                            ]
                            + SHorizontalBox::Slot()
                            .AutoWidth()
                            .Padding(0.f, 0.f, 4.f, 0.f)
                            [
                                SNew(SButton)
                                .Text(INVTEXT("Reverse"))
                                .ToolTipText(INVTEXT(
                                    "Play the profile-key sequence backward from the current playhead."))
                                .IsEnabled(this, &SStormProfilePainter::CanPreviewProfileSequence)
                                .OnClicked(
                                    this,
                                    &SStormProfilePainter::OnPlayProfileSequenceReverse)
                            ]
                            + SHorizontalBox::Slot()
                            .AutoWidth()
                            .Padding(0.f, 0.f, 4.f, 0.f)
                            [
                                SNew(SButton)
                                .Text(INVTEXT("Pause"))
                                .ToolTipText(INVTEXT(
                                    "Pause sequence playback and keep the current preview."))
                                .IsEnabled(this, &SStormProfilePainter::IsProfileSequencePlaying)
                                .OnClicked(this, &SStormProfilePainter::OnPauseProfileSequence)
                            ]
                            + SHorizontalBox::Slot()
                            .AutoWidth()
                            .Padding(0.f, 0.f, 8.f, 0.f)
                            [
                                SNew(SButton)
                                .Text(INVTEXT("Stop"))
                                .ToolTipText(INVTEXT(
                                    "Stop sequence preview and return to the selected key."))
                                .IsEnabled(this, &SStormProfilePainter::IsProfileSequencePreviewActive)
                                .OnClicked(this, &SStormProfilePainter::OnStopProfileSequence)
                            ]
                            + SHorizontalBox::Slot()
                            .FillWidth(1.f)
                            .VAlign(VAlign_Center)
                            .HAlign(HAlign_Right)
                            [
                                SNew(STextBlock)
                                .Text(this, &SStormProfilePainter::GetProfileSequencePreviewText)
                            ]
                        ]
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(0.f, 6.f, 0.f, 0.f)
                        [
                            SNew(SHorizontalBox)
                            + SHorizontalBox::Slot()
                            .AutoWidth()
                            .VAlign(VAlign_Center)
                            .Padding(0.f, 0.f, 4.f, 0.f)
                            [
                                SNew(STextBlock).Text(INVTEXT("Time"))
                            ]
                            + SHorizontalBox::Slot()
                            .FillWidth(1.f)
                            .Padding(0.f, 0.f, 8.f, 0.f)
                            [
                                SNew(SSpinBox<float>)
                                .IsEnabled(
                                    this,
                                    &SStormProfilePainter::CanEditSelectedKeyTime)
                                .MinValue(0.0f)
                                .MaxValue(
                                    UStormVerticalProfileAsset::MaxKeyTimeSeconds)
                                .MinSliderValue(0.0f)
                                .MaxSliderValue(60.0f)
                                .Delta(0.05f)
                                .Value(this, &SStormProfilePainter::GetSelectedKeyTime)
                                .OnValueCommitted(
                                    this,
                                    &SStormProfilePainter::OnSelectedKeyTimeCommitted)
                            ]
                            + SHorizontalBox::Slot()
                            .AutoWidth()
                            .VAlign(VAlign_Center)
                            .Padding(0.f, 0.f, 4.f, 0.f)
                            [
                                SNew(STextBlock).Text(INVTEXT("Outgoing"))
                            ]
                            + SHorizontalBox::Slot()
                            .AutoWidth()
                            .Padding(0.f, 0.f, 4.f, 0.f)
                            [
                                SNew(SCheckBox)
                                .Style(FAppStyle::Get(), "ToggleButtonCheckbox")
                                .IsEnabled(this, &SStormProfilePainter::HasSelectedKey)
                                .IsChecked(
                                    this,
                                    &SStormProfilePainter::GetSelectedKeyLinearEasingState)
                                .OnCheckStateChanged(
                                    this,
                                    &SStormProfilePainter::OnSelectedKeyLinearEasingChanged)
                                [
                                    SNew(STextBlock).Text(INVTEXT("Linear"))
                                ]
                            ]
                            + SHorizontalBox::Slot()
                            .AutoWidth()
                            [
                                SNew(SCheckBox)
                                .Style(FAppStyle::Get(), "ToggleButtonCheckbox")
                                .IsEnabled(this, &SStormProfilePainter::HasSelectedKey)
                                .IsChecked(
                                    this,
                                    &SStormProfilePainter::GetSelectedKeySmoothStepEasingState)
                                .OnCheckStateChanged(
                                    this,
                                    &SStormProfilePainter::OnSelectedKeySmoothStepEasingChanged)
                                [
                                    SNew(STextBlock).Text(INVTEXT("Smoothstep"))
                                ]
                            ]
                        ]
                    ]
                ]
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(8.f, 4.f, 8.f, 4.f)
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    [
                        SNew(SCheckBox)
                        .Style(FAppStyle::Get(), "ToggleButtonCheckbox")
                        .IsChecked(this, &SStormProfilePainter::GetTopViewCheckState)
                        .OnCheckStateChanged(this, &SStormProfilePainter::OnTopViewCheckStateChanged)
                        [
                            SNew(STextBlock).Text(INVTEXT("Top"))
                        ]
                    ]
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .Padding(4.f, 0.f, 0.f, 0.f)
                    [
                        SNew(SCheckBox)
                        .Style(FAppStyle::Get(), "ToggleButtonCheckbox")
                        .IsChecked(this, &SStormProfilePainter::GetBottomViewCheckState)
                        .OnCheckStateChanged(this, &SStormProfilePainter::OnBottomViewCheckStateChanged)
                        [
                            SNew(STextBlock).Text(INVTEXT("Bottom"))
                        ]
                    ]
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .Padding(4.f, 0.f, 0.f, 0.f)
                    [
                        SNew(SCheckBox)
                        .Style(FAppStyle::Get(), "ToggleButtonCheckbox")
                        .ToolTipText(INVTEXT("View and paint the 2D Anvil profile. X selects Stratus, Cumulus, or Cumulonimbus; Y runs downward from the Anvil anchor."))
                        .IsChecked(this, &SStormProfilePainter::GetAnvilViewCheckState)
                        .OnCheckStateChanged(this, &SStormProfilePainter::OnAnvilViewCheckStateChanged)
                        [
                            SNew(STextBlock).Text(INVTEXT("Anvil"))
                        ]
                    ]
                ]
                + SVerticalBox::Slot()
                .FillHeight(1.f)
                .Padding(8.f)
                [
                    SNew(SBox)
                    .HAlign(HAlign_Fill)
                    .VAlign(VAlign_Fill)
                    .MinAspectRatio(1.f)
                    .MaxAspectRatio(1.f)
                    [
                        SNew(SProfilePaintSurface)
                        .OnPaintUV(this, &SStormProfilePainter::HandlePaintUV)
                        .OnStrokeBegin(this, &SStormProfilePainter::HandleStrokeBegin)
                        .OnStrokeEnd(this, &SStormProfilePainter::HandleStrokeEnd)
                        .PaintEnabled(this, &SStormProfilePainter::IsPaintingEnabled)
                        .BrushRadiusUV(this, &SStormProfilePainter::GetBrushRadiusUV)
                        .RenderTarget(this, &SStormProfilePainter::GetActiveRenderTarget)
                    ]
                ]
            ]
            + SSplitter::Slot()
            .Value(0.20f)
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(8.f, 8.f, 8.f, 6.f)
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot()
                    .FillWidth(1.f)
                    .Padding(0.f, 0.f, 4.f, 0.f)
                    [
                        SNew(SCheckBox)
                        .Style(FAppStyle::Get(), "ToggleButtonCheckbox")
                        .HAlign(HAlign_Center)
                        .IsEnabled(this, &SStormProfilePainter::HasTarget)
                        .IsChecked(this, &SStormProfilePainter::GetParameterizeModeCheckState)
                        .OnCheckStateChanged(this, &SStormProfilePainter::OnParameterizeModeCheckStateChanged)
                        [
                            SNew(STextBlock).Text(INVTEXT("Parameterize"))
                        ]
                    ]
                    + SHorizontalBox::Slot()
                    .FillWidth(1.f)
					.Padding(4.f, 0.f, 0.f, 0.f)
                    [
                        SNew(SCheckBox)
                        .Style(FAppStyle::Get(), "ToggleButtonCheckbox")
                        .HAlign(HAlign_Center)
                        .IsEnabled(this, &SStormProfilePainter::HasTarget)
                        .IsChecked(this, &SStormProfilePainter::GetPaintModeCheckState)
                        .OnCheckStateChanged(this, &SStormProfilePainter::OnPaintModeCheckStateChanged)
                        [
                            SNew(STextBlock).Text(INVTEXT("Paint"))
                        ]
                    ]
                ]
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(8.f, 0.f, 8.f, 6.f)
                [
                    SNew(STextBlock)
                    .Text(this, &SStormProfilePainter::GetStatusText)
                ]
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(8.f, 0.f, 8.f, 4.f)
                [
                    SNew(SButton)
                    .Visibility(this, &SStormProfilePainter::GetClearCanvasButtonVisibility)
                    .Text(INVTEXT("Clear Canvas"))
                    .ToolTipText(INVTEXT(
                        "Wipe the currently displayed Top or Anvil surface to an empty "
                        "canvas. Undo restores it.\n\n"
                        "To bring back the parametric profile instead, switch to "
                        "Parameterize mode, which rebuilds both surfaces from the "
                        "profile parameters."))
                    .IsEnabled(this, &SStormProfilePainter::CanClearCanvas)
                    .OnClicked(this, &SStormProfilePainter::OnClearCanvas)
                ]
                // Checkpoint is gone: the edit log makes every past state reachable,
                // so there is no separate baseline to advance.
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(8.f, 0.f, 8.f, 12.f)
                [
                    SNew(SButton)
                    .Text(INVTEXT("Revert"))
                    .ToolTipText(INVTEXT(
                        "Discard every change made to this key since it was last saved, "
                        "restoring its params and painted surfaces from the asset.\n\n"
                        "Requires a saved profile asset: a profile that has never been "
                        "saved has no stored state to restore from."))
                    .IsEnabled(this, &SStormProfilePainter::CanRevert)
                    .OnClicked(this, &SStormProfilePainter::OnRevert)
                ]
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(8.f, 0.f, 8.f, 4.f)
                [
                    SNew(SButton)
                    .Text(INVTEXT("New Profile"))
                    .ToolTipText(INVTEXT("Release the targeted asset and start a clean unsaved profile. This clears the profile editing history and cannot be undone."))
                    .IsEnabled(this, &SStormProfilePainter::HasTarget)
                    .OnClicked(this, &SStormProfilePainter::OnNewProfile)
                ]
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(8.f, 0.f, 8.f, 4.f)
                [
                    SNew(SButton)
                    .Text(INVTEXT("Save As..."))
                    .ToolTipText(INVTEXT("Save the current profile to a new Vertical Profile asset on disk and target it."))
                    .IsEnabled(this, &SStormProfilePainter::HasTarget)
                    .OnClicked(this, &SStormProfilePainter::OnSaveAs)
                ]
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(8.f, 0.f, 8.f, 4.f)
                [
                    SNew(SButton)
                    .Text(INVTEXT("Save"))
                    .ToolTipText(this, &SStormProfilePainter::GetSaveButtonTooltip)
                    .IsEnabled(this, &SStormProfilePainter::CanSaveToCurrentAsset)
                    .OnClicked(this, &SStormProfilePainter::OnSave)
                ]
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(8.f, 0.f, 8.f, 12.f)
                [
                    SNew(SButton)
                    .Text(INVTEXT("Load..."))
                    .ToolTipText(INVTEXT("Replace the working profile with a saved Vertical Profile asset and target it. This clears the current profile history and cannot be undone."))
                    .IsEnabled(this, &SStormProfilePainter::HasTarget)
                    .OnClicked(this, &SStormProfilePainter::OnLoad)
                ]
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(8.f, 0.f)
                [
                    SNew(SVerticalBox)
                    .Visibility(this, &SStormProfilePainter::GetPaintToolsVisibility)
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.f, 0.f, 0.f, 4.f)
                    [
                        SNew(STextBlock).Text(INVTEXT("Brush texels"))
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.f, 0.f, 0.f, 10.f)
                    [
                        SNew(SSpinBox<int32>)
                        .MinValue(1)
                        .MaxValue(8)
                        .Value(BrushTexels)
                        .OnValueChanged_Lambda([this](int32 V){ BrushTexels = V; })
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.f, 0.f, 0.f, 4.f)
                    [
                        SNew(STextBlock).Text(INVTEXT("Paint mode"))
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.f, 0.f, 0.f, 10.f)
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot()
                        .FillWidth(1.f)
                        .Padding(0.f, 0.f, 4.f, 0.f)
                        [
                            SNew(SCheckBox)
                            .Style(FAppStyle::Get(), "ToggleButtonCheckbox")
                            .HAlign(HAlign_Center)
                            .IsChecked(this, &SStormProfilePainter::GetBlendPaintModeCheckState)
                            .OnCheckStateChanged(this, &SStormProfilePainter::OnBlendPaintModeCheckStateChanged)
                            [
                                SNew(STextBlock).Text(INVTEXT("Blend"))
                            ]
                        ]
                        + SHorizontalBox::Slot()
                        .FillWidth(1.f)
                        [
                            SNew(SCheckBox)
                            .Style(FAppStyle::Get(), "ToggleButtonCheckbox")
                            .HAlign(HAlign_Center)
                            .IsChecked(this, &SStormProfilePainter::GetOverwritePaintModeCheckState)
                            .OnCheckStateChanged(this, &SStormProfilePainter::OnOverwritePaintModeCheckStateChanged)
                            [
                                SNew(STextBlock).Text(INVTEXT("Overwrite"))
                            ]
                        ]
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.f, 0.f, 0.f, 4.f)
                    [
                        SNew(STextBlock)
                        .IsEnabled(this, &SStormProfilePainter::IsBlendPaintModeActive)
                        .Text(INVTEXT("Brush strength"))
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.f, 0.f, 0.f, 10.f)
                    [
                        SNew(SSpinBox<float>)
                        .IsEnabled(this, &SStormProfilePainter::IsBlendPaintModeActive)
                        .MinValue(0.f)
                        .MaxValue(1.f)
                        .MinSliderValue(0.f)
                        .MaxSliderValue(1.f)
                        .Delta(0.01f)
                        .Value_Lambda([this](){ return FMath::Clamp(BrushStrength, 0.f, 1.f); })
                        .OnValueChanged_Lambda([this](float V){ BrushStrength = FMath::Clamp(V, 0.f, 1.f); })
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.f, 0.f, 0.f, 4.f)
                    [
                        SNew(STextBlock).Text(INVTEXT("Paint value"))
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.f, 0.f, 0.f, 10.f)
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot()
                        .FillWidth(1.f)
                        [
                            SNew(SSpinBox<float>)
                            .MinValue(0.f)
                            .MaxValue(1.f)
                            .MinSliderValue(0.f)
                            .MaxSliderValue(1.f)
                            .Delta(0.01f)
                            .Value_Lambda([this](){ return FMath::Clamp(BrushValue, 0.f, 1.f); })
                            .OnValueChanged_Lambda([this](float V){ BrushValue = FMath::Clamp(V, 0.f, 1.f); })
                        ]
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(8.f, 0.f, 0.f, 0.f)
                        [
                            SNew(SBorder)
                            .Padding(0.f)
                            .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                            .BorderBackgroundColor_Lambda([this]()
                            {
                                const float Value = FMath::Clamp(BrushValue, 0.f, 1.f);
                                return FLinearColor(Value, Value, Value, 1.f);
                            })
                            .ToolTipText(INVTEXT("Paint value color preview"))
                            [
                                SNew(SBox)
                                .WidthOverride(28.f)
                                .HeightOverride(22.f)
                            ]
                        ]
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(SCheckBox)
                        .OnCheckStateChanged_Lambda([this](ECheckBoxState S){ bErase = (S==ECheckBoxState::Checked); })
                        [
                            SNew(STextBlock).Text(INVTEXT("Erase"))
                        ]
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.f, 8.f, 0.f, 4.f)
                    [
                        SNew(STextBlock).Text(INVTEXT("Eraser strength"))
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.f, 0.f, 0.f, 10.f)
                    [
                        SNew(SSpinBox<float>)
                        .MinValue(0.f)
                        .MaxValue(1.f)
                        .MinSliderValue(0.f)
                        .MaxSliderValue(1.f)
                        .Delta(0.01f)
                        .Value_Lambda([this](){ return FMath::Clamp(EraserStrength, 0.f, 1.f); })
                        .OnValueChanged_Lambda([this](float V){ EraserStrength = FMath::Clamp(V, 0.f, 1.f); })
                    ]
                ]
            ]
        ];

    // Keep the editing canvas as the center column's primary surface and dock
    // the sequence controls beneath it.
    if (CenterPanel.IsValid() && TimelinePanel.IsValid())
    {
        CenterPanel->RemoveSlot(TimelinePanel.ToSharedRef());
        CenterPanel->AddSlot()
        .AutoHeight()
        .Padding(8.f, 4.f, 8.f, 8.f)
        [
            TimelinePanel.ToSharedRef()
        ];
    }

    if (GEditor)
    {
        GEditor->RegisterForUndo(this);
    }

    // Broadcast from UWorld::CleanupWorld, which EditorDestroyWorld runs before the
    // GC whose survivors it then treats as fatal leaks. That ordering is the whole
    // point: it is the last moment at which dropping a reference still counts.
    WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddSP(
        this,
        &SStormProfilePainter::HandleWorldCleanup);
}

SStormProfilePainter::~SStormProfilePainter()
{
    // Last resort for the manual stroke bracket: closing the window mid-drag
    // destroys the paint surface, and an unclosed transaction would swallow every
    // editor action that followed.
    if (bStrokeTransactionOpen)
    {
        HandleStrokeEnd();
    }
    if (GEditor)
    {
        GEditor->UnregisterForUndo(this);
    }
    if (CompositePreview.IsValid())
    {
        CompositePreview->ReleaseProfileTextures();
    }
    // AddSP no-ops once this widget is gone, but the entry would sit in a global
    // multicast delegate until the next broadcast swept it.
    FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
}

void SStormProfilePainter::HandleWorldCleanup(
    UWorld* World,
    bool bSessionEnded,
    bool bCleanupResources)
{
    (void)bSessionEnded;
    (void)bCleanupResources;

    // Only react to the world we are actually editing in. PIE start/stop and
    // preview worlds broadcast this too, and detaching on those would close the
    // painter out from under an editing session that is still perfectly valid.
    const UStormVerticalProfileToolComponent* Tool = GetTool();
    if (!World || !Tool || Tool->GetWorld() != World)
    {
        return;
    }

    // Working-set edits live in the component's render targets and die with the
    // world by design; there is nothing to save here.
    SetTarget(nullptr);
    if (CompositePreview.IsValid())
    {
        CompositePreview->ReleaseProfileTextures();
    }
}

void SStormProfilePainter::PostUndo(bool bSuccess)
{
    HandleUndoRedo(bSuccess);
}

void SStormProfilePainter::PostRedo(bool bSuccess)
{
    HandleUndoRedo(bSuccess);
}

void SStormProfilePainter::HandleUndoRedo(bool bSuccess)
{
    if (!bSuccess)
    {
        return;
    }

    UStormVerticalProfileToolComponent* Tool = GetTool();
    UStormVerticalProfileAsset* Asset =
        GetEditedProfileAsset();
    if (!Tool)
    {
        RefreshTimelineKeys();
        return;
    }

    // Asset undo can invalidate sequence indices/times while Current RTs still
    // contain a prior frame. Return to the selected key before reconciling UI.
    ExitProfileSequencePreview();

    // bParameterizeMode is transacted on the component, so undo already moved it.
    // This painter-side mirror is not, and it is what the mode checkboxes,
    // IsPaintingEnabled, and the paint-tool visibility actually read -- leaving it
    // behind makes an undone mode switch look like it did nothing while the brush
    // silently stops working.
    const EAuthoringMode RestoredMode = Tool->IsParameterizeMode()
        ? EAuthoringMode::Parameterize
        : EAuthoringMode::Paint;
    if (RestoredMode != ActiveAuthoringMode)
    {
        ActiveAuthoringMode = RestoredMode;
        if (DetailsView.IsValid())
        {
            // The authoring params are gated on bParameterizeMode through an
            // EditCondition, so the panel has to re-evaluate which of them are
            // locked. Done here rather than at the end because several paths
            // below return early.
            DetailsView->ForceRefresh();
        }
    }

    // Undo/redo restores the asset's Keys array but not the component's transient
    // workspace maps. Reconcile live sets and tombstones before anything asks
    // whether this profile is unsaved.
    TArray<FGuid> RestoredWorkspaceIds;
    const bool bDroppedActiveSet =
        Tool->ReconcileKeyRenderTargets(
            Asset,
            RestoredWorkspaceIds);

    // A stroke undo rolls a key's log back without touching the asset, so the key
    // still exists and the repair logic further down finds nothing wrong.
    // Reconcile pixels against the restored logs here instead.
    //
    // This has to run before the null-asset bail below: a profile that has never
    // been saved still has a working set and a log, and its strokes must undo just
    // like a saved profile's.
    TArray<FGuid> StaleKeys;
    Tool->CollectStaleKeys(StaleKeys);
    if (!StaleKeys.IsEmpty())
    {
        // Follow the undo to whichever key owns it: undoing a stroke the user
        // cannot see is worse than not undoing it at all. Safe to re-select from
        // inside PostUndo because selection is view state and is not transacted,
        // so this cannot recurse into the transaction system. Only meaningful when
        // an asset actually holds that key -- an unsaved profile has one working
        // set and no selection to move.
        if (Asset &&
            !StaleKeys.Contains(SelectedKeyId) &&
            VolumetricSuperStorm::ProfileEditor::FindKeyIndexById(
                Asset,
                StaleKeys[0]) != INDEX_NONE)
        {
            SelectProfileKey(StaleKeys[0]);
        }
        // Only the active key is replayed now; the rest catch up lazily when they
        // are next activated, which keeps undo independent of key count.
        Tool->ReplayActiveKeyIfStale();
        RefreshProfileDetails();
    }

    if (!Asset)
    {
        RefreshTimelineKeys();
        return;
    }
    Tool->SetProfileSequenceLooping(
        Asset->bLoopByDefault);

    // A restored structural key should become visible again. Resolve by asset
    // order rather than tombstone-map order so a multi-key transaction remains
    // deterministic.
    for (const FStormProfileKey& Key : Asset->Keys)
    {
        if (!RestoredWorkspaceIds.Contains(Key.KeyId))
        {
            continue;
        }
        SelectedKeyId.Invalidate();
        if (SelectProfileKey(Key.KeyId))
        {
            return;
        }
        break;
    }

    const bool bSelectedKeyStillExists =
        VolumetricSuperStorm::ProfileEditor::FindKeyIndexById(
            Asset,
            SelectedKeyId) != INDEX_NONE;
    if ((!bSelectedKeyStillExists || bDroppedActiveSet) &&
        !Asset->Keys.IsEmpty())
    {
        // RefreshTimelineKeys can repair the highlighted ID, but selection also
        // controls the component's active RT pointers. Route through the normal
        // selection path so the rendered profile and details panel stay aligned.
        const FGuid FallbackKeyId = bSelectedKeyStillExists
            ? SelectedKeyId
            : Asset->Keys[0].KeyId;
        SelectedKeyId.Invalidate();
        if (SelectProfileKey(FallbackKeyId))
        {
            return;
        }
        SelectedKeyId = FallbackKeyId;
    }
    else if (Asset->Keys.IsEmpty())
    {
        SelectedKeyId.Invalidate();
    }

    RefreshTimelineKeys();
    RefreshProfileDetails();
}

void SStormProfilePainter::Tick(
    const FGeometry& AllottedGeometry,
    const double InCurrentTime,
    const float InDeltaTime)
{
    SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

    UStormVerticalProfileToolComponent* Tool = GetTool();
    UWorld* World = Tool ? Tool->GetWorld() : nullptr;
    if (!Tool || !World || World->WorldType != EWorldType::Editor)
    {
        return;
    }

    TryCheckpointProfileHistory();

    if (!Tool->IsProfileSequencePlaying())
    {
        return;
    }

    if (GEditor)
    {
        GEditor->RedrawLevelEditingViewports();
    }
}

bool SStormProfilePainter::TryCheckpointProfileHistory()
{
    UStormVerticalProfileToolComponent* Tool = GetTool();
    if (!Tool || bStrokeTransactionOpen ||
        !Tool->IsProfileHistoryCheckpointRequired() ||
        !GEditor || !GEditor->Trans || GEditor->Trans->IsActive())
    {
        return false;
    }

    const bool bRebased = Tool->RebaseProfileHistoriesWithUndoBoundary(
        [Tool]()
        {
            bool bBoundaryCommitted = false;
            {
                const FScopedTransaction BoundaryTransaction(
                    INVTEXT("Checkpoint Storm Profile History"));
                if (!BoundaryTransaction.IsOutstanding())
                {
                    return false;
                }
                Tool->AdvanceProfileEditSession();
                bBoundaryCommitted = true;
            }

            if (!bBoundaryCommitted || !GEditor || !GEditor->Trans)
            {
                return false;
            }
            GEditor->Trans->SetUndoBarrier();
            return true;
        });
    if (bRebased)
    {
        // Rebasing changes only the history backing the current profile. Forcing
        // the details tree to rebuild here destroys Unreal's runtime-curve
        // customization, which also owns and closes the curve editor pop-out.
        RefreshProfilePreview();
    }
    return bRebased;
}

void SStormProfilePainter::SetTarget(UStormVerticalProfileToolComponent* InTool)
{
    if (CompositePreview.IsValid())
    {
        CompositePreview->ReleaseProfileTextures();
    }
    WeakTool = InTool;
    SelectedKeyId.Invalidate();
    if (InTool)
    {
        InTool->StopProfileSequence();
    }
    if (InTool && InTool->PersistentProfileAsset)
    {
        if (const FStormProfileKey* PrimaryKey =
                InTool->PersistentProfileAsset->GetPrimaryKey())
        {
            SelectedKeyId = PrimaryKey->KeyId;
            InTool->SeedSurfacesFromKey(*PrimaryKey);
        }
    }
    ActiveAuthoringMode = InTool && !InTool->IsParameterizeMode()
        ? EAuthoringMode::Paint
        : EAuthoringMode::Parameterize;

    if (DetailsView.IsValid())
    {
        DetailsView->SetObject(InTool);
    }

    // Bind the live authoring RT onto the cloud so paint/param edits preview in the
    // editor viewport. The profile update re-reads the (editor) getter -> live RT and
    // publishes it to the material binder. Once bound, the RT is a
    // live handle: subsequent strokes show up with no further push.
    if (InTool)
    {
        if (AVolumetricSuperStormActor* Actor = Cast<AVolumetricSuperStormActor>(InTool->GetOwner()))
        {
            Actor->RequestRenderDataUpdate(EStormRenderUpdateScope::Profile);
        }
    }

    RefreshTimelineKeys();
}

UTextureRenderTarget2D* SStormProfilePainter::GetActiveRenderTarget() const
{
    const UStormVerticalProfileToolComponent* Tool = GetTool();
    if (!Tool)
    {
        return nullptr;
    }

    switch (ActiveViewMode)
    {
    case EProfileViewMode::Top:
        return Tool->TopTypeProfileRT.Get();
    case EProfileViewMode::Anvil:
        return Tool->AnvilProfileRT.Get();
    case EProfileViewMode::Bottom:
    default:
        return Tool->BottomTypeProfileRT.Get();
    }
}

bool SStormProfilePainter::IsTopViewActive() const
{
    return ActiveViewMode == EProfileViewMode::Top;
}

bool SStormProfilePainter::IsAnvilViewActive() const
{
    return ActiveViewMode == EProfileViewMode::Anvil;
}

bool SStormProfilePainter::IsParameterizeMode() const
{
    return HasTarget() && ActiveAuthoringMode == EAuthoringMode::Parameterize;
}

bool SStormProfilePainter::IsPaintMode() const
{
    return HasTarget() && ActiveAuthoringMode == EAuthoringMode::Paint;
}

bool SStormProfilePainter::IsPaintingEnabled() const
{
    return (IsTopViewActive() || IsAnvilViewActive()) &&
        IsPaintMode() &&
        !IsProfileSequencePreviewActive();
}

bool SStormProfilePainter::IsBlendPaintModeActive() const
{
    return PaintMode == EBrushPaintMode::Blend;
}

UStormVerticalProfileAsset* SStormProfilePainter::GetEditedProfileAsset() const
{
    const UStormVerticalProfileToolComponent* Tool = GetTool();
    return Tool ? Tool->PersistentProfileAsset.Get() : nullptr;
}

int32 SStormProfilePainter::GetSelectedKeyIndex() const
{
    return VolumetricSuperStorm::ProfileEditor::FindKeyIndexById(
        GetEditedProfileAsset(),
        SelectedKeyId);
}

bool SStormProfilePainter::HasSelectedKey() const
{
    return GetSelectedKeyIndex() != INDEX_NONE;
}

bool SStormProfilePainter::CanAddProfileKey() const
{
    const UStormVerticalProfileAsset* Asset = GetEditedProfileAsset();
    const int32 KeyIndex = GetSelectedKeyIndex();
    return Asset &&
        Asset->IsKeyComplete(KeyIndex) &&
        Asset->GetKeyCount() <
            UStormVerticalProfileAsset::MaxProfileKeys;
}

bool SStormProfilePainter::CanDeleteSelectedKey() const
{
    const UStormVerticalProfileAsset* Asset = GetEditedProfileAsset();
    return Asset &&
        HasSelectedKey() &&
        Asset->GetKeyCount() > 1;
}

bool SStormProfilePainter::CanEditSelectedKeyTime() const
{
    // The first key defines the sequence origin and is always fixed at zero.
    return GetSelectedKeyIndex() > 0;
}

float SStormProfilePainter::GetSelectedKeyTime() const
{
    const UStormVerticalProfileAsset* Asset = GetEditedProfileAsset();
    const int32 KeyIndex = GetSelectedKeyIndex();
    return Asset && Asset->Keys.IsValidIndex(KeyIndex)
        ? Asset->Keys[KeyIndex].TimeSeconds
        : 0.0f;
}

ECheckBoxState SStormProfilePainter::GetSelectedKeyLinearEasingState() const
{
    const UStormVerticalProfileAsset* Asset = GetEditedProfileAsset();
    const int32 KeyIndex = GetSelectedKeyIndex();
    return Asset && Asset->Keys.IsValidIndex(KeyIndex) &&
        Asset->Keys[KeyIndex].OutgoingEasing ==
            EStormProfileTransitionEasing::Linear
        ? ECheckBoxState::Checked
        : ECheckBoxState::Unchecked;
}

ECheckBoxState SStormProfilePainter::GetSelectedKeySmoothStepEasingState() const
{
    const UStormVerticalProfileAsset* Asset = GetEditedProfileAsset();
    const int32 KeyIndex = GetSelectedKeyIndex();
    return Asset && Asset->Keys.IsValidIndex(KeyIndex) &&
        Asset->Keys[KeyIndex].OutgoingEasing ==
            EStormProfileTransitionEasing::SmoothStep
        ? ECheckBoxState::Checked
        : ECheckBoxState::Unchecked;
}

ECheckBoxState SStormProfilePainter::GetSequenceLoopState() const
{
    const UStormVerticalProfileAsset* Asset = GetEditedProfileAsset();
    return Asset && Asset->bLoopByDefault
        ? ECheckBoxState::Checked
        : ECheckBoxState::Unchecked;
}

bool SStormProfilePainter::CanPreviewProfileSequence() const
{
    const UStormVerticalProfileToolComponent* Tool = GetTool();
    return Tool && Tool->CanPlayProfileSequence();
}

bool SStormProfilePainter::IsProfileSequencePlaying() const
{
    const UStormVerticalProfileToolComponent* Tool = GetTool();
    return Tool && Tool->IsProfileSequencePlaying();
}

bool SStormProfilePainter::IsProfileSequencePreviewActive() const
{
    const UStormVerticalProfileToolComponent* Tool = GetTool();
    return Tool && Tool->IsProfileSequencePreviewActive();
}

FText SStormProfilePainter::GetProfileSequencePreviewText() const
{
    const UStormVerticalProfileToolComponent* Tool = GetTool();
    if (!Tool)
    {
        return INVTEXT("0.00 / 0.00 s");
    }

    FNumberFormattingOptions TimeFormat;
    TimeFormat.MinimumFractionalDigits = 2;
    TimeFormat.MaximumFractionalDigits = 2;
    return FText::Format(
        INVTEXT("{0} / {1} s"),
        FText::AsNumber(Tool->GetProfileSequenceTime(), &TimeFormat),
        FText::AsNumber(Tool->GetProfileSequenceDuration(), &TimeFormat));
}

float SStormProfilePainter::GetBrushRadiusUV() const
{
    const UStormVerticalProfileToolComponent* Tool = GetTool();
    if (!Tool)
    {
        return 0.f;
    }

    return static_cast<float>(BrushTexels) /
        static_cast<float>(VolumetricSuperStorm::RenderTargetResolution::Profile);
}

UTexture* SStormProfilePainter::GetDisplayTopProfile() const
{
    const UStormVerticalProfileToolComponent* Tool = GetTool();
    if (!Tool)
    {
        return nullptr;
    }

    FStormProfileRenderData RenderData;
    Tool->GetProfileRenderData(RenderData);
    return RenderData.TopProfile;
}

UTexture* SStormProfilePainter::GetDisplayBottomProfile() const
{
    const UStormVerticalProfileToolComponent* Tool = GetTool();
    if (!Tool)
    {
        return nullptr;
    }

    FStormProfileRenderData RenderData;
    Tool->GetProfileRenderData(RenderData);
    return RenderData.BottomProfile;
}

UTexture* SStormProfilePainter::GetDisplayAnvilProfile() const
{
    const UStormVerticalProfileToolComponent* Tool = GetTool();
    if (!Tool)
    {
        return nullptr;
    }

    FStormProfileRenderData RenderData;
    Tool->GetProfileRenderData(RenderData);
    return RenderData.AnvilProfile;
}

EVisibility SStormProfilePainter::GetPaintToolsVisibility() const
{
    return IsPaintingEnabled() ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SStormProfilePainter::GetClearCanvasButtonVisibility() const
{
    return IsPaintMode() ? EVisibility::Visible : EVisibility::Collapsed;
}

bool SStormProfilePainter::CanClearCanvas() const
{
    const UStormVerticalProfileToolComponent* Tool = GetTool();
    return Tool && IsPaintingEnabled() &&
        Tool->HasClearableCanvas(IsAnvilViewActive());
}

bool SStormProfilePainter::CanRevert() const
{
    const UStormVerticalProfileToolComponent* Tool = GetTool();
    return Tool && Tool->PersistentProfileAsset && Tool->IsActiveKeyModified();
}

FText SStormProfilePainter::GetStatusText() const
{
    const UStormVerticalProfileToolComponent* Tool = GetTool();
    if (!Tool)
    {
        return FText::GetEmpty();
    }
    if (Tool->IsProfileSequencePreviewActive())
    {
        FNumberFormattingOptions TimeFormat;
        TimeFormat.MinimumFractionalDigits = 2;
        TimeFormat.MaximumFractionalDigits = 2;
        const bool bPlayingReverse =
            Tool->IsProfileSequencePlaying() &&
            Tool->GetProfileSequencePlaybackDirection() ==
                EStormProfileSequencePlaybackDirection::Reverse;
        return FText::Format(
            Tool->IsProfileSequencePlaying()
                ? (bPlayingReverse
                    ? INVTEXT("Sequence playing reverse - {0}s")
                    : INVTEXT("Sequence playing forward - {0}s"))
                : INVTEXT("Sequence paused - {0}s"),
            FText::AsNumber(
                Tool->GetProfileSequenceTime(),
                &TimeFormat));
    }
    // A scratch document is baselined against the parameters that built it, so it
    // reports saved once untouched. That is the right answer for the close prompt
    // and the wrong one to display: there is no asset it could be saved to. The
    // two "unsaved" strings below describe a document relative to its asset, and
    // neither applies until one exists.
    if (!Tool->PersistentProfileAsset)
    {
        return INVTEXT("Unsaved - no asset");
    }
    if (Tool->IsUnsavedToAsset())
    {
        return Tool->IsActiveKeyModified()
            ? INVTEXT("Unsaved - pending changes")
            : INVTEXT("Unsaved - retained key changes");
    }
    return INVTEXT("Saved");
}

ECheckBoxState SStormProfilePainter::GetTopViewCheckState() const
{
    return IsTopViewActive() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

ECheckBoxState SStormProfilePainter::GetBottomViewCheckState() const
{
    return ActiveViewMode == EProfileViewMode::Bottom ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

ECheckBoxState SStormProfilePainter::GetAnvilViewCheckState() const
{
    return IsAnvilViewActive() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

ECheckBoxState SStormProfilePainter::GetBlendPaintModeCheckState() const
{
    return PaintMode == EBrushPaintMode::Blend ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

ECheckBoxState SStormProfilePainter::GetOverwritePaintModeCheckState() const
{
    return PaintMode == EBrushPaintMode::Overwrite ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SStormProfilePainter::OnTopViewCheckStateChanged(ECheckBoxState NewState)
{
    if (NewState == ECheckBoxState::Checked)
    {
        ActiveViewMode = EProfileViewMode::Top;
    }
}

void SStormProfilePainter::OnBottomViewCheckStateChanged(ECheckBoxState NewState)
{
    if (NewState == ECheckBoxState::Checked)
    {
        ActiveViewMode = EProfileViewMode::Bottom;
    }
}

void SStormProfilePainter::OnAnvilViewCheckStateChanged(ECheckBoxState NewState)
{
    if (NewState == ECheckBoxState::Checked)
    {
        ActiveViewMode = EProfileViewMode::Anvil;
    }
}

ECheckBoxState SStormProfilePainter::GetParameterizeModeCheckState() const
{
    return IsParameterizeMode() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

ECheckBoxState SStormProfilePainter::GetPaintModeCheckState() const
{
    return IsPaintMode() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SStormProfilePainter::OnParameterizeModeCheckStateChanged(ECheckBoxState NewState)
{
    // Only react to the button being turned on; the destructive switch is gated by a warning.
    if (NewState != ECheckBoxState::Checked)
    {
        return;
    }
    if (UStormVerticalProfileToolComponent* Tool = GetTool())
    {
        if (!Tool->IsParameterizeMode())
        {
            RequestEnterParameterizeMode();
        }
        if (Tool->IsParameterizeMode())
        {
            ActiveAuthoringMode = EAuthoringMode::Parameterize;
        }
    }
}

void SStormProfilePainter::OnPaintModeCheckStateChanged(ECheckBoxState NewState)
{
    if (NewState != ECheckBoxState::Checked)
    {
        return;
    }
    if (UStormVerticalProfileToolComponent* Tool = GetTool())
    {
        if (Tool->IsParameterizeMode())
        {
            // Entering Paint is non-destructive: the current macro becomes the paint base.
            Tool->SetParameterizeMode(false);
            if (DetailsView.IsValid())
            {
                DetailsView->ForceRefresh(); // grey the now-locked params
            }
        }
        ActiveAuthoringMode = EAuthoringMode::Paint;
    }
}

void SStormProfilePainter::RequestEnterParameterizeMode()
{
    UStormVerticalProfileToolComponent* Tool = GetTool();
    if (!Tool)
    {
        return;
    }

    // Warn only when there is brushwork to lose and the user hasn't suppressed it this session.
    if (Tool->HasPaintedProfiles() && !GbSuppressParamModeWarning)
    {
        bool bDontShowAgain = false;
        if (!ShowSwitchToParameterizeWarning(bDontShowAgain))
        {
            return; // cancelled -- stay in Paint mode (checkbox re-reads the real mode)
        }
        if (bDontShowAgain)
        {
            GbSuppressParamModeWarning = true;
        }
    }

    {
        // Records a reseed op, so it needs its own transaction like any other log
        // mutation. Entering Paint mode does not touch the log and so needs none.
        const FScopedTransaction Transaction(INVTEXT("Enter Parameterize Mode"));
        Tool->Modify();
        Tool->SetParameterizeMode(true); // reseeds the top from params (discards paint)
    }
    if (DetailsView.IsValid())
    {
        DetailsView->ForceRefresh(); // unlock the params
    }
}

void SStormProfilePainter::OnBlendPaintModeCheckStateChanged(ECheckBoxState NewState)
{
    if (NewState == ECheckBoxState::Checked)
    {
        PaintMode = EBrushPaintMode::Blend;
    }
}

void SStormProfilePainter::OnOverwritePaintModeCheckStateChanged(ECheckBoxState NewState)
{
    if (NewState == ECheckBoxState::Checked)
    {
        PaintMode = EBrushPaintMode::Overwrite;
    }
}

void SStormProfilePainter::ExitProfileSequencePreview()
{
    if (UStormVerticalProfileToolComponent* Tool = GetTool())
    {
        Tool->StopProfileSequence();
    }
    if (GEditor)
    {
        GEditor->RedrawLevelEditingViewports();
    }
}

void SStormProfilePainter::OnSelectedKeyTimeCommitted(
    float NewTime,
    ETextCommit::Type CommitType)
{
    (void)CommitType;
    UStormVerticalProfileAsset* Asset = GetEditedProfileAsset();
    const int32 KeyIndex = GetSelectedKeyIndex();
    if (!Asset || !Asset->Keys.IsValidIndex(KeyIndex))
    {
        return;
    }
    if (KeyIndex == 0)
    {
        // Keep the invariant even if this handler is invoked without the
        // disabled first-key spin box.
        return;
    }

    if (!FMath::IsFinite(NewTime))
    {
        FMessageDialog::Open(
            EAppMsgType::Ok,
            INVTEXT("Profile key time must be finite."));
        return;
    }
    // Covers both writers: the spin box bounds itself, but a timeline key drag
    // commits through here too and its ceiling is the widget's view range.
    const float SanitizedTime = FMath::Clamp(
        NewTime,
        0.0f,
        UStormVerticalProfileAsset::MaxKeyTimeSeconds);
    if (FMath::IsNearlyEqual(
            Asset->Keys[KeyIndex].TimeSeconds,
            SanitizedTime))
    {
        return;
    }

    for (int32 OtherIndex = 0;
        OtherIndex < Asset->Keys.Num();
        ++OtherIndex)
    {
        if (OtherIndex != KeyIndex &&
            FMath::IsNearlyEqual(
                Asset->Keys[OtherIndex].TimeSeconds,
                SanitizedTime))
        {
            FMessageDialog::Open(
                EAppMsgType::Ok,
                INVTEXT("Two profile keys cannot have the same time."));
            return;
        }
    }

    const FScopedTransaction Transaction(
        INVTEXT("Change Storm Profile Key Time"));
    Asset->Modify();
    Asset->Keys[KeyIndex].TimeSeconds = SanitizedTime;
    Asset->Keys.Sort(
        [](const FStormProfileKey& A, const FStormProfileKey& B)
        {
            return A.TimeSeconds < B.TimeSeconds;
        });
    Asset->PostEditChange();
    Asset->MarkPackageDirty();
    RefreshTimelineKeys();
    if (UStormVerticalProfileToolComponent* Tool = GetTool();
        Tool && Tool->IsProfileSequencePreviewActive())
    {
        Tool->SetProfileSequenceTime(
            Tool->GetProfileSequenceTime());
    }
}

void SStormProfilePainter::OnSelectedKeyLinearEasingChanged(
    ECheckBoxState NewState)
{
    if (NewState != ECheckBoxState::Checked)
    {
        return;
    }

    UStormVerticalProfileAsset* Asset = GetEditedProfileAsset();
    const int32 KeyIndex = GetSelectedKeyIndex();
    if (!Asset || !Asset->Keys.IsValidIndex(KeyIndex) ||
        Asset->Keys[KeyIndex].OutgoingEasing ==
            EStormProfileTransitionEasing::Linear)
    {
        return;
    }

    const FScopedTransaction Transaction(
        INVTEXT("Change Storm Profile Key Easing"));
    Asset->Modify();
    Asset->Keys[KeyIndex].OutgoingEasing =
        EStormProfileTransitionEasing::Linear;
    Asset->PostEditChange();
    Asset->MarkPackageDirty();
    if (UStormVerticalProfileToolComponent* Tool = GetTool();
        Tool && Tool->IsProfileSequencePreviewActive())
    {
        Tool->SetProfileSequenceTime(
            Tool->GetProfileSequenceTime());
    }
}

void SStormProfilePainter::OnSelectedKeySmoothStepEasingChanged(
    ECheckBoxState NewState)
{
    if (NewState != ECheckBoxState::Checked)
    {
        return;
    }

    UStormVerticalProfileAsset* Asset = GetEditedProfileAsset();
    const int32 KeyIndex = GetSelectedKeyIndex();
    if (!Asset || !Asset->Keys.IsValidIndex(KeyIndex) ||
        Asset->Keys[KeyIndex].OutgoingEasing ==
            EStormProfileTransitionEasing::SmoothStep)
    {
        return;
    }

    const FScopedTransaction Transaction(
        INVTEXT("Change Storm Profile Key Easing"));
    Asset->Modify();
    Asset->Keys[KeyIndex].OutgoingEasing =
        EStormProfileTransitionEasing::SmoothStep;
    Asset->PostEditChange();
    Asset->MarkPackageDirty();
    if (UStormVerticalProfileToolComponent* Tool = GetTool();
        Tool && Tool->IsProfileSequencePreviewActive())
    {
        Tool->SetProfileSequenceTime(
            Tool->GetProfileSequenceTime());
    }
}

void SStormProfilePainter::OnSequenceLoopChanged(ECheckBoxState NewState)
{
    UStormVerticalProfileAsset* Asset = GetEditedProfileAsset();
    if (!Asset ||
        !VolumetricSuperStorm::AuthoringPaths::CanOverwriteAsset(
            Asset,
            VolumetricSuperStorm::AuthoringPaths::ECategory::VerticalProfile))
    {
        return;
    }

    const bool bLoop = NewState == ECheckBoxState::Checked;
    if (Asset->bLoopByDefault == bLoop)
    {
        return;
    }

    const FScopedTransaction Transaction(
        INVTEXT("Change Storm Profile Looping"));
    Asset->Modify();
    Asset->bLoopByDefault = bLoop;
    Asset->PostEditChange();
    Asset->MarkPackageDirty();

    if (UStormVerticalProfileToolComponent* Tool = GetTool())
    {
        Tool->SetProfileSequenceLooping(bLoop);
    }
}

void SStormProfilePainter::OnTimelineKeySelected(FGuid KeyId)
{
    SelectProfileKey(KeyId);
}

void SStormProfilePainter::OnTimelineKeyTimeCommitted(
    FGuid KeyId,
    float NewTime)
{
    if (SelectedKeyId != KeyId &&
        !SelectProfileKey(KeyId))
    {
        return;
    }

    OnSelectedKeyTimeCommitted(
        NewTime,
        ETextCommit::Default);
}

void SStormProfilePainter::OnTimelineScrubbed(
    float TimeSeconds)
{
    UStormVerticalProfileToolComponent* Tool = GetTool();
    if (!Tool)
    {
        return;
    }

    Tool->SetProfileSequenceTime(TimeSeconds);
    if (GEditor)
    {
        GEditor->RedrawLevelEditingViewports();
    }
}

void SStormProfilePainter::OnTimelineAddKeyRequested(
    float TimeSeconds)
{
    DuplicateSelectedKey(TimeSeconds);
}

void SStormProfilePainter::HandlePaintUV(FVector2D UV)
{
    if (!IsPaintingEnabled())
    {
        return;
    }

    if (UStormVerticalProfileToolComponent* Tool = GetTool())
    {
        const float RadiusUV = GetBrushRadiusUV();
        const float Strength = bErase ? EraserStrength : BrushStrength;
        const bool bOverwrite = !bErase && PaintMode == EBrushPaintMode::Overwrite;
        Tool->StampBrush(
            UV, RadiusUV, Strength, BrushValue, bErase, bOverwrite, IsAnvilViewActive());
        if (GEditor)
        {
            GEditor->RedrawLevelEditingViewports();
        }
    }
}

void SStormProfilePainter::HandleStrokeBegin()
{
    UStormVerticalProfileToolComponent* Tool = GetTool();
    if (!Tool || !GEditor)
    {
        return;
    }

    // One transaction spans the whole drag, not each dab. The immutable graph can
    // record every dab while the transaction snapshots the bookmark once, so
    // Ctrl+Z returns to the state before the complete stroke.
    //
    // Bracketed manually rather than with FScopedTransaction because the drag
    // outlives this call by many frames. bStrokeTransactionOpen is what keeps the
    // manual bracket honest, since the two halves no longer share a scope.
    GEditor->BeginTransaction(INVTEXT("Paint Profile Stroke"));
    bStrokeTransactionOpen = true;
    Tool->BeginStroke();
}

void SStormProfilePainter::HandleStrokeEnd()
{
    // Closing the transaction is deliberately not conditional on the tool: an
    // actor deleted mid-drag would otherwise leave the stroke's transaction open,
    // and every later editor action would be folded into it.
    if (UStormVerticalProfileToolComponent* Tool = GetTool())
    {
        Tool->EndStroke();
    }
    if (bStrokeTransactionOpen && GEditor)
    {
        GEditor->EndTransaction();
    }
    bStrokeTransactionOpen = false;
}

FReply SStormProfilePainter::OnClearCanvas()
{
    if (UStormVerticalProfileToolComponent* Tool = GetTool();
        Tool && CanClearCanvas())
    {
        // Every log mutation needs its own transaction. An untransacted one would
        // still move the playhead, so the next Ctrl+Z would roll the log back past
        // it and appear to undo two actions at once.
        const FScopedTransaction Transaction(INVTEXT("Clear Profile Canvas"));
        Tool->ClearCanvas(IsAnvilViewActive());
    }
    return FReply::Handled();
}

FReply SStormProfilePainter::OnRevert()
{
    if (UStormVerticalProfileToolComponent* Tool = GetTool();
        Tool && CanRevert())
    {
        // Revert moves the playhead, so it is itself an undoable action.
        const FScopedTransaction Transaction(INVTEXT("Revert Profile Key"));
        Tool->RevertActiveKeyToSaved();
        // Params were written back on the component; refresh the details panel to match.
        if (DetailsView.IsValid())
        {
            DetailsView->ForceRefresh();
        }
    }
    return FReply::Handled();
}

void SStormProfilePainter::RefreshTimelineKeys()
{
    const UStormVerticalProfileAsset* Asset =
        GetEditedProfileAsset();
    if (!Asset)
    {
        SelectedKeyId.Invalidate();
        if (TimelineWidget.IsValid())
        {
            TimelineWidget->Invalidate(
                EInvalidateWidgetReason::Paint);
        }
        return;
    }
    if (VolumetricSuperStorm::ProfileEditor::FindKeyIndexById(
            Asset,
            SelectedKeyId) == INDEX_NONE &&
        !Asset->Keys.IsEmpty())
    {
        SelectedKeyId = Asset->Keys[0].KeyId;
    }
    if (TimelineWidget.IsValid())
    {
        TimelineWidget->Invalidate(
            EInvalidateWidgetReason::Paint);
    }
}

void SStormProfilePainter::RefreshProfilePreview()
{
    if (UStormVerticalProfileToolComponent* Tool = GetTool())
    {
        if (AVolumetricSuperStormActor* Actor =
                Cast<AVolumetricSuperStormActor>(Tool->GetOwner()))
        {
            Actor->RequestRenderDataUpdate(EStormRenderUpdateScope::Profile);
        }
    }

    if (GEditor)
    {
        GEditor->RedrawLevelEditingViewports();
    }
}

void SStormProfilePainter::RefreshProfileDetails()
{
    if (DetailsView.IsValid())
    {
        DetailsView->ForceRefresh();
    }

    RefreshProfilePreview();
}

bool SStormProfilePainter::CommitAllKeys()
{
    UStormVerticalProfileToolComponent* Tool = GetTool();
    UStormVerticalProfileAsset* Asset =
        GetEditedProfileAsset();
    if (!Tool || !Asset)
    {
        return false;
    }

    if (!VolumetricSuperStorm::ProfileEditor::CommitToolToAllKeys(
            Tool,
            Asset,
            SelectedKeyId))
    {
        return false;
    }

    // CommitToolToAllKeys already moved each committed key's saved marker via
    // MarkKeyRenderTargetsSaved, so there is nothing further to checkpoint.
    RefreshTimelineKeys();
    return true;
}

bool SStormProfilePainter::PrepareToReplaceProfileDocument(
    const FText& DialogTitle,
    const FText& Prompt)
{
    UStormVerticalProfileToolComponent* Tool = GetTool();
    if (!Tool || !Tool->IsUnsavedToAsset())
    {
        return true;
    }

    const EAppReturnType::Type Choice =
        FMessageDialog::Open(
            EAppMsgType::YesNoCancel,
            Prompt,
            DialogTitle);
    if (Choice == EAppReturnType::Cancel)
    {
        return false;
    }
    if (Choice == EAppReturnType::Yes)
    {
        if (!Tool->PersistentProfileAsset)
        {
            OnSaveAs();
            return !Tool->IsUnsavedToAsset();
        }
        return CommitAllKeys();
    }
    return true;
}

bool SStormProfilePainter::SelectProfileKey(FGuid KeyId)
{
    UStormVerticalProfileToolComponent* Tool = GetTool();
    UStormVerticalProfileAsset* Asset =
        GetEditedProfileAsset();
    const int32 KeyIndex =
        VolumetricSuperStorm::ProfileEditor::FindKeyIndexById(
            Asset,
            KeyId);
    if (!Tool || !Asset ||
        !Asset->Keys.IsValidIndex(KeyIndex))
    {
        return false;
    }
    if (SelectedKeyId == KeyId)
    {
        ExitProfileSequencePreview();
        return true;
    }
    ExitProfileSequencePreview();
    if (!Tool->SeedSurfacesFromKey(
            Asset->Keys[KeyIndex]))
    {
        return false;
    }

    SelectedKeyId = KeyId;
    Tool->PersistentProfileAsset = Asset;
    RefreshTimelineKeys();
    RefreshProfileDetails();
    return true;
}

FReply SStormProfilePainter::OnPlayProfileSequence()
{
    if (UStormVerticalProfileToolComponent* Tool = GetTool())
    {
        Tool->PlayProfileSequence();
    }
    return FReply::Handled();
}

FReply SStormProfilePainter::OnPlayProfileSequenceReverse()
{
    if (UStormVerticalProfileToolComponent* Tool = GetTool())
    {
        Tool->PlayProfileSequenceReverse();
    }
    return FReply::Handled();
}

FReply SStormProfilePainter::OnPauseProfileSequence()
{
    if (UStormVerticalProfileToolComponent* Tool = GetTool())
    {
        Tool->PauseProfileSequence();
    }
    return FReply::Handled();
}

FReply SStormProfilePainter::OnStopProfileSequence()
{
    ExitProfileSequencePreview();
    return FReply::Handled();
}

FReply SStormProfilePainter::OnAddProfileKey()
{
    DuplicateSelectedKey();
    return FReply::Handled();
}

bool SStormProfilePainter::DuplicateSelectedKey(
    TOptional<float> RequestedTime)
{
    UStormVerticalProfileToolComponent* Tool = GetTool();
    UStormVerticalProfileAsset* Asset =
        GetEditedProfileAsset();
    int32 SourceIndex = GetSelectedKeyIndex();
    if (!Tool || !Asset ||
        !Asset->Keys.IsValidIndex(SourceIndex))
    {
        return false;
    }
    if (!EnsureEditableProfileAsset())
    {
        return false;
    }
    // A fork retargets the tool at the copy. Key ids survive CopyKeysToAsset, so the
    // selection still resolves -- but against a different asset and index.
    Asset = GetEditedProfileAsset();
    SourceIndex = GetSelectedKeyIndex();
    if (!Asset || !Asset->Keys.IsValidIndex(SourceIndex))
    {
        return false;
    }
    ExitProfileSequencePreview();

    const FGuid SourceKeyId =
        Asset->Keys[SourceIndex].KeyId;
    FGuid NewKeyId;
    const bool bDuplicated = RequestedTime.IsSet()
        ? VolumetricSuperStorm::ProfileEditor::DuplicateKeyAtTime(
            Asset,
            SourceIndex,
            RequestedTime.GetValue(),
            NewKeyId)
        : VolumetricSuperStorm::ProfileEditor::DuplicateKeyAfter(
            Asset,
            SourceIndex,
            NewKeyId);
    if (!bDuplicated)
    {
        FMessageDialog::Open(
            EAppMsgType::Ok,
            INVTEXT("Could not duplicate the selected profile key."));
        return false;
    }

    const int32 NewKeyIndex =
        VolumetricSuperStorm::ProfileEditor::FindKeyIndexById(
            Asset,
            NewKeyId);
    if (!Asset->Keys.IsValidIndex(NewKeyIndex) ||
        !Tool->DuplicateKeyRenderTargets(
            SourceKeyId,
            Asset->Keys[NewKeyIndex]))
    {
        FMessageDialog::Open(
            EAppMsgType::Ok,
            INVTEXT(
                "The key was created, but its live render targets could not be duplicated."));
        return false;
    }
    return SelectProfileKey(NewKeyId);
}

FReply SStormProfilePainter::OnDeleteSelectedKey()
{
    UStormVerticalProfileToolComponent* Tool = GetTool();
    UStormVerticalProfileAsset* Asset =
        GetEditedProfileAsset();
    int32 KeyIndex = GetSelectedKeyIndex();
    if (!Tool || !Asset ||
        !Asset->Keys.IsValidIndex(KeyIndex) ||
        Asset->GetKeyCount() <= 1)
    {
        return FReply::Handled();
    }

    const EAppReturnType::Type Choice =
        FMessageDialog::Open(
            EAppMsgType::YesNo,
            INVTEXT(
                "Delete the selected profile key?\n\n"
                "Undo will restore the key with its live edits."),
            INVTEXT("Delete Profile Key"));
    if (Choice != EAppReturnType::Yes)
    {
        return FReply::Handled();
    }

    // Confirm the intent first, then resolve where it lands: a shipped profile forks
    // into the authoring root and the delete applies to the copy.
    if (!EnsureEditableProfileAsset())
    {
        return FReply::Handled();
    }
    Asset = GetEditedProfileAsset();
    KeyIndex = GetSelectedKeyIndex();
    if (!Asset || !Asset->Keys.IsValidIndex(KeyIndex))
    {
        return FReply::Handled();
    }

    ExitProfileSequencePreview();
    const FGuid RemovedKeyId = Asset->Keys[KeyIndex].KeyId;
    if (!VolumetricSuperStorm::ProfileEditor::RemoveKey(
            Asset,
            KeyIndex))
    {
        return FReply::Handled();
    }

    const int32 ReplacementIndex =
        FMath::Min(KeyIndex, Asset->Keys.Num() - 1);
    Tool->TombstoneKeyRenderTargets(RemovedKeyId);
    SelectedKeyId.Invalidate();
    SelectProfileKey(Asset->Keys[ReplacementIndex].KeyId);
    return FReply::Handled();
}

FReply SStormProfilePainter::OnNewProfile()
{
    UStormVerticalProfileToolComponent* Tool = GetTool();
    if (!Tool)
    {
        return FReply::Handled();
    }

    if (!PrepareToReplaceProfileDocument(
            INVTEXT("New Vertical Profile"),
            INVTEXT(
                "The current profile has live key edits.\n\n"
                "Save them before starting a new profile?")))
    {
        return FReply::Handled();
    }

    ExitProfileSequencePreview();

    // New Profile is an irreversible document boundary. A real transaction is
    // required first so beginning it discards an existing redo tail; placing the
    // barrier after it prevents Ctrl-Z from consuming this marker or any
    // transactions belonging to the released profile.
    if (GEditor && GEditor->Trans)
    {
        {
            const FScopedTransaction BoundaryTransaction(
                INVTEXT("Start New Storm Profile"));
            Tool->AdvanceProfileEditSession();
        }
        GEditor->Trans->SetUndoBarrier();
    }

    SelectedKeyId.Invalidate();
    ActiveAuthoringMode = EAuthoringMode::Parameterize;
    const bool bInitialized = Tool->ResetToNewProfile();
    RefreshTimelineKeys();
    RefreshProfileDetails();
    if (!bInitialized)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("Could not initialize a new vertical profile editing session."));
    }
    return FReply::Handled();
}

FReply SStormProfilePainter::OnSaveAs()
{
    UStormVerticalProfileToolComponent* Tool = GetTool();
    if (!Tool)
    {
        return FReply::Handled();
    }
    ExitProfileSequencePreview();

    FString ProfilePackageName;
    FString ProfileAssetName;
    if (!VolumetricSuperStorm::AuthoringPaths::PickSaveNameModal(
            VolumetricSuperStorm::AuthoringPaths::ECategory::VerticalProfile,
            UStormVerticalProfileAsset::StaticClass(),
            TEXT("VP_StormVerticalProfile"),
            INVTEXT("Save Vertical Profile As"),
            ProfilePackageName,
            ProfileAssetName))
    {
        return FReply::Handled();
    }

    UStormVerticalProfileAsset* ProfileAsset =
        CreateOrLoadProfileAsset(
            ProfilePackageName,
            ProfileAssetName);
    if (!ProfileAsset)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("Could not create or open vertical profile asset: %s"),
            *ProfilePackageName);
        return FReply::Handled();
    }

    // Save As onto the asset already open is a plain Save. CopyKeysToAsset refuses a
    // self-copy, and the tool is already targeting this asset, so there is nothing to
    // retarget -- only the working sets need baking.
    if (ProfileAsset == Tool->PersistentProfileAsset)
    {
        if (!CommitAllKeys())
        {
            UE_LOG(LogTemp, Warning,
                TEXT("Failed to save vertical profile asset: %s"),
                *ProfilePackageName);
        }
        return FReply::Handled();
    }

    const UStormVerticalProfileAsset* SourceAsset =
        Tool->PersistentProfileAsset;
    int32 DestinationKeyIndex = INDEX_NONE;
    if (SourceAsset)
    {
        if (!VolumetricSuperStorm::ProfileEditor::CopyKeysToAsset(
                SourceAsset,
                ProfileAsset))
        {
            UE_LOG(LogTemp, Warning,
                TEXT("Failed to copy profile timeline into asset: %s"),
                *ProfilePackageName);
            return FReply::Handled();
        }

        DestinationKeyIndex =
            VolumetricSuperStorm::ProfileEditor::FindKeyIndexById(
                ProfileAsset,
                SelectedKeyId);
    }
    else
    {
        ProfileAsset->Modify();
        FStormProfileKey& PrimaryKey =
            ProfileAsset->Keys.AddDefaulted_GetRef();
        // Adopt the tool's working-set id instead of minting a fresh one. This is
        // the first save of a scratch profile, and its surfaces and edit history
        // live under that id -- a new id here would leave the key matching no
        // working set at all, so CommitToolToAllKeys (which filters on
        // HasKeyRenderTargets) would skip it and bake an empty asset.
        const FGuid ActiveKeyId = Tool->GetActiveKeyId();
        PrimaryKey.KeyId =
            ActiveKeyId.IsValid() ? ActiveKeyId : FGuid::NewGuid();
        PrimaryKey.TimeSeconds = 0.0f;
        DestinationKeyIndex = 0;
    }

    if (!ProfileAsset->Keys.IsValidIndex(DestinationKeyIndex))
    {
        DestinationKeyIndex = 0;
    }

    if (VolumetricSuperStorm::ProfileEditor::CommitToolToAllKeys(
            Tool,
            ProfileAsset,
            SelectedKeyId))
    {
        SelectedKeyId =
            ProfileAsset->Keys[DestinationKeyIndex].KeyId;
        if (!Tool->HasKeyRenderTargets(SelectedKeyId))
        {
            Tool->SeedSurfacesFromKey(
                ProfileAsset->Keys[DestinationKeyIndex]);
        }
        // All resident working sets now match the new asset's baked keys; each key's
        // saved marker was moved as it was committed.
        RefreshTimelineKeys();
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to save vertical profile asset: %s"), *ProfilePackageName);
    }

    return FReply::Handled();
}

bool SStormProfilePainter::EnsureEditableProfileAsset()
{
    UStormVerticalProfileAsset* Original = GetEditedProfileAsset();
    if (!Original)
    {
        // A scratch profile has no asset to protect yet; the caller's own null
        // handling decides what happens next.
        return true;
    }
    if (VolumetricSuperStorm::AuthoringPaths::CanOverwriteAsset(
            Original,
            VolumetricSuperStorm::AuthoringPaths::ECategory::VerticalProfile))
    {
        return true;
    }

    const EAppReturnType::Type Choice = FMessageDialog::Open(
        EAppMsgType::YesNo,
        INVTEXT(
            "Modifying a sequencer timeline requires a saved, non-plugin default asset.\n\n"
            "Create an editable copy and make this change there?"),
        INVTEXT("Plugin Default Profile"));
    if (Choice != EAppReturnType::Yes)
    {
        return false;
    }

    // Save As already is the fork: it copies the timeline into the chosen asset and
    // CommitToolToKey reassigns PersistentProfileAsset to it. It reports failure only
    // through the log, so confirm the retarget landed rather than trusting the call.
    OnSaveAs();

    const UStormVerticalProfileAsset* Forked = GetEditedProfileAsset();
    return Forked != Original &&
        VolumetricSuperStorm::AuthoringPaths::CanOverwriteAsset(
            Forked,
            VolumetricSuperStorm::AuthoringPaths::ECategory::VerticalProfile);
}

FReply SStormProfilePainter::OnSave()
{
    UStormVerticalProfileToolComponent* Tool = GetTool();
    if (!Tool)
    {
        return FReply::Handled();
    }
    ExitProfileSequencePreview();

    if (!Tool->PersistentProfileAsset)
    {
        return OnSaveAs();
    }

    if (CommitAllKeys())
    {
        // Every resident key working set now matches its baked textures.
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to save vertical profile asset: %s"), *GetNameSafe(Tool->PersistentProfileAsset));
    }

    return FReply::Handled();
}

bool SStormProfilePainter::CanSaveToCurrentAsset() const
{
    const UStormVerticalProfileToolComponent* Tool = GetTool();
    if (!Tool)
    {
        return false;
    }

    return !Tool->PersistentProfileAsset ||
        VolumetricSuperStorm::AuthoringPaths::CanOverwriteAsset(
            Tool->PersistentProfileAsset,
            VolumetricSuperStorm::AuthoringPaths::ECategory::VerticalProfile);
}

FText SStormProfilePainter::GetSaveButtonTooltip() const
{
    const UStormVerticalProfileToolComponent* Tool = GetTool();
    if (Tool && Tool->PersistentProfileAsset &&
        !VolumetricSuperStorm::AuthoringPaths::CanOverwriteAsset(
            Tool->PersistentProfileAsset,
            VolumetricSuperStorm::AuthoringPaths::ECategory::VerticalProfile))
    {
        return VolumetricSuperStorm::AuthoringPaths::GetShippedAssetOverwriteBlockedText();
    }

    return INVTEXT(
        "Save into the targeted Vertical Profile asset "
        "(falls back to Save As... when none is set).");
}

FReply SStormProfilePainter::OnLoad()
{
    UStormVerticalProfileToolComponent* Tool = GetTool();
    if (!Tool)
    {
        return FReply::Handled();
    }

    UStormVerticalProfileAsset* ProfileAsset =
        VolumetricSuperStorm::AuthoringPaths::PickAssetModal<UStormVerticalProfileAsset>(
            VolumetricSuperStorm::AuthoringPaths::ECategory::VerticalProfile,
            INVTEXT("Load Vertical Profile"));
    if (!ProfileAsset)
    {
        return FReply::Handled(); // cancelled
    }
    const FStormProfileKey* PrimaryKey = ProfileAsset->GetPrimaryKey();
    if (!PrimaryKey || !PrimaryKey->TopProfile || !PrimaryKey->BottomProfile ||
        !PrimaryKey->AnvilProfile)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("Selected vertical profile asset has no complete primary key."));
        return FReply::Handled();
    }
    if (!PrepareToReplaceProfileDocument(
            INVTEXT("Load Vertical Profile"),
            INVTEXT(
                "The current profile has live key edits.\n\n"
                "Save them before loading another profile?")))
    {
        return FReply::Handled();
    }

    ExitProfileSequencePreview();

    // Loading an asset is a profile-document replacement, not an edit within the
    // current document. The real transaction drops an existing redo tail; the
    // barrier placed after it blocks this marker and every transaction that still
    // points into the released document's non-transactional registry.
    bool bApplied = false;
    {
        const FScopedTransaction BoundaryTransaction(
            INVTEXT("Load Vertical Profile"));
        Tool->AdvanceProfileEditSession();
        bApplied = Tool->ApplyProfileConfiguration(ProfileAsset);
        if (bApplied)
        {
            Tool->SetProfileSequenceLooping(
                ProfileAsset->bLoopByDefault);
        }
    }
    if (GEditor && GEditor->Trans)
    {
        GEditor->Trans->SetUndoBarrier();
    }

    if (!bApplied)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("Could not load the primary key from profile asset: %s"),
            *GetNameSafe(ProfileAsset));
        return FReply::Handled();
    }
    SelectedKeyId = PrimaryKey->KeyId;
    RefreshTimelineKeys();
    RefreshProfileDetails();

    return FReply::Handled();
}

bool SStormProfilePainter::ConfirmClose()
{
    // Any path that actually closes the window drops transient playback previews.
    // Cancel / a failed Save keep the window open and do not reset.
    auto Finalize = [this](bool bMayClose) -> bool
    {
        if (bMayClose)
        {
            ExitProfileSequencePreview();
            if (CompositePreview.IsValid())
            {
                CompositePreview->ReleaseProfileTextures();
            }
        }
        return bMayClose;
    };

    UStormVerticalProfileToolComponent* Tool = GetTool();
    if (!Tool || !Tool->IsUnsavedToAsset())
    {
        return Finalize(true); // nothing at risk
    }

    const EAppReturnType::Type Choice = FMessageDialog::Open(
        EAppMsgType::YesNoCancel,
        INVTEXT("This profile has changes that are not saved to the asset.\n"
                "Live per-key render-target changes are lost when the editor restarts.\n\n"
                "Save before closing?"),
        INVTEXT("Storm Profile Editor"));

    if (Choice == EAppReturnType::Cancel)
    {
        return Finalize(false); // keep the window open
    }
    if (Choice == EAppReturnType::Yes)
    {
        OnSave(); // routes to Save As when no asset exists yet
        // If the save was cancelled/failed, stay open rather than lose the work.
        return Finalize(!Tool->IsUnsavedToAsset());
    }

    return Finalize(true); // No: discard and close (session RT is regenerated next session)
}
