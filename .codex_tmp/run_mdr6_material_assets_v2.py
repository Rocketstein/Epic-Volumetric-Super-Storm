from pathlib import Path


script_path = Path(__file__).with_name("apply_mdr6_material_assets.py")
source = script_path.read_text(encoding="utf-8")

replacements = {
    """    radius_mask,
    "Input",
    "StormCenterRadius -> radius B mask",""": """    radius_mask,
    "",
    "StormCenterRadius -> radius B mask",""",
    """    connect(
        node,
        "",
        function_call,
        function_input_name,""": """    connect(
        node,
        "RGBA",
        function_call,
        function_input_name,""",
}

for old, new in replacements.items():
    count = source.count(old)
    if count != 1:
        raise RuntimeError(
            f"Expected one runner substitution, found {count}: {old!r}"
        )
    source = source.replace(old, new, 1)

exec(compile(source, str(script_path), "exec"))
