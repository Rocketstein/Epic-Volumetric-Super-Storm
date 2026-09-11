#pragma once

#include "CoreMinimal.h"

struct FStormRenderData;
class UMaterialInterface;
class UTextureRenderTarget2D;
class UVolumetricCloudComponent;

// Single source of truth for the cloud material-parameter contract shared by
// UCloudInteractionWorldSubsystem and UStormMaterialBinderComponent. Keeping the
// MID parameter names and the shared geometry helpers here prevents the two
// binders from drifting apart (e.g. one renaming a parameter string and not the
// other).
namespace SavageSuperStorm::CloudMaterialParams
{
	// FName has no constexpr constructor, so these cannot be constexpr. `inline`
	// gives a single shared definition across every translation unit (C++17).
	inline const FName MotionTexture     (TEXT("StormMotionRT"));      // Departure RT0: (Dlow.xy, Dmid.xy)
	inline const FName MotionHiTexture   (TEXT("StormMotionHiRT"));    // Departure RT1: (Dhigh.xy, support, dMid.z)
	inline const FName AnchorHeights     (TEXT("StormAnchorHeights")); // (low, mid, high)
	inline const FName FieldExtent       (TEXT("StormFieldExtent"));   // half-extent, decodes normalized departure
	inline const FName FieldResolution   (TEXT("StormFieldResolution")); // RT width used for safe source-domain sampling
	inline const FName ShapeWarpStrength (TEXT("StormShapeWarpStrength"));
	inline const FName ErosionWarpStrength(TEXT("StormErosionWarpStrength"));
	inline const FName ShapeTexture      (TEXT("StormShapeRT"));
	inline const FName ShapeTexture2     (TEXT("StormShapeRT2"));
	inline const FName BottomProfileRT   (TEXT("BottomVerticalProfileRT"));
	inline const FName TopProfileRT      (TEXT("TopVerticalProfileRT"));
	inline const FName FlowMapLower      (TEXT("StormFlowMapLower"));
	inline const FName FlowMapMiddle     (TEXT("StormFlowMapMiddle"));
	inline const FName FlowMapUpper      (TEXT("StormFlowMapUpper"));
	inline const FName FlowMapLayerHeights(TEXT("StormFlowMapLayerHeights"));
	inline const FName FlowMapUVWStrength(TEXT("StormFlowMapUVWStrength")); // (normalized displacement, cycle seconds)
	inline const FName FlowMapEnabled    (TEXT("StormFlowMapEnabled"));
	inline const FName StormCenterRadius (TEXT("StormCenterRadius"));
	inline const FName StormCenter       (TEXT("StormCenter"));
	inline const FName StormExtent       (TEXT("StormExtent"));
	inline const FName StormRadius       (TEXT("StormRadius"));
	inline const FName StormReferenceCenter(TEXT("StormReferenceCenter"));
	inline const FName StormCoordinateScale(TEXT("StormCoordinateScale")); // reference Radius / current Radius
	inline const FName CloudLayerParams  (TEXT("CloudLayerParams"));
	inline const FName UndersideVisibility (TEXT("StormUndersideVisibility"));
	inline const FName BrimEmissiveColor (TEXT("BrimEmissiveColor"));
	inline const FName LifecycleControl (TEXT("StormLifecycleControl")); // runtime-owned, not artist-facing
	inline const FName AnvilStrength(TEXT("AnvilStrength"));
	inline const FName AnvilTypeBias(TEXT("AnvilTypeBias"));
	inline const FName AnvilCeilingFade01(TEXT("AnvilCeilingFade01"));
	inline const FName RotationSign(TEXT("RotationSign"));
	inline const FName OuterBrimRadiusScale(TEXT("OuterBrimRadiusScale"));
	inline const FName CoverageCeiling(TEXT("CoverageCeiling"));
	inline const FName CoverageRimValue(TEXT("CoverageRimValue"));
	inline const FName CoverageRimStart01(TEXT("CoverageRimStart01"));
	inline const FName ShapeTwistRadians(TEXT("ShapeTwistRadians"));
	inline const FName SpiralTwistPower(TEXT("SpiralTwistPower"));
	inline const FName SpiralFullStrengthRadius01(TEXT("SpiralFullStrengthRadius01"));
	inline const FName SpiralZeroStrengthRadius01(TEXT("SpiralZeroStrengthRadius01"));
	inline const FName LFNoiseWorldSize(TEXT("LFNoiseWorldSize"));
	inline const FName HFNoiseWorldSize(TEXT("HFNoiseWorldSize"));
	inline const FName CurlNoiseWorldSize(TEXT("CurlNoiseWorldSize"));
	inline const FName LFUnitsPerBodyRadius(TEXT("LFUnitsPerBodyRadius"));
	inline const FName HFUnitsPerBodyRadius(TEXT("HFUnitsPerBodyRadius"));
	inline const FName CurlUnitsPerBodyRadius(TEXT("CurlUnitsPerBodyRadius"));
	inline const FName DensityMultiplier (TEXT("DensityMultiplier"));
	inline const FName DensityGamma      (TEXT("DensityGamma"));
	inline const FName HFStrength        (TEXT("HFStrength"));
	inline const FName UseStormShapeRT   (TEXT("UseStormShapeRT"));
	inline const FName DepartureHorizon     (TEXT("StormDepartureHorizonSeconds"));       // fixed-horizon length the departure map backtraces over
	inline const FName VerticalDomainHeight (TEXT("StormDepartureVerticalDomainHeight")); // world height that normalizes the vertical departure
	inline const FName FieldPhaseRadians    (TEXT("StormFieldPhaseRadians"));             // persistent phase integrated from the probe-ring angular velocity; consumed by the shape RT pass and optional material paths
	inline const FName NoisePhaseRadians    (TEXT("StormNoisePhaseRadians"));             // desynchronized LF/noise material phase
	inline const FName ErosionPhaseRadians  (TEXT("StormErosionPhaseRadians"));           // desynchronized HF/erosion material phase
	inline const FName StormTimeSeconds     (TEXT("StormTimeSeconds"));               // continuous material flow clock; never used to mutate ShapeRT
	inline const FName LifecycleState       (TEXT("StormLifecycleState"));                // (phase01, ingest, convect, exhaust)
	inline const FName DissipationState     (TEXT("StormDissipationState"));              // (dissipation, turbulence01, lifecycleStrength, verticalDecorrelation)
	inline const FName VerticalGrowth      (TEXT("StormVerticalGrowth"));
	inline const FName AnvilSpread         (TEXT("StormAnvilSpread"));
	inline const FName WaistTightness      (TEXT("StormWaistTightness"));
	inline const FName BaseShelf           (TEXT("StormBaseShelf"));

	inline const FName MDRBoundaries0(TEXT("StormMDRBoundaries0"));
	inline const FName MDRBoundaries1(TEXT("StormMDRBoundaries1"));
	inline const FName MDRSpeeds0(TEXT("StormMDRSpeeds0"));
	inline const FName MDRSpeeds1(TEXT("StormMDRSpeeds1"));
	inline const FName MDRPhases0(TEXT("StormMDRPhases0"));
	inline const FName MDRPhases1(TEXT("StormMDRPhases1"));
	inline const FName MDRSkews0(TEXT("StormMDRSkews0"));
	inline const FName MDRSkews1(TEXT("StormMDRSkews1"));
	inline const FName MDRControl0(TEXT("StormMDRControl0"));
	inline const FName MDRControl1(TEXT("StormMDRControl1"));

	inline constexpr float KilometersToCentimeters = 100000.0f;

	// Compatibility coordinate payload: (LayerBottom, LayerHeight, 0, 0) in
	// centimeters. This is not part of the ShapeRT2 height-envelope decoder:
	// the material's CloudLayerHeight01 and ShapeRT2.r/b already share 0..1.
	// Uses the cloud component's layer when available, otherwise the supplied
	// fallbacks (already in world units).
	FLinearColor MakeCloudLayerParams(
		const UVolumetricCloudComponent* CloudComponent,
		float FallbackBottom,
		float FallbackHeight);

	// Accept only materials that declare both ShapeRT textures and the active
	// density contract. The material reuses the ShapeRT static-twist controls
	// for Anvil rotation; no Anvil-specific transform parameters are exposed.
	bool HasUnifiedFieldMaterialContract(const UMaterialInterface* Material);

	// Resolve supported SavageSuperStorm materials while leaving unrelated project
	// materials untouched. Returns nullptr when no current ShapeRT consumer
	// is available.
	UMaterialInterface* ResolveUnifiedFieldMaterial(UMaterialInterface* Candidate);
}
