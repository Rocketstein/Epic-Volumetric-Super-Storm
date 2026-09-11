// Copyright 2026 GoroGoro. All Rights Reserved.

/**
 * @file SFlowMapPaintSurface.cpp
 * @brief Implements pointer input and brush painting for flow maps.
 */

#include "Widgets/Flowmap/SFlowMapPaintSurface.h"

#include "InputCoreTypes.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Rendering/DrawElements.h"

void SFlowMapPaintSurface::Construct(const FArguments& InArgs)
{
	OnPaintStroke            = InArgs._OnPaintStroke;
	OnStrokeBegin            = InArgs._OnStrokeBegin;
	OnStrokeEnd              = InArgs._OnStrokeEnd;
	RenderTargetAttr         = InArgs._RenderTarget;
	PaintEnabledAttr         = InArgs._PaintEnabled;
	BrushRadiusUVAttr        = InArgs._BrushRadiusUV;
	ShowDirectionAttr        = InArgs._ShowDirection;
	RenderTargetBrush.DrawAs = ESlateBrushDrawType::Image;
}

int32 SFlowMapPaintSurface::OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&, FSlateWindowElementList& Out, int32 LayerId, const FWidgetStyle&, bool) const
{
	if (UTextureRenderTarget2D* RenderTarget = RenderTargetAttr.Get())
	{
		RenderTargetBrush.SetResourceObject(RenderTarget);
		FSlateDrawElement::MakeBox(Out, LayerId, Geometry.ToPaintGeometry(), &RenderTargetBrush, ESlateDrawEffect::None);
	}
	else
	{
		// See SProfilePaintSurface: not a leak fix, but it keeps the brush from
		// latching a pointer to an RT whose world has since been torn down.
		RenderTargetBrush.SetResourceObject(nullptr);
	}

	const FVector2D         Size        = Geometry.GetLocalSize();
	const float             MaxX        = FMath::Max(static_cast<float>(Size.X) - 1.0f, 1.0f);
	const float             MaxY        = FMath::Max(static_cast<float>(Size.Y) - 1.0f, 1.0f);
	const TArray<FVector2f> FramePoints = { FVector2f(1.0f, 1.0f), FVector2f(MaxX, 1.0f), FVector2f(MaxX, MaxY), FVector2f(1.0f, MaxY), FVector2f(1.0f, 1.0f), };
	FSlateDrawElement::MakeLines(Out, LayerId + 1, Geometry.ToPaintGeometry(), FramePoints, ESlateDrawEffect::None, FLinearColor::Black, true, 4.0f);
	FSlateDrawElement::MakeLines(Out, LayerId + 2, Geometry.ToPaintGeometry(), FramePoints, ESlateDrawEffect::None, FLinearColor(0.75f, 0.75f, 0.75f), true, 1.5f);

	const float RadiusUV = GetBrushRadiusUV();
	if (!bMouseOver || !IsPaintingEnabled() || RadiusUV <= 0.0f || Size.X <= 0.0 || Size.Y <= 0.0)
	{
		return LayerId + 3;
	}

	const float     RadiusPixels = RadiusUV * FMath::Min(Size.X, Size.Y);
	const FVector2D Center(FMath::Clamp(LastMouseLocal.X, 0.0, Size.X), FMath::Clamp(LastMouseLocal.Y, 0.0, Size.Y));

	constexpr int32   SegmentCount = 48;
	TArray<FVector2f> CirclePoints;
	CirclePoints.Reserve(SegmentCount + 1);
	for (int32 Index = 0; Index <= SegmentCount; ++Index)
	{
		const float     Angle = static_cast<float>(Index) / SegmentCount * 2.0f * PI;
		const FVector2D Point = Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * RadiusPixels;
		CirclePoints.Add(FVector2f(static_cast<float>(Point.X), static_cast<float>(Point.Y)));
	}

	FSlateDrawElement::MakeLines(Out, LayerId + 3, Geometry.ToPaintGeometry(), CirclePoints, ESlateDrawEffect::None, FLinearColor::Black, true, 3.0f);
	FSlateDrawElement::MakeLines(Out, LayerId + 4, Geometry.ToPaintGeometry(), CirclePoints, ESlateDrawEffect::None, FLinearColor::White, true, 1.5f);

	if (!ShouldShowDirection())
	{
		return LayerId + 5;
	}

	const FVector2D   ArrowEnd = Center + LastDirectionUV * RadiusPixels * 0.8;
	TArray<FVector2f> ArrowPoints;
	ArrowPoints.Add(FVector2f(static_cast<float>(Center.X), static_cast<float>(Center.Y)));
	ArrowPoints.Add(FVector2f(static_cast<float>(ArrowEnd.X), static_cast<float>(ArrowEnd.Y)));
	FSlateDrawElement::MakeLines(Out, LayerId + 5, Geometry.ToPaintGeometry(), ArrowPoints, ESlateDrawEffect::None, FLinearColor(0.05f, 0.75f, 1.0f), true, 2.0f);

	return LayerId + 6;
}

FReply SFlowMapPaintSurface::OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (Event.GetEffectingButton() != EKeys::LeftMouseButton || !OnPaintStroke.IsBound() || !IsPaintingEnabled())
	{
		return FReply::Unhandled();
	}

	LastMouseLocal = Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition());
	bPainting      = true;
	bHasLastStamp  = false;
	OnStrokeBegin.ExecuteIfBound();
	EmitStrokeTo(Geometry, Event);
	Invalidate(EInvalidateWidgetReason::Paint);
	return FReply::Handled().CaptureMouse(SharedThis(this));
}

FReply SFlowMapPaintSurface::OnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event)
{
	LastMouseLocal = Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition());
	Invalidate(EInvalidateWidgetReason::Paint);

	if (bPainting && IsPaintingEnabled())
	{
		EmitStrokeTo(Geometry, Event);
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

FReply SFlowMapPaintSurface::OnMouseButtonUp(const FGeometry&, const FPointerEvent& Event)
{
	if (Event.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}

	EndStrokeIfPainting();
	return FReply::Handled().ReleaseMouseCapture();
}

void SFlowMapPaintSurface::EndStrokeIfPainting()
{
	if (!bPainting)
	{
		return;
	}
	bPainting     = false;
	bHasLastStamp = false;
	OnStrokeEnd.ExecuteIfBound();
}

void SFlowMapPaintSurface::OnMouseEnter(const FGeometry& Geometry, const FPointerEvent& Event)
{
	bMouseOver     = true;
	LastMouseLocal = Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition());
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SFlowMapPaintSurface::OnMouseLeave(const FPointerEvent&)
{
	bMouseOver    = false;
	EndStrokeIfPainting();
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SFlowMapPaintSurface::OnMouseCaptureLost(const FCaptureLostEvent& Event)
{
	SLeafWidget::OnMouseCaptureLost(Event);
	EndStrokeIfPainting();
	bMouseOver = false;
	Invalidate(EInvalidateWidgetReason::Paint);
}

FVector2D SFlowMapPaintSurface::MouseUV(const FGeometry& Geometry, const FPointerEvent& Event) const
{
	const FVector2D Size = Geometry.GetLocalSize();
	if (Size.X <= 0.0 || Size.Y <= 0.0)
	{
		return FVector2D::ZeroVector;
	}

	const FVector2D Local = Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition());
	return FVector2D(FMath::Clamp(Local.X / Size.X, 0.0, 1.0), FMath::Clamp(Local.Y / Size.Y, 0.0, 1.0));
}

void SFlowMapPaintSurface::EmitStrokeTo(const FGeometry& Geometry, const FPointerEvent& Event)
{
	const FVector2D CurrentUV = MouseUV(Geometry, Event);
	if (!bHasLastStamp)
	{
		LastStampUV   = CurrentUV;
		bHasLastStamp = true;
		OnPaintStroke.ExecuteIfBound(CurrentUV, LastDirectionUV);
		return;
	}

	const FVector2D Delta    = CurrentUV - LastStampUV;
	const double    Distance = Delta.Length();
	if (Distance <= UE_DOUBLE_SMALL_NUMBER)
	{
		return;
	}

	LastDirectionUV             = Delta / Distance;
	const double    Spacing     = FMath::Max(static_cast<double>(GetBrushRadiusUV()) * 0.35, 1.0 / 2048.0);
	const int32     StepCount   = FMath::Clamp(FMath::CeilToInt(Distance / Spacing), 1, 128);
	const FVector2D StrokeStart = LastStampUV;
	for (int32 Step = 1; Step <= StepCount; ++Step)
	{
		const double Alpha = static_cast<double>(Step) / StepCount;
		OnPaintStroke.ExecuteIfBound(FMath::Lerp(StrokeStart, CurrentUV, Alpha), LastDirectionUV);
	}
	LastStampUV = CurrentUV;
}

bool SFlowMapPaintSurface::IsPaintingEnabled() const
{
	return PaintEnabledAttr.IsSet() && PaintEnabledAttr.Get();
}

bool SFlowMapPaintSurface::ShouldShowDirection() const
{
	return !ShowDirectionAttr.IsSet() || ShowDirectionAttr.Get();
}

float SFlowMapPaintSurface::GetBrushRadiusUV() const
{
	return BrushRadiusUVAttr.IsSet() ? BrushRadiusUVAttr.Get() : 0.0f;
}
