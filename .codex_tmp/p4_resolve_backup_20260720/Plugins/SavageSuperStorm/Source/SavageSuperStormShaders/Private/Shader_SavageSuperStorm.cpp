#include "Shader_SavageSuperStorm.h"

#include "GlobalShader.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphFwd.h"
#include "RenderGraphUtils.h"
#include "ShaderParameterStruct.h"

class FSavageSuperStormCS : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FSavageSuperStormCS, Global, SAVAGESUPERSTORMSHADERS_API);
	SHADER_USE_PARAMETER_STRUCT(FSavageSuperStormCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, Resolution)
		SHADER_PARAMETER(FVector2f, Scale)
		SHADER_PARAMETER(FVector2f, Origin)
		SHADER_PARAMETER(FVector2f, Location)
		SHADER_PARAMETER(float, Radius)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, OutTexture)
	END_SHADER_PARAMETER_STRUCT()

public:
	static constexpr uint32 NumThreadsX = 8;
	static constexpr uint32 NumThreadsY = 8;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("NUM_THREADS_X"), NumThreadsX);
		OutEnvironment.SetDefine(TEXT("NUM_THREADS_Y"), NumThreadsY);
	}
};

class FStormShapeCS : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FStormShapeCS, Global, SAVAGESUPERSTORMSHADERS_API);
	SHADER_USE_PARAMETER_STRUCT(FStormShapeCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, Resolution)
		SHADER_PARAMETER(FVector2f, StormExtent)
		SHADER_PARAMETER(float, StormRadius)
		SHADER_PARAMETER(float, EnvelopeRadius)
		SHADER_PARAMETER(float, EnvelopeFalloff)
		SHADER_PARAMETER(float, OuterBrimRadiusScale)
		SHADER_PARAMETER(float, CoveragePower)
		SHADER_PARAMETER(float, CoverageFloor)
		SHADER_PARAMETER(float, CoverageCeiling)
		SHADER_PARAMETER(float, StratusShelfFraction)
		SHADER_PARAMETER(float, RotationSign)
		SHADER_PARAMETER(float, CumulusShelfFraction)
		SHADER_PARAMETER(float, VortexDentRegionRadius01)
		SHADER_PARAMETER(float, VortexDentInnerRadius01)
		SHADER_PARAMETER(float, VortexDentTwistRadians)
		SHADER_PARAMETER(float, VortexDentTwistPower)
		SHADER_PARAMETER(float, VortexDentCellFrequency)
		SHADER_PARAMETER(float, VortexDentOccupancy)
		SHADER_PARAMETER(float, VortexDentBlobRadiusMinCell)
		SHADER_PARAMETER(float, VortexDentBlobRadiusMaxCell)
		SHADER_PARAMETER(float, VortexDentTendrilStretch)
		SHADER_PARAMETER(uint32, VortexDentSeed)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, OutShapeTexture)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, OutShapeTexture2)
	END_SHADER_PARAMETER_STRUCT()

public:
	static constexpr uint32 NumThreadsX = 8;
	static constexpr uint32 NumThreadsY = 8;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("NUM_THREADS_X"), NumThreadsX);
		OutEnvironment.SetDefine(TEXT("NUM_THREADS_Y"), NumThreadsY);
	}
};

class FStormBottomTypeCS : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FStormBottomTypeCS, Global, SAVAGESUPERSTORMSHADERS_API);
	SHADER_USE_PARAMETER_STRUCT(FStormBottomTypeCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, Resolution)
		SHADER_PARAMETER(float, BottomFade)
		SHADER_PARAMETER(float, VerticalVoidOffset)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, OutBottomProfileTexture)
	END_SHADER_PARAMETER_STRUCT()

public:
	static constexpr uint32 NumThreadsX = 8;
	static constexpr uint32 NumThreadsY = 8;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("NUM_THREADS_X"), NumThreadsX);
		OutEnvironment.SetDefine(TEXT("NUM_THREADS_Y"), NumThreadsY);
	}
};

class FStormTopTypeCS : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FStormTopTypeCS, Global, SAVAGESUPERSTORMSHADERS_API);
	SHADER_USE_PARAMETER_STRUCT(FStormTopTypeCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, Resolution)
		SHADER_PARAMETER(uint32, bUseCurveLUT)
		SHADER_PARAMETER(float, VerticalVoidOffset)
		SHADER_PARAMETER(float, TopFade)
		SHADER_PARAMETER(float, BottomFade)
		SHADER_PARAMETER(FVector3f, TopTypeHeights)
		SHADER_PARAMETER(FVector3f, TopTypeWeights)
		SHADER_PARAMETER(float, StratusToStratocumulusBlendStrength)
		SHADER_PARAMETER(float, StratocumulusToCumulusBlendStrength)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float>, TopHeightLUT)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, OutTopProfileTexture)
	END_SHADER_PARAMETER_STRUCT()

public:
	static constexpr uint32 NumThreadsX = 8;
	static constexpr uint32 NumThreadsY = 8;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("NUM_THREADS_X"), NumThreadsX);
		OutEnvironment.SetDefine(TEXT("NUM_THREADS_Y"), NumThreadsY);
	}
};

class FStormProfileBrushCS : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FStormProfileBrushCS, Global, SAVAGESUPERSTORMSHADERS_API);
	SHADER_USE_PARAMETER_STRUCT(FStormProfileBrushCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, Resolution)
		SHADER_PARAMETER(FVector2f, BrushCenterUV)
		SHADER_PARAMETER(float, BrushRadiusUV)
		SHADER_PARAMETER(float, BrushStrength)
		SHADER_PARAMETER(float, BrushValue)
		SHADER_PARAMETER(uint32, bBrushErase)
		SHADER_PARAMETER(uint32, bBrushOverwrite)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, PaintedTopTexture)
	END_SHADER_PARAMETER_STRUCT()

public:
	static constexpr uint32 NumThreadsX = 8;
	static constexpr uint32 NumThreadsY = 8;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("NUM_THREADS_X"), NumThreadsX);
		OutEnvironment.SetDefine(TEXT("NUM_THREADS_Y"), NumThreadsY);
	}
};

IMPLEMENT_GLOBAL_SHADER(FSavageSuperStormCS, "/SavageSuperStormShaders/Private/SavageSuperStormShaders.usf", "SavageSuperStorm", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FStormShapeCS, "/SavageSuperStormShaders/Private/StormControlCS.usf", "BuildStormShapeRT", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FStormBottomTypeCS, "/SavageSuperStormShaders/Private/StormProfileCS.usf", "BuildBottomTypeProfileRT", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FStormTopTypeCS, "/SavageSuperStormShaders/Private/StormProfileCS.usf", "BuildTopTypeProfileRT", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FStormProfileBrushCS, "/SavageSuperStormShaders/Private/StormProfileCS.usf", "StampProfileBrushRT", SF_Compute);

void FSavageSuperStormShaderInterface::AddPass_RenderThread(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* InShaderMap,
	uint32 InResolution,
	const FVector2f& InScale,
	const FVector2f& InOrigin,
	const FVector2f& InLocation,
	float InRadius,
	FRDGTextureRef InTextureRef)
{
	ensure(IsInRenderingThread());

	RDG_EVENT_SCOPE(GraphBuilder, "SavageSuperStorm");

	TShaderMapRef<FSavageSuperStormCS> ComputeShader(InShaderMap);

	FSavageSuperStormCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FSavageSuperStormCS::FParameters>();
	PassParameters->Resolution = InResolution;
	PassParameters->Scale = InScale;
	PassParameters->Origin = InOrigin;
	PassParameters->Location = InLocation;
	PassParameters->Radius = InRadius;
	PassParameters->OutTexture = GraphBuilder.CreateUAV(InTextureRef);

	const FIntVector GroupCount(
		FMath::DivideAndRoundUp(InResolution, FSavageSuperStormCS::NumThreadsX),
		FMath::DivideAndRoundUp(InResolution, FSavageSuperStormCS::NumThreadsY),
		1);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("SavageSuperStorm"),
		ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
		ComputeShader,
		PassParameters,
		GroupCount);
}

void FSavageSuperStormShaderInterface::AddShapePass_RenderThread(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* InShaderMap,
	const FSavageSuperStormShapePassParameters& InPassParameters,
	FRDGTextureRef InTextureRef,
	FRDGTextureRef InTextureRef2)
{
	ensure(IsInRenderingThread());

	RDG_EVENT_SCOPE(GraphBuilder, "BuildStormShapeRT");

	TShaderMapRef<FStormShapeCS> ComputeShader(InShaderMap);

	const uint32 Resolution = FMath::Max<uint32>(InPassParameters.Resolution, 1u);

	FStormShapeCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FStormShapeCS::FParameters>();
	PassParameters->Resolution = Resolution;
	PassParameters->StormExtent = InPassParameters.StormExtent;
	PassParameters->StormRadius = InPassParameters.StormRadius;
	PassParameters->EnvelopeRadius = InPassParameters.EnvelopeRadius;
	PassParameters->EnvelopeFalloff = InPassParameters.EnvelopeFalloff;
	PassParameters->OuterBrimRadiusScale = InPassParameters.OuterBrimRadiusScale;
	PassParameters->CoveragePower = InPassParameters.CoveragePower;
	PassParameters->CoverageFloor = InPassParameters.CoverageFloor;
	PassParameters->CoverageCeiling = InPassParameters.CoverageCeiling;
	PassParameters->StratusShelfFraction = InPassParameters.StratusShelfFraction;
	PassParameters->CumulusShelfFraction = InPassParameters.CumulusShelfFraction;
	PassParameters->RotationSign = InPassParameters.RotationSign;
	PassParameters->VortexDentRegionRadius01 = InPassParameters.VortexDentRegionRadius01;
	PassParameters->VortexDentInnerRadius01 = InPassParameters.VortexDentInnerRadius01;
	PassParameters->VortexDentTwistRadians = InPassParameters.VortexDentTwistRadians;
	PassParameters->VortexDentTwistPower = InPassParameters.VortexDentTwistPower;
	PassParameters->VortexDentCellFrequency = InPassParameters.VortexDentCellFrequency;
	PassParameters->VortexDentOccupancy = InPassParameters.VortexDentOccupancy;
	PassParameters->VortexDentBlobRadiusMinCell = InPassParameters.VortexDentBlobRadiusMinCell;
	PassParameters->VortexDentBlobRadiusMaxCell = InPassParameters.VortexDentBlobRadiusMaxCell;
	PassParameters->VortexDentTendrilStretch = InPassParameters.VortexDentTendrilStretch;
	PassParameters->VortexDentSeed = InPassParameters.VortexDentSeed;
	PassParameters->OutShapeTexture = GraphBuilder.CreateUAV(InTextureRef);
	PassParameters->OutShapeTexture2 = GraphBuilder.CreateUAV(InTextureRef2);

	const FIntVector GroupCount(
		FMath::DivideAndRoundUp(Resolution, FStormShapeCS::NumThreadsX),
		FMath::DivideAndRoundUp(Resolution, FStormShapeCS::NumThreadsY),
		1);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("BuildStormShapeRT"),
		ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
		ComputeShader,
		PassParameters,
		GroupCount);
}

void FSavageSuperStormShaderInterface::AddBottomTypeProfilePass_RenderThread(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* InShaderMap,
	const FSavageSuperStormBottomTypePassParameters& InPassParameters,
	FRDGTextureRef InTextureRef)
{
	ensure(IsInRenderingThread());

	RDG_EVENT_SCOPE(GraphBuilder, "BuildBottomTypeProfileRT");

	TShaderMapRef<FStormBottomTypeCS> ComputeShader(InShaderMap);

	const uint32 Resolution = FMath::Max<uint32>(InPassParameters.Resolution, 1u);

	FStormBottomTypeCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FStormBottomTypeCS::FParameters>();
	PassParameters->Resolution = Resolution;
	PassParameters->BottomFade = InPassParameters.BottomFade;
	PassParameters->VerticalVoidOffset = InPassParameters.VerticalVoidOffset;
	PassParameters->OutBottomProfileTexture = GraphBuilder.CreateUAV(InTextureRef);

	const FIntVector GroupCount(
		FMath::DivideAndRoundUp(Resolution, FStormBottomTypeCS::NumThreadsX),
		FMath::DivideAndRoundUp(Resolution, FStormBottomTypeCS::NumThreadsY),
		1);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("BuildBottomTypeProfileRT"),
		ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
		ComputeShader,
		PassParameters,
		GroupCount);
}

void FSavageSuperStormShaderInterface::AddTopTypeProfilePass_RenderThread(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* InShaderMap,
	const FSavageSuperStormTopTypePassParameters& InPassParameters,
	FRDGTextureRef InTextureRef)
{
	ensure(IsInRenderingThread());

	RDG_EVENT_SCOPE(GraphBuilder, "BuildTopTypeProfileRT");

	TShaderMapRef<FStormTopTypeCS> ComputeShader(InShaderMap);

	const uint32 Resolution = FMath::Max<uint32>(InPassParameters.Resolution, 1u);

	FStormTopTypeCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FStormTopTypeCS::FParameters>();
	PassParameters->Resolution = Resolution;
	PassParameters->bUseCurveLUT = InPassParameters.bUseCurveLUT ? 1u : 0u;
	PassParameters->VerticalVoidOffset = InPassParameters.VerticalVoidOffset;
	PassParameters->TopFade = InPassParameters.TopFade;
	PassParameters->BottomFade = InPassParameters.BottomFade;
	PassParameters->TopTypeHeights = InPassParameters.TypeHeights;
	PassParameters->TopTypeWeights = InPassParameters.TypeWeights;
	PassParameters->StratusToStratocumulusBlendStrength = InPassParameters.StratusToStratocumulusBlendStrength;
	PassParameters->StratocumulusToCumulusBlendStrength = InPassParameters.StratocumulusToCumulusBlendStrength;

	float* LUTData = reinterpret_cast<float*>(GraphBuilder.Alloc(sizeof(float) * Resolution, alignof(float)));
	const int32 SrcNum = InPassParameters.TopHeightLUT.Num();
	for (uint32 Index = 0; Index < Resolution; ++Index)
	{
		LUTData[Index] = static_cast<int32>(Index) < SrcNum ? InPassParameters.TopHeightLUT[Index] : 0.0f;
	}

	FRDGBufferRef TopHeightLUTBuffer = CreateStructuredBuffer(
		GraphBuilder,
		TEXT("StormTopHeightLUT"),
		sizeof(float),
		Resolution,
		LUTData,
		sizeof(float) * Resolution);

	PassParameters->TopHeightLUT = GraphBuilder.CreateSRV(TopHeightLUTBuffer);
	PassParameters->OutTopProfileTexture = GraphBuilder.CreateUAV(InTextureRef);

	const FIntVector GroupCount(
		FMath::DivideAndRoundUp(Resolution, FStormTopTypeCS::NumThreadsX),
		FMath::DivideAndRoundUp(Resolution, FStormTopTypeCS::NumThreadsY),
		1);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("BuildTopTypeProfileRT"),
		ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
		ComputeShader,
		PassParameters,
		GroupCount);
}

void FSavageSuperStormShaderInterface::AddProfileBrushPass_RenderThread(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* InShaderMap,
	const FSavageSuperStormProfileBrushPassParameters& InPassParameters,
	FRDGTextureRef InPaintedTopTexture)
{
	ensure(IsInRenderingThread());

	RDG_EVENT_SCOPE(GraphBuilder, "StampProfileBrushRT");

	TShaderMapRef<FStormProfileBrushCS> ComputeShader(InShaderMap);

	const uint32 Resolution = FMath::Max<uint32>(InPassParameters.Resolution, 1u);

	FStormProfileBrushCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FStormProfileBrushCS::FParameters>();
	PassParameters->Resolution		= Resolution;
	PassParameters->BrushCenterUV	= InPassParameters.BrushCenterUV;
	PassParameters->BrushRadiusUV	= InPassParameters.BrushRadiusUV;
	PassParameters->BrushStrength	= InPassParameters.BrushStrength;
	PassParameters->BrushValue		= InPassParameters.BrushValue;
	PassParameters->bBrushErase		= InPassParameters.bErase ? 1u : 0u;
	PassParameters->bBrushOverwrite	= InPassParameters.bOverwrite ? 1u : 0u;
	PassParameters->PaintedTopTexture	  = GraphBuilder.CreateUAV(InPaintedTopTexture);

	const FIntVector GroupCount(
		FMath::DivideAndRoundUp(Resolution, FStormProfileBrushCS::NumThreadsX),
		FMath::DivideAndRoundUp(Resolution, FStormProfileBrushCS::NumThreadsY),
		1);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("StampProfileBrushRT"),
		ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
		ComputeShader,
		PassParameters,
		GroupCount);
}
