import unreal


ASSETS = (
    "/SavageSuperStorm/Materials/Functions/MF_Erosion",
    "/SavageSuperStorm/Materials/Functions/MF_StormMain",
    "/SavageSuperStorm/Materials/M_SSS",
    "/SavageSuperStorm/Materials/M_SSS_NubisIntegrated_VC",
    "/SavageSuperStorm/Materials/M_SSS_NubisIntegrated_Merge_VC",
    "/SavageSuperStorm/Materials/M_SSS_Beom_Coverage_Lighting_Debug1",
    "/SavageSuperStorm/Materials/M_SSS_Beom_Coverage_Lighting_V3",
    "/SavageSuperStorm/Materials/M_SSS_Beom_Coverage_Lighting_V4_RedGlow",
)

TOKENS = ("vortex", "mesocyclone", "stormshapert2", "erosion")


def read_property(obj, name):
    try:
        return obj.get_editor_property(name)
    except Exception:
        return None


def label(expression):
    pieces = [expression.get_class().get_name(), expression.get_name()]
    for prop in ("parameter_name", "input_name", "name", "desc"):
        value = read_property(expression, prop)
        if value:
            pieces.append(f"{prop}={value}")
    return " | ".join(str(piece) for piece in pieces)


def inspect_asset(asset_path):
    asset = unreal.load_asset(asset_path)
    if asset is None:
        unreal.log_warning(f"CODEX_VORTEX_AUDIT MISSING asset={asset_path}")
        return

    is_function = isinstance(asset, unreal.MaterialFunctionInterface)
    if is_function:
        expressions = list(unreal.MaterialEditingLibrary.get_material_function_expressions(asset))
        get_inputs = lambda expression: list(
            unreal.MaterialEditingLibrary.get_inputs_for_material_function_expression(asset, expression)
        )
    else:
        expressions = list(unreal.MaterialEditingLibrary.get_material_expressions(asset))
        get_inputs = lambda expression: list(
            unreal.MaterialEditingLibrary.get_inputs_for_material_expression(asset, expression)
        )

    unreal.log_warning(
        f"CODEX_VORTEX_AUDIT BEGIN asset={asset_path} class={asset.get_class().get_name()} count={len(expressions)}"
    )
    for expression in expressions:
        node_label = label(expression)
        lowered = node_label.lower()
        material_function = read_property(expression, "material_function")
        if material_function:
            lowered += " " + str(material_function).lower()
        if not any(token in lowered for token in TOKENS):
            continue

        input_labels = [label(source) for source in get_inputs(expression)]
        unreal.log_warning(
            f"CODEX_VORTEX_AUDIT NODE asset={asset_path} node={node_label} inputs={input_labels}"
        )
        if isinstance(expression, unreal.MaterialExpressionCustom):
            code = str(read_property(expression, "code") or "")
            custom_inputs = read_property(expression, "inputs") or []
            custom_input_names = []
            for custom_input in custom_inputs:
                try:
                    custom_input_names.append(str(custom_input.input_name))
                except Exception:
                    custom_input_names.append(str(custom_input))
            unreal.log_warning(
                f"CODEX_VORTEX_AUDIT CUSTOM asset={asset_path} inputs={custom_input_names} "
                f"mentions_vortex={'Vortex' in code or 'vortex' in code} "
                f"mentions_mesocyclone={'Mesocyclone' in code or 'mesocyclone' in code}"
            )

    unreal.log_warning(f"CODEX_VORTEX_AUDIT END asset={asset_path}")


for path in ASSETS:
    inspect_asset(path)

unreal.log_warning("CODEX_VORTEX_AUDIT COMPLETE_READ_ONLY=1")
