from pathlib import Path


path = Path("Plugins/SavageSuperStorm/Source/SavageSuperStormRuntime/Public/Field/StormDensityContract.h")
text = path.read_text(encoding="utf-8")


def replace_once(old: str, new: str) -> None:
    global text
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"Expected one contract block, found {count}: {old[:80]!r}")
    text = text.replace(old, new)


replace_once(
"""inline constexpr float DefaultAnvilStrength = 0.0f;
inline constexpr float DefaultAnvilTypeBias = 0.0f;
inline constexpr float DefaultAnvilCeilingFade01 = 0.05f;
inline constexpr float DefaultAnvilWindDirectionX = 1.0f;
inline constexpr float DefaultAnvilWindDirectionY = 0.0f;
inline constexpr float DefaultAnvilWindStretch = 1.8f;
inline constexpr float DefaultAnvilWindSkew = 0.20f;
inline constexpr float DefaultAnvilWarpStart01 = 0.62f;
inline constexpr float DefaultAnvilWarpEnd01 = 0.86f;
""",
"""inline constexpr float DefaultAnvilStrength = 0.0f;
inline constexpr float DefaultAnvilTypeBias = 0.0f;
inline constexpr float DefaultAnvilCeilingFade01 = 0.05f;
""",
)

replace_once(
"""inline constexpr float DefaultCoverageCeiling = 0.98f;
inline constexpr float DefaultCoverageRimValue = 0.58f;
inline constexpr float DefaultCoverageRimStart01 = 0.55f;
inline constexpr float DefaultShapeTwistRadians = 2.97f;
inline constexpr float DefaultSpiralTendrilRimCrispness = 0.55f;
inline constexpr float DefaultSpiralTendrilCoreIntensity01 = 0.12f;
inline constexpr float DefaultSpiralTendrilRimIntensity01 = 1.0f;
inline constexpr float DefaultSpiralTendrilRampStart01 = 0.15f;
inline constexpr float DefaultSpiralTendrilRampEnd01 = 0.75f;
""",
"""inline constexpr float DefaultCoverageCeiling = 0.98f;
inline constexpr float DefaultCoverageRimValue = 0.58f;
inline constexpr float DefaultCoverageRimStart01 = 0.55f;
inline constexpr float DefaultShapeTwistRadians = 2.97f;
""",
)

replace_once(
"""//   WispyAmount = saturate(BaseBottomType) inside the fixed lower band
//   SpiralTendrilMask is stored in ShapeRT2.G but does not feed density
//   BodyProfile = BottomProfile(BaseBottomType, HLocal)
//       * TopProfile(TopType, HLocal)
//   FlippedAnvilProfile = BottomProfile(BaseBottomType, 1 - HLocal)
//       * TopProfile(TopType + AnvilTypeBias, 1 - HLocal)
//       * CeilingEnvelope(HLocal)
//   AnvilWeight = saturate(AnvilStrength) * saturate(SuperstormCoverage)
""",
"""//   WispyAmount = saturate(BaseBottomType) inside the fixed lower band
//   ShapeRT2.G = saturate(TwistedModelingNoise * MatureStencil)
//   raw TwistedModelingNoise affects Shape-compute Coverage and TopType only
//   BodyProfile = BottomProfile(BaseBottomType, HLocal)
//       * TopProfile(FinalTopType, HLocal)
//   FlippedAnvilProfile = BottomProfile(BaseBottomType, 1 - HLocal)
//       * TopProfile(FinalTopType + AnvilTypeBias, 1 - HLocal)
//       * CeilingEnvelope(HLocal)
//   AnvilWeight = saturate(AnvilStrength) * saturate(MatureStencil)
//   Anvil coordinates use one center-relative inverse rotation and no skew
""",
)

replace_once(
"""struct FShapeTendrilSignals
{
    float Coverage = 0.0f;
    float SpiralTendrilMask = 0.0f;
};
""",
"""struct FShapeModelingSignals
{
    float TwistedModelingNoise = 0.0f;
};
""",
)

replace_once(
"""inline float EvaluateAnvilWeight(
    float AnvilStrength,
    float SuperstormCoverage)
{
    return Saturate(AnvilStrength) * Saturate(SuperstormCoverage);
}
""",
"""inline float EvaluateAnvilWeight(
    float AnvilStrength,
    float MatureStencil)
{
    return Saturate(AnvilStrength) * Saturate(MatureStencil);
}

inline float EvaluateAnvilAddedProfile(
    float NormalProfile,
    float AnvilProfile)
{
    return Saturate(AnvilProfile - NormalProfile);
}

inline float EvaluateAnvilRotationWeight(
    float AnvilWeight,
    float AnvilAddedProfile)
{
    return Saturate(AnvilWeight * AnvilAddedProfile);
}
""",
)

anvil_start = text.index("inline float EvaluateAnvilWarpWeight(")
anvil_end = text.index("inline float RotationHandedness(", anvil_start)
text = text[:anvil_start] + """inline FVector2f EvaluateAnvilRotationBodyOffset(
    const FVector2f& BodyCoord,
    float AnvilRotationWeight,
    float OuterBrimRadiusScale,
    float RotationSign,
    float ShapeTwistRadians,
    float SpiralTwistPower,
    float SpiralFullStrengthRadius01,
    float SpiralZeroStrengthRadius01)
{
    const float Radius01 = FMath::Sqrt(
        BodyCoord.X * BodyCoord.X + BodyCoord.Y * BodyCoord.Y);
    const float MaterialRadiusAcrossOuterBrim01 = Saturate(
        Radius01 / FMath::Max(FMath::Abs(OuterBrimRadiusScale), Epsilon));
    const float FullAngle = EvaluateSpiralAngleRadians(
        MaterialRadiusAcrossOuterBrim01,
        RotationSign,
        ShapeTwistRadians,
        1.0f,
        SpiralFullStrengthRadius01,
        SpiralZeroStrengthRadius01,
        SpiralTwistPower);
    const FVector2f SourceBody = RotateSpiral2D(
        BodyCoord,
        -FullAngle * Saturate(AnvilRotationWeight));
    return SourceBody - BodyCoord;
}

inline FAnvilNoiseCoordinates ApplyAnvilNoiseRotation(
    const FAnvilNoiseCoordinates& Source,
    const FVector2f& BodyCoord,
    float AnvilRotationWeight,
    float OuterBrimRadiusScale,
    float RotationSign,
    float ShapeTwistRadians,
    float SpiralTwistPower,
    float SpiralFullStrengthRadius01,
    float SpiralZeroStrengthRadius01,
    float LFUnitsPerBodyRadius,
    float HFUnitsPerBodyRadius,
    float CurlUnitsPerBodyRadius)
{
    const FVector2f Offset = EvaluateAnvilRotationBodyOffset(
        BodyCoord,
        AnvilRotationWeight,
        OuterBrimRadiusScale,
        RotationSign,
        ShapeTwistRadians,
        SpiralTwistPower,
        SpiralFullStrengthRadius01,
        SpiralZeroStrengthRadius01);
    FAnvilNoiseCoordinates Result = Source;
    Result.LFUVW += FVector3f(
        Offset * FMath::Abs(LFUnitsPerBodyRadius), 0.0f);
    Result.HFUVW += FVector3f(
        Offset * FMath::Abs(HFUnitsPerBodyRadius), 0.0f);
    Result.CurlUVW += Offset * FMath::Abs(CurlUnitsPerBodyRadius);
    return Result;
}

""" + text[anvil_end:]

replace_once(
"""inline FShapeTendrilSignals EvaluateShapeTendrilSignals(
    float Coverage,
    float RawSpiralTendrilMask,
    float MatureStencil)
{
    FShapeTendrilSignals Result;
    Result.Coverage = Coverage;
    if (MatureStencil > 1.0e-5f)
    {
        Result.SpiralTendrilMask = Saturate(
            RawSpiralTendrilMask * MatureStencil);
    }
    return Result;
}
""",
"""inline FShapeModelingSignals EvaluateShapeModelingSignals(
    float RawTwistedModelingNoise,
    float MatureStencil)
{
    FShapeModelingSignals Result;
    Result.TwistedModelingNoise = Saturate(
        RawTwistedModelingNoise * MatureStencil);
    return Result;
}
""",
)

tendril_start = text.index("inline float EvaluateTendrilRadialIntensity(")
tendril_end = text.index("inline float EvaluateRadialCloudType(", tendril_start)
text = text[:tendril_start] + """inline float EvaluateCoverageComposite(
    float RadiusAcrossOuterBrim01,
    float RawTwistedModelingNoise,
    float CenterMask,
    float CoverageCeiling = DefaultCoverageCeiling,
    float CoverageRimValue = DefaultCoverageRimValue,
    float CoverageRimStart01 = DefaultCoverageRimStart01)
{
    const float Ceiling = Saturate(CoverageCeiling);
    const float Minimum = FMath::Min(Saturate(CoverageRimValue), Ceiling);
    const float RadialCoverage = EvaluateRadialCoverage(
        RadiusAcrossOuterBrim01,
        Ceiling,
        Minimum,
        CoverageRimStart01);
    const float NoisyCoverage = FMath::Lerp(
        Minimum,
        RadialCoverage,
        Saturate(RawTwistedModelingNoise));
    return Saturate(FMath::Lerp(
        NoisyCoverage,
        Ceiling,
        Saturate(CenterMask)));
}

inline float EvaluateTopTypeComposite(
    float RadiusAcrossOuterBrim01,
    float RawTwistedModelingNoise,
    float CenterMask)
{
    const float RadialTopType = 1.0f - Saturate(RadiusAcrossOuterBrim01);
    const float NoisyTopType = Saturate(
        RadialTopType * Saturate(RawTwistedModelingNoise));
    return Saturate(FMath::Lerp(
        NoisyTopType,
        1.0f,
        Saturate(CenterMask)));
}

""" + text[tendril_end:]

path.write_text(text, encoding="utf-8")
