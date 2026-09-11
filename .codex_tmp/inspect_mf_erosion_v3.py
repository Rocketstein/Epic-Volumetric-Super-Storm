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
        "input_name",
        "output_name",
        "parameter_name",
        "name",
        "desc",
        "default_value",
        "constant",
        "input_type",
        "sort_priority",
    ):
        value = prop(expr, name)
        if value not in (None, ""):
            bits.append(f"{name}={value}")
    return "|".join(str(x) for x in bits)


function = unreal.load_asset("/SavageSuperStorm/Materials/Functions/MF_Erosion")
if function is None:
    raise RuntimeError("MF_Erosion not found")

expressions = list(
    unreal.MaterialEditingLibrary.get_material_function_expressions(function)
)
unreal.log_warning(
    f"CODEX_MF3|asset={function.get_path_name()}|expressions={len(expressions)}"
)
for expr in expressions:
    unreal.log_warning(f"CODEX_MF3_EXPR|{label(expr)}")
    if isinstance(expr, unreal.MaterialExpressionCustom):
        unreal.log_warning(f"CODEX_MF3_CODE|{prop(expr, 'code', '')}")
        for index, custom_input in enumerate(list(prop(expr, "inputs", []) or [])):
            input_name = prop(custom_input, "input_name", f"input_{index}")
            expression_input = prop(custom_input, "input")
            source = prop(expression_input, "expression")
            output_index = prop(expression_input, "output_index", -1)
            unreal.log_warning(
                f"CODEX_MF3_CUSTOM_INPUT|index={index}|pin={input_name}|source={label(source)}|output_index={output_index}"
            )
        for index, custom_output in enumerate(list(prop(expr, "additional_outputs", []) or [])):
            unreal.log_warning(
                f"CODEX_MF3_CUSTOM_OUTPUT|index={index}|name={prop(custom_output, 'output_name')}|type={prop(custom_output, 'output_type')}"
            )

    names = [
        str(x)
        for x in unreal.MaterialEditingLibrary.get_material_expression_input_names(expr)
    ]
    sources = list(
        unreal.MaterialEditingLibrary.get_inputs_for_material_function_expression(
            function, expr
        )
    )
    for index, input_name in enumerate(names):
        source = sources[index] if index < len(sources) else None
        unreal.log_warning(
            f"CODEX_MF3_EDGE|node={expr.get_name()}|index={index}|pin={input_name}|source={label(source)}"
        )

unreal.log_warning("CODEX_MF3_COMPLETE")
