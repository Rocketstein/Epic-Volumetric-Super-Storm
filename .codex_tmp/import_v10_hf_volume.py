from pathlib import Path

import unreal


source = str(
    Path(__file__).resolve().with_name(
        "VT_Nubis_Hillaire_WorleyDetail_32_RGBA8_Volume_v10.dds"
    )
)
destination_path = "/VolumetricSuperStorm/Materials/Texture"
asset_name = "VT_Nubis_Hillaire_WorleyDetail_32_RGBA8_Volume"
asset_path = f"{destination_path}/{asset_name}"

task = unreal.AssetImportTask()
task.set_editor_property("filename", source)
task.set_editor_property("destination_path", destination_path)
task.set_editor_property("destination_name", asset_name)
task.set_editor_property("automated", True)
task.set_editor_property("replace_existing", True)
task.set_editor_property("replace_existing_settings", True)
task.set_editor_property("save", True)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

texture = unreal.load_asset(asset_path)
if texture is None or not isinstance(texture, unreal.VolumeTexture):
    raise RuntimeError(f"v10 HF import did not produce a VolumeTexture: {asset_path}")
texture.modify()
texture.set_editor_property("srgb", False)
texture.set_editor_property(
    "compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT
)
texture.set_editor_property("address_mode", unreal.TextureAddress.TA_WRAP)

if not unreal.EditorAssetLibrary.save_loaded_asset(texture, False):
    raise RuntimeError("Failed to save the v10 HF volume")
unreal.log(
    "CODEX_V10_HF_IMPORT|SUCCESS|"
    f"path={texture.get_path_name()}|class={texture.get_class().get_name()}|"
    f"source={source}"
)
unreal.log("CODEX_V10_HF_IMPORT_COMPLETE")
