import unreal


MATERIAL_PATH = "/SavageSuperStorm/Materials/M_SSS"
FUNCTION_PATH = "/SavageSuperStorm/Materials/Functions/MF_Erosion"
STALE_NAME = "SpiralTendrilMask"
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


def call_contract(material, expressions):
    calls = []
    for expr in expressions:
        if not isinstance(expr, unreal.MaterialExpressionMaterialFunctionCall):
            continue
        called = prop(expr, "material_function")
        if called is not None and called.get_path_name().startswith(FUNCTION_PATH):
            calls.append(expr)
    if len(calls) != 1:
        raise RuntimeError(f"Expected one MF_Erosion call, found {len(calls)}")

    call = calls[0]
    names = [
        str(value)
        for value in unreal.MaterialEditingLibrary.get_material_expression_input_names(call)
    ]
    sources = list(
        unreal.MaterialEditingLibrary.get_inputs_for_material_expression(material, call)
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
    return call


def consumers_of(material, expressions, candidate):
    consumers = []
    for expr in expressions:
        if expr == candidate:
            continue
        sources = list(
            unreal.MaterialEditingLibrary.get_inputs_for_material_expression(
                material, expr
            )
        )
        if any(source == candidate for source in sources):
            consumers.append(expr)
    return consumers


material = unreal.load_asset(MATERIAL_PATH)
if material is None:
    raise RuntimeError("M_SSS was not found")

expressions = list(unreal.MaterialEditingLibrary.get_material_expressions(material))
call_contract(material, expressions)

declarations = [
    expr
    for expr in expressions
    if isinstance(expr, unreal.MaterialExpressionNamedRerouteDeclaration)
    and str(prop(expr, "name", "")) == STALE_NAME
]
if len(declarations) != 1:
    raise RuntimeError(
        f"Expected one {STALE_NAME} declaration, found {len(declarations)}"
    )
declaration = declarations[0]

usages = [
    expr
    for expr in expressions
    if isinstance(expr, unreal.MaterialExpressionNamedRerouteUsage)
    and prop(expr, "declaration") == declaration
]
if not usages:
    raise RuntimeError(f"No usages reference the {STALE_NAME} declaration")

for usage in usages:
    consumers = consumers_of(material, expressions, usage)
    if consumers:
        raise RuntimeError(
            f"Refusing to delete used {STALE_NAME} reroute {usage.get_name()}: "
            + ",".join(expr.get_name() for expr in consumers)
        )

material.modify()
for usage in usages:
    unreal.MaterialEditingLibrary.delete_material_expression(material, usage)
unreal.MaterialEditingLibrary.delete_material_expression(material, declaration)

remaining = list(unreal.MaterialEditingLibrary.get_material_expressions(material))
call_contract(material, remaining)
remaining_stale = []
for expr in remaining:
    for property_name in (
        "name",
        "declaration_name",
        "parameter_name",
        "input_name",
        "desc",
    ):
        if STALE_NAME in str(prop(expr, property_name, "")):
            remaining_stale.append(f"{expr.get_name()}.{property_name}")
if remaining_stale:
    raise RuntimeError(f"Stale Tendril expressions remain: {remaining_stale}")

if not unreal.EditorAssetLibrary.save_loaded_asset(material, False):
    raise RuntimeError("Failed to save M_SSS")

unreal.log(
    "CODEX_V10_MSSS_CLEANUP|SUCCESS|"
    f"deleted_declarations={len(declarations)}|deleted_usages={len(usages)}|"
    f"caller_inputs={len(EXPECTED_CALL_INPUTS)}"
)
unreal.log("CODEX_V10_MSSS_CLEANUP_COMPLETE")
