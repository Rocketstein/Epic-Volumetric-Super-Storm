# Volumetric Super Storm

<p align="center">
  <strong>A large-scale, art-directable storm system for Unreal Engine 5.8.</strong>
</p>

<p align="center">
  <a href="https://www.fab.com/listings/1c2d0fea-7278-49f0-9559-3efef3076ac0"><img alt="Available on Fab" src="https://img.shields.io/badge/Fab-View%20Listing-000000?style=flat-square"></a>
  <img alt="Unreal Engine 5.8" src="https://img.shields.io/badge/Unreal%20Engine-5.8-0E1128?style=flat-square&logo=unrealengine">
  <img alt="DirectX 12" src="https://img.shields.io/badge/DirectX-12-107C10?style=flat-square">
  <img alt="Shader Model 6" src="https://img.shields.io/badge/Shader%20Model-6-5C2D91?style=flat-square">
</p>

## Overview

**Volumetric Super Storm** is a real-time storm-authoring system built on Unreal Engine's native **Volumetric Cloud** component. It lets artists sculpt a storm's silhouette and shape its motion directly in the editor, making it easier to create dramatic and evolving stormscapes that fit the needs of a scene.

The system combines actor-level controls with dedicated **Vertical Profile** and **Flow Map** editors. Storm setups can be saved as reusable assets or complete presets, allowing a finished look to be transferred and iterated on without rebuilding it from scratch.

## Key Features

- Large-scale volumetric storms rendered through Unreal Engine's native Volumetric Cloud system
- Real-time, art-directable control over storm shape and motion
- A single **Volumetric Super Storm Actor** for scene-level authoring
- A **Vertical Profile editor** for shaping the storm's vertical structure
- A **Flow Map editor** for painting directional motion detail
- Reusable profile, flow-map, and storm preset assets
- Integration options for both the included starter material and existing cloud materials
- Editor-first iteration without requiring a separate external authoring workflow

## Requirements

| Requirement | Version or setting |
|---|---|
| Unreal Engine | 5.8 |
| Rendering API | DirectX 12 |
| Shader Model | 6 |
| Required plugin | Niagara |

Your level should also contain the standard Unreal Engine sky components used by the system:

- **Directional Light**
- **Sky Atmosphere**
- **Volumetric Cloud**

## Getting Started

1. Add a **Directional Light**, **Sky Atmosphere**, and **Volumetric Cloud** component to the level.
2. Place one **Volumetric Super Storm Actor** in the scene.
3. Choose the included `M_StormMinimalSetup` material, or integrate `MF_Storm_Global` into an existing cloud material.
4. Adjust the actor's shape and motion settings to establish the overall storm.
5. Open the **Vertical Profile** and **Flow Map** editors to paint finer structural and directional detail.
6. Save the result as reusable assets or as a complete **Storm Preset**.

## Documentation

- [Product page on Fab](https://www.fab.com/listings/1c2d0fea-7278-49f0-9559-3efef3076ac0)
- [Online documentation](https://teamgorogoro.github.io/)
- [Plugin documentation sources](Plugins/SavageSuperStorm/Docs/README.md)
- [Changelog](Plugins/SavageSuperStorm/CHANGELOG.md)
- [Third-party credits](Plugins/SavageSuperStorm/CREDITS.md)

## Project Structure

```text
Config/                         Unreal Engine project configuration
Plugins/SavageSuperStorm/       Runtime, editor, shader, and documentation modules
Source/                         Host Unreal Engine project module
Tools/                          Supporting asset-generation tools and tests
VolumetricSuperStorm.uproject   Unreal Engine project file
```

## License

Use of the product is governed by the license terms provided through its [Fab listing](https://www.fab.com/listings/1c2d0fea-7278-49f0-9559-3efef3076ac0). Third-party components and assets may carry separate terms; see [CREDITS.md](Plugins/SavageSuperStorm/CREDITS.md).

