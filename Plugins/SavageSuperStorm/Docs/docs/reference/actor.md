# Storm Actor

`AVolumetricSuperStormActor` is the single storm orchestrator supported by one
world. It owns the authorable state and coordinates rendering, motion, profiles,
flow maps, lightning, formation, and the cloud material binder.

A world accepts one registered storm actor. An additional actor is disabled
instead of replacing the active storm or sharing mutable render state.

## Components

| Component | Purpose |
|---|---|
| `SceneRoot` | Root transform for the storm actor. |
| `MaterialBinder` | Creates and updates the target cloud's dynamic material instance. |
| `LightningComponent` | Plays the authored lightning sequence and publishes lightning events. |
| `VerticalProfileTool` | Owns the live bottom, top, and anvil profile render targets. |
| `FlowMapComponent` | Owns the three flow-map layers and their live render targets. |
| `EditorIcon` | Editor-only viewport selection icon. |

## Authorable settings

| Property | Type | Purpose |
|---|---|---|
| `StableStormId` | `int32` | Shared deterministic seed for motion and lightning. Zero derives the seed from the actor name. |
| `ShapeSettings` | `FStormShapeSettings` | Shape bake, anvil, density, lighting, wind, and appearance controls. |
| `MotionSettings` | `FStormMotionSettings` | Ring motion, masks, height range, frequency-band rotation, and shear. |
| `LightningSequence` | `FStormLightningSequenceSettings` | Lightning cue sequence and material pulse behavior. |
| `LightningRepeatDelaySeconds` | `float` | Delay between automatically repeated lightning sequences. |
| `GroundStrikeProbability` | `float` | Probability that an eligible lightning sequence produces a ground strike. |
| `FormationDurationSeconds` | `float` | Formation animation duration. |
| `DissolutionDurationSeconds` | `float` | Dissolution animation duration. |

See [Core Parameters](../guides/core-parameters.md) for the complete shape and
motion controls.

## Runtime behavior

The actor registers with `UStormRenderWorldSubsystem`, builds one immutable
shape-bake input set, and uploads the resulting shape textures to the target
cloud material. Static changes rebuild the shape data. Profile changes update
only profile textures. Motion, transform, lifecycle, and lightning changes use
frame-level material uploads.

The actor ticks while time-dependent state is active. There is no public tick
or simulation-mode switch.

## Main functions

| Function | Purpose |
|---|---|
| `PrepareStormResources` | Registers the actor and initializes render and material resources. |
| `RebuildRenderData` | Rebuilds static render data and refreshes the target material. |
| `ApplyPreset` | Copies a preset into the actor and rebuilds runtime state. |
| `Create Storm` | Starts formation. |
| `Dissolve Storm` | Starts dissolution. |
| `ShowMatureStorm` | Immediately shows the mature state. |
| `HideStormImmediately` | Immediately hides the storm. |
| `PlayLightningSequence` | Starts the configured lightning sequence. |
| `SetProfileTransitionTarget` | Selects the destination profile for blending. |
| `SetStormMotionEnabled` | Enables or disables shader motion. |
| `PauseStormMotion` / `ResumeStormMotion` | Controls motion time integration. |
| `ResetStormMotion` | Resets motion time and ring phases. |

## See also

- [Blueprint API](blueprint-api.md)
- [Components](components.md)
- [Presets](../guides/presets.md)
- [Limitations](../limitations.md)
