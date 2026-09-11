import unreal

function = unreal.load_asset("/SavageSuperStorm/Materials/Functions/MF_Erosion")
vector = unreal.MaterialEditingLibrary.create_material_expression_in_function(
    function, unreal.MaterialExpressionVectorParameter, -1000, -1000
)
mask = unreal.MaterialEditingLibrary.create_material_expression_in_function(
    function, unreal.MaterialExpressionComponentMask, -800, -1000
)

unreal.log(
    "CODEX_CONNECT_VECTOR_OUTPUTS|"
    + ",".join(
        str(x)
        for x in unreal.MaterialEditingLibrary.get_material_expression_output_names(vector)
    )
)
unreal.log(
    "CODEX_CONNECT_MASK_INPUTS|"
    + ",".join(
        str(x)
        for x in unreal.MaterialEditingLibrary.get_material_expression_input_names(mask)
    )
)
unreal.log(
    "CODEX_CONNECT_MASK_OUTPUTS|"
    + ",".join(
        str(x)
        for x in unreal.MaterialEditingLibrary.get_material_expression_output_names(mask)
    )
)
for output_name in ("", "RGB", "RGBA"):
    for input_name in ("", "Input"):
        result = unreal.MaterialEditingLibrary.connect_material_expressions(
            vector, output_name, mask, input_name
        )
        unreal.log(
            f"CODEX_CONNECT_TRY|out={output_name!r}|in={input_name!r}|result={result}"
        )
unreal.log("CODEX_CONNECT_COMPLETE")
