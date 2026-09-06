/**
 * @file StormRenderWorldSubsystem.cpp
 * @brief Owns shape render targets for the single storm registered in a world.
 */

#include "Subsystems/StormRenderWorldSubsystem.h"

#include "Actors/VolumetricSuperStormActor.h"
#include "Data/StormRenderData.h"
#include "Debug/DebugDrawService.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "GlobalShader.h"
#include "ProfilingDebugging/RealtimeGPUProfiler.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RHICommandList.h"
#include "SavageSuperStormRuntime.h"
#include "SceneInterface.h"
#include "SceneView.h"
#include "Shader_SavageSuperStorm.h"
#include "Stats/Stats.h"
#include "SystemTextures.h"
#include "TextureResource.h"

DECLARE_STATS_GROUP(TEXT("SavageSuperStorm"), STATGROUP_SavageSuperStorm, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("Subsystem Tick"), STAT_SSS_Tick, STATGROUP_SavageSuperStorm);
DECLARE_CYCLE_STAT(TEXT("Submit Render Data"), STAT_SSS_SubmitRenderData, STATGROUP_SavageSuperStorm);
DECLARE_CYCLE_STAT(TEXT("Build Shape RT (GT)"), STAT_SSS_BuildShapeRT, STATGROUP_SavageSuperStorm);
DECLARE_GPU_STAT_NAMED(SSS_ShapePass, TEXT("SavageSuperStorm ShapePass"));

static TAutoConsoleVariable<int32> CVarStats(
	TEXT("SavageSuperStorm.Stats"),
	0,
	TEXT("When non-zero, draws the SavageSuperStorm storm-state overlay (4-decimal timings + render data)."),
	ECVF_Default);

namespace
{
template <typename TValue>
void CombineShapeHash(uint32& Hash, const TValue& Value)
{
	Hash = HashCombineFast(Hash, GetTypeHash(Value));
}

void BuildShapeBakeInputs(
	const FStormRenderData& RenderData,
	TArray<FVector4f>& OutCoverageLUT,
	TArray<FVector4f>& OutTypeLUT,
	TArray<FVector4f>& OutLayerHeightLUT,
	uint32& OutHash)
{
	constexpr int32 CurveSampleCount = 256;
	OutCoverageLUT.Reset(CurveSampleCount);
	OutTypeLUT.Reset(CurveSampleCount);
	OutLayerHeightLUT.Reset(CurveSampleCount);
	OutHash = 0;

	const FStormShapeSettings& Shape = RenderData.Shape;
	CombineShapeHash(OutHash, Shape.ShapeRenderTargetResolution);
	CombineShapeHash(OutHash, RenderData.WorldExtent.X);
	CombineShapeHash(OutHash, RenderData.WorldExtent.Y);
	CombineShapeHash(OutHash, Shape.Radius);
	CombineShapeHash(OutHash, Shape.EnvelopeRadius);
	CombineShapeHash(OutHash, Shape.EnvelopeFalloff);
	CombineShapeHash(OutHash, Shape.GetAnvilOuterRadiusScale());
	CombineShapeHash(OutHash, RenderData.WindDirection.X);
	CombineShapeHash(OutHash, RenderData.WindDirection.Y);
	CombineShapeHash(OutHash, RenderData.FlowMap.bEnabled);
	CombineShapeHash(OutHash, RenderData.FlowMap.UpperTexture.Get());

	for (int32 Index = 0; Index < CurveSampleCount; ++Index)
	{
		const float Radius01 = static_cast<float>(Index)
			/ static_cast<float>(CurveSampleCount - 1);
		const FLinearColor Coverage =
			Shape.ShapeCurves.CoverageStrength.GetLinearColorValue(Radius01).GetClamped();
		const FLinearColor Type =
			Shape.ShapeCurves.TypeStrength.GetLinearColorValue(Radius01).GetClamped();
		const FLinearColor LayerHeight =
			Shape.ShapeCurves.LayerHeight.GetLinearColorValue(Radius01).GetClamped();

		OutCoverageLUT.Emplace(Coverage.R, Coverage.G, Coverage.B, Coverage.A);
		OutTypeLUT.Emplace(Type.R, Type.G, Type.B, Type.A);
		OutLayerHeightLUT.Emplace(
			LayerHeight.R,
			LayerHeight.G,
			LayerHeight.B,
			LayerHeight.A);

		CombineShapeHash(OutHash, Coverage.R);
		CombineShapeHash(OutHash, Coverage.A);
		CombineShapeHash(OutHash, Type.G);
		CombineShapeHash(OutHash, Type.B);
		CombineShapeHash(OutHash, LayerHeight.R);
		CombineShapeHash(OutHash, LayerHeight.B);
	}
}

struct FScopedMsTimer
{
	double& OutMs;
	const double StartSeconds;
	explicit FScopedMsTimer(double& InOutMs)
		: OutMs(InOutMs), StartSeconds(FPlatformTime::Seconds()) {}
	~FScopedMsTimer() { OutMs = (FPlatformTime::Seconds() - StartSeconds) * 1000.0; }
};
}

void UStormRenderWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	FDebugDrawDelegate StatsDelegate =
		FDebugDrawDelegate::CreateUObject(this, &UStormRenderWorldSubsystem::DrawStatsOverlay);
	StatsDrawHandle = UDebugDrawService::Register(TEXT("Game"), StatsDelegate);
	StatsDrawEditorHandle = UDebugDrawService::Register(TEXT("Editor"), StatsDelegate);
}

void UStormRenderWorldSubsystem::Deinitialize()
{
	if (StatsDrawHandle.IsValid())
	{
		UDebugDrawService::Unregister(StatsDrawHandle);
		StatsDrawHandle.Reset();
	}
	if (StatsDrawEditorHandle.IsValid())
	{
		UDebugDrawService::Unregister(StatsDrawEditorHandle);
		StatsDrawEditorHandle.Reset();
	}

	ResetStormState();

	Super::Deinitialize();
}

bool UStormRenderWorldSubsystem::RegisterStorm(AVolumetricSuperStormActor* StormActor)
{
	if (!IsValid(StormActor))
	{
		return false;
	}

	if (RegisteredStorm.IsValid() && RegisteredStorm.Get() != StormActor)
	{
		UE_LOG(
			LogSavageSuperStormRuntime,
			Error,
			TEXT("World '%s' already contains the registered storm '%s'. Additional storm '%s' is disabled."),
			*GetNameSafe(GetWorld()),
			*GetNameSafe(RegisteredStorm.Get()),
			*GetNameSafe(StormActor));
		return false;
	}

	RegisteredStorm = StormActor;
	return true;
}

void UStormRenderWorldSubsystem::UnregisterStorm(const AVolumetricSuperStormActor* StormActor)
{
	if (RegisteredStorm.Get() == StormActor)
	{
		ResetStormState();
	}
}

bool UStormRenderWorldSubsystem::IsRegisteredStorm(const AVolumetricSuperStormActor* StormActor) const
{
	return IsValid(StormActor) && RegisteredStorm.Get() == StormActor;
}

void UStormRenderWorldSubsystem::ResetStormState()
{
	RegisteredStorm = nullptr;
	StormRenderData = FStormRenderData();
	bHasStormRenderData = false;
	bShapeDirty = true;
	PendingShapeBakeHash = 0;
	LastBakedShapeHash = 0;
	bHasPendingShapeBake = false;
	bHasLastBakedShapeHash = false;
	CoverageStrengthCurveLUT.Reset();
	TypeStrengthCurveLUT.Reset();
	LayerHeightCurveLUT.Reset();
	ShapeRenderTarget = nullptr;
	ShapeRenderTarget2 = nullptr;
}

TStatId UStormRenderWorldSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UStormRenderWorldSubsystem, STATGROUP_Tickables);
}

bool UStormRenderWorldSubsystem::DoesSupportWorldType(EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game ||
		WorldType == EWorldType::PIE ||
		WorldType == EWorldType::GamePreview ||
		WorldType == EWorldType::Editor;
}

void UStormRenderWorldSubsystem::Tick(float)
{
	SCOPE_CYCLE_COUNTER(STAT_SSS_Tick);
	FScopedMsTimer Timer(LastTickMs);

	if (!RegisteredStorm.IsValid())
	{
		ResetStormState();
		return;
	}

	if (!bHasStormRenderData)
	{
		return;
	}

	if (!EnsureShapeRenderTargets())
	{
		return;
	}

	if (bShapeDirty)
	{
		bShapeDirty = !BuildShapeRenderTargets();
	}
}

bool UStormRenderWorldSubsystem::IsTickable() const
{
	const UWorld* World = GetWorld();
	const bool bTickableWorld = World &&
		(World->WorldType == EWorldType::Game ||
			World->WorldType == EWorldType::PIE ||
			World->WorldType == EWorldType::GamePreview ||
			World->WorldType == EWorldType::Editor);

	return bTickableWorld && !IsTemplate();
}

bool UStormRenderWorldSubsystem::IsTickableInEditor() const
{
	return true;
}

bool UStormRenderWorldSubsystem::IsTickableWhenPaused() const
{
	return false;
}

UTextureRenderTarget2D* UStormRenderWorldSubsystem::GetShapeRenderTarget() const
{
	return ShapeRenderTarget.Get();
}

UTextureRenderTarget2D* UStormRenderWorldSubsystem::GetShapeRenderTarget2() const
{
	return ShapeRenderTarget2.Get();
}

void UStormRenderWorldSubsystem::MarkStormShapeDirty(
	const AVolumetricSuperStormActor* StormActor)
{
	if (IsRegisteredStorm(StormActor))
	{
		bShapeDirty = true;
	}
}

bool UStormRenderWorldSubsystem::SubmitStormRenderData(
	AVolumetricSuperStormActor* StormActor,
	const FStormRenderData& RenderData)
{
	SCOPE_CYCLE_COUNTER(STAT_SSS_SubmitRenderData);
	FScopedMsTimer Timer(LastSubmitMs);

	if (!RegisterStorm(StormActor))
	{
		return false;
	}

	BuildShapeBakeInputs(
		RenderData,
		CoverageStrengthCurveLUT,
		TypeStrengthCurveLUT,
		LayerHeightCurveLUT,
		PendingShapeBakeHash);
	bHasPendingShapeBake = true;
	if (!bHasLastBakedShapeHash || PendingShapeBakeHash != LastBakedShapeHash)
	{
		bShapeDirty = true;
	}

	StormRenderData = RenderData;
	bHasStormRenderData = true;

	const bool bTargetsReady = EnsureShapeRenderTargets();
	if (bTargetsReady && bShapeDirty)
	{
		bShapeDirty = !BuildShapeRenderTargets();
	}

	return true;
}

bool UStormRenderWorldSubsystem::EnsureShapeRenderTargets()
{
	return EnsureShapeRenderTarget() && EnsureShapeRenderTarget2();
}

bool UStormRenderWorldSubsystem::EnsureShapeRenderTarget()
{
	return EnsureRenderTarget(
		ShapeRenderTarget,
		TEXT("RT_StormShape_Runtime"),
		GetShapeRenderTargetResolution());
}

bool UStormRenderWorldSubsystem::EnsureShapeRenderTarget2()
{
	return EnsureRenderTarget(
		ShapeRenderTarget2,
		TEXT("RT_StormShape2_Runtime"),
		GetShapeRenderTargetResolution());
}

bool UStormRenderWorldSubsystem::EnsureRenderTarget(
	TObjectPtr<UTextureRenderTarget2D>& RenderTarget,
	const TCHAR* RenderTargetName,
	int32 Resolution)
{
	const int32 SafeResolution = FMath::Clamp(Resolution, 512, 2048);

	if (RenderTarget &&
		RenderTarget->SizeX == SafeResolution &&
		RenderTarget->SizeY == SafeResolution &&
		RenderTarget->GetFormat() == PF_FloatRGBA &&
		RenderTarget->Filter == TF_Bilinear)
	{
		return true;
	}

	if (!RenderTarget)
	{
		RenderTarget = NewObject<UTextureRenderTarget2D>(this, FName(RenderTargetName));
		if (!RenderTarget)
		{
			UE_LOG(LogSavageSuperStormRuntime, Error, TEXT("Failed to create %s."), RenderTargetName);
			return false;
		}
	}

	RenderTarget->ClearColor = FLinearColor::Black;
	RenderTarget->bCanCreateUAV = true;
	RenderTarget->SRGB = false;
	RenderTarget->Filter = TF_Bilinear;
	RenderTarget->AddressX = TA_Clamp;
	RenderTarget->AddressY = TA_Clamp;
	RenderTarget->InitCustomFormat(SafeResolution, SafeResolution, PF_FloatRGBA, true);
	RenderTarget->UpdateResourceImmediate(true);
	bShapeDirty = true;

	return ValidateRenderTarget(RenderTarget.Get(), RenderTargetName);
}

bool UStormRenderWorldSubsystem::ValidateRenderTarget(const UTextureRenderTarget2D* RenderTarget, const TCHAR* RenderTargetName) const
{
	const bool bValid =
		RenderTarget != nullptr &&
		RenderTarget->SizeX >= 512 &&
		RenderTarget->SizeY >= 512 &&
		RenderTarget->GetFormat() == PF_FloatRGBA &&
		RenderTarget->Filter == TF_Bilinear;

	if (!bValid)
	{
		UE_LOG(
			LogSavageSuperStormRuntime,
			Error,
			TEXT("StormRenderWorldSubsystem RT validation failed. Name=%s RT=%s Size=%dx%d Format=%s"),
			RenderTargetName,
			*GetNameSafe(RenderTarget),
			RenderTarget ? RenderTarget->SizeX : 0,
			RenderTarget ? RenderTarget->SizeY : 0,
			RenderTarget ? *UEnum::GetValueAsName(RenderTarget->GetFormat()).ToString() : TEXT("None"));
	}

	return bValid;
}

int32 UStormRenderWorldSubsystem::GetShapeRenderTargetResolution() const
{
	return FMath::Clamp(StormRenderData.Shape.ShapeRenderTargetResolution, 512, 2048);
}

bool UStormRenderWorldSubsystem::BuildShapeRenderTargets()
{
	SCOPE_CYCLE_COUNTER(STAT_SSS_BuildShapeRT);
	FScopedMsTimer Timer(LastBuildShapeMs);

	if (!bHasPendingShapeBake || !EnsureShapeRenderTargets())
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!World || !World->Scene)
	{
		return false;
	}

	FTextureRenderTargetResource* RTResource = ShapeRenderTarget->GameThread_GetRenderTargetResource();
	FTextureRenderTargetResource* RTResource2 = ShapeRenderTarget2->GameThread_GetRenderTargetResource();
	if (!RTResource || !RTResource2)
	{
		return false;
	}

	FSavageSuperStormShapePassParameters PassParameters;
	const FStormShapeSettings& Shape = StormRenderData.Shape;
	UTextureRenderTarget2D* UpperFlowMapRT =
		Cast<UTextureRenderTarget2D>(StormRenderData.FlowMap.UpperTexture.Get());
	FTextureRenderTargetResource* UpperFlowMapResource =
		UpperFlowMapRT ? UpperFlowMapRT->GameThread_GetRenderTargetResource() : nullptr;

	PassParameters.CoverageStrengthCurveLUT = CoverageStrengthCurveLUT;
	PassParameters.TypeStrengthCurveLUT = TypeStrengthCurveLUT;
	PassParameters.LayerHeightCurveLUT = LayerHeightCurveLUT;

	PassParameters.Resolution = static_cast<uint32>(ShapeRenderTarget->SizeX);

	PassParameters.StormExtent = FVector2f(
		FMath::Max(1.0f, static_cast<float>(FMath::Abs(StormRenderData.WorldExtent.X))),
		FMath::Max(1.0f, static_cast<float>(FMath::Abs(StormRenderData.WorldExtent.Y))));
	PassParameters.StormRadius = FMath::Max(1.0f, StormRenderData.Shape.Radius);
	PassParameters.EnvelopeRadius = Shape.EnvelopeRadius;
	PassParameters.EnvelopeFalloff = Shape.EnvelopeFalloff;
	PassParameters.OuterBrimRadiusScale = Shape.GetAnvilOuterRadiusScale();

	FVector2D FallbackWindDirection(
		StormRenderData.WindDirection.X,
		StormRenderData.WindDirection.Y);
	if (!FallbackWindDirection.Normalize())
	{
		FallbackWindDirection = FVector2D(1.0, 0.0);
	}
	PassParameters.FallbackWindDirectionXY = FVector2f(
		static_cast<float>(FallbackWindDirection.X),
		static_cast<float>(FallbackWindDirection.Y));
	PassParameters.bUpperFlowMapEnabled =
		StormRenderData.FlowMap.bEnabled && UpperFlowMapResource ? 1u : 0u;

	const ERHIFeatureLevel::Type FeatureLevel = World->Scene->GetFeatureLevel();
	ENQUEUE_RENDER_COMMAND(BuildStormShapeRT)(
		[PassParameters, RTResource, RTResource2, UpperFlowMapResource, FeatureLevel](FRHICommandListImmediate& RHICmdList)
		{
			FRDGBuilder GraphBuilder(RHICmdList);
			FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(FeatureLevel);
			FRDGTextureRef RDGTexture = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(RTResource->GetRenderTargetTexture(), TEXT("RT_StormShape_Runtime")));
			FRDGTextureRef RDGTexture2 = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(RTResource2->GetRenderTargetTexture(), TEXT("RT_StormShape2_Runtime")));
			FRDGTextureRef RDGUpperFlowMap = UpperFlowMapResource
				? GraphBuilder.RegisterExternalTexture(CreateRenderTarget(
					UpperFlowMapResource->GetRenderTargetTexture(),
					TEXT("RT_StormFlowMapUpper_ShapeInput")))
				: GSystemTextures.GetBlackDummy(GraphBuilder);
			{
				RDG_EVENT_SCOPE_STAT(GraphBuilder, SSS_ShapePass, "SavageSuperStorm ShapePass");
				FSavageSuperStormShaderInterface::AddShapePass_RenderThread(
					GraphBuilder,
					GlobalShaderMap,
					PassParameters,
					RDGUpperFlowMap,
					RDGTexture,
					RDGTexture2);
			}
			GraphBuilder.Execute();
		});
	LastBakedShapeHash = PendingShapeBakeHash;
	bHasLastBakedShapeHash = true;
	++ShapeBakeCount;
	return true;
}

void UStormRenderWorldSubsystem::DrawStatsOverlay(UCanvas* Canvas, APlayerController* )
{
	if (!Canvas || CVarStats.GetValueOnGameThread() == 0)
	{
		return;
	}

	const FSceneView* View = Canvas->SceneView;
	if (!View || !View->Family || View->Family->Scene == nullptr
		|| View->Family->Scene->GetWorld() != GetWorld())
	{
		return;
	}

	UFont* Font = GEngine ? GEngine->GetSmallFont() : nullptr;
	if (!Font)
	{
		return;
	}

	constexpr float X = 40.0f;
	float Y = 60.0f;

	auto Line = [&](const FLinearColor& Color, const FString& Text)
	{
		Canvas->SetDrawColor(Color.ToFColor(true));
		Y += Canvas->DrawText(Font, Text, X, Y) + 2.0f;
	};

	Line(FLinearColor(0.4f, 0.8f, 1.0f), TEXT("SavageSuperStorm Stats"));
	Line(FLinearColor::Green, FString::Printf(TEXT("Tick             %.4f ms"), LastTickMs));
	Line(FLinearColor::Green, FString::Printf(TEXT("Submit           %.4f ms"), LastSubmitMs));
	Line(FLinearColor::Green, FString::Printf(TEXT("BuildShapeRT GT  %.4f ms   (bakes %d)"), LastBuildShapeMs, ShapeBakeCount));

	if (!bHasStormRenderData)
	{
		Line(FLinearColor::Yellow, TEXT("No storm render data submitted."));
		return;
	}

	const FStormRenderData& D = StormRenderData;
	Line(FLinearColor::White, FString::Printf(
		TEXT("ShapeDirty %d    ShapeRT %d    Formation %d %.4f"),
		bShapeDirty ? 1 : 0,
		GetShapeRenderTargetResolution(),
		static_cast<int32>(D.FormationState),
		D.FormationProgress));
	Line(FLinearColor::White, FString::Printf(
		TEXT("Center %.4f %.4f %.4f"),
		D.WorldCenter.X, D.WorldCenter.Y, D.WorldCenter.Z));
	Line(FLinearColor::White, FString::Printf(
		TEXT("Extent %.4f %.4f %.4f    Radius %.4f"),
		D.WorldExtent.X, D.WorldExtent.Y, D.WorldExtent.Z, D.Shape.Radius));
	Line(FLinearColor::White, FString::Printf(
		TEXT("Time %.4f    Density %.4f    HF %.4f"),
		D.TimeSeconds, D.Shape.Density, D.Shape.HFStrength));
}
