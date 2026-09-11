import math
import unreal


FUNCTION_PATH = "/SavageSuperStorm/Materials/Functions/MF_Erosion"
MATERIAL_PATH = "/SavageSuperStorm/Materials/M_SSS"
MERGE_PATH = "/SavageSuperStorm/Materials/Functions/MF_Erosion_Merge"
LATEST_INCLUDE = (
    "/SavageSuperStormShaders/Public/StormDensityLatestMDRAdapter.ush"
)

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

RING_DEFAULTS = {
    "StormMDRBoundaries0": (0.10, 0.22, 0.36, 0.54),
    "StormMDRBoundaries1": (0.74, 0.04, 1.00, 0.08),
    "StormMDRSpeeds0": (0.0, 0.0, 0.0, 0.0),
    "StormMDRSpeeds1": (0.0, 0.0, 1.0, 0.0),
    "StormMDRPhases0": (0.0, 0.0, 0.0, 0.0),
    "StormMDRPhases1": (0.0, 0.0, 0.0, 0.0),
    "StormMDRSkews0": (0.0, 0.0, 0.0, 0.0),
    "StormMDRSkews1": (0.0, 0.0, 0.0, 0.62),
    "StormMDRControl0": (0.08, 0.0, 1.0, 0.0),
    "StormMDRControl1": (0.0, 0.0, 0.0, 1.0),
}


def prop(obj, name, default=None):
    try:
        return obj.get_editor_property(name)
    except Exception:
        return default


def inputs(owner, expression, is_function):
    names = [
        str(value)
        for value in unreal.MaterialEditingLibrary.get_material_expression_input_names(
            expression
        )
    ]
    sources = list(
        (
            unreal.MaterialEditingLibrary.get_inputs_for_material_function_expression(
                owner, expression
            )
            if is_function
            else unreal.MaterialEditingLibrary.get_inputs_for_material_expression(
                owner, expression
            )
        )
    )
    return {
        name: sources[index] if index < len(sources) else None
        for index, name in enumerate(names)
    }


def one(expressions, cls, property_name=None, value=None):
    matches = [
        expression
        for expression in expressions
        if isinstance(expression, cls)
        and (
            property_name is None
            or str(prop(expression, property_name, "")) == value
        )
    ]
    if len(matches) != 1:
        raise RuntimeError(
            f"Expected one {cls.__name__} {property_name}={value}, "
            f"found {len(matches)}"
        )
    return matches[0]


def calls_to(expressions, path):
    result = []
    for expression in expressions:
        if not isinstance(
            expression, unreal.MaterialExpressionMaterialFunctionCall
        ):
            continue
        function = prop(expression, "material_function")
        if function is not None and function.get_path_name() == path:
            result.append(expression)
    return result


function = unreal.load_asset(FUNCTION_PATH)
material = unreal.load_asset(MATERIAL_PATH)
if function is None or material is None:
    raise RuntimeError("Merged assets could not be loaded")

function_expressions = list(
    unreal.MaterialEditingLibrary.get_material_function_expressions(function)
)
material_expressions = list(
    unreal.MaterialEditingLibrary.get_material_expressions(material)
)

custom = one(function_expressions, unreal.MaterialExpressionCustom)
custom_inputs = inputs(function, custom, True)
if len(custom_inputs) != 50:
    raise RuntimeError(
        f"Expected 50 merged Custom inputs, found {len(custom_inputs)}"
    )
if any(source is None for source in custom_inputs.values()):
    raise RuntimeError("Merged Custom has an unconnected input")
if [str(value) for value in prop(custom, "include_file_paths", [])] != [
    LATEST_INCLUDE
]:
    raise RuntimeError("MF_Erosion does not include the Latest MDR adapter")
custom_code = str(prop(custom, "code", ""))
for token in (
    "StormEvaluateMaterialDensityMDRLatest(",
    "AnvilDepth01",
    "MDRBodyCoord",
    "MDRBoundaries0",
    "MDRControl1",
    "OutCoarseDensity",
):
    if token not in custom_code:
        raise RuntimeError(f"Custom code is missing {token}")

function_inputs = [
    expression
    for expression in function_expressions
    if isinstance(expression, unreal.MaterialExpressionFunctionInput)
]
if len(function_inputs) != 38:
    raise RuntimeError(
        f"Expected 38 public FunctionInputs, found {len(function_inputs)}"
    )
by_function_input = {
    str(prop(expression, "input_name", "")): expression
    for expression in function_inputs
}
for name in MDR_INPUTS:
    node = by_function_input.get(name)
    if node is None:
        raise RuntimeError(f"Missing FunctionInput {name}")
    expected_type = (
        unreal.FunctionInputType.FUNCTION_INPUT_VECTOR2
        if name == "MDRBodyCoord"
        else unreal.FunctionInputType.FUNCTION_INPUT_VECTOR4
    )
    if prop(node, "input_type") != expected_type:
        raise RuntimeError(f"Unexpected input type for {name}")
    if not bool(prop(node, "use_preview_value_as_default", False)):
        raise RuntimeError(f"{name} lacks a safe preview default")
    if custom_inputs[name] != node:
        raise RuntimeError(f"Custom {name} does not use its FunctionInput")

function_calls = calls_to(material_expressions, function.get_path_name())
if len(function_calls) != 1:
    raise RuntimeError(
        f"Expected one M_SSS MF_Erosion call, found {len(function_calls)}"
    )
if calls_to(material_expressions, MERGE_PATH):
    raise RuntimeError("M_SSS still contains an MF_Erosion_Merge call")
function_call = function_calls[0]
call_inputs = inputs(material, function_call, False)
if len(call_inputs) != 38:
    raise RuntimeError(
        f"Expected 38 M_SSS call inputs, found {len(call_inputs)}"
    )
if any(source is None for source in call_inputs.values()):
    raise RuntimeError("M_SSS MF_Erosion call has an unconnected input")

ring_nodes = {}
for parameter_name, expected in RING_DEFAULTS.items():
    node = one(
        material_expressions,
        unreal.MaterialExpressionVectorParameter,
        "parameter_name",
        parameter_name,
    )
    ring_nodes[parameter_name] = node
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
    function_input_name = parameter_name.removeprefix("Storm")
    if call_inputs[function_input_name] != node:
        raise RuntimeError(
            f"{function_input_name} is not driven by {parameter_name}"
        )

body_coord = call_inputs["MDRBodyCoord"]
if not isinstance(body_coord, unreal.MaterialExpressionDivide):
    raise RuntimeError("MDRBodyCoord is not produced by Divide")
body_edges = inputs(material, body_coord, False)
if not isinstance(body_edges.get("A"), unreal.MaterialExpressionSubtract):
    raise RuntimeError("MDRBodyCoord numerator is not centered XY")
radius_max = body_edges.get("B")
if not isinstance(radius_max, unreal.MaterialExpressionMax):
    raise RuntimeError("MDRBodyCoord denominator is not Max")
if not math.isclose(
    float(prop(radius_max, "const_b", 0.0)),
    1.0,
    rel_tol=0.0,
    abs_tol=1.0e-6,
):
    raise RuntimeError("MDR radius denominator is not clamped to one")
max_edges = inputs(material, radius_max, False)
radius_mask = max_edges.get("A")
if not isinstance(radius_mask, unreal.MaterialExpressionComponentMask):
    raise RuntimeError("MDR radius is not extracted by ComponentMask")
if (
    bool(prop(radius_mask, "r", False))
    or bool(prop(radius_mask, "g", False))
    or not bool(prop(radius_mask, "b", False))
    or bool(prop(radius_mask, "a", False))
):
    raise RuntimeError("MDR radius mask is not the B channel")
mask_source = next(iter(inputs(material, radius_mask, False).values()))
if (
    not isinstance(mask_source, unreal.MaterialExpressionVectorParameter)
    or str(prop(mask_source, "parameter_name", ""))
    != "StormCenterRadius"
):
    raise RuntimeError("MDR radius is not sourced from StormCenterRadius")

for declaration_name in ("Density", "CoarseDensity"):
    declaration = one(
        material_expressions,
        unreal.MaterialExpressionNamedRerouteDeclaration,
        "name",
        declaration_name,
    )
    if list(inputs(material, declaration, False).values()) != [function_call]:
        raise RuntimeError(
            f"{declaration_name} output no longer comes from MF_Erosion"
        )

unreal.MaterialEditingLibrary.recompile_material(material)
unreal.log(
    "CODEX_MDR6_VALIDATE|SUCCESS|"
    f"custom_inputs={len(custom_inputs)}|"
    f"function_inputs={len(function_inputs)}|"
    f"caller_inputs={len(call_inputs)}|"
    f"ring_parameters={len(ring_nodes)}"
)
unreal.log("CODEX_MDR6_VALIDATE_COMPLETE")
