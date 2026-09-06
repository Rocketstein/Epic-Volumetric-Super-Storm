/**
 * @file StormTypes.h
 * @brief Defines editable storm shape, motion, and lifecycle data.
 */

#pragma once

#include "CoreMinimal.h"
#include "Curves/CurveLinearColor.h"
#include "StormTypes.generated.h"

/** @brief Identifies the current formation or dissolution state. */
UENUM(BlueprintType)
enum class EStormFormationState : uint8
{
	Hidden UMETA(DisplayName = "Hidden"),
	Forming UMETA(DisplayName = "Forming"),
	Mature UMETA(DisplayName = "Mature"),
	Dissolving UMETA(DisplayName = "Dissolving")
};

/**
 * @brief Defines the radial curve channels baked into the shape textures.
 */
USTRUCT(BlueprintType)
struct SAVAGESUPERSTORMRUNTIME_API FStormShapeCurves
{
	GENERATED_BODY()

	FStormShapeCurves()
	{
		EnsureCoverageStrengthCurve();
		EnsureTypeStrengthCurve();
		EnsureLayerHeightCurve();
	}

	void EnsureCoverageStrengthCurve()
	{
		EnsureBodyBaseCoverageCurve(CoverageStrength, 0);
		EnsureAnvilCoverageCurve(CoverageStrength, 3);
	}

	void EnsureTypeStrengthCurve()
	{
		EnsureNormalizedCurve(TypeStrength, 1, 1.0f, 0.0f);
		EnsureNormalizedCurve(TypeStrength, 2, 1.0f, 0.0f);
	}

	void EnsureLayerHeightCurve()
	{
		EnsureNormalizedCurve(LayerHeight, 0, 0.08f, 0.12f);
		EnsureNormalizedCurve(LayerHeight, 2, 1.0f, 0.96f);
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Shape|Regions", meta = (DisplayName = "Coverage Strength (R=Body, A=Anvil)"))
	FRuntimeCurveLinearColor CoverageStrength;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Shape|Regions", meta = (DisplayName = "Type Strength (ShapeRT.G/B)"))
	FRuntimeCurveLinearColor TypeStrength;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Shape|Regions", meta = (DisplayName = "Min/Max Layer Height (ShapeRT2.R/B)"))
	FRuntimeCurveLinearColor LayerHeight;

private:
	static FRichCurve* GetLinearColorChannel(FRuntimeCurveLinearColor& Curve, int32 ChannelIndex)
	{
		if (ChannelIndex < 0 || ChannelIndex >= UE_ARRAY_COUNT(Curve.ColorCurves))
		{
			return nullptr;
		}

		return Curve.ExternalCurve ? &Curve.ExternalCurve->FloatCurves[ChannelIndex] : &Curve.ColorCurves[ChannelIndex];
	}

	static void EnsureCoverageCurveChannel(FRuntimeCurveLinearColor& Curve, int32 ChannelIndex, float CenterValue, float MiddleValue, float EdgeValue)
	{
		FRichCurve* RichCurve = GetLinearColorChannel(Curve, ChannelIndex);
		if (RichCurve && RichCurve->GetNumKeys() < 2)
		{
			RichCurve->Reset();
			RichCurve->AddKey(0.0f, CenterValue);
			RichCurve->AddKey(0.55f, MiddleValue);
			RichCurve->AddKey(1.0f, EdgeValue);
		}
	}

	static void EnsureBodyBaseCoverageCurve(FRuntimeCurveLinearColor& Curve, int32 ChannelIndex)
	{
		EnsureCoverageCurveChannel(Curve, ChannelIndex, 0.98f, 0.98f, 0.58f);
	}

	static void EnsureAnvilCoverageCurve(FRuntimeCurveLinearColor& Curve, int32 ChannelIndex)
	{
		EnsureCoverageCurveChannel(Curve, ChannelIndex, 0.98f, 0.98f, 0.58f);
	}

	static void EnsureNormalizedCurve(FRuntimeCurveLinearColor& Curve, int32 ChannelIndex, float CenterValue, float RadiusValue)
	{
		FRichCurve* RichCurve = GetLinearColorChannel(Curve, ChannelIndex);
		if (RichCurve && RichCurve->GetNumKeys() < 2)
		{
			RichCurve->Reset();
			RichCurve->AddKey(0.0f, CenterValue);
			RichCurve->AddKey(1.0f, RadiusValue);
		}
	}
};

/**
 * @brief Defines shape, density, lighting, wind, and render-target controls.
 */
USTRUCT(BlueprintType)
struct SAVAGESUPERSTORMRUNTIME_API FStormShapeSettings
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Storm|Shape|Render Target", meta = (ClampMin = "512", ClampMax = "2048", UIMin = "512", UIMax = "2048"))
	int32 ShapeRenderTargetResolution = 512;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Storm|Shape", meta = (ClampMin = "1.0"))
	FVector Extent = FVector(800000, 800000, 2200);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Storm|Shape", meta = (ClampMin = "1.0"))
	float Radius = 400000;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Storm|Shape", meta = (ClampMin = "0.01"))
	float EnvelopeRadius = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Storm|Shape", meta = (ClampMin = "0.0"))
	float EnvelopeFalloff = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Shape")
	bool bClockwise = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Shape|Anvil", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AnvilStrength = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Shape|Anvil", meta = (ToolTip = "Normalized depth measured downward from the upper cloud-layer anchor.", ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float AnvilDepth01 = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Shape|Anvil", meta = (DisplayName = "Anvil Height Twist Degrees", ToolTip = "Signed maximum upper-Anvil rotation relative to the anchored lower Anvil. Negative values twist in the opposite direction. This varies only with Anvil-local height and is not a radial polar twist.", ClampMin = "-30.0", ClampMax = "30.0", UIMin = "-30.0", UIMax = "30.0"))
	float AnvilHeightTwistDegrees = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Shape|Anvil", meta = (DisplayName = "Anvil Height Twist Start", ToolTip = "Anvil-local height where upper-biased twist begins. Zero is the lower boundary and one is the upper anchor.", ClampMin = "0.0", ClampMax = "0.9", UIMin = "0.0", UIMax = "0.9"))
	float AnvilHeightTwistStart01 = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Shape|Anvil", meta = (DisplayName = "Anvil Coverage", ClampMin = "0.2", ClampMax = "1.0", UIMin = "0.2", UIMax = "1.0"))
	float AnvilCoverage = 0.35f;

	FORCEINLINE float GetAnvilOuterRadiusScale() const
	{
		return 1.0f + FMath::Clamp(AnvilCoverage, 0.2f, 1.0f);
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Shape")
	FLinearColor StormBaseColor = FLinearColor(0.182f, 0.189f, 0.193f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Shape|Regions")
	FStormShapeCurves ShapeCurves;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Shape|Lighting", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float UndersideVisibility = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Shape|Lighting")
	FLinearColor BrimEmissiveColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Density", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Density = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Density", meta = (ClampMin = "0.001", ClampMax = "4.0"))
	float DensityGamma = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Density", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HFStrength = 1.0f;

	/** @brief Horizontal direction used to orient shape variation and upper flow. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Shape|Wind")
	FVector WindDirection = FVector(1.0, 0.15, 0.0);
};

/** @brief Selects how adjacent motion rings blend at their boundaries. */
UENUM(BlueprintType)
enum class EStormRingTransitionMode : uint8
{
	SpeedBlend UMETA(DisplayName = "Speed Blend - Low Cost"),
	ResultBlend UMETA(DisplayName = "Noise Result Blend - High Quality"),
	Jittered UMETA(DisplayName = "Jittered Boundary - Low Cost")
};

/**
 * @brief Defines ring-based motion controls consumed by the density shader.
 */
USTRUCT(BlueprintType)
struct SAVAGESUPERSTORMRUNTIME_API FStormMotionSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Motion")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Motion")
	bool bPreviewInEditor = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Motion", meta = (ClampMin = "0.0"))
	float MotionStrength = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Motion", meta = (ClampMin = "0.0"))
	float TimeScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Motion|Rings", meta = (ClampMin = "1", ClampMax = "6", UIMin = "1", UIMax = "6", ForceRebuildProperty))
	int32 RingCount = 6;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = "Storm|Motion|Rings", meta = (DisplayName = "Visualize Ring Influence Ranges", ToolTip = "Color the storm's sampled ring ranges, overlap, and motion mask."))
	bool bDebugDrawRingEndRadii = false;
#endif

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Motion|Rings", meta = (EditFixedSize, ClampMin = "0.01", ClampMax = "0.99", UIMin = "0.01", UIMax = "0.99", ToolTip = "Ordered Ring boundaries normalized across the complete Anvil radius. 1.0 equals Shape Radius multiplied by (1 + Anvil Coverage)."))
	TArray<float> RingEndRadii01 = { 0.10f, 0.22f, 0.36f, 0.54f, 0.74f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Storm|Motion|Rings", meta = (EditFixedSize))
	TArray<float> RingAngularSpeedDegrees = { 3.00f, 2.40f, 1.80f, 1.25f, 0.75f, 0.35f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Storm|Motion|Rings", meta = (EditFixedSize))
	TArray<float> RingSkewDegrees = { 140.0f, 96.0f, 60.0f, 34.0f, 16.0f, 0.0f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Storm|Motion|Boundary", meta = (ClampMin = "0.0", ClampMax = "0.2"))
	float BoundaryOverlap01 = 0.04f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Storm|Motion|Boundary")
	EStormRingTransitionMode TransitionMode = EStormRingTransitionMode::ResultBlend;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Storm|Motion|Mask", meta = (ClampMin = "0.0", ClampMax = "2.0", ToolTip = "Motion outer radius relative to the complete Anvil radius. 1.0 covers the full Storm."))
	float MotionRadiusScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Storm|Motion|Mask", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float RadialFeather01 = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Storm|Motion|Height", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HeightMin01 = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Storm|Motion|Height", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HeightMax01 = 0.62f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Storm|Motion|Height", meta = (ClampMin = "0.001", ClampMax = "0.5"))
	float HeightFeather01 = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Storm|Motion|Noise")
	float LFRotationMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Storm|Motion|Noise")
	float HFRotationMultiplier = 1.08f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Storm|Motion|Noise")
	float CurlRotationMultiplier = 1.12f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Storm|Motion|Rings", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float RadialShearGain = 0.6f;
};