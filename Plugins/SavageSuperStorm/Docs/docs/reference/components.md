# Components

## Storm Material Binder

`UStormMaterialBinderComponent` binds the registered storm to one Volumetric
Cloud component. It accepts only a material implementing the complete storm
parameter contract.

### Target selection

`TargetCloudReference` is the preferred target. When it is unset, automatic
selection succeeds only when the world contains exactly one Volumetric Cloud
component. An ambiguous world is rejected rather than selecting an arbitrary
cloud.

### Material ownership

The binder stores the material active before binding, creates a dynamic instance
owned by the component, assigns it to the cloud, and restores the previous
material when the storm unregisters or is destroyed. External material changes
can be adopted as the new parent while editing when
`bTrackTargetCloudMaterialChange` is enabled.

### Properties

| Property | Purpose |
|---|---|
| `TargetCloudReference` | Explicit cloud component target. |
| `BaseCloudMaterial` | Preferred parent material. It must implement the storm material contract. |
| `bTrackTargetCloudMaterialChange` | Editor-only tracking of external target material changes. |

### Update paths

| Function | Purpose |
|---|---|
| `PushStormRenderData` | Uploads textures and all static and frame parameters. |
| `PushStormFrameData` | Uploads transform, motion, lifecycle, and time-dependent values only. |
| `PushStormProfileData` | Uploads bottom, top, and anvil profile textures only. |
| `ApplyVolumetricCloudMaterial` | Creates or recreates the dynamic material and uploads the current state. |
| `UpdateVolumetricCloudMaterial` | Reuses the current dynamic instance and refreshes its state. |
| `ReleaseVolumetricCloudMaterial` | Restores the material that was active before binding. |

## Storm Lightning Component

`UStormLightningComponent` evaluates the lightning cue sequence, updates the
material pulse, resolves ground-strike locations, and publishes start, cue,
strike, pulse, and finish events. Motion and lightning use the same resolved
storm identifier.

## Storm Flow Map Component

`UStormFlowMapComponent` owns the lower, middle, and upper painted flow layers,
their live render targets, and the persistent flow-map asset. Painting the upper
layer marks the storm shape bake dirty because the shape compute pass samples
that layer.

See [Flow Map](../guides/flow-map.md).

## Storm Vertical Profile Tool Component

`UStormVerticalProfileToolComponent` owns live bottom, top, and anvil profile
render targets. Profile transitions blend all three surfaces in one render graph
and then update only the profile texture bindings; they do not resubmit or
rebake the storm shape.

See [Vertical Profile](../guides/vertical-profile.md).

## Storm Render World Subsystem

`UStormRenderWorldSubsystem` enforces the one-storm-per-world rule and owns the
two shape render targets. It caches sampled curve lookup tables with their bake
hash so each curve is sampled once per full update.
