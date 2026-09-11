from pathlib import Path

import unreal


texture = unreal.load_asset(
    "/VolumetricSuperStorm/Materials/Texture/VT_WorleyDetail_32"
)
if texture is None:
    raise RuntimeError("Unable to load HF volume texture")

output_path = Path(__file__).resolve().with_name("VT_WorleyDetail_32.dds")
task = unreal.AssetExportTask()
task.set_editor_property("object", texture)
task.set_editor_property("filename", str(output_path))
task.set_editor_property("automated", True)
task.set_editor_property("prompt", False)
task.set_editor_property("replace_identical", True)
task.set_editor_property("use_file_archive", False)
task.set_editor_property("exporter", unreal.TextureExporterDDS())

result = unreal.Exporter.run_asset_export_task(task)
unreal.log(f"CODEX_HF_EXPORT_RESULT={result}")
unreal.log(f"CODEX_HF_EXPORT_ERRORS={task.get_editor_property('errors')}")
