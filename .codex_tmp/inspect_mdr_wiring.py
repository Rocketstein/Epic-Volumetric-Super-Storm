import json
import unreal


MDR_INPUTS = {
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
}


def prop(obj, name, default=None):
    try:
        return obj.get_editor_property(name)
    except Exception:
        return default


def position(expression):
    x, y = unreal.MaterialEditingLibrary.get_material_expression_node_position(
        expression
    )
    return [x, y]


def node_record(expression):
    if expression is None:
        return None
    record = {
        "class": expression.get_class().get_name(),
        "object": expression.get_name(),
        "position": position(expression),
    }
    for property_name in (
        "input_name",
        "output_name",
        "parameter_name",
        "declaration_name",
        "name",
        "desc",
        "default_value",
        "constant",
        "const_a",
        "const_b",
        "r",
        "g",
        "b",
        "a",
        "sort_priority",
    ):
        value = prop(expression, property_name)
        if value not in (None, ""):
            record[property_name] = str(value)
    declaration = prop(expression, "declaration")
    if declaration is not None:
        record["declaration"] = declaration.get_name()
    material_function = prop(expression, "material_function")
    if material_function is not None:
        record["material_function"] = material_function.get_path_name()
    return record


def input_edges(material, expression):
    names = [
        str(value)
        for value in unreal.MaterialEditingLibrary.get_material_expression_input_names(
            expression
        )
    ]
    sources = list(
        unreal.MaterialEditingLibrary.get_inputs_for_material_expression(
            material, expression
        )
    )
    return [
        {
            "index": index,
            "name": name,
            "source": node_record(
                sources[index] if index < len(sources) else None
            ),
        }
        for index, name in enumerate(names)
    ]


def emit(marker, payload):
    unreal.log_warning(
        marker + "|" + json.dumps(payload, separators=(",", ":"))
    )


def find_call(material, function_name):
    calls = []
    for expression in unreal.MaterialEditingLibrary.get_material_expressions(
        material
    ):
        if not isinstance(
            expression, unreal.MaterialExpressionMaterialFunctionCall
        ):
            continue
        function = prop(expression, "material_function")
        if function is not None and function.get_name() == function_name:
            calls.append(expression)
    if len(calls) != 1:
        raise RuntimeError(
            f"Expected one {function_name} call in {material.get_name()}, "
            f"found {len(calls)}"
        )
    return calls[0]


def walk_upstream(material, expression, visited, depth=0):
    if expression is None or expression.get_name() in visited or depth > 8:
        return
    visited.add(expression.get_name())
    record = node_record(expression)
    record["inputs"] = input_edges(material, expression)
    emit(
        "CODEX_MDR_WIRE_NODE",
        {"asset": material.get_path_name(), "node": record},
    )
    declaration = prop(expression, "declaration")
    if declaration is not None:
        walk_upstream(material, declaration, visited, depth + 1)
    for edge in record["inputs"]:
        source_name = (
            edge["source"]["object"] if edge["source"] is not None else None
        )
        if source_name is None:
            continue
        source = next(
            (
                candidate
                for candidate in unreal.MaterialEditingLibrary.get_material_expressions(
                    material
                )
                if candidate.get_name() == source_name
            ),
            None,
        )
        walk_upstream(material, source, visited, depth + 1)


m_a = unreal.load_asset("/SavageSuperStorm/Materials/M_A")
m_sss = unreal.load_asset("/SavageSuperStorm/Materials/M_SSS")
if m_a is None or m_sss is None:
    raise RuntimeError("M_A or M_SSS is missing")

m_a_call = find_call(m_a, "MF_Erosion_Merge")
m_a_edges = input_edges(m_a, m_a_call)
for edge in m_a_edges:
    if edge["name"] not in MDR_INPUTS:
        continue
    emit("CODEX_MDR_WIRE_ROOT", {"asset": m_a.get_path_name(), "edge": edge})
    source_name = (
        edge["source"]["object"] if edge["source"] is not None else None
    )
    source = next(
        (
            candidate
            for candidate in unreal.MaterialEditingLibrary.get_material_expressions(
                m_a
            )
            if candidate.get_name() == source_name
        ),
        None,
    )
    walk_upstream(m_a, source, set())

m_sss_call = find_call(m_sss, "MF_Erosion")
for edge in input_edges(m_sss, m_sss_call):
    if edge["name"] not in ("WorldPosition", "StormCenterRadius"):
        continue
    emit(
        "CODEX_MDR_WIRE_ROOT",
        {"asset": m_sss.get_path_name(), "edge": edge},
    )
    source_name = (
        edge["source"]["object"] if edge["source"] is not None else None
    )
    source = next(
        (
            candidate
            for candidate in unreal.MaterialEditingLibrary.get_material_expressions(
                m_sss
            )
            if candidate.get_name() == source_name
        ),
        None,
    )
    walk_upstream(m_sss, source, set())

for expression in unreal.MaterialEditingLibrary.get_material_expressions(m_sss):
    if not isinstance(
        expression,
        (
            unreal.MaterialExpressionNamedRerouteDeclaration,
            unreal.MaterialExpressionNamedRerouteUsage,
        ),
    ):
        continue
    record = node_record(expression)
    record["inputs"] = input_edges(m_sss, expression)
    emit(
        "CODEX_MDR_WIRE_REROUTE",
        {"asset": m_sss.get_path_name(), "node": record},
    )

unreal.log_warning("CODEX_MDR_WIRING_INSPECT_COMPLETE")
