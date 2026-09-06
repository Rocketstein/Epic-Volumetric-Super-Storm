# Troubleshooting

## The storm is invisible

Confirm all of the following:

- The level contains a Volumetric Cloud component.
- The storm actor is the only registered storm in the world.
- `TargetCloudReference` identifies the intended cloud when several clouds exist.
- `BaseCloudMaterial` implements the complete storm material contract.
- The cloud has valid layer altitude and layer height settings.

The binder logs an error when the target is ambiguous or the material contract
is incomplete.

## A second storm does not render

This is intentional. The world subsystem accepts one storm actor and rejects an
additional actor rather than sharing shape render targets. Keep one storm in the
world or isolate storms in separate world instances.

## The cloud material changes back when the storm is removed

This is intentional. The binder restores the material that was active before it
created its dynamic instance.

## Profile edits made during PIE disappear

PIE owns separate transient profile render targets. Save profile changes to a
profile asset in the editor world when they must persist.

## A profile transition fades toward empty density

Verify that the target asset contains valid bottom, top, and anvil profile
textures. A black or missing target surface produces an empty density response.

## Shape edits appear delayed

A static shape edit updates the bake input and is processed by the world
subsystem on its next tick. Profile, motion, lifecycle, and lightning updates do
not wait for a shape rebake.
