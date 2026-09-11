import math
import unreal


FUNCTION_PATH = "/SavageSuperStorm/Materials/Functions/MF_Erosion"
MATERIAL_PATH = "/SavageSuperStorm/Materials/M_SSS"
MERGE_PATH = "/SavageSuperStorm/Materials/Functions/MF_Erosion_Merge"
LATEST_INCLUDE = (
    "/SavageSuperStormShaders/Public/StormDensityLatestMDRAdapter.ush"
)

OLD_CUSTOM_ORDER = [
    "BottomProfileTex",
    "TopProfileTex",
    "AnvilProfileTex",
    "LFNoiseTex",
    "HFNoiseTex",
    "CurlNoiseTex",
    "Coverage",
    "HLocal",
    "HeightInside",
    "CloudLayerHeight01",
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
    "AnvilCoverageField",
    "ShapeTwistRadians",
    "SpiralTwistPower",
    "SpiralFullStrengthRadius01",
    "SpiralZeroStrengthRadius01",
    "AnvilStrength",
    "AnvilAnchorHeight01",
    "AnvilDepth01",
    "DensityGamma",
    "HFStrength",
    "FlowOffsetA",
    "FlowOffsetB",
    "FlowWeightA",
]

OLD_CALL_ORDER = [
    "BottomProfileTex",
    "TopProfileTex",
    "AnvilProfileTex",
    "LFNoiseTex",
    "HFNoiseTex",
    "CurlNoiseTex",
    "Coverage",
    "HLocal",
    "HeightInside",
    "CloudLayerHeight01",
    "BaseBottomType",
    "TopType",
    "StormStencil",
    "WorldPosition",
    "StormCenterRadius",
    "LFUVW",
    "HFUVW",
    "CurlUVW",
    "CurlTiling",
    "CurlDisplacementUVW",
    "StormLifecycleControl",
    "DensityGamma",
    "HFStrength",
    "FlowOffsetA",
    "FlowOffsetB",
    "FlowWeightA",
    "AnvilCoverageField",
]

MDR_INPUTS = [
    "MDRBodyCoord",
    "MDRBoundaries0",
    "MDRBoundaries1",
    "MDRSpeeds0",
    "MDRSpeeds1",
    "MDRPhases0",
    "MDRPhases1",
    "MDRSkews0",
    "MDRSkews1",
    "MDRControl0",
    "MDRControl1",
]

CUSTOM_INPUT_ORDER = OLD_CUSTOM_ORDER + MDR_INPUTS
UPDATED_CALL_ORDER = OLD_CALL_ORDER[:-1] + MDR_INPUTS + [
    "AnvilCoverageField"
]

RING_PARAMETERS = {
    "StormMDRBoundaries0": ((0.10, 0.22, 0.36, 0.54), (-1216, -4272)),
    "StormMDRBoundaries1": ((0.74, 0.04, 1.00, 0.08), (-992, -4272)),
    "StormMDRSpeeds0": ((0.0, 0.0, 0.0, 0.0), (-1216, -4032)),
    "StormMDRSpeeds1": ((0.0, 0.0, 1.0, 0.0), (-992, -4032)),
    "StormMDRPhases0": ((0.0, 0.0, 0.0, 0.0), (-768, -4272)),
    "StormMDRPhases1": ((0.0, 0.0, 0.0, 0.0), (-544, -4272)),
    "StormMDRSkews0": ((0.0, 0.0, 0.0, 0.0), (-768, -4032)),
    "StormMDRSkews1": ((0.0, 0.0, 0.0, 0.62), (-544, -4032)),
    "StormMDRControl0": ((0.08, 0.0, 1.0, 0.0), (-320, -4032)),
    "StormMDRControl1": ((0.0, 0.0, 0.0, 1.0), (-96, -4032)),
}

OLD_CUSTOM_CODE = """return StormEvaluateMaterialDensity(
    BottomProfileTex, BottomProfileTexSampler,
    TopProfileTex, TopProfileTexSampler,
    AnvilProfileTex, AnvilProfileTexSampler,
    LFNoiseTex, LFNoiseTexSampler,
    HFNoiseTex, HFNoiseTexSampler,
    CurlNoiseTex, CurlNoiseTexSampler,
    Coverage, HLocal, HeightInside, CloudLayerHeight01,
    BaseBottomType, TopType, StormStencil,
    WorldPosition, StormCenterRadius,
    LFUVW, HFUVW,
    FlowOffsetA, FlowOffsetB, FlowWeightA,
    CurlUVW,
    LFUnitsPerBodyRadius, HFUnitsPerBodyRadius,
    CurlUnitsPerBodyRadius, CurlTiling, CurlDisplacementUVW,
    StormLifecycleControl, RotationSign,
    OuterBrimRadiusScale,
    AnvilCoverageField,
    ShapeTwistRadians, SpiralTwistPower,
    SpiralFullStrengthRadius01, SpiralZeroStrengthRadius01,
    AnvilStrength, AnvilAnchorHeight01, AnvilDepth01,
    DensityGamma, HFStrength, OutCoarseDensity);"""

NEW_CUSTOM_CODE = """return StormEvaluateMaterialDensityMDRLatest(
    BottomProfileTex, BottomProfileTexSampler,
    TopProfileTex, TopProfileTexSampler,
    AnvilProfileTex, AnvilProfileTexSampler,
    LFNoiseTex, LFNoiseTexSampler,
    HFNoiseTex, HFNoiseTexSampler,
    CurlNoiseTex, CurlNoiseTexSampler,
    Coverage, HLocal, HeightInside, CloudLayerHeight01,
    BaseBottomType, TopType, StormStencil,
    WorldPosition, StormCenterRadius,
    LFUVW, HFUVW,
    FlowOffsetA, FlowOffsetB, FlowWeightA,
    CurlUVW,
    LFUnitsPerBodyRadius, HFUnitsPerBodyRadius,
    CurlUnitsPerBodyRadius, CurlTiling, CurlDisplacementUVW,
    StormLifecycleControl, RotationSign,
    OuterBrimRadiusScale,
    AnvilCoverageField,
    ShapeTwistRadians, SpiralTwistPower,
    SpiralFullStrengthRadius01, SpiralZeroStrengthRadius01,
    AnvilStrength, AnvilAnchorHeight01, AnvilDepth01,
    DensityGamma, HFStrength,
    MDRBodyCoord,
    MDRBoundaries0, MDRBoundaries1,
    MDRSpeeds0, MDRSpeeds1,
    MDRPhases0, MDRPhases1,
    MDRSkews0, MDRSkews1,
    MDRControl0, MDRControl1,
    OutCoarseDensity);"""


def prop(obj, name, default=None):
    try:
        return obj.get_editor_property(name)
    except Exception:
        return default


def set_prop(obj, name, value):
    try:
        obj.set_editor_property(name, value)
    except Exception as exc:
        raise RuntimeError(
            f"Failed to set {obj.get_name()}.{name}: {exc}"
        ) from exc


def expression_inputs(owner, expression, is_function):
    names = [
        str(value)
        for value in unreal.MaterialEditingLibrary.get_material_expression_input_names(
            expression
        )
    ]
    if is_function:
        sources = list(
            unreal.MaterialEditingLibrary.get_inputs_for_material_function_expression(
                owner, expression
            )
        )
    else:
        sources = list(
            unreal.MaterialEditingLibrary.get_inputs_for_material_expression(
                owner, expression
            )
        )
    return {
        name: sources[index] if index < len(sources) else None
        for index, name in enumerate(names)
    }


def find_exact(expressions, cls, property_name, value):
    matches = [
        expression
        for expression in expressions
        if isinstance(expression, cls)
        and str(prop(expression, property_name, "")) == value
    ]
    if len(matches) != 1:
        raise RuntimeError(
            f"Expected one {cls.__name__} with {property_name}={value}, "
            f"found {len(matches)}"
        )
    return matches[0]


def find_call(expressions, function_path):
    matches = []
    for expression in expressions:
        if not isinstance(
            expression, unreal.MaterialExpressionMaterialFunctionCall
        ):
            continue
        called = prop(expression, "material_function")
        if called is not None and called.get_path_name() == function_path:
            matches.append(expression)
    if len(matches) != 1:
        raise RuntimeError(
            f"Expected one call to {function_path}, found {len(matches)}"
        )
    return matches[0]


def connect(source, source_output, target, target_input, label):
    if not unreal.MaterialEditingLibrary.connect_material_expressions(
        source, source_output, target, target_input
    ):
        raise RuntimeError(f"Failed connection: {label}")


def named_declaration(expressions, declaration_name):
    return find_exact(
        expressions,
        unreal.MaterialExpressionNamedRerouteDeclaration,
        "name",
        declaration_name,
    )


function = unreal.load_asset(FUNCTION_PATH)
material = unreal.load_asset(MATERIAL_PATH)
if function is None or material is None:
    raise RuntimeError("MF_Erosion or M_SSS could not be loaded")

function.modify()
material.modify()

function_expressions = list(
    unreal.MaterialEditingLibrary.get_material_function_expressions(function)
)
material_expressions = list(
    unreal.MaterialEditingLibrary.get_material_expressions(material)
)

custom_nodes = [
    expression
    for expression in function_expressions
    if isinstance(expression, unreal.MaterialExpressionCustom)
]
if len(custom_nodes) != 1:
    raise RuntimeError(
        f"Expected one MF_Erosion Custom node, found {len(custom_nodes)}"
    )
custom = custom_nodes[0]

if str(prop(custom, "code", "")).strip() != OLD_CUSTOM_CODE.strip():
    raise RuntimeError("MF_Erosion Custom code is not the expected current ABI")
old_include_paths = [str(value) for value in prop(custom, "include_file_paths", [])]
if old_include_paths != [
    "/SavageSuperStormShaders/Public/StormDensityMaterialAdapter.ush"
]:
    raise RuntimeError(
        f"Unexpected MF_Erosion Custom includes: {old_include_paths}"
    )

old_custom_edges = expression_inputs(function, custom, True)
if list(old_custom_edges.keys()) != OLD_CUSTOM_ORDER:
    raise RuntimeError(
        f"Unexpected MF_Erosion Custom inputs: {list(old_custom_edges.keys())}"
    )
missing_old_custom = [
    name for name, source in old_custom_edges.items() if source is None
]
if missing_old_custom:
    raise RuntimeError(
        f"Pre-merge Custom inputs are unconnected: {missing_old_custom}"
    )

existing_function_inputs = [
    expression
    for expression in function_expressions
    if isinstance(expression, unreal.MaterialExpressionFunctionInput)
]
existing_function_input_names = [
    str(prop(expression, "input_name", ""))
    for expression in existing_function_inputs
]
unexpected_mdr_inputs = sorted(
    set(existing_function_input_names).intersection(MDR_INPUTS)
)
if unexpected_mdr_inputs:
    raise RuntimeError(
        f"MF_Erosion already contains MDR inputs: {unexpected_mdr_inputs}"
    )

function_call = find_call(material_expressions, function.get_path_name())
old_call_edges = expression_inputs(material, function_call, False)
if list(old_call_edges.keys()) != OLD_CALL_ORDER:
    raise RuntimeError(
        f"Unexpected M_SSS MF_Erosion interface: {list(old_call_edges.keys())}"
    )
missing_old_call = [
    name for name, source in old_call_edges.items() if source is None
]
if missing_old_call:
    raise RuntimeError(
        f"Pre-merge M_SSS inputs are unconnected: {missing_old_call}"
    )
old_call_source_names = {
    name: source.get_name() for name, source in old_call_edges.items()
}

merge_calls = []
for expression in material_expressions:
    if not isinstance(
        expression, unreal.MaterialExpressionMaterialFunctionCall
    ):
        continue
    called = prop(expression, "material_function")
    if called is not None and called.get_path_name() == MERGE_PATH:
        merge_calls.append(expression)
if merge_calls:
    raise RuntimeError("M_SSS unexpectedly calls MF_Erosion_Merge")

existing_ring_names = {
    str(prop(expression, "parameter_name", ""))
    for expression in material_expressions
    if isinstance(expression, unreal.MaterialExpressionVectorParameter)
}
duplicates = sorted(set(RING_PARAMETERS).intersection(existing_ring_names))
if duplicates:
    raise RuntimeError(
        f"M_SSS already contains MDR ring parameters: {duplicates}"
    )

density_declaration = named_declaration(material_expressions, "Density")
coarse_declaration = named_declaration(
    material_expressions, "CoarseDensity"
)
for declaration_name, declaration in (
    ("Density", density_declaration),
    ("CoarseDensity", coarse_declaration),
):
    sources = expression_inputs(material, declaration, False)
    if list(sources.values()) != [function_call]:
        raise RuntimeError(
            f"{declaration_name} is not driven by the MF_Erosion call"
        )

subtract = next(
    (
        expression
        for expression in material_expressions
        if isinstance(expression, unreal.MaterialExpressionSubtract)
        and expression.get_name() == "MaterialExpressionSubtract_0"
    ),
    None,
)
if subtract is None:
    raise RuntimeError("M_SSS coordinate Subtract_0 was not found")
subtract_position = (
    unreal.MaterialEditingLibrary.get_material_expression_node_position(
        subtract
    )
)
if tuple(subtract_position) != (-1152, -736):
    raise RuntimeError(
        f"Unexpected Subtract_0 position: {tuple(subtract_position)}"
    )
subtract_edges = expression_inputs(material, subtract, False)
if any(source is None for source in subtract_edges.values()):
    raise RuntimeError("M_SSS coordinate Subtract_0 is not fully connected")

storm_center = find_exact(
    material_expressions,
    unreal.MaterialExpressionVectorParameter,
    "parameter_name",
    "StormCenterRadius",
)

zero_preview = prop(existing_function_inputs[0], "preview_value")
new_function_inputs = {}
for index, name in enumerate(MDR_INPUTS):
    node = (
        unreal.MaterialEditingLibrary.create_material_expression_in_function(
            function,
            unreal.MaterialExpressionFunctionInput,
            112,
            1600 + index * 80,
        )
    )
    if node is None:
        raise RuntimeError(f"Failed to create FunctionInput {name}")
    node.modify()
    set_prop(node, "input_name", name)
    set_prop(
        node,
        "input_type",
        (
            unreal.FunctionInputType.FUNCTION_INPUT_VECTOR2
            if name == "MDRBodyCoord"
            else unreal.FunctionInputType.FUNCTION_INPUT_VECTOR4
        ),
    )
    set_prop(node, "sort_priority", 26 + index)
    if zero_preview is not None:
        set_prop(node, "preview_value", zero_preview)
    set_prop(node, "use_preview_value_as_default", True)
    new_function_inputs[name] = node

new_custom_inputs = []
for name in CUSTOM_INPUT_ORDER:
    custom_input = unreal.CustomInput()
    custom_input.set_editor_property("input_name", name)
    new_custom_inputs.append(custom_input)

custom.modify()
set_prop(custom, "inputs", new_custom_inputs)
set_prop(custom, "code", NEW_CUSTOM_CODE)
set_prop(custom, "include_file_paths", [LATEST_INCLUDE])

for name in CUSTOM_INPUT_ORDER:
    source = (
        new_function_inputs[name]
        if name in new_function_inputs
        else old_custom_edges[name]
    )
    connect(source, "", custom, name, f"MF_Erosion {name} -> Custom")

unreal.MaterialEditingLibrary.update_material_function(function)

ring_nodes = {}
for parameter_name, (default, position) in RING_PARAMETERS.items():
    node = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionVectorParameter,
        position[0],
        position[1],
    )
    if node is None:
        raise RuntimeError(f"Failed to create {parameter_name}")
    node.modify()
    set_prop(node, "parameter_name", parameter_name)
    set_prop(node, "default_value", unreal.LinearColor(*default))
    set_prop(node, "sort_priority", 32)
    ring_nodes[parameter_name] = node

radius_mask = unreal.MaterialEditingLibrary.create_material_expression(
    material,
    unreal.MaterialExpressionComponentMask,
    -544,
    -3792,
)
radius_max = unreal.MaterialEditingLibrary.create_material_expression(
    material,
    unreal.MaterialExpressionMax,
    -320,
    -3792,
)
body_coord = unreal.MaterialEditingLibrary.create_material_expression(
    material,
    unreal.MaterialExpressionDivide,
    -96,
    -3792,
)
if radius_mask is None or radius_max is None or body_coord is None:
    raise RuntimeError("Failed to create MDR body-coordinate nodes")

radius_mask.modify()
set_prop(radius_mask, "r", False)
set_prop(radius_mask, "g", False)
set_prop(radius_mask, "b", True)
set_prop(radius_mask, "a", False)
radius_max.modify()
set_prop(radius_max, "const_b", 1.0)

connect(
    storm_center,
    "",
    radius_mask,
    "Input",
    "StormCenterRadius -> radius B mask",
)
connect(radius_mask, "", radius_max, "A", "StormRadius -> Max")
connect(subtract, "", body_coord, "A", "centered XY -> Divide")
connect(radius_max, "", body_coord, "B", "safe radius -> Divide")

function_call.modify()
function_call.set_material_function(function)

for name, source in old_call_edges.items():
    connect(
        source,
        "",
        function_call,
        name,
        f"restore M_SSS {name}",
    )

connect(
    body_coord,
    "",
    function_call,
    "MDRBodyCoord",
    "MDR body coordinate -> MF_Erosion",
)
for parameter_name, node in ring_nodes.items():
    function_input_name = parameter_name.removeprefix("Storm")
    connect(
        node,
        "",
        function_call,
        function_input_name,
        f"{parameter_name} -> {function_input_name}",
    )

unreal.MaterialEditingLibrary.refresh_material_function_editor(function)
unreal.MaterialEditingLibrary.recompile_material(material)

custom_edges = expression_inputs(function, custom, True)
if list(custom_edges.keys()) != CUSTOM_INPUT_ORDER:
    raise RuntimeError(
        f"Merged Custom order mismatch: {list(custom_edges.keys())}"
    )
missing_custom = [
    name for name, source in custom_edges.items() if source is None
]
if missing_custom:
    raise RuntimeError(
        f"Merged Custom inputs are unconnected: {missing_custom}"
    )
if str(prop(custom, "code", "")).strip() != NEW_CUSTOM_CODE.strip():
    raise RuntimeError("Merged Custom code did not persist in memory")
actual_includes = [
    str(value) for value in prop(custom, "include_file_paths", [])
]
if actual_includes != [LATEST_INCLUDE]:
    raise RuntimeError(f"Merged include mismatch: {actual_includes}")

updated_call_edges = expression_inputs(material, function_call, False)
if list(updated_call_edges.keys()) != UPDATED_CALL_ORDER:
    raise RuntimeError(
        f"Merged M_SSS call order mismatch: {list(updated_call_edges.keys())}"
    )
missing_call = [
    name for name, source in updated_call_edges.items() if source is None
]
if missing_call:
    raise RuntimeError(
        f"Merged M_SSS inputs are unconnected: {missing_call}"
    )
for name, old_source_name in old_call_source_names.items():
    if updated_call_edges[name].get_name() != old_source_name:
        raise RuntimeError(
            f"Existing M_SSS source changed for {name}: "
            f"{old_source_name} -> {updated_call_edges[name].get_name()}"
        )
if updated_call_edges["MDRBodyCoord"] != body_coord:
    raise RuntimeError("MDRBodyCoord is not driven by the exact coordinate graph")
for parameter_name, node in ring_nodes.items():
    input_name = parameter_name.removeprefix("Storm")
    if updated_call_edges[input_name] != node:
        raise RuntimeError(f"{input_name} is not driven by {parameter_name}")

function_expressions = list(
    unreal.MaterialEditingLibrary.get_material_function_expressions(function)
)
function_input_nodes = [
    expression
    for expression in function_expressions
    if isinstance(expression, unreal.MaterialExpressionFunctionInput)
]
function_input_names = [
    str(prop(expression, "input_name", ""))
    for expression in function_input_nodes
]
for name in MDR_INPUTS:
    if function_input_names.count(name) != 1:
        raise RuntimeError(
            f"Expected one merged FunctionInput {name}, "
            f"found {function_input_names.count(name)}"
        )

material_expressions = list(
    unreal.MaterialEditingLibrary.get_material_expressions(material)
)
for parameter_name, (expected, _) in RING_PARAMETERS.items():
    node = find_exact(
        material_expressions,
        unreal.MaterialExpressionVectorParameter,
        "parameter_name",
        parameter_name,
    )
    actual = prop(node, "default_value")
    components = (
        float(actual.r),
        float(actual.g),
        float(actual.b),
        float(actual.a),
    )
    if any(
        not math.isclose(value, wanted, rel_tol=0.0, abs_tol=1.0e-5)
        for value, wanted in zip(components, expected)
    ):
        raise RuntimeError(
            f"{parameter_name} default mismatch: {components} != {expected}"
        )

for declaration_name, declaration in (
    ("Density", density_declaration),
    ("CoarseDensity", coarse_declaration),
):
    sources = expression_inputs(material, declaration, False)
    if list(sources.values()) != [function_call]:
        raise RuntimeError(
            f"{declaration_name} output wiring changed during merge"
        )

if not unreal.EditorAssetLibrary.save_loaded_asset(function, False):
    raise RuntimeError("Failed to save MF_Erosion")
if not unreal.EditorAssetLibrary.save_loaded_asset(material, False):
    raise RuntimeError("Failed to save M_SSS")

unreal.log(
    "CODEX_MDR6_ASSETS|SUCCESS|"
    f"custom_inputs={len(custom_edges)}|"
    f"function_inputs={len(function_input_nodes)}|"
    f"caller_inputs={len(updated_call_edges)}|"
    f"ring_parameters={len(ring_nodes)}"
)
unreal.log("CODEX_MDR6_ASSETS_COMPLETE")
