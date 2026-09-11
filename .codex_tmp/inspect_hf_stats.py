import unreal


texture = unreal.load_asset("/SavageSuperStorm/Materials/Texture/VT_WorleyDetail_32")
if texture is None:
    raise RuntimeError("Unable to load HF volume texture")

for method_name in (
    "blueprint_get_built_texture_size",
    "blueprint_get_memory_size",
    "blueprint_get_texture_source_disk_and_memory_size",
    "compute_texture_source_channel_min_max",
):
    method = getattr(texture, method_name)
    unreal.log(f"CODEX_HF_METHOD_DOC={method_name}:{method.__doc__}")
    for args in ((), (False,), (True,)):
        try:
            value = method(*args)
        except Exception as error:
            unreal.log(f"CODEX_HF_METHOD_ERROR={method_name}:{args}:{error}")
            continue
        unreal.log(f"CODEX_HF_METHOD={method_name}:{args}:{value}")
