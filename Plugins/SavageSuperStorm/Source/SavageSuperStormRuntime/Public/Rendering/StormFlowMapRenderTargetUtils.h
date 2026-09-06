/**
 * @file StormFlowMapRenderTargetUtils.h
 * @brief Declares flow-map render-target creation and copy helpers.
 */

#pragma once

#include "CoreMinimal.h"

class UTexture;
class UTextureRenderTarget2D;
class UWorld;

struct SAVAGESUPERSTORMRUNTIME_API FStormFlowMapPaintParameters
{
	FVector2f    BrushCenterUV     = FVector2f(0.5f, 0.5f);
	float        BrushRadiusUV     = 0.05f;
	FVector3f    BrushDirectionUVW = FVector3f(1.0f, 0.0f, 0.0f);
	FLinearColor BrushEncodedRGBA  = FLinearColor(1.0f, 0.5f, 0.5f, 0.75f);
	float        BrushStrength     = 0.75f;
	float        BrushOpacity      = 0.35f;
	bool         bErase            = false;
	bool         bUseEncodedRGBA   = false;
};

namespace SavageSuperStorm::FlowMapRenderTargetUtils
{
	SAVAGESUPERSTORMRUNTIME_API UTextureRenderTarget2D* CreateSurface(UObject* Outer, FName BaseName, int32 Resolution);

	SAVAGESUPERSTORMRUNTIME_API bool ClearSurface(UTextureRenderTarget2D* Target);

	SAVAGESUPERSTORMRUNTIME_API bool StampSurface(UTextureRenderTarget2D* Target, const FStormFlowMapPaintParameters& PaintParameters);

	SAVAGESUPERSTORMRUNTIME_API bool BlitToSurface(UWorld* World, UTexture* Source, UTextureRenderTarget2D* Target);
}