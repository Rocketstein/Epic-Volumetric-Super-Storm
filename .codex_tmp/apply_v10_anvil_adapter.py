from pathlib import Path


TARGET = Path(
    "Plugins/SavageSuperStorm/Shaders/Public/StormDensityMaterialAdapter.ush"
)


def replace_exact(text: str, old: str, new: str, expected: int = 1) -> str:
    actual = text.count(old)
    if actual != expected:
        raise RuntimeError(
            f"expected {expected} exact match(es), found {actual}: {old[:80]!r}"
        )
    return text.replace(old, new)


text = TARGET.read_text(encoding="utf-8")

text = replace_exact(
    text,
    '#include "/SavageSuperStormShaders/Public/StormDensityField.ush"\n',
    '#include "/SavageSuperStormShaders/Public/StormDensityField.ush"\n'
    '#include "/SavageSuperStormShaders/Public/StormSpiralContract.ush"\n',
)

old_helper = """void StormApplyUpperAnvilNoiseWarp(
    FStormDomainSample Domain,
    float HLocal,
    float AnvilWeight,
    float2 AnvilWindDirectionXY,
    float AnvilWindStretch,
    float AnvilWindSkew,
    float AnvilWarpStart01,
    float AnvilWarpEnd01,
    float LFUnitsPerBodyRadius,
    float HFUnitsPerBodyRadius,
    float CurlUnitsPerBodyRadius,
    float3 LFUVW,
    float3 HFUVW,
    float2 CurlUVW,
    out float3 OutLFUVW,
    out float3 OutHFUVW,
    out float2 OutCurlUVW)
{
    float2 wind = StormTransportSafeDirection(AnvilWindDirectionXY);
    float2 crossWind = float2(-wind.y, wind.x);
    float2 windLocal = float2(
        dot(Domain.BodyCoord, wind),
        dot(Domain.BodyCoord, crossWind));

    // This is the inverse of a wind-axis stretch followed by an XY shear.
    // Applying only the body-space delta preserves lifecycle transport and
    // advection already present in the current source coordinates.
    float inverseStretch = rcp(max(abs(AnvilWindStretch), 1.0e-3));
    float2 inverseLocal = float2(
        (windLocal.x - windLocal.y * AnvilWindSkew) * inverseStretch,
        windLocal.y);
    float2 targetBody = wind * inverseLocal.x
        + crossWind * inverseLocal.y;

    float warpStart = clamp(
        AnvilWarpStart01,
        0.0,
        1.0 - 1.0e-4);
    float warpEnd = clamp(
        AnvilWarpEnd01,
        warpStart + 1.0e-4,
        1.0);
    float warpWeight = smoothstep(
        warpStart,
        warpEnd,
        saturate(HLocal))
        * saturate(AnvilWeight);
    float2 bodyDelta = (targetBody - Domain.BodyCoord) * warpWeight;

    OutLFUVW = LFUVW;
    OutHFUVW = HFUVW;
    OutCurlUVW = CurlUVW;
    OutLFUVW.xy += bodyDelta * abs(LFUnitsPerBodyRadius);
    OutHFUVW.xy += bodyDelta * abs(HFUnitsPerBodyRadius);
    OutCurlUVW += bodyDelta * abs(CurlUnitsPerBodyRadius);
}
"""

new_helper = """void StormApplyAnvilNoiseRotation(
    FStormDomainSample Domain,
    float AnvilRotationWeight,
    float OuterBrimRadiusScale,
    float RotationSign,
    float ShapeTwistRadians,
    float SpiralTwistPower,
    float SpiralFullStrengthRadius01,
    float SpiralZeroStrengthRadius01,
    float LFUnitsPerBodyRadius,
    float HFUnitsPerBodyRadius,
    float CurlUnitsPerBodyRadius,
    float3 LFUVW,
    float3 HFUVW,
    float2 CurlUVW,
    out float3 OutLFUVW,
    out float3 OutHFUVW,
    out float2 OutCurlUVW)
{
    float materialRadiusAcrossOuterBrim01 = saturate(
        Domain.Radius01
        / max(abs(OuterBrimRadiusScale), 1.0e-4));
    float fullAngle = StormEvaluateSpiralAngleRadians(
        materialRadiusAcrossOuterBrim01,
        RotationSign,
        ShapeTwistRadians,
        1.0,
        SpiralFullStrengthRadius01,
        SpiralZeroStrengthRadius01,
        SpiralTwistPower);
    float2 sourceBody = StormSpiralRotate2D(
        Domain.BodyCoord,
        -fullAngle * saturate(AnvilRotationWeight));
    float2 bodyOffset = sourceBody - Domain.BodyCoord;

    OutLFUVW = LFUVW;
    OutHFUVW = HFUVW;
    OutCurlUVW = CurlUVW;
    OutLFUVW.xy += bodyOffset * LFUnitsPerBodyRadius;
    OutHFUVW.xy += bodyOffset * HFUnitsPerBodyRadius;
    OutCurlUVW += bodyOffset * CurlUnitsPerBodyRadius;
}
"""
text = replace_exact(text, old_helper, new_helper)

text = replace_exact(
    text,
    """    float StormLifecycleControl,
    float RotationSign,
    float AnvilStrength,
    float AnvilTypeBias,
    float AnvilCeilingFade01,
    float2 AnvilWindDirectionXY,
    float AnvilWindStretch,
    float AnvilWindSkew,
    float AnvilWarpStart01,
    float AnvilWarpEnd01,
    float DensityGamma,
""",
    """    float StormLifecycleControl,
    float RotationSign,
    float OuterBrimRadiusScale,
    float ShapeTwistRadians,
    float SpiralTwistPower,
    float SpiralFullStrengthRadius01,
    float SpiralZeroStrengthRadius01,
    float AnvilStrength,
    float AnvilTypeBias,
    float AnvilCeilingFade01,
    float DensityGamma,
""",
)

old_profile = """    // Wispy selection is owned exclusively by the authored BaseBottomType.
    // ShapeRT2.G (SpiralTendrilMask) is intentionally not part of density.
    float bottomProfile = StormEvaluateBottomTypeProfile(
        BottomProfileTex,
        BottomProfileSampler,
        h,
        BaseBottomType);
    float topProfile = StormEvaluateCloudTypeProfile(
        TopProfileTex,
        TopProfileSampler,
        h,
        TopType);
    float bodyProfile = StormEvaluateVerticalProfile(
        bottomProfile,
        topProfile);

    // The anvil is a complete Bottom*Top profile at flipped local height.
    // It deliberately uses the original BaseBottomType.
    float anvilType = saturate(TopType + AnvilTypeBias);
    float flippedAnvilProfile = StormEvaluateAnvilVerticalProfile(
        BottomProfileTex,
        BottomProfileSampler,
        TopProfileTex,
        TopProfileSampler,
        h,
        BaseBottomType,
        anvilType,
        AnvilCeilingFade01);
    float anvilWeight = saturate(AnvilStrength)
        * saturate(domain.SuperstormCoverage);
    float verticalProfile = StormEvaluateAnvilUnionProfile(
        bodyProfile,
        flippedAnvilProfile,
        anvilWeight);
"""

new_profile = """    // Wispy selection is owned exclusively by the authored BaseBottomType.
    // ShapeRT2.G (TwistedModelingNoise) is already composed into RT1 Coverage
    // and TopType by Shape compute and is never sampled by material density.
    float normalBottom = StormEvaluateBottomTypeProfile(
        BottomProfileTex,
        BottomProfileSampler,
        h,
        BaseBottomType);
    float normalTop = StormEvaluateCloudTypeProfile(
        TopProfileTex,
        TopProfileSampler,
        h,
        TopType);
    float normalProfile = StormEvaluateVerticalProfile(
        normalBottom,
        normalTop);

    // The anvil is a complete Bottom*Top profile at flipped local height.
    // It deliberately uses the original BaseBottomType.
    float anvilTopType = saturate(TopType + AnvilTypeBias);
    float anvilProfile = StormEvaluateAnvilVerticalProfile(
        BottomProfileTex,
        BottomProfileSampler,
        TopProfileTex,
        TopProfileSampler,
        h,
        BaseBottomType,
        anvilTopType,
        AnvilCeilingFade01);
    float anvilWeight = saturate(AnvilStrength)
        * saturate(domain.SuperstormCoverage);
    float combinedVerticalProfile = lerp(
        normalProfile,
        max(normalProfile, anvilProfile),
        anvilWeight);
    float anvilAddedProfile = saturate(
        anvilProfile - normalProfile);
    float anvilRotationWeight = saturate(
        anvilWeight * anvilAddedProfile);
"""
text = replace_exact(text, old_profile, new_profile)

old_call_12 = """        StormApplyUpperAnvilNoiseWarp(
            domain,
            h,
            anvilWeight,
            AnvilWindDirectionXY,
            AnvilWindStretch,
            AnvilWindSkew,
            AnvilWarpStart01,
            AnvilWarpEnd01,
"""
new_call_12 = """        StormApplyAnvilNoiseRotation(
            domain,
            anvilRotationWeight,
            OuterBrimRadiusScale,
            RotationSign,
            ShapeTwistRadians,
            SpiralTwistPower,
            SpiralFullStrengthRadius01,
            SpiralZeroStrengthRadius01,
"""
text = replace_exact(text, old_call_12, new_call_12, expected=2)

old_call_8 = """    StormApplyUpperAnvilNoiseWarp(
        domain,
        h,
        anvilWeight,
        AnvilWindDirectionXY,
        AnvilWindStretch,
        AnvilWindSkew,
        AnvilWarpStart01,
        AnvilWarpEnd01,
"""
new_call_8 = """    StormApplyAnvilNoiseRotation(
        domain,
        anvilRotationWeight,
        OuterBrimRadiusScale,
        RotationSign,
        ShapeTwistRadians,
        SpiralTwistPower,
        SpiralFullStrengthRadius01,
        SpiralZeroStrengthRadius01,
"""
text = replace_exact(text, old_call_8, new_call_8)

text = replace_exact(
    text,
    "            verticalProfile,",
    "            combinedVerticalProfile,",
    expected=2,
)
text = replace_exact(
    text,
    "        verticalProfile,",
    "        combinedVerticalProfile,",
)

for forbidden in (
    "StormApplyUpperAnvilNoiseWarp",
    "AnvilWindDirectionXY",
    "AnvilWindStretch",
    "AnvilWindSkew",
    "AnvilWarpStart01",
    "AnvilWarpEnd01",
    "verticalProfile",
    "bodyProfile",
    "flippedAnvilProfile",
):
    if forbidden in text:
        raise RuntimeError(f"obsolete active Anvil symbol remains: {forbidden}")

TARGET.write_text(text, encoding="utf-8", newline="\n")
print("CODEX_V10_ANVIL_ADAPTER|SUCCESS")
