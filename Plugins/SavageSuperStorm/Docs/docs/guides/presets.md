# Presets

A Storm Preset data asset stores a reusable copy of the storm's complete
authorable configuration.

## Stored values

- Shape settings, including radial curves, density, lighting, and wind.
- Motion settings.
- Formation and dissolution durations.
- Lightning sequence, repeat delay, and ground-strike probability.
- Vertical profile asset and profile transition settings.
- Flow-map asset and enabled state.

Profile and flow-map pixels are not embedded in the preset. The preset stores
references to saved assets, so transient painter surfaces must be saved before
creating the preset.

A preset does not store the actor transform, target cloud reference, base cloud
material, or Volumetric Cloud component settings.

## Applying a preset

`ApplyPreset` copies values into the actor, updates the profile and flow-map
components, sanitizes the result, and performs a full render-data rebuild. The
actor does not retain a live link to the preset asset.

## Editor workflow

1. Configure the storm actor.
2. Save the vertical profile and flow map when they should be part of the preset.
3. Use **Save Preset As** from the actor's Details panel.
4. Select a Storm Preset asset with **Load Preset** to copy it into another storm.

Volumetric Cloud layer altitude, layer height, tracing, and quality settings
remain properties of the cloud component and must be configured separately.
