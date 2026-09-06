/**
 * @file StormMotionComponentVisualizer.h
 * @brief Declares the storm motion-ring viewport visualizer.
 */

#pragma once

#include "ComponentVisualizer.h"

class UStormMaterialBinderComponent;

struct HStormMotionBoundaryVisProxy final : public HComponentVisProxy
{
	DECLARE_HIT_PROXY();

	HStormMotionBoundaryVisProxy(const UActorComponent* InComponent, int32 InBoundaryIndex) : HComponentVisProxy(InComponent, HPP_Foreground), BoundaryIndex(InBoundaryIndex)
	{}

	int32 BoundaryIndex = INDEX_NONE;
};

class FStormMotionComponentVisualizer final : public FComponentVisualizer
{
public:
	virtual void DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI) override;

	virtual void DrawVisualizationHUD(const UActorComponent* Component, const FViewport* Viewport, const FSceneView* View, FCanvas* Canvas) override;

	virtual bool VisProxyHandleClick(FEditorViewportClient* InViewportClient, HComponentVisProxy* VisProxy, const FViewportClick& Click) override;

	virtual bool ShouldAutoSelectElementOnHandleClick() const override
	{
		return false;
	}

	virtual bool GetWidgetLocation(const FEditorViewportClient* ViewportClient, FVector& OutLocation) const override;

	virtual bool GetCustomInputCoordinateSystem(const FEditorViewportClient* ViewportClient, FMatrix& OutMatrix) const override;

	virtual bool HandleInputDelta(FEditorViewportClient* ViewportClient, FViewport* Viewport, FVector& DeltaTranslate, FRotator& DeltaRotate, FVector& DeltaScale) override;

	virtual void TrackingStarted(FEditorViewportClient* InViewportClient) override;

	virtual void TrackingStopped(FEditorViewportClient* InViewportClient, bool bInDidMove) override;

	virtual void             EndEditing() override;
	virtual UActorComponent* GetEditedComponent() const override;

private:
	UStormMaterialBinderComponent* GetEditedBinder() const;

	FComponentPropertyPath EditedComponentPath;
	int32                  SelectedBoundaryIndex          = INDEX_NONE;
	bool                   bBoundaryChangedDuringTracking = false;
};