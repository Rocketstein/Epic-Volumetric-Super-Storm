from pathlib import Path
import hashlib
import importlib.util


project_root = Path(__file__).resolve().parents[1]
generator_path = project_root / "Tools/GenerateHFWorleyVolume.py"
spec = importlib.util.spec_from_file_location("hf_generator", generator_path)
module = importlib.util.module_from_spec(spec)
assert spec.loader is not None
spec.loader.exec_module(module)

source_path = Path(__file__).resolve().with_name(
    "VT_Nubis_Hillaire_WorleyDetail_32_RGBA8_Volume.dds"
)
source = source_path.read_bytes()
expected_payload = module.generate_volume()
expected = module._dds_header(module.DEFAULT_SIZE) + expected_payload
source_payload = source[len(module._dds_header(module.DEFAULT_SIZE)):]
print(f"source_bytes={len(source)} expected_bytes={len(expected)}")
print(f"source_sha256={hashlib.sha256(source).hexdigest()}")
print(f"expected_sha256={hashlib.sha256(expected).hexdigest()}")
print(f"exact_default_match={source == expected}")
if len(source_payload) == len(expected_payload):
    print(
        "source_neighbor_deltas="
        f"{module.validate_volume(source_payload, module.DEFAULT_SIZE)}"
    )
