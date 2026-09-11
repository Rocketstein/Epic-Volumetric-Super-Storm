from pathlib import Path


path = (
    Path(__file__).resolve().parents[1]
    / "Plugins/SavageSuperStorm/Source/SavageSuperStormRuntime/Private/Tests/StormDensityContractTests.cpp"
)
original = path.read_text(encoding="utf-8")
needle = '''    TestFalse(TEXT("No threshold/depth/speck modeling path remains"),
        ShapeSource.Contains(TEXT("SpiralCarveSoftLow"))
        || ShapeSource.Contains(TEXT("SpiralCarveDepthMin"))
        || ShapeSource.Contains(TEXT("SpiralCarveSpeckLow"))
        || ShapeSource.Contains(TEXT("SpiralCarveRimCrispness")));

    FString AdapterSource;'''
replacement = '''    TestFalse(TEXT("No threshold/depth/speck modeling path remains"),
        ShapeSource.Contains(TEXT("SpiralCarveSoftLow"))
        || ShapeSource.Contains(TEXT("SpiralCarveDepthMin"))
        || ShapeSource.Contains(TEXT("SpiralCarveSpeckLow"))
        || ShapeSource.Contains(TEXT("SpiralCarveRimCrispness")));

    FString DensityFieldSource;
    const FString DensityFieldPath = FPaths::Combine(
        FPaths::ProjectPluginsDir(),
        TEXT("SavageSuperStorm/Shaders/Public/StormDensityField.ush"));
    if (!TestTrue(TEXT("Density field source loads"),
        FFileHelper::LoadFileToString(DensityFieldSource, *DensityFieldPath)))
    {
        return false;
    }
    TestTrue(TEXT("LF shader lower bound is exactly WorleyFBM minus one"),
        DensityFieldSource.Contains(TEXT("(worleyFBM - 1.0),")));
    TestFalse(TEXT("LF shader lower bound is not sign-inverted"),
        DensityFieldSource.Contains(TEXT("(worleyFBM - 1.0) * -1")));

    FString AdapterSource;'''
if original.count(needle) != 1:
    raise RuntimeError("Expected one v10 ownership insertion point")
path.write_text(original.replace(needle, replacement, 1), encoding="utf-8")
print("CODEX_V10_LF_SHADER_CONTRACT_TEST|SUCCESS")
