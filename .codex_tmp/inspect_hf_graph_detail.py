import unreal


material = unreal.load_asset("/SavageSuperStorm/Materials/M_SSS")
if material is None:
    raise RuntimeError("Unable to load M_SSS")


def label(expression):
    pieces = [expression.get_class().get_name(), expression.get_name()]
    for prop in ("parameter_name", "input_name", "desc"):
        try:
            value = expression.get_editor_property(prop)
        except Exception:
            continue
        if value:
            pieces.append(f"{prop}={value}")
    return " | ".join(str(piece) for piece in pieces)


targets = {
    "MaterialExpressionMaterialFunctionCall_3",
    "MaterialExpressionMultiply_3",
    "MaterialExpressionMultiply_4",
    "MaterialExpressionMultiply_5",
    "MaterialExpressionNamedRerouteDeclaration_16",
}

for expression in unreal.MaterialEditingLibrary.get_material_expressions(material):
    is_reroute = isinstance(
        expression,
        (unreal.MaterialExpressionNamedRerouteDeclaration, unreal.MaterialExpressionNamedRerouteUsage),
    )
    if expression.get_name() not in targets and not is_reroute:
        continue

    unreal.log(f"CODEX_DETAIL_NODE={label(expression)}")
    for prop in (
        "name",
        "declaration",
        "a",
        "b",
        "function_inputs",
        "material_function",
        "material_expression_editor_x",
        "material_expression_editor_y",
    ):
        try:
            value = expression.get_editor_property(prop)
        except Exception:
            continue
        unreal.log(f"CODEX_DETAIL_PROP={expression.get_name()}:{prop}:{value}")
