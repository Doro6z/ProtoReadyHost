# ProtoReady: Footstep System

> **[UE 5.5+]** **[C++]** **[Production-Ready]**

Готовая к использованию система шагов на базе Physical Surfaces и DataAsset.
Для старта не требуется Blueprint‑код.

---

## Настройка

1. Включите плагин: `Edit > Plugins > ProtoReady Footstep`.
2. Определите Physical Surfaces: `Edit > Project Settings > Engine > Physics > Physical Surface`.
3. Добавьте компонент `PRFootstepComponent` в Character (или любой Actor).
4. Создайте DataAsset `PRFootstepData` и назначьте его компоненту.

> Пример поверхностей: Concrete, Dirt, Grass, Water, Wood.
> В редакторе, если существует только один `PRFootstepData`, он назначается автоматически при создании компонента.

---

## Как это работает

### Срабатывание шагов
- **AnimNotify (animation-driven)**: добавьте notify **PR Footstep** в анимации. Notify содержит `FootSocketName` и доступен сразу после установки плагина.
- **Distance (auto)**: срабатывает каждый раз, когда пройдено `DistanceInterval` см. В режиме Distance Tick включается автоматически.

### Приземление
- `bAutoTriggerLand` автоматически привязывается к `ACharacter::LandedDelegate`.
- `LandingSound` используется, если задан; иначе проигрывается обычный шаг с флагом приземления.

### Трассировки и поверхности
- Определение поверхности через трассировку по `ECC_Visibility` (Line / Sphere / Multi).
- Тип поверхности берётся из **Physical Material** меша. Если его нет, используется `SurfaceType_Default`.
- `DefaultSound` — **fallback‑звук**, используемый если поверхность не сопоставлена или Physical Material отсутствует.

---

## Параметры (tooltips видны в редакторе)

### `PRFootstepComponent`
- `FootstepData` — Tooltip: Main configuration asset. Defines surfaces, sounds, trigger modes, and trace settings.
- `bDebugTraces` — Tooltip: If true, draws debug lines for traces. Green = Hit (Surface found) / Red = Miss (Air/Gap).

### `PRFootstepData` — General
- `TriggerMode` — Tooltip: Determines how footsteps are triggered. AnimNotify = manual trigger (PRFootstep notify). Distance = automatic trigger every stride interval (cm).
  Значения: `AnimNotify`, `Distance`.

### Surfaces
- `SurfaceSounds` — Tooltip: Map Physical Surface → Footstep Sound. Keys must match Surface Types defined in Project Settings > Engine > Physics.
- `DefaultSound` — Tooltip: Fallback sound if surface is not mapped or Physical Material is None. Ideally should never be empty.

### Landing Sound
- `LandingSound` — Tooltip: Sound to play on heavy landing (Jump/Fall). If not set, standard footstep sound is used (with bIsHeavyLand flag).
- `bAutoTriggerLand` — Tooltip: If true, binds to Character::OnLanded to trigger heavy landing sound. Requires Owner to be ACharacter.

### Trace
- `TraceLength` — Tooltip: Trace length (cm) below foot socket/bone. Increase if footsteps miss on steep slopes.
- `TraceType` — Tooltip: Type of trace to use for surface detection (Line/Sphere/Multi).
  Значения: `Line` (fast, precise), `Sphere` (catch edges, recommended), `Multi` (heavier, more accurate on rough terrain).
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

> Все расстояния указаны в **сантиметрах** (единицы Unreal).

---

## AnimNotify (кратко)

Добавьте **PR Footstep** в анимацию и задайте `FootSocketName`.
Система использует сокеты, если включён `bUseFootSockets`; иначе используется трассировка от капсулы.

---

## Troubleshooting (tech)

- **Нет звука**
  - `DefaultSound` не назначен (fallback обязателен, если поверхность не сопоставлена или PhysMat отсутствует).
  - LOD: `MaxLODDistance` слишком мал (см).
- **Трассировка промахивается по земле**
  - `TraceType` = `Line` на неровной поверхности → используйте `Sphere`.
  - `TraceLength` слишком короткий.
  - `bUseFootSockets` включён, но сокеты/кости отсутствуют → fallback капсулы смещён → настройте `CapsuleZOffset`.
- **Нет вариации по поверхности**
  - Нет **Physical Material** на меше/материале → `SurfaceType_Default` → `DefaultSound` (fallback).
- **Трассировка ни во что не попадает**
  - Коллизия земли не блокирует `Visibility`.
  - Включите `bDebugTraces` для визуализации.

---

## Включённый контент

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
