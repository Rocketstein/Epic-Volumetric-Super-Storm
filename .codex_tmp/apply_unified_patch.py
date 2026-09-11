import re
import sys
from pathlib import Path


if len(sys.argv) != 3:
    raise SystemExit("usage: apply_unified_patch.py PATCH TARGET")

patch_path = Path(sys.argv[1])
target_path = Path(sys.argv[2])
patch_lines = patch_path.read_text(encoding="utf-8").splitlines(keepends=True)
source_lines = target_path.read_text(encoding="utf-8").splitlines(keepends=True)

hunk_indices = [
    index for index, line in enumerate(patch_lines) if line.startswith("@@ ")
]
if not hunk_indices:
    raise RuntimeError("Patch contains no hunks")

output = []
source_index = 0
for hunk_number, patch_index in enumerate(hunk_indices):
    header = patch_lines[patch_index]
    match = re.match(r"@@ -(\d+)(?:,\d+)? \+\d+(?:,\d+)? @@", header)
    if not match:
        raise RuntimeError(f"Malformed hunk header: {header.rstrip()}")
    old_start = int(match.group(1)) - 1
    if old_start < source_index:
        raise RuntimeError("Overlapping hunks")
    output.extend(source_lines[source_index:old_start])
    source_index = old_start

    end = (
        hunk_indices[hunk_number + 1]
        if hunk_number + 1 < len(hunk_indices)
        else len(patch_lines)
    )
    for patch_line in patch_lines[patch_index + 1:end]:
        if patch_line.startswith("\\"):
            continue
        marker = patch_line[:1]
        payload = patch_line[1:]
        if marker == "+":
            output.append(payload)
        elif marker in (" ", "-"):
            if source_index >= len(source_lines):
                raise RuntimeError("Patch reads beyond target")
            if source_lines[source_index] != payload:
                raise RuntimeError(
                    "Patch context mismatch at target line "
                    f"{source_index + 1}: expected {payload!r}, "
                    f"found {source_lines[source_index]!r}"
                )
            if marker == " ":
                output.append(payload)
            source_index += 1
        else:
            raise RuntimeError(
                f"Unexpected patch line in hunk: {patch_line.rstrip()}"
            )

output.extend(source_lines[source_index:])
target_path.write_text("".join(output), encoding="utf-8", newline="")
print(
    "CODEX_UNIFIED_PATCH|SUCCESS|"
    f"hunks={len(hunk_indices)}|target={target_path}"
)
