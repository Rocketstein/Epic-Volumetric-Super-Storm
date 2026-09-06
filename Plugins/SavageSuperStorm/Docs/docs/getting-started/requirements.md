# Requirements

## Engine

| Requirement | Value |
|---|---|
| Unreal Engine | 5.8 |
| Project type | C++ or Blueprint project |
| Rendering path | Native Volumetric Cloud |

## Level setup

The level requires a Volumetric Cloud component. A Sky Atmosphere and a
Directional Light are normally required for the expected cloud lighting.

## GPU support

The plugin uses compute shaders for shape, profile, and flow-map render targets.
The target RHI and platform must support the formats and compute functionality
used by Unreal's Volumetric Cloud renderer and RGBA16F UAV render targets.

## Module loading

| Module | Type | Loading phase |
|---|---|---|
| `SavageSuperStormShaders` | Runtime | `PostConfigInit` |
| `SavageSuperStormRuntime` | Runtime | `Default` |
| `SavageSuperStormEditor` | Editor | `PostEngineInit` |

The shader module loads first so `/SavageSuperStormShaders` is registered before
materials or compute shaders compile.
