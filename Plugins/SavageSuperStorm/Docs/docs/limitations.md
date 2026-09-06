# Limitations

## One storm per world

The renderer intentionally supports one registered storm actor in each world.
The world subsystem rejects an additional actor instead of sharing or replacing
shape render targets. Use separate worlds or streamed world instances when two
independent storms must be rendered simultaneously.

## One unambiguous Volumetric Cloud target

The plugin renders through Unreal's native Volumetric Cloud component. Set
`TargetCloudReference` explicitly when the world contains more than one cloud.
Automatic targeting succeeds only when exactly one candidate exists.

The binder replaces the target cloud's material with a dynamic instance while
the storm is active. It stores and restores the previous material when the storm
unregisters or is destroyed.

## Cloud-layer ownership

Vertical placement is defined by the Volumetric Cloud component's layer bottom
altitude and layer height. Storm profiles and shape heights use normalized layer
space from zero at the layer bottom to one at the layer top.

Moving the storm actor vertically does not move the cloud layer. Change the
Volumetric Cloud layer settings to change absolute altitude or thickness.

## Native renderer constraints

Tracing distance, quality, platform support, and project-level
`r.VolumetricCloud.*` settings come from Unreal's cloud renderer. These settings
can change appearance and performance between editor, PIE, and packaged builds.

## Persistent render-target cost

The subsystem owns two RGBA16F shape targets. At the maximum supported 2048
resolution they use roughly 64 MiB together. Flow maps and profile targets add
to that amount, so high resolutions should be reserved for shots that need the
extra detail.
