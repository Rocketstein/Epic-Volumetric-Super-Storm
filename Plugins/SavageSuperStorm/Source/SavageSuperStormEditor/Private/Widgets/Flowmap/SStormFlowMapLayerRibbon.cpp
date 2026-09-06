/**
 * @file SStormFlowMapLayerRibbon.cpp
 * @brief Renders one editable flow-map layer ribbon.
 */

#include "Widgets/Flowmap/SStormFlowMapLayerRibbon.h"

#include "InputCoreTypes.h"
#include "Data/StormFlowmapParams.h"
#include "Fonts/SlateFontInfo.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

#define LOCTEXT_NAMESPACE "StormFlowMapLayerRibbon"

namespace
{
	const FLinearColor LayerColors[3] = { FLinearColor::FromSRGBColor(FColor(0xD8, 0x5A, 0x30)), FLinearColor::FromSRGBColor(FColor(0x1D, 0x9E, 0x75)), FLinearColor::FromSRGBColor(FColor(0x7F, 0x77, 0xDD)), };

	constexpr float HandleGutterWidth = 30.0f;
	constexpr float SlabHeight        = 4.0f;
	constexpr float KnotGrabRadius    = 5.0f;
	constexpr float HandleHeight      = 12.0f;
	constexpr float ChipWidth         = 46.0f;
	constexpr float ChipHeight        = 14.0f;

	constexpr float KnotGrabMinX = ChipWidth + 12.0f;

	FText LayerName(int32 Index)
	{
		switch (Index)
		{
		case 0:
			return LOCTEXT("LowerLayer", "Lower");
		case 1:
			return LOCTEXT("MiddleLayer", "Middle");
		default:
			return LOCTEXT("UpperLayer", "Upper");
		}
	}

	FNumberFormattingOptions TwoDecimals()
	{
		FNumberFormattingOptions Options;
		Options.MinimumFractionalDigits = 2;
		Options.MaximumFractionalDigits = 2;
		return Options;
	}
}

void SStormFlowMapLayerRibbon::Construct(const FArguments& InArgs)
{
	LayerHeightsAttr = InArgs._LayerHeights;
	ActiveLayerAttr  = InArgs._ActiveLayer;
	OnHeightDragStarted = InArgs._OnHeightDragStarted;
	OnHeightDragEnded   = InArgs._OnHeightDragEnded;
	OnHeightsChanged = InArgs._OnHeightsChanged;
	OnLayerSelected  = InArgs._OnLayerSelected;

	SetToolTipText(TAttribute<FText>::CreateSP(this, &SStormFlowMapLayerRibbon::GetHoverTooltip));
}

FLinearColor SStormFlowMapLayerRibbon::GetLayerColor(EStormFlowMapLayer Layer)
{
	switch (Layer)
	{
	case EStormFlowMapLayer::Lower:
		return LayerColors[0];
	case EStormFlowMapLayer::Middle:
		return LayerColors[1];
	default:
		return LayerColors[2];
	}
}

int32 SStormFlowMapLayerRibbon::OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&, FSlateWindowElementList& Out, int32 LayerId, const FWidgetStyle&, bool) const
{
	const FVector2D Size = Geometry.GetLocalSize();
	if (Size.X <= 0.0 || Size.Y <= 0.0)
	{
		return LayerId;
	}

	const FSlateBrush* WhiteBrush = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
	const FVector3f    Heights    = GetHeights();
	const float        Width      = PlotWidth(Geometry);
	const float        Height     = static_cast<float>(Size.Y);

	for (float SlabTop = 0.0f; SlabTop < Height; SlabTop += SlabHeight)
	{
		const float     SlabBottom = FMath::Min(SlabTop + SlabHeight, Height);
		const FVector3f Weights    = SavageSuperStorm::FlowMap::EvaluateLayerWeights(LocalYToHeight((SlabTop + SlabBottom) * 0.5f, Geometry), Heights);

		float SegmentX = 0.0f;
		for (int32 Index = 0; Index < LayerCount; ++Index)
		{
			const float SegmentWidth = Weights[Index] * Width;
			if (SegmentWidth <= 0.0f)
			{
				continue;
			}

			FSlateDrawElement::MakeBox(Out, LayerId, Geometry.ToPaintGeometry(FVector2f(SegmentWidth, SlabBottom - SlabTop), FSlateLayoutTransform(FVector2f(SegmentX, SlabTop))), WhiteBrush, ESlateDrawEffect::None, LayerColors[Index]);
			SegmentX += SegmentWidth;
		}
	}

	const FLinearColor FlatWash(1.0f, 1.0f, 1.0f, 0.16f);
	const float        LowerFlatY = HeightToLocalY(Heights.X, Geometry);
	const float        UpperFlatY = HeightToLocalY(Heights.Z, Geometry);
	if (LowerFlatY < Height)
	{
		FSlateDrawElement::MakeBox(Out, LayerId + 1, Geometry.ToPaintGeometry(FVector2f(Width, Height - LowerFlatY), FSlateLayoutTransform(FVector2f(0.0f, LowerFlatY))), WhiteBrush, ESlateDrawEffect::None, FlatWash);
	}
	if (UpperFlatY > 0.0f)
	{
		FSlateDrawElement::MakeBox(Out, LayerId + 1, Geometry.ToPaintGeometry(FVector2f(Width, UpperFlatY), FSlateLayoutTransform(FVector2f(0.0f, 0.0f))), WhiteBrush, ESlateDrawEffect::None, FlatWash);
	}

	const FSlateFontInfo SmallFont = FCoreStyle::GetDefaultFontStyle("Regular", 8);

	for (int32 Index = 0; Index < LayerCount; ++Index)
	{
		const float             KnotY    = HeightToLocalY(Heights[Index], Geometry);
		const TArray<FVector2f> KnotLine = { FVector2f(0.0f, KnotY), FVector2f(Width, KnotY), };
		FSlateDrawElement::MakeLines(Out, LayerId + 2, Geometry.ToPaintGeometry(), KnotLine, ESlateDrawEffect::None, FLinearColor(1.0f, 1.0f, 1.0f, 0.7f), true, 1.0f);

		const bool bKnotHot = DraggedKnot == Index || HoveredKnot == Index;
		FSlateDrawElement::MakeBox(Out, LayerId + 3, Geometry.ToPaintGeometry(FVector2f(HandleGutterWidth - 2.0f, HandleHeight), FSlateLayoutTransform(FVector2f(Width + 2.0f, KnotY - HandleHeight * 0.5f))), WhiteBrush, ESlateDrawEffect::None, bKnotHot ? FLinearColor::White : LayerColors[Index]);

		FSlateDrawElement::MakeText(Out, LayerId + 4, Geometry.ToPaintGeometry(FVector2f(HandleGutterWidth - 2.0f, HandleHeight), FSlateLayoutTransform(FVector2f(Width + 4.0f, KnotY - HandleHeight * 0.5f))), FString::Printf(TEXT("%.2f"), Heights[Index]), SmallFont, ESlateDrawEffect::None, FLinearColor::Black);
	}

	const EStormFlowMapLayer ActiveLayer    = ActiveLayerAttr.Get(EStormFlowMapLayer::Lower);
	const float              ChipAnchors[3] = { Heights.X * 0.5f, Heights.Y, (Heights.Z + 1.0f) * 0.5f, };
	for (int32 Index = 0; Index < LayerCount; ++Index)
	{
		const bool           bActive      = static_cast<int32>(ActiveLayer) == Index;
		const float          ChipY        = FMath::Clamp(HeightToLocalY(ChipAnchors[Index], Geometry) - ChipHeight * 0.5f, 0.0f, FMath::Max(Height - ChipHeight, 0.0f));
		const FPaintGeometry ChipGeometry = Geometry.ToPaintGeometry(FVector2f(ChipWidth, ChipHeight), FSlateLayoutTransform(FVector2f(6.0f, ChipY)));

		FSlateDrawElement::MakeBox(Out, LayerId + 5, ChipGeometry, WhiteBrush, ESlateDrawEffect::None, bActive ? LayerColors[Index] : FLinearColor(0.0f, 0.0f, 0.0f, 0.45f));

		FSlateDrawElement::MakeText(Out, LayerId + 6, Geometry.ToPaintGeometry(FVector2f(ChipWidth, ChipHeight), FSlateLayoutTransform(FVector2f(10.0f, ChipY + 1.0f))), LayerName(Index), SmallFont, ESlateDrawEffect::None, bActive ? FLinearColor::Black : FLinearColor::White);
	}

	return LayerId + 7;
}

FReply SStormFlowMapLayerRibbon::OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (Event.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}

	const int32 Knot = FindKnotUnderCursor(Geometry, Event);
	if (Knot != INDEX_NONE)
	{
		DraggedKnot = Knot;
		return FReply::Handled().CaptureMouse(SharedThis(this));
	}

	const float LocalY = static_cast<float>(Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition()).Y);
	OnLayerSelected.ExecuteIfBound(DominantLayerAt(LocalYToHeight(LocalY, Geometry)));
	return FReply::Handled();
}

FReply SStormFlowMapLayerRibbon::OnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event)
{
	const float LocalY = static_cast<float>(Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition()).Y);
	bHovered           = true;
	HoveredHeight      = LocalYToHeight(LocalY, Geometry);

	if (DraggedKnot == INDEX_NONE)
	{
		HoveredKnot = FindKnotUnderCursor(Geometry, Event);
		return FReply::Unhandled();
	}

	if (!bHeightDragStarted)
	{
		bHeightDragStarted = true;
		OnHeightDragStarted.ExecuteIfBound();
	}
	OnHeightsChanged.ExecuteIfBound(ApplyKnotDrag(DraggedKnot, HoveredHeight));
	return FReply::Handled();
}

FReply SStormFlowMapLayerRibbon::OnMouseButtonUp(const FGeometry&, const FPointerEvent& Event)
{
	if (Event.GetEffectingButton() != EKeys::LeftMouseButton || DraggedKnot == INDEX_NONE)
	{
		return FReply::Unhandled();
	}

	EndHeightDrag();
	return FReply::Handled().ReleaseMouseCapture();
}

void SStormFlowMapLayerRibbon::OnMouseLeave(const FPointerEvent& Event)
{
	SLeafWidget::OnMouseLeave(Event);
	bHovered    = false;
	HoveredKnot = INDEX_NONE;
}

void SStormFlowMapLayerRibbon::OnMouseCaptureLost(const FCaptureLostEvent& Event)
{
	SLeafWidget::OnMouseCaptureLost(Event);
	EndHeightDrag();
}

FCursorReply SStormFlowMapLayerRibbon::OnCursorQuery(const FGeometry& Geometry, const FPointerEvent& Event) const
{
	const bool bOverKnot = DraggedKnot != INDEX_NONE || FindKnotUnderCursor(Geometry, Event) != INDEX_NONE;
	return bOverKnot ? FCursorReply::Cursor(EMouseCursor::ResizeUpDown) : FCursorReply::Unhandled();
}

FVector3f SStormFlowMapLayerRibbon::GetHeights() const
{
	return SavageSuperStorm::FlowMap::SanitizeLayerHeights(LayerHeightsAttr.Get(FVector3f(0.15f, 0.50f, 0.85f)));
}

float SStormFlowMapLayerRibbon::PlotWidth(const FGeometry& Geometry) const
{
	return FMath::Max(static_cast<float>(Geometry.GetLocalSize().X) - HandleGutterWidth, 8.0f);
}

float SStormFlowMapLayerRibbon::HeightToLocalY(float Height01, const FGeometry& Geometry) const
{
	const float Height = FMath::Max(static_cast<float>(Geometry.GetLocalSize().Y), 1.0f);
	return (1.0f - FMath::Clamp(Height01, 0.0f, 1.0f)) * Height;
}

float SStormFlowMapLayerRibbon::LocalYToHeight(float LocalY, const FGeometry& Geometry) const
{
	const float Height = FMath::Max(static_cast<float>(Geometry.GetLocalSize().Y), 1.0f);
	return FMath::Clamp(1.0f - LocalY / Height, 0.0f, 1.0f);
}

int32 SStormFlowMapLayerRibbon::FindKnotUnderCursor(const FGeometry& Geometry, const FPointerEvent& Event) const
{
	const FVector2D Local = Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition());
	if (static_cast<float>(Local.X) < KnotGrabMinX)
	{
		return INDEX_NONE;
	}

	const float     LocalY  = static_cast<float>(Local.Y);
	const FVector3f Heights = GetHeights();

	int32 ClosestKnot     = INDEX_NONE;
	float ClosestDistance = KnotGrabRadius;
	for (int32 Index = 0; Index < LayerCount; ++Index)
	{
		const float Distance = FMath::Abs(HeightToLocalY(Heights[Index], Geometry) - LocalY);
		if (Distance <= ClosestDistance)
		{
			ClosestDistance = Distance;
			ClosestKnot     = Index;
		}
	}
	return ClosestKnot;
}

EStormFlowMapLayer SStormFlowMapLayerRibbon::DominantLayerAt(float Height01) const
{
	const FVector3f Weights = SavageSuperStorm::FlowMap::EvaluateLayerWeights(Height01, GetHeights());
	if (Weights.X >= Weights.Y && Weights.X >= Weights.Z)
	{
		return EStormFlowMapLayer::Lower;
	}
	return Weights.Z >= Weights.Y ? EStormFlowMapLayer::Upper : EStormFlowMapLayer::Middle;
}

FVector3f SStormFlowMapLayerRibbon::ApplyKnotDrag(int32 KnotIndex, float DesiredHeight) const
{
	FVector3f   Heights = GetHeights();
	const float Desired = FMath::Clamp(DesiredHeight, 0.0f, 1.0f);

	switch (KnotIndex)
	{
	case 0:
		Heights.X = FMath::Min(Desired, 1.0f - 2.0f * MinDragGap);
		Heights.Y = FMath::Max(Heights.Y, Heights.X + MinDragGap);
		Heights.Z = FMath::Max(Heights.Z, Heights.Y + MinDragGap);
		break;

	case 1:
		Heights.Y = FMath::Clamp(Desired, MinDragGap, 1.0f - MinDragGap);
		Heights.X = FMath::Min(Heights.X, Heights.Y - MinDragGap);
		Heights.Z = FMath::Max(Heights.Z, Heights.Y + MinDragGap);
		break;

	default:
		Heights.Z = FMath::Max(Desired, 2.0f * MinDragGap);
		Heights.Y = FMath::Min(Heights.Y, Heights.Z - MinDragGap);
		Heights.X = FMath::Min(Heights.X, Heights.Y - MinDragGap);
		break;
	}

	return SavageSuperStorm::FlowMap::SanitizeLayerHeights(Heights);
}

FText SStormFlowMapLayerRibbon::GetHoverTooltip() const
{
	if (!bHovered)
	{
		return LOCTEXT("RibbonIdleTooltip", "Layer authority across the cloud column. Drag a knot to move a " "layer, click a band to select it for painting.");
	}

	const FVector3f                Weights = SavageSuperStorm::FlowMap::EvaluateLayerWeights(HoveredHeight, GetHeights());
	const FNumberFormattingOptions Options = TwoDecimals();
	return FText::Format(LOCTEXT("RibbonHoverTooltip", "HLocal {0}\nLower {1}   Middle {2}   Upper {3}"), FText::AsNumber(HoveredHeight, &Options), FText::AsNumber(Weights.X, &Options), FText::AsNumber(Weights.Y, &Options), FText::AsNumber(Weights.Z, &Options));
}

void SStormFlowMapLayerRibbon::EndHeightDrag()
{
	if (DraggedKnot == INDEX_NONE)
	{
		return;
	}

	DraggedKnot = INDEX_NONE;
	if (bHeightDragStarted)
	{
		bHeightDragStarted = false;
		OnHeightDragEnded.ExecuteIfBound();
	}
}

#undef LOCTEXT_NAMESPACE
