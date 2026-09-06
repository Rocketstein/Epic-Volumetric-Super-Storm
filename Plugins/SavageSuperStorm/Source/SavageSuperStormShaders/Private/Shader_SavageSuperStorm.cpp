/**
 * @file Shader_SavageSuperStorm.cpp
 * @brief Implements render-graph compute passes used by the storm renderer.
 */

#include "Shader_SavageSuperStorm.h"

#include "GlobalShader.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphFwd.h"
#include "RenderGraphUtils.h"
#include "RHIStaticStates.h"
#include "ShaderParameterStruct.h"

class FStormShapeCS : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FStormShapeCS, Global, SAVAGESUPERSTORMSHADERS_API);
	SHADER_USE_PARAMETER_STRUCT(FStormShapeCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters,) SHADER_PARAMETER(uint32, Resolution) SHADER_PARAMETER(FVector2f, StormExtent) SHADER_PARAMETER(float, StormRadius) SHADER_PARAMETER(float, EnvelopeRadius) SHADER_PARAMETER(float, EnvelopeFalloff) SHADER_PARAMETER(float, OuterBrimRadiusScale) SHADER_PARAMETER(uint32, CoverageStrengthCurveLUTSize) SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FVector4f>, CoverageStrengthCurveLUT) SHADER_PARAMETER(uint32, TypeStrengthCurveLUTSize) SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FVector4f>, TypeStrengthCurveLUT) SHADER_PARAMETER(uint32, LayerHeightCurveLUTSize) SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FVector4f>, LayerHeightCurveLUT) SHADER_PARAMETER(FVector2f, FallbackWindDirectionXY) SHADER_PARAMETER(uint32, FlowMapEnabled) SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, UpperFlowMapTexture) SHADER_PARAMETER_SAMPLER(SamplerState, UpperFlowMapSampler) SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, OutShapeTexture) SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, OutShapeTexture2) END_SHADER_PARAMETER_STRUCT()

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

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters,) SHADER_PARAMETER(uint32, Resolution) SHADER_PARAMETER(float, BottomFade) SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, OutBottomProfileTexture) END_SHADER_PARAMETER_STRUCT()

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

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters,) SHADER_PARAMETER(uint32, Resolution) SHADER_PARAMETER(float, TopFade) SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float>, TopHeightLUT) SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, OutTopProfileTexture) END_SHADER_PARAMETER_STRUCT()

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

class FStormAnvilProfileCS : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FStormAnvilProfileCS, Global, SAVAGESUPERSTORMSHADERS_API);
	SHADER_USE_PARAMETER_STRUCT(FStormAnvilProfileCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters,) SHADER_PARAMETER(uint32, Resolution) SHADER_PARAMETER(float, AnvilFade) SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float>, AnvilHeightLUT) SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, OutAnvilProfileTexture) END_SHADER_PARAMETER_STRUCT()

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

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters,) SHADER_PARAMETER(uint32, Resolution) SHADER_PARAMETER(FVector2f, BrushCenterUV) SHADER_PARAMETER(float, BrushRadiusUV) SHADER_PARAMETER(float, BrushStrength) SHADER_PARAMETER(float, BrushValue) SHADER_PARAMETER(uint32, bBrushErase) SHADER_PARAMETER(uint32, bBrushOverwrite) SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, PaintedTopTexture) END_SHADER_PARAMETER_STRUCT()

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

class FStormProfileBlendCS : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FStormProfileBlendCS, Global, SAVAGESUPERSTORMSHADERS_API);
	SHADER_USE_PARAMETER_STRUCT(FStormProfileBlendCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters,) SHADER_PARAMETER(uint32, Resolution) SHADER_PARAMETER(float, BlendAlpha) SHADER_PARAMETER_RDG_TEXTURE(Texture2D, ProfileA) SHADER_PARAMETER_RDG_TEXTURE(Texture2D, ProfileB) SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, OutBlendedProfileTexture) END_SHADER_PARAMETER_STRUCT()

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

class FStormFlowMapClearCS : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FStormFlowMapClearCS, Global, SAVAGESUPERSTORMSHADERS_API);
	SHADER_USE_PARAMETER_STRUCT(FStormFlowMapClearCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters,) SHADER_PARAMETER(uint32, Resolution) SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, FlowMapTexture) END_SHADER_PARAMETER_STRUCT()

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

class FStormFlowMapBrushCS : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FStormFlowMapBrushCS, Global, SAVAGESUPERSTORMSHADERS_API);
	SHADER_USE_PARAMETER_STRUCT(FStormFlowMapBrushCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters,) SHADER_PARAMETER(uint32, Resolution) SHADER_PARAMETER(FVector2f, BrushCenterUV) SHADER_PARAMETER(float, BrushRadiusUV) SHADER_PARAMETER(FVector3f, BrushDirectionUVW) SHADER_PARAMETER(FVector4f, BrushEncodedRGBA) SHADER_PARAMETER(float, BrushStrength) SHADER_PARAMETER(float, BrushOpacity) SHADER_PARAMETER(uint32, bBrushErase) SHADER_PARAMETER(uint32, bUseEncodedRGBA) SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, FlowMapTexture) END_SHADER_PARAMETER_STRUCT()

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

IMPLEMENT_GLOBAL_SHADER(FStormShapeCS, "/SavageSuperStormShaders/Private/StormControlCS.usf", "BuildStormShapeRT", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FStormBottomTypeCS, "/SavageSuperStormShaders/Private/StormProfileCS.usf", "BuildBottomTypeProfileRT", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FStormTopTypeCS, "/SavageSuperStormShaders/Private/StormProfileCS.usf", "BuildTopTypeProfileRT", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FStormProfileBrushCS, "/SavageSuperStormShaders/Private/StormProfileCS.usf", "StampProfileBrushRT", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FStormProfileBlendCS, "/SavageSuperStormShaders/Private/StormProfileCS.usf", "BlendProfilesRT", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FStormFlowMapClearCS, "/SavageSuperStormShaders/Private/StormFlowMapPaintCS.usf", "ClearFlowMapRT", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FStormAnvilProfileCS, "/SavageSuperStormShaders/Private/StormProfileCS.usf", "BuildAnvilProfileRT", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FStormFlowMapBrushCS, "/SavageSuperStormShaders/Private/StormFlowMapPaintCS.usf", "StampFlowMapBrushRT", SF_Compute);

void FSavageSuperStormShaderInterface::AddShapePass_RenderThread(FRDGBuilder& GraphBuilder, FGlobalShaderMap* InShaderMap, const FSavageSuperStormShapePassParameters& InPassParameters, FRDGTextureRef InUpperFlowMapTexture, FRDGTextureRef InTextureRef, FRDGTextureRef InTextureRef2)
{
	ensure(IsInRenderingThread());

	RDG_EVENT_SCOPE(GraphBuilder, "BuildStormShapeRT");

	TShaderMapRef<FStormShapeCS> ComputeShader(InShaderMap);

	const uint32 Resolution = FMath::Max<uint32>(InPassParameters.Resolution, 1u);

	FStormShapeCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FStormShapeCS::FParameters>();

	auto UploadLinearColorCurveLUT = [&GraphBuilder](const TArray<FVector4f>& SourceLUT, const TCHAR* BufferName, uint32& OutSize, FRDGBufferSRVRef& OutSRV)
	{
		const uint32 Count = static_cast<uint32>(SourceLUT.Num());
		check(Count >= 2u);

		FVector4f* Data = reinterpret_cast<FVector4f*>(GraphBuilder.Alloc(sizeof(FVector4f) * Count, alignof(FVector4f)));
		FMemory::Memcpy(Data, SourceLUT.GetData(), sizeof(FVector4f) * Count);

		FRDGBufferRef Buffer = CreateStructuredBuffer(GraphBuilder, BufferName, sizeof(FVector4f), Count, Data, sizeof(FVector4f) * Count);

		OutSize = Count;
		OutSRV  = GraphBuilder.CreateSRV(Buffer);
	};

	UploadLinearColorCurveLUT(InPassParameters.CoverageStrengthCurveLUT, TEXT("StormCoverageStrengthCurveLUT"), PassParameters->CoverageStrengthCurveLUTSize, PassParameters->CoverageStrengthCurveLUT);

	UploadLinearColorCurveLUT(InPassParameters.TypeStrengthCurveLUT, TEXT("StormTypeStrengthCurveLUT"), PassParameters->TypeStrengthCurveLUTSize, PassParameters->TypeStrengthCurveLUT);

	UploadLinearColorCurveLUT(InPassParameters.LayerHeightCurveLUT, TEXT("StormLayerHeightCurveLUT"), PassParameters->LayerHeightCurveLUTSize, PassParameters->LayerHeightCurveLUT);

	PassParameters->Resolution              = Resolution;
	PassParameters->StormExtent             = InPassParameters.StormExtent;
	PassParameters->StormRadius             = InPassParameters.StormRadius;
	PassParameters->EnvelopeRadius          = InPassParameters.EnvelopeRadius;
	PassParameters->EnvelopeFalloff         = InPassParameters.EnvelopeFalloff;
	PassParameters->OuterBrimRadiusScale    = InPassParameters.OuterBrimRadiusScale;
	PassParameters->OutShapeTexture         = GraphBuilder.CreateUAV(InTextureRef);
	PassParameters->OutShapeTexture2        = GraphBuilder.CreateUAV(InTextureRef2);
	PassParameters->FallbackWindDirectionXY = InPassParameters.FallbackWindDirectionXY;
	PassParameters->FlowMapEnabled          = InPassParameters.bUpperFlowMapEnabled;
	PassParameters->UpperFlowMapTexture     = InUpperFlowMapTexture;
	PassParameters->UpperFlowMapSampler     = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

	const FIntVector GroupCount(FMath::DivideAndRoundUp(Resolution, FStormShapeCS::NumThreadsX), FMath::DivideAndRoundUp(Resolution, FStormShapeCS::NumThreadsY), 1);

	FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("BuildStormShapeRT"), ERDGPassFlags::Compute | ERDGPassFlags::NeverCull, ComputeShader, PassParameters, GroupCount);
}

void FSavageSuperStormShaderInterface::AddBottomTypeProfilePass_RenderThread(FRDGBuilder& GraphBuilder, FGlobalShaderMap* InShaderMap, const FSavageSuperStormBottomTypePassParameters& InPassParameters, FRDGTextureRef InTextureRef)
{
	ensure(IsInRenderingThread());

	RDG_EVENT_SCOPE(GraphBuilder, "BuildBottomTypeProfileRT");

	TShaderMapRef<FStormBottomTypeCS> ComputeShader(InShaderMap);

	const uint32 Resolution = FMath::Max<uint32>(InPassParameters.Resolution, 1u);

	FStormBottomTypeCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FStormBottomTypeCS::FParameters>();
	PassParameters->Resolution                      = Resolution;
	PassParameters->BottomFade                      = InPassParameters.BottomFade;
	PassParameters->OutBottomProfileTexture         = GraphBuilder.CreateUAV(InTextureRef);

	const FIntVector GroupCount(FMath::DivideAndRoundUp(Resolution, FStormBottomTypeCS::NumThreadsX), FMath::DivideAndRoundUp(Resolution, FStormBottomTypeCS::NumThreadsY), 1);

	FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("BuildBottomTypeProfileRT"), ERDGPassFlags::Compute | ERDGPassFlags::NeverCull, ComputeShader, PassParameters, GroupCount);
}

void FSavageSuperStormShaderInterface::AddTopTypeProfilePass_RenderThread(FRDGBuilder& GraphBuilder, FGlobalShaderMap* InShaderMap, const FSavageSuperStormTopTypePassParameters& InPassParameters, FRDGTextureRef InTextureRef)
{
	ensure(IsInRenderingThread());

	RDG_EVENT_SCOPE(GraphBuilder, "BuildTopTypeProfileRT");

	TShaderMapRef<FStormTopTypeCS> ComputeShader(InShaderMap);

	const uint32 Resolution = FMath::Max<uint32>(InPassParameters.Resolution, 1u);

	FStormTopTypeCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FStormTopTypeCS::FParameters>();
	PassParameters->Resolution                   = Resolution;
	PassParameters->TopFade                      = InPassParameters.TopFade;

	float*      LUTData = reinterpret_cast<float*>(GraphBuilder.Alloc(sizeof(float) * Resolution, alignof(float)));
	const int32 SrcNum  = InPassParameters.TopHeightLUT.Num();
	for (uint32 Index = 0; Index < Resolution; ++Index)
	{
		LUTData[Index] = static_cast<int32>(Index) < SrcNum ? InPassParameters.TopHeightLUT[Index] : 0.0f;
	}

	FRDGBufferRef TopHeightLUTBuffer = CreateStructuredBuffer(GraphBuilder, TEXT("StormTopHeightLUT"), sizeof(float), Resolution, LUTData, sizeof(float) * Resolution);

	PassParameters->TopHeightLUT         = GraphBuilder.CreateSRV(TopHeightLUTBuffer);
	PassParameters->OutTopProfileTexture = GraphBuilder.CreateUAV(InTextureRef);

	const FIntVector GroupCount(FMath::DivideAndRoundUp(Resolution, FStormTopTypeCS::NumThreadsX), FMath::DivideAndRoundUp(Resolution, FStormTopTypeCS::NumThreadsY), 1);

	FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("BuildTopTypeProfileRT"), ERDGPassFlags::Compute | ERDGPassFlags::NeverCull, ComputeShader, PassParameters, GroupCount);
}

void FSavageSuperStormShaderInterface::AddAnvilProfilePass_RenderThread(FRDGBuilder& GraphBuilder, FGlobalShaderMap* InShaderMap, const FSavageSuperStormAnvilProfilePassParameters& InPassParameters, FRDGTextureRef InTextureRef)
{
	ensure(IsInRenderingThread());

	RDG_EVENT_SCOPE(GraphBuilder, "BuildAnvilProfileRT");

	TShaderMapRef<FStormAnvilProfileCS> ComputeShader(InShaderMap);
	const uint32                        Resolution = FMath::Max<uint32>(InPassParameters.Resolution, 1u);

	FStormAnvilProfileCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FStormAnvilProfileCS::FParameters>();
	PassParameters->Resolution                        = Resolution;
	PassParameters->AnvilFade                         = InPassParameters.Fade;

	float*      LUTData     = reinterpret_cast<float*>(GraphBuilder.Alloc(sizeof(float) * Resolution, alignof(float)));
	const int32 SourceCount = InPassParameters.HeightLUT.Num();
	for (uint32 Index = 0; Index < Resolution; ++Index)
	{
		LUTData[Index] = static_cast<int32>(Index) < SourceCount ? InPassParameters.HeightLUT[Index] : 0.0f;
	}

	FRDGBufferRef HeightLUTBuffer          = CreateStructuredBuffer(GraphBuilder, TEXT("StormAnvilHeightLUT"), sizeof(float), Resolution, LUTData, sizeof(float) * Resolution);
	PassParameters->AnvilHeightLUT         = GraphBuilder.CreateSRV(HeightLUTBuffer);
	PassParameters->OutAnvilProfileTexture = GraphBuilder.CreateUAV(InTextureRef);

	const FIntVector GroupCount(FMath::DivideAndRoundUp(Resolution, FStormAnvilProfileCS::NumThreadsX), FMath::DivideAndRoundUp(Resolution, FStormAnvilProfileCS::NumThreadsY), 1);

	FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("BuildAnvilProfileRT"), ERDGPassFlags::Compute | ERDGPassFlags::NeverCull, ComputeShader, PassParameters, GroupCount);
}

void FSavageSuperStormShaderInterface::AddProfileBrushPass_RenderThread(FRDGBuilder& GraphBuilder, FGlobalShaderMap* InShaderMap, const FSavageSuperStormProfileBrushPassParameters& InPassParameters, FRDGTextureRef InPaintedTopTexture)
{
	ensure(IsInRenderingThread());

	RDG_EVENT_SCOPE(GraphBuilder, "StampProfileBrushRT");

	TShaderMapRef<FStormProfileBrushCS> ComputeShader(InShaderMap);

	const uint32 Resolution = FMath::Max<uint32>(InPassParameters.Resolution, 1u);

	FStormProfileBrushCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FStormProfileBrushCS::FParameters>();
	PassParameters->Resolution                        = Resolution;
	PassParameters->BrushCenterUV                     = InPassParameters.BrushCenterUV;
	PassParameters->BrushRadiusUV                     = InPassParameters.BrushRadiusUV;
	PassParameters->BrushStrength                     = InPassParameters.BrushStrength;
	PassParameters->BrushValue                        = InPassParameters.BrushValue;
	PassParameters->bBrushErase                       = InPassParameters.bErase ? 1u : 0u;
	PassParameters->bBrushOverwrite                   = InPassParameters.bOverwrite ? 1u : 0u;
	PassParameters->PaintedTopTexture                 = GraphBuilder.CreateUAV(InPaintedTopTexture);

	const FIntVector GroupCount(FMath::DivideAndRoundUp(Resolution, FStormProfileBrushCS::NumThreadsX), FMath::DivideAndRoundUp(Resolution, FStormProfileBrushCS::NumThreadsY), 1);

	FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("StampProfileBrushRT"), ERDGPassFlags::Compute | ERDGPassFlags::NeverCull, ComputeShader, PassParameters, GroupCount);
}

void FSavageSuperStormShaderInterface::AddProfileBlendPass_RenderThread(FRDGBuilder& GraphBuilder, FGlobalShaderMap* InShaderMap, const FSavageSuperStormProfileBlendPassParameters& InPassParameters, FRDGTextureRef InProfileA, FRDGTextureRef InProfileB, FRDGTextureRef OutBlendedProfileTexture)
{
	ensure(IsInRenderingThread());

	RDG_EVENT_SCOPE(GraphBuilder, "BlendStormProfilesRT");

	TShaderMapRef<FStormProfileBlendCS> ComputeShader(InShaderMap);
	const uint32                        Resolution = FMath::Max<uint32>(InPassParameters.Resolution, 1u);

	FStormProfileBlendCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FStormProfileBlendCS::FParameters>();
	PassParameters->Resolution                        = Resolution;
	PassParameters->BlendAlpha                        = FMath::Clamp(InPassParameters.Alpha, 0.0f, 1.0f);
	PassParameters->ProfileA                          = InProfileA;
	PassParameters->ProfileB                          = InProfileB;
	PassParameters->OutBlendedProfileTexture          = GraphBuilder.CreateUAV(OutBlendedProfileTexture);

	const FIntVector GroupCount(FMath::DivideAndRoundUp(Resolution, FStormProfileBlendCS::NumThreadsX), FMath::DivideAndRoundUp(Resolution, FStormProfileBlendCS::NumThreadsY), 1);

	FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("BlendStormProfilesRT"), ERDGPassFlags::Compute | ERDGPassFlags::NeverCull, ComputeShader, PassParameters, GroupCount);
}

void FSavageSuperStormShaderInterface::AddFlowMapClearPass_RenderThread(FRDGBuilder& GraphBuilder, FGlobalShaderMap* InShaderMap, uint32 InResolution, FRDGTextureRef InOutFlowMap)
{
	ensure(IsInRenderingThread());

	RDG_EVENT_SCOPE(GraphBuilder, "ClearFlowMapRT");

	TShaderMapRef<FStormFlowMapClearCS> ComputeShader(InShaderMap);
	const uint32                        Resolution = FMath::Max<uint32>(InResolution, 1u);

	FStormFlowMapClearCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FStormFlowMapClearCS::FParameters>();
	PassParameters->Resolution                        = Resolution;
	PassParameters->FlowMapTexture                    = GraphBuilder.CreateUAV(InOutFlowMap);

	const FIntVector GroupCount(FMath::DivideAndRoundUp(Resolution, FStormFlowMapClearCS::NumThreadsX), FMath::DivideAndRoundUp(Resolution, FStormFlowMapClearCS::NumThreadsY), 1);

	FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("ClearFlowMapRT"), ERDGPassFlags::Compute | ERDGPassFlags::NeverCull, ComputeShader, PassParameters, GroupCount);
}

void FSavageSuperStormShaderInterface::AddFlowMapBrushPass_RenderThread(FRDGBuilder& GraphBuilder, FGlobalShaderMap* InShaderMap, const FSavageSuperStormFlowMapBrushPassParameters& InPassParameters, FRDGTextureRef InOutFlowMap)
{
	ensure(IsInRenderingThread());

	RDG_EVENT_SCOPE(GraphBuilder, "StampFlowMapBrushRT");

	TShaderMapRef<FStormFlowMapBrushCS> ComputeShader(InShaderMap);
	const uint32                        Resolution = FMath::Max<uint32>(InPassParameters.Resolution, 1u);

	FStormFlowMapBrushCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FStormFlowMapBrushCS::FParameters>();
	PassParameters->Resolution                        = Resolution;
	PassParameters->BrushCenterUV                     = InPassParameters.BrushCenterUV;
	PassParameters->BrushRadiusUV                     = FMath::Max(InPassParameters.BrushRadiusUV, 0.0f);
	PassParameters->BrushDirectionUVW                 = InPassParameters.BrushDirectionUVW;
	PassParameters->BrushEncodedRGBA                  = FVector4f(InPassParameters.BrushEncodedRGBA.R, InPassParameters.BrushEncodedRGBA.G, InPassParameters.BrushEncodedRGBA.B, InPassParameters.BrushEncodedRGBA.A);
	PassParameters->BrushStrength                     = FMath::Clamp(InPassParameters.BrushStrength, 0.0f, 1.0f);
	PassParameters->BrushOpacity                      = FMath::Clamp(InPassParameters.BrushOpacity, 0.0f, 1.0f);
	PassParameters->bBrushErase                       = InPassParameters.bErase ? 1u : 0u;
	PassParameters->bUseEncodedRGBA                   = InPassParameters.bUseEncodedRGBA ? 1u : 0u;
	PassParameters->FlowMapTexture                    = GraphBuilder.CreateUAV(InOutFlowMap);

	const FIntVector GroupCount(FMath::DivideAndRoundUp(Resolution, FStormFlowMapBrushCS::NumThreadsX), FMath::DivideAndRoundUp(Resolution, FStormFlowMapBrushCS::NumThreadsY), 1);

	FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("StampFlowMapBrushRT"), ERDGPassFlags::Compute | ERDGPassFlags::NeverCull, ComputeShader, PassParameters, GroupCount);
}