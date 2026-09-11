from pathlib import Path


field_path = Path("Plugins/SavageSuperStorm/Shaders/Public/StormDensityField.ush")
field = field_path.read_text(encoding="utf-8")
old = """// BaseBottomType selects weakly correlated underside HF carriers inside a
// fixed lower band. SpiralTendrilMask never affects the Wispy/HF path.
"""
new = """// BaseBottomType selects weakly correlated underside HF carriers inside a
// fixed lower band. TwistedModelingNoise affects upstream Coverage/TopType only
// and never directly affects the Wispy/HF or Curl paths.
"""
if field.count(old) != 1:
    raise RuntimeError("Stale Wispy ownership comment changed")
field = field.replace(old, new)
old_abi = "// ShapeRT2 = (MinLayerHeight01, SpiralTendrilMask, MaxLayerHeight01, CenterMask)"
new_abi = "// ShapeRT2 = (MinLayerHeight01, TwistedModelingNoise, MaxLayerHeight01, CenterMask)"
if field.count(old_abi) != 1:
    raise RuntimeError("Stale ShapeRT2 ABI comment changed")
field = field.replace(old_abi, new_abi)
field_path.write_text(field, encoding="utf-8")


v9_path = Path("Docs/NubisMesocycloneV9MaterialWiring.md")
v9 = v9_path.read_text(encoding="utf-8")
notice = """> [!WARNING]
> 이 문서는 v9 기록용입니다. 활성 구현 계약은
> `Docs/NubisMesocycloneV10Implementation.md`가 대체합니다. 특히
> ShapeRT2.G와 Anvil wind/stretch/skew/warp 설명은 더 이상 유효하지 않습니다.

"""
if not v9.startswith(notice):
    v9_path.write_text(notice + v9, encoding="utf-8")
