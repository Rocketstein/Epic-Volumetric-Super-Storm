import json
import unreal


def prop(obj, name, default=None):
    try:
        return obj.get_editor_property(name)
    except Exception as error:
        return f"<ERROR {error}>"


def pos(expression):
    return list(
        unreal.MaterialEditingLibrary.get_material_expression_node_position(
            expression
        )
    )


def emit(payload):
    unreal.log_warning(
        "CODEX_REROUTE|" + json.dumps(payload, separators=(",", ":"))
    )


for asset_path in (
    "/SavageSuperStorm/Materials/M_A",
    "/SavageSuperStorm/Materials/M_SSS",
):
    material = unreal.load_asset(asset_path)
    if material is None:
        raise RuntimeError(f"Missing {asset_path}")
    expressions = list(
        unreal.MaterialEditingLibrary.get_material_expressions(material)
    )
    for expression in expressions:
        if not isinstance(
            expression,
            (
                unreal.MaterialExpressionNamedRerouteDeclaration,
                unreal.MaterialExpressionNamedRerouteUsage,
            ),
        ):
            continue
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
        record = {
            "asset": material.get_path_name(),
            "class": expression.get_class().get_name(),
            "object": expression.get_name(),
            "position": pos(expression),
            "name": str(prop(expression, "name", "")),
            "declaration": str(prop(expression, "declaration", "")),
            "declaration_guid": str(
                prop(expression, "declaration_guid", "")
            ),
            "variable_guid": str(prop(expression, "variable_guid", "")),
            "inputs": [
                {
                    "index": index,
                    "name": name,
                    "source_class": (
                        sources[index].get_class().get_name()
                        if index < len(sources)
                        and sources[index] is not None
                        else None
                    ),
                    "source_object": (
                        sources[index].get_name()
                        if index < len(sources)
                        and sources[index] is not None
                        else None
                    ),
                }
                for index, name in enumerate(names)
            ],
        }
        emit(record)

unreal.log_warning("CODEX_REROUTE_COMPLETE")
