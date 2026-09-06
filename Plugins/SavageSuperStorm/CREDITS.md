# Credits and third-party attribution

Savage Super Storm is developed by Team GoroGoro.

Everything in this plugin is either original work by Team GoroGoro or listed
below with its source and license.

> **This file is incomplete and must be finished before submission.** Rows marked
> `VERIFY` were reconstructed from asset filenames, not from a licence record.
> Fill each one from the original download page or purchase receipt, or remove
> the asset from the shipping build. Distributing an asset whose licence you
> cannot produce on request is the most common cause of a marketplace takedown.

## Audio

Five thunder recordings ship under `Content/Materials/Sound/`. Their filenames
follow the `uploader-title-id` pattern used by stock audio libraries, which
indicates they were downloaded rather than recorded in house.

| Asset | Apparent uploader | Source | Licence | Attribution required |
|---|---|---|---|---|
| `freesound_community-thunder-sound-45992` | freesound_community | `VERIFY` | `VERIFY` | `VERIFY` |
| `lordsonny-thunder-for-anime-161022` | lordsonny | `VERIFY` | `VERIFY` | `VERIFY` |
| `soundmarker33-thunder-clap-512544` | soundmarker33 | `VERIFY` | `VERIFY` | `VERIFY` |
| `tanweraman-big-thunder-rumble-321631` | tanweraman | `VERIFY` | `VERIFY` | `VERIFY` |
| `u_q2hb2391vb-thunder-clap-521194` | u_q2hb2391vb | `VERIFY` | `VERIFY` | `VERIFY` |

For each row, confirm all three of the following before shipping:

1. The licence permits **commercial use**.
2. The licence permits **redistribution inside a paid product**. Several stock
   libraries allow commercial *use* while prohibiting redistribution of the file
   itself as part of a product others can extract — which is what bundling into
   a plugin does. This is the clause that most often fails.
3. Any **attribution text** the licence requires is reproduced in this file.

If a row cannot clear all three, replace the asset with an original recording or
a licence you own, or cut audio from the release and document the lightning VFX
as silent.

## Textures

| Asset | Origin | Status |
|---|---|---|
| `VT_PerlinWorley_Balanced_128_RGBA16F` | Procedural Perlin-Worley volume noise | `VERIFY` — confirm generated in house |
| `VT_WorleyDetail_32_RGBA8` | Procedural Worley detail noise | `VERIFY` — confirm generated in house |
| `VT_Hillaire_PerlinWorley_Balanced_128_RGBA16F_Volume` | Named after published cloud-rendering work | `VERIFY` — see naming note below |
| `VT_Hillaire_WorleyDetail_32_RGBA8_Volume` | Named after published cloud-rendering work | `VERIFY` — see naming note below |
| `VT_Nubis_Hillaire_PerlinWorley_Balanced_128_RGBA16F_Volume` | Named after published cloud-rendering work | `VERIFY` — see naming note below |
| `VT_Nubis_Hillaire_WorleyDetail_32_RGBA8_Volume` | Named after published cloud-rendering work | `VERIFY` — see naming note below |
| `T_CurlNoise`, `T_CurlNoise_RGB_128_Full_RGBA8` | Procedural curl noise | `VERIFY` — confirm generated in house |
| `BottomProfile`, `T_Profile_FallBack` | Authored profile lookups | Original work |
| `ChatGPT_Image_2026_06_30_...` | Generative AI image | `VERIFY` — see AI note below |

### Naming note

"Nubis" is Guerrilla Games' cloud system and "Hillaire" is the author of the
cloud-rendering papers the technique derives from. Using a *technique* published
in a paper or conference talk is normal and needs no licence. Shipping assets
whose **names** carry another studio's system name or another author's surname
is a separate matter — it implies provenance the plugin does not have, and
duplicated variants (`VT_*`, `VT_Hillaire_*`, `VT_Nubis_Hillaire_*` are three
near-identical sets) suggest these are working copies rather than deliberate
shipping assets.

Recommended: keep one set, rename it descriptively
(`T_PerlinWorley_128_Volume`), and credit the technique in prose here rather
than in asset names.

### Generative AI note

`ChatGPT_Image_2026_06_30_...` is AI-generated, per its filename. Two things
follow:

- Fab requires products containing generative-AI content to declare it on the
  listing. Decide whether this asset ships; if it does, tick the AI disclosure.
- The filename contains non-ASCII characters that are already mojibake on disk.
  Rename it regardless of the disclosure decision — see `비상!!!!!!!!.md` at the
  project root.

If the asset is a leftover reference image rather than a shipping dependency,
delete it and this row.

## Engine and techniques

Savage Super Storm renders through Unreal Engine's native Volumetric Cloud
component. Unreal Engine is a trademark of Epic Games, Inc.

The density and erosion model builds on publicly documented volumetric cloud
rendering techniques, including Perlin-Worley noise composition and curl-noise
advection. These are techniques, used under no licence obligation; no code or
assets were taken from those sources.

## Documentation toolchain

The documentation site is built with [MkDocs](https://www.mkdocs.org/) (BSD
2-Clause) and the
[Material for MkDocs](https://squidfunk.github.io/mkdocs-material/) theme (MIT).
Neither ships inside the plugin; both are build-time tools for the docs site
only.
