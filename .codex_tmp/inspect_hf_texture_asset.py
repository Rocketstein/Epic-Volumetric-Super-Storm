import unreal


asset_path = "/SavageSuperStorm/Materials/Texture/VT_WorleyDetail_32"
texture = unreal.load_asset(asset_path)
if texture is None:
    raise RuntimeError(f"Unable to load {asset_path}")

unreal.log(f"CODEX_HF_CLASS={texture.get_class().get_name()}")
unreal.log(f"CODEX_HF_PATH={texture.get_path_name()}")

for name in dir(texture):
    lowered = name.lower()
    if any(token in lowered for token in ("size", "source", "format", "filter", "address", "mip", "compression", "srgb")):
        unreal.log(f"CODEX_HF_MEMBER={name}")

for property_name in (
    "filter",
    "mip_gen_settings",
    "compression_settings",
    "srgb",
    "address_mode",
    "source",
    "source_file_path",
):
    try:
        value = texture.get_editor_property(property_name)
    except Exception as error:
        unreal.log(f"CODEX_HF_PROPERTY_ERROR={property_name}:{error}")
        continue
    unreal.log(f"CODEX_HF_PROPERTY={property_name}:{value}")

asset_registry = unreal.AssetRegistryHelpers.get_asset_registry()
asset_data = asset_registry.get_asset_by_object_path(unreal.Name(texture.get_path_name()))
unreal.log(f"CODEX_HF_ASSET_VALID={asset_data.is_valid()}")
try:
    unreal.log(f"CODEX_HF_TAGS={asset_data.tags_and_values}")
except Exception as error:
    unreal.log(f"CODEX_HF_TAGS_ERROR={error}")

for tag_name in (
    "Dimensions",
    "SizeX",
    "SizeY",
    "SizeZ",
    "SourceFile",
    "SourceFilePath",
    "PixelFormat",
    "CompressionSettings",
):
    try:
        value = asset_data.get_tag_value(tag_name)
    except Exception as error:
        unreal.log(f"CODEX_HF_TAG_ERROR={tag_name}:{error}")
        continue
    unreal.log(f"CODEX_HF_TAG={tag_name}:{value}")
