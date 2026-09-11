#pragma once

#include "CoreMinimal.h"
#include "Curves/CurveFloat.h"
#include "StormTypes.generated.h"

UENUM(BlueprintType)
enum class EStormSimulationState : uint8
{
	Inactive UMETA(DisplayName = "Inactive"),
	Preview UMETA(DisplayName = "Preview"),
	Active UMETA(DisplayName = "Active"),
	FadingOut UMETA(DisplayName = "Fading Out")
};

// Presentation-only state for assembling and dispersing the already-authored
// storm. This never changes the mature ShapeRT design: Forming transports that
// exact density field inward from a shallow, wider source distribution, while
// Dissolving transports and dilutes it outward with a different (non-reversed)
// flow.
UENUM(BlueprintType)
enum class EStormFormationState : uint8
{
	Hidden UMETA(DisplayName = "Hidden"),
	Forming UMETA(DisplayName = "Forming"),
	Mature UMETA(DisplayName = "Mature"),
	Dissolving UMETA(DisplayName = "Dissolving")
};

USTRUCT(BlueprintType)
struct SAVAGESUPERSTORMRUNTIME_API FStormShapeSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Shape|Render Target", meta = (ClampMin = "1", ClampMax = "4096", UIMin = "128", UIMax = "2048"))
	int32 ShapeRenderTargetResolution = 512;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Storm|Shape", meta = (ClampMin = "1.0"))
	FVector Extent = FVector(800000, 800000, 2200);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Storm|Shape", meta = (ClampMin = "1.0"))
	float Radius = 800000;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Storm|Shape", meta = (ClampMin = "0.01"))
	float EnvelopeRadius = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Storm|Shape", meta = (ClampMin = "0.0"))
	float EnvelopeFalloff = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Shape")
	bool bClockwise = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Shape")
	float VerticalGrowth = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Shape")
	float AnvilSpread = 0.62f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Shape")
	float WaistTightness = 0.74f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Shape")
	float TurbulenceStrength = 0.0f;

	// Radius of the complete ShapeRT stencil relative to the authored storm radius.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Shape|Regions", meta = (ClampMin = "0.01", ClampMax = "2.0"))
	float OuterBrimRadiusScale = 1.35f;

	// Shapes radial Coverage between the outer floor and the saturated core.
	// Values below one reinforce the outer shelf.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Shape|Regions", meta = (ClampMin = "0.1", ClampMax = "4.0", UIMin = "0.35", UIMax = "1.5"))
	float CoveragePower = 0.65f;

	// Minimum raw Coverage at the outer brim. StormStencil still owns final containment.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Shape|Regions", meta = (ClampMin = "0.0", ClampMax = "0.95", UIMin = "0.0", UIMax = "0.6"))
	float CoverageFloor = 0.25f;

	// Combined outer allocation before CumulusShelfFraction is carved from the pure stratus region.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Shape|Regions", meta = (ClampMin = "0.0", ClampMax = "0.95"))
	float StratusShelfFraction = 0.65f;

	// Fraction of total radius reassigned from stratus to a pure TopType=0.5 cumulus shelf.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Shape|Regions", meta = (ClampMin = "0.0", ClampMax = "0.5", UIMin = "0.0", UIMax = "0.25"))
	float CumulusShelfFraction = 0.10f;


	// Optional minimum radius of the baked dent field in authored-radius units.
	// The field always reaches at least the complete Coverage/outer-brim radius.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Shape|Vortex Dent", meta = (ClampMin = "0.01", ClampMax = "2.0"))
	float VortexDentRegionRadius01 = 0.62f;

	// Keeps the eye clear while the surrounding sparse blobs form the spiral dent.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Shape|Vortex Dent", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float VortexDentInnerRadius01 = 0.10f;

	// Runtime baking can reduce this center twist to prevent sub-texel arcs.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Shape|Vortex Dent", meta = (ClampMin = "0.0", ClampMax = "12.566", UIMax = "8.0"))
	float VortexDentTwistRadians = 5.5f;

	// Values above one concentrate differential winding toward the outer Coverage
	// perimeter while the field remains continuous through the inner region.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Shape|Vortex Dent", meta = (ClampMin = "1.01", ClampMax = "4.0"))
	float VortexDentTwistPower = 1.7f;

	// Sparse Worley controls separate blob count (frequency/occupancy) from size.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Shape|Vortex Dent", meta = (ClampMin = "0.25", ClampMax = "64.0"))
	float VortexDentCellFrequency = 6.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Shape|Vortex Dent", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float VortexDentOccupancy = 0.35f;

	// Constant radial half-width, expressed in radial-cell units.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Shape|Vortex Dent", meta = (ClampMin = "0.05", ClampMax = "0.95"))
	float VortexDentBlobRadiusMinCell = 0.28f;

	// Inner tangential half-length, expressed in radial-cell units.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Shape|Vortex Dent", meta = (ClampMin = "0.05", ClampMax = "0.95"))
	float VortexDentBlobRadiusMaxCell = 0.46f;

	// Multiplies the outer tangential length. Winding and radial width remain
	// independent in the polar swirl frame.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Shape|Vortex Dent", meta = (ClampMin = "1.0", ClampMax = "3.0", UIMin = "1.0", UIMax = "2.5"))
	float VortexDentTendrilStretch = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Shape|Vortex Dent", meta = (ClampMin = "0", ClampMax = "65535"))
	int32 VortexDentSeed = 1337;

	// Single artist-facing control for the density-aware underside ambient fill.
	// The material still derives colour and energy from the current sky; this only
	// controls how strongly the underside structure is preserved.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Lighting", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float UndersideVisibility = 0.4;

	// Artist tint applied to the environment-responsive underside ambient fill.
	// The original property name restores the colour picker removed by change 106.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Lighting")
	FLinearColor BrimEmissiveColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Density", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Density = 0.65f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Density", meta = (ClampMin = "0.01"))
	float EdgeFalloff = 0.35f;
};

USTRUCT(BlueprintType)
struct SAVAGESUPERSTORMRUNTIME_API FStormCoreSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Weather", meta = (ClampMin = "0.0"))
	float WindSpeed = 1200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Weather", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LightningIntensity = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Weather", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PrecipitationIntensity = 0.5f;
};

USTRUCT(BlueprintType)
struct SAVAGESUPERSTORMRUNTIME_API FStormNoiseSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Noise", meta = (ClampMin = "0.001"))
	float NoiseScale = 0.003f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Noise", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float NoiseIntensity = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Noise", meta = (ClampMin = "0.0"))
	float NoiseSpeed = 0.18f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Wind")
	FVector WindDirection = FVector(1.0, 0.15, 0.05);
};
