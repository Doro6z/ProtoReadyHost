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
   * @brief Single line trace. Fast and precise but may miss edges designated
   * for small objects.
   */
  Line,

  /**
   * @brief Sphere sweep trace. Better for catching edges and uneven surfaces.
   * Recommended for most use cases.
   */
  Sphere,

  /**
   * @brief Box sweep trace. Good for flat feet or rectangular contact areas.
   */
  Box,

  /**
   * @brief Multi-line trace. Shoots multiple rays for complex surfaces.
   * Heavier but more accurate on rough terrain.
   */
  Multi
};

UENUM(BlueprintType)
enum class EPRTraceStartReference : uint8 {
  /** @brief Trace starts from the Actor's Capsule Center (with Z Offset). */
  Capsule,

  /** @brief Trace starts from the Actor's Root Component Location (0,0,0
     local). */
  Root,

  /** @brief Trace starts from the specified Socket Location. */
  Socket
};

class USoundBase;
class USoundEffectSourcePresetChain;

/**
 * @brief Configuration asset for the Footstep System.
 *
 * Centralizes all settings:
 * - Surface to Sound mapping
 * - Trace parameters (Line/Sphere/Box, Size, Distance)
 * - Trigger behavior (AnimNotify vs Distance)
 * - Audio modulation (Volume/Pitch/Attenuation)
 */
UCLASS(BlueprintType, Const, meta = (DisplayName = "PR Footstep Data Asset"))
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
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PR Footstep|General")
  EPRFootstepTriggerMode TriggerMode = EPRFootstepTriggerMode::AnimNotify;

  /**
   * @brief Distance traveled (cm) before triggering a step.
   * Active only when Trigger Mode is set to Distance.
   *
   * Formula: Stride = Speed / StepsPerSec.
   */
  UPROPERTY(
      EditAnywhere, BlueprintReadWrite, Category = "PR Footstep|General",
      meta = (ClampMin = "10.0",
              EditCondition = "TriggerMode == EPRFootstepTriggerMode::Distance",
              EditConditionHides))
  float FootIntervalDistance;

  // --- Surface Mappings ---

  /**
   * @brief Map Physical Surface -> Footstep Sound.
   *
   * @note Keys must match Surface Types defined in 'Project Settings > Engine >
   * Physics'. Example: SurfaceType1 (Concrete) -> SC_Concrete.
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PR Footstep|Surfaces")
  TMap<TEnumAsByte<EPhysicalSurface>, TObjectPtr<USoundBase>> SurfaceSounds;

  /**
   * @brief Fallback sound if surface is not mapped or Physical Material is
   * None.
   * @warning Ideally should never be empty.
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PR Footstep|Surfaces")
  TObjectPtr<USoundBase> DefaultSound;

  // --- Landing Sound ---

  /**
   * @brief Sound to play on heavy landing (Jump/Fall).
   * If not set, standard footstep sound is used (with bIsHeavyLand flag).
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite,
            Category = "PR Footstep|Landing Sound")
  TObjectPtr<USoundBase> LandingSound;

  /**
   * @brief If true, binds to Character::OnLanded to trigger heavy landing
   * sound. Requires Owner to be ACharacter.
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite,
            Category = "PR Footstep|Landing Sound")
  bool bAutoTriggerLand = true;

  /**
   * @brief Z offset (cm) specific for Landing trace start.
   * Useful to trigger landing sound slightly earlier or later.
   * Default: 0.0f.
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite,
            Category = "PR Footstep|Landing Sound")
  float LandingTraceOffset = 0.0f;

  // --- Trace Configuration ---

  /**
   * @brief Type of trace to use for surface detection.
   * - Line: Fast, precise.
   * - Sphere: Catch edges (Recommended).
   * - Box: Good for flat feet.
   * - Multi: Complex terrain accuracy.
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PR Footstep|Trace")
  EPRTraceType TraceType = EPRTraceType::Sphere;

  /**
   * @brief Trace length (cm) below foot socket/bone.
   * Increase this value if footsteps miss on steep slopes.
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PR Footstep|Trace",
            meta = (ClampMin = "5.0"))
  float TraceLength;

  /**
   * @brief Radius (Sphere) of the trace shape.
   * Only used if TraceType is Sphere or Multi.
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PR Footstep|Trace",
            meta = (ClampMin = "1.0",
                    EditCondition = "TraceType == EPRTraceType::Sphere",
                    EditConditionHides))
  float SphereRadius = 10.0f;

  /**
   * @brief Half-Extent of the Box trace (X, Y, Z).
   * Only used if TraceType is Box.
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PR Footstep|Trace",
            meta = (EditCondition = "TraceType == EPRTraceType::Box",
                    EditConditionHides))
  FVector BoxHalfExtent = FVector(10.0f, 10.0f, 10.0f);

  /**
   * @brief Toggle to enable Socket-based tracing (Iterates through FootSockets
   * list). If False, uses TraceStartRef to determine start point.
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PR Footstep|Trace")
  bool bUseFootSockets = true;

  /**
   * @brief List of Sockets to trace from.
   * Cycles sequentially in Distance Mode.
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PR Footstep|Trace",
            meta = (EditCondition = "bUseFootSockets", EditConditionHides))
  TArray<FName> FootSockets;

  /**
   * @brief Fallback bone names if Sockets not found.
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PR Footstep|Trace",
            meta = (EditCondition = "bUseFootSockets", EditConditionHides))
  TArray<FName> FootBones;

  /**
   * @brief Determines where to start the trace when NOT using the FootSockets
   * list.
   * - Capsule: Start from Actor Capsule Center (+ Z Offset).
   * - Root: Start from Actor Root Component Location (+ Z Offset).
   * - Socket: Start from a specific single socket (ReferenceSocketName).
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PR Footstep|Trace",
            meta = (EditCondition = "!bUseFootSockets", EditConditionHides))
  EPRTraceStartReference TraceStartRef = EPRTraceStartReference::Capsule;

  /**
   * @brief Name of the socket to use as trace origin when TraceStartRef is
   * Socket.
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PR Footstep|Trace",
            meta = (EditCondition = "!bUseFootSockets", EditConditionHides))
  FName ReferenceSocketName;

  /**
   * @brief Z offset (cm) from Start Reference.
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PR Footstep|Trace",
            meta = (EditCondition = "!bUseFootSockets", EditConditionHides))
  float CapsuleZOffset = -89.0f;

  // --- Audio Settings ---

  /** @brief Random Volume range [Min, Max] (Default: ~1.0) */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PR Footstep|Audio")
  FVector2D VolumeRange;

  /** @brief Random Pitch range [Min, Max] (Default: ~1.0) */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PR Footstep|Audio")
  FVector2D PitchRange;

  /**
   * @brief Attenuation settings for 3D spatialization.
   * If empty, sound plays as 2D UI sound (heard everywhere).
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PR Footstep|Audio")
  TObjectPtr<USoundAttenuation> AttenuationSettings;

  /**
   * @brief DSP Effects Chain (Reverb, EQ).
   * @note Using this spawns an AudioComponent (heavier than static
   * fire-and-forget).
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PR Footstep|Audio")
  TObjectPtr<USoundEffectSourcePresetChain> EffectsChain;

  // --- LOD (Optimization) ---

  /**
   * @brief Max distance (cm) from camera to allow processing.
   * System stops ticking/tracing beyond this range to save performance.
   * Set to 0 to disable LOD.
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PR Footstep|LOD")
  float MaxLODDistance = 3000.0f;
};
