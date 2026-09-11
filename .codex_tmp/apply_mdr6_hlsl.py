from pathlib import Path


path = Path(
    "Plugins/SavageSuperStorm/Shaders/Public/"
    "StormDensityLatestMDRAdapter.ush"
)
data = path.read_bytes()


def replace_once(old: bytes, new: bytes, label: str) -> None:
    global data
    count = data.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one match, found {count}")
    data = data.replace(old, new, 1)


replace_once(
    b"    float DensityGamma,\n"
    b"    float HFStrength,\n"
    b"    float2 MDRBodyCoord,",
    b"    float DensityGamma,\n"
    b"    float HFStrength,\n"
    b"    float AnvilDepth01,\n"
    b"    float2 MDRBodyCoord,",
    "canonical AnvilDepth01 parameter",
)

replace_once(
    b"    // This compatibility material has no serialized AnvilDepth01 pin, so one\n"
    b"    // preserves its historical anchor-to-bottom window.\n"
    b"    float anvilCoord = StormEvaluateAnvilCoord(\n"
    b"        CloudLayerHeight01,\n"
    b"        AnvilAnchorHeight01,\n"
    b"        1.0);\n"
    b"    float anvilHeightMask = StormEvaluateAnvilHeightMask(\n"
    b"        CloudLayerHeight01,\n"
    b"        AnvilAnchorHeight01,\n"
    b"        1.0);",
    b"    // Keep the current MF_Erosion Anvil depth window when that ABI supplies it.\n"
    b"    // Legacy callers pass one to preserve their historical anchor-to-bottom window.\n"
    b"    float anvilCoord = StormEvaluateAnvilCoord(\n"
    b"        CloudLayerHeight01,\n"
    b"        AnvilAnchorHeight01,\n"
    b"        AnvilDepth01);\n"
    b"    float anvilHeightMask = StormEvaluateAnvilHeightMask(\n"
    b"        CloudLayerHeight01,\n"
    b"        AnvilAnchorHeight01,\n"
    b"        AnvilDepth01);",
    "canonical Anvil depth forwarding",
)

replace_once(
    b"        DensityGamma,\n"
    b"        HFStrength,\n"
    b"        MDRBodyCoord,",
    b"        DensityGamma,\n"
    b"        HFStrength,\n"
    b"        1.0,\n"
    b"        MDRBodyCoord,",
    "legacy Anvil depth default",
)

new_overload = b"""

// Current MF_Erosion ABI plus six-ring MDR motion. The existing density,
// phased Flow, Anvil profile, height twist, and Anvil depth inputs are retained.
float StormEvaluateMaterialDensityMDRLatest(
    Texture2D BottomProfileTex,
    SamplerState BottomProfileSampler,
    Texture2D TopProfileTex,
    SamplerState TopProfileSampler,
    Texture2D AnvilProfileTex,
    SamplerState AnvilProfileSampler,
    Texture3D LFNoiseTex,
    SamplerState LFNoiseSampler,
    Texture3D HFNoiseTex,
    SamplerState HFNoiseSampler,
    Texture2D CurlNoiseTex,
    SamplerState CurlNoiseSampler,
    float Coverage,
    float HLocal,
    float HeightInside,
    float CloudLayerHeight01,
    float BaseBottomType,
    float TopType,
    float StormStencil,
    float3 WorldPosition,
    float3 StormCenterRadius,
    float3 LFUVW,
    float3 HFUVW,
    float3 FlowOffsetA,
    float3 FlowOffsetB,
    float FlowWeightA,
    float2 CurlUVW,
    float LFUnitsPerBodyRadius,
    float HFUnitsPerBodyRadius,
    float CurlUnitsPerBodyRadius,
    float CurlTiling,
    float CurlDisplacementUVW,
    float StormLifecycleControl,
    float RotationSign,
    float OuterBrimRadiusScale,
    float AnvilCoverageField,
    float ShapeTwistRadians,
    float SpiralTwistPower,
    float LegacySpiralInnerRadius01,
    float LegacySpiralOuterRadius01,
    float AnvilStrength,
    float AnvilAnchorHeight01,
    float AnvilDepth01,
    float DensityGamma,
    float HFStrength,
    float2 MDRBodyCoord,
    float4 MDRBoundaries0,
    float4 MDRBoundaries1,
    float4 MDRSpeeds0,
    float4 MDRSpeeds1,
    float4 MDRPhases0,
    float4 MDRPhases1,
    float4 MDRSkews0,
    float4 MDRSkews1,
    float4 MDRControl0,
    float4 MDRControl1,
    out float OutCoarseDensity)
{
    return StormEvaluateMaterialDensityMDRLatest(
        BottomProfileTex,
        BottomProfileSampler,
        TopProfileTex,
        TopProfileSampler,
        AnvilProfileTex,
        AnvilProfileSampler,
        LFNoiseTex,
        LFNoiseSampler,
        HFNoiseTex,
        HFNoiseSampler,
        CurlNoiseTex,
        CurlNoiseSampler,
        Coverage,
        HLocal,
        HeightInside,
        CloudLayerHeight01,
        BaseBottomType,
        TopType,
        StormStencil,
        WorldPosition,
        StormCenterRadius,
        LFUVW,
        HFUVW,
        FlowOffsetA,
        FlowOffsetB,
        FlowWeightA,
        CurlUVW,
        LFUnitsPerBodyRadius,
        HFUnitsPerBodyRadius,
        CurlUnitsPerBodyRadius,
        CurlTiling,
        CurlDisplacementUVW,
        StormLifecycleControl,
        RotationSign,
        OuterBrimRadiusScale,
        AnvilCoverageField,
        ShapeTwistRadians,
        SpiralTwistPower,
        AnvilStrength,
        AnvilAnchorHeight01,
        0.35,
        1.0,
        DensityGamma,
        HFStrength,
        AnvilDepth01,
        MDRBodyCoord,
        MDRBoundaries0,
        MDRBoundaries1,
        MDRSpeeds0,
        MDRSpeeds1,
        MDRPhases0,
        MDRPhases1,
        MDRSkews0,
        MDRSkews1,
        MDRControl0,
        MDRControl1,
        1u,
        OutCoarseDensity
    );
}
"""

tail = b"        OutCoarseDensity\n    );\n}\n"
replace_once(tail, tail + new_overload, "current MF_Erosion overload")

path.write_bytes(data)
print(f"CODEX_MDR6_HLSL|SUCCESS|bytes={len(data)}")
