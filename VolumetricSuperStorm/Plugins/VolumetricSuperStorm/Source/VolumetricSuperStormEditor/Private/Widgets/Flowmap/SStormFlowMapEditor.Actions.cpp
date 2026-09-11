// Copyright 2026 GoroGoro. All Rights Reserved.

/**
 * @file SStormFlowMapEditor.Actions.cpp
 * @brief Implements flow-map painting, state, and asset operations.
 */

#include "Widgets/Flowmap/SStormFlowMapEditor.h"

#include "Editor.h"
#include "IDetailsView.h"
#include "InputCoreTypes.h"
#include "RenderingThread.h"
#include "ScopedTransaction.h"
#include "TextureResource.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Assets/StormAuthoringPaths.h"
#include "Assets/StormWindFlowMapDataAsset.h"
#include "Components/StormFlowMapComponent.h"
#include "Data/Flowmap/StormFlowmapParams.h"
#include "Data/StormRenderTargetResolution.h"
#include "Editor/EditorEngine.h"
#include "Editor/Transactor.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"
#include "UObject/Package.h"
#include "Widgets/Colors/SColorPicker.h"
#include "Widgets/Flowmap/SStormFlowMapLayerRibbon.h"

#define LOCTEXT_NAMESPACE "SStormFlowMapEditor"

namespace
{
	FString MakeFlowMapObjectPath(const FString& PackageName, const FString& AssetName)
	{
		return PackageName + TEXT(".") + AssetName;
	}

	int32 LayerIndex(int32 Layer)
	{
		return FMath::Clamp(Layer, 0, 2);
	}

	EStormFlowMapLayer RuntimeLayer(int32 Layer)
	{
		switch (LayerIndex(Layer))
		{
		case 0:
			return EStormFlowMapLayer::Lower;
		case 1:
			return EStormFlowMapLayer::Middle;
		default:
			return EStormFlowMapLayer::Upper;
		}
	}

	// Returns the existing flow map when the name is already taken by one, so Save As can
	// overwrite. Refuses only a name held by some other asset type.
	UStormWindFlowMapDataAsset* CreateOrLoadFlowMapAsset(const FString& PackageName, const FString& AssetName)
	{
		UPackage* Package = CreatePackage(*PackageName);
		if (!Package)
		{
			return nullptr;
		}
		Package->FullyLoad();

		const FString ObjectPath      = MakeFlowMapObjectPath(PackageName, AssetName);
		UObject*      ExistingObject  = StaticLoadObject(UObject::StaticClass(), nullptr, *ObjectPath);
		if (ExistingObject)
		{
			return Cast<UStormWindFlowMapDataAsset>(ExistingObject);
		}

		UStormWindFlowMapDataAsset* Asset = NewObject<UStormWindFlowMapDataAsset>(Package, *AssetName, RF_Public | RF_Standalone | RF_Transactional);
		if (!Asset)
		{
			return nullptr;
		}

		FAssetRegistryModule::AssetCreated(Asset);
		Package->MarkPackageDirty();
		return Asset;
	}

	bool ReplaceFlowMapDocumentWithUndoBoundary(UStormFlowMapComponent* Component, UStormWindFlowMapDataAsset* Asset, const FText& TransactionText)
	{
		if (!Component)
		{
			return false;
		}

		if (!GEditor || !GEditor->Trans)
		{
			return Component->ReplaceFlowMapDocument(Asset);
		}

		bool bReplaced = false;
		{
			const FScopedTransaction BoundaryTransaction(TransactionText);
			if (!BoundaryTransaction.IsOutstanding())
			{
				return false;
			}
			bReplaced = Component->ReplaceFlowMapDocument(Asset);
		}

		// The real transaction above discards any redo tail. The barrier then keeps
		// Undo from reaching commands that reference the released pixel document.
		GEditor->Trans->SetUndoBarrier();
		return bReplaced;
	}

	UTexture2D* CreateOrUpdateEmbeddedFlowTexture(UTextureRenderTarget2D* SourceRenderTarget, UStormWindFlowMapDataAsset* FlowMapAsset, FName TextureName, UTexture2D* ExistingTexture)
	{
		if (!SourceRenderTarget || !FlowMapAsset)
		{
			return nullptr;
		}

		FTextureRenderTargetResource* RenderTargetResource = SourceRenderTarget->GameThread_GetRenderTargetResource();
		if (!RenderTargetResource)
		{
			return nullptr;
		}

		TArray<FFloat16Color> Pixels;
		if (!RenderTargetResource->ReadFloat16Pixels(Pixels))
		{
			return nullptr;
		}

		const int32 SizeX = SourceRenderTarget->SizeX;
		const int32 SizeY = SourceRenderTarget->SizeY;
		if (Pixels.Num() != SizeX * SizeY)
		{
			return nullptr;
		}

		UTexture2D* Texture = ExistingTexture && ExistingTexture->GetOuter() == FlowMapAsset ? ExistingTexture : FindObject<UTexture2D>(FlowMapAsset, *TextureName.ToString());
		if (!Texture)
		{
			Texture = NewObject<UTexture2D>(FlowMapAsset, TextureName, RF_Public | RF_Transactional);
		}
		if (!Texture)
		{
			return nullptr;
		}

		Texture->Modify();
		Texture->PreEditChange(nullptr);
		Texture->Source.Init(SizeX, SizeY, 1, 1, TSF_RGBA16F, reinterpret_cast<const uint8*>(Pixels.GetData()));
		Texture->SRGB                = false;
		Texture->CompressionSettings = TC_HDR;
		Texture->MipGenSettings      = TMGS_NoMipmaps;
		Texture->AddressX            = TA_Clamp;
		Texture->AddressY            = TA_Clamp;
		Texture->PostEditChange();
		Texture->UpdateResource();
		return Texture;
	}
}

SStormFlowMapEditor::~SStormFlowMapEditor()
{
	if (bLayerHeightTransactionOpen)
	{
		HandleLayerHeightDragEnd();
	}
	if (bStrokeTransactionOpen)
	{
		HandleStrokeEnd();
	}
	if (GEditor)
	{
		GEditor->UnregisterForUndo(this);
	}
}

void SStormFlowMapEditor::PostUndo(bool bSuccess)
{
	ReplayAfterUndoOrRedo(bSuccess);
}

void SStormFlowMapEditor::PostRedo(bool bSuccess)
{
	ReplayAfterUndoOrRedo(bSuccess);
}

void SStormFlowMapEditor::HandleStrokeBegin()
{
	UStormFlowMapComponent* Component = FlowMapComponent.Get();
	if (bStrokeTransactionOpen || !Component || !GEditor)
	{
		return;
	}

	GEditor->BeginTransaction(LOCTEXT("PaintFlowMapStrokeTransaction", "Paint Storm Flow Map Stroke"));
	bStrokeTransactionOpen = true;
	if (!Component->BeginStroke())
	{
		GEditor->EndTransaction();
		bStrokeTransactionOpen = false;
	}
}

void SStormFlowMapEditor::HandleLayerHeightDragBegin()
{
	UStormFlowMapComponent* Component = FlowMapComponent.Get();
	if (bLayerHeightTransactionOpen || !Component || !GEditor)
	{
		return;
	}

	GEditor->BeginTransaction(LOCTEXT("AdjustFlowMapLayerHeightsTransaction", "Adjust Storm Flow Map Layer Heights"));
	bLayerHeightTransactionOpen = true;
	Component->Modify();
}

void SStormFlowMapEditor::HandleLayerHeightDragEnd()
{
	if (!bLayerHeightTransactionOpen)
	{
		return;
	}
	if (GEditor)
	{
		GEditor->EndTransaction();
	}
	bLayerHeightTransactionOpen = false;
}

bool SStormFlowMapEditor::TryCheckpointFlowMapHistory()
{
	UStormFlowMapComponent* Component = FlowMapComponent.Get();
	if (!Component || bStrokeTransactionOpen || bLayerHeightTransactionOpen ||
		!Component->IsFlowMapHistoryCheckpointRequired() ||
		!GEditor || !GEditor->Trans || GEditor->Trans->IsActive())
	{
		return false;
	}

	const bool bRebased = Component->RebaseFlowMapHistoryWithUndoBoundary(
		[Component]()
		{
			bool bBoundaryCommitted = false;
			{
				const FScopedTransaction BoundaryTransaction(
					LOCTEXT("CheckpointFlowMapHistoryTransaction", "Checkpoint Storm Flow Map History"));
				if (!BoundaryTransaction.IsOutstanding())
				{
					return false;
				}
				Component->AdvanceFlowMapEditSession();
				bBoundaryCommitted = true;
			}

			if (!bBoundaryCommitted || !GEditor || !GEditor->Trans)
			{
				return false;
			}
			GEditor->Trans->SetUndoBarrier();
			return true;
		});
	if (bRebased)
	{
		Invalidate(EInvalidateWidgetReason::Paint);
	}
	return bRebased;
}

void SStormFlowMapEditor::HandleStrokeEnd()
{
	if (!bStrokeTransactionOpen)
	{
		return;
	}
	if (UStormFlowMapComponent* Component = FlowMapComponent.Get())
	{
		Component->EndStroke();
	}
	if (GEditor)
	{
		GEditor->EndTransaction();
	}
	bStrokeTransactionOpen = false;
}

void SStormFlowMapEditor::ReplayAfterUndoOrRedo(bool bSuccess)
{
	if (!bSuccess)
	{
		return;
	}
	EStormFlowMapLayer AffectedLayer = GetActiveRuntimeLayer();
	if (UStormFlowMapComponent* Component = FlowMapComponent.Get();
		Component && Component->ReplayHistoryIfStale(&AffectedLayer))
	{
		SelectRuntimeLayer(AffectedLayer);
		Invalidate(EInvalidateWidgetReason::Paint);
		if (GEditor)
		{
			GEditor->RedrawLevelEditingViewports();
		}
	}
}

bool SStormFlowMapEditor::CommitWorkingSurfacesToAsset(UStormWindFlowMapDataAsset* DestinationAsset)
{
	if (!VolumetricSuperStorm::AuthoringPaths::CanOverwriteAsset(
			DestinationAsset,
			VolumetricSuperStorm::AuthoringPaths::ECategory::FlowMap))
	{
		return false;
	}

	UStormFlowMapComponent* Component = FlowMapComponent.Get();
	UStormWindFlowMapDataAsset* Asset         = DestinationAsset;
	UTextureRenderTarget2D*     LowerSurface  = GetWorkingSurface(ELayer::Bottom);
	UTextureRenderTarget2D*     MiddleSurface = GetWorkingSurface(ELayer::Middle);
	UTextureRenderTarget2D*     UpperSurface  = GetWorkingSurface(ELayer::Top);
	if (!Component || !Asset || !LowerSurface || !MiddleSurface || !UpperSurface)
	{
		return false;
	}
	const int32 RequiredResolution = VolumetricSuperStorm::RenderTargetResolution::FlowMap;
	if (LowerSurface->SizeX != RequiredResolution || LowerSurface->SizeY != RequiredResolution ||
		MiddleSurface->SizeX != RequiredResolution || MiddleSurface->SizeY != RequiredResolution ||
		UpperSurface->SizeX != RequiredResolution || UpperSurface->SizeY != RequiredResolution)
	{
		FMessageDialog::Open(
			EAppMsgType::Ok,
			FText::Format(
				LOCTEXT("ResolutionChanged", "The component's flow-map document is not using the required {0} x {0} resolution. Reload the document before baking."),
				FText::AsNumber(RequiredResolution)));
		return false;
	}

	FlushRenderingCommands();
	Asset->Modify();

	UTexture2D* BottomTexture = CreateOrUpdateEmbeddedFlowTexture(LowerSurface, Asset, TEXT("BottomFlowMapTexture"), Asset->BottomFlowMap);
	UTexture2D* MiddleTexture = CreateOrUpdateEmbeddedFlowTexture(MiddleSurface, Asset, TEXT("MiddleFlowMapTexture"), Asset->MiddleFlowMap);
	UTexture2D* TopTexture    = CreateOrUpdateEmbeddedFlowTexture(UpperSurface, Asset, TEXT("TopFlowMapTexture"), Asset->TopFlowMap);
	if (!BottomTexture || !MiddleTexture || !TopTexture)
	{
		return false;
	}

	Asset->BottomFlowMap = BottomTexture;
	Asset->MiddleFlowMap = MiddleTexture;
	Asset->TopFlowMap    = TopTexture;
	Asset->Params = Component->WorkingParams;
	if (Component->PersistentFlowMapAsset == Asset)
	{
		Component->MarkSavedToAsset();
	}
	Asset->MarkPackageDirty();
	return true;
}

bool SStormFlowMapEditor::HasUnsavedPaint() const
{
	const UStormFlowMapComponent* Component = FlowMapComponent.Get();
	return Component && Component->IsUnsavedToAsset();
}

bool SStormFlowMapEditor::ConfirmClose()
{
	if (!HasUnsavedPaint())
	{
		return true;
	}

	const EAppReturnType::Type Choice = FMessageDialog::Open(EAppMsgType::YesNoCancel, LOCTEXT("UnsavedComponentPaint", "This actor has unsaved flow-map changes.\n\nBake them into the targeted asset before closing?"), LOCTEXT("FlowMapEditorTitle", "Storm Flow Map Editor"));
	if (Choice == EAppReturnType::Cancel)
	{
		return false;
	}
	if (Choice == EAppReturnType::Yes)
	{
		const EAssetOperationResult Result = SaveCurrentFlowMapAsset();
		if (Result == EAssetOperationResult::Failed)
		{
			ShowAssetOperationFailure(LOCTEXT("CloseSaveFailed", "Could not bake the flow-map working surfaces into the asset."));
		}
		return Result == EAssetOperationResult::Succeeded && !HasUnsavedPaint();
	}
	return true;
}

void SStormFlowMapEditor::SetTarget(UStormFlowMapComponent* InComponent)
{
	if (bLayerHeightTransactionOpen)
	{
		HandleLayerHeightDragEnd();
	}
	if (bStrokeTransactionOpen)
	{
		HandleStrokeEnd();
	}
	FlowMapComponent = InComponent;
	if (InComponent)
	{
		InComponent->EnsureFlowMapsInitialized();
	}
	if (DetailsView.IsValid())
	{
		DetailsView->SetObject(InComponent);
	}
	Invalidate(EInvalidateWidgetReason::Layout);
}

void SStormFlowMapEditor::StampFlow(FVector2D UV, FVector2D DirectionUV)
{
	if (UStormFlowMapComponent* Component = FlowMapComponent.Get())
	{
		Component->StampBrush(RuntimeLayer(static_cast<int32>(ActiveLayer)), UV, DirectionUV, GetBrushRadiusUV(), VerticalDirection, BrushStrength, BrushOpacity, RGBAColor, bErase, BrushInputMode == EBrushInputMode::EncodedRGBA);
	}
}

FReply SStormFlowMapEditor::SelectLayer(ELayer NewLayer)
{
	ActiveLayer = NewLayer;
	return FReply::Handled();
}

void SStormFlowMapEditor::SelectRuntimeLayer(EStormFlowMapLayer NewLayer)
{
	switch (NewLayer)
	{
	case EStormFlowMapLayer::Lower:
		SelectLayer(ELayer::Bottom);
		break;
	case EStormFlowMapLayer::Middle:
		SelectLayer(ELayer::Middle);
		break;
	default:
		SelectLayer(ELayer::Top);
		break;
	}
}

void SStormFlowMapEditor::SetLayerHeights(FVector3f NewHeights)
{
	const FVector3f Heights = VolumetricSuperStorm::FlowMap::SanitizeLayerHeights(NewHeights);

	if (UStormFlowMapComponent* Component = FlowMapComponent.Get())
	{
		FStormFlowmapParams Params = Component->WorkingParams;
		Params.LowerLayerHeight    = Heights.X;
		Params.MiddleLayerHeight   = Heights.Y;
		Params.UpperLayerHeight    = Heights.Z;
		Component->SetWorkingParams(Params);
	}
}

FReply SStormFlowMapEditor::ClearActiveLayer()
{
	if (UStormFlowMapComponent* Component = FlowMapComponent.Get())
	{
		const FScopedTransaction Transaction(LOCTEXT("ClearFlowMapLayerTransaction", "Clear Storm Flow Map Layer"));
		Component->ClearLayer(RuntimeLayer(static_cast<int32>(ActiveLayer)));
	}
	return FReply::Handled();
}

FReply SStormFlowMapEditor::RevertFromAsset()
{
	if (HasUnsavedPaint() && FMessageDialog::Open(EAppMsgType::YesNo, LOCTEXT("ConfirmRevert", "Discard all unsaved flow-map changes?")) != EAppReturnType::Yes)
	{
		return FReply::Handled();
	}

	if (UStormFlowMapComponent* Component = FlowMapComponent.Get())
	{
		ReplaceFlowMapDocumentWithUndoBoundary(Component, Component->PersistentFlowMapAsset, LOCTEXT("RevertFlowMapDocument", "Revert Storm Flow Map"));
	}
	return FReply::Handled();
}

FReply SStormFlowMapEditor::SaveAsset()
{
	if (SaveCurrentFlowMapAsset() == EAssetOperationResult::Failed)
	{
		ShowAssetOperationFailure(LOCTEXT("SaveFailed", "Could not bake the flow-map working surfaces into the asset."));
	}
	return FReply::Handled();
}

FReply SStormFlowMapEditor::SaveAssetAs()
{
	if (SaveAsFlowMapAsset() == EAssetOperationResult::Failed)
	{
		ShowAssetOperationFailure(LOCTEXT("SaveAsFailed", "Could not create and bake the new flow-map asset."));
	}
	return FReply::Handled();
}

FReply SStormFlowMapEditor::LoadAsset()
{
	if (LoadFlowMapAsset() == EAssetOperationResult::Failed)
	{
		ShowAssetOperationFailure(LOCTEXT("LoadFailed", "Could not load the selected flow-map asset."));
	}
	return FReply::Handled();
}

SStormFlowMapEditor::EAssetOperationResult SStormFlowMapEditor::SaveCurrentFlowMapAsset()
{
	UStormFlowMapComponent* Component = FlowMapComponent.Get();
	if (!Component)
	{
		return EAssetOperationResult::Failed;
	}

	UStormWindFlowMapDataAsset* Asset = Component->PersistentFlowMapAsset;
	if (!Asset)
	{
		return SaveAsFlowMapAsset();
	}

	return CommitWorkingSurfacesToAsset(Asset) ? EAssetOperationResult::Succeeded : EAssetOperationResult::Failed;
}

SStormFlowMapEditor::EAssetOperationResult SStormFlowMapEditor::SaveAsFlowMapAsset()
{
	UStormFlowMapComponent* Component = FlowMapComponent.Get();
	if (!Component)
	{
		return EAssetOperationResult::Failed;
	}

	FString PackageName;
	FString AssetName;
	if (!VolumetricSuperStorm::AuthoringPaths::PickSaveNameModal(
			VolumetricSuperStorm::AuthoringPaths::ECategory::FlowMap,
			UStormWindFlowMapDataAsset::StaticClass(),
			TEXT("FM_StormFlowMap"),
			INVTEXT("Save Storm Flow Map As"),
			PackageName,
			AssetName))
	{
		return EAssetOperationResult::Cancelled;
	}

	UStormWindFlowMapDataAsset* NewAsset = CreateOrLoadFlowMapAsset(PackageName, AssetName);
	if (!NewAsset || !CommitWorkingSurfacesToAsset(NewAsset))
	{
		return EAssetOperationResult::Failed;
	}

	if (!Component->AdoptBakedFlowMapAsset(NewAsset))
	{
		return EAssetOperationResult::Failed;
	}
	if (DetailsView.IsValid())
	{
		DetailsView->ForceRefresh();
	}
	return EAssetOperationResult::Succeeded;
}

SStormFlowMapEditor::EAssetOperationResult SStormFlowMapEditor::LoadFlowMapAsset()
{
	UStormFlowMapComponent* Component = FlowMapComponent.Get();
	if (!Component)
	{
		return EAssetOperationResult::Failed;
	}

	UStormWindFlowMapDataAsset* SelectedAsset =
		VolumetricSuperStorm::AuthoringPaths::PickAssetModal<UStormWindFlowMapDataAsset>(
			VolumetricSuperStorm::AuthoringPaths::ECategory::FlowMap,
			INVTEXT("Load Storm Flow Map"));
	if (!SelectedAsset)
	{
		return EAssetOperationResult::Cancelled;
	}

	if (HasUnsavedPaint() && FMessageDialog::Open(EAppMsgType::YesNo, LOCTEXT("ConfirmLoad", "Discard the current unsaved flow-map changes and load the selected asset?")) != EAppReturnType::Yes)
	{
		return EAssetOperationResult::Cancelled;
	}

	const bool bInitialized = ReplaceFlowMapDocumentWithUndoBoundary(Component, SelectedAsset, LOCTEXT("LoadFlowMapDocument", "Load Storm Flow Map"));
	if (!bInitialized)
	{
		return EAssetOperationResult::Failed;
	}

	Component->MarkPackageDirty();
	if (DetailsView.IsValid())
	{
		DetailsView->ForceRefresh();
	}
	Invalidate(EInvalidateWidgetReason::Paint);
	return EAssetOperationResult::Succeeded;
}

void SStormFlowMapEditor::ShowAssetOperationFailure(const FText& Message) const
{
	FMessageDialog::Open(EAppMsgType::Ok, Message);
}

void SStormFlowMapEditor::SetPaintMode(ECheckBoxState NewState)
{
	if (NewState == ECheckBoxState::Checked)
	{
		bErase = false;
	}
}

void SStormFlowMapEditor::SetEraseMode(ECheckBoxState NewState)
{
	if (NewState == ECheckBoxState::Checked)
	{
		bErase = true;
	}
}

void SStormFlowMapEditor::SetDirectionBrushMode(ECheckBoxState NewState)
{
	if (NewState == ECheckBoxState::Checked)
	{
		BrushInputMode = EBrushInputMode::DragDirection;
	}
}

void SStormFlowMapEditor::SetRGBABrushMode(ECheckBoxState NewState)
{
	if (NewState == ECheckBoxState::Checked)
	{
		BrushInputMode = EBrushInputMode::EncodedRGBA;
	}
}

void SStormFlowMapEditor::SetBrushRadius(float NewValue)
{
	BrushRadiusTexels = FMath::Clamp(NewValue, 1.0f, static_cast<float>(VolumetricSuperStorm::RenderTargetResolution::FlowMap));
}

void SStormFlowMapEditor::SetBrushStrength(float NewValue)
{
	BrushStrength = FMath::Clamp(NewValue, 0.0f, 1.0f);
}

void SStormFlowMapEditor::SetBrushOpacity(float NewValue)
{
	BrushOpacity = FMath::Clamp(NewValue, 0.0f, 1.0f);
}

void SStormFlowMapEditor::SetVerticalDirection(float NewValue)
{
	VerticalDirection = FMath::Clamp(NewValue, -1.0f, 1.0f);
}

FReply SStormFlowMapEditor::OpenRGBAColorPicker(const FGeometry&, const FPointerEvent& Event)
{
	if (Event.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}

	FColorPickerArgs PickerArgs;
	PickerArgs.ParentWidget     = AsShared();
	PickerArgs.InitialColor     = RGBAColor;
	PickerArgs.bUseAlpha        = true;
	PickerArgs.bClampValue      = true;
	PickerArgs.sRGBOverride     = false;
	PickerArgs.OnColorCommitted = FOnLinearColorValueChanged::CreateSP(this, &SStormFlowMapEditor::SetRGBAColor);
	OpenColorPicker(PickerArgs);
	return FReply::Handled();
}

void SStormFlowMapEditor::SetRGBAColor(FLinearColor NewColor)
{
	RGBAColor = FLinearColor(FMath::Clamp(NewColor.R, 0.0f, 1.0f), FMath::Clamp(NewColor.G, 0.0f, 1.0f), FMath::Clamp(NewColor.B, 0.0f, 1.0f), FMath::Clamp(NewColor.A, 0.0f, 1.0f));
}

UTextureRenderTarget2D* SStormFlowMapEditor::GetWorkingSurface(ELayer Layer) const
{
	const UStormFlowMapComponent* Component = FlowMapComponent.Get();
	return Component
		? Component->GetLayerRenderTarget(RuntimeLayer(static_cast<int32>(Layer)))
		: nullptr;
}

UTextureRenderTarget2D* SStormFlowMapEditor::GetActiveRenderTarget() const
{
	return GetWorkingSurface(ActiveLayer);
}

float SStormFlowMapEditor::GetBrushRadiusUV() const
{
	return BrushRadiusTexels /
		static_cast<float>(VolumetricSuperStorm::RenderTargetResolution::FlowMap);
}

FText SStormFlowMapEditor::GetActiveLayerLabel() const
{
	switch (ActiveLayer)
	{
	case ELayer::Bottom:
		return LOCTEXT("BottomLayerLabel", "Lower");
	case ELayer::Middle:
		return LOCTEXT("MiddleLayerLabel", "Middle");
	case ELayer::Top:
		return LOCTEXT("TopLayerLabel", "Upper");
	default:
		return FText::GetEmpty();
	}
}

FText SStormFlowMapEditor::GetStatusText() const
{
	const UStormFlowMapComponent* Component = FlowMapComponent.Get();
	if (Component)
	{
		const FText AssetLabel = Component->PersistentFlowMapAsset ? FText::FromString(Component->PersistentFlowMapAsset->GetName()) : LOCTEXT("TransientFlowMap", "Unsaved transient flow map");
		return FText::Format(HasUnsavedPaint() ? LOCTEXT("ActorDirtyStatus", "{0} | {1} | {2} x {2} | unsaved changes") : LOCTEXT("ActorCleanStatus", "{0} | {1} | {2} x {2}"), AssetLabel, GetActiveLayerLabel(), FText::AsNumber(VolumetricSuperStorm::RenderTargetResolution::FlowMap));
	}
	return LOCTEXT("NoComponentStatus", "No flow-map component selected");
}

FText SStormFlowMapEditor::GetSaveButtonText() const
{
	const UStormFlowMapComponent* Component = FlowMapComponent.Get();
	return Component && Component->PersistentFlowMapAsset
		? LOCTEXT("BakeToAsset", "Bake to Asset")
		: LOCTEXT("BakeAs", "Bake As...");
}

bool SStormFlowMapEditor::CanSaveToCurrentAsset() const
{
	const UStormFlowMapComponent* Component = FlowMapComponent.Get();
	if (!Component)
	{
		return false;
	}

	return !Component->PersistentFlowMapAsset ||
		VolumetricSuperStorm::AuthoringPaths::CanOverwriteAsset(
			Component->PersistentFlowMapAsset,
			VolumetricSuperStorm::AuthoringPaths::ECategory::FlowMap);
}

FText SStormFlowMapEditor::GetSaveButtonTooltip() const
{
	const UStormFlowMapComponent* Component = FlowMapComponent.Get();
	if (Component && Component->PersistentFlowMapAsset &&
		!VolumetricSuperStorm::AuthoringPaths::CanOverwriteAsset(
			Component->PersistentFlowMapAsset,
			VolumetricSuperStorm::AuthoringPaths::ECategory::FlowMap))
	{
		return VolumetricSuperStorm::AuthoringPaths::GetShippedAssetOverwriteBlockedText();
	}

	return LOCTEXT(
		"SaveTooltip",
		"Bake the component's working flow maps into its targeted asset. Opens Save As when no asset is targeted.");
}

FVector3f SStormFlowMapEditor::GetLayerHeights() const
{
	if (const UStormFlowMapComponent* Component = FlowMapComponent.Get())
	{
		return VolumetricSuperStorm::FlowMap::GetLayerHeights(Component->WorkingParams);
	}

	return VolumetricSuperStorm::FlowMap::GetLayerHeights(FStormFlowmapParams());
}

EStormFlowMapLayer SStormFlowMapEditor::GetActiveRuntimeLayer() const
{
	return RuntimeLayer(static_cast<int32>(ActiveLayer));
}

FSlateColor SStormFlowMapEditor::GetLayerButtonColor(ELayer Layer) const
{
	return Layer == ActiveLayer ? FSlateColor(SStormFlowMapLayerRibbon::GetLayerColor(RuntimeLayer(static_cast<int32>(Layer)))) : FSlateColor(FLinearColor::White);
}

ECheckBoxState SStormFlowMapEditor::GetPaintModeState() const
{
	return bErase ? ECheckBoxState::Unchecked : ECheckBoxState::Checked;
}

ECheckBoxState SStormFlowMapEditor::GetEraseModeState() const
{
	return bErase ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

ECheckBoxState SStormFlowMapEditor::GetDirectionBrushModeState() const
{
	return IsDirectionBrushMode() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

ECheckBoxState SStormFlowMapEditor::GetRGBABrushModeState() const
{
	return BrushInputMode == EBrushInputMode::EncodedRGBA ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

bool SStormFlowMapEditor::IsDirectionBrushMode() const
{
	return BrushInputMode == EBrushInputMode::DragDirection;
}

FLinearColor SStormFlowMapEditor::GetRGBAColor() const
{
	return RGBAColor;
}

FText SStormFlowMapEditor::GetRGBAValueText() const
{
	return FText::FromString(FString::Printf(TEXT("R %.2f  G %.2f  B %.2f  A %.2f"), RGBAColor.R, RGBAColor.G, RGBAColor.B, RGBAColor.A));
}

EVisibility SStormFlowMapEditor::GetRGBAControlsVisibility() const
{
	return BrushInputMode == EBrushInputMode::EncodedRGBA ? EVisibility::Visible : EVisibility::Collapsed;
}

#undef LOCTEXT_NAMESPACE
