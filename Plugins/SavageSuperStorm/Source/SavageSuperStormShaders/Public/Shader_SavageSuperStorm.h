/**
 * @file Shader_SavageSuperStorm.h
 * @brief Declares render-graph compute passes used by the storm renderer.
 */

#pragma once

#include "CoreMinimal.h"
#include "RenderGraphFwd.h"

class FRDGBuilder;
class FGlobalShaderMap;

struct FSavageSuperStormShapePassParameters
{
	uint32    Resolution  = 512;
	FVector2f StormExtent = FVector2f(8000.0f, 8000.0f);
	float     StormRadius = 4000.0f;

	float EnvelopeRadius  = 100.f;
	float EnvelopeFalloff = 10.f;

	float             OuterBrimRadiusScale    = 1.35f;
	FVector2f         FallbackWindDirectionXY = FVector2f(1.0f, 0.0f);
	uint32            bUpperFlowMapEnabled    = 0u;
	TArray<FVector4f> CoverageStrengthCurveLUT;
	TArray<FVector4f> TypeStrengthCurveLUT;
	TArray<FVector4f> LayerHeightCurveLUT;
};

struct FSavageSuperStormBottomTypePassParameters
{
	uint32 Resolution = 64;
	float  BottomFade = 0.1f;
};

struct FSavageSuperStormTopTypePassParameters
{
	uint32        Resolution = 64;
	float         TopFade    = 0.1f;
	TArray<float> TopHeightLUT;
};

struct FSavageSuperStormAnvilProfilePassParameters
{
	uint32        Resolution = 64;
	float         Fade       = 0.1f;
	TArray<float> HeightLUT;
};

struct FSavageSuperStormProfileBrushPassParameters
{
	uint32    Resolution    = 64;
	FVector2f BrushCenterUV = FVector2f(0.5f, 0.5f);
	float     BrushRadiusUV = 0.05f;
	float     BrushStrength = 0.8f;
	float     BrushValue    = 1.0f;
	uint32    bErase        = 0;
	uint32    bOverwrite    = 1;
};

struct FSavageSuperStormProfileBlendPassParameters
{
	uint32 Resolution = 64;
	float  Alpha      = 0.0f;
};

struct FSavageSuperStormFlowMapBrushPassParameters
{
	uint32       Resolution        = 256;
	FVector2f    BrushCenterUV     = FVector2f(0.5f, 0.5f);
	float        BrushRadiusUV     = 0.05f;
	FVector3f    BrushDirectionUVW = FVector3f(1.0f, 0.0f, 0.0f);
	FLinearColor BrushEncodedRGBA  = FLinearColor(1.0f, 0.5f, 0.5f, 0.75f);
	float        BrushStrength     = 0.75f;
	float        BrushOpacity      = 0.35f;
	uint32       bErase            = 0;
	uint32       bUseEncodedRGBA   = 0;
};

class SAVAGESUPERSTORMSHADERS_API FSavageSuperStormShaderInterface
{
public:
	static void AddShapePass_RenderThread(FRDGBuilder& GraphBuilder, FGlobalShaderMap* InShaderMap, const FSavageSuperStormShapePassParameters& InPassParameters, FRDGTextureRef InUpperFlowMapTexture, FRDGTextureRef InTextureRef, FRDGTextureRef InTextureRef2);

	static void AddBottomTypeProfilePass_RenderThread(FRDGBuilder& GraphBuilder, FGlobalShaderMap* InShaderMap, const FSavageSuperStormBottomTypePassParameters& InPassParameters, FRDGTextureRef InTextureRef);

	static void AddTopTypeProfilePass_RenderThread(FRDGBuilder& GraphBuilder, FGlobalShaderMap* InShaderMap, const FSavageSuperStormTopTypePassParameters& InPassParameters, FRDGTextureRef InTextureRef);

	static void AddAnvilProfilePass_RenderThread(FRDGBuilder& GraphBuilder, FGlobalShaderMap* InShaderMap, const FSavageSuperStormAnvilProfilePassParameters& InPassParameters, FRDGTextureRef InTextureRef);

	static void AddProfileBrushPass_RenderThread(FRDGBuilder& GraphBuilder, FGlobalShaderMap* InShaderMap, const FSavageSuperStormProfileBrushPassParameters& InPassParameters, FRDGTextureRef InPaintedTopTexture);

	static void AddProfileBlendPass_RenderThread(FRDGBuilder& GraphBuilder, FGlobalShaderMap* InShaderMap, const FSavageSuperStormProfileBlendPassParameters& InPassParameters, FRDGTextureRef InProfileA, FRDGTextureRef InProfileB, FRDGTextureRef OutBlendedProfileTexture);

	static void AddFlowMapClearPass_RenderThread(FRDGBuilder& GraphBuilder, FGlobalShaderMap* InShaderMap, uint32 InResolution, FRDGTextureRef InOutFlowMap);

	static void AddFlowMapBrushPass_RenderThread(FRDGBuilder& GraphBuilder, FGlobalShaderMap* InShaderMap, const FSavageSuperStormFlowMapBrushPassParameters& InPassParameters, FRDGTextureRef InOutFlowMap);
};