# Installation

## Install the plugin

1. Close Unreal Editor.
2. Copy the `SavageSuperStorm` folder into the project's `Plugins` directory.
3. Regenerate project files when the project uses C++.
4. Build the project or accept Unreal's module rebuild prompt.

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

An engine-wide installation under `Engine/Plugins/Marketplace/` also works, but
a project-local installation is easier to version with the project.

## Verify the modules

Open **Edit → Plugins**, search for `SavageSuperStorm`, and confirm that it is
enabled. Then enable **Show Plugin Content** in the Content Browser and confirm
that the `SavageSuperStorm` content root is available.

The plugin loads three modules:

| Module | Role |
|---|---|
| `SavageSuperStormShaders` | Registers shader source and compute passes. |
| `SavageSuperStormRuntime` | Provides the actor, components, assets, and world subsystem. |
| `SavageSuperStormEditor` | Provides painters, asset editors, Details customization, and viewport tools. |

## Shipped content

| Asset area | Purpose |
|---|---|
| `Materials/M_SSS` | Primary Volumetric Cloud material implementing the storm contract. |
| `Materials/Functions` | Storm density, lighting, erosion, and utility functions. |
| `Materials/Texture` | Profile, curl, and volume-noise textures. |
| `VerticalProfile` | Default profile assets and profile preview materials. |
| `FlowMaps` | Saved three-layer flow-map assets. |
| `Preset` | Reusable storm presets. |

Continue to [Your First Storm](first-storm.md).
