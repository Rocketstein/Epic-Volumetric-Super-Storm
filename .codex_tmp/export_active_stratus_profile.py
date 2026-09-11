import os
import unreal


world = unreal.EditorLoadingAndSavingUtils.load_map("/Game/NubisMap")
actor = next(
    actor
    for actor in unreal.EditorLevelLibrary.get_all_level_actors()
    if isinstance(actor, unreal.VolumetricSuperStormActor)
)
tool = actor.get_component_by_class(unreal.StormVerticalProfileToolComponent)
params = tool.get_editor_property("storm_profile_params")
tool.set_editor_property("storm_profile_params", params)
actor.rebuild_render_data()
actor.notify_render_data_updated()
output_dir = os.path.join(unreal.Paths.project_saved_dir(), "CodexLiveProfile")
os.makedirs(output_dir, exist_ok=True)
unreal.RenderingLibrary.export_render_target(
    world,
    tool.get_editor_property("top_type_profile_rt"),
    output_dir,
    "LiveTopProfile",
)
unreal.log_warning(f"CODEX_STRATUS_EXPORT_OK|dir={output_dir}")
