from pathlib import Path


path = Path(__file__).resolve().parents[1] / "Docs/NubisMesocycloneV10Implementation.md"
original = path.read_text(encoding="utf-8")
needle = "## LF/HF와 머티리얼 연결\n\n"
replacement = (
    "## LF/HF와 머티리얼 연결\n\n"
    "- LF decode lower bound는 최종 사용자 지정대로 `WorleyFBM - 1.0`이다. "
    "HLSL과 CPU 미러가 동일하며 부호 반전(`* -1`)은 사용하지 않는다.\n"
)
if original.count(needle) != 1:
    raise RuntimeError("Expected one LF/HF documentation section")
path.write_text(original.replace(needle, replacement, 1), encoding="utf-8")
print("CODEX_V10_DOC_LF_CONTRACT|SUCCESS")
