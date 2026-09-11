import json
import unreal


ASSET_PATHS = (
    "/SavageSuperStorm/Materials/Functions/MF_Erosion",
    "/SavageSuperStorm/Materials/Functions/MF_Erosion_Merge",
    "/SavageSuperStorm/Materials/M_SSS",
    "/SavageSuperStorm/Materials/M_A",
)


def prop(obj, name, default=None):
    try:
        return obj.get_editor_property(name)
    except Exception:
        return default


def serial(value):
    if value is None:
        return None
    if isinstance(value, (bool, int, float, str)):
        return value
    return str(value)


def position(expression):
    x, y = unreal.MaterialEditingLibrary.get_material_expression_node_position(
        expression
    )
    return [x, y]


def node_record(expression):
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
        "input_type",
        "sort_priority",
        "parameter_group",
    ):
        value = prop(expression, property_name)
        if value not in (None, ""):
            record[property_name] = serial(value)

    material_function = prop(expression, "material_function")
    if material_function is not None:
        record["material_function"] = material_function.get_path_name()

    declaration = prop(expression, "declaration")
    if declaration is not None:
        record["declaration"] = declaration.get_name()
    return record


def get_expressions(asset, is_function):
    if is_function:
        return list(
            unreal.MaterialEditingLibrary.get_material_function_expressions(asset)
        )
    return list(unreal.MaterialEditingLibrary.get_material_expressions(asset))


def get_input_names(expression):
    return [
        str(value)
        for value in unreal.MaterialEditingLibrary.get_material_expression_input_names(
            expression
        )
    ]


def get_sources(asset, expression, is_function):
    if is_function:
        return list(
            unreal.MaterialEditingLibrary.get_inputs_for_material_function_expression(
                asset, expression
            )
        )
    return list(
        unreal.MaterialEditingLibrary.get_inputs_for_material_expression(
            asset, expression
        )
    )


def source_record(source):
    return None if source is None else node_record(source)


for asset_path in ASSET_PATHS:
    asset = unreal.load_asset(asset_path)
    if asset is None:
        raise RuntimeError(f"Missing asset: {asset_path}")
    is_function = isinstance(asset, unreal.MaterialFunctionInterface)
    expressions = get_expressions(asset, is_function)
    unreal.log_warning(
        "CODEX_MDR_ASSET|"
        + json.dumps(
            {
                "path": asset.get_path_name(),
                "class": asset.get_class().get_name(),
                "expression_count": len(expressions),
            },
            separators=(",", ":"),
        )
    )

    for expression in expressions:
        record = node_record(expression)
        record_text = " ".join(str(value) for value in record.values()).lower()
        material_function = str(record.get("material_function", "")).lower()
        is_relevant = (
            is_function
            or "mdr" in record_text
            or "ring" in record_text
            or "mf_erosion" in material_function
        )
        if not is_relevant:
            continue

        input_names = get_input_names(expression)
        sources = get_sources(asset, expression, is_function)
        record["inputs"] = [
            {
                "index": index,
                "name": input_name,
                "source": source_record(
                    sources[index] if index < len(sources) else None
                ),
            }
            for index, input_name in enumerate(input_names)
        ]
        unreal.log_warning(
            "CODEX_MDR_NODE|"
            + json.dumps(
                {"asset": asset.get_path_name(), "node": record},
                separators=(",", ":"),
            )
        )

        if isinstance(expression, unreal.MaterialExpressionCustom):
            custom_record = {
                "asset": asset.get_path_name(),
                "node": expression.get_name(),
                "code": str(prop(expression, "code", "")),
                "include_file_paths": [
                    str(value)
                    for value in list(prop(expression, "include_file_paths", []) or [])
                ],
                "additional_defines": [
                    str(value)
                    for value in list(prop(expression, "additional_defines", []) or [])
                ],
                "inputs": [],
                "outputs": [],
            }
            for index, custom_input in enumerate(
                list(prop(expression, "inputs", []) or [])
            ):
                custom_record["inputs"].append(
                    {
                        "index": index,
                        "name": str(prop(custom_input, "input_name", "")),
                        "source": source_record(
                            sources[index] if index < len(sources) else None
                        ),
                    }
                )
            for index, custom_output in enumerate(
                list(prop(expression, "additional_outputs", []) or [])
            ):
                custom_record["outputs"].append(
                    {
                        "index": index,
                        "name": str(prop(custom_output, "output_name", "")),
                        "type": str(prop(custom_output, "output_type", "")),
                    }
                )
            unreal.log_warning(
                "CODEX_MDR_CUSTOM|"
                + json.dumps(custom_record, separators=(",", ":"))
            )

unreal.log_warning("CODEX_MDR_INSPECT_COMPLETE")
