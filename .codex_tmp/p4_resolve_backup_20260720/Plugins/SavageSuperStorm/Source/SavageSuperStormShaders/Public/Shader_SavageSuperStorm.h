#pragma once

#include "CoreMinimal.h"
#include "RenderGraphFwd.h"

class FRDGBuilder;
class FGlobalShaderMap;

struct FSavageSuperStormShapePassParameters
{
	uint32 Resolution		= 512;
	FVector2f StormExtent	= FVector2f(8000.0f, 8000.0f);
	float StormRadius		= 4000.0f;

	float EnvelopeRadius	= 100.f;
	float EnvelopeFalloff	= 10.f;
	float OuterBrimRadiusScale = 1.35f;
	float CoveragePower = 0.65f;
	float CoverageFloor = 0.30f;
	float CoverageCeiling = 0.80f;
	float StratusShelfFraction = 0.65f;

	float CumulusShelfFraction = 0.10f;
	float RotationSign = 1.0f;
	float VortexDentRegionRadius01 = 0.62f;
	float VortexDentInnerRadius01 = 0.10f;
	float VortexDentTwistRadians = 5.5f;
	float VortexDentTwistPower = 1.7f;
	float VortexDentCellFrequency = 6.4f;
	float VortexDentOccupancy = 0.35f;
	float VortexDentBlobRadiusMinCell = 0.28f;
	float VortexDentBlobRadiusMaxCell = 0.46f;
	float VortexDentTendrilStretch = 2.0f;
	uint32 VortexDentSeed = 1337u;
};

struct FSavageSuperStormBottomTypePassParameters
{
	uint32 Resolution = 64;
	float BottomFade = 0.1f;
	float VerticalVoidOffset = 0.1f;
};

struct FSavageSuperStormTopTypePassParameters
{
	uint32 Resolution = 64;
	uint32 bUseCurveLUT = 0;
	float VerticalVoidOffset = 0.1f;
	float TopFade = 0.1f;
	// Effective analytic lower fade. Used only to reserve a full-density
	// interior when the top and bottom profiles are multiplied.
	float BottomFade = 0.1f;

	// Packed per type: x = Stratus, y = Stratocumulus, z = Cumulus.
	FVector3f TypeHeights = FVector3f(0.1f, 0.4f, 0.7f);
	FVector3f TypeWeights = FVector3f(0.2f, 0.3f, 0.5f);

	float StratusToStratocumulusBlendStrength = 0.5f;
	float StratocumulusToCumulusBlendStrength = 0.3f;

	TArray<float> TopHeightLUT;
};

struct FSavageSuperStormProfileBrushPassParameters
{
	uint32 Resolution = 64;
	FVector2f BrushCenterUV = FVector2f(0.5f, 0.5f);
	float BrushRadiusUV = 0.05f;
	float BrushStrength = 0.8f;
	float BrushValue = 1.0f;
	uint32 bErase = 0;
	uint32 bOverwrite = 1;
};

class SAVAGESUPERSTORMSHADERS_API FSavageSuperStormShaderInterface
{
public:
	static void AddPass_RenderThread(FRDGBuilder& GraphBuilder, FGlobalShaderMap* InShaderMap,
		uint32 InResolution,
		const FVector2f& InScale,
		const FVector2f& InOrigin,
		const FVector2f& InLocation,
		float InRadius,
		FRDGTextureRef InTextureRef);


	static void AddShapePass_RenderThread(
		FRDGBuilder& GraphBuilder,
		FGlobalShaderMap* InShaderMap,
		const FSavageSuperStormShapePassParameters& InPassParameters,
		FRDGTextureRef InTextureRef,
		FRDGTextureRef InTextureRef2);

	static void AddBottomTypeProfilePass_RenderThread(
		FRDGBuilder& GraphBuilder,
		FGlobalShaderMap* InShaderMap,
		const FSavageSuperStormBottomTypePassParameters& InPassParameters,
		FRDGTextureRef InTextureRef);

	static void AddTopTypeProfilePass_RenderThread(
		FRDGBuilder& GraphBuilder,
		FGlobalShaderMap* InShaderMap,
		const FSavageSuperStormTopTypePassParameters& InPassParameters,
		FRDGTextureRef InTextureRef);

	// Stamps one brush dab into the painted-top texture and writes it in place.
	static void AddProfileBrushPass_RenderThread(
		FRDGBuilder& GraphBuilder,
		FGlobalShaderMap* InShaderMap,
		const FSavageSuperStormProfileBrushPassParameters& InPassParameters,
		FRDGTextureRef InPaintedTopTexture);
};
