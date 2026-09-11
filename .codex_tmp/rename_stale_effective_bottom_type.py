from pathlib import Path


path = Path("Plugins/SavageSuperStorm/Shaders/Public/StormDensityMaterialAdapter.ush")
raw = path.read_bytes()
old = b"EffectiveBottomType"
new = b"BaseBottomType"
if raw.count(old) != 3:
    raise RuntimeError(f"Expected three stale names, found {raw.count(old)}")
path.write_bytes(raw.replace(old, new))
