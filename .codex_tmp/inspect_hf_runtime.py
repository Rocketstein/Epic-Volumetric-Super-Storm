import unreal


material = unreal.load_asset("/SavageSuperStorm/Materials/M_SSS")
if material is None:
    raise RuntimeError("Unable to load M_SSS")

for parameter_name in ("HFTex", "LFNoiseTex"):
    texture = unreal.MaterialEditingLibrary.get_material_default_texture_parameter_value(
        material, parameter_name
    )
    unreal.log(f"CODEX_TEXTURE={parameter_name}:{texture.get_path_name() if texture else 'None'}")
    if texture:
        for method_name in ("blueprint_get_size_x", "blueprint_get_size_y", "blueprint_get_size_z"):
            method = getattr(texture, method_name, None)
            if method:
                unreal.log(f"CODEX_TEXTURE_SIZE={parameter_name}:{method_name}:{method()}")

world = unreal.EditorLoadingAndSavingUtils.load_map("/Game/NubisMap")
if world is None:
    raise RuntimeError("Unable to load /Game/NubisMap")

actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
for actor in actor_subsystem.get_all_level_actors():
    for component in actor.get_components_by_class(unreal.VolumetricCloudComponent):
        cloud_material = component.get_editor_property("material")
        unreal.log(
            f"CODEX_CLOUD_MATERIAL={actor.get_actor_label()}:{cloud_material.get_path_name() if cloud_material else 'None'}"
        )
        if isinstance(cloud_material, unreal.MaterialInstanceDynamic):
            parent = cloud_material.get_editor_property("parent")
            unreal.log(f"CODEX_CLOUD_PARENT={parent.get_path_name() if parent else 'None'}")
            for parameter_name in ("HF_Strength", "HF_StrengthAnvil", "HFNoiseWorldSize", "HFNoiseScale"):
                try:
                    value = cloud_material.get_scalar_parameter_value(parameter_name)
                except Exception as error:
                    unreal.log_error(f"CODEX_MID_READ_ERROR={parameter_name}:{error}")
                    continue
                unreal.log(f"CODEX_MID_SCALAR={parameter_name}:{value:.9g}")
