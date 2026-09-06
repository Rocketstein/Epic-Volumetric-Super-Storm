# Core Parameters

The actor exposes only controls consumed by the active render, motion, formation,
and lightning paths. Values are sanitized before they enter render data.

## Shape and render target

`Storm | Settings → Shape Settings`

| Parameter | Default | Range | Purpose |
|---|---:|---:|---|
| Shape Render Target Resolution | 512 | 512–2048 | Resolution of both RGBA16F shape render targets. |
| Extent | 800000, 800000, 2200 | ≥ 1 per axis | Storm bounds used by spatial normalization. X and Y are never smaller than Radius. |
| Radius | 400000 | ≥ 1 | Horizontal body radius in Unreal units. |
| Envelope Radius | 100 | ≥ 0.01 | Width of the outer shape envelope. |
| Envelope Falloff | 10 | ≥ 0 | Falloff across the outer envelope. |
| Clockwise | false | — | Rotation direction used by motion and density evaluation. |
| Storm Base Color | 0.182, 0.189, 0.193 | — | Base cloud color uploaded to the material. |
| Wind Direction | 1.0, 0.15, 0.05 | — | XY direction used by the shape compute pass. |

A 2048 shape resolution allocates roughly 64 MiB for the two persistent
RGBA16F targets. Use the lowest resolution that preserves the required radial
detail.

## Radial shape curves

| Curve | Consumed channels | Purpose |
|---|---|---|
| Coverage Strength | R, A | Body and anvil coverage across normalized radius. |
| Type Strength | G, B | Bottom and top type strengths written to the shape texture. |
| Min/Max Layer Height | R, B | Minimum and maximum normalized cloud-layer height. |

The world subsystem samples each curve into a 256-entry lookup table during a
full update. Unused color channels do not participate in the bake hash.

## Anvil

| Parameter | Default | Range | Purpose |
|---|---:|---:|---|
| Anvil Strength | 0.0 | 0–1 | Master anvil contribution. Zero removes the contribution. |
| Anvil Coverage | 0.35 | 0–1 | Extends the outer radius to `Radius × (1 + Coverage)`. |
| Anvil Depth 01 | 1.0 | 0–1 | Normalized depth below the upper layer anchor. |
| Anvil Height Twist Degrees | 20 | −30–30 | Maximum upper-anvil rotation. |
| Anvil Height Twist Start | 0.35 | 0–0.9 | Local anvil height where twist begins. |

The upper anchor is the top of normalized cloud-layer space. There is no
separate start-height property.

## Density and lighting

| Parameter | Default | Range | Purpose |
|---|---:|---:|---|
| Density | 0.05 | 0–1 | Overall material density multiplier. |
| Density Gamma | 1.0 | 0.001–4 | Coarse-density shaping exponent. |
| HF Strength | 1.0 | 0–1 | High-frequency erosion contribution. |
| Underside Visibility | 0.4 | 0–1 | Visibility contribution for the storm underside. |
| Brim Emissive Color | white | — | Emissive tint for the storm brim. |

## Motion

`Storm | Settings → Motion Settings`

| Parameter | Default | Range | Purpose |
|---|---:|---:|---|
| Enabled | true | — | Enables shader motion. |
| Preview In Editor | true | — | Advances motion in editor viewport preview. |
| Motion Strength | 1.0 | ≥ 0 | Master motion amount. |
| Time Scale | 1.0 | ≥ 0 | Motion-time multiplier. |
| Ring Count | 6 | 1–6 | Number of concentric motion rings. |
| Ring End Radii 01 | 0.10, 0.22, 0.36, 0.54, 0.74 | 0.01–0.99 | Ordered boundaries between rings. |
| Ring Angular Speed Degrees | 3.0, 2.4, 1.8, 1.25, 0.75, 0.35 | — | Angular speed of each ring. |
| Ring Skew Degrees | 140, 96, 60, 34, 16, 0 | — | Per-ring phase skew. |
| Boundary Overlap 01 | 0.04 | 0–0.2 | Width of the ring transition region. |
| Transition Mode | Result Blend | — | Ring-boundary blending strategy. |
| Motion Radius Scale | 1.0 | 0–2 | Motion mask radius relative to the complete anvil radius. |
| Radial Feather 01 | 0.08 | 0–0.5 | Radial motion-mask feather. |
| Height Min 01 | 0.0 | 0–1 | Lower motion height. |
| Height Max 01 | 0.62 | 0–1 | Upper motion height. |
| Height Feather 01 | 0.08 | 0.001–0.5 | Vertical motion-mask feather. |
| LF Rotation Multiplier | 1.0 | — | Low-frequency rotation multiplier. |
| HF Rotation Multiplier | 1.08 | — | High-frequency rotation multiplier. |
| Curl Rotation Multiplier | 1.12 | — | Curl-field rotation multiplier. |
| Radial Shear Gain | 0.6 | 0–2 | Additional radial phase shear. |

`Visualize Ring Influence Ranges` is an editor visualization control and does
not change the rendered density.

## Formation

| Parameter | Default | Range | Purpose |
|---|---:|---:|---|
| Create Animation Duration | 13 s | ≥ 0.1 s | Formation duration. |
| Dissolve Animation Duration | 10 s | ≥ 0.1 s | Dissolution duration. |

## Lightning

The actor exposes the complete lightning sequence, repeat delay, and ground
strike probability. Material pulse values are owned by the lightning component
and are not duplicated in the shape settings.
