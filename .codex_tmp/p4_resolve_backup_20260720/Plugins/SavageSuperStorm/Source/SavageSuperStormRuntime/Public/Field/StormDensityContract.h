#pragma once

#include "CoreMinimal.h"

namespace SavageSuperStorm::DensityContract
{
inline constexpr float Epsilon = 1.0e-4f;
inline constexpr float CentimetersToKilometers = 1.0e-5f;
inline constexpr float DefaultNubisHFStrength = 0.20f;
inline constexpr float DefaultCoveragePower = 0.65f;
inline constexpr float DefaultCoverageFloor = 0.30f;
inline constexpr float DefaultCoverageCeiling = 0.80f;
inline constexpr float DefaultBottomTypeOuter = 0.00f;
inline constexpr float DefaultBottomTypeCore = 0.00f;
inline constexpr float DefaultStratusShelfFraction = 0.65f;
inline constexpr float DefaultCumulusShelfFraction = 0.10f;
inline constexpr float DefaultCumulusShareOfInnerRadius = 0.57142857f;
inline constexpr float DefaultLiftedStratusType = 0.30f;
inline constexpr float DefaultPureStratusShare = 0.30f;
inline constexpr float DefaultStratusLiftBlendShare = 0.10f;

// CPU mirror of the active shader contract:
//   LFShape = remap(LF.r, -(1 - LFFBM), 1, 0, 1)
//   Bottom  = BottomProfile(BottomType)
//   Top     = TopProfile(TopType)
//   Profile = Bottom * Top
//   Base    = LFShape * Profile
//   Covered = remap(Base, 1 - Coverage, 1, 0, 1) * Coverage
//   Coarse  = Covered * HeightInside * StormStencil
//   HFMod   = lerp(HFFBM, 1 - HFFBM, saturate(HLocal * 10))
//   Fine    = remap(Covered, HFMod * HFStrength, 1, 0, 1)
//             * HeightInside * StormStencil

struct FHeightSample
{
    float MinLayerHeight01 = 0.0f;
    float MaxLayerHeight01 = 1.0f;
    float HeightInside = 0.0f;
    float HLocal = 0.0f;
};

struct FNoiseCoordinates
{
    FVector3f ReferenceStormLocalKm = FVector3f::ZeroVector;
    FVector3f LFReferencePositionKm = FVector3f::ZeroVector;
    FVector3f HFReferencePositionKm = FVector3f::ZeroVector;
    FVector2f CurlReferencePositionKm = FVector2f::ZeroVector;
};

inline float Remap(
    float Value,
    float OriginalMin,
    float OriginalMax,
    float NewMin,
    float NewMax)
{
    const float Denominator = OriginalMax - OriginalMin;
    if (FMath::Abs(Denominator) < Epsilon)
    {
        return NewMin;
    }

    return NewMin
        + ((Value - OriginalMin) / Denominator)
        * (NewMax - NewMin);
}

inline float RemapSaturated(
    float Value,
    float OriginalMin,
    float OriginalMax,
    float NewMin,
    float NewMax)
{
    return FMath::Clamp(
        Remap(Value, OriginalMin, OriginalMax, NewMin, NewMax),
        0.0f,
        1.0f);
}

inline float DecodeNubisLFShape(const FVector4f& LFRaw)
{
    const float LFWorley = FMath::Clamp(
        LFRaw.Y * 0.625f + LFRaw.Z * 0.250f + LFRaw.W * 0.125f,
        0.0f,
        1.0f);
    return RemapSaturated(
        FMath::Clamp(LFRaw.X, 0.0f, 1.0f),
        -(1.0f - LFWorley),
        1.0f,
        0.0f,
        1.0f);
}

inline float DecodeNubisHFFBM(const FVector3f& HFRaw)
{
    return FMath::Clamp(
        HFRaw.X * 0.625f + HFRaw.Y * 0.250f + HFRaw.Z * 0.125f,
        0.0f,
        1.0f);
}

inline float ApplyCoverage(float BaseCloud, float Coverage)
{
    const float ClampedCoverage = FMath::Clamp(Coverage, 0.0f, 1.0f);
    return RemapSaturated(
        FMath::Clamp(BaseCloud, 0.0f, 1.0f),
        1.0f - ClampedCoverage,
        1.0f,
        0.0f,
        1.0f)
        * ClampedCoverage;
}

inline float EvaluateCoveredBaseCloud(
    float LFShape,
    float VerticalProfile,
    float Coverage)
{
    const float BaseCloud = FMath::Clamp(LFShape, 0.0f, 1.0f)
        * FMath::Clamp(VerticalProfile, 0.0f, 1.0f);
    return ApplyCoverage(BaseCloud, Coverage);
}

inline float EvaluateHeightDependentHFModifier(float HFFBM, float HLocal)
{
    const float HF = FMath::Clamp(HFFBM, 0.0f, 1.0f);
    return FMath::Lerp(
        HF,
        1.0f - HF,
        FMath::Clamp(HLocal * 10.0f, 0.0f, 1.0f));
}

inline float EvaluateBottomTopVerticalProfile(
    float BottomProfile,
    float TopProfile)
{
    return FMath::Clamp(
        FMath::Clamp(BottomProfile, 0.0f, 1.0f)
            * FMath::Clamp(TopProfile, 0.0f, 1.0f),
        0.0f,
        1.0f);
}

inline float ApplyHFErosion(
    float CoveredBaseCloud,
    float HFModifier,
    float HFStrength = DefaultNubisHFStrength)
{
    return RemapSaturated(
        FMath::Clamp(CoveredBaseCloud, 0.0f, 1.0f),
        FMath::Clamp(HFModifier, 0.0f, 1.0f)
            * FMath::Max(HFStrength, 0.0f),
        1.0f,
        0.0f,
        1.0f);
}

inline float ApplyDensityMasks(
    float Density,
    float HeightInside,
    float StormStencil)
{
    return FMath::Clamp(Density, 0.0f, 1.0f)
        * FMath::Clamp(HeightInside, 0.0f, 1.0f)
        * FMath::Clamp(StormStencil, 0.0f, 1.0f);
}

inline FHeightSample DecodeLayerHeight(
    float CloudLayerHeight01,
    float MinLayerHeight01,
    float MaxLayerHeight01)
{
    FHeightSample Result;
    Result.MinLayerHeight01 = FMath::Clamp(
        MinLayerHeight01, 0.0f, 1.0f);
    Result.MaxLayerHeight01 = FMath::Max(
        FMath::Clamp(MaxLayerHeight01, 0.0f, 1.0f),
        Result.MinLayerHeight01 + Epsilon);
    Result.HeightInside =
        (CloudLayerHeight01 >= Result.MinLayerHeight01
            && CloudLayerHeight01 <= Result.MaxLayerHeight01)
        ? 1.0f
        : 0.0f;
    Result.HLocal = FMath::Clamp(
        (CloudLayerHeight01 - Result.MinLayerHeight01)
            / FMath::Max(
                Result.MaxLayerHeight01 - Result.MinLayerHeight01,
                Epsilon),
        0.0f,
        1.0f);
    return Result;
}

inline FHeightSample DecodeHeight(
    float CloudLayerHeight01,
    float MinLayerHeight01,
    float MaxLayerHeight01)
{
    return DecodeLayerHeight(
        CloudLayerHeight01,
        MinLayerHeight01,
        MaxLayerHeight01);
}

inline float CurrentRadiusKm(float CurrentStormRadiusCm)
{
    return FMath::Max(
        FMath::Abs(CurrentStormRadiusCm) * CentimetersToKilometers,
        1.0e-3f);
}

inline float ProportionalScale(
    float CurrentStormRadiusCm,
    float ReferenceStormRadiusKm)
{
    return CurrentRadiusKm(CurrentStormRadiusCm)
        / FMath::Max(FMath::Abs(ReferenceStormRadiusKm), 1.0e-3f);
}

inline FVector3f MakeReferenceLocalKm(
    const FVector3f& CurrentStormLocalKm,
    float StormScale)
{
    return CurrentStormLocalKm / FMath::Max(FMath::Abs(StormScale), 1.0e-3f);
}

inline FNoiseCoordinates MakeNoiseCoordinates(
    const FVector3f& CurrentStormLocalKm,
    float StormScale,
    const FVector3f& ReferenceAdvectionVelocityLocalKmPerSec,
    float TimeSeconds,
    float LFAdvectionMultiplier,
    float HFAdvectionMultiplier,
    float CurlAdvectionMultiplier)
{
    FNoiseCoordinates Result;
    Result.ReferenceStormLocalKm = MakeReferenceLocalKm(
        CurrentStormLocalKm,
        StormScale);
    Result.LFReferencePositionKm = Result.ReferenceStormLocalKm
        - ReferenceAdvectionVelocityLocalKmPerSec
            * TimeSeconds
            * LFAdvectionMultiplier;
    Result.HFReferencePositionKm = Result.ReferenceStormLocalKm
        - ReferenceAdvectionVelocityLocalKmPerSec
            * TimeSeconds
            * HFAdvectionMultiplier;
    Result.CurlReferencePositionKm = FVector2f(
        Result.ReferenceStormLocalKm.X,
        Result.ReferenceStormLocalKm.Y)
        - FVector2f(
            ReferenceAdvectionVelocityLocalKmPerSec.X,
            ReferenceAdvectionVelocityLocalKmPerSec.Y)
            * TimeSeconds
            * CurlAdvectionMultiplier;
    return Result;
}

inline FVector2f CurlOffsetReferenceKm(
    const FVector2f& CurlVector,
    float CurlReferenceDisplacementKm,
    float HLocal)
{
    return CurlVector
        * CurlReferenceDisplacementKm
        * (1.0f - FMath::Clamp(HLocal, 0.0f, 1.0f));
}

inline float EvaluateRadialCoverage(
    float RadiusAcrossOuterBrim01,
    float CoveragePower = DefaultCoveragePower,
    float CoverageFloor = DefaultCoverageFloor,
    float CoverageCeiling = DefaultCoverageCeiling)
{
    const float LinearCoverage = 1.0f
        - FMath::Clamp(RadiusAcrossOuterBrim01, 0.0f, 1.0f);
    const float PoweredCoverage = FMath::Pow(
        LinearCoverage,
        FMath::Max(CoveragePower, Epsilon));
    const float Ceiling = FMath::Clamp(CoverageCeiling, 0.0f, 1.0f);
    return FMath::Lerp(
        FMath::Clamp(CoverageFloor, 0.0f, Ceiling),
        Ceiling,
        PoweredCoverage);
}

inline float EvaluateRadialTopType(
    float RadiusAcrossOuterBrim01,
    float StratusShelfFraction = DefaultStratusShelfFraction,
    float CumulusShelfFraction = DefaultCumulusShelfFraction)
{
    const float OuterShelfAllocation = FMath::Clamp(
        StratusShelfFraction, 0.0f, 0.95f);
    const float CumulusShelf = FMath::Clamp(
        CumulusShelfFraction, 0.0f, OuterShelfAllocation);
    const float StratusShelf = OuterShelfAllocation - CumulusShelf;
    const float TransitionFraction = FMath::Max(
        1.0f - StratusShelf - CumulusShelf, 1.0e-3f);
    const float CumulusShelfStart = StratusShelf
        + TransitionFraction * DefaultCumulusShareOfInnerRadius;
    const float CumulusShelfEnd = FMath::Min(
        CumulusShelfStart + CumulusShelf, 1.0f);
    const float InwardCoordinate = 1.0f
        - FMath::Clamp(RadiusAcrossOuterBrim01, 0.0f, 1.0f);
    const float LiftedStratusStart = StratusShelf
        * DefaultPureStratusShare;
    const float LiftedStratusEnd = StratusShelf
        * (DefaultPureStratusShare + DefaultStratusLiftBlendShare);
    const float LiftedStratusBlend = StratusShelf > Epsilon
        ? FMath::SmoothStep(
            LiftedStratusStart,
            FMath::Max(LiftedStratusEnd, LiftedStratusStart + Epsilon),
            InwardCoordinate)
        : 0.0f;
    const float LiftedStratusType = DefaultLiftedStratusType
        * LiftedStratusBlend;
    const float StratusToCumulus = FMath::SmoothStep(
        StratusShelf, CumulusShelfStart, InwardCoordinate);
    const float CumulusToCumulonimbus = FMath::SmoothStep(
        CumulusShelfEnd, 1.0f, InwardCoordinate);
    return FMath::Lerp(
        FMath::Lerp(LiftedStratusType, 0.5f, StratusToCumulus),
        1.0f,
        CumulusToCumulonimbus);
}

// ShapeRT.G is the independent horizontal coordinate for the Bottom profile
// LUT. It does not drive any additional underside mask or erosion branch.
inline float EvaluateRadialBottomType(
    float RadiusAcrossOuterBrim01,
    float CoveragePower = DefaultCoveragePower,
    float BottomTypeOuter = DefaultBottomTypeOuter,
    float BottomTypeCore = DefaultBottomTypeCore)
{
    const float LinearCoverage = 1.0f
        - FMath::Clamp(RadiusAcrossOuterBrim01, 0.0f, 1.0f);
    const float PoweredCoverage = FMath::Pow(
        LinearCoverage,
        FMath::Max(CoveragePower, Epsilon));
    return FMath::Lerp(
        FMath::Clamp(BottomTypeOuter, 0.0f, 1.0f),
        FMath::Clamp(BottomTypeCore, 0.0f, 1.0f),
        PoweredCoverage);
}

inline float EvaluateCoverageAlignedBottomType(
    float RadiusAcrossOuterBrim01,
    float CoveragePower = DefaultCoveragePower,
    float BottomTypeOuter = DefaultBottomTypeOuter,
    float BottomTypeCore = DefaultBottomTypeCore)
{
    return EvaluateRadialBottomType(
        RadiusAcrossOuterBrim01,
        CoveragePower,
        BottomTypeOuter,
        BottomTypeCore);
}

inline FVector2f ClampProfileUV(
    float ProfileType,
    float ProfileV,
    int32 ProfileWidth,
    int32 ProfileHeight)
{
    const FVector2f HalfTexel(
        0.5f / static_cast<float>(FMath::Max(ProfileWidth, 1)),
        0.5f / static_cast<float>(FMath::Max(ProfileHeight, 1)));
    return FVector2f(
        FMath::Clamp(ProfileType, HalfTexel.X, 1.0f - HalfTexel.X),
        FMath::Clamp(ProfileV, HalfTexel.Y, 1.0f - HalfTexel.Y));
}

inline float EvaluateUndersideLightingDetailSupport(
    float FineDensity,
    float CoarseDensity)
{
    const float Fine = FMath::Clamp(FineDensity, 0.0f, 1.0f);
    const float Coarse = FMath::Clamp(CoarseDensity, 0.0f, 1.0f);
    const float FineSupport = FMath::Clamp(
        Fine / FMath::Max(Coarse, 1.0e-3f),
        0.0f,
        1.0f);
    return FMath::SmoothStep(0.15f, 0.45f, FineSupport);
}
}
