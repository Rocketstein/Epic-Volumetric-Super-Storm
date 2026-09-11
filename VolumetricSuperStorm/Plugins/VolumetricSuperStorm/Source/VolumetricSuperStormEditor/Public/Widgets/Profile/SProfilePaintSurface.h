// Copyright 2026 GoroGoro. All Rights Reserved.

/**
 * @file SProfilePaintSurface.h
 * @brief Declares the paintable surface widget of the vertical profile painter.
 */

#pragma once
#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"

class UTextureRenderTarget2D;

/** Displays a profile render target and emits normalized UVs for brush input. */
class SProfilePaintSurface : public SLeafWidget
{
public:
    /** Invoked with a brush position in the render target's normalized [0,1] UV space. */
    DECLARE_DELEGATE_OneParam(FOnPaintUV, FVector2D /*UV in [0,1]*/);
    /**
     * Brackets one press-drag-release gesture. The dabs emitted between these two
     * form a single entry in the tool's edit history, so a drag undoes as one step
     * rather than as the dozens of dabs it actually issued.
     */
    DECLARE_DELEGATE(FOnStrokeBoundary);

    SLATE_BEGIN_ARGS(SProfilePaintSurface) {}
        SLATE_EVENT(FOnPaintUV, OnPaintUV)                       // unbound = display-only
        SLATE_EVENT(FOnStrokeBoundary, OnStrokeBegin)
        SLATE_EVENT(FOnStrokeBoundary, OnStrokeEnd)
        SLATE_ATTRIBUTE(UTextureRenderTarget2D*, RenderTarget)
        SLATE_ATTRIBUTE(bool, PaintEnabled)
        SLATE_ATTRIBUTE(float, BrushRadiusUV)
    SLATE_END_ARGS()

    /** Binds the render target, paint state, brush radius, and paint callback. */
    void Construct(const FArguments& InArgs);

    /** Draws the current profile render target. */
    virtual int32 OnPaint(const FPaintArgs&, const FGeometry&, const FSlateRect&,
        FSlateWindowElementList&, int32, const FWidgetStyle&, bool) const override;
    /** Begins painting or captures the initial pointer position. */
    virtual FReply OnMouseButtonDown(const FGeometry&, const FPointerEvent&) override;
    /** Emits brush UVs while the pointer moves during a paint gesture. */
    virtual FReply OnMouseMove(const FGeometry&, const FPointerEvent&) override;
    /** Completes the current paint gesture. */
    virtual FReply OnMouseButtonUp(const FGeometry&, const FPointerEvent&) override;
    /** Tracks whether the pointer is over the paint surface. */
    virtual void OnMouseEnter(const FGeometry&, const FPointerEvent&) override;
    /** Clears hover and active paint state when the pointer leaves. */
    virtual void OnMouseLeave(const FPointerEvent&) override;
    /**
     * Closes an open stroke when capture is taken away without a button release.
     *
     * The gesture holds mouse capture, so window deactivation, a modal dialog, or
     * anything else that steals capture ends the drag without ever delivering
     * OnMouseButtonUp. Without this the stroke stays open and the editor
     * transaction bracketing it is never closed.
     */
    virtual void OnMouseCaptureLost(const FCaptureLostEvent&) override;
    /** Returns the paint surface's preferred square display size. */
    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(320.f, 320.f); }

private:
    /** Converts the pointer position to UV space and emits a paint callback. */
    void StampAt(const FGeometry&, const FPointerEvent&);
    /** Closes an open stroke exactly once, whether it ended by release or by leaving. */
    void EndStrokeIfPainting();

    FOnPaintUV OnPaintUV;
    FOnStrokeBoundary OnStrokeBegin;
    FOnStrokeBoundary OnStrokeEnd;
    TAttribute<UTextureRenderTarget2D*> RenderTargetAttr;
    TAttribute<bool> PaintEnabledAttr;
    TAttribute<float> BrushRadiusUVAttr;
    mutable FSlateBrush RTBrush;
    FVector2D LastMouseLocal = FVector2D::ZeroVector;
    bool bPainting = false;
    bool bMouseOver = false;
};
