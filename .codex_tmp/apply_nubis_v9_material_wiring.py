import unreal


FUNCTION_PATH = "/SavageSuperStorm/Materials/Functions/MF_Erosion"
MATERIAL_PATH = "/SavageSuperStorm/Materials/M_SSS"
KM_TO_CM = 100000.0
CONVERSION_MARKER = "Nubis v9 world-size km conversion"


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
    "SpiralTendrilMask",
    "SpiralTendrilStrength",
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
    StormStencil, SpiralTendrilMask, SpiralTendrilStrength,
    WorldPosition, StormCenterRadius, LFUVW, HFUVW, CurlUVW,
    LFUnitsPerBodyRadius, HFUnitsPerBodyRadius,
    CurlUnitsPerBodyRadius, CurlTiling, CurlDisplacementUVW,
    StormLifecycleControl, RotationSign,
    AnvilStrength, AnvilTypeBias, AnvilCeilingFade01,
    AnvilWindDirectionXY, AnvilWindStretch, AnvilWindSkew,
    AnvilWarpStart01, AnvilWarpEnd01,
    DensityGamma, HFStrength, OutCoarseDensity);"""


FUNCTION_INPUT_RENAMES = {
    "BottomTex": "BottomProfileTex",
    "TopTex": "TopProfileTex",
    "BottomType": "BaseBottomType",
    "CloudType": "TopType",
    "ShapeRT2G": "SpiralTendrilMask",
    "LFPos": "LFUVW",
    "HFPos": "HFUVW",
    "HFTex": "HFNoiseTex",
    "CurlTex": "CurlNoiseTex",
    "CurlPos": "CurlUVW",
}


SCALAR_DEFAULTS = {
    "SpiralTendrilStrength": 0.90,
    "LFUnitsPerBodyRadius": 2.40,
    "HFUnitsPerBodyRadius": 10.0,
    "CurlUnitsPerBodyRadius": 2.0,
    "RotationSign": -1.0,
    "AnvilWindStretch": 1.8,
    "AnvilWindSkew": 0.20,
    "AnvilWarpStart01": 0.62,
    "AnvilWarpEnd01": 0.86,
}


OBSOLETE_NAMES = {
    "SpiralCarveStrength",
    "SpiralCarveCoverageFloor",
    "FormationSwirlSign",
}


def prop(obj, name, default=None):
    try:
        return obj.get_editor_property(name)
    except Exception:
        return default


def set_prop(obj, name, value):
    obj.set_editor_property(name, value)


def position(expr):
    return unreal.MaterialEditingLibrary.get_material_expression_node_position(expr)


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
    for expr in expressions:
        if isinstance(expr, cls) and str(prop(expr, "parameter_name", "")) == name:
            return expr
    return None


function = unreal.load_asset(FUNCTION_PATH)
material = unreal.load_asset(MATERIAL_PATH)
if function is None or material is None:
    raise RuntimeError("Required M_SSS or MF_Erosion asset was not found")

function.modify()
material.modify()

function_expressions = list(
    unreal.MaterialEditingLibrary.get_material_function_expressions(function)
)
material_expressions = list(unreal.MaterialEditingLibrary.get_material_expressions(material))

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

# Capture the caller's existing sources before the function signature changes.
old_call_sources = input_sources(material, function_call, False)

# Keep the existing FunctionInput objects (and therefore their GUIDs) while
# updating only their public names/order. This preserves all caller ownership.
function_inputs = {}
for expr in function_expressions:
    if not isinstance(expr, unreal.MaterialExpressionFunctionInput):
        continue
    old_name = str(prop(expr, "input_name", ""))
    new_name = FUNCTION_INPUT_RENAMES.get(old_name, old_name)
    if new_name != old_name:
        expr.modify()
        set_prop(expr, "input_name", new_name)
    function_inputs[new_name] = expr

for name, expr in function_inputs.items():
    if name in CUSTOM_INPUT_ORDER:
        set_prop(expr, "sort_priority", CUSTOM_INPUT_ORDER.index(name))

# Remove only parameters explicitly retired by the v9 contract.
for expr in list(function_expressions):
    if str(prop(expr, "parameter_name", "")) in OBSOLETE_NAMES:
        unreal.MaterialEditingLibrary.delete_material_expression_in_function(function, expr)
for expr in list(material_expressions):
    if str(prop(expr, "parameter_name", "")) in OBSOLETE_NAMES:
        unreal.MaterialEditingLibrary.delete_material_expression(material, expr)

custom_x, custom_y = position(custom)
scalar_parameters = {}
for index, (name, default_value) in enumerate(SCALAR_DEFAULTS.items()):
    node = find_parameter(
        function_expressions, name, unreal.MaterialExpressionScalarParameter
    )
    if node is None:
        node = unreal.MaterialEditingLibrary.create_material_expression_in_function(
            function,
            unreal.MaterialExpressionScalarParameter,
            custom_x - 420,
            custom_y + 480 + index * 96,
        )
        function_expressions.append(node)
    node.modify()
    set_prop(node, "parameter_name", name)
    set_prop(node, "default_value", float(default_value))
    set_prop(node, "sort_priority", 40 + index)
    scalar_parameters[name] = node

wind_parameter = find_parameter(
    function_expressions,
    "AnvilWindDirectionXY",
    unreal.MaterialExpressionVectorParameter,
)
if wind_parameter is None:
    wind_parameter = unreal.MaterialEditingLibrary.create_material_expression_in_function(
        function,
        unreal.MaterialExpressionVectorParameter,
        custom_x - 640,
        custom_y + 1248,
    )
    function_expressions.append(wind_parameter)
wind_parameter.modify()
set_prop(wind_parameter, "parameter_name", "AnvilWindDirectionXY")
set_prop(wind_parameter, "default_value", unreal.LinearColor(1.0, 0.0, 0.0, 0.0))
set_prop(wind_parameter, "sort_priority", 49)

wind_mask = None
for expr in function_expressions:
    if (
        isinstance(expr, unreal.MaterialExpressionComponentMask)
        and str(prop(expr, "desc", "")) == "Nubis v9 AnvilWindDirectionXY RG"
    ):
        wind_mask = expr
        break
if wind_mask is None:
    wind_mask = unreal.MaterialEditingLibrary.create_material_expression_in_function(
        function,
        unreal.MaterialExpressionComponentMask,
        custom_x - 360,
        custom_y + 1248,
    )
    function_expressions.append(wind_mask)
wind_mask.modify()
set_prop(wind_mask, "desc", "Nubis v9 AnvilWindDirectionXY RG")
set_prop(wind_mask, "r", True)
set_prop(wind_mask, "g", True)
set_prop(wind_mask, "b", False)
set_prop(wind_mask, "a", False)
if not unreal.MaterialEditingLibrary.connect_material_expressions(
    wind_parameter, "", wind_mask, "Input"
):
    raise RuntimeError("Failed to connect AnvilWindDirectionXY to its RG mask")

# Existing Anvil parameters remain internal by design.
for name in ("AnvilStrength", "AnvilTypeBias", "AnvilCeilingFade01"):
    node = find_parameter(
        function_expressions, name, unreal.MaterialExpressionScalarParameter
    )
    if node is None:
        raise RuntimeError(f"Existing MF_Erosion parameter is missing: {name}")
    scalar_parameters[name] = node

# Replace the old Custom pin array wholesale so retired names cannot survive.
new_custom_inputs = []
for name in CUSTOM_INPUT_ORDER:
    custom_input = unreal.CustomInput()
    custom_input.set_editor_property("input_name", name)
    new_custom_inputs.append(custom_input)
custom.modify()
set_prop(custom, "inputs", new_custom_inputs)
set_prop(custom, "code", CUSTOM_CODE)

custom_sources = {
    "BottomProfileTex": function_inputs["BottomProfileTex"],
    "TopProfileTex": function_inputs["TopProfileTex"],
    "LFNoiseTex": function_inputs["LFNoiseTex"],
    "HFNoiseTex": function_inputs["HFNoiseTex"],
    "CurlNoiseTex": function_inputs["CurlNoiseTex"],
    "Coverage": function_inputs["Coverage"],
    "HLocal": function_inputs["HLocal"],
    "HeightInside": function_inputs["HeightInside"],
    "BaseBottomType": function_inputs["BaseBottomType"],
    "TopType": function_inputs["TopType"],
    "StormStencil": function_inputs["StormStencil"],
    "SpiralTendrilMask": function_inputs["SpiralTendrilMask"],
    "SpiralTendrilStrength": scalar_parameters["SpiralTendrilStrength"],
    "WorldPosition": function_inputs["WorldPosition"],
    "StormCenterRadius": function_inputs["StormCenterRadius"],
    "LFUVW": function_inputs["LFUVW"],
    "HFUVW": function_inputs["HFUVW"],
    "CurlUVW": function_inputs["CurlUVW"],
    "LFUnitsPerBodyRadius": scalar_parameters["LFUnitsPerBodyRadius"],
    "HFUnitsPerBodyRadius": scalar_parameters["HFUnitsPerBodyRadius"],
    "CurlUnitsPerBodyRadius": scalar_parameters["CurlUnitsPerBodyRadius"],
    "CurlTiling": function_inputs["CurlTiling"],
    "CurlDisplacementUVW": function_inputs["CurlDisplacementUVW"],
    "StormLifecycleControl": function_inputs["StormLifecycleControl"],
    "RotationSign": scalar_parameters["RotationSign"],
    "AnvilStrength": scalar_parameters["AnvilStrength"],
    "AnvilTypeBias": scalar_parameters["AnvilTypeBias"],
    "AnvilCeilingFade01": scalar_parameters["AnvilCeilingFade01"],
    "AnvilWindDirectionXY": wind_mask,
    "AnvilWindStretch": scalar_parameters["AnvilWindStretch"],
    "AnvilWindSkew": scalar_parameters["AnvilWindSkew"],
    "AnvilWarpStart01": scalar_parameters["AnvilWarpStart01"],
    "AnvilWarpEnd01": scalar_parameters["AnvilWarpEnd01"],
    "DensityGamma": function_inputs["DensityGamma"],
    "HFStrength": function_inputs["HFStrength"],
}

for input_name in CUSTOM_INPUT_ORDER:
    if not unreal.MaterialEditingLibrary.connect_material_expressions(
        custom_sources[input_name], "", custom, input_name
    ):
        raise RuntimeError(f"Failed to connect MF_Erosion Custom input: {input_name}")

# Convert the three existing coordinate-scale parameters from reciprocal cm to
# actual texture world-size km while preserving exactly the authored UV period.
material_expressions = list(unreal.MaterialEditingLibrary.get_material_expressions(material))
size_parameter_names = (
    "LFNoiseWorldSize",
    "HFNoiseWorldSize",
    "CurlNoiseWorldSize",
)
for parameter_index, parameter_name in enumerate(size_parameter_names):
    parameter = find_parameter(
        material_expressions, parameter_name, unreal.MaterialExpressionScalarParameter
    )
    if parameter is None:
        raise RuntimeError(f"M_SSS parameter is missing: {parameter_name}")

    marker_cm = f"{CONVERSION_MARKER}: {parameter_name} km-to-cm"
    marker_inv = f"{CONVERSION_MARKER}: {parameter_name} reciprocal"
    world_size_cm = next(
        (
            expr
            for expr in material_expressions
            if isinstance(expr, unreal.MaterialExpressionMultiply)
            and str(prop(expr, "desc", "")) == marker_cm
        ),
        None,
    )
    reciprocal = next(
        (
            expr
            for expr in material_expressions
            if isinstance(expr, unreal.MaterialExpressionDivide)
            and str(prop(expr, "desc", "")) == marker_inv
        ),
        None,
    )

    direct_consumer = None
    for expr in material_expressions:
        sources = input_sources(material, expr, False)
        if sources.get("B") is parameter and isinstance(
            expr, unreal.MaterialExpressionMultiply
        ):
            direct_consumer = expr
            break

    if world_size_cm is None or reciprocal is None:
        if direct_consumer is None:
            raise RuntimeError(
                f"Could not find the coordinate multiply driven by {parameter_name}"
            )
        old_scale_per_cm = abs(float(prop(parameter, "default_value", 0.0)))
        if old_scale_per_cm <= 1.0e-12:
            raise RuntimeError(f"Invalid prior coordinate scale for {parameter_name}")
        preserved_world_size_km = 1.0 / (old_scale_per_cm * KM_TO_CM)
        parameter.modify()
        set_prop(parameter, "default_value", preserved_world_size_km)
        prior_desc = str(prop(parameter, "desc", "")).strip()
        if CONVERSION_MARKER not in prior_desc:
            set_prop(
                parameter,
                "desc",
                (prior_desc + " " if prior_desc else "")
                + f"{CONVERSION_MARKER}; value is texture period in km.",
            )

        px, py = position(parameter)
        world_size_cm = unreal.MaterialEditingLibrary.create_material_expression(
            material, unreal.MaterialExpressionMultiply, px + 208, py
        )
        reciprocal = unreal.MaterialEditingLibrary.create_material_expression(
            material, unreal.MaterialExpressionDivide, px + 416, py
        )
        material_expressions.extend((world_size_cm, reciprocal))
        set_prop(world_size_cm, "desc", marker_cm)
        set_prop(world_size_cm, "const_b", KM_TO_CM)
        set_prop(reciprocal, "desc", marker_inv)
        set_prop(reciprocal, "const_a", 1.0)
    else:
        # On an idempotent rerun the existing reciprocal feeds the original
        # coordinate multiply; locate it from that edge instead.
        for expr in material_expressions:
            sources = input_sources(material, expr, False)
            if sources.get("B") is reciprocal and isinstance(
                expr, unreal.MaterialExpressionMultiply
            ):
                direct_consumer = expr
                break

    if direct_consumer is None:
        raise RuntimeError(f"Coordinate multiply was lost for {parameter_name}")
    if not unreal.MaterialEditingLibrary.connect_material_expressions(
        parameter, "", world_size_cm, "A"
    ):
        raise RuntimeError(f"Failed {parameter_name} -> world-size cm conversion")
    if not unreal.MaterialEditingLibrary.connect_material_expressions(
        world_size_cm, "", reciprocal, "B"
    ):
        raise RuntimeError(f"Failed {parameter_name} reciprocal denominator")
    if not unreal.MaterialEditingLibrary.connect_material_expressions(
        reciprocal, "", direct_consumer, "B"
    ):
        raise RuntimeError(f"Failed {parameter_name} coordinate scale connection")

# Rebuild the caller interface, then restore every captured source by its v9 name.
unreal.MaterialEditingLibrary.update_material_function(function)
function_call.modify()
function_call.set_material_function(function)
for old_name, source in old_call_sources.items():
    if source is None:
        continue
    new_name = FUNCTION_INPUT_RENAMES.get(old_name, old_name)
    if not unreal.MaterialEditingLibrary.connect_material_expressions(
        source, "", function_call, new_name
    ):
        raise RuntimeError(f"Failed to restore M_SSS call input: {new_name}")

unreal.MaterialEditingLibrary.refresh_material_function_editor(function)
unreal.MaterialEditingLibrary.recompile_material(material)

# Structural verification before committing either binary asset to disk.
function_edges = input_sources(function, custom, True)
if list(function_edges.keys()) != CUSTOM_INPUT_ORDER:
    raise RuntimeError(
        f"Custom input order mismatch: {list(function_edges.keys())}"
    )
missing_custom_sources = [name for name, source in function_edges.items() if source is None]
if missing_custom_sources:
    raise RuntimeError(f"Unconnected Custom inputs: {missing_custom_sources}")

call_edges = input_sources(material, function_call, False)
missing_call_sources = [name for name, source in call_edges.items() if source is None]
if missing_call_sources:
    raise RuntimeError(f"Unconnected M_SSS MF_Erosion inputs: {missing_call_sources}")

if not unreal.EditorAssetLibrary.save_loaded_asset(function, False):
    raise RuntimeError("Failed to save MF_Erosion")
if not unreal.EditorAssetLibrary.save_loaded_asset(material, False):
    raise RuntimeError("Failed to save M_SSS")

unreal.log(
    "CODEX_V9_APPLY|SUCCESS|"
    f"custom_inputs={len(function_edges)}|"
    f"caller_inputs={len(call_edges)}|"
    f"caller_pins={','.join(call_edges.keys())}"
)
unreal.log("CODEX_V9_APPLY_COMPLETE")
