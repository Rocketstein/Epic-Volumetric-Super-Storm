/**
 * @file StormFlowMapRenderTargetUtils.cpp
 * @brief Creates and copies flow-map render targets.
 */

#include "Rendering/StormFlowMapRenderTargetUtils.h"

#include "GlobalShader.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RenderingThread.h"
#include "SceneInterface.h"
#include "Shader_SavageSuperStorm.h"
#include "TextureResource.h"
#include "Engine/Canvas.h"
#include "Engine/Texture.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "UObject/Package.h"

namespace SavageSuperStorm::FlowMapRenderTargetUtils
{
	UTextureRenderTarget2D* CreateSurface(UObject* Outer, FName BaseName, int32 Resolution)
	{
		if (!Outer)
		{
			return nullptr;
		}

		const int32             SafeResolution = FMath::Clamp(Resolution, 32, 2048);
		const FName             ObjectName     = MakeUniqueObjectName(Outer, UTextureRenderTarget2D::StaticClass(), BaseName);
		UTextureRenderTarget2D* RenderTarget   = NewObject<UTextureRenderTarget2D>(Outer, ObjectName, RF_Transient);
		if (!RenderTarget)
		{
			return nullptr;
		}

		RenderTarget->ClearColor        = FLinearColor(0.5f, 0.5f, 0.5f, 0.0f);
		RenderTarget->bAutoGenerateMips = false;
		RenderTarget->bCanCreateUAV     = true;
		RenderTarget->SRGB              = false;
		RenderTarget->AddressX          = TA_Clamp;
		RenderTarget->AddressY          = TA_Clamp;
		RenderTarget->InitCustomFormat(SafeResolution, SafeResolution, PF_FloatRGBA, true);
		RenderTarget->UpdateResourceImmediate(true);
		return RenderTarget;
	}

	bool ClearSurface(UTextureRenderTarget2D* Target)
	{
		if (!Target)
		{
			return false;
		}

		FTextureRenderTargetResource* Resource = Target->GameThread_GetRenderTargetResource();
		if (!Resource)
		{
			return false;
		}

		const uint32 Resolution = static_cast<uint32>(Target->SizeX);
		ENQUEUE_RENDER_COMMAND(ClearStormFlowMapRT)(
			[Resource, Resolution](FRHICommandListImmediate& RHICmdList)
			{
				FRDGBuilder    GraphBuilder(RHICmdList);
				FRDGTextureRef Texture = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(Resource->GetRenderTargetTexture(), TEXT("RT_StormFlowMapClear")));
				FSavageSuperStormShaderInterface::AddFlowMapClearPass_RenderThread(GraphBuilder, GetGlobalShaderMap(GMaxRHIFeatureLevel), Resolution, Texture);
				GraphBuilder.Execute();
			}
		);
		return true;
	}

	bool StampSurface(UTextureRenderTarget2D* Target, const FStormFlowMapPaintParameters& PaintParameters)
	{
		if (!Target)
		{
			return false;
		}

		FTextureRenderTargetResource* Resource = Target->GameThread_GetRenderTargetResource();
		if (!Resource)
		{
			return false;
		}

		FSavageSuperStormFlowMapBrushPassParameters PassParameters;
		PassParameters.Resolution        = static_cast<uint32>(Target->SizeX);
		PassParameters.BrushCenterUV     = PaintParameters.BrushCenterUV;
		PassParameters.BrushRadiusUV     = PaintParameters.BrushRadiusUV;
		PassParameters.BrushDirectionUVW = PaintParameters.BrushDirectionUVW;
		PassParameters.BrushEncodedRGBA  = PaintParameters.BrushEncodedRGBA;
		PassParameters.BrushStrength     = PaintParameters.BrushStrength;
		PassParameters.BrushOpacity      = PaintParameters.BrushOpacity;
		PassParameters.bErase            = PaintParameters.bErase ? 1u : 0u;
		PassParameters.bUseEncodedRGBA   = PaintParameters.bUseEncodedRGBA ? 1u : 0u;

		ENQUEUE_RENDER_COMMAND(StampStormFlowMapBrushRT)(
			[Resource, PassParameters](FRHICommandListImmediate& RHICmdList)
			{
				FRDGBuilder    GraphBuilder(RHICmdList);
				FRDGTextureRef Texture = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(Resource->GetRenderTargetTexture(), TEXT("RT_StormFlowMapPaint")));
				FSavageSuperStormShaderInterface::AddFlowMapBrushPass_RenderThread(GraphBuilder, GetGlobalShaderMap(GMaxRHIFeatureLevel), PassParameters, Texture);
				GraphBuilder.Execute();
			}
		);
		return true;
	}

	bool BlitToSurface(UWorld* World, UTexture* Source, UTextureRenderTarget2D* Target)
	{
		if (!World || !World->Scene || !Source || !Target)
		{
			return false;
		}

		UCanvas*                   Canvas     = nullptr;
		FVector2D                  CanvasSize = FVector2D::ZeroVector;
		FDrawToRenderTargetContext Context;
		UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(World, Target, Canvas, CanvasSize, Context);
		const bool bDrew = Canvas != nullptr;
		if (bDrew)
		{
			Canvas->K2_DrawTexture(Source, FVector2D::ZeroVector, CanvasSize, FVector2D::ZeroVector, FVector2D::UnitVector, FLinearColor::White, BLEND_Opaque);
		}
		UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(World, Context);
		return bDrew;
	}
}