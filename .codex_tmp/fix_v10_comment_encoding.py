from pathlib import Path


path = (
    Path(__file__).resolve().parents[1]
    / "Plugins/SavageSuperStorm/Shaders/Public/StormDensityMaterialAdapter.ush"
)
original = path.read_text(encoding="utf-8")
old = "    // is gated on actual proximity to the body, not on elapsed time ??segments"
new = "    // is gated on actual proximity to the body, not elapsed time; segments"
if original.count(old) != 1:
    raise RuntimeError("Expected one corrupted comment")
path.write_text(original.replace(old, new, 1), encoding="utf-8")
print("CODEX_V10_COMMENT_ENCODING|SUCCESS")
