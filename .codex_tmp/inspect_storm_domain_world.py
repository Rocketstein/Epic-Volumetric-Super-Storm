import json
import unreal


def prop(obj, name, default=None):
    try:
        return obj.get_editor_property(name)
    except Exception:
        return default


def node_record(material, expression):
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
        "class": expression.get_class().get_name(),
        "object": expression.get_name(),
    }
    for name in (
        "name",
        "parameter_name",
        "desc",
        "default_value",
        "constant",
        "const_a",
        "const_b",
        "r",
        "g",
        "b",
        "a",
    ):
        value = prop(expression, name)
        if value not in (None, ""):
            record[name] = str(value)
    record["inputs"] = [
        {
            "name": input_name,
            "source": (
                sources[index].get_name()
                if index < len(sources) and sources[index] is not None
                else None
            ),
        }
        for index, input_name in enumerate(names)
    ]
    return record


for asset_path in (
    "/SavageSuperStorm/Materials/M_A",
    "/SavageSuperStorm/Materials/M_SSS",
):
    material = unreal.load_asset(asset_path)
    expressions = list(
        unreal.MaterialEditingLibrary.get_material_expressions(material)
    )
    by_name = {expression.get_name(): expression for expression in expressions}
    declaration = next(
        expression
        for expression in expressions
        if isinstance(
            expression, unreal.MaterialExpressionNamedRerouteDeclaration
        )
        and str(prop(expression, "name", "")) == "StormDomainWorld"
    )
    pending = [declaration]
    visited = set()
    while pending:
        expression = pending.pop()
        if expression.get_name() in visited:
            continue
        visited.add(expression.get_name())
        record = node_record(material, expression)
        unreal.log_warning(
            "CODEX_DOMAIN|"
            + json.dumps(
                {"asset": material.get_path_name(), "node": record},
                separators=(",", ":"),
            )
        )
        for edge in record["inputs"]:
            source = by_name.get(edge["source"])
            if source is not None:
                pending.append(source)
unreal.log_warning("CODEX_DOMAIN_COMPLETE")
