import unreal


unreal.log(
    "CODEX_TEXTURE_EXPORTERS="
    + ",".join(
        sorted(
            name
            for name in dir(unreal)
            if "export" in name.lower()
            and any(token in name.lower() for token in ("texture", "dds", "hdr", "exr"))
        )
    )
)
unreal.log(
    "CODEX_TEXTURE_SUBSYSTEMS="
    + ",".join(
        sorted(
            name
            for name in dir(unreal)
            if "texture" in name.lower()
            and any(token in name.lower() for token in ("subsystem", "library", "blueprint"))
        )
    )
)

