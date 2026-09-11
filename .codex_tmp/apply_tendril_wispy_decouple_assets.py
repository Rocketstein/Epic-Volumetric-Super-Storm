import unreal


FUNCTION_PATH = "/SavageSuperStorm/Materials/Functions/MF_Erosion"
MATERIAL_PATH = "/SavageSuperStorm/Materials/M_SSS"
REMOVED_MASK = "SpiralTendrilMask"
REMOVED_STRENGTH = "SpiralTendrilStrength"


CUSTOM_INPUT_ORDER = [
    "BottomProfileTex",
    "TopProfileTex",
    "LFNoiseTex",
    "HFNoiseTex",
    "CurlNoiseTex",
    "Coverage",
    "HLocal",
    "HeightInside",
    "BaseBottomType",
    "TopType",
    "StormStencil",
    "WorldPosition",
    "StormCenterRadius",
    "LFUVW",
    "HFUVW",
    "CurlUVW",
    "LFUnitsPerBodyRadius",
    "HFUnitsPerBodyRadius",
    "CurlUnitsPerBodyRadius",
    "CurlTiling",
    "CurlDisplacementUVW",
    "StormLifecycleControl",
    "RotationSign",
    "AnvilStrength",
    "AnvilTypeBias",
    "AnvilCeilingFade01",
    "AnvilWindDirectionXY",
    "AnvilWindStretch",
    "AnvilWindSkew",
    "AnvilWarpStart01",
    "AnvilWarpEnd01",
    "DensityGamma",
    "HFStrength",
]


CUSTOM_CODE = """return StormEvaluateMaterialDensity(
    BottomProfileTex, BottomProfileTexSampler,
    TopProfileTex, TopProfileTexSampler,
    LFNoiseTex, LFNoiseTexSampler,
    HFNoiseTex, HFNoiseTexSampler,
    CurlNoiseTex, CurlNoiseTexSampler,
    Coverage, HLocal, HeightInside, BaseBottomType, TopType,
    StormStencil, WorldPosition, StormCenterRadius, LFUVW, HFUVW, CurlUVW,
    LFUnitsPerBodyRadius, HFUnitsPerBodyRadius,
    CurlUnitsPerBodyRadius, CurlTiling, CurlDisplacementUVW,
    StormLifecycleControl, RotationSign,
    AnvilStrength, AnvilTypeBias, AnvilCeilingFade01,
    AnvilWindDirectionXY, AnvilWindStretch, AnvilWindSkew,
    AnvilWarpStart01, AnvilWarpEnd01,
    DensityGamma, HFStrength, OutCoarseDensity);"""


def prop(obj, name, default=None):
    try:
        return obj.get_editor_property(name)
    except Exception:
        return default


def input_sources(owner, expr, is_function):
    names = [
        str(x)
        for x in unreal.MaterialEditingLibrary.get_material_expression_input_names(expr)
    ]
    if is_function:
        sources = list(
            unreal.MaterialEditingLibrary.get_inputs_for_material_function_expression(
                owner, expr
            )
        )
    else:
        sources = list(
            unreal.MaterialEditingLibrary.get_inputs_for_material_expression(owner, expr)
        )
    return {
        name: sources[index] if index < len(sources) else None
        for index, name in enumerate(names)
    }


def find_parameter(expressions, name, cls):
    matches = [
        expr
        for expr in expressions
        if isinstance(expr, cls)
        and str(prop(expr, "parameter_name", "")) == name
    ]
    if len(matches) > 1:
        raise RuntimeError(f"Duplicate parameter {name}: {len(matches)}")
    return matches[0] if matches else None


function = unreal.load_asset(FUNCTION_PATH)
material = unreal.load_asset(MATERIAL_PATH)
if function is None or material is None:
    raise RuntimeError("Required M_SSS or MF_Erosion asset was not found")

function.modify()
material.modify()

function_expressions = list(
    unreal.MaterialEditingLibrary.get_material_function_expressions(function)
)
material_expressions = list(
    unreal.MaterialEditingLibrary.get_material_expressions(material)
)

custom_nodes = [
    expr
    for expr in function_expressions
    if isinstance(expr, unreal.MaterialExpressionCustom)
]
if len(custom_nodes) != 1:
    raise RuntimeError(f"Expected one MF_Erosion Custom node, found {len(custom_nodes)}")
custom = custom_nodes[0]

function_calls = []
for expr in material_expressions:
    if not isinstance(expr, unreal.MaterialExpressionMaterialFunctionCall):
        continue
    called = prop(expr, "material_function")
    if called is not None and called.get_path_name() == function.get_path_name():
        function_calls.append(expr)
if len(function_calls) != 1:
    raise RuntimeError(f"Expected one M_SSS MF_Erosion call, found {len(function_calls)}")
function_call = function_calls[0]

old_call_sources = input_sources(material, function_call, False)
if REMOVED_MASK not in old_call_sources:
    raise RuntimeError("M_SSS caller did not contain the expected Tendril mask input")
old_custom_sources = input_sources(function, custom, True)
if list(old_custom_sources.keys()) != (
    CUSTOM_INPUT_ORDER[:11]
    + [REMOVED_MASK, REMOVED_STRENGTH]
    + CUSTOM_INPUT_ORDER[11:]
):
    raise RuntimeError(
        f"Unexpected pre-edit Custom inputs: {list(old_custom_sources.keys())}"
    )

# Delete the two retired MF_Erosion expressions. ShapeRT2.G remains authored in
# M_SSS for bake/debug inspection, but it no longer enters this function call.
mask_inputs = [
    expr
    for expr in function_expressions
    if isinstance(expr, unreal.MaterialExpressionFunctionInput)
    and str(prop(expr, "input_name", "")) == REMOVED_MASK
]
if len(mask_inputs) != 1:
    raise RuntimeError(f"Expected one {REMOVED_MASK} FunctionInput, found {len(mask_inputs)}")
strength = find_parameter(
    function_expressions,
    REMOVED_STRENGTH,
    unreal.MaterialExpressionScalarParameter,
)
if strength is None:
    raise RuntimeError(f"Expected parameter was not found: {REMOVED_STRENGTH}")
unreal.MaterialEditingLibrary.delete_material_expression_in_function(
    function, mask_inputs[0]
)
unreal.MaterialEditingLibrary.delete_material_expression_in_function(function, strength)

function_expressions = list(
    unreal.MaterialEditingLibrary.get_material_function_expressions(function)
)
function_inputs = {
    str(prop(expr, "input_name", "")): expr
    for expr in function_expressions
    if isinstance(expr, unreal.MaterialExpressionFunctionInput)
}
scalar_parameters = {
    str(prop(expr, "parameter_name", "")): expr
    for expr in function_expressions
    if isinstance(expr, unreal.MaterialExpressionScalarParameter)
}
wind_masks = [
    expr
    for expr in function_expressions
    if isinstance(expr, unreal.MaterialExpressionComponentMask)
    and str(prop(expr, "desc", "")) == "Nubis v9 AnvilWindDirectionXY RG"
]
if len(wind_masks) != 1:
    raise RuntimeError(f"Expected one Anvil wind RG mask, found {len(wind_masks)}")

new_custom_inputs = []
for name in CUSTOM_INPUT_ORDER:
    custom_input = unreal.CustomInput()
    custom_input.set_editor_property("input_name", name)
    new_custom_inputs.append(custom_input)
custom.modify()
custom.set_editor_property("inputs", new_custom_inputs)
custom.set_editor_property("code", CUSTOM_CODE)

internal_scalar_names = {
    "LFUnitsPerBodyRadius",
    "HFUnitsPerBodyRadius",
    "CurlUnitsPerBodyRadius",
    "RotationSign",
    "AnvilStrength",
    "AnvilTypeBias",
    "AnvilCeilingFade01",
    "AnvilWindStretch",
    "AnvilWindSkew",
    "AnvilWarpStart01",
    "AnvilWarpEnd01",
}
for name in CUSTOM_INPUT_ORDER:
    if name == "AnvilWindDirectionXY":
        source = wind_masks[0]
    elif name in internal_scalar_names:
        source = scalar_parameters.get(name)
    else:
        source = function_inputs.get(name)
    if source is None:
        raise RuntimeError(f"Source expression is missing for Custom input: {name}")
    if not unreal.MaterialEditingLibrary.connect_material_expressions(
        source, "", custom, name
    ):
        raise RuntimeError(f"Failed to connect Custom input: {name}")

unreal.MaterialEditingLibrary.update_material_function(function)
function_call.modify()
function_call.set_material_function(function)
for name, source in old_call_sources.items():
    if name == REMOVED_MASK or source is None:
        continue
    if not unreal.MaterialEditingLibrary.connect_material_expressions(
        source, "", function_call, name
    ):
        raise RuntimeError(f"Failed to restore M_SSS call input: {name}")

unreal.MaterialEditingLibrary.refresh_material_function_editor(function)
unreal.MaterialEditingLibrary.recompile_material(material)

# Verify every remaining edge before saving either binary asset.
function_edges = input_sources(function, custom, True)
if list(function_edges.keys()) != CUSTOM_INPUT_ORDER:
    raise RuntimeError(f"Custom input order mismatch: {list(function_edges.keys())}")
missing_custom = [name for name, source in function_edges.items() if source is None]
if missing_custom:
    raise RuntimeError(f"Unconnected Custom inputs: {missing_custom}")

call_edges = input_sources(material, function_call, False)
if REMOVED_MASK in call_edges:
    raise RuntimeError("M_SSS still exposes the Tendril mask input")
missing_call = [name for name, source in call_edges.items() if source is None]
if missing_call:
    raise RuntimeError(f"Unconnected M_SSS function inputs: {missing_call}")
if len(call_edges) != 21:
    raise RuntimeError(f"Expected 21 M_SSS inputs, found {len(call_edges)}")

remaining_function_inputs = [
    str(prop(expr, "input_name", ""))
    for expr in unreal.MaterialEditingLibrary.get_material_function_expressions(function)
    if isinstance(expr, unreal.MaterialExpressionFunctionInput)
]
remaining_parameters = [
    str(prop(expr, "parameter_name", ""))
    for expr in unreal.MaterialEditingLibrary.get_material_function_expressions(function)
    if isinstance(expr, unreal.MaterialExpressionScalarParameter)
]
if REMOVED_MASK in remaining_function_inputs:
    raise RuntimeError("Retired Tendril mask FunctionInput still exists")
if REMOVED_STRENGTH in remaining_parameters:
    raise RuntimeError("Retired Tendril strength parameter still exists")

if not unreal.EditorAssetLibrary.save_loaded_asset(function, False):
    raise RuntimeError("Failed to save MF_Erosion")
if not unreal.EditorAssetLibrary.save_loaded_asset(material, False):
    raise RuntimeError("Failed to save M_SSS")

unreal.log(
    "CODEX_DECOUPLE_APPLY|SUCCESS|"
    f"custom_inputs={len(function_edges)}|"
    f"caller_inputs={len(call_edges)}|"
    f"caller_pins={','.join(call_edges.keys())}"
)
unreal.log("CODEX_DECOUPLE_APPLY_COMPLETE")
