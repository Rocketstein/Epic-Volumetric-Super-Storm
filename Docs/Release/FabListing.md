# Fab listing — Volumetric Super Storm 1.0.0

Source text for the Fab product page. Paste each block into the matching field
in the publisher portal. Nothing here ships inside the plugin.

Every field the portal exposes is filled below. Leave none of them blank.

---

## Product name

```
Volumetric Super Storm
```

## Seller

```
Team GoroGoro
```

## Short description

```
An art-directable volumetric supercell for Unreal Engine 5.8. Renders through the native Volumetric Cloud component and ships in-editor painters for storm shape, wind flow, and vertical structure.
```

## Long description

```
Volumetric Super Storm renders a single large, art-directable storm — mesocyclone body, anvil, spiral banding, underside detail — through Unreal Engine's native Volumetric Cloud component. It is a complete authoring system, not a material you wire up yourself: drop the actor in, point it at your cloud, and shape the storm with dedicated editor tools.

ONE ACTOR DRIVES EVERYTHING
The storm actor owns the storm's shape, motion, lightning, and formation state, and binds the cloud material for you. It stores the cloud's previous material and restores it when the storm is removed, so dropping a storm into an existing sky is non-destructive. Drag the native storm actor into the level and it works. A BP_VolumetricSuperStorm Blueprint subclass is also included as a reference for the Blueprint event API, if you want one — place the actor or the Blueprint, not both.

PAINT THE STORM, DON'T GUESS AT IT
Two dedicated editor tools replace numeric guesswork.

The vertical profile painter authors the storm's silhouette — bottom, top, and anvil boundaries — in normalized cloud-layer space, with live composite preview and undo. Save profiles as assets and blend between them at runtime.

The wind flow-map editor paints three independently authored flow layers — lower, middle, upper — that advect the storm's noise independently of its overall rotation. Paint by dragging the direction you want the flow to travel, and control each layer's height of influence with a layer-authority ribbon. A vertical brush component gives the map its 2.5D character.

MOTION WITH STRUCTURE
Up to six concentric rotation rings, each with its own angular speed and phase skew, blended across authorable boundaries. Radial and vertical masks limit where motion applies, and low-frequency, high-frequency, and curl fields rotate at independent multipliers so the storm shears rather than spinning as a rigid disc.

AUTHORED LIGHTNING
Cue sequences drive material pulses and resolve ground-strike locations, with repeat delay and strike probability. Start, cue, strike, pulse, and finish all fire as Blueprint events, so you can hang bolt VFX, thunder, camera shake, and gameplay off the storm's own timing. What ships is the timing, not the presentation: no bolt VFX and no audio are bundled. BP_VolumetricSuperStorm carries worked examples of the handlers so you can see exactly where your own effects attach.

FORMATION AND DISSOLUTION
Timed create and dissolve animations with completion events, plus immediate show and hide for cutting between shots.

PRESETS THAT TRAVEL
A preset asset captures the complete authorable configuration — shape, motion, lightning, formation, and profile and flow-map references — and applies onto any storm actor.

EDITOR-TIME PREVIEW
The storm renders and animates in the editor viewport. You are not iterating through PIE.

DOCUMENTED
A full documentation site covers requirements, installation, a first-storm walkthrough, four authoring guides, complete actor and Blueprint reference, limitations, and troubleshooting.

BEFORE YOU BUY
This plugin requires Windows 64-bit with DirectX 12 and Shader Model 6; DirectX 11 and SM5 are not supported. It renders one storm per world by design — the world subsystem accepts a single registered storm actor rather than sharing shape render targets. Vertical placement is owned by your Volumetric Cloud layer, not by the storm actor's transform. Lightning ships as timing and events, not as presentation: the cue sequence, ground-strike resolution, and cloud material pulse are all here, but no bolt VFX and no audio are bundled. All four behaviours are documented in detail; please read the Requirements and Limitations pages linked below if any of them is a constraint for your project.
```

## Technical Details

Paste into the **Technical Details** field.

```
Features:
 • One-actor storm system driving Unreal's native Volumetric Cloud component
 • Ready-to-place BP_VolumetricSuperStorm Blueprint, or the native C++ actor for custom subclassing
 • Vertical profile painter — bottom, top, and anvil silhouette authoring with composite preview, undo, and runtime blending between saved profiles
 • Three-layer wind flow-map editor with drag-direction and encoded-RGBA brushes, 32–2048 authoring resolution, and a layer-authority ribbon
 • Six-ring motion system with per-ring speed and skew, radial and vertical masks, and independent LF/HF/curl rotation
 • Authored lightning cue sequences with Landscape-aware ground strikes and Blueprint events
 • Timed formation and dissolution animations with completion events
 • Storm preset assets
 • Editor viewport preview without entering PIE

Code Modules:
 • VolumetricSuperStormRuntime (Runtime)
 • VolumetricSuperStormShaders (Runtime)
 • VolumetricSuperStormEditor (Editor)

Number of Blueprints: 1
Number of C++ Classes: 16
Network Replicated: No
Supported Development Platforms: Windows
Supported Target Build Platforms: Windows
Documentation: https://teamgorogoro.github.io/
Example Project: None — the plugin ships four storm presets, two wind flow maps, and nine vertical profiles that can be applied to any level, plus M_StormMinimalSetup for levels without an existing cloud material.
Important/Additional Notes: Requires Windows 64-bit with DirectX 12 and Shader Model 6. DirectX 11 and Shader Model 5 are not supported. Requires the Niagara plugin, which ships with Unreal Engine and is enabled by default. Requires a Volumetric Cloud component, Sky Atmosphere, and Directional Light in the level. Place either BP_VolumetricSuperStorm or the native storm actor — one storm actor per world; a second is rejected by the world subsystem. Storm heights are normalized to the Volumetric Cloud layer, so absolute altitude and thickness stay properties of that component. No bolt VFX and no audio assets are bundled; the lightning system exposes Blueprint events for driving your own bolts and thunder, and BP_VolumetricSuperStorm carries worked examples of the handlers.
```

### Counts, and what they were taken from

Counted against the working tree on 2026-08-06, after removing the five audio
assets, both `BP_StormLightningController` variants, the three
`CF_StormLightning*` curves, and the three presets and profiles named after a
team member, and after CL238 added `M_StormMinimalSetup` and
`VP_DocsProfileExample`.

| Field | Value | Source |
|---|---:|---|
| C++ classes | 16 | `UCLASS` declarations across `Source/**/*.h` |
| Blueprints | 1 | `BP_VolumetricSuperStorm` |
| Structs / enums | 23 / 10 | `USTRUCT` / `UENUM` |
| `UFUNCTION` declarations | 84 | `Source/**/*.h` |
| Blueprint nodes | 72 | 44 `BlueprintCallable` + 28 `BlueprintPure` |
| Blueprint events | 7 | `BlueprintAssignable` |
| Content assets | 41 | `.uasset` files under `Content/` |
| Shaders | 3 `.usf`, 11 `.ush` | `Shaders/` |

Platforms list Windows only because Windows is the only platform a packaged
build has been launched on. Nothing in the three `Build.cs` files restricts
platforms, so adding a platform later is a listing edit once it has been tested.

## Supported engine versions

```
5.8
```

## Tags

```
volumetric, clouds, storm, weather, supercell, hurricane, tornado, sky, lightning, vfx, environment, atmosphere
```

## Support contact

```
gorogoro9012@gmail.com
```

```
https://teamgorogoro.github.io/
```

The same address is set in `VolumetricSuperStorm.uplugin` (`SupportURL`) and in the
plugin `README.md`. All three must agree.

## Generative AI disclosure

```
No
```

No generative-AI content ships in the plugin.

## Preview media

Confirm current dimension and count requirements in the publisher portal, then
produce:

| Asset | Purpose | Notes |
|---|---|---|
| Featured image | Listing hero | Mature storm, dramatic lighting, no UI |
| Gallery — 5 to 8 images | Feature proof | At minimum: full storm exterior, underside, anvil detail, the flow-map editor, the vertical profile painter, the actor Details panel |
| Video | Motion proof | Storm rotation, a formation animation, a lightning sequence, and one editor-authoring pass. Motion is the whole point of this product; a still gallery undersells it. **The lightning shot must show the cloud material pulse, not a bolt** — no bolt VFX ships, and a captured bolt would misrepresent the product. |
| Thumbnail | Grid listing | Readable at small size — silhouette, not detail |

Captures already available in the docs tree
(`Plugins/VolumetricSuperStorm/Docs/assets/`):
`flow-map-editor-widget.png`, `profile-editor.png`, `profile-paint.png`,
`flow-map-working-example.mp4`, `flow-upward.mp4`,
`profile-transition-viewport.mp4`.
