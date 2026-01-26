# ProtoReady: Footstep System

> **[UE 5.5+]** **[C++]** **[Production-Ready]**

Ready-to-use footstep system based on Physical Surfaces and a DataAsset.
No Blueprint code required to get started.

---

## Setup

1. Enable the plugin: `Edit > Plugins > ProtoReady Footstep`.
2. Define your Physical Surfaces: `Edit > Project Settings > Engine > Physics > Physical Surface`.
3. Add the `PRFootstepComponent` to your Character (or any Actor).
4. Create a `PRFootstepData` DataAsset and assign it to the component.

> Example surfaces: Concrete, Dirt, Grass, Water, Wood.
> In editor, if only one `PRFootstepData` exists, it is auto-assigned when the component is created.

---

## How it works

### Footstep triggering
- **AnimNotify (animation-driven)**: add the **PR Footstep** notify to your animations. The notify exposes `FootSocketName` and is available as soon as the plugin is installed.
- **Distance (auto)**: triggers a step every `DistanceInterval` cm traveled. In Distance mode, Tick is enabled automatically.

### Landing
- `bAutoTriggerLand` auto-binds `ACharacter::LandedDelegate`.
- `LandingSound` is used if set; otherwise the system plays a standard footstep with the landing flag.

### Traces & surfaces
- Surface detection is done by trace on `ECC_Visibility` (Line / Sphere / Multi).
- Surfaces come from the mesh **Physical Material**. If missing, `SurfaceType_Default` is used.
- `DefaultSound` is the **fallback** sound used when the surface is not mapped or when no Physical Material is found.

---

## Parameters (tooltips visible in editor)

### `PRFootstepComponent`
- `FootstepData` — Tooltip: Main configuration asset. Defines surfaces, sounds, trigger modes, and trace settings.
- `bDebugTraces` — Tooltip: If true, draws debug lines for traces. Green = Hit (Surface found) / Red = Miss (Air/Gap).

### `PRFootstepData` — General
- `TriggerMode` — Tooltip: Determines how footsteps are triggered. AnimNotify = manual trigger (PRFootstep notify). Distance = automatic trigger every stride interval (cm).
  Values: `AnimNotify`, `Distance`.

### Surfaces
- `SurfaceSounds` — Tooltip: Map Physical Surface → Footstep Sound. Keys must match Surface Types defined in Project Settings > Engine > Physics.
- `DefaultSound` — Tooltip: Fallback sound if surface is not mapped or Physical Material is None. Ideally should never be empty.

### Landing Sound
- `LandingSound` — Tooltip: Sound to play on heavy landing (Jump/Fall). If not set, standard footstep sound is used (with bIsHeavyLand flag).
- `bAutoTriggerLand` — Tooltip: If true, binds to Character::OnLanded to trigger heavy landing sound. Requires Owner to be ACharacter.

### Trace
- `TraceLength` — Tooltip: Trace length (cm) below foot socket/bone. Increase if footsteps miss on steep slopes.
- `TraceType` — Tooltip: Type of trace to use for surface detection (Line/Sphere/Multi).
  Values: `Line` (fast, precise), `Sphere` (catch edges, recommended), `Multi` (heavier, more accurate on rough terrain).
- `FootSockets` — Tooltip: List of sockets to trace from. Cycles sequentially in Distance Mode. If empty, system falls back to Root/Capsule trace.
- `FootBones` — Tooltip: Fallback bone names if Sockets not found. Used as alternative to Sockets.
- `bUseFootSockets` — Tooltip: Toggle between socket-based and capsule-based tracing. True = uses FootSockets/FootBones. False = traces from Actor center (Capsule) with Z offset.
- `CapsuleZOffset` — Tooltip: Z offset (cm) from Actor origin when NOT using foot sockets. Negative values move trace start down toward foot level (e.g. -50).
- `TraceShapeSize` — Tooltip: Radius (Sphere) of the trace shape. Larger values catch edges better but might hit walls. Default: 10.0 cm.

### Audio
- `VolumeRange` — Tooltip: Random Volume range [Min, Max].
- `PitchRange` — Tooltip: Random Pitch range [Min, Max].
- `AttenuationSettings` — Tooltip: Attenuation settings for 3D spatialization. If empty, sound plays as 2D UI sound (heard everywhere).
- `EffectsChain` — Tooltip: DSP Effects Chain (Reverb, EQ). Using this spawns an AudioComponent (heavier than static fire-and-forget).

### Distance Mode
- `DistanceInterval` — Tooltip: Distance traveled (cm) before triggering a step (Distance Mode). Formula: Stride = MaxWalkSpeed / StepsPerSec.

### LOD
- `MaxLODDistance` — Tooltip: Max distance (cm) from camera to allow processing. System stops ticking/tracing beyond this range to save performance. Set to 0 to disable LOD.

> All distances are in **centimeters** (Unreal units).

---

## AnimNotify usage (summary)

Add **PR Footstep** to the animation and set `FootSocketName`.
The system uses sockets if `bUseFootSockets` is enabled; otherwise it falls back to capsule tracing.

---

## Troubleshooting (tech)

- **No sound**
  - `DefaultSound` not assigned (fallback required if surface not mapped or PhysMat missing).
  - LOD: `MaxLODDistance` too small (cm).
- **Trace misses the ground**
  - `TraceType` set to `Line` on uneven terrain → use `Sphere`.
  - `TraceLength` too short.
  - `bUseFootSockets` enabled but sockets/bones missing → capsule fallback is mispositioned → adjust `CapsuleZOffset`.
- **No surface variation**
  - No **Physical Material** on the mesh/material → `SurfaceType_Default` → `DefaultSound` (fallback).
- **Trace hits nothing**
  - Ground collision doesn’t block `Visibility`.
  - Enable `bDebugTraces` to visualize.

---

## Included content

- **SoundCues (5, included)**: `SC_Concrete`, `SC_Dirt`, `SC_Grass`, `SC_Water`, `SC_Wood`.
- **WAV Footsteps (23, included)**: Concrete ×3, Dirt ×5, Grass ×5, Water ×5, Wood ×5.
- **Landing (1, included)**: `S_PR_JumpLanding`.
- **MetaSound (demo)**: `MS_Footstep_Metal` (metal synthesis).
- **DataAsset**: `DA_Footstep_Config_A`.
- **Demo map**: `Level_PR_Footstep_Showcase`.

---

## Support & Info

- **License**: MIT (commercial use allowed after purchase on Fab).

*(c) 2026 ProtoReady Pack*
