import unreal


def prop(obj, name, default=None):
    if obj is None:
        return default
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
    return "|".join(str(x) for x in bits)


function = unreal.load_asset("/SavageSuperStorm/Materials/Functions/MF_Erosion")
if function is None:
    function = unreal.load_asset("/SavageSuperStorm/Materials/MF_Erosion")
if function is None:
    raise RuntimeError("MF_Erosion not found")

expressions = list(prop(function, "function_expressions", []) or [])
if not expressions:
    collection = prop(function, "expression_collection")
    expressions = list(prop(collection, "expressions", []) or [])

unreal.log_warning(f"CODEX_MF_EROSION|asset={function.get_path_name()}|expressions={len(expressions)}")
for expr in expressions:
    if isinstance(expr, unreal.MaterialExpressionCustom):
        unreal.log_warning(f"CODEX_MF_EROSION_CUSTOM|node={label(expr)}")
        unreal.log_warning(f"CODEX_MF_EROSION_CODE|node={expr.get_name()}|{prop(expr, 'code', '')}")
        inputs = list(prop(expr, "inputs", []) or [])
        for index, custom_input in enumerate(inputs):
            name = prop(custom_input, "input_name", f"input_{index}")
            source_input = prop(custom_input, "input")
            source = prop(source_input, "expression")
            unreal.log_warning(
                f"CODEX_MF_EROSION_INPUT|node={expr.get_name()}|index={index}|pin={name}|source={label(source)}"
            )

unreal.log_warning("CODEX_MF_EROSION_COMPLETE")
