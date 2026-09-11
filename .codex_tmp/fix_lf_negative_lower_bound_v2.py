from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def replace_exact(relative_path: str, old: str, new: str) -> None:
    path = ROOT / relative_path
    original = path.read_text(encoding="utf-8")
    if original.count(new) == 1 and original.count(old) == 0:
        return
    count = original.count(old)
    if count != 1:
        raise RuntimeError(f"Expected one old or one target match in {relative_path}")
    path.write_text(original.replace(old, new, 1), encoding="utf-8")


replace_exact(
    "Plugins/SavageSuperStorm/Shaders/Public/StormDensityField.ush",
    "        (worleyFBM - 1.0) * -1,",
    "        (worleyFBM - 1.0),",
)
replace_exact(
    "Plugins/SavageSuperStorm/Source/SavageSuperStormRuntime/Public/Field/StormDensityContract.h",
    "//   LFShape = remap_saturated(\n"
    "//       saturate(LF.r), (WorleyFBM - 1) * -1, 1, 0, 1)",
    "//   LFShape = remap_saturated(saturate(LF.r), WorleyFBM - 1, 1, 0, 1)",
)
replace_exact(
    "Plugins/SavageSuperStorm/Source/SavageSuperStormRuntime/Public/Field/StormDensityContract.h",
    "        (WorleyFBM - 1.0f) * -1.0f,",
    "        WorleyFBM - 1.0f,",
)
replace_exact(
    "Plugins/SavageSuperStorm/Source/SavageSuperStormRuntime/Private/Tests/StormDensityContractTests.cpp",
    '''    TestTrue(TEXT("LFShape uses the locked positive Worley lower bound"),
        FMath::IsNearlyEqual(
            DecodeNubisLFShape(LFRaw),
            0.40f,
            1.0e-6f));
    TestTrue(TEXT("LF GBA octaves reshape the same R carrier"),
        FMath::IsNearlyEqual(
            DecodeNubisLFShape(SameCarrierDifferentWorley),
            0.60f,
            1.0e-6f));
    TestEqual(TEXT("WorleyFBM zero keeps the degenerate lower bound finite"),
        DecodeNubisLFShape(FVector4f::Zero()),
        0.0f);''',
    '''    TestTrue(TEXT("LFShape uses the negative Worley lower bound"),
        FMath::IsNearlyEqual(
            DecodeNubisLFShape(LFRaw),
            0.80f,
            1.0e-6f));
    TestTrue(TEXT("LF GBA octaves reshape the same R carrier"),
        FMath::IsNearlyEqual(
            DecodeNubisLFShape(SameCarrierDifferentWorley),
            0.76f,
            1.0e-6f));
    TestTrue(TEXT("Zero raw LF maps halfway from minus one to one"),
        FMath::IsNearlyEqual(
            DecodeNubisLFShape(FVector4f::Zero()),
            0.50f,
            1.0e-6f));''',
)

print("CODEX_LF_NEGATIVE_LOWER_BOUND|SUCCESS")
