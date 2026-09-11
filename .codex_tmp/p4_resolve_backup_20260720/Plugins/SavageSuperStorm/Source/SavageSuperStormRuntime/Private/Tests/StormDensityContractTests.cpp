#include "AssetCompilingManager.h"
#include "Field/StormDensityContract.h"
#include "Materials/Material.h"
#include "MaterialShared.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace SavageSuperStorm
{
using namespace DensityContract;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FStormDensityHeightContractTest,
    "SavageSuperStorm.Density.Height.CloudLayerEnvelope",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStormDensityHeightContractTest::RunTest(const FString& Parameters)
{
    const FHeightSample Inside = DecodeLayerHeight(0.52f, 0.12f, 0.96f);
    TestEqual(TEXT("ShapeRT2.R is absolute in cloud-layer space"),
        Inside.MinLayerHeight01, 0.12f);
    TestEqual(TEXT("ShapeRT2.B is absolute in cloud-layer space"),
        Inside.MaxLayerHeight01, 0.96f);
    TestEqual(TEXT("A sample inside the envelope is accepted"),
        Inside.HeightInside, 1.0f);
    TestTrue(TEXT("HLocal is normalized inside the envelope"),
        FMath::IsNearlyEqual(Inside.HLocal, 0.47619048f, 1.0e-5f));

    const FHeightSample AtLowerBound = DecodeLayerHeight(0.12f, 0.12f, 0.96f);
    const FHeightSample AtUpperBound = DecodeLayerHeight(0.96f, 0.12f, 0.96f);
    TestEqual(TEXT("The lower boundary is inclusive"),
        AtLowerBound.HeightInside, 1.0f);
    TestEqual(TEXT("The lower boundary maps to local zero"),
        AtLowerBound.HLocal, 0.0f);
    TestEqual(TEXT("The upper boundary is inclusive"),
        AtUpperBound.HeightInside, 1.0f);
    TestEqual(TEXT("The upper boundary maps to local one"),
        AtUpperBound.HLocal, 1.0f);

    const FHeightSample Below = DecodeLayerHeight(0.08f, 0.12f, 0.96f);
    TestEqual(TEXT("A sample below the envelope is rejected"),
        Below.HeightInside, 0.0f);
    TestEqual(TEXT("Rejected samples keep bounded HLocal"),
        Below.HLocal, 0.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
>>>> ORIGINAL //depot/VolumetricSuperStorm/Plugins/SavageSuperStorm/Source/SavageSuperStormRuntime/Private/Tests/StormDensityContractTests.cpp#4
    FStormDensityFormationContractTest,
    "SavageSuperStorm.Density.Coverage.Formation",
==== THEIRS //depot/VolumetricSuperStorm/Plugins/SavageSuperStorm/Source/SavageSuperStormRuntime/Private/Tests/StormDensityContractTests.cpp#5
    FStormDensityFormationContractTest,
    "SavageSuperStorm.Density.Coverage.Shape",
==== YOURS //CptTangerine_Workspace/VolumetricSuperStorm/Plugins/SavageSuperStorm/Source/SavageSuperStormRuntime/Private/Tests/StormDensityContractTests.cpp
    FStormDensityCoverageAndTypeContractTest,
    "SavageSuperStorm.Density.CoverageAndCloudType.Sombrero",
<<<<
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStormDensityCoverageAndTypeContractTest::RunTest(
    const FString& Parameters)
{
>>>> ORIGINAL //depot/VolumetricSuperStorm/Plugins/SavageSuperStorm/Source/SavageSuperStormRuntime/Private/Tests/StormDensityContractTests.cpp#4
    TestEqual(TEXT("Full formation preserves raw coverage"),
        ApplyFormationToCoverage(0.72f, 1.0f), 0.72f);
    TestEqual(TEXT("Zero formation rejects all coverage"),
        ApplyFormationToCoverage(0.72f, 0.0f), 0.0f);
    TestTrue(TEXT("Partial formation remaps threshold headroom once"),
        FMath::IsNearlyEqual(ApplyFormationToCoverage(0.80f, 0.50f), 0.60f, 1.0e-5f));

==== THEIRS //depot/VolumetricSuperStorm/Plugins/SavageSuperStorm/Source/SavageSuperStormRuntime/Private/Tests/StormDensityContractTests.cpp#5
==== YOURS //CptTangerine_Workspace/VolumetricSuperStorm/Plugins/SavageSuperStorm/Source/SavageSuperStormRuntime/Private/Tests/StormDensityContractTests.cpp
    TestEqual(TEXT("Full formation preserves raw Coverage"),
        ApplyFormationToCoverage(0.72f, 1.0f), 0.72f);
    TestEqual(TEXT("Zero formation rejects Coverage"),
        ApplyFormationToCoverage(0.72f, 0.0f), 0.0f);
    TestTrue(TEXT("Partial formation remaps Coverage once"),
        FMath::IsNearlyEqual(
            ApplyFormationToCoverage(0.80f, 0.50f),
            0.60f,
            1.0e-5f));

<<<<
    TestEqual(TEXT("Coverage outer brim is 0.3"),
        EvaluateRadialCoverage(1.0f), 0.30f);
    TestEqual(TEXT("Coverage core is 0.8"),
        EvaluateRadialCoverage(0.0f), 0.80f);
    TestTrue(TEXT("Coverage rises continuously toward the core"),
        EvaluateRadialCoverage(0.75f)
            < EvaluateRadialCoverage(0.50f)
        && EvaluateRadialCoverage(0.50f)
            < EvaluateRadialCoverage(0.25f));

    TestEqual(TEXT("Outer CloudType is stratus"),
        EvaluateRadialTopType(0.835f), 0.0f);
    TestTrue(TEXT("Lifted stratus remains below cumulus"),
        FMath::IsNearlyEqual(
            EvaluateRadialTopType(0.78f),
            0.30f,
            1.0e-6f));
    TestTrue(TEXT("Cumulus shelf reaches CloudType 0.5"),
        FMath::IsNearlyEqual(
            EvaluateRadialTopType(0.20f),
            0.50f,
            1.0e-6f));
    TestEqual(TEXT("Core CloudType is cumulonimbus"),
        EvaluateRadialTopType(0.0f), 1.0f);

    TestEqual(TEXT("Outer BottomType samples the outer Bottom LUT coordinate"),
        EvaluateRadialBottomType(1.0f), 0.0f);
    TestEqual(TEXT("Core BottomType samples the outer Bottom LUT coordinate"),
        EvaluateRadialBottomType(0.0f), 0.0f);
    TestEqual(TEXT("BottomType remains zero across the storm"),
        EvaluateRadialBottomType(0.50f), 0.0f);

    const FVector2f StratusTopEdgeUV = ClampProfileUV(
        0.0f, 1.0f, 256, 256);
    TestTrue(TEXT("CloudType zero samples the first texel center"),
        FMath::IsNearlyEqual(
            StratusTopEdgeUV.X,
            0.5f / 256.0f,
            1.0e-7f));
    TestTrue(TEXT("Profile height one samples the final texel center"),
        FMath::IsNearlyEqual(
            StratusTopEdgeUV.Y,
            1.0f - 0.5f / 256.0f,
            1.0e-7f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FStormDensityScaleContractTest,
    "SavageSuperStorm.Density.Coordinates.ProportionalScale",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStormDensityScaleContractTest::RunTest(const FString& Parameters)
{
    const float ReferenceRadiusKm = 10.0f;
    const FVector3f ReferencePointKm(3.5f, -2.0f, 7.2f);
    const FVector3f AdvectionKmPerSec(0.012f, -0.004f, 0.001f);
    const FNoiseCoordinates Radius10 = MakeNoiseCoordinates(
        ReferencePointKm,
        ProportionalScale(1000000.0f, ReferenceRadiusKm),
        AdvectionKmPerSec,
        12.0f,
        1.0f,
        1.35f,
        0.8f);
    const FNoiseCoordinates Radius40 = MakeNoiseCoordinates(
        ReferencePointKm * 4.0f,
        ProportionalScale(4000000.0f, ReferenceRadiusKm),
        AdvectionKmPerSec,
        12.0f,
        1.0f,
        1.35f,
        0.8f);

    TestTrue(TEXT("Radius-scaled samples share LF coordinates"),
        Radius10.LFReferencePositionKm.Equals(
            Radius40.LFReferencePositionKm, 1.0e-6f));
    TestTrue(TEXT("Radius-scaled samples share HF coordinates"),
        Radius10.HFReferencePositionKm.Equals(
            Radius40.HFReferencePositionKm, 1.0e-6f));
    TestTrue(TEXT("Radius-scaled samples share Curl coordinates"),
        Radius10.CurlReferencePositionKm.Equals(
            Radius40.CurlReferencePositionKm, 1.0e-6f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FStormDensityNubisContractTest,
    "SavageSuperStorm.Density.Noise.NubisBasic",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStormDensityNubisContractTest::RunTest(const FString& Parameters)
{
    const FVector4f LFRaw(0.20f, 0.40f, 0.60f, 0.80f);
    const float ExpectedLFWorley = 0.50f;
    const float ExpectedLFShape = (0.20f + 0.50f) / 1.50f;
    TestTrue(TEXT("LF Worley FBM uses the canonical GBA weights"),
        FMath::IsNearlyEqual(ExpectedLFWorley, 0.50f, 1.0e-6f));
    TestTrue(TEXT("LFShape remaps R from -(1-Worley) to one"),
        FMath::IsNearlyEqual(
            DecodeNubisLFShape(LFRaw),
            ExpectedLFShape,
            1.0e-6f));

    const FVector3f HFRaw(0.50f, 0.50f, 0.50f);
    TestTrue(TEXT("HF FBM uses raw Worley with canonical RGB weights"),
        FMath::IsNearlyEqual(
            DecodeNubisHFFBM(HFRaw),
            0.50f,
            1.0e-6f));

    TestTrue(TEXT("Coverage remaps against Coverage then multiplies it"),
        FMath::IsNearlyEqual(
            ApplyCoverage(0.80f, 0.50f),
            0.30f,
            1.0e-6f));
    TestEqual(TEXT("Zero Coverage is empty"),
        ApplyCoverage(1.0f, 0.0f), 0.0f);

    const float CoveredBase = EvaluateCoveredBaseCloud(
        0.80f,
        0.50f,
        0.80f);
    TestTrue(TEXT("LFShape multiplies Profile before Coverage"),
        FMath::IsNearlyEqual(CoveredBase, 0.20f, 1.0e-6f));
    TestTrue(TEXT("Coarse density ends before HF erosion"),
        FMath::IsNearlyEqual(
            ApplyDensityMasks(CoveredBase, 0.80f, 0.50f),
            0.08f,
            1.0e-6f));

    TestEqual(TEXT("Zero Bottom lookup rejects the vertical profile"),
        EvaluateBottomTopVerticalProfile(0.0f, 1.0f), 0.0f);
    TestTrue(TEXT("Bottom and Top lookup results are multiplied"),
        FMath::IsNearlyEqual(
            EvaluateBottomTopVerticalProfile(0.20f, 0.80f),
            0.16f,
            1.0e-6f));
    TestEqual(TEXT("Unit lookup results preserve the full vertical profile"),
        EvaluateBottomTopVerticalProfile(1.0f, 1.0f), 1.0f);

    TestEqual(TEXT("HF morphology uses raw HF at the cloud base"),
        EvaluateHeightDependentHFModifier(0.25f, 0.0f), 0.25f);
    TestTrue(TEXT("HF morphology blends over the bottom ten percent"),
        FMath::IsNearlyEqual(
            EvaluateHeightDependentHFModifier(0.25f, 0.05f),
            0.50f,
            1.0e-6f));
    TestEqual(TEXT("HF morphology is inverted above the bottom ten percent"),
        EvaluateHeightDependentHFModifier(0.25f, 0.10f), 0.75f);

    const float GenericLowHF = ApplyHFErosion(
        0.60f,
        EvaluateHeightDependentHFModifier(0.10f, 0.05f),
        0.20f);
    const float GenericHighHF = ApplyHFErosion(
        0.60f,
        EvaluateHeightDependentHFModifier(0.90f, 0.05f),
        0.20f);
    TestTrue(TEXT("Canonical midpoint alone flattens HF contrast"),
        FMath::IsNearlyEqual(GenericLowHF, GenericHighHF, 1.0e-6f));
    TestEqual(TEXT("2015 HF erosion starts at strength 0.2"),
        DefaultNubisHFStrength, 0.20f);
    TestTrue(TEXT("HF erosion uses modifier times strength as the threshold"),
        FMath::IsNearlyEqual(
            ApplyHFErosion(0.80f, 0.50f),
            0.77777778f,
            1.0e-6f));
    TestTrue(TEXT("Fine density applies masks after HF erosion"),
        FMath::IsNearlyEqual(
            ApplyDensityMasks(
                ApplyHFErosion(0.80f, 0.50f),
                0.50f,
                0.25f),
            0.09722222f,
            1.0e-6f));

    bool bFiniteAndBounded = true;
    bool bFineNeverExceedsCoarse = true;
    for (int32 DensityStep = 0; DensityStep <= 20; ++DensityStep)
    {
        for (int32 HFStep = 0; HFStep <= 20; ++HFStep)
        {
            const float Density = static_cast<float>(DensityStep) / 20.0f;
            const float HF = static_cast<float>(HFStep) / 20.0f;
            const float Fine = ApplyHFErosion(Density, HF);
            bFiniteAndBounded = bFiniteAndBounded
                && FMath::IsFinite(Fine)
                && Fine >= 0.0f
                && Fine <= 1.0f;
            bFineNeverExceedsCoarse = bFineNeverExceedsCoarse
                && Fine <= Density + 1.0e-6f;
        }
    }
    TestTrue(TEXT("All HF erosion endpoints are finite and bounded"),
        bFiniteAndBounded);
    TestTrue(TEXT("Fine density never exceeds its coarse carrier"),
        bFineNeverExceedsCoarse);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FStormDensityLightingFineSupportContractTest,
    "SavageSuperStorm.Density.Lighting.FineDensitySupport",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStormDensityLightingFineSupportContractTest::RunTest(
    const FString& Parameters)
{
    TestEqual(TEXT("Empty fine density has no lighting support"),
        EvaluateUndersideLightingDetailSupport(0.0f, 1.0f), 0.0f);
    TestTrue(TEXT("The support ramp is half strength at ratio 0.30"),
        FMath::IsNearlyEqual(
            EvaluateUndersideLightingDetailSupport(0.30f, 1.0f),
            0.50f,
            1.0e-6f));
    TestEqual(TEXT("The support ramp completes at ratio 0.45"),
        EvaluateUndersideLightingDetailSupport(0.45f, 1.0f), 1.0f);
    TestEqual(TEXT("Matching fine and coarse density preserves lighting"),
        EvaluateUndersideLightingDetailSupport(0.25f, 0.25f), 1.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FStormDensityMaterialCompileContractTest,
    "SavageSuperStorm.Density.Material.MSSSCompile",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStormDensityMaterialCompileContractTest::RunTest(
    const FString& Parameters)
{
    UMaterial* Material = LoadObject<UMaterial>(
        nullptr,
        TEXT("/SavageSuperStorm/Materials/M_SSS.M_SSS"));
    if (!TestNotNull(TEXT("M_SSS loads"), Material))
    {
        return false;
    }

    Material->ForceRecompileForRendering();
    FAssetCompilingManager::Get().FinishAllCompilation();

    const FMaterialResource* Resource = Material->GetMaterialResource(
        GMaxRHIShaderPlatform);
    if (!TestNotNull(TEXT("M_SSS has a material resource"), Resource))
    {
        return false;
    }

    const TArray<FString>& CompileErrors = Resource->GetCompileErrors();
    for (const FString& CompileError : CompileErrors)
    {
        AddError(FString::Printf(
            TEXT("M_SSS shader compile error: %s"),
            *CompileError));
    }
    TestTrue(TEXT("M_SSS Custom HLSL compiles without errors"),
        CompileErrors.IsEmpty());
    TestNotNull(TEXT("M_SSS has a completed game-thread shader map"),
        Resource->GetGameThreadShaderMap());
    return CompileErrors.IsEmpty();
}

}

#endif
