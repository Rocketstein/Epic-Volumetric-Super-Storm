// Copyright 2026 GoroGoro. All Rights Reserved.

/**
 * @file SStormFlowMapEditor.cpp
 * @brief Builds the flow-map editor interface.
 */

#include "Widgets/Flowmap/SStormFlowMapEditor.h"

#include "DetailsViewArgs.h"
#include "Data/StormRenderTargetResolution.h"
#include "Editor.h"
#include "IDetailsView.h"
#include "PropertyEditorModule.h"
#include "Modules/ModuleManager.h"
#include "Styling/AppStyle.h"
#include "Engine/World.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Flowmap/SFlowMapPaintSurface.h"
#include "Widgets/Flowmap/SStormFlowMapLayerRibbon.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SStormFlowMapEditor"

void SStormFlowMapEditor::Construct(const FArguments& InArgs)
{
	FlowMapComponent = InArgs._FlowMapComponent;
	if (UStormFlowMapComponent* Component = FlowMapComponent.Get())
	{
		Component->EnsureFlowMapsInitialized();
	}

	if (UStormFlowMapComponent* Component = FlowMapComponent.Get())
	{
		FDetailsViewArgs DetailsViewArgs;
		DetailsViewArgs.bAllowSearch      = true;
		DetailsViewArgs.bHideSelectionTip = true;
		DetailsViewArgs.bShowObjectLabel  = false;
		DetailsViewArgs.NameAreaSettings  = FDetailsViewArgs::HideNameArea;

		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
		DetailsView                           = PropertyModule.CreateDetailView(DetailsViewArgs);
		DetailsView->SetObject(Component);
	}

	TSharedRef<SWidget> PaintPanel = SNew(SBorder).BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder"))).Padding(12.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				SNew(STextBlock).Text(LOCTEXT("EncodingHint", "Drag to paint. Direction mode follows the stroke; RGBA mode paints encoded RGB direction and alpha strength.")).ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 8.0f, 0.0f)
				[
					SNew(STextBlock).Text(LOCTEXT("BrushInputLabel", "Brush Input"))
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 12.0f, 0.0f)
				[
					SNew(SCheckBox).IsChecked(this, &SStormFlowMapEditor::GetDirectionBrushModeState).OnCheckStateChanged(this, &SStormFlowMapEditor::SetDirectionBrushMode)
					[
						SNew(STextBlock).Text(LOCTEXT("DirectionBrushMode", "Drag Direction"))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 18.0f, 0.0f)
				[
					SNew(SCheckBox).IsChecked(this, &SStormFlowMapEditor::GetRGBABrushModeState).OnCheckStateChanged(this, &SStormFlowMapEditor::SetRGBABrushMode)
					[
						SNew(STextBlock).Text(LOCTEXT("RGBABrushMode", "Encoded RGBA"))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SBox).Visibility(this, &SStormFlowMapEditor::GetRGBAControlsVisibility)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
						[
							SNew(SColorBlock).Color(this, &SStormFlowMapEditor::GetRGBAColor).ShowBackgroundForAlpha(true).UseSRGB(false).Size(FVector2D(52.0f, 22.0f)).OnMouseButtonDown(this, &SStormFlowMapEditor::OpenRGBAColorPicker)
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(STextBlock).Text(this, &SStormFlowMapEditor::GetRGBAValueText).ColorAndOpacity(FSlateColor::UseSubduedForeground())
						]
					]
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 12.0f, 0.0f)
				[
					SNew(SCheckBox).IsChecked(this, &SStormFlowMapEditor::GetPaintModeState).OnCheckStateChanged(this, &SStormFlowMapEditor::SetPaintMode)
					[
						SNew(STextBlock).Text(LOCTEXT("PaintMode", "Paint"))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 18.0f, 0.0f)
				[
					SNew(SCheckBox).IsChecked(this, &SStormFlowMapEditor::GetEraseModeState).OnCheckStateChanged(this, &SStormFlowMapEditor::SetEraseMode)
					[
						SNew(STextBlock).Text(LOCTEXT("EraseMode", "Erase"))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(STextBlock).Text(LOCTEXT("RadiusLabel", "Radius"))
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 12.0f, 0.0f)
				[
					SNew(SSpinBox<float>).MinValue(1.0f).MaxValue(static_cast<float>(VolumetricSuperStorm::RenderTargetResolution::FlowMap)).Value(BrushRadiusTexels).OnValueChanged(this, &SStormFlowMapEditor::SetBrushRadius)
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(STextBlock).Text(LOCTEXT("StrengthLabel", "Strength"))
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 12.0f, 0.0f)
				[
					SNew(SSpinBox<float>).IsEnabled(this, &SStormFlowMapEditor::IsDirectionBrushMode).MinValue(0.0f).MaxValue(1.0f).Delta(0.05f).Value(BrushStrength).OnValueChanged(this, &SStormFlowMapEditor::SetBrushStrength)
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(STextBlock).Text(LOCTEXT("OpacityLabel", "Opacity"))
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 12.0f, 0.0f)
				[
					SNew(SSpinBox<float>).MinValue(0.0f).MaxValue(1.0f).Delta(0.05f).Value(BrushOpacity).OnValueChanged(this, &SStormFlowMapEditor::SetBrushOpacity)
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(STextBlock).Text(LOCTEXT("VerticalLabel", "Vertical"))
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SSpinBox<float>).IsEnabled(this, &SStormFlowMapEditor::IsDirectionBrushMode).MinValue(-1.0f).MaxValue(1.0f).Delta(0.05f).Value(VerticalDirection).OnValueChanged(this, &SStormFlowMapEditor::SetVerticalDirection)
				]
			]
			+ SVerticalBox::Slot().FillHeight(1.0f).HAlign(HAlign_Center).VAlign(VAlign_Center)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Fill).Padding(0.0f, 0.0f, 8.0f, 0.0f)
				[
					SNew(SBox).WidthOverride(104.0f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
						[
							SNew(STextBlock).Text(LOCTEXT("RibbonHeading", "Layer authority"))
						]
						+ SVerticalBox::Slot().FillHeight(1.0f)
						[
							SNew(SBorder).BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Panel"))).Padding(4.0f)
							[
								SNew(SStormFlowMapLayerRibbon).LayerHeights(this, &SStormFlowMapEditor::GetLayerHeights).ActiveLayer(this, &SStormFlowMapEditor::GetActiveRuntimeLayer).OnHeightDragStarted(this, &SStormFlowMapEditor::HandleLayerHeightDragBegin).OnHeightDragEnded(this, &SStormFlowMapEditor::HandleLayerHeightDragEnd).OnHeightsChanged(this, &SStormFlowMapEditor::SetLayerHeights).OnLayerSelected(this, &SStormFlowMapEditor::SelectRuntimeLayer)
							]
						]
					]
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, 3.0f, 0.0f)
						[
							SNew(SButton).Text(LOCTEXT("BottomLayer", "Lower")).HAlign(HAlign_Center).ToolTipText(LOCTEXT("BottomLayerTooltip", "Paint the Lower flow-map surface.")).ButtonColorAndOpacity(this, &SStormFlowMapEditor::GetLayerButtonColor, ELayer::Bottom).OnClicked(this, &SStormFlowMapEditor::SelectLayer, ELayer::Bottom)
						]
						+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(3.0f, 0.0f)
						[
							SNew(SButton).Text(LOCTEXT("MiddleLayer", "Middle")).HAlign(HAlign_Center).ToolTipText(LOCTEXT("MiddleLayerTooltip", "Paint the Middle flow-map surface.")).ButtonColorAndOpacity(this, &SStormFlowMapEditor::GetLayerButtonColor, ELayer::Middle).OnClicked(this, &SStormFlowMapEditor::SelectLayer, ELayer::Middle)
						]
						+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(3.0f, 0.0f, 0.0f, 0.0f)
						[
							SNew(SButton).Text(LOCTEXT("TopLayer", "Upper")).HAlign(HAlign_Center).ToolTipText(LOCTEXT("TopLayerTooltip", "Paint the Upper flow-map surface. This is the " "only layer the shape bake samples.")).ButtonColorAndOpacity(this, &SStormFlowMapEditor::GetLayerButtonColor, ELayer::Top).OnClicked(this, &SStormFlowMapEditor::SelectLayer, ELayer::Top)
						]
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SBox).MinDesiredWidth(384.0f).MinDesiredHeight(384.0f).MaxDesiredWidth(768.0f).MaxDesiredHeight(768.0f)
						[
							SNew(SBorder).BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Panel"))).Padding(4.0f)
							[
								SNew(SFlowMapPaintSurface).RenderTarget(this, &SStormFlowMapEditor::GetActiveRenderTarget).PaintEnabled(true).BrushRadiusUV(this, &SStormFlowMapEditor::GetBrushRadiusUV).ShowDirection(this, &SStormFlowMapEditor::IsDirectionBrushMode).OnStrokeBegin(this, &SStormFlowMapEditor::HandleStrokeBegin).OnStrokeEnd(this, &SStormFlowMapEditor::HandleStrokeEnd).OnPaintStroke(this, &SStormFlowMapEditor::StampFlow)
							]
						]
					]
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(SButton).Text(LOCTEXT("ClearLayer", "Clear Layer")).OnClicked(this, &SStormFlowMapEditor::ClearActiveLayer)
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(SButton).Text(LOCTEXT("Revert", "Revert")).OnClicked(this, &SStormFlowMapEditor::RevertFromAsset)
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(SButton).Text(this, &SStormFlowMapEditor::GetSaveButtonText).ToolTipText(this, &SStormFlowMapEditor::GetSaveButtonTooltip).IsEnabled(this, &SStormFlowMapEditor::CanSaveToCurrentAsset).OnClicked(this, &SStormFlowMapEditor::SaveAsset)
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(SButton).Text(LOCTEXT("SaveAs", "Save As...")).ToolTipText(LOCTEXT("SaveAsTooltip", "Bake the current working flow maps into a new asset and target it.")).OnClicked(this, &SStormFlowMapEditor::SaveAssetAs)
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton).Text(LOCTEXT("Load", "Load...")).ToolTipText(LOCTEXT("LoadTooltip", "Load a saved flow-map asset into this actor's working surfaces.")).OnClicked(this, &SStormFlowMapEditor::LoadAsset)
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f).HAlign(HAlign_Right).VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(this, &SStormFlowMapEditor::GetStatusText).ColorAndOpacity(FSlateColor::UseSubduedForeground())
				]
			]
		];

	if (DetailsView.IsValid())
	{
		ChildSlot
		[
			SNew(SSplitter).Orientation(Orient_Horizontal)
			+ SSplitter::Slot().Value(0.72f)
			[
				PaintPanel
			]
			+ SSplitter::Slot().Value(0.28f)
			[
				SNew(SBox).MinDesiredWidth(260.0f).Padding(6.0f)
				[
					DetailsView.ToSharedRef()
				]
			]
		];
	}
	else
	{
		ChildSlot
		[
			PaintPanel
		];
	}

	if (GEditor)
	{
		GEditor->RegisterForUndo(this);
	}
}

void SStormFlowMapEditor::Tick(
	const FGeometry& AllottedGeometry,
	double InCurrentTime,
	float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	UStormFlowMapComponent* Component = FlowMapComponent.Get();
	UWorld* World = Component ? Component->GetWorld() : nullptr;
	if (Component && World && World->WorldType == EWorldType::Editor)
	{
		TryCheckpointFlowMapHistory();
	}
}

#undef LOCTEXT_NAMESPACE
