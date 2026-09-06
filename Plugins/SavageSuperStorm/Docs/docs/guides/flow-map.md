# Flow Map

<figure markdown="span">
  <video controls autoplay loop muted playsinline width="100%">
    <source src="../../assets/videos/flow-upward.mp4" type="video/mp4">
  </video>
  <figcaption>Example: Storm with an upward-painted flow map noise advection.</figcaption>
</figure>

This page will walk you through the flow map editor, a feature that allows the users to author a flow map asset used to animate the storm independently of the
storm's overall rotation.

## Step 1. Opening the flow map editor

<figure markdown="span">
  ![Storm actor Details panel with the Functions section expanded, showing Edit Storm Profile, Edit Storm Flow Map, and Load Preset buttons](../assets/images/flow-map-details-panel.png){ width="480" }
</figure>

Locate the Actor details panel -> Functions.

Clicking the Edit Flow Map Button should open a slate widget as shown in part 2.

## Step 2. Getting used to the editor UI

<figure markdown="span">
  ![The Storm Flow Map Editor window: brush controls along the top, layer tabs and the paint canvas in the centre, the layer authority ribbon on the left, file actions along the bottom, and the Flow Map details panel on the right](../assets/images/flow-map-editor-widget.png){ width="800" }
</figure>

Everything happens in one window. The five outlined regions above:

### Brush input — top left, green outline

- **Drag Direction** takes the flow vector from the stroke itself: drag the way
  you want the noise to travel. **Encoded RGBA** instead paints an explicit
  direction encoded into RGB, with alpha carrying strength.
- **Paint / Erase** switches the brush between writing and clearing.
- **Radius**, **Strength**, and **Opacity** set the brush footprint and how
  forcefully each dab writes. Strength defaults to `0.75` and opacity to `0.35`
  — the low opacity is deliberate, since flow reads better built up over several
  passes than stamped in one.
- **Vertical** sets the vertical component of the painted direction (z-direction). Zero keeps the flow in-plane; this is the extra half-dimension that makes the map 2.5D.

### Layer tabs — top centre, red outline

- The flow map has three layers — **Lower**, **Middle**, **Upper** — painted
  independently. The tab chooses which one the brush writes to.
- Only one layer is edited at a time, and the status bar always names the active
  one. Check there if you are unsure what you are about to paint on.

### Layer authority — left, blue outline

- The ribbon shows how strongly each layer governs the result at a given height.
  Its handles at `0.15`, `0.50`, and `0.85` are the *same values* as Lower,
  Middle, and Upper Layer Height in the details panel — drag a handle or type a
  number, they are one setting.
- The three weights always sum to one. Below the lowest knot and above the
  highest, the nearest layer holds constant; there is no fade to zero at either
  end.

### Flow Map details — right, magenta outline

- **Persistent Flow Map** — the asset this editor reads from and writes to.
- **Flow Map Enabled** — master switch for the flow map's contribution.
- **Resolution** — authoring resolution of each of the three square layers.
  Defaults to `256`, accepts `32` to `2048`. We strongly recommend keeping the resolution a power of 2.
- **Lower / Middle / Upper Layer Height** — the normalized cloud heights at
  which each layer is exact. Mirrors the ribbon handles.
- **UVW Strength** — maximum displacement for a fully opaque texel, in the
  normalized storm-flow domain.
- **Cycle Duration** — length of one local-flow cycle used in dual-phase noise blend.

### File actions — bottom left, cyan outline

- **Clear Layer** empties only the active layer, not all three.
- **Revert** discards changes back to the last saved state.
- **Save** and **Save As…** write to a persistent flow map asset; **Load…**
  brings one back in.
- Saving matters even though the storm already updates live — see step 4.

## Step 3. Painting the 2.5D flow map

The centre panel is the paint canvas, showing the active layer.

Hovering the cursor over the canvas previews the brush's radius of influence.
Left-drag inside that preview to paint. What gets written is either the direction
of your stroke or a fixed encoded RGBA value, depending on the brush mode set in
step 2.

### What the colour encodes

Every texel stores a direction plus a strength:

| Channel | Carries | Stored as |
|---|---|---|
| **R** | U component of the flow direction | `direction * 0.5 + 0.5` |
| **G** | V component | `direction * 0.5 + 0.5` |
| **B** | W component — the vertical half-dimension | `direction * 0.5 + 0.5` |
| **A** | Strength of the flow, `0` to `1` | stored directly |

- **RGB sets direction only.** On decode the RGB vector is re-normalized, so how
  far a channel sits from `0.5` has no effect on speed. Magnitude comes entirely
  from alpha.
- **A neutral texel is `(0.5, 0.5, 0.5, 0)`** — zero direction, zero strength.
  This is what an empty layer holds, and what **Clear Layer** writes back.

The decoded vector is scaled per axis by **UVW Strength**, and gated by **Flow Map
Enabled**.

## Step 4. Save and Load

### What you paint is transient

The three layers live in render targets on the storm's flow map component. They
are transient — built at runtime, never serialized. A flow map **asset** is a
saved snapshot of them, and seeding runs one way: asset into surfaces, when the
component initialises or when you revert.

That is why painting appears immediately, including in PIE, and equally why
nothing you paint survives closing Unreal unless you have written it into an
asset.

### Read the status bar

The bar under the canvas answers "what am I editing, and is it safe to close?":

`Unsaved transient flow map | Lower | 256 x 256 | unsaved changes`

- **First field** — the bound asset's name, or *Unsaved transient flow map* when
  no asset is bound. In that state there is nothing on disk to save back to, and
  **Save As…** is the only way to keep the work.
- **Second field** — the active layer.
- **Third field** — the working resolution.
- **`unsaved changes`** — shown whenever the painted surfaces differ from the
  asset. Its absence is your green light.

### The four actions

- **Save** writes all three layers into the bound asset as embedded textures and
  copies the current parameters in alongside them.
- **Save As…** creates a new asset from the current surfaces. This is what you
  need the first time, while the status bar still reads *Unsaved transient flow
  map*.
- **Load…** binds a different asset and re-seeds the surfaces from it. It asks
  for confirmation first, because it discards unsaved paint.
- **Revert** re-seeds from the bound asset, discarding everything painted since
  the last save. It confirms first as well.

!!! warning "Save marks the asset dirty — it does not write to disk"

    **Save** writes into the asset object and marks its package dirty. The
    package still has to be saved in the editor — `Ctrl+S`, or
    **File → Save All** — before anything reaches disk. Closing Unreal without
    that step loses the save.

!!! tip "Changing Resolution requires a Revert before you can save"

    Resolution determines how the working layers are built, so saving is refused
    while the asset's resolution and the working resolution disagree:

    > The asset resolution changed. Revert to rebuild the working layers at the
    > new resolution before saving.

    Revert first to rebuild the layers at the new resolution, then repaint. When
    you are editing an asset directly rather than through a storm, the status bar
    also reports `Resolution changed | Revert to rebuild working layers`.

### Opening an asset directly

Double-clicking a flow map asset in the Content Browser opens the same editor
with no storm attached. The Save button reads **Bake and Save** in that mode,
and the asset-bound controls are hidden — you are editing the asset's own
surfaces rather than a live storm's.

## Working Example

<figure markdown="span">
  <video controls autoplay loop muted playsinline width="100%">
    <source src="../../assets/videos/flow-map-working-example.mp4" type="video/mp4">
  </video>
</figure>
