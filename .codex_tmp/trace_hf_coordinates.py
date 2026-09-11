import unreal


material = unreal.load_asset("/SavageSuperStorm/Materials/M_SSS")
if material is None:
    raise RuntimeError("Unable to load M_SSS")

expressions = list(unreal.MaterialEditingLibrary.get_material_expressions(material))
by_name = {expression.get_name(): expression for expression in expressions}


def label(expression):
    pieces = [expression.get_class().get_name(), expression.get_name()]
    for prop in ("parameter_name", "name", "desc"):
        try:
            value = expression.get_editor_property(prop)
        except Exception:
            continue
        if value:
            pieces.append(f"{prop}={value}")
    for prop in ("default_value", "r", "constant"):
        try:
            value = expression.get_editor_property(prop)
        except Exception:
            continue
        pieces.append(f"{prop}={value}")
    return " | ".join(str(piece) for piece in pieces)


def trace_upstream(expression, depth, visited):
    unreal.log(f"CODEX_COORD_NODE depth={depth}: {label(expression)}")
    if depth >= 10 or expression in visited:
        return
    visited.add(expression)
    inputs = list(unreal.MaterialEditingLibrary.get_inputs_for_material_expression(material, expression))
    for source in inputs:
        unreal.log(f"CODEX_COORD_EDGE={label(source)} -> {label(expression)}")
        trace_upstream(source, depth + 1, visited)


for target_name in (
    "MaterialExpressionMultiply_4",
    "MaterialExpressionMultiply_5",
    "MaterialExpressionMultiply_3",
):
    target = by_name.get(target_name)
    if target is None:
        raise RuntimeError(f"Missing {target_name}")
    unreal.log(f"CODEX_COORD_BEGIN={target_name}")
    trace_upstream(target, 0, set())
    unreal.log(f"CODEX_COORD_END={target_name}")
