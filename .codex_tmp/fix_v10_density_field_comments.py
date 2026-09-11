from pathlib import Path


path = (
    Path(__file__).resolve().parents[1]
    / "Plugins/SavageSuperStorm/Shaders/Public/StormDensityField.ush"
)
original = path.read_text(encoding="utf-8")
replacements = {
    "//   WispyAmount = EffectiveBottomType":
        "//   WispyAmount = BaseBottomType",
    "// EffectiveBottomType selects weakly correlated underside HF carriers inside a\n"
    "// fixed lower band. It never expands the band or adds a second remap.":
        "// BaseBottomType selects weakly correlated underside HF carriers inside a\n"
        "// fixed lower band. TwistedModelingNoise affects upstream Coverage/TopType only\n"
        "// and never directly affects the Wispy/HF or Curl paths.",
    "// ShapeRT2 = (MinLayerHeight01, SpiralTendrilMask, MaxLayerHeight01, CenterMask)":
        "// ShapeRT2 = (MinLayerHeight01, TwistedModelingNoise, MaxLayerHeight01, CenterMask)",
}
updated = original
for old, new in replacements.items():
    if updated.count(old) != 1:
        raise RuntimeError(f"Expected one stale comment: {old}")
    updated = updated.replace(old, new, 1)
path.write_text(updated, encoding="utf-8")
print("CODEX_V10_DENSITY_FIELD_COMMENTS|SUCCESS")
