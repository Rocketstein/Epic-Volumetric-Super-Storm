/**
 * @file StormMotionComponentVisualizer.cpp
 * @brief Draws and edits storm motion-ring boundaries in the viewport.
 */

#include "Visualization/StormMotionComponentVisualizer.h"

#include "Actors/VolumetricSuperStormActor.h"
#include "Components/StormMaterialBinderComponent.h"
#include "Motion/StormMotionSettingsUtils.h"

#include "Editor.h"
#include "PrimitiveDrawingUtils.h"
#include "SceneView.h"
#include "Components/VolumetricCloudComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"

IMPLEMENT_HIT_PROXY(HStormMotionBoundaryVisProxy, HComponentVisProxy);

namespace
{
	constexpr int32  CircleSegmentCount        = 128;
	constexpr float  BoundaryCenterThickness   = 1.0f;
	constexpr float  SelectedBoundaryThickness = 5.0f;
	constexpr float  HandlePointSize           = 14.0f;
	constexpr double CentimetersToKilometers   = 1.0e-5;
	constexpr double KilometersToCentimeters   = 100000.0;

	struct FStormDebugLayerBounds
	{
		double BottomZ = 0.0;
		double Height  = 1.0;
	};

	const AVolumetricSuperStormActor* GetStormActor(const UActorComponent* Component)
	{
		const UStormMaterialBinderComponent* Binder = Cast<const UStormMaterialBinderComponent>(Component);
		return Binder ? Cast<AVolumetricSuperStormActor>(Binder->GetOwner()) : nullptr;
	}

	AVolumetricSuperStormActor* GetMutableStormActor(UActorComponent* Component)
	{
		UStormMaterialBinderComponent* Binder = Cast<UStormMaterialBinderComponent>(Component);
		return Binder ? Cast<AVolumetricSuperStormActor>(Binder->GetOwner()) : nullptr;
	}

	FLinearColor GetRingColor(int32 RingIndex)
	{
		const uint8 Hue = static_cast<uint8>((RingIndex * 47) % 255);
		return FLinearColor::MakeFromHSV8(Hue, 190, 255);
	}

	double GetSafeShapeRadius(const FStormRenderData& RenderData)
	{
		return FMath::Max(1.0, static_cast<double>(FMath::Abs(RenderData.Shape.Radius)));
	}

	double GetSafeOuterBrimWorldRadius(const FStormRenderData& RenderData)
	{
		const double OuterBrimRadiusScale = RenderData.Shape.GetAnvilOuterRadiusScale();
		return GetSafeShapeRadius(RenderData) * OuterBrimRadiusScale;
	}

	FStormDebugLayerBounds GetStormLayerBounds(const UStormMaterialBinderComponent* Binder, const FStormRenderData& RenderData)
	{
		if (Binder)
		{
			if (const UVolumetricCloudComponent* Cloud = Binder->ResolveTargetCloud())
			{
				return { static_cast<double>(Cloud->LayerBottomAltitude) * KilometersToCentimeters, FMath::Max(1.0, static_cast<double>(Cloud->LayerHeight) * KilometersToCentimeters) };
			}
		}

		const double FallbackHeight = FMath::Max(1.0, static_cast<double>(FMath::Abs(RenderData.WorldExtent.Z)));
		return { RenderData.WorldCenter.Z - FallbackHeight * 0.5, FallbackHeight };
	}

	FVector GetMotionSliceCenterAtHeight(const UStormMaterialBinderComponent* Binder, const FStormRenderData& RenderData, double OuterBrimRadius01, double MotionHeight01)
	{
		const FStormShapeSettings& Shape                   = RenderData.Shape;
		const double               RadiusAcrossOuterBrim01 = FMath::Clamp(OuterBrimRadius01, 0.0, 1.0);
		const FLinearColor         LayerHeightsAtRadius    = Shape.ShapeCurves.LayerHeight.GetLinearColorValue(static_cast<float>(RadiusAcrossOuterBrim01));
		const double               MinimumHeight01         = FMath::Clamp(static_cast<double>(LayerHeightsAtRadius.R), 0.0, 1.0 - 1.0e-4);
		const double               MaximumHeight01         = FMath::Clamp(static_cast<double>(LayerHeightsAtRadius.B), MinimumHeight01 + 1.0e-4, 1.0);

		const double                 CloudLayerHeight01 = FMath::Lerp(MinimumHeight01, MaximumHeight01, FMath::Clamp(MotionHeight01, 0.0, 1.0));
		const FStormDebugLayerBounds LayerBounds        = GetStormLayerBounds(Binder, RenderData);

		FVector Center = RenderData.WorldCenter;
		Center.Z       = LayerBounds.BottomZ + LayerBounds.Height * CloudLayerHeight01;
		return Center;
	}

	FVector GetMotionSliceCenter(const UStormMaterialBinderComponent* Binder, const FStormRenderData& RenderData, double OuterBrimRadius01)
	{
		const double MotionHeight01 = FMath::Clamp(0.5 * (static_cast<double>(RenderData.Motion.HeightMin01) + static_cast<double>(RenderData.Motion.HeightMax01)), 0.0, 1.0);
		return GetMotionSliceCenterAtHeight(Binder, RenderData, OuterBrimRadius01, MotionHeight01);
	}

	FVector GetBoundaryHandleLocation(const UStormMaterialBinderComponent* Binder, const FStormRenderData& RenderData, int32 BoundaryIndex)
	{
		const double OuterBrimWorldRadius = GetSafeOuterBrimWorldRadius(RenderData);
		const double BoundaryRadius01     = RenderData.Motion.RingEndRadii01[BoundaryIndex];
		return GetMotionSliceCenter(Binder, RenderData, BoundaryRadius01) + FVector(OuterBrimWorldRadius * BoundaryRadius01, 0.0, 0.0);
	}

	void DrawCircleAtRadius(FPrimitiveDrawInterface* PDI, const FVector& Center, double Radius, const FLinearColor& Color, float Thickness)
	{
		if (!PDI || Radius <= UE_KINDA_SMALL_NUMBER)
		{
			return;
		}

		DrawCircle(PDI, Center, FVector::ForwardVector, FVector::RightVector, Color, Radius, CircleSegmentCount, SDPG_Foreground, Thickness, 0.0f, true);
	}

	bool IsDebugVisualizationEnabled(const FStormRenderData& RenderData)
	{
		return RenderData.Motion.bDebugDrawRingEndRadii;
	}

	void DrawWorldLabel(const FSceneView* View, FCanvas* Canvas, const FVector& WorldLocation, const FString& Label, const FLinearColor& Color, float VerticalOffset)
	{
		if (!View || !Canvas || !GEngine || !GEngine->GetSmallFont())
		{
			return;
		}

		FVector2D PixelPosition;
		if (!View->WorldToPixel(WorldLocation, PixelPosition))
		{
			return;
		}

		const FIntRect  ViewRect    = Canvas->GetViewRect();
		constexpr float LabelMargin = 200.0f;
		if (PixelPosition.X < ViewRect.Min.X - LabelMargin || PixelPosition.X > ViewRect.Max.X + LabelMargin || PixelPosition.Y < ViewRect.Min.Y - LabelMargin || PixelPosition.Y > ViewRect.Max.Y + LabelMargin)
		{
			return;
		}

		Canvas->DrawShadowedString(static_cast<float>(PixelPosition.X) + 10.0f, static_cast<float>(PixelPosition.Y) + VerticalOffset, *Label, GEngine->GetSmallFont(), Color);
	}
}

void FStormMotionComponentVisualizer::DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	(void)View;

	const AVolumetricSuperStormActor* StormActor = GetStormActor(Component);
	if (!StormActor || !PDI)
	{
		return;
	}

	const FStormRenderData& RenderData = StormActor->GetStormRenderDataRef();
	if (!IsDebugVisualizationEnabled(RenderData))
	{
		return;
	}

	const UStormMaterialBinderComponent* Binder               = Cast<UStormMaterialBinderComponent>(Component);
	const TArray<float>&                 Boundaries           = RenderData.Motion.RingEndRadii01;
	const double                         OuterBrimWorldRadius = GetSafeOuterBrimWorldRadius(RenderData);

	for (int32 BoundaryIndex = 0; BoundaryIndex < Boundaries.Num(); ++BoundaryIndex)
	{
		const bool         bSelected        = Component == GetEditedComponent() && BoundaryIndex == SelectedBoundaryIndex;
		const FLinearColor BoundaryColor    = bSelected ? FLinearColor::Yellow : FLinearColor(0.8f, 0.8f, 0.8f, 0.35f);
		const double       BoundaryRadius01 = static_cast<double>(Boundaries[BoundaryIndex]);
		const double       BoundaryRadius   = OuterBrimWorldRadius * BoundaryRadius01;

		PDI->SetHitProxy(new HStormMotionBoundaryVisProxy(Component, BoundaryIndex));
		DrawCircleAtRadius(PDI, GetMotionSliceCenter(Binder, RenderData, BoundaryRadius01), BoundaryRadius, BoundaryColor, bSelected ? SelectedBoundaryThickness : BoundaryCenterThickness);
		PDI->DrawPoint(GetBoundaryHandleLocation(Binder, RenderData, BoundaryIndex), BoundaryColor, HandlePointSize, SDPG_Foreground);
		PDI->SetHitProxy(nullptr);
	}

	PDI->DrawPoint(GetMotionSliceCenter(Binder, RenderData, 0.0), FLinearColor::White, 8.0f, SDPG_Foreground);
}

void FStormMotionComponentVisualizer::DrawVisualizationHUD(const UActorComponent* Component, const FViewport* Viewport, const FSceneView* View, FCanvas* Canvas)
{
	(void)Viewport;

	const AVolumetricSuperStormActor* StormActor = GetStormActor(Component);
	if (!StormActor || !View || !Canvas)
	{
		return;
	}

	const FStormRenderData& RenderData = StormActor->GetStormRenderDataRef();
	if (!IsDebugVisualizationEnabled(RenderData))
	{
		return;
	}

	const UStormMaterialBinderComponent* Binder               = Cast<UStormMaterialBinderComponent>(Component);
	const TArray<float>&                 Boundaries           = RenderData.Motion.RingEndRadii01;
	const double                         OuterBrimWorldRadius = GetSafeOuterBrimWorldRadius(RenderData);
	const int32                          RingCount            = Boundaries.Num() + 1;
	for (int32 RingIndex = 0; RingIndex < RingCount; ++RingIndex)
	{
		const SavageSuperStorm::Motion::FStormRingInfluenceRange InfluenceRange        = SavageSuperStorm::Motion::GetRingInfluenceRange(RenderData.Motion, RingIndex);
		const double                                             LabelRadius01         = InfluenceRange.OuterRadius01;
		const double                                             InnerRadiusKilometers = OuterBrimWorldRadius * InfluenceRange.InnerRadius01 * CentimetersToKilometers;
		const double                                             OuterRadiusKilometers = OuterBrimWorldRadius * InfluenceRange.OuterRadius01 * CentimetersToKilometers;
		const double                                             AngularSpeedDegrees   = RenderData.Motion.RingAngularSpeedDegrees.IsValidIndex(RingIndex) ? static_cast<double>(RenderData.Motion.RingAngularSpeedDegrees[RingIndex]) : 0.0;
		const FString                                            Label                 = FString::Printf(TEXT("Ring %d Influence: %.3f - %.3f") TEXT("  (%.2f - %.2f km)  %.1f deg/s"), RingIndex + 1, InfluenceRange.InnerRadius01, InfluenceRange.OuterRadius01, InnerRadiusKilometers, OuterRadiusKilometers, AngularSpeedDegrees);

		DrawWorldLabel(View, Canvas, GetMotionSliceCenter(Binder, RenderData, LabelRadius01) + FVector(OuterBrimWorldRadius * LabelRadius01, 0.0, 0.0), Label, GetRingColor(RingIndex), (RingIndex % 2 == 0) ? -16.0f : 4.0f);
	}

	if (Component == GetEditedComponent() && Boundaries.IsValidIndex(SelectedBoundaryIndex))
	{
		const double BoundaryRadius01 = static_cast<double>(Boundaries[SelectedBoundaryIndex]);
		DrawWorldLabel(View, Canvas, GetBoundaryHandleLocation(Binder, RenderData, SelectedBoundaryIndex), FString::Printf(TEXT("> Edit Boundary %d Center: %.3f"), SelectedBoundaryIndex + 1, BoundaryRadius01), FLinearColor::Yellow, 22.0f);
	}
}

bool FStormMotionComponentVisualizer::VisProxyHandleClick(FEditorViewportClient* InViewportClient, HComponentVisProxy* VisProxy, const FViewportClick& Click)
{
	(void)InViewportClient;
	(void)Click;

	if (!VisProxy || !VisProxy->IsA(HStormMotionBoundaryVisProxy::StaticGetType()) || !VisProxy->Component.IsValid())
	{
		return false;
	}

	const HStormMotionBoundaryVisProxy*  BoundaryProxy = static_cast<HStormMotionBoundaryVisProxy*>(VisProxy);
	const UStormMaterialBinderComponent* Binder        = Cast<UStormMaterialBinderComponent>(VisProxy->Component.Get());
	const AVolumetricSuperStormActor*    StormActor    = Binder ? Cast<AVolumetricSuperStormActor>(Binder->GetOwner()) : nullptr;
	if (!StormActor)
	{
		return false;
	}

	const FStormRenderData& RenderData = StormActor->GetStormRenderDataRef();
	if (!IsDebugVisualizationEnabled(RenderData) || !RenderData.Motion.RingEndRadii01.IsValidIndex(BoundaryProxy->BoundaryIndex))
	{
		return false;
	}

	EditedComponentPath   = FComponentPropertyPath(Binder);
	SelectedBoundaryIndex = BoundaryProxy->BoundaryIndex;
	return true;
}

bool FStormMotionComponentVisualizer::GetWidgetLocation(const FEditorViewportClient* ViewportClient, FVector& OutLocation) const
{
	(void)ViewportClient;

	const UStormMaterialBinderComponent* Binder     = GetEditedBinder();
	const AVolumetricSuperStormActor*    StormActor = Binder ? Cast<AVolumetricSuperStormActor>(Binder->GetOwner()) : nullptr;
	if (!StormActor)
	{
		return false;
	}

	const FStormRenderData& RenderData = StormActor->GetStormRenderDataRef();
	if (!IsDebugVisualizationEnabled(RenderData) || !RenderData.Motion.RingEndRadii01.IsValidIndex(SelectedBoundaryIndex))
	{
		return false;
	}

	OutLocation = GetBoundaryHandleLocation(Binder, RenderData, SelectedBoundaryIndex);
	return true;
}

bool FStormMotionComponentVisualizer::GetCustomInputCoordinateSystem(const FEditorViewportClient* ViewportClient, FMatrix& OutMatrix) const
{
	(void)ViewportClient;

	if (!GetEditedBinder() || SelectedBoundaryIndex == INDEX_NONE)
	{
		return false;
	}

	OutMatrix = FMatrix::Identity;
	return true;
}

bool FStormMotionComponentVisualizer::HandleInputDelta(FEditorViewportClient* ViewportClient, FViewport* Viewport, FVector& DeltaTranslate, FRotator& DeltaRotate, FVector& DeltaScale)
{
	(void)ViewportClient;
	(void)Viewport;
	(void)DeltaRotate;
	(void)DeltaScale;

	UStormMaterialBinderComponent* Binder     = GetEditedBinder();
	AVolumetricSuperStormActor*    StormActor = GetMutableStormActor(Binder);
	if (!StormActor)
	{
		return false;
	}

	const FStormRenderData& RenderData = StormActor->GetStormRenderDataRef();
	if (!IsDebugVisualizationEnabled(RenderData) || !RenderData.Motion.RingEndRadii01.IsValidIndex(SelectedBoundaryIndex))
	{
		return false;
	}

	const double DeltaRadius = DeltaTranslate.X;
	if (!FMath::IsNearlyZero(DeltaRadius))
	{
		const double OuterBrimWorldRadius = GetSafeOuterBrimWorldRadius(RenderData);
		const double CurrentWorldRadius   = OuterBrimWorldRadius * static_cast<double>(RenderData.Motion.RingEndRadii01[SelectedBoundaryIndex]);
		const float  NewRadius01          = static_cast<float>(FMath::Max(0.0, CurrentWorldRadius + DeltaRadius) / OuterBrimWorldRadius);

		if (StormActor->SetMotionRingEndRadius01ForEditor(SelectedBoundaryIndex, NewRadius01))
		{
			bBoundaryChangedDuringTracking = true;
			if (GEditor)
			{
				GEditor->RedrawLevelEditingViewports(false);
			}
		}
	}

	return true;
}

void FStormMotionComponentVisualizer::TrackingStarted(FEditorViewportClient* InViewportClient)
{
	(void)InViewportClient;
	bBoundaryChangedDuringTracking = false;
}

void FStormMotionComponentVisualizer::TrackingStopped(FEditorViewportClient* InViewportClient, bool bInDidMove)
{
	(void)InViewportClient;

	if (bInDidMove && bBoundaryChangedDuringTracking)
	{
		if (UStormMaterialBinderComponent* Binder = GetEditedBinder())
		{
			if (AVolumetricSuperStormActor* StormActor = GetMutableStormActor(Binder))
			{
				StormActor->CommitMotionRingEndRadiiEditForEditor();
			}
		}
	}

	bBoundaryChangedDuringTracking = false;
}

void FStormMotionComponentVisualizer::EndEditing()
{
	EditedComponentPath.Reset();
	SelectedBoundaryIndex          = INDEX_NONE;
	bBoundaryChangedDuringTracking = false;
}

UActorComponent* FStormMotionComponentVisualizer::GetEditedComponent() const
{
	return EditedComponentPath.GetComponent();
}

UStormMaterialBinderComponent* FStormMotionComponentVisualizer::GetEditedBinder() const
{
	return Cast<UStormMaterialBinderComponent>(EditedComponentPath.GetComponent());
}