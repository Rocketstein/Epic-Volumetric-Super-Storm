from pathlib import Path


path = Path("Plugins/SavageSuperStorm/Shaders/Public/StormDensityField.ush")
raw = path.read_bytes()
old = b"//   WispyAmount = EffectiveBottomType"
new = b"//   WispyAmount = BaseBottomType"
if raw.count(old) != 1:
    raise RuntimeError(f"Expected one stale comment, found {raw.count(old)}")
path.write_bytes(raw.replace(old, new))
