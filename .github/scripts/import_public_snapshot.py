from __future__ import annotations

import hashlib
import json
import shutil
import stat
import tarfile
import tempfile
import urllib.parse
import urllib.request
import zipfile
from pathlib import Path, PurePosixPath

repo = Path.cwd().resolve()
request_path = repo / ".public_import/request.json"
if not request_path.is_file():
    raise RuntimeError("Missing .public_import/request.json")
request = json.loads(request_path.read_text(encoding="utf-8"))

url = str(request["download_url"])
parsed = urllib.parse.urlparse(url)
if parsed.scheme != "https" or not parsed.hostname or not parsed.hostname.endswith(
    ".oaiusercontent.com"
):
    raise RuntimeError("Snapshot URL is not an approved one-time artifact URL")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


with tempfile.TemporaryDirectory(prefix="vss-public-import-") as temporary:
    temp = Path(temporary)
    artifact_zip = temp / "artifact.zip"
    archive_path = temp / "public-snapshot.tar.gz"
    extracted = temp / "extracted"
    extracted.mkdir()

    http_request = urllib.request.Request(
        url,
        headers={"User-Agent": "Volumetric-Super-Storm-public-import"},
    )
    with urllib.request.urlopen(http_request, timeout=120) as response:
        with artifact_zip.open("wb") as output:
            shutil.copyfileobj(response, output)

    if artifact_zip.stat().st_size != int(request["zip_bytes"]):
        raise RuntimeError("Artifact ZIP size mismatch")
    if sha256(artifact_zip) != str(request["zip_sha256"]):
        raise RuntimeError("Artifact ZIP SHA-256 mismatch")

    with zipfile.ZipFile(artifact_zip) as artifact:
        files = [member for member in artifact.infolist() if not member.is_dir()]
        if len(files) != 1 or PurePosixPath(files[0].filename).name != "public-snapshot.tar.gz":
            raise RuntimeError("Artifact ZIP must contain only public-snapshot.tar.gz")
        member = files[0]
        member_path = PurePosixPath(member.filename)
        if member_path.is_absolute() or ".." in member_path.parts:
            raise RuntimeError("Unsafe artifact ZIP path")
        mode = member.external_attr >> 16
        if stat.S_ISLNK(mode):
            raise RuntimeError("Symlinks are not allowed in artifact ZIP")
        with artifact.open(member) as source, archive_path.open("wb") as output:
            shutil.copyfileobj(source, output)

    if archive_path.stat().st_size != int(request["archive_bytes"]):
        raise RuntimeError("Snapshot archive size mismatch")
    expected_archive_hash = str(request["archive_sha256"])
    if sha256(archive_path) != expected_archive_hash:
        raise RuntimeError("Snapshot archive SHA-256 mismatch")

    with tarfile.open(archive_path, "r:gz") as archive:
        for member in archive.getmembers():
            name = PurePosixPath(member.name)
            if name.is_absolute() or ".." in name.parts:
                raise RuntimeError(f"Unsafe snapshot path: {member.name}")
            if member.issym() or member.islnk() or member.isdev():
                raise RuntimeError(f"Unsafe snapshot member: {member.name}")
        archive.extractall(extracted, filter="data")

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
            raise RuntimeError(f"Required snapshot path is missing: {relative}")

    banned = {
        Path("Content"),
        Path("Plugins/SavageSuperStorm"),
        Path("Plugins/VisualStudioTools"),
        Path("Plugins/VolumetricSuperStorm/Docs"),
        Path("Plugins/VolumetricSuperStorm/README.md"),
        Path("Plugins/VolumetricSuperStorm/CHANGELOG.md"),
        Path("Plugins/VolumetricSuperStorm/CREDITS.md"),
        Path("Plugins/VolumetricSuperStorm/Config/FilterPlugin.ini"),
        Path("README.md"),
        Path(".github"),
        Path(".public_import"),
        Path(".public_export"),
    }
    for relative in banned:
        if (extracted / relative).exists():
            raise RuntimeError(f"Banned snapshot path survived: {relative}")

    extracted_files = [path for path in extracted.rglob("*") if path.is_file()]
    if len(extracted_files) != int(request["file_count"]):
        raise RuntimeError("Snapshot file count mismatch")
    if sum(path.stat().st_size for path in extracted_files) != int(
        request["payload_bytes"]
    ):
        raise RuntimeError("Snapshot payload size mismatch")

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
            "archive_sha256": expected_archive_hash,
            "file_count": request["file_count"],
            "payload_bytes": request["payload_bytes"],
        },
        indent=2,
    )
)
