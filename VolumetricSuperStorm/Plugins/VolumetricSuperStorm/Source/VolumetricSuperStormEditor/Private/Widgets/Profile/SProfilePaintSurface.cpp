// Copyright 2026 GoroGoro. All Rights Reserved.

/**
 * @file SProfilePaintSurface.cpp
 * @brief Implements the paintable surface widget of the vertical profile painter.
 */

#include "Widgets/Profile/SProfilePaintSurface.h"
#include "Engine/TextureRenderTarget2D.h"
#include "InputCoreTypes.h"
#include "Rendering/DrawElements.h"

void SProfilePaintSurface::Construct(const FArguments& InArgs)
{
    OnPaintUV = InArgs._OnPaintUV;
    OnStrokeBegin = InArgs._OnStrokeBegin;
    OnStrokeEnd = InArgs._OnStrokeEnd;
    RenderTargetAttr = InArgs._RenderTarget;
    PaintEnabledAttr = InArgs._PaintEnabled;
    BrushRadiusUVAttr = InArgs._BrushRadiusUV;
    RTBrush.DrawAs = ESlateBrushDrawType::Image;
}

int32 SProfilePaintSurface::OnPaint(const FPaintArgs&, const FGeometry& Geometry,
    const FSlateRect&, FSlateWindowElementList& Out, int32 LayerId,
    const FWidgetStyle&, bool) const
{
    if (UTextureRenderTarget2D* RT = RenderTargetAttr.Get())
    {
        RTBrush.SetResourceObject(RT);                 // live pointer, refreshes each frame
        FSlateDrawElement::MakeBox(Out, LayerId, Geometry.ToPaintGeometry(),
            &RTBrush, ESlateDrawEffect::None);
    }
    else
    {
        // Not a GC root -- box elements are excluded from Slate's reference
        // collection -- so this does not hold the RT's world alive.
        RTBrush.SetResourceObject(nullptr);
    }

    const bool bCanPaint = PaintEnabledAttr.IsSet() ? PaintEnabledAttr.Get() : false;
    const float BrushRadiusUV = BrushRadiusUVAttr.IsSet() ? BrushRadiusUVAttr.Get() : 0.f;
    const FVector2D Size = Geometry.GetLocalSize();
    if (bMouseOver && bCanPaint && BrushRadiusUV > 0.f && Size.X > 0.f && Size.Y > 0.f)
    {
        const float RadiusPixels = BrushRadiusUV * FMath::Min(Size.X, Size.Y);
        if (RadiusPixels > 0.f)
        {
            const FVector2D Center(
                FMath::Clamp(LastMouseLocal.X, 0.f, Size.X),
                FMath::Clamp(LastMouseLocal.Y, 0.f, Size.Y));

            constexpr int32 SegmentCount = 48;
            TArray<FVector2f> CirclePoints;
            CirclePoints.Reserve(SegmentCount + 1);

            for (int32 Index = 0; Index <= SegmentCount; ++Index)
            {
                const float Angle = (static_cast<float>(Index) / static_cast<float>(SegmentCount)) * 2.f * PI;
                const FVector2D Point = Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * RadiusPixels;
                CirclePoints.Add(FVector2f(static_cast<float>(Point.X), static_cast<float>(Point.Y)));
            }

            FSlateDrawElement::MakeLines(
                Out,
                LayerId + 1,
                Geometry.ToPaintGeometry(),
                CirclePoints,
                ESlateDrawEffect::None,
                FLinearColor::Black,
                true,
                3.f);

            FSlateDrawElement::MakeLines(
                Out,
                LayerId + 2,
                Geometry.ToPaintGeometry(),
                CirclePoints,
                ESlateDrawEffect::None,
                FLinearColor::White,
                true,
                1.5f);

            return LayerId + 3;
        }
    }

    return LayerId + 1;
}

void SProfilePaintSurface::StampAt(const FGeometry& Geometry, const FPointerEvent& Event)
{
    const FVector2D Size = Geometry.GetLocalSize();
    if (Size.X <= 0.f || Size.Y <= 0.f) return;
    const FVector2D Local = Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition());
    FVector2D UV(Local.X / Size.X, Local.Y / Size.Y);  // native, NO V-flip
    UV.X = FMath::Clamp(UV.X, 0.0, 1.0);
    UV.Y = FMath::Clamp(UV.Y, 0.0, 1.0);
    OnPaintUV.ExecuteIfBound(UV);
}

FReply SProfilePaintSurface::OnMouseButtonDown(const FGeometry& G, const FPointerEvent& E)
{
    if (E.GetEffectingButton() != EKeys::LeftMouseButton)
    {
        return FReply::Unhandled();
    }

    LastMouseLocal = G.AbsoluteToLocal(E.GetScreenSpacePosition());
    Invalidate(EInvalidateWidgetReason::Paint);

    const bool bCanPaint = PaintEnabledAttr.IsSet() ? PaintEnabledAttr.Get() : false;
    if (!OnPaintUV.IsBound() || !bCanPaint) return FReply::Unhandled();   // preview surfaces don't capture
    bPainting = true;
    // Open the stroke before the first dab, so every dab in this gesture lands
    // inside the same history group.
    OnStrokeBegin.ExecuteIfBound();
    StampAt(G, E);
    return FReply::Handled().CaptureMouse(SharedThis(this));
}

FReply SProfilePaintSurface::OnMouseMove(const FGeometry& G, const FPointerEvent& E)
{
    LastMouseLocal = G.AbsoluteToLocal(E.GetScreenSpacePosition());
    Invalidate(EInvalidateWidgetReason::Paint);

    const bool bCanPaint = PaintEnabledAttr.IsSet() ? PaintEnabledAttr.Get() : false;
    if (bPainting && bCanPaint) { StampAt(G, E); return FReply::Handled(); }
    return FReply::Unhandled();
}

FReply SProfilePaintSurface::OnMouseButtonUp(const FGeometry&, const FPointerEvent& E)
{
    if (E.GetEffectingButton() != EKeys::LeftMouseButton)
    {
        return FReply::Unhandled();
    }

    EndStrokeIfPainting();
    return FReply::Handled().ReleaseMouseCapture();
}

void SProfilePaintSurface::EndStrokeIfPainting()
{
    if (!bPainting)
    {
        return;
    }
    bPainting = false;
    OnStrokeEnd.ExecuteIfBound();
}

void SProfilePaintSurface::OnMouseEnter(const FGeometry& G, const FPointerEvent& E)
{
    bMouseOver = true;
    LastMouseLocal = G.AbsoluteToLocal(E.GetScreenSpacePosition());
    Invalidate(EInvalidateWidgetReason::Paint);
}

void SProfilePaintSurface::OnMouseLeave(const FPointerEvent&)
{
    bMouseOver = false;
    EndStrokeIfPainting();
    Invalidate(EInvalidateWidgetReason::Paint);
}

void SProfilePaintSurface::OnMouseCaptureLost(const FCaptureLostEvent& Event)
{
    SLeafWidget::OnMouseCaptureLost(Event);
    // The only remaining path out of a stroke that never sees a button release.
    EndStrokeIfPainting();
    bMouseOver = false;
    Invalidate(EInvalidateWidgetReason::Paint);
}
