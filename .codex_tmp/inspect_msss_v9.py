import unreal


def prop(obj, name, default=None):
    try:
        return obj.get_editor_property(name)
    except Exception:
        return default


def label(expr):
    if expr is None:
        return "<None>"
    bits = [expr.get_class().get_name(), expr.get_name()]
    for name in (
        "parameter_name",
        "declaration_name",
        "name",
        "desc",
        "default_value",
        "constant",
    ):
        value = prop(expr, name)
        if value not in (None, ""):
            bits.append(f"{name}={value}")
    x, y = unreal.MaterialEditingLibrary.get_material_expression_node_position(expr)
    bits.append(f"xy={x},{y}")
    return "|".join(str(x) for x in bits)


material = unreal.load_asset("/SavageSuperStorm/Materials/M_SSS")
if material is None:
    raise RuntimeError("M_SSS not found")

expressions = list(unreal.MaterialEditingLibrary.get_material_expressions(material))
unreal.log(f"CODEX_MSSS9|expressions={len(expressions)}")

relevant = {
    "LFNoiseWorldSize",
    "HFNoiseWorldSize",
    "CurlNoiseWorldSize",
    "LFUnitsPerBodyRadius",
    "HFUnitsPerBodyRadius",
    "CurlUnitsPerBodyRadius",
    "SpiralTendrilStrength",
    "RotationSign",
    "AnvilWindDirectionXY",
    "AnvilWindStretch",
    "AnvilWindSkew",
    "AnvilWarpStart01",
    "AnvilWarpEnd01",
    "SpiralCarveStrength",
    "SpiralCarveCoverageFloor",
    "FormationSwirlSign",
}

for expr in expressions:
    parameter_name = str(prop(expr, "parameter_name", ""))
    if parameter_name in relevant:
        unreal.log(f"CODEX_MSSS9_PARAM|{label(expr)}")

for expr in expressions:
    names = [
        str(x)
        for x in unreal.MaterialEditingLibrary.get_material_expression_input_names(expr)
    ]
    sources = list(
        unreal.MaterialEditingLibrary.get_inputs_for_material_expression(material, expr)
    )
    if isinstance(expr, unreal.MaterialExpressionMaterialFunctionCall):
        function = prop(expr, "material_function")
        if function and "MF_Erosion" in function.get_path_name():
            unreal.log(f"CODEX_MSSS9_CALL|{label(expr)}|function={function.get_path_name()}")
            for index, input_name in enumerate(names):
                source = sources[index] if index < len(sources) else None
                unreal.log(
                    f"CODEX_MSSS9_CALL_INPUT|index={index}|pin={input_name}|source={label(source)}"
                )

    for index, input_name in enumerate(names):
        source = sources[index] if index < len(sources) else None
        source_parameter = str(prop(source, "parameter_name", ""))
        if source_parameter in relevant:
            unreal.log(
                f"CODEX_MSSS9_PARAM_EDGE|source={label(source)}|dest={label(expr)}|pin={input_name}|index={index}"
            )

for expr in expressions:
    if isinstance(
        expr,
        (
            unreal.MaterialExpressionNamedRerouteDeclaration,
            unreal.MaterialExpressionNamedRerouteUsage,
        ),
    ):
        text = label(expr)
        if any(
            key in text
            for key in (
                "Coverage",
                "BottomType",
                "CloudType",
                "ShapeRT2",
                "WorldPosition",
                "StormCenterRadius",
                "LFPos",
                "HFPos",
                "CurlPos",
            )
        ):
            unreal.log(f"CODEX_MSSS9_REROUTE|{text}")

unreal.log("CODEX_MSSS9_COMPLETE")
