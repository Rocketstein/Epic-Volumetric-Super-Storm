# Savage Super Storm

A volumetric storm system for Unreal Engine 5.8. It renders one large,
art-directable storm — mesocyclone body, anvil, spiral banding, underside detail
— through the engine's native Volumetric Cloud component, and ships in-editor
painters for authoring its shape, motion, and vertical structure.

- **Full documentation:** https://team-gorogoro.github.io/VolumetricSuperstorm_Docs/
- **Changelog:** [CHANGELOG.md](CHANGELOG.md)
- **Third-party attribution:** [CREDITS.md](CREDITS.md)

## Requirements

| | |
|---|---|
| Unreal Engine | 5.8 |
| Project type | C++ or Blueprint |
| Required plugins | Niagara (bundled lightning VFX) |
| Rendering path | Native Volumetric Cloud |
| Level actors | Directional Light, Sky Atmosphere, Volumetric Cloud |

The plugin runs compute passes that write RGBA16F UAV render targets. The target
RHI must support that alongside Unreal's own Volumetric Cloud renderer.

## Installation

1. Close Unreal Editor.
2. Copy the `SavageSuperStorm` folder into your project's `Plugins` directory.
3. Regenerate project files (C++ projects only).
4. Reopen the project and accept the module rebuild prompt.

```text
YourProject/
  YourProject.uproject
  Plugins/
    SavageSuperStorm/
      SavageSuperStorm.uplugin
      Source/
      Shaders/
      Content/
      Resources/
```

Then open **Edit → Plugins**, confirm `SavageSuperStorm` is enabled, and turn on
**Show Plugin Content** in the Content Browser.

## Quick start

1. Add a **Directional Light**, **Sky Atmosphere**, and **Volumetric Cloud** to
   the level.
2. Place a **Volumetric Super Storm Actor**. One world supports one registered
   storm actor.
3. Select its **Material Binder** component. Set **Base Cloud Material** to
   `M_SSS`, set **Target Cloud Reference** if the level has more than one cloud,
   then press **Apply Volumetric Cloud Material**.
4. Tune **Radius**, **Anvil Coverage**, **Density**, and **HF Strength** under
   Shape Settings.
5. Use the Details panel's **Functions** section to open the vertical profile
   painter and the flow-map editor.

Full walkthrough: [Your First Storm](Docs/docs/getting-started/first-storm.md).

## What is included

| | |
|---|---|
| Modules | 3 — `SavageSuperStormShaders` (Runtime, `PostConfigInit`), `SavageSuperStormRuntime` (Runtime, `Default`), `SavageSuperStormEditor` (Editor, `PostEngineInit`) |
| C++ classes | 12 `UCLASS` — 1 actor, 4 components, 3 data assets, 1 world subsystem, 2 undo-state objects, 1 asset definition |
| Reflected types | 18 structs, 8 enums, 84 `UFUNCTION` declarations |
| Blueprint surface | 72 nodes (44 callable, 28 pure) and 7 assignable events |
| Shaders | 3 compute shaders (`.usf`), 11 shared headers (`.ush`) |
| Content | 50 assets — cloud material and material functions, noise and profile textures, default vertical profiles, a sample flow map, a sample preset, lightning VFX and audio |

### Content roots

| Path | Contents |
|---|---|
| `Materials/M_SSS` | Primary Volumetric Cloud material implementing the storm parameter contract. |
| `Materials/Functions` | Density, lighting, erosion, and utility material functions. |
| `Materials/Texture` | Perlin-Worley volume noise, curl noise, and profile textures. |
| `Materials/VFX`, `Materials/Sound` | Lightning Niagara system, lightning material, and thunder audio. |
| `VerticalProfile` | Default vertical profile assets and the painter preview material. |
| `FlowMaps` | Sample three-layer wind flow map. |
| `Preset` | Sample storm preset. |

## Features

**One actor drives everything.** `Volumetric Super Storm Actor` owns the shape,
motion, lightning, and formation state and binds the cloud material itself. No
manual material wiring on drop-in, and the binder restores the cloud's previous
material when the storm is removed.

**Painters, not just sliders.** A three-layer wind flow-map editor and a
vertical profile painter let you paint and preview storm structure directly,
with composite preview and undo.

**Vertical profiles with transitions.** Bottom, top, and anvil profiles carve
the storm silhouette in normalized cloud-layer space, and blend between saved
profile assets at runtime.

**Authored lightning.** A cue sequence drives material pulses, ground-strike
resolution, and Blueprint events for start, cue, strike, pulse, and finish.

**Formation and dissolution.** Timed create/dissolve animations with completion
events, plus immediate show/hide.

**Presets that travel.** A preset asset captures the complete authorable
configuration and applies onto any storm actor.

## Known limitations

- **One storm per world.** The world subsystem accepts a single registered storm
  actor and disables additional ones rather than sharing render targets.
- **One unambiguous cloud target.** Automatic cloud selection works only when the
  level contains exactly one Volumetric Cloud component; otherwise set
  `TargetCloudReference` explicitly.
- **The cloud layer owns altitude.** Storm heights are normalized to the
  Volumetric Cloud layer. Moving the actor vertically does not move the layer.
- **Render-target cost.** Two persistent RGBA16F shape targets use roughly
  64 MiB together at the maximum 2048 resolution, before flow-map and profile
  targets.

See [Limitations](Docs/docs/limitations.md) and
[Troubleshooting](Docs/docs/troubleshooting.md).

## Support

Report issues and ask setup questions at the support address listed on the
product page. Include your engine version, platform, RHI, and the
`LogSavageSuperStormRuntime` output from the session.

## License

Distributed under the Fab standard license terms that apply to your purchase.
Bundled third-party assets carry their own terms — see [CREDITS.md](CREDITS.md).
