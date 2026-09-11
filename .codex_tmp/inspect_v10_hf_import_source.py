import unreal


texture = unreal.load_asset(
    "/SavageSuperStorm/Materials/Texture/"
    "VT_Nubis_Hillaire_WorleyDetail_32_RGBA8_Volume"
)
if texture is None:
    raise RuntimeError("HF volume not found")
data = texture.get_editor_property("asset_import_data")
for name in ("extract_filenames", "get_first_filename"):
    method = getattr(data, name, None)
    if method is None:
        unreal.log(f"CODEX_V10_HF_SOURCE|{name}=<missing>")
        continue
    try:
        unreal.log(f"CODEX_V10_HF_SOURCE|{name}={method()}")
    except Exception as error:
        unreal.log(f"CODEX_V10_HF_SOURCE|{name}=<error:{error}>")
unreal.log("CODEX_V10_HF_SOURCE_COMPLETE")
