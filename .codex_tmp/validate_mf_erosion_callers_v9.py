import unreal

function = unreal.load_asset("/SavageSuperStorm/Materials/Functions/MF_Erosion")
materials = list(unreal.MaterialEditingLibrary.get_materials_referencing_function(function))
checked = 0
for material in materials:
    for expr in unreal.MaterialEditingLibrary.get_material_expressions(material):
        if not isinstance(expr, unreal.MaterialExpressionMaterialFunctionCall):
            continue
        called = expr.get_editor_property("material_function")
        if called is None or called.get_path_name() != function.get_path_name():
            continue
        names = list(unreal.MaterialEditingLibrary.get_material_expression_input_names(expr))
        sources = list(
            unreal.MaterialEditingLibrary.get_inputs_for_material_expression(material, expr)
        )
        missing = [
            str(names[index])
            for index in range(len(names))
            if index >= len(sources) or sources[index] is None
        ]
        if missing:
            raise RuntimeError(
                f"{material.get_path_name()} has disconnected MF_Erosion inputs: {missing}"
            )
        checked += 1

unreal.log(
    f"CODEX_V9_CALLERS|SUCCESS|materials={len(materials)}|calls={checked}"
)
