import math
import unreal


FUNCTION_PATH = "/SavageSuperStorm/Materials/Functions/MF_Erosion"
MATERIAL_PATH = "/SavageSuperStorm/Materials/M_SSS"

EXPECTED_CUSTOM_INPUTS = [
    "BottomProfileTex", "TopProfileTex", "LFNoiseTex", "HFNoiseTex",
    "CurlNoiseTex", "Coverage", "HLocal", "HeightInside",
    "BaseBottomType", "TopType", "StormStencil", "SpiralTendrilMask",
    "SpiralTendrilStrength", "WorldPosition", "StormCenterRadius", "LFUVW",
    "HFUVW", "CurlUVW", "LFUnitsPerBodyRadius", "HFUnitsPerBodyRadius",
    "CurlUnitsPerBodyRadius", "CurlTiling", "CurlDisplacementUVW",
    "StormLifecycleControl", "RotationSign", "AnvilStrength", "AnvilTypeBias",
    "AnvilCeilingFade01", "AnvilWindDirectionXY", "AnvilWindStretch",
    "AnvilWindSkew", "AnvilWarpStart01", "AnvilWarpEnd01", "DensityGamma",
    "HFStrength",
]

EXPECTED_CALL_INPUTS = [
    name
    for name in EXPECTED_CUSTOM_INPUTS
    if name
    not in {
        "SpiralTendrilStrength",
        "LFUnitsPerBodyRadius",
        "HFUnitsPerBodyRadius",
        "CurlUnitsPerBodyRadius",
        "RotationSign",
        "AnvilStrength",
        "AnvilTypeBias",
        "AnvilCeilingFade01",
        "AnvilWindDirectionXY",
        "AnvilWindStretch",
        "AnvilWindSkew",
        "AnvilWarpStart01",
        "AnvilWarpEnd01",
    }
]

EXPECTED_SCALARS = {
    "AnvilStrength": 0.0,
    "AnvilTypeBias": 0.0,
    "AnvilCeilingFade01": 0.05,
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

EXPECTED_WORLD_SIZES_KM = {
    "LFNoiseWorldSize": 1.0 / (6.0e-6 * 100000.0),
    "HFNoiseWorldSize": 1.0 / (2.5e-5 * 100000.0),
    "CurlNoiseWorldSize": 1.0 / (5.0e-6 * 100000.0),
}

OBSOLETE = {
    "SpiralCarveStrength",
    "SpiralCarveCoverageFloor",
    "FormationSwirlSign",
    "ShapeRT2G",
}


def prop(obj, name, default=None):
    try:
        return obj.get_editor_property(name)
    except Exception:
        return default


def edges(owner, expr, is_function):
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
    return names, [sources[i] if i < len(sources) else None for i in range(len(names))]


function = unreal.load_asset(FUNCTION_PATH)
material = unreal.load_asset(MATERIAL_PATH)
if function is None or material is None:
    raise RuntimeError("Failed to reload M_SSS or MF_Erosion")

function_expressions = list(
    unreal.MaterialEditingLibrary.get_material_function_expressions(function)
)
material_expressions = list(unreal.MaterialEditingLibrary.get_material_expressions(material))

custom = next(
    expr for expr in function_expressions if isinstance(expr, unreal.MaterialExpressionCustom)
)
custom_names, custom_sources = edges(function, custom, True)
if custom_names != EXPECTED_CUSTOM_INPUTS:
    raise RuntimeError(f"Custom input order mismatch: {custom_names}")
if any(source is None for source in custom_sources):
    raise RuntimeError("At least one Custom input is disconnected")
if str(prop(custom, "code", "")).strip().count("StormEvaluateMaterialDensity") != 1:
    raise RuntimeError("Custom code is not the v9 density call")

call = next(
    expr
    for expr in material_expressions
    if isinstance(expr, unreal.MaterialExpressionMaterialFunctionCall)
    and prop(expr, "material_function")
    and prop(expr, "material_function").get_path_name() == function.get_path_name()
)
call_names, call_sources = edges(material, call, False)
if call_names != EXPECTED_CALL_INPUTS:
    raise RuntimeError(f"M_SSS call order mismatch: {call_names}")
if any(source is None for source in call_sources):
    raise RuntimeError("At least one M_SSS MF_Erosion input is disconnected")

parameters = {}
for expr in function_expressions + material_expressions:
    name = str(prop(expr, "parameter_name", ""))
    if name:
        parameters.setdefault(name, []).append(expr)

for name, expected in EXPECTED_SCALARS.items():
    matches = [
        expr
        for expr in parameters.get(name, [])
        if isinstance(expr, unreal.MaterialExpressionScalarParameter)
    ]
    if len(matches) != 1:
        raise RuntimeError(f"Expected one scalar parameter {name}, found {len(matches)}")
    actual = float(prop(matches[0], "default_value", math.nan))
    if not math.isclose(actual, expected, rel_tol=0.0, abs_tol=1.0e-5):
        raise RuntimeError(f"{name} default mismatch: {actual} != {expected}")

wind_matches = [
    expr
    for expr in parameters.get("AnvilWindDirectionXY", [])
    if isinstance(expr, unreal.MaterialExpressionVectorParameter)
]
if len(wind_matches) != 1:
    raise RuntimeError("AnvilWindDirectionXY vector parameter is missing or duplicated")

for name, expected in EXPECTED_WORLD_SIZES_KM.items():
    matches = [
        expr
        for expr in parameters.get(name, [])
        if isinstance(expr, unreal.MaterialExpressionScalarParameter)
    ]
    if len(matches) != 1:
        raise RuntimeError(f"Expected one M_SSS world-size parameter {name}")
    actual = float(prop(matches[0], "default_value", math.nan))
    if not math.isclose(actual, expected, rel_tol=0.0, abs_tol=1.0e-4):
        raise RuntimeError(f"{name} km default mismatch: {actual} != {expected}")
    cm_marker = f"Nubis v9 world-size km conversion: {name} km-to-cm"
    inv_marker = f"Nubis v9 world-size km conversion: {name} reciprocal"
    if not any(str(prop(expr, "desc", "")) == cm_marker for expr in material_expressions):
        raise RuntimeError(f"Missing km-to-cm node for {name}")
    if not any(str(prop(expr, "desc", "")) == inv_marker for expr in material_expressions):
        raise RuntimeError(f"Missing reciprocal node for {name}")

all_contract_names = set(parameters)
all_contract_names.update(
    str(prop(expr, "input_name", ""))
    for expr in function_expressions
    if isinstance(expr, unreal.MaterialExpressionFunctionInput)
)
all_contract_names.update(custom_names)
retired_found = sorted(OBSOLETE.intersection(all_contract_names))
if retired_found:
    raise RuntimeError(f"Retired v9 names still exist: {retired_found}")

unreal.MaterialEditingLibrary.recompile_material(material)
unreal.log(
    "CODEX_V9_VALIDATE|SUCCESS|"
    f"function_expressions={len(function_expressions)}|"
    f"material_expressions={len(material_expressions)}|"
    f"custom_inputs={len(custom_names)}|caller_inputs={len(call_names)}|"
    f"world_sizes_km={','.join(f'{k}:{v:.6f}' for k, v in EXPECTED_WORLD_SIZES_KM.items())}"
)
unreal.log("CODEX_V9_VALIDATE_COMPLETE")
