import unreal


ASSET_PATH = "/SavageSuperStorm/Materials/Texture/VT_WorleyDetail_32"
texture = unreal.load_asset(ASSET_PATH)
if texture is None:
    raise RuntimeError(f"Unable to load {ASSET_PATH}")

for property_name in (
    "address_mode",
    "filter",
    "mip_gen_settings",
    "compression_settings",
    "compression_no_alpha",
    "compression_quality",
    "srgb",
    "lod_bias",
    "lod_group",
    "never_stream",
    "virtual_texture_streaming",
    "max_texture_size",
    "resize_during_build_x",
    "resize_during_build_y",
    "use_new_mip_filter",
):
    try:
        value = texture.get_editor_property(property_name)
        unreal.log(f"CODEX_HF_PROPERTY={property_name}:{value}")
    except Exception as error:
        unreal.log(f"CODEX_HF_PROPERTY_ERROR={property_name}:{error}")

for method_name in (
    "blueprint_get_built_texture_size",
    "blueprint_get_memory_size",
    "blueprint_get_texture_source_disk_and_memory_size",
    "blueprint_get_texture_source_id_string",
    "compute_texture_source_channel_min_max",
    "get_texture_streaming_method",
):
    method = getattr(texture, method_name, None)
    if method is None:
        unreal.log(f"CODEX_HF_METHOD_MISSING={method_name}")
        continue
    try:
        unreal.log(f"CODEX_HF_METHOD={method_name}:{method()}")
    except Exception as error:
        unreal.log(f"CODEX_HF_METHOD_ERROR={method_name}:{error}")

registry = unreal.AssetRegistryHelpers.get_asset_registry()
asset_data = registry.get_asset_by_object_path(unreal.SoftObjectPath(texture.get_path_name()))
unreal.log(
    "CODEX_HF_ASSET_MEMBERS="
    + ",".join(
        sorted(
            name
            for name in dir(asset_data)
            if any(token in name.lower() for token in ("tag", "chunk", "package", "asset"))
        )
    )
)
for tag in (
    "Dimensions",
    "SizeX",
    "SizeY",
    "SizeZ",
    "PixelFormat",
    "SourceFile",
    "SourceFileHash",
    "CompressionSettings",
    "TextureGroup",
):
    try:
        unreal.log(f"CODEX_HF_TAG_LOOKUP={tag}:{asset_data.get_tag_value(tag)}")
    except Exception as error:
        unreal.log(f"CODEX_HF_TAG_LOOKUP_ERROR={tag}:{error}")

unreal.log(
    "CODEX_TEXTURE_EXPORTERS="
    + ",".join(sorted(name for name in dir(unreal) if "textureexport" in name.lower()))
)

