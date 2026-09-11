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
    for name in ("parameter_name", "name", "desc", "default_value", "constant"):
        value = prop(expr, name)
        if value not in (None, ""):
            bits.append(f"{name}={value}")
    return "|".join(str(x) for x in bits)


material = unreal.load_asset("/SavageSuperStorm/Materials/M_SSS")
expressions = list(unreal.MaterialEditingLibrary.get_material_expressions(material))

for expr in expressions:
    if isinstance(expr, unreal.MaterialExpressionCustom):
        names = [
            str(x)
            for x in unreal.MaterialEditingLibrary.get_material_expression_input_names(expr)
        ]
        sources = list(
            unreal.MaterialEditingLibrary.get_inputs_for_material_expression(material, expr)
        )
        unreal.log_warning(f"CODEX_ROUTE_CUSTOM|node={label(expr)}")
        for index, name in enumerate(names):
            source = sources[index] if index < len(sources) else None
            unreal.log_warning(
                f"CODEX_ROUTE_CUSTOM_INPUT|node={expr.get_name()}|index={index}|pin={name}|source={label(source)}"
            )

for expr in expressions:
    if not isinstance(
        expr,
        (unreal.MaterialExpressionNamedRerouteDeclaration, unreal.MaterialExpressionNamedRerouteUsage),
    ):
        continue
    sources = list(
        unreal.MaterialEditingLibrary.get_inputs_for_material_expression(material, expr)
    )
    unreal.log_warning(
        f"CODEX_ROUTE_REROUTE|node={label(expr)}|declaration={label(prop(expr, 'declaration'))}|sources={len(sources)}"
    )
    for index, source in enumerate(sources):
        unreal.log_warning(
            f"CODEX_ROUTE_REROUTE_INPUT|node={expr.get_name()}|index={index}|source={label(source)}"
        )

unreal.log_warning("CODEX_ROUTE_COMPLETE")
