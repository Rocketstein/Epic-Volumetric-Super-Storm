import unreal


FUNCTION_PATH = "/SavageSuperStorm/Materials/Functions/MF_Erosion"
MATERIAL_PATH = "/SavageSuperStorm/Materials/M_SSS"

OLD_ANVIL_INPUTS = [
    "AnvilWindDirectionXY",
    "AnvilWindStretch",
    "AnvilWindSkew",
    "AnvilWarpStart01",
    "AnvilWarpEnd01",
]

NEW_TWIST_DEFAULTS = {
    "OuterBrimRadiusScale": 1.35,
    "ShapeTwistRadians": 2.97,
    "SpiralTwistPower": 1.0,
    "SpiralFullStrengthRadius01": 0.08,
    "SpiralZeroStrengthRadius01": 0.92,
}

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
    "OuterBrimRadiusScale",
    "ShapeTwistRadians",
    "SpiralTwistPower",
    "SpiralFullStrengthRadius01",
    "SpiralZeroStrengthRadius01",
    "AnvilStrength",
    "AnvilTypeBias",
    "AnvilCeilingFade01",
    "DensityGamma",
    "HFStrength",
]

EXPECTED_OLD_ORDER = CUSTOM_INPUT_ORDER[:23] + [
    "AnvilStrength",
    "AnvilTypeBias",
    "AnvilCeilingFade01",
] + OLD_ANVIL_INPUTS + [
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
    OuterBrimRadiusScale, ShapeTwistRadians, SpiralTwistPower,
    SpiralFullStrengthRadius01, SpiralZeroStrengthRadius01,
    AnvilStrength, AnvilTypeBias, AnvilCeilingFade01,
    DensityGamma, HFStrength, OutCoarseDensity);"""


def prop(obj, name, default=None):
    try:
        return obj.get_editor_property(name)
    except Exception:
        return default


def input_sources(owner, expr, is_function):
    names = [
        str(value)
        for value in unreal.MaterialEditingLibrary.get_material_expression_input_names(expr)
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


def find_by_parameter(expressions, name, cls):
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
    raise RuntimeError("Required MF_Erosion or M_SSS asset was not found")
function.modify()
material.modify()

function_expressions = list(
    unreal.MaterialEditingLibrary.get_material_function_expressions(function)
)
material_expressions = list(
    unreal.MaterialEditingLibrary.get_material_expressions(material)
)
custom_nodes = [
    expr for expr in function_expressions
    if isinstance(expr, unreal.MaterialExpressionCustom)
]
if len(custom_nodes) != 1:
    raise RuntimeError(f"Expected one Custom node, found {len(custom_nodes)}")
custom = custom_nodes[0]

calls = []
for expr in material_expressions:
    if not isinstance(expr, unreal.MaterialExpressionMaterialFunctionCall):
        continue
    called = prop(expr, "material_function")
    if called is not None and called.get_path_name() == function.get_path_name():
        calls.append(expr)
if len(calls) != 1:
    raise RuntimeError(f"Expected one M_SSS MF_Erosion call, found {len(calls)}")
function_call = calls[0]

old_custom_edges = input_sources(function, custom, True)
if list(old_custom_edges.keys()) != EXPECTED_OLD_ORDER:
    raise RuntimeError(f"Unexpected pre-v10 Custom order: {list(old_custom_edges.keys())}")
old_call_edges = input_sources(material, function_call, False)
if len(old_call_edges) != 21:
    raise RuntimeError(f"Expected 21 external call inputs, found {len(old_call_edges)}")

# Reuse the four retired scalar nodes as four existing-twist parameters to keep
# graph layout stable, then create the fifth. The retired vector and RG mask are
# deleted because rotation-only Anvil has no wind-direction input.
renames = {
    "AnvilWindStretch": "OuterBrimRadiusScale",
    "AnvilWindSkew": "ShapeTwistRadians",
    "AnvilWarpStart01": "SpiralTwistPower",
    "AnvilWarpEnd01": "SpiralFullStrengthRadius01",
}
for old_name, new_name in renames.items():
    node = find_by_parameter(
        function_expressions, old_name, unreal.MaterialExpressionScalarParameter
    )
    if node is None:
        raise RuntimeError(f"Retired scalar parameter was not found: {old_name}")
    node.modify()
    node.set_editor_property("parameter_name", new_name)
    node.set_editor_property("default_value", float(NEW_TWIST_DEFAULTS[new_name]))

wind_parameter = find_by_parameter(
    function_expressions,
    "AnvilWindDirectionXY",
    unreal.MaterialExpressionVectorParameter,
)
if wind_parameter is None:
    raise RuntimeError("Retired AnvilWindDirectionXY parameter was not found")
wind_masks = [
    expr for expr in function_expressions
    if isinstance(expr, unreal.MaterialExpressionComponentMask)
    and str(prop(expr, "desc", "")) == "Nubis v9 AnvilWindDirectionXY RG"
]
if len(wind_masks) != 1:
    raise RuntimeError(f"Expected one retired Anvil wind RG mask, found {len(wind_masks)}")
unreal.MaterialEditingLibrary.delete_material_expression_in_function(
    function, wind_masks[0]
)
unreal.MaterialEditingLibrary.delete_material_expression_in_function(
    function, wind_parameter
)

function_expressions = list(
    unreal.MaterialEditingLibrary.get_material_function_expressions(function)
)
custom_x, custom_y = unreal.MaterialEditingLibrary.get_material_expression_node_position(
    custom
)
zero_radius = find_by_parameter(
    function_expressions,
    "SpiralZeroStrengthRadius01",
    unreal.MaterialExpressionScalarParameter,
)
if zero_radius is None:
    zero_radius = unreal.MaterialEditingLibrary.create_material_expression_in_function(
        function,
        unreal.MaterialExpressionScalarParameter,
        custom_x - 420,
        custom_y + 1440,
    )
zero_radius.modify()
zero_radius.set_editor_property("parameter_name", "SpiralZeroStrengthRadius01")
zero_radius.set_editor_property(
    "default_value", float(NEW_TWIST_DEFAULTS["SpiralZeroStrengthRadius01"])
)

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
    "OuterBrimRadiusScale",
    "ShapeTwistRadians",
    "SpiralTwistPower",
    "SpiralFullStrengthRadius01",
    "SpiralZeroStrengthRadius01",
    "AnvilStrength",
    "AnvilTypeBias",
    "AnvilCeilingFade01",
}
for name in CUSTOM_INPUT_ORDER:
    source = (
        scalar_parameters.get(name)
        if name in internal_scalar_names
        else function_inputs.get(name)
    )
    if source is None:
        raise RuntimeError(f"Custom source is missing: {name}")
    if not unreal.MaterialEditingLibrary.connect_material_expressions(
        source, "", custom, name
    ):
        raise RuntimeError(f"Failed to connect Custom input: {name}")

unreal.MaterialEditingLibrary.update_material_function(function)
function_call.modify()
function_call.set_material_function(function)
for name, source in old_call_edges.items():
    if source is None:
        raise RuntimeError(f"Pre-v10 M_SSS input was unconnected: {name}")
    if not unreal.MaterialEditingLibrary.connect_material_expressions(
        source, "", function_call, name
    ):
        raise RuntimeError(f"Failed to restore M_SSS input: {name}")

unreal.MaterialEditingLibrary.refresh_material_function_editor(function)
unreal.MaterialEditingLibrary.recompile_material(material)

custom_edges = input_sources(function, custom, True)
if list(custom_edges.keys()) != CUSTOM_INPUT_ORDER:
    raise RuntimeError(f"v10 Custom input order mismatch: {list(custom_edges.keys())}")
missing_custom = [name for name, source in custom_edges.items() if source is None]
if missing_custom:
    raise RuntimeError(f"Unconnected v10 Custom inputs: {missing_custom}")
call_edges = input_sources(material, function_call, False)
if list(call_edges.keys()) != list(old_call_edges.keys()):
    raise RuntimeError("M_SSS external function interface changed unexpectedly")
missing_call = [name for name, source in call_edges.items() if source is None]
if missing_call:
    raise RuntimeError(f"Unconnected M_SSS inputs: {missing_call}")

function_expressions = list(
    unreal.MaterialEditingLibrary.get_material_function_expressions(function)
)
remaining_names = {
    str(prop(expr, "parameter_name", ""))
    for expr in function_expressions
    if isinstance(
        expr,
        (unreal.MaterialExpressionScalarParameter,
         unreal.MaterialExpressionVectorParameter),
    )
}
retired = sorted(set(OLD_ANVIL_INPUTS) & remaining_names)
if retired:
    raise RuntimeError(f"Retired Anvil parameters remain: {retired}")
missing_twist = sorted(set(NEW_TWIST_DEFAULTS) - remaining_names)
if missing_twist:
    raise RuntimeError(f"Required twist parameters are missing: {missing_twist}")

if not unreal.EditorAssetLibrary.save_loaded_asset(function, False):
    raise RuntimeError("Failed to save MF_Erosion")
if not unreal.EditorAssetLibrary.save_loaded_asset(material, False):
    raise RuntimeError("Failed to save M_SSS")

unreal.log(
    "CODEX_V10_MATERIAL|SUCCESS|"
    f"custom_inputs={len(custom_edges)}|"
    f"caller_inputs={len(call_edges)}|"
    f"twist={','.join(NEW_TWIST_DEFAULTS.keys())}"
)
unreal.log("CODEX_V10_MATERIAL_COMPLETE")
