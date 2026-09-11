import unreal


MATERIAL_PATH = "/SavageSuperStorm/Materials/M_SSS"
TARGET_FUNCTIONS = {"MF_Erosion", "MF_StormMain"}


def prop(obj, name, default=None):
    try:
        return obj.get_editor_property(name)
    except Exception:
        return default


def label(expr):
    if expr is None:
        return "<None>"
    bits = [expr.get_class().get_name(), expr.get_name()]
    for name in ("parameter_name", "name", "desc", "default_value", "constant"):
        value = prop(expr, name)
        if value not in (None, ""):
            bits.append(f"{name}={value}")
    function = prop(expr, "material_function")
    if function is not None:
        bits.append(f"function={function.get_path_name()}")
    return "|".join(str(x) for x in bits)


material = unreal.load_asset(MATERIAL_PATH)
if material is None:
    raise RuntimeError(f"Missing {MATERIAL_PATH}")

expressions = list(unreal.MaterialEditingLibrary.get_material_expressions(material))
for expr in expressions:
    if not isinstance(expr, unreal.MaterialExpressionMaterialFunctionCall):
        continue
    function = prop(expr, "material_function")
    if function is None or function.get_name() not in TARGET_FUNCTIONS:
        continue
    names = [
        str(x)
        for x in unreal.MaterialEditingLibrary.get_material_expression_input_names(expr)
    ]
    sources = list(
        unreal.MaterialEditingLibrary.get_inputs_for_material_expression(material, expr)
    )
    unreal.log_warning(f"CODEX_DENSITY_CALL|{label(expr)}|inputs={len(names)}|sources={len(sources)}")
    for index, name in enumerate(names):
        source = sources[index] if index < len(sources) else None
        unreal.log_warning(
            f"CODEX_DENSITY_INPUT|call={expr.get_name()}|index={index}|pin={name}|source={label(source)}"
        )

for expr in expressions:
    if isinstance(expr, unreal.MaterialExpressionCustom):
        unreal.log_warning(f"CODEX_DENSITY_CUSTOM|{label(expr)}")
        unreal.log_warning(f"CODEX_DENSITY_CODE|{expr.get_name()}|{prop(expr, 'code', '')}")
        inputs = prop(expr, "inputs", []) or []
        for index, custom_input in enumerate(inputs):
            unreal.log_warning(
                f"CODEX_DENSITY_CUSTOM_INPUT|custom={expr.get_name()}|index={index}|value={custom_input}"
            )

unreal.log_warning("CODEX_DENSITY_GRAPH_COMPLETE")
