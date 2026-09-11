import unreal


material = unreal.load_asset("/SavageSuperStorm/Materials/M_SSS")
if material is None:
    raise RuntimeError("M_SSS not found")

matches = []
for expr in unreal.MaterialEditingLibrary.get_material_expressions(material):
    if not isinstance(expr, unreal.MaterialExpressionTextureObjectParameter):
        continue
    name = str(expr.get_editor_property("parameter_name"))
    if name != "HFTex":
        continue
    texture = expr.get_editor_property("texture")
    matches.append(texture)
if len(matches) != 1 or matches[0] is None:
    raise RuntimeError(f"Expected one HFTex default texture, found {len(matches)}")

texture = matches[0]
unreal.log(f"CODEX_V10_HF|path={texture.get_path_name()}|class={texture.get_class().get_name()}")
for property_name in (
    "asset_import_data",
    "compression_settings",
    "srgb",
    "filter",
    "address_mode",
):
    try:
        unreal.log(
            f"CODEX_V10_HF_PROP|{property_name}="
            f"{texture.get_editor_property(property_name)}"
        )
    except Exception as error:
        unreal.log(f"CODEX_V10_HF_PROP|{property_name}=<unavailable:{error}>")
unreal.log("CODEX_V10_HF_COMPLETE")
