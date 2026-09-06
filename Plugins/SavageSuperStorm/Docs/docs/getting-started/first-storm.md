# Your First Storm

## 1. Prepare the sky

Add these actors to the level:

- Directional Light
- Sky Atmosphere
- Volumetric Cloud

The storm renders through the existing Volumetric Cloud component; it does not
create a separate volumetric renderer.

## 2. Place the storm actor

Place `Volumetric Super Storm Actor` in the level. A world supports one
registered storm actor. Remove or disable any additional storm actor before
continuing.

## 3. Bind the cloud material

Select the actor's **Material Binder** component.

- Set **Target Cloud Reference** when the world has more than one Volumetric
  Cloud component.
- Set **Base Cloud Material** to `M_SSS`.
- Press **Apply Volumetric Cloud Material**.

When the target reference is empty, automatic selection succeeds only when the
world contains exactly one Volumetric Cloud component.

The binder stores the material that was active before binding and restores it
when the storm unregisters or is destroyed.

## 4. Shape the storm

The main controls are:

| Group | Purpose |
|---|---|
| Shape Settings | Radius, extent, anvil, radial curves, density, lighting, and wind. |
| Motion Settings | Ring rotation, masks, height range, and frequency-band motion. |
| Lightning | Sequence, repeat delay, and ground-strike probability. |
| Formation | Formation and dissolution duration. |

Useful first adjustments:

- **Radius** changes horizontal size.
- **Anvil Coverage** changes the complete outer radius.
- **Anvil Strength** controls the anvil contribution.
- **Density** and **HF Strength** change body density and erosion detail.
- **Underside Visibility** changes the lower lighting response.

Static shape changes schedule a shape rebake. Motion and lifecycle changes use
frame-level material uploads.

## 5. Animate formation

Use the Blueprint nodes **Create Storm**, **Dissolve Storm**,
**Show Mature Storm**, and **Hide Storm Immediately**. Formation and dissolution
completion events are exposed on the actor.

## 6. Author profiles and flow

- Open the vertical profile painter to edit bottom, top, and anvil profiles.
- Open the flow-map editor to paint lower, middle, and upper flow layers.
- Save painter output to assets before storing those references in a preset.

## Next

- [Core Parameters](../guides/core-parameters.md)
- [Flow Map](../guides/flow-map.md)
- [Vertical Profile](../guides/vertical-profile.md)
- [Presets](../guides/presets.md)
