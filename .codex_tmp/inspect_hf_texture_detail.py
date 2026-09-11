import unreal


ASSET_PATH = "/SavageSuperStorm/Materials/Texture/VT_WorleyDetail_32"
texture = unreal.load_asset(ASSET_PATH)
if texture is None:
    raise RuntimeError(f"Unable to load {ASSET_PATH}")

unreal.log(f"CODEX_HF_CLASS={texture.get_class().get_path_name()}")
unreal.log(f"CODEX_HF_PATH={texture.get_path_name()}")

interesting_members = [
    name
    for name in dir(texture)
    if any(
        token in name.lower()
        for token in (
            "size",
            "source",
            "address",
            "filter",
            "compression",
            "mip",
            "format",
            "srgb",
            "lod",
            "stream",
            "memory",
            "platform",
        )
    )
]
unreal.log("CODEX_HF_MEMBERS=" + ",".join(sorted(interesting_members)))

properties = (
    "address_x",
    "address_y",
    "address_z",
    "filter",
    "mip_gen_settings",
    "compression_settings",
    "compression_no_alpha",
    "compression_none",
    "compression_quality",
    "srgb",
    "lod_bias",
    "lod_group",
    "never_stream",
    "virtual_texture_streaming",
    "source_texture",
    "source_encoding",
)
for property_name in properties:
    try:
        value = texture.get_editor_property(property_name)
        if isinstance(value, unreal.Object):
            value = value.get_path_name()
        unreal.log(f"CODEX_HF_PROPERTY={property_name}:{value}")
    except Exception as error:
        unreal.log(f"CODEX_HF_PROPERTY_ERROR={property_name}:{error}")

for method_name in (
    "get_size_x",
    "get_size_y",
    "get_size_z",
    "blueprint_get_size_x",
    "blueprint_get_size_y",
    "blueprint_get_size_z",
    "get_surface_width",
    "get_surface_height",
    "get_surface_depth",
    "get_num_mips",
    "get_pixel_format",
    "get_running_platform_data",
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
asset_data = registry.get_asset_by_object_path(unreal.Name(texture.get_path_name()))
unreal.log(f"CODEX_HF_ASSET_DATA={asset_data}")
try:
    for tag, value in asset_data.tags_and_values.items():
        unreal.log(f"CODEX_HF_TAG={tag}:{value}")
except Exception as error:
    unreal.log(f"CODEX_HF_TAG_ERROR={error}")

