# Fab listing copy — Savage Super Storm 1.0.0

Internal document. This is the source text for the Fab product page; it does not
ship inside the plugin. Paste each block into the matching field in the Fab
publisher portal.

Field limits and preview-media dimensions change; check the current values in the
publisher portal before pasting, and trim rather than let the form truncate.

---

## Product name

```
Savage Super Storm
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
Savage Super Storm renders a single large, art-directable storm — mesocyclone body, anvil, spiral banding, underside detail — through Unreal Engine's native Volumetric Cloud component. It is a complete authoring system, not a material you wire up yourself: drop the actor in, point it at your cloud, and shape the storm with dedicated editor tools.

ONE ACTOR DRIVES EVERYTHING
Volumetric Super Storm Actor owns the storm's shape, motion, lightning, and formation state, and binds the cloud material for you. It stores the cloud's previous material and restores it when the storm is removed, so dropping a storm into an existing sky is non-destructive.

PAINT THE STORM, DON'T GUESS AT IT
Two dedicated editor tools replace numeric guesswork.

The vertical profile painter authors the storm's silhouette — bottom, top, and anvil boundaries — in normalized cloud-layer space, with live composite preview and undo. Save profiles as assets and blend between them at runtime.

The wind flow-map editor paints three independently authored flow layers — lower, middle, upper — that advect the storm's noise independently of its overall rotation. Paint by dragging the direction you want the flow to travel, and control each layer's height of influence with a layer-authority ribbon. A vertical brush component gives the map its 2.5D character.

MOTION WITH STRUCTURE
Up to six concentric rotation rings, each with its own angular speed and phase skew, blended across authorable boundaries. Radial and vertical masks limit where motion applies, and low-frequency, high-frequency, and curl fields rotate at independent multipliers so the storm shears rather than spinning as a rigid disc.

AUTHORED LIGHTNING
Cue sequences drive material pulses and resolve ground-strike locations, with repeat delay and strike probability. Start, cue, strike, pulse, and finish all fire as Blueprint events, so you can hang thunder, camera shake, and gameplay off the storm's own timing. Ships with a Niagara lightning system, lightning material, and thunder audio.

FORMATION AND DISSOLUTION
Timed create and dissolve animations with completion events, plus immediate show and hide for cutting between shots.

PRESETS THAT TRAVEL
A preset asset captures the complete authorable configuration — shape, motion, lightning, formation, and profile and flow-map references — and applies onto any storm actor.

EDITOR-TIME PREVIEW
The storm renders and animates in the editor viewport. You are not iterating through PIE.

DOCUMENTED
A full documentation site covers installation, a first-storm walkthrough, four authoring guides, complete actor and Blueprint reference, limitations, and troubleshooting.

BEFORE YOU BUY
This plugin renders one storm per world by design — the world subsystem accepts a single registered storm actor rather than sharing shape render targets. Vertical placement is owned by your Volumetric Cloud layer, not by the storm actor's transform. Both behaviours are documented in detail; please read the Limitations page linked below if either is a constraint for your project.
```

## Technical details

Paste into the **Technical Details** field. Every number below was counted from
the 1.0.0 source tree — recount if the tree changes before submission.

```
Features:
 • One-actor storm system driving Unreal's native Volumetric Cloud component
 • Vertical profile painter — bottom, top, and anvil silhouette authoring with composite preview, undo, and runtime blending between saved profiles
 • Three-layer wind flow-map editor with drag-direction and encoded-RGBA brushes, 32–2048 authoring resolution, and a layer-authority ribbon
 • Six-ring motion system with per-ring speed and skew, radial and vertical masks, and independent LF/HF/curl rotation
 • Authored lightning cue sequences with Landscape-aware ground strikes and Blueprint events
 • Timed formation and dissolution animations with completion events
 • Storm preset assets
 • Editor viewport preview without entering PIE

Code Modules:
 • SavageSuperStormRuntime (Runtime)
 • SavageSuperStormShaders (Runtime)
 • SavageSuperStormEditor (Editor)

Number of Blueprints: 4
Number of C++ Classes: 12
Network Replicated: No
Supported Development Platforms: Windows
Supported Target Build Platforms: Windows
Documentation: https://team-gorogoro.github.io/VolumetricSuperstorm_Docs/
Important/Additional Notes: Requires the Niagara plugin, which ships with Unreal Engine and is enabled by default. Requires a Volumetric Cloud component, Sky Atmosphere, and Directional Light in the level. Renders one storm per world by design. Storm heights are normalized to the Volumetric Cloud layer, so absolute altitude and thickness stay properties of that component.
```

### Where the numbers come from

| Field | Value | Counted from |
|---|---:|---|
| C++ classes | 12 | `UCLASS` declarations across `Source/**/*.h` |
| Blueprints | 4 | `BP_VolumetricSuperStorm`, `BP_StormLightningController` ×3 versions |
| Structs / enums | 18 / 8 | `USTRUCT` / `UENUM` |
| `UFUNCTION` declarations | 84 | `Source/**/*.h` |
| Blueprint nodes | 72 | 44 `BlueprintCallable` + 28 `BlueprintPure` |
| Blueprint events | 7 | `BlueprintAssignable` |
| Content assets | 50 | `.uasset` files under `Content/` |
| Shaders | 3 `.usf`, 11 `.ush` | `Shaders/` |

> **Blueprint count is provisional.** `BP_StormLightningController`,
> `_V2`, and `_V4` are three versions of one controller. Ship one, and the count
> becomes 2. Resolve this before pasting — see `비상!!!!!!!!.md` at the project
> root.

> **Platform claim is deliberately narrow.** Windows is the only platform the
> plugin has been built and run on. Nothing in the three `Build.cs` files
> restricts platforms, so Mac, Linux, and console are *plausible* — but do not
> list a platform you have not launched a packaged build on. Adding platforms
> later is a listing edit; a refund wave over an untested claim is not.

## Supported engine versions

```
5.8
```

## Tags

```
volumetric, clouds, storm, weather, supercell, hurricane, tornado, sky, lightning, vfx, environment, atmosphere
```

## Preview media

Confirm current dimension and count requirements in the publisher portal, then
produce:

| Asset | Purpose | Notes |
|---|---|---|
| Featured image | Listing hero | Mature storm, dramatic lighting, no UI |
| Gallery — 5 to 8 images | Feature proof | At minimum: full storm exterior, underside, anvil detail, the flow-map editor, the vertical profile painter, the actor Details panel |
| Video | Motion proof | Storm rotation, a formation animation, a lightning sequence, and one editor-authoring pass. Motion is the whole point of this product; a still gallery undersells it. |
| Thumbnail | Grid listing | Readable at small size — silhouette, not detail |

Existing captures reusable from the docs site (`Docs/docs/assets/`):
`flow-map-editor-widget.png`, `profile-editor.png`, `profile-paint.png`,
`flow-map-working-example.mp4`, `flow-upward.mp4`,
`profile-transition-viewport.mp4`.

## Support contact

```
VERIFY — no support address is set anywhere in the project yet.
```

Fab requires a reachable support contact, and `SavageSuperStorm.uplugin` has an
empty `SupportURL`. Decide the address, then set it in three places: this
listing, the `.uplugin`, and `README.md`.
