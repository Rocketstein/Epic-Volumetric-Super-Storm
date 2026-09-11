import unreal


def expression_label(expression):
    pieces = [expression.get_class().get_name(), expression.get_name()]
    for prop in ("parameter_name", "input_name", "desc"):
        try:
            value = expression.get_editor_property(prop)
        except Exception:
            continue
        if value:
            pieces.append(f"{prop}={value}")
    return " | ".join(str(piece) for piece in pieces)


def inspect_graph(asset_path, is_function=False):
    asset = unreal.load_asset(asset_path)
    if asset is None:
        raise RuntimeError(f"Unable to load {asset_path}")

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

    unreal.log(f"CODEX_GRAPH_BEGIN={asset_path};expressions={len(expressions)}")
    consumers = {expression: [] for expression in expressions}
    for consumer in expressions:
        for source in get_inputs(consumer):
            consumers.setdefault(source, []).append(consumer)

    interesting = []
    for expression in expressions:
        label = expression_label(expression)
        lowered = label.lower()
        if any(token in lowered for token in ("hf", "erosion", "worley", "custom")):
            interesting.append(expression)
            unreal.log(f"CODEX_NODE={label}")
            if isinstance(expression, unreal.MaterialExpressionScalarParameter):
                value = float(expression.get_editor_property("default_value"))
                unreal.log(f"CODEX_SCALAR={expression.get_editor_property('parameter_name')}:{value:.9g}")

    frontier = list(interesting)
    visited = set(interesting)
    for _ in range(5):
        next_frontier = []
        for source in frontier:
            for consumer in consumers.get(source, []):
                unreal.log(f"CODEX_EDGE={expression_label(source)} -> {expression_label(consumer)}")
                if consumer not in visited:
                    visited.add(consumer)
                    next_frontier.append(consumer)
        frontier = next_frontier

    unreal.log(f"CODEX_GRAPH_END={asset_path}")


inspect_graph("/SavageSuperStorm/Materials/M_SSS")
inspect_graph("/SavageSuperStorm/Materials/Functions/MF_Erosion", is_function=True)
