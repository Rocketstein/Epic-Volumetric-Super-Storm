/**
 * @file SStormFlowMapLayerRibbon.h
 * @brief Declares one flow-map layer ribbon widget.
 */

#pragma once

#include "CoreMinimal.h"
#include "Components/StormFlowMapComponent.h"
#include "Widgets/SLeafWidget.h"

class SStormFlowMapLayerRibbon final : public SLeafWidget
{
public:
	DECLARE_DELEGATE_OneParam(FOnLayerHeightsChanged, FVector3f);
	DECLARE_DELEGATE_OneParam(FOnLayerSelected, EStormFlowMapLayer);

	SLATE_BEGIN_ARGS(SStormFlowMapLayerRibbon)
		{}

		SLATE_ATTRIBUTE(FVector3f, LayerHeights)
		SLATE_ATTRIBUTE(EStormFlowMapLayer, ActiveLayer)
		SLATE_EVENT(FSimpleDelegate, OnHeightDragStarted)
		SLATE_EVENT(FSimpleDelegate, OnHeightDragEnded)
		SLATE_EVENT(FOnLayerHeightsChanged, OnHeightsChanged)
		SLATE_EVENT(FOnLayerSelected, OnLayerSelected)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	static FLinearColor GetLayerColor(EStormFlowMapLayer Layer);

	virtual int32        OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FReply       OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply       OnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply       OnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual void         OnMouseLeave(const FPointerEvent& Event) override;
	virtual void         OnMouseCaptureLost(const FCaptureLostEvent& Event) override;
	virtual FCursorReply OnCursorQuery(const FGeometry& Geometry, const FPointerEvent& Event) const override;

	virtual FVector2D ComputeDesiredSize(float) const override
	{
		return FVector2D(104.0f, 300.0f);
	}

	virtual bool ComputeVolatility() const override
	{
		return true;
	}

private:
	static constexpr int32 LayerCount = 3;

	static constexpr float MinDragGap = 0.01f;

	FVector3f          GetHeights() const;
	float              PlotWidth(const FGeometry& Geometry) const;
	float              HeightToLocalY(float Height01, const FGeometry& Geometry) const;
	float              LocalYToHeight(float LocalY, const FGeometry& Geometry) const;
	int32              FindKnotUnderCursor(const FGeometry& Geometry, const FPointerEvent& Event) const;
	EStormFlowMapLayer DominantLayerAt(float Height01) const;
	FVector3f          ApplyKnotDrag(int32 KnotIndex, float DesiredHeight) const;
	FText              GetHoverTooltip() const;
	void               EndHeightDrag();

	FSimpleDelegate                 OnHeightDragStarted;
	FSimpleDelegate                 OnHeightDragEnded;
	FOnLayerHeightsChanged         OnHeightsChanged;
	FOnLayerSelected               OnLayerSelected;
	TAttribute<FVector3f>          LayerHeightsAttr;
	TAttribute<EStormFlowMapLayer> ActiveLayerAttr;
	int32                          DraggedKnot        = INDEX_NONE;
	int32                          HoveredKnot        = INDEX_NONE;
	float                          HoveredHeight      = 0.0f;
	bool                           bHovered           = false;
	bool                           bHeightDragStarted = false;
};
