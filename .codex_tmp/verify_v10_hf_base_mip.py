from pathlib import Path
import importlib.util


project_root = Path(__file__).resolve().parents[1]
generator_path = project_root / "Tools/GenerateHFWorleyVolume.py"
spec = importlib.util.spec_from_file_location("hf_generator", generator_path)
module = importlib.util.module_from_spec(spec)
assert spec.loader is not None
spec.loader.exec_module(module)

source = (
    Path(__file__).resolve().with_name(
        "VT_Nubis_Hillaire_WorleyDetail_32_RGBA8_Volume.dds"
    )
).read_bytes()
header_size = len(module._dds_header(module.DEFAULT_SIZE))
expected_payload = module.generate_volume()
source_base_mip = source[header_size:header_size + len(expected_payload)]
print(f"base_mip_default_match={source_base_mip == expected_payload}")
print(
    "base_mip_neighbor_deltas="
    f"{module.validate_volume(source_base_mip, module.DEFAULT_SIZE)}"
)
