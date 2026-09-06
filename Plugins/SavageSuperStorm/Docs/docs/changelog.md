# Changelog

All notable changes to Savage Super Storm are recorded here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and versions follow
[Semantic Versioning](https://semver.org/spec/v2.0.0.html).

!!! info "What counts as a breaking change"
    The material parameter contract is part of the plugin's public surface. A
    change that adds, renames, or removes a contract parameter requires a major
    version, because a cloud material authored against the old contract stops
    binding.

## 1.0.0 — unreleased

First public release for Unreal Engine 5.8.

### Added

**Storm actor.** `Volumetric Super Storm Actor` orchestrates shape, motion,
lightning, formation, vertical profiles, and flow maps from a single actor, and
binds the target Volumetric Cloud material without manual wiring. See
[Storm Actor](reference/actor.md).

**Shape authoring.** Radius, extent, envelope, anvil, density, and lighting
controls, plus radial curves for coverage, cloud type, and min/max layer height.
Shape bakes to two RGBA16F render targets at 512–2048 resolution. See
[Core Parameters](guides/core-parameters.md).

**Motion system.** Up to six concentric rotation rings with per-ring angular
speed and skew, boundary blending, radial and vertical motion masks, and
independent low-frequency, high-frequency, and curl rotation multipliers.

**Vertical profile painter.** Paints bottom, top, and anvil profiles in
normalized cloud-layer space, with composite preview, undo, and saving to a
Vertical Profile asset. Runtime transitions blend all three surfaces between
profile assets in one render graph. See
[Vertical Profile](guides/vertical-profile.md).

**Wind flow-map editor.** Paints lower, middle, and upper flow layers at 32–2048
resolution, in drag-direction or encoded-RGBA mode, with a layer-authority
ribbon controlling each layer's height of influence. Includes a vertical flow
component for 2.5D advection. See [Flow Map](guides/flow-map.md).

**Lightning.** Authored cue sequences drive material pulses and ground-strike
resolution against Landscape height, with repeat delay and strike probability.
Start, cue, strike, pulse, and finish events are exposed to Blueprint. Ships a
Niagara lightning system, lightning material, and thunder audio.

**Formation and dissolution.** Timed create and dissolve animations with
completion events, plus immediate show and hide.

**Presets.** A Storm Preset asset captures the complete authorable
configuration — shape, motion, lightning, formation, and profile and flow-map
references — and applies onto any storm actor. See [Presets](guides/presets.md).

**Blueprint API.** 72 exposed nodes — 44 callable, 28 pure — plus 7 assignable
events, covering preset application, render rebuild, formation, motion control,
profile transitions, lightning, and material binding. See
[Blueprint API](reference/blueprint-api.md).

**Editor integration.** Storm actor Details customization, a Wind Flow Map asset
editor, a component visualizer for motion ring ranges, and an editor viewport
that previews storm motion without entering PIE.

**Content.** Cloud material `M_SSS` implementing the storm parameter contract,
supporting material functions, Perlin-Worley and curl noise textures, default
vertical profiles, and one sample flow map and preset.

### Known limitations

- One registered storm actor per world; additional actors are disabled rather
  than sharing shape render targets.
- Automatic Volumetric Cloud targeting requires exactly one cloud component in
  the level. Otherwise `TargetCloudReference` must be set explicitly.
- Storm heights are normalized to the Volumetric Cloud layer. Absolute altitude
  and thickness remain properties of the cloud component.
- Profile edits made during PIE use separate transient render targets and do not
  persist to the editor world.
- Two persistent RGBA16F shape targets cost roughly 64 MiB together at 2048
  resolution.

The full list, with the reasoning behind each, is on the
[Limitations](limitations.md) page.
