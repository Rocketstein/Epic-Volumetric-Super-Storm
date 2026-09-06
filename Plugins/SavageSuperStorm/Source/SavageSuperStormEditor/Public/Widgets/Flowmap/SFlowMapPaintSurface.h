/**
 * @file SFlowMapPaintSurface.h
 * @brief Declares the flow-map painting surface.
 */

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"

class UTextureRenderTarget2D;

class SFlowMapPaintSurface final : public SLeafWidget
{
public:
	DECLARE_DELEGATE_TwoParams(FOnPaintStroke, FVector2D, FVector2D);
	DECLARE_DELEGATE(FOnStrokeBoundary);

	SLATE_BEGIN_ARGS(SFlowMapPaintSurface)
		{}

		SLATE_EVENT(FOnPaintStroke, OnPaintStroke)
		SLATE_EVENT(FOnStrokeBoundary, OnStrokeBegin)
		SLATE_EVENT(FOnStrokeBoundary, OnStrokeEnd)
		SLATE_ATTRIBUTE(UTextureRenderTarget2D*, RenderTarget)
		SLATE_ATTRIBUTE(bool, PaintEnabled)
		SLATE_ATTRIBUTE(float, BrushRadiusUV)
		SLATE_ATTRIBUTE(bool, ShowDirection)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual int32  OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FReply OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply OnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply OnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual void   OnMouseEnter(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual void   OnMouseLeave(const FPointerEvent& Event) override;
	virtual void   OnMouseCaptureLost(const FCaptureLostEvent& Event) override;

	virtual FVector2D ComputeDesiredSize(float) const override
	{
		return FVector2D(512.0f, 512.0f);
	}

private:
	FVector2D MouseUV(const FGeometry& Geometry, const FPointerEvent& Event) const;
	void      EmitStrokeTo(const FGeometry& Geometry, const FPointerEvent& Event);
	void      EndStrokeIfPainting();
	bool      IsPaintingEnabled() const;
	bool      ShouldShowDirection() const;
	float     GetBrushRadiusUV() const;

	FOnPaintStroke                      OnPaintStroke;
	FOnStrokeBoundary                   OnStrokeBegin;
	FOnStrokeBoundary                   OnStrokeEnd;
	TAttribute<UTextureRenderTarget2D*> RenderTargetAttr;
	TAttribute<bool>                    PaintEnabledAttr;
	TAttribute<float>                   BrushRadiusUVAttr;
	TAttribute<bool>                    ShowDirectionAttr;
	mutable FSlateBrush                 RenderTargetBrush;
	FVector2D                           LastMouseLocal  = FVector2D::ZeroVector;
	FVector2D                           LastStampUV     = FVector2D::ZeroVector;
	FVector2D                           LastDirectionUV = FVector2D(1.0, 0.0);
	bool                                bHasLastStamp   = false;
	bool                                bPainting       = false;
	bool                                bMouseOver      = false;
};
