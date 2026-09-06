# Vertical Profile

This page will walk you through the vertical profile editor, a feature that allows the users to author
the general shape of the storm.

## What is a Vertical Profile?

A vertical profile is a **lookup texture that returns a density multiplier for a
given height**. The material samples it while marching through the cloud and
multiplies the result into the density field, so profiles are what carve the
storm's silhouette from top to bottom.

There are three, and each owns one boundary:

| Profile | Owns | Shaped by |
|---|---|---|
| **Top** | The upper surface of the body — how high each column reaches. | Vertical Profile Curve, Top Fade |
| **Bottom** | The lower boundary — how the storm dissolves into its base. | Bottom Fade |
| **Anvil** | How far the anvil extends downward from its anchor. | Anvil Profile Curve |

All three work the same way: a threshold height is full density on one side and
empty on the other, with a fade band across the boundary. Top is solid below its
curve height and fades out as it approaches it. Anvil mirrors that, solid from
its anchor downward and fading out at the curve's depth. Bottom ramps up from
zero at the very base.

The Bottom profile is indexed differently from the other two: its horizontal axis
is the cloud-bottom *type* selector rather than a column position, and a higher
type widens the translucent transition so more of the wispy structure near the
base shows through.

!!! tip "A profile multiplies, it does not add"
    Wherever a profile reads zero the
    storm is empty, regardless of what shape, noise, or erosion would have put
    there. That is why a profile edit can appear to delete large parts of the
    storm rather than reshape them.

!!! warning "Heights are normalized, not altitudes"
    The vertical axis runs from the base
    to the top of the cloud column, `0` to `1`. A profile has no idea where the
    storm sits in the world — that belongs to the Volumetric Cloud layer, as
    [Limitations](../limitations.md) explains. The same profile in a taller layer
    produces a taller storm.

## Step 1. Locating the Vertical Profile Editor
<figure markdown="span">
  ![The storm actor's Details panel with the Functions section expanded; the Edit Storm Profile button is outlined](../assets/images/profile-details-panel.png){ width="480" }
</figure>

Locate the Actor details panel -> Functions.

Clicking the Edit Storm Profile Button should open a slate widget as shown in part 2.

## Step 2. Getting used to the Editor UI

<figure markdown="span">
  ![The Storm Profile Editor window: composite preview and profile parameters on the left, Top/Bottom/Anvil tabs above the profile canvas in the centre, and mode tabs with save actions on the right](../assets/images/profile-editor.png){ width="800" }
</figure>

Three column regions, outlined above.

### Composite preview — top left, red outline

- A live thumbnail of the profile as the material reads it. It updates as you
  edit parameters or paint, so use it to judge the overall silhouette while the
  large canvas shows one profile at a time in detail.

### Profile parameters — left, red outline

**Storm Profile Params** holds the values the profile is generated from:

| Parameter | Default | Range | Controls |
|---|---|---|---|
| **Bottom Fade** | 0.1 | 0 – 1 | Vertical width of the fade band at the base. |
| **Top Fade** | 0.1 | 0 – 1 | Blend strength of the top profile's edge fade. |
| **Vertical Profile Curve** | — | — | Per-column top height of the storm body. |
| **Anvil Profile Curve** | — | — | Per-column downward extent from the anvil anchor. |

- **External Curve** binds a curve asset in place of the inline curve, so a shape
  can be shared between storms or version-controlled on its own. **Create
  External Curve** promotes the current inline curve into an asset.
- **Persistent Profile Asset**, under *Loaded*, names the asset the live surfaces
  were seeded from.

### Profile canvas — centre, green outline

- The **Top**, **Bottom**, and **Anvil** tabs choose which profile the canvas
  shows.
- The canvas renders density across the profile: white is full density, black is
  empty.

### Modes and actions — right, cyan outline

- **Parameterize**, **Paint**, and **Transition** switch what you are doing. The
  first two are covered in step 3; Transition drives A → B profile blending.
- **Rebuild from params** regenerates the surfaces from the parameters on the
  left.
- **Checkpoint** snapshots the current parameters and painted surfaces as the
  revert baseline. **Revert** restores that snapshot.
- **Save**, **Save As…**, and **Load…** move the profile to and from a persistent
  asset. The label above them reports the current state — `Saved` in the
  screenshot.

!!! tip "Checkpoint is not Save"

    A checkpoint is an in-session baseline for **Revert**. It advances what Revert
    returns to, but it does not write to the asset — the editor still treats the
    profile as unsaved until you press **Save**.

## Step 3. Editing the Top Profile

The top profile sets the upper surface of the storm body: per column, how high
the body reaches.

### Parameterize

In **Parameterize** mode the surface is generated from the parameters on the
left, and regenerated whenever they change.

- **Vertical Profile Curve** is the main control — its value at each point across
  the radius is that column's top height.
- **Top Fade** softens the profile's edge.
- Press **Rebuild from params** to regenerate after editing.
- To reuse a shape elsewhere, bind an **External Curve**, or press **Create
  External Curve** to promote the inline curve into an asset.

### Paint

<figure markdown="span">
  ![The Storm Profile Editor in Paint mode: a brush stroke on the Top profile canvas with the round brush cursor visible, and the right panel showing brush texels, paint mode, paint value, and eraser controls](../assets/images/profile-paint.png){ width="800" }
</figure>

**Paint** mode freezes the current surfaces as a base and lets you brush directly
onto them, for detail a curve cannot express. Strokes apply to the profile
selected by the centre tab.

| Control | Does |
|---|---|
| **Brush texels** | Brush radius, in texels of the 128-wide profile. |
| **Paint mode** | **Blend** eases covered texels toward the paint value; **Overwrite** replaces them outright. |
| **Brush strength** | Blend weight per dab. Greys out under Overwrite, which ignores it. |
| **Paint value** | The density written — `1.0` full, `0.0` empty. |
| **Erase** / **Eraser strength** | Erasing targets zero density and ignores paint mode. |

!!! warning "Switching back to Parameterize discards your paint"

    Entering Parameterize rebuilds Top and Anvil from the parameters, throwing
    away everything painted since you left it. Entering Paint freezes the current
    surfaces as the new paint base.

    So work the parameters first, then paint. If you must adjust a curve after
    painting, press **Checkpoint** first — Revert restores params *and* the
    painted surface, so the checkpoint is your way back.

## Step 4. Editing the Bottom Profile

The bottom profile is **parametric only** — it has no painting stage.

- Select the **Bottom** tab to view it on the canvas.
- **Bottom Fade** is its control: the vertical width of the fade band at the base
  of the storm.
- It is regenerated from parameters whenever the profile rebuilds, including as
  part of a **Revert**, so it stays coherent with the current parameter set.

!!! tip "The brush writes to the Top and Anvil surfaces only"
    If a stroke on the Bottom
    tab appears to do nothing, that is why.

## Step 5. Editing the Anvil Profile

The anvil profile controls how far the anvil reaches **downward from its anchor**,
column by column.

It is edited exactly like the top profile — the same Parameterize and Paint
modes, the same brush, and the same caution about switching back to Parameterize.
Follow [step 3](#step-3-editing-the-top-profile) for the workflow, then note the
four differences below.

**It is measured downward.** `V = 0` is the anchor and `V = 1` is the cloud-layer
bottom, mirroring the top profile. The Anvil Profile Curve gives each column a
*depth* rather than a height, so a larger value reaches further down.

**It is paintable.** Unlike the bottom profile, the brush does write to the anvil
surface. Select the **Anvil** tab before painting.

!!! warning "The anchor lives elsewhere"

    **Anvil Strength** on the storm actor defaults to `0.0`, and zero is an exact
    identity — the anvil contributes nothing regardless of what you author here.
    Raise it under the actor's Shape settings, and check **Anvil Depth 01**,
    which limits the depth window the profile is allowed to shape. Both are in
    [Core Parameters](core-parameters.md).

## Step 6. Authoring Profile Transition

The **Transition** tab blends the storm from the profile you are editing into a
second, saved profile — for a storm that changes silhouette over time rather than
holding one shape.

<div class="grid" markdown>

<figure markdown="span">
  <video controls loop muted playsinline width="100%">
    <source src="../../assets/videos/profile-transition.mp4" type="video/mp4">
  </video>
  <figcaption>The transition running in the profile editor.</figcaption>
</figure>

<figure markdown="span">
  <video controls loop muted playsinline width="84%">
    <source src="../../assets/videos/profile-transition-viewport.mp4" type="video/mp4">
  </video>
  <figcaption>The same blend seen in the level viewport.</figcaption>
</figure>

</div>

### The two endpoints are not symmetric

- **Profile A** is the component's live authoring surface: whatever you built in
  steps 3 to 5. It is not an asset.
- **Profile B** is a saved **Storm Vertical Profile** asset, and stays immutable
  for the duration of the transition.

So a transition always runs "from what I am editing, to that asset on disk."
Clearing the target removes the transition.

### Settings

| Setting | Default | Controls |
|---|---|---|
| **Target Profile Asset** | none | The Profile B endpoint. |
| **Easing** | Linear | `Linear` or `SmoothStep`. |
| **Forward Duration** | 2.0 s | Time to blend A → B. |
| **Backward Duration** | 2.0 s | Time to blend B → A. |

Forward and backward durations are independent, so a storm can build slowly and
collapse fast. These values are authored in the Transition tab rather than the
details panel.

### Driving it from Blueprint

On the storm's vertical profile tool component, under **Storm | Profile |
Transition**:

| Node | Does |
|---|---|
| `Set Transition Target` | Sets the Profile B asset. Null clears it. |
| `Play Transition Forward` | Blends A → B over Forward Duration. |
| `Play Transition Backward` | Blends B → A over Backward Duration. |
| `Stop Transition` | Halts playback where it stands. |
| `Set Preview Transition Alpha` | Stops playback and sets B's weight directly. |
| `Get Transition Alpha` | Current weight of B. |
| `Is Transition Active` | Whether playback is running. |
| `Has Valid Transition Target` | Whether a usable B is set. |

`Set Preview Transition Alpha` is what the editor's own scrubbing uses. It is
equally valid in gameplay when you want the blend driven by your own curve rather
than the built-in timing.

!!! tip "How the blend is applied"
    Both endpoints are handed to the material as separate profile textures alongside
    the current weight, and the blend happens as the material samples them. Nothing
    is composed into an intermediate render target, so scrubbing the alpha is cheap
    and the A surface stays fully editable mid-transition.
