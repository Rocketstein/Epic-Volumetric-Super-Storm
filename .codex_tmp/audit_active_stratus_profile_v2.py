import os
import unreal


def path(obj):
    return obj.get_path_name() if obj is not None else "<None>"


def prop(obj, name, default=None):
    try:
        return obj.get_editor_property(name)
    except Exception:
        return default


world = unreal.EditorLoadingAndSavingUtils.load_map("/Game/NubisMap")
actors = [
    actor
    for actor in unreal.EditorLevelLibrary.get_all_level_actors()
    if isinstance(actor, unreal.VolumetricSuperStormActor)
]
if len(actors) != 1:
    raise RuntimeError(f"Expected one storm actor, got {len(actors)}")

actor = actors[0]
tool = actor.get_component_by_class(unreal.StormVerticalProfileToolComponent)
binder = actor.get_material_binder()
params = prop(tool, "storm_profile_params")

# Reassigning the same reflected struct invokes PostEditChangeProperty on the
# component, which rebuilds the parametric top into the existing live RT.
tool.set_editor_property("storm_profile_params", params)
actor.rebuild_render_data()
actor.notify_render_data_updated()
binder.update_volumetric_cloud_material()

mid = binder.get_dynamic_cloud_material()
cloud = binder.resolve_target_cloud()
top_rt = prop(tool, "top_type_profile_rt")
bottom_rt = prop(tool, "bottom_type_profile_rt")

try:
    bound_top = mid.get_texture_parameter_value("TopVerticalProfileRT")
except Exception as exc:
    bound_top = f"<error:{type(exc).__name__}:{exc}>"

unreal.log_warning(
    "CODEX_STRATUS_ACTIVE|"
    f"actor={path(actor)}|tool={path(tool)}|"
    f"stratus={prop(params, 'stratus_height')}|"
    f"stratus_floor={prop(params, 'stratus_top_height_floor')}|"
    f"use_curve={prop(params, 'use_vertical_profile_curve')}|"
    f"top_rt={path(top_rt)}|bottom_rt={path(bottom_rt)}|"
    f"baked={path(prop(tool, 'baked_profile_asset'))}|"
    f"cloud={path(cloud)}|mid={path(mid)}|bound_top={path(bound_top) if not isinstance(bound_top, str) else bound_top}"
)

output_dir = os.path.join(unreal.Paths.project_saved_dir(), "CodexLiveProfile")
os.makedirs(output_dir, exist_ok=True)
unreal.KismetRenderingLibrary.export_render_target(
    world, top_rt, output_dir, "LiveTopProfile"
)
unreal.log_warning(f"CODEX_STRATUS_EXPORT|dir={output_dir}|rt={path(top_rt)}")
unreal.log_warning("CODEX_STRATUS_ACTIVE_COMPLETE")
