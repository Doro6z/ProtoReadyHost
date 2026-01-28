PROTOREADY: FOOTSTEP SYSTEM
UE 5.5+ | C++ | Production-Ready

Ready-to-use Footstep System based on Physical Surfaces and DataAssets.
No Blueprint code required to start.

---
SETUP
---

1. Enable Plugin: Edit > Plugins > ProtoReady Footstep.
2. Define Physical Surfaces: Edit > Project Settings > Engine > Physics > Physical Surface.
3. Add 'PRFootstepComponent' to your Character (or any Actor).
4. Create a 'PRFootstepData' DataAsset and assign it to the component.

Example Surfaces: Concrete, Dirt, Grass, Water, Wood.
Note: In Editor, if only one PRFootstepData exists, it is auto-assigned upon component creation.

---
FEATURES
---

[Triggering]
- AnimNotify (Animation-Driven): Add 'PR Footstep' notify in your animations. Supports FootSocketName.
- Distance (Automatic): Triggers a step every 'DistanceInterval' cm traveled.

[Landing]
- Auto-binds to Character::LandedDelegate if 'bAutoTriggerLand' is true.
- Uses separate 'LandingSound' or falls back to standard footstep with heavy flag.

[Trace & Surfaces]
- Detects surface via 'ECC_Visibility' trace (Line / Sphere / Box / Multi).
- Maps Physical Material to Sound. Uses 'DefaultSound' as fallback.
- Supports Debug Drawing (bDebugTraces) to visualize hits/misses.

---
PARAMETERS
---

[PRFootstepComponent]
- FootstepData: Main configuration asset.
- bDebugTraces: Draws debug lines (Green=Hit, Red=Miss).

[PRFootstepData]
- TriggerMode: AnimNotify vs Distance.
- SurfaceSounds: Map Physical Surface -> SoundBase.
- DefaultSound: Fallback sound.
- LandingSound: Clean landing sound.
- LandingTraceOffset: Z offset logic for landing timing.

[Trace Config]
- TraceType: Line (Fast), Sphere (Recommended), Box (Flat), Multi (Accurate).
- SphereRadius / BoxHalfExtent: Size of the trace shape.
- bUseFootSockets: Toggle socket-based vs capsule-based tracing.
- Socket List: Define specific bones/sockets to trace from.

[Audio]
- VolumeRange / PitchRange: Randomization.
- AttenuationSettings: 3D Spatialization asset.
- EffectsChain: DSP (Reverb/EQ).

---
PACKAGE CONTENT
---

- 5 SoundCues: Concrete, Dirt, Grass, Water, Wood.
- 23 WAV Files: High-quality footstep layers.
- 1 Landing Sound: Generic Jump/Land.
- 1 MetaSound Demo: Procedural Metal synthesis.
- 1 Demo Map: Showcase level.

---
SUPPORT
---
License: MIT (Commercial use allowed).
(c) 2026 ProtoReady Pack
