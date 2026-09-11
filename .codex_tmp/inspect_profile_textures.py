import unreal


material = unreal.load_asset("/SavageSuperStorm/Materials/M_SSS")
if material is None:
    raise RuntimeError("Unable to load M_SSS")

expressions = unreal.MaterialEditingLibrary.get_material_expressions(material)
for expression in expressions:
    if not isinstance(expression, unreal.MaterialExpressionTextureObjectParameter):
        continue
    name = str(expression.get_editor_property("parameter_name"))
    texture = expression.get_editor_property("texture")
    unreal.log(
        f"CODEX_TEX_PARAM={name}:{texture.get_path_name() if texture else 'None'}"
    )
    if texture:
        try:
            unreal.log(f"CODEX_TEX_SIZE={name}:{texture.blueprint_get_built_texture_size()}")
        except Exception as error:
            unreal.log(f"CODEX_TEX_SIZE_ERROR={name}:{error}")
        try:
            unreal.log(
                f"CODEX_TEX_RANGE={name}:{texture.compute_texture_source_channel_min_max()}"
            )
        except Exception as error:
            unreal.log(f"CODEX_TEX_RANGE_ERROR={name}:{error}")
