from pathlib import Path


path = (
    Path(__file__).resolve().parents[1]
    / "Plugins/SavageSuperStorm/Shaders/Public/StormDensityField.ush"
)
original = path.read_text(encoding="utf-8")
dead_block = '''float StormEvaluateSpiralUndersideBand(
    float HLocal,
    float BaseBottomType)
{
    // BottomType controls demand/strength only. The lower-band extent remains
    // fixed at the former maximum (0.30) for every column.
    return (1.0 - StormEvaluateWispyToBillowyBlend(HLocal))
        * StormEvaluateBottomTypeWispyAmount(BaseBottomType);
}

'''
if original.count(dead_block) != 1:
    raise RuntimeError("Expected one unused Spiral underside helper")
path.write_text(original.replace(dead_block, "", 1), encoding="utf-8")
print("CODEX_V10_DEAD_UNDERSIDE_HELPER|REMOVED")
