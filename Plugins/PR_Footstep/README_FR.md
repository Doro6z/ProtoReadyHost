# ProtoReady: Footstep System

> **[UE 5.5+]** **[C++]** **[Production-Ready]**

Système de pas prêt à l’emploi, basé sur Physical Surfaces et DataAsset.
Aucun code Blueprint requis pour démarrer.

---

## Setup

1. Activer le plugin : `Edit > Plugins > ProtoReady Footstep`.
2. Définir vos Physical Surfaces : `Edit > Project Settings > Engine > Physics > Physical Surface`.
3. Ajouter le composant `PRFootstepComponent` à votre Character (ou tout Actor).
4. Créer un DataAsset `PRFootstepData` et l’assigner au composant.

> Exemple de surfaces : Concrete, Dirt, Grass, Water, Wood.
> En éditeur, si un seul `PRFootstepData` existe, il est auto‑assigné à la création du composant.

---

## Fonctionnement

### Déclenchement des pas
- **AnimNotify (animation-driven)** : ajoutez le notify **PR Footstep** dans vos animations. Le notify expose `FootSocketName` et est disponible dès l’installation du plugin.
- **Distance (auto)** : déclenche un pas tous les `DistanceInterval` cm parcourus. En Distance, le Tick est activé automatiquement.

### Atterrissage
- `bAutoTriggerLand` bind automatiquement `ACharacter::LandedDelegate`.
- `LandingSound` est utilisé si défini, sinon le système rejoue un pas standard avec le flag d’atterrissage.

### Traces & surfaces
- Détection par trace sur `ECC_Visibility` (Line / Sphere / Multi).
- Les surfaces proviennent du **Physical Material** du mesh. Si absent, `SurfaceType_Default` est utilisé.
- `DefaultSound` est le **son de secours** (fallback) utilisé quand la surface n’est pas mappée ou quand aucun Physical Material n’est trouvé.

---

## Paramètres (tooltips visibles dans l’éditeur)

### `PRFootstepComponent`
- `FootstepData` — Tooltip : Main configuration asset. Defines surfaces, sounds, trigger modes, and trace settings.
- `bDebugTraces` — Tooltip : If true, draws debug lines for traces. Green = Hit (Surface found) / Red = Miss (Air/Gap).

### `PRFootstepData` — General
- `TriggerMode` — Tooltip : Determines how footsteps are triggered. AnimNotify = manual trigger (PRFootstep notify). Distance = automatic trigger every stride interval (cm).
  Valeurs : `AnimNotify`, `Distance`.

### Surfaces
- `SurfaceSounds` — Tooltip : Map Physical Surface → Footstep Sound. Keys must match Surface Types defined in Project Settings > Engine > Physics.
- `DefaultSound` — Tooltip : Fallback sound if surface is not mapped or Physical Material is None. Ideally should never be empty.

### Landing Sound
- `LandingSound` — Tooltip : Sound to play on heavy landing (Jump/Fall). If not set, standard footstep sound is used (with bIsHeavyLand flag).
- `bAutoTriggerLand` — Tooltip : If true, binds to Character::OnLanded to trigger heavy landing sound. Requires Owner to be ACharacter.

### Trace
- `TraceLength` — Tooltip : Trace length (cm) below foot socket/bone. Increase if footsteps miss on steep slopes.
- `TraceType` — Tooltip : Type of trace to use for surface detection (Line/Sphere/Multi).
  Valeurs : `Line` (fast, precise), `Sphere` (catch edges, recommended), `Multi` (heavier, more accurate on rough terrain).
- `FootSockets` — Tooltip : List of sockets to trace from. Cycles sequentially in Distance Mode. If empty, system falls back to Root/Capsule trace.
- `FootBones` — Tooltip : Fallback bone names if Sockets not found. Used as alternative to Sockets.
- `bUseFootSockets` — Tooltip : Toggle between socket-based and capsule-based tracing. True = uses FootSockets/FootBones. False = traces from Actor center (Capsule) with Z offset.
- `CapsuleZOffset` — Tooltip : Z offset (cm) from Actor origin when NOT using foot sockets. Negative values move trace start down toward foot level (e.g. -50).
- `TraceShapeSize` — Tooltip : Radius (Sphere) of the trace shape. Larger values catch edges better but might hit walls. Default: 10.0 cm.

### Audio
- `VolumeRange` — Tooltip : Random Volume range [Min, Max].
- `PitchRange` — Tooltip : Random Pitch range [Min, Max].
- `AttenuationSettings` — Tooltip : Attenuation settings for 3D spatialization. If empty, sound plays as 2D UI sound (heard everywhere).
- `EffectsChain` — Tooltip : DSP Effects Chain (Reverb, EQ). Using this spawns an AudioComponent (heavier than static fire-and-forget).

### Distance Mode
- `DistanceInterval` — Tooltip : Distance traveled (cm) before triggering a step (Distance Mode). Formula: Stride = MaxWalkSpeed / StepsPerSec.

### LOD
- `MaxLODDistance` — Tooltip : Max distance (cm) from camera to allow processing. System stops ticking/tracing beyond this range to save performance. Set to 0 to disable LOD.

> Toutes les distances sont en **centimètres** (unités Unreal).

---

## Utilisation AnimNotify (résumé)

Ajoutez **PR Footstep** dans l’animation et renseignez `FootSocketName`.
Le système utilise les sockets si `bUseFootSockets` est activé; sinon il bascule sur la trace capsule.

---

## Troubleshooting (tech)

- **Aucun son**
  - `DefaultSound` non assigné (fallback requis si surface non mappée ou PhysMat absent).
  - LOD : `MaxLODDistance` trop faible (cm).
- **Trace rate le sol**
  - `TraceType` à `Line` sur terrain irrégulier → passer `Sphere`.
  - `TraceLength` trop court.
  - `bUseFootSockets` activé mais sockets/bones inexistants → fallback capsule mal positionné → ajuster `CapsuleZOffset`.
- **Pas de variation de surface**
  - Pas de **Physical Material** sur le mesh/material → `SurfaceType_Default` → `DefaultSound` (fallback).
- **Trace ne touche rien**
  - Collision du sol ne bloque pas `Visibility`.
  - Activez `bDebugTraces` pour visualiser.

---

## Contenu fourni

- **SoundCues (5, inclus)** : `SC_Concrete`, `SC_Dirt`, `SC_Grass`, `SC_Water`, `SC_Wood`.
- **WAV Footsteps (23, inclus)** : Concrete ×3, Dirt ×5, Grass ×5, Water ×5, Wood ×5.
- **Landing (1, inclus)** : `S_PR_JumpLanding`.
- **MetaSound (demo)** : `MS_Footstep_Metal` (synthèse métal).
- **DataAsset** : `DA_Footstep_Config_A`.
- **Map de démo** : `Level_PR_Footstep_Showcase`.

---

## Support & Infos

- **License** : MIT (usage commercial autorisé après achat sur Fab).

*(c) 2026 ProtoReady Pack*
