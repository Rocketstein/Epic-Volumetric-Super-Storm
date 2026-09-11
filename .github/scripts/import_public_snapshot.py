from __future__ import annotations

import base64
import hashlib
import json
import shutil
import tarfile
import tempfile
from pathlib import Path, PurePosixPath

repo = Path.cwd().resolve()
transfer = repo / ".public_export"
manifest_path = transfer / "manifest.json"
ready_path = transfer / "READY"

if not manifest_path.is_file() or not ready_path.is_file():
    raise RuntimeError("Transfer package is incomplete: manifest.json and READY are required")

manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
chunk_count = int(manifest["chunk_count"])
chunks = sorted(transfer.glob("chunk-*.b64"))
if len(chunks) != chunk_count:
    raise RuntimeError(f"Expected {chunk_count} chunks, found {len(chunks)}")

encoded = "".join(path.read_text(encoding="ascii").strip() for path in chunks)
if len(encoded) != int(manifest["base64_characters"]):
    raise RuntimeError("Base64 payload length does not match manifest")

archive_bytes = base64.b64decode(encoded, validate=True)
actual_hash = hashlib.sha256(archive_bytes).hexdigest()
expected_hash = str(manifest["archive_sha256"])
if actual_hash != expected_hash:
    raise RuntimeError(f"Archive SHA-256 mismatch: {actual_hash} != {expected_hash}")
if ready_path.read_text(encoding="ascii").strip() != expected_hash:
    raise RuntimeError("READY marker does not match archive hash")
if len(archive_bytes) != int(manifest["archive_bytes"]):
    raise RuntimeError("Archive byte length does not match manifest")

with tempfile.TemporaryDirectory(prefix="vss-public-import-") as temporary:
    temp = Path(temporary)
    archive_path = temp / "snapshot.tar.gz"
    extracted = temp / "extracted"
    archive_path.write_bytes(archive_bytes)
    extracted.mkdir()

    with tarfile.open(archive_path, "r:gz") as tar:
        for member in tar.getmembers():
            name = PurePosixPath(member.name)
            if name.is_absolute() or ".." in name.parts:
                raise RuntimeError(f"Unsafe archive path: {member.name}")
            if member.issym() or member.islnk() or member.isdev():
                raise RuntimeError(f"Unsafe archive member type: {member.name}")
        tar.extractall(extracted, filter="data")

    required = {
        Path(".codex_tmp"),
        Path("Docs/Release/FabListing.md"),
        Path("Plugins/VolumetricSuperStorm/VolumetricSuperStorm.uplugin"),
        Path("Plugins/VolumetricSuperStorm/Source"),
        Path("Plugins/VolumetricSuperStorm/Shaders"),
        Path("Plugins/VolumetricSuperStorm/Content"),
        Path("VolumetricSuperStorm.uproject"),
    }
    for relative in required:
        if not (extracted / relative).exists():
            raise RuntimeError(f"Required extracted path is missing: {relative}")

    banned = {
        Path("Content"),
        Path("Plugins/SavageSuperStorm"),
        Path("Plugins/VisualStudioTools"),
        Path("Plugins/VolumetricSuperStorm/Docs"),
        Path("Plugins/VolumetricSuperStorm/README.md"),
        Path("Plugins/VolumetricSuperStorm/CHANGELOG.md"),
        Path("Plugins/VolumetricSuperStorm/CREDITS.md"),
        Path("README.md"),
        Path(".github"),
        Path(".public_export"),
    }
    for relative in banned:
        if (extracted / relative).exists():
            raise RuntimeError(f"Banned extracted path survived: {relative}")

    extracted_files = [path for path in extracted.rglob("*") if path.is_file()]
    if len(extracted_files) != int(manifest["file_count"]):
        raise RuntimeError("Extracted file count does not match manifest")
    if sum(path.stat().st_size for path in extracted_files) != int(manifest["payload_bytes"]):
        raise RuntimeError("Extracted payload size does not match manifest")

    # Replace the branch worktree wholesale while preserving only Git metadata.
    for existing in repo.iterdir():
        if existing.name == ".git":
            continue
        if existing.is_dir() and not existing.is_symlink():
            shutil.rmtree(existing)
        else:
            existing.unlink()

    for item in extracted.iterdir():
        shutil.move(str(item), repo / item.name)

print(
    json.dumps(
        {
            "archive_sha256": expected_hash,
            "file_count": manifest["file_count"],
            "payload_bytes": manifest["payload_bytes"],
        },
        indent=2,
    )
)
