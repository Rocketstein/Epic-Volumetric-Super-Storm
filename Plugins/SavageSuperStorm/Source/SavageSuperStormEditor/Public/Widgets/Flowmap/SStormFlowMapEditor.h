/**
 * @file SStormFlowMapEditor.h
 * @brief Declares the flow-map painting and asset-management panel.
 */

#pragma once

#include "CoreMinimal.h"
#include "Components/StormFlowMapComponent.h"
#include "EditorUndoClient.h"
#include "Types/SlateEnums.h"
#include "Widgets/SCompoundWidget.h"

class UStormWindFlowMapDataAsset;
class UStormFlowMapComponent;
class UTextureRenderTarget2D;
class IDetailsView;

class SStormFlowMapEditor final : public SCompoundWidget, public FEditorUndoClient
{
public:
	SLATE_BEGIN_ARGS(SStormFlowMapEditor) : _FlowMapComponent(nullptr)
		{}

		SLATE_ARGUMENT(UStormFlowMapComponent*, FlowMapComponent)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SStormFlowMapEditor() override;
	virtual void Tick(
		const FGeometry& AllottedGeometry,
		double InCurrentTime,
		float InDeltaTime) override;

	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;

	bool HasUnsavedPaint() const;
	bool ConfirmClose();
	void SetTarget(UStormFlowMapComponent* InComponent);

private:
	enum class ELayer : uint8
	{
		Bottom,
		Middle,
		Top,
	};

	enum class EBrushInputMode : uint8
	{
		DragDirection,
		EncodedRGBA,
	};

	enum class EAssetOperationResult : uint8
	{
		Succeeded,
		Cancelled,
		Failed,
	};

	bool CommitWorkingSurfacesToAsset(UStormWindFlowMapDataAsset* DestinationAsset);
	void HandleStrokeBegin();
	void HandleStrokeEnd();
	void HandleLayerHeightDragBegin();
	void HandleLayerHeightDragEnd();
	bool TryCheckpointFlowMapHistory();
	void ReplayAfterUndoOrRedo(bool bSuccess);
	void StampFlow(FVector2D UV, FVector2D DirectionUV);

	FReply                SelectLayer(ELayer NewLayer);
	void                  SelectRuntimeLayer(EStormFlowMapLayer NewLayer);
	void                  SetLayerHeights(FVector3f NewHeights);
	FReply                ClearActiveLayer();
	FReply                RevertFromAsset();
	FReply                SaveAsset();
	FReply                SaveAssetAs();
	FReply                LoadAsset();
	EAssetOperationResult SaveCurrentFlowMapAsset();
	EAssetOperationResult SaveAsFlowMapAsset();
	EAssetOperationResult LoadFlowMapAsset();
	void                  ShowAssetOperationFailure(const FText& Message) const;
	void                  SetPaintMode(ECheckBoxState NewState);
	void                  SetEraseMode(ECheckBoxState NewState);
	void                  SetDirectionBrushMode(ECheckBoxState NewState);
	void                  SetRGBABrushMode(ECheckBoxState NewState);
	void                  SetBrushRadius(float NewValue);
	void                  SetBrushStrength(float NewValue);
	void                  SetBrushOpacity(float NewValue);
	void                  SetVerticalDirection(float NewValue);
	FReply                OpenRGBAColorPicker(const FGeometry& Geometry, const FPointerEvent& Event);
	void                  SetRGBAColor(FLinearColor NewColor);

	UTextureRenderTarget2D*     GetWorkingSurface(ELayer Layer) const;
	UTextureRenderTarget2D*     GetActiveRenderTarget() const;
	float                       GetBrushRadiusUV() const;
	FText                       GetActiveLayerLabel() const;
	FText                       GetStatusText() const;
	FText                       GetSaveButtonText() const;
	FVector3f                   GetLayerHeights() const;
	EStormFlowMapLayer          GetActiveRuntimeLayer() const;
	FSlateColor                 GetLayerButtonColor(ELayer Layer) const;
	ECheckBoxState              GetPaintModeState() const;
	ECheckBoxState              GetEraseModeState() const;
	ECheckBoxState              GetDirectionBrushModeState() const;
	ECheckBoxState              GetRGBABrushModeState() const;
	bool                        IsDirectionBrushMode() const;
	FLinearColor                GetRGBAColor() const;
	FText                       GetRGBAValueText() const;
	EVisibility                 GetRGBAControlsVisibility() const;

	TWeakObjectPtr<UStormFlowMapComponent>     FlowMapComponent;
	TSharedPtr<IDetailsView>                   DetailsView;
	ELayer                                     ActiveLayer                 = ELayer::Bottom;
	EBrushInputMode                            BrushInputMode              = EBrushInputMode::DragDirection;
	float                                      BrushRadiusTexels           = 24.0f;
	float                                      BrushStrength               = 0.75f;
	float                                      BrushOpacity                = 0.35f;
	float                                      VerticalDirection           = 0.0f;
	FLinearColor                               RGBAColor                   = FLinearColor(1.0f, 0.5f, 0.5f, 0.75f);
	bool                                       bErase                      = false;
	bool                                       bStrokeTransactionOpen      = false;
	bool                                       bLayerHeightTransactionOpen = false;
};
