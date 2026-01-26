#pragma once

#include "Chaos/ChaosEngineInterface.h" // For EPhysicalSurface
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundEffectSource.h"

#include "PRFootstepData.generated.h"

UENUM(BlueprintType)
enum class EPRFootstepTriggerMode : uint8 {
  /**
   * @brief Standard mode. Footsteps are triggered manually via AnimNotifies in
   * Animation Sequences. Best for: Player Characters, detailed animations.
   */
  AnimNotify,

  /**
   * @brief Automatic mode. Footsteps are triggered based on distance traveled.
   * Best for: NPCs, simple prototypes, or non-animated actors.
   *
   * STRIDE CALCULATION: For a biped at 600 cm/s walk speed:
   * - Walk: 90-120 cm interval (sounds natural)
   * - Run: 150-200 cm interval
   * Formula: Stride = Speed / DesiredStepsPerSecond (e.g., 600 / 5 = 120)
   */
  Distance
};

UENUM(BlueprintType)
enum class EPRTraceType : uint8 {
  /**
   * @brief Single line trace. Fast and precise but may miss edges.
   */
  Line,

  /**
   * @brief Sphere sweep trace. Better for catching edges and uneven surfaces.
   * Recommended for most use cases.
   */
  Sphere,

  /**
   * @brief Multi-line trace. Shoots multiple rays for complex surfaces.
   * Heavier but more accurate on rough terrain.
   */
  Multi
};

class USoundBase;
class USoundEffectSourcePresetChain;

/**
 * @brief Configuration asset for the Footstep System.
 *
 * Centralizes all settings:
 * - Surface to Sound mapping
 * - Trace parameters (Line/Sphere, Size, Distance)
 * - Trigger behavior (AnimNotify vs Distance)
 * - Audio modulation (Volume/Pitch/Attenuation)
 */
UCLASS(BlueprintType, Const)
class PR_FOOTSTEP_API UPRFootstepData : public UPrimaryDataAsset {
  GENERATED_BODY()

public:
  UPRFootstepData();

  //~ Begin UPrimaryDataAsset Interface
  virtual FPrimaryAssetId GetPrimaryAssetId() const override;
  //~ End UPrimaryDataAsset Interface

  // --- Main Configuration ---

  /**
   * @brief Determines how footsteps are triggered.
   * - AnimNotify: Manual trigger. Requires 'PRFootstep' notify in Animation
   * Sequence. Best for high-fidelity sync (walking, running, specific gaits).
   * - Distance: Automatic trigger. Fires every 'Stride Interval' cm.
   *   Best for simple movement, prototypes, or actors without AnimBPs.
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProtoReady|General")
  EPRFootstepTriggerMode TriggerMode = EPRFootstepTriggerMode::AnimNotify;

  // --- Surface Mappings ---

  /**
   * @brief Map Physical Surface -> Footstep Sound.
   *
   * @note Keys must match Surface Types defined in 'Project Settings > Engine >
   * Physics'. Example: SurfaceType1 (Concrete) -> SC_Concrete.
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProtoReady|Surfaces")
  TMap<TEnumAsByte<EPhysicalSurface>, TObjectPtr<USoundBase>> SurfaceSounds;

  /**
   * @brief Fallback sound if surface is not mapped or Physical Material is
   * None.
   * @warning Ideally should never be empty.
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProtoReady|Surfaces")
  TObjectPtr<USoundBase> DefaultSound;

  // --- Landing Sound ---

  /**
   * @brief Sound to play on heavy landing (Jump/Fall).
   * If not set, standard footstep sound is used (with bIsHeavyLand flag).
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite,
            Category = "ProtoReady|Landing Sound")
  TObjectPtr<USoundBase> LandingSound;

  /**
   * @brief If true, binds to Character::OnLanded to trigger heavy landing
   * sound. Requires Owner to be ACharacter.
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite,
            Category = "ProtoReady|Landing Sound")
  bool bAutoTriggerLand = true;

  // --- Trace Configuration ---

  /**
   * @brief Trace length (cm) below foot socket/bone.
   * Increase this value if footsteps miss on steep slopes.
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProtoReady|Trace",
            meta = (ClampMin = "5.0"))
  float TraceLength;

  /**
   * @brief Type of trace to use for surface detection.
   * - Line: Fast, precise.
   * - Sphere: Catch edges (Recommended).
   * - Multi: Complex terrain accuracy.
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProtoReady|Trace")
  EPRTraceType TraceType = EPRTraceType::Sphere;

  /**
   * @brief List of Sockets to trace from.
   * Cycles sequentially in Distance Mode.
   * @note If empty, system falls back to Root/Capsule trace.
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProtoReady|Trace",
            meta = (EditCondition = "bUseFootSockets"))
  TArray<FName> FootSockets;

  /**
   * @brief Fallback bone names if Sockets not found.
   * Used as alternative to Sockets.
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProtoReady|Trace",
            meta = (EditCondition = "bUseFootSockets"))
  TArray<FName> FootBones;

  /**
   * @brief Toggle between Socket-based and Capsule-based tracing.
   * - True: Uses FootSockets/FootBones.
   * - False: Traces from Actor Center (Capsule) with Z Offset.
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProtoReady|Trace")
  bool bUseFootSockets = true;

  /**
   * @brief Z offset (cm) from Actor origin when NOT using foot sockets.
   * Negative values move trace start down toward foot level (e.g. -50).
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProtoReady|Trace",
            meta = (EditCondition = "!bUseFootSockets", EditConditionHides))
  float CapsuleZOffset = -50.0f;

  /**
   * @brief Radius (Sphere) of the trace shape.
   * Larger values catch edges better but might hit walls.
   * Default: 10.0 cm.
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProtoReady|Trace",
            meta = (ClampMin = "1.0"))
  float TraceShapeSize = 10.0f;

  // --- Audio Settings ---

  /** @brief Random Volume range [Min, Max] (Default: ~1.0) */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProtoReady|Audio")
  FVector2D VolumeRange;

  /** @brief Random Pitch range [Min, Max] (Default: ~1.0) */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProtoReady|Audio")
  FVector2D PitchRange;

  /**
   * @brief Attenuation settings for 3D spatialization.
   * If empty, sound plays as 2D UI sound (heard everywhere).
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProtoReady|Audio")
  TObjectPtr<USoundAttenuation> AttenuationSettings;

  /**
   * @brief DSP Effects Chain (Reverb, EQ).
   * @note Using this spawns an AudioComponent (heavier than static
   * fire-and-forget).
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProtoReady|Audio")
  TObjectPtr<USoundEffectSourcePresetChain> EffectsChain;

  // --- Distance Mode Settings ---

  /**
   * @brief Distance traveled (cm) before triggering a step (Distance Mode).
   *
   * Formula: Stride = MaxWalkSpeed / StepsPerSec.
   * - Walk (300): ~80cm
   * - Jog (600): ~120cm
   * - Sprint (900): ~180cm
   */
  UPROPERTY(
      EditAnywhere, BlueprintReadWrite, Category = "ProtoReady|Distance Mode",
      meta = (ClampMin = "10.0",
              EditCondition = "TriggerMode == EPRFootstepTriggerMode::Distance",
              EditConditionHides))
  float DistanceInterval;

  // --- LOD (Optimization) ---

  /**
   * @brief Max distance (cm) from camera to allow processing.
   * System stops ticking/tracing beyond this range to save performance.
   * Set to 0 to disable LOD.
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProtoReady|LOD")
  float MaxLODDistance = 3000.0f;
};
