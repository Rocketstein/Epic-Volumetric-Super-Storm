from pathlib import Path

import unreal


project_root = Path(__file__).resolve().parents[1]
source = str(
    (
        project_root
        / "Tools"
        / "Generated"
        / "VT_Nubis_Hillaire_WorleyDetail_32_RGBA8_Volume_v10.dds"
    ).resolve()
)
destination_path = "/VolumetricSuperStorm/Materials/Texture"
asset_name = "VT_Nubis_Hillaire_WorleyDetail_32_RGBA8_Volume"

task = unreal.AssetImportTask()
task.set_editor_property("filename", source)
task.set_editor_property("destination_path", destination_path)
task.set_editor_property("destination_name", asset_name)
task.set_editor_property("automated", True)
task.set_editor_property("replace_existing", True)
task.set_editor_property("replace_existing_settings", True)
task.set_editor_property("save", True)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

texture = unreal.load_asset(f"{destination_path}/{asset_name}")
if texture is None or not isinstance(texture, unreal.VolumeTexture):
    raise RuntimeError("Project-source HF reimport failed")
texture.modify()
texture.set_editor_property("srgb", False)
texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
texture.set_editor_property("address_mode", unreal.TextureAddress.TA_WRAP)
if not unreal.EditorAssetLibrary.save_loaded_asset(texture, False):
    raise RuntimeError("Failed to save project-source HF volume")
unreal.log(f"CODEX_V10_HF_PROJECT_SOURCE|SUCCESS|source={source}")
