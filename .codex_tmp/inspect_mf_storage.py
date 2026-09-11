import unreal


def show_property(obj, name):
    try:
        value = obj.get_editor_property(name)
        unreal.log_warning(
            f"CODEX_PROP_OK|owner={obj.get_class().get_name()}|name={name}|type={type(value)}|value={value}"
        )
        return value
    except Exception as exc:
        unreal.log_warning(
            f"CODEX_PROP_FAIL|owner={obj.get_class().get_name()}|name={name}|error={exc}"
        )
        return None


function = unreal.load_asset("/SavageSuperStorm/Materials/Functions/MF_Erosion")
if function is None:
    raise RuntimeError("MF_Erosion not found")

unreal.log_warning(
    f"CODEX_FUNCTION|class={function.get_class().get_name()}|path={function.get_path_name()}"
)
unreal.log_warning(
    "CODEX_FUNCTION_DIR|" + ",".join(name for name in dir(function) if not name.startswith("__"))
)

for candidate in (
    "editor_only_data",
    "expression_collection",
    "function_expressions",
    "expressions",
    "function_editor_only_data",
    "description",
):
    value = show_property(function, candidate)
    if value is not None and not isinstance(value, (str, int, float, bool)):
        unreal.log_warning(
            f"CODEX_VALUE_DIR|property={candidate}|"
            + ",".join(name for name in dir(value) if not name.startswith("__"))
        )
        for nested in ("expression_collection", "expressions", "comments"):
            show_property(value, nested)

unreal.log_warning(
    "CODEX_MEL_DIR|"
    + ",".join(
        name
        for name in dir(unreal.MaterialEditingLibrary)
        if "expression" in name.lower() or "function" in name.lower()
    )
)
if hasattr(unreal, "MaterialEditorSubsystem"):
    unreal.log_warning(
        "CODEX_MES_DIR|"
        + ",".join(
            name
            for name in dir(unreal.MaterialEditorSubsystem)
            if "expression" in name.lower() or "function" in name.lower()
        )
    )

unreal.log_warning("CODEX_INSPECT_STORAGE_COMPLETE")
