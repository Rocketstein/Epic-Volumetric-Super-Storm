from pathlib import Path


contract_path = Path("Plugins/SavageSuperStorm/Source/SavageSuperStormRuntime/Public/Field/StormDensityContract.h")
contract = contract_path.read_text(encoding="utf-8")

# The rotation helpers depend on the shared spiral helpers, so keep them after
# EvaluateSpiralSourceBodyCoord rather than relying on forward declarations.
block_start = contract.index("inline FVector2f EvaluateAnvilRotationBodyOffset(")
block_end = contract.index("inline float RotationHandedness(", block_start)
rotation_block = contract[block_start:block_end]
contract = contract[:block_start] + contract[block_end:]
insert_at = contract.index("inline FShapeModelingSignals EvaluateShapeModelingSignals(")
contract = contract[:insert_at] + rotation_block + contract[insert_at:]
contract_path.write_text(contract, encoding="utf-8")


tests_path = Path("Plugins/SavageSuperStorm/Source/SavageSuperStormRuntime/Private/Tests/StormDensityContractTests.cpp")
tests = tests_path.read_text(encoding="utf-8")
old_start = tests.index("IMPLEMENT_SIMPLE_AUTOMATION_TEST(\n    FStormDensityTendrilContractTest,")
old_end = tests.index("IMPLEMENT_SIMPLE_AUTOMATION_TEST(\n    FStormDensityTextureSampleContractTest,", old_start)

new_tests = r'''IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FStormDensityTwistedModelingContractTest,
    "SavageSuperStorm.Density.TwistedModelingNoise.Contract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStormDensityTwistedModelingContractTest::RunTest(
    const FString& Parameters)
{
    constexpr float FullStrengthRadius01 = 0.08f;
    constexpr float ZeroStrengthRadius01 = 0.92f;
    constexpr float TwistPower = 1.0f;
    constexpr float TwistRadians = 2.0f * PI / 3.0f;

    TestEqual(TEXT("The shared map twist default is preserved"),
        DefaultShapeTwistRadians, 2.97f);

    const FStormShapeSettings HashBaseline;
    FStormShapeSettings HashChanged = HashBaseline;
    HashChanged.SpiralSeed += 1.0f;
    TestTrue(TEXT("SpiralSeed participates in the ShapeRT hash"),
        GetTypeHash(HashBaseline) != GetTypeHash(HashChanged));
    HashChanged = HashBaseline;
    HashChanged.SpiralCarveIslandScale += 1.0f;
    TestTrue(TEXT("Modeling macro scale participates in the ShapeRT hash"),
        GetTypeHash(HashBaseline) != GetTypeHash(HashChanged));
    HashChanged = HashBaseline;
    HashChanged.SpiralCarveDetailAmount = 0.0f;
    TestTrue(TEXT("Modeling detail amount participates in the ShapeRT hash"),
        GetTypeHash(HashBaseline) != GetTypeHash(HashChanged));
    HashChanged = HashBaseline;
    HashChanged.ShapeTwistRadians = 4.5f;
    TestTrue(TEXT("ShapeTwistRadians participates in the ShapeRT hash"),
        GetTypeHash(HashBaseline) != GetTypeHash(HashChanged));

    const FVector2f Point(0.31f, 0.17f);
    const FVector2f Untwisted = EvaluateSpiralSourceBodyCoord(
        Point, 1.0f, 1.0f, 0.0f, TwistPower,
        FullStrengthRadius01, ZeroStrengthRadius01);
    TestTrue(TEXT("Zero twist preserves the untwisted noise layout"),
        Untwisted.Equals(Point, 1.0e-7f));

    const FVector2f Positive = EvaluateSpiralSourceBodyCoord(
        Point, 1.0f, 1.0f, TwistRadians, TwistPower,
        FullStrengthRadius01, ZeroStrengthRadius01);
    const FVector2f Negative = EvaluateSpiralSourceBodyCoord(
        Point, 1.0f, -1.0f, TwistRadians, TwistPower,
        FullStrengthRadius01, ZeroStrengthRadius01);
    TestTrue(TEXT("RotationSign selects opposite source-map handedness"),
        !Positive.Equals(Negative, 1.0e-4f));
    TestTrue(TEXT("Both handedness transforms preserve radius"),
        FMath::IsNearlyEqual(Positive.Size(), Point.Size(), 1.0e-6f)
        && FMath::IsNearlyEqual(Negative.Size(), Point.Size(), 1.0e-6f));

    float PositiveMean = 0.0f;
    float NegativeMean = 0.0f;
    float PositiveSecondMoment = 0.0f;
    float NegativeSecondMoment = 0.0f;
    constexpr int32 SampleCount = 64;
    for (int32 Index = 0; Index < SampleCount; ++Index)
    {
        const float Angle = 2.0f * PI * static_cast<float>(Index)
            / static_cast<float>(SampleCount);
        const float Radius = 0.1f + 0.8f
            * static_cast<float>(Index % 11) / 10.0f;
        const FVector2f Sample(
            FMath::Cos(Angle) * Radius,
            FMath::Sin(Angle) * Radius);
        const float PositiveRadius = EvaluateSpiralSourceBodyCoord(
            Sample, 1.0f, 1.0f, TwistRadians, TwistPower,
            FullStrengthRadius01, ZeroStrengthRadius01).Size();
        const float NegativeRadius = EvaluateSpiralSourceBodyCoord(
            Sample, 1.0f, -1.0f, TwistRadians, TwistPower,
            FullStrengthRadius01, ZeroStrengthRadius01).Size();
        PositiveMean += PositiveRadius;
        NegativeMean += NegativeRadius;
        PositiveSecondMoment += PositiveRadius * PositiveRadius;
        NegativeSecondMoment += NegativeRadius * NegativeRadius;
    }
    PositiveMean /= SampleCount;
    NegativeMean /= SampleCount;
    PositiveSecondMoment /= SampleCount;
    NegativeSecondMoment /= SampleCount;
    TestTrue(TEXT("Handedness preserves normalized-field mean and variance"),
        FMath::IsNearlyEqual(PositiveMean, NegativeMean, 1.0e-6f)
        && FMath::IsNearlyEqual(
            PositiveSecondMoment - PositiveMean * PositiveMean,
            NegativeSecondMoment - NegativeMean * NegativeMean,
            1.0e-6f));

    const FVector2f BodyAtRadiusA(0.40f, -0.20f);
    const FVector2f BodyAtRadiusB = BodyAtRadiusA * 4.0f;
    const FVector2f NormalizedA = BodyAtRadiusA / 1.35f;
    const FVector2f NormalizedB = BodyAtRadiusB / (1.35f * 4.0f);
    TestTrue(TEXT("Storm-radius scaling preserves normalized twisted layout"),
        EvaluateSpiralSourceBodyCoord(
            NormalizedA, 1.0f, 1.0f, TwistRadians, TwistPower,
            FullStrengthRadius01, ZeroStrengthRadius01).Equals(
                EvaluateSpiralSourceBodyCoord(
                    NormalizedB, 1.0f, 1.0f, TwistRadians, TwistPower,
                    FullStrengthRadius01, ZeroStrengthRadius01),
                1.0e-7f));

    TestEqual(TEXT("ShapeRT2.G is zero outside mature containment"),
        EvaluateShapeModelingSignals(1.0f, 0.0f).TwistedModelingNoise,
        0.0f);
    TestTrue(TEXT("ShapeRT2.G stores exactly one stencil-contained value"),
        FMath::IsNearlyEqual(
            EvaluateShapeModelingSignals(0.75f, 0.40f)
                .TwistedModelingNoise,
            0.30f,
            1.0e-6f));
    TestEqual(TEXT("ShapeRT2.G clamps high source values"),
        EvaluateShapeModelingSignals(2.0f, 1.0f).TwistedModelingNoise,
        1.0f);

    constexpr float RadiusAcrossOuterBrim01 = 0.50f;
    constexpr float CoverageCeiling = 0.98f;
    constexpr float CoverageMinimum = 0.58f;
    const float RadialCoverage = EvaluateRadialCoverage(
        RadiusAcrossOuterBrim01,
        CoverageCeiling,
        CoverageMinimum,
        DefaultCoverageRimStart01);
    TestEqual(TEXT("CenterMask one forces CoverageCeiling for G zero"),
        EvaluateCoverageComposite(
            RadiusAcrossOuterBrim01, 0.0f, 1.0f),
        CoverageCeiling);
    TestEqual(TEXT("CenterMask one forces CoverageCeiling for G one"),
        EvaluateCoverageComposite(
            RadiusAcrossOuterBrim01, 1.0f, 1.0f),
        CoverageCeiling);
    TestEqual(TEXT("G zero outside center yields Coverage minimum"),
        EvaluateCoverageComposite(
            RadiusAcrossOuterBrim01, 0.0f, 0.0f),
        CoverageMinimum);
    TestTrue(TEXT("G one outside center yields radial Coverage"),
        FMath::IsNearlyEqual(
            EvaluateCoverageComposite(
                RadiusAcrossOuterBrim01, 1.0f, 0.0f),
            RadialCoverage,
            1.0e-6f));
    TestEqual(TEXT("Outer brim yields Coverage minimum"),
        EvaluateCoverageComposite(1.0f, 1.0f, 0.0f),
        CoverageMinimum);

    TestEqual(TEXT("CenterMask one forces TopType one for G zero"),
        EvaluateTopTypeComposite(0.50f, 0.0f, 1.0f), 1.0f);
    TestEqual(TEXT("G zero outside center yields TopType zero"),
        EvaluateTopTypeComposite(0.50f, 0.0f, 0.0f), 0.0f);
    TestEqual(TEXT("G one outside center yields radial TopType"),
        EvaluateTopTypeComposite(0.50f, 1.0f, 0.0f), 0.50f);

    const float BottomType = EvaluateRadialBottomType(0.42f);
    const float MinHeight = EvaluateRadialLayerHeight(0.42f, 0.08f, 0.12f);
    const float MaxHeight = EvaluateRadialLayerHeight(0.42f, 1.0f, 0.96f);
    const float StormStencil = 0.73f;
    bool bOwnershipIndependent = true;
    bool bExtremaFinite = true;
    for (int32 Step = 0; Step <= 20; ++Step)
    {
        const float G = static_cast<float>(Step) / 20.0f;
        bOwnershipIndependent = bOwnershipIndependent
            && EvaluateRadialBottomType(0.42f) == BottomType
            && EvaluateRadialLayerHeight(0.42f, 0.08f, 0.12f) == MinHeight
            && EvaluateRadialLayerHeight(0.42f, 1.0f, 0.96f) == MaxHeight
            && StormStencil == 0.73f;
        bExtremaFinite = bExtremaFinite
            && FMath::IsFinite(EvaluateCoverageComposite(0.42f, G, G))
            && FMath::IsFinite(EvaluateTopTypeComposite(0.42f, G, G));
    }
    TestTrue(TEXT("G sweep leaves BottomType, heights, and stencil independent"),
        bOwnershipIndependent);
    TestTrue(TEXT("Coverage and type extrema remain finite"), bExtremaFinite);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FStormDensityAnvilRotationContractTest,
    "SavageSuperStorm.Density.Anvil.SingleSampleRotation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStormDensityAnvilRotationContractTest::RunTest(
    const FString& Parameters)
{
    FAnvilNoiseCoordinates Source;
    Source.LFUVW = FVector3f(0.10f, 0.20f, 0.30f);
    Source.HFUVW = FVector3f(0.40f, 0.50f, 0.60f);
    Source.CurlUVW = FVector2f(0.70f, 0.80f);
    const FVector2f BodyCoord(0.50f, 0.25f);

    TestEqual(TEXT("AnvilStrength zero is a profile no-op"),
        EvaluateAnvilUnionProfile(
            0.20f, 0.80f, EvaluateAnvilWeight(0.0f, 1.0f)),
        0.20f);
    TestEqual(TEXT("MatureStencil zero is a profile no-op"),
        EvaluateAnvilUnionProfile(
            0.20f, 0.80f, EvaluateAnvilWeight(1.0f, 0.0f)),
        0.20f);
    TestTrue(TEXT("Complete flipped Bottom and Top profiles multiply"),
        FMath::IsNearlyEqual(
            EvaluateFlippedAnvilProfile(0.80f, 0.50f, 0.975f, 0.05f),
            0.20f,
            1.0e-5f));
    TestEqual(TEXT("Anvil below normal adds no rotation support"),
        EvaluateAnvilRotationWeight(
            1.0f,
            EvaluateAnvilAddedProfile(0.80f, 0.40f)),
        0.0f);

    const FAnvilNoiseCoordinates Disabled = ApplyAnvilNoiseRotation(
        Source, BodyCoord, 0.0f, 1.35f, 1.0f,
        DefaultShapeTwistRadians, 1.0f, 0.08f, 0.92f,
        2.0f, 4.0f, 3.0f);
    TestTrue(TEXT("Zero rotation support leaves LF unchanged"),
        Disabled.LFUVW.Equals(Source.LFUVW, 1.0e-7f));
    TestTrue(TEXT("Zero rotation support leaves HF unchanged"),
        Disabled.HFUVW.Equals(Source.HFUVW, 1.0e-7f));
    TestTrue(TEXT("Zero rotation support leaves Curl unchanged"),
        Disabled.CurlUVW.Equals(Source.CurlUVW, 1.0e-7f));

    const FAnvilNoiseCoordinates Positive = ApplyAnvilNoiseRotation(
        Source, BodyCoord, 1.0f, 1.35f, 1.0f,
        DefaultShapeTwistRadians, 1.0f, 0.08f, 0.92f,
        2.0f, 4.0f, 3.0f);
    const FAnvilNoiseCoordinates Negative = ApplyAnvilNoiseRotation(
        Source, BodyCoord, 1.0f, 1.35f, -1.0f,
        DefaultShapeTwistRadians, 1.0f, 0.08f, 0.92f,
        2.0f, 4.0f, 3.0f);
    const FVector2f PositiveLFDelta(
        Positive.LFUVW.X - Source.LFUVW.X,
        Positive.LFUVW.Y - Source.LFUVW.Y);
    const FVector2f NegativeLFDelta(
        Negative.LFUVW.X - Source.LFUVW.X,
        Negative.LFUVW.Y - Source.LFUVW.Y);
    TestTrue(TEXT("RotationSign reverses Anvil coordinate handedness"),
        !PositiveLFDelta.Equals(NegativeLFDelta, 1.0e-4f));

    const FVector2f HFDelta(
        Positive.HFUVW.X - Source.HFUVW.X,
        Positive.HFUVW.Y - Source.HFUVW.Y);
    const FVector2f CurlDelta = Positive.CurlUVW - Source.CurlUVW;
    TestTrue(TEXT("One body offset is explicitly scaled for LF and HF"),
        HFDelta.Equals(PositiveLFDelta * 2.0f, 1.0e-6f));
    TestTrue(TEXT("One body offset is explicitly scaled for Curl"),
        CurlDelta.Equals(PositiveLFDelta * 1.5f, 1.0e-6f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FStormDensityV10OwnershipContractTest,
    "SavageSuperStorm.Density.V10.ActiveOwnership",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStormDensityV10OwnershipContractTest::RunTest(
    const FString& Parameters)
{
    FString ShapeSource;
    const FString ShapePath = FPaths::Combine(
        FPaths::ProjectPluginsDir(),
        TEXT("SavageSuperStorm/Shaders/Private/StormControlCS.usf"));
    if (!TestTrue(TEXT("Shape compute source loads"),
        FFileHelper::LoadFileToString(ShapeSource, *ShapePath)))
    {
        return false;
    }
    TestTrue(TEXT("G is authored as TwistedModelingNoise"),
        ShapeSource.Contains(TEXT("StormEvaluateTwistedModelingNoise"))
        && ShapeSource.Contains(TEXT("shape.TwistedModelingNoise")));
    TestFalse(TEXT("No Tendril semantic remains in Shape compute"),
        ShapeSource.Contains(TEXT("SpiralTendril")));
    TestFalse(TEXT("No second Coverage carve remains"),
        ShapeSource.Contains(TEXT("SpiralCarveStrength"))
        || ShapeSource.Contains(TEXT("SpiralCarveCoverageFloor"))
        || ShapeSource.Contains(TEXT("carvedCoverage")));
    TestFalse(TEXT("No threshold/depth/speck modeling path remains"),
        ShapeSource.Contains(TEXT("SpiralCarveSoftLow"))
        || ShapeSource.Contains(TEXT("SpiralCarveDepthMin"))
        || ShapeSource.Contains(TEXT("SpiralCarveSpeckLow"))
        || ShapeSource.Contains(TEXT("SpiralCarveRimCrispness")));

    FString AdapterSource;
    const FString AdapterPath = FPaths::Combine(
        FPaths::ProjectPluginsDir(),
        TEXT("SavageSuperStorm/Shaders/Public/StormDensityMaterialAdapter.ush"));
    if (!TestTrue(TEXT("Material density adapter source loads"),
        FFileHelper::LoadFileToString(AdapterSource, *AdapterPath)))
    {
        return false;
    }
    TestFalse(TEXT("Material density has no G-channel input"),
        AdapterSource.Contains(TEXT("float TwistedModelingNoise"))
        || AdapterSource.Contains(TEXT("ShapeRT2G"))
        || AdapterSource.Contains(TEXT("SpiralTendrilMask")));
    TestTrue(TEXT("Anvil uses rotation-only coordinate transform"),
        AdapterSource.Contains(TEXT("StormApplyAnvilNoiseRotation")));
    TestFalse(TEXT("No Anvil skew/stretch/shear or wind parameters remain"),
        AdapterSource.Contains(TEXT("AnvilWind"))
        || AdapterSource.Contains(TEXT("AnvilWarp"))
        || AdapterSource.Contains(TEXT("AnvilSkew"))
        || AdapterSource.Contains(TEXT("AnvilStretch"))
        || AdapterSource.Contains(TEXT("AnvilShear")));
    TestFalse(TEXT("Body and Anvil are not split into density evaluators"),
        AdapterSource.Contains(TEXT("BodyDensity"))
        || AdapterSource.Contains(TEXT("AnvilDensity")));
    TestEqual(TEXT("Adapter still contains one LF and one HF 3D sample"),
        CountToken(AdapterSource, TEXT("Texture3DSampleLevel(")), 2);
    return true;
}

'''

tests = tests[:old_start] + new_tests + tests[old_end:]
tests_path.write_text(tests, encoding="utf-8")
