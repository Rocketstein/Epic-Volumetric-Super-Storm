import unreal


material = unreal.load_asset("/SavageSuperStorm/Materials/M_A")
if material is None:
    raise RuntimeError("M_A missing")
usage = next(
    expression
    for expression in unreal.MaterialEditingLibrary.get_material_expressions(
        material
    )
    if expression.get_name() == "MaterialExpressionNamedRerouteUsage_0"
)
for name in dir(usage):
    lowered = name.lower()
    if any(
        token in lowered
        for token in ("caption", "declar", "route", "output", "name")
    ):
        try:
            value = getattr(usage, name)
        except Exception as error:
            value = f"<ERROR {error}>"
        unreal.log_warning(f"CODEX_REROUTE_API|{name}|{value}")
unreal.log_warning(f"CODEX_REROUTE_API|str|{usage}")
unreal.log_warning("CODEX_REROUTE_API_COMPLETE")
