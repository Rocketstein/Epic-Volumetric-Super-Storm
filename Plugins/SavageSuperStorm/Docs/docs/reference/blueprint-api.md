# Blueprint API

## Storm Actor

### Preset and render

| Node | Purpose |
|---|---|
| `Apply Preset` | Copies a Storm Preset into the actor and performs a full rebuild. |
| `Prepare Storm Resources` | Registers the actor as the world's storm and initializes resources. |
| `Rebuild Render Data` | Rebuilds static and frame render state. |
| `Get Storm Render Data` | Returns a Blueprint-safe copy of the current render data. |

### Formation

| Node | Purpose |
|---|---|
| `Create Storm` | Starts formation. |
| `Dissolve Storm` | Starts dissolution. |
| `Show Mature Storm` | Immediately switches to the mature state. |
| `Hide Storm Immediately` | Immediately switches to the hidden state. |
| `Set/Get Formation Animation Duration` | Changes or reads formation duration. |
| `Set/Get Dissolution Animation Duration` | Changes or reads dissolution duration. |

### Motion

| Node | Purpose |
|---|---|
| `Set Storm Motion Enabled` | Enables or disables motion evaluation. |
| `Pause Storm Motion` / `Resume Storm Motion` | Pauses or resumes motion time. |
| `Reset Storm Motion` | Resets motion time and ring phases. |
| `Set Storm Motion Time` | Sets deterministic motion time. |

### Profile transition

| Node | Purpose |
|---|---|
| `Set Profile Transition Target` | Selects the destination vertical profile asset. |
| `Play Profile Transition Forward/Backward` | Animates the transition alpha. |
| `Stop Profile Transition` | Stops the active transition. |
| `Set/Get Profile Transition Alpha` | Writes or reads transition progress. |
| `Is Profile Transition Active` | Reports whether the transition is playing. |
| `Has Valid Profile Transition Target` | Reports whether a usable target is assigned. |

### Lightning

| Node | Purpose |
|---|---|
| `Play Lightning Sequence` | Starts the configured sequence. |
| `Stop Lightning` | Stops the sequence and clears the material state. |
| `Is Lightning Sequence Playing` | Reports the current play state. |

## Storm Material Binder

| Node | Purpose |
|---|---|
| `Resolve Target Cloud` | Resolves the explicit target or the only cloud in the world. |
| `Push Storm Render Data` | Uploads complete storm material state. |
| `Apply Volumetric Cloud Material` | Creates or recreates the dynamic instance and uploads state. |
| `Update Volumetric Cloud Material` | Refreshes the existing dynamic instance. |
| `Get Dynamic Cloud Material` | Returns the active binder-owned instance. |
| `Apply Lightning Material State` | Uploads a complete lightning material payload. |
| `Set Lightning Pulse` | Updates the lightning pulse scalar. |
| `Clear Lightning Material State` | Clears all lightning material values. |
