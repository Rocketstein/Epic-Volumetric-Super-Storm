import unreal


MATERIAL_PATH = "/SavageSuperStorm/Materials/M_SSS"
FUNCTION_PATH = "/SavageSuperStorm/Materials/Functions/MF_Erosion"
STALE_NAMES = {
    "SpiralTendrilMask",
    "SpiralTendrilStrength",
    "AnvilWindDirectionXY",
    "AnvilWindStretch",
    "AnvilWindSkew",
    "AnvilWarpStart01",
    "AnvilWarpEnd01",
}
EXPECTED_CALL_INPUTS = [
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
    "CurlTiling",
    "CurlDisplacementUVW",
    "StormLifecycleControl",
    "DensityGamma",
    "HFStrength",
]


def prop(obj, name, default=None):
    try:
        return obj.get_editor_property(name)
    except Exception:
        return default


material = unreal.load_asset(MATERIAL_PATH)
if material is None:
    raise RuntimeError("M_SSS was not found")
expressions = list(unreal.MaterialEditingLibrary.get_material_expressions(material))

stale = []
for expr in expressions:
    for property_name in (
        "name",
        "declaration_name",
        "parameter_name",
        "input_name",
        "desc",
    ):
        value = str(prop(expr, property_name, ""))
        if any(name in value for name in STALE_NAMES):
            stale.append(f"{expr.get_name()}.{property_name}={value}")
if stale:
    raise RuntimeError(f"Retired v10 material expressions remain: {stale}")

calls = []
for expr in expressions:
    if not isinstance(expr, unreal.MaterialExpressionMaterialFunctionCall):
        continue
    called = prop(expr, "material_function")
    if called is not None and called.get_path_name().startswith(FUNCTION_PATH):
        calls.append(expr)
if len(calls) != 1:
    raise RuntimeError(f"Expected one MF_Erosion call, found {len(calls)}")

names = [
    str(value)
    for value in unreal.MaterialEditingLibrary.get_material_expression_input_names(calls[0])
]
sources = list(
    unreal.MaterialEditingLibrary.get_inputs_for_material_expression(material, calls[0])
)
if names != EXPECTED_CALL_INPUTS:
    raise RuntimeError(f"MF_Erosion call input mismatch: {names}")
missing = [
    names[index]
    for index in range(len(names))
    if index >= len(sources) or sources[index] is None
]
if missing:
    raise RuntimeError(f"Disconnected MF_Erosion call inputs: {missing}")

unreal.log(
    "CODEX_V10_MSSS_VALIDATE|SUCCESS|"
    f"expressions={len(expressions)}|caller_inputs={len(names)}|missing=0|stale=0"
)
unreal.log("CODEX_V10_MSSS_VALIDATE_COMPLETE")
