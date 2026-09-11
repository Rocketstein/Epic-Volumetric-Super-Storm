import unreal

material = unreal.load_asset("/SavageSuperStorm/Materials/M_SSS")
call = next(
    expr
    for expr in unreal.MaterialEditingLibrary.get_material_expressions(material)
    if isinstance(expr, unreal.MaterialExpressionMaterialFunctionCall)
    and expr.get_editor_property("material_function")
    and "MF_Erosion" in expr.get_editor_property("material_function").get_path_name()
)

unreal.log("CODEX_API_CALL_DIR|" + ",".join(name for name in dir(call) if not name.startswith("__")))
for name in (
    "update_material_function",
    "refresh_material_function_editor",
    "create_material_expression_in_function",
    "connect_material_expressions",
):
    fn = getattr(unreal.MaterialEditingLibrary, name)
    unreal.log(f"CODEX_API_DOC|{name}|{fn.__doc__}")
unreal.log(f"CODEX_API_CUSTOM_INPUT_DOC|{unreal.CustomInput.__doc__}")
unreal.log("CODEX_API_COMPLETE")
