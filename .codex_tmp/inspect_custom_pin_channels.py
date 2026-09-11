import unreal


def prop(obj, name, default=None):
    try:
        return obj.get_editor_property(name)
    except Exception:
        try:
            return getattr(obj, name)
        except Exception:
            return default


def dump_input(prefix, value):
    fields = {}
    for name in (
        "expression",
        "output_index",
        "input_name",
        "mask",
        "mask_r",
        "mask_g",
        "mask_b",
        "mask_a",
    ):
        fields[name] = prop(value, name, "<missing>")
    expression = fields["expression"]
    if expression not in (None, "<missing>"):
        fields["expression"] = expression.get_path_name()
    unreal.log_warning(
        prefix + "|" + "|".join(f"{name}={fields[name]}" for name in fields)
    )


material = unreal.load_asset("/SavageSuperStorm/Materials/M_SSS")
expressions = list(unreal.MaterialEditingLibrary.get_material_expressions(material))

for expr in expressions:
    if isinstance(expr, unreal.MaterialExpressionCustom):
        for index, custom_input in enumerate(prop(expr, "inputs", []) or []):
            name = prop(custom_input, "input_name", "<missing>")
            unreal.log_warning(
                f"CODEX_PIN_CUSTOM|node={expr.get_name()}|index={index}|name={name}"
            )
            dump_input(
                f"CODEX_PIN_CUSTOM_LINK|node={expr.get_name()}|index={index}|name={name}",
                prop(custom_input, "input"),
            )

for expr in expressions:
    if isinstance(expr, unreal.MaterialExpressionNamedRerouteDeclaration):
        name = str(prop(expr, "name", ""))
        if name not in {"MinH", "MaxH", "CloudType", "Bottomtype", "HLocal"}:
            continue
        unreal.log_warning(f"CODEX_PIN_REROUTE|node={expr.get_name()}|name={name}")
        dump_input(
            f"CODEX_PIN_REROUTE_LINK|node={expr.get_name()}|name={name}",
            prop(expr, "input"),
        )

unreal.log_warning("CODEX_PIN_COMPLETE")
