#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Data/PRFootstepData.h"
#include "PRFootstepComponent.generated.h"

// Forward Decls
class UPRFootstepData;
class USkeletalMeshComponent;

/**
 * @brief Advanced Footstep System Component.
 *
 * Handles physical surface detection via Traces (Sphere/Line/Multi) and
 * triggers corresponding audio from a DataAsset. Supports both
 * **Animation-Sync** (AnimNotify) and **Distance-Based** (Automatic) workflows.
 *
 * @see UPRFootstepData
 */
UCLASS(ClassGroup = (ProtoReady), meta = (BlueprintSpawnableComponent))
class PR_FOOTSTEP_API UPRFootstepComponent : public UActorComponent {
  GENERATED_BODY()

public:
  UPRFootstepComponent();

  virtual void BeginPlay() override;
  virtual void
  TickComponent(float DeltaTime, ELevelTick TickType,
                FActorComponentTickFunction *ThisTickFunction) override;

#if WITH_EDITOR
  virtual void OnComponentCreated() override;
#endif

  // --- Configuration ---

  /**
   * @brief Main configuration asset.
   * Defines surfaces, sounds, trigger modes, and trace settings.
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProtoReady")
  TObjectPtr<UPRFootstepData> FootstepData;

  /**
   * @brief If true, draws debug lines for traces.
   * - Green = Hit (Surface found)
   * - Red = Miss (Air/Gap)
   */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ProtoReady|Debug")
  bool bDebugTraces = false;

  // --- Core API ---

  /**
   * @brief Manually triggers a footstep trace from a specific socket.
   * Typically called via AnimModify (AnimSequence) or Blueprint.
   *
   * @param SocketName The name of the socket to trace from (e.g., 'foot_l').
   * If 'None', falls back to Actor location logic defined in DataAsset.
   */
  UFUNCTION(BlueprintCallable, Category = "Footsteps")
  void TriggerFootstep(FName SocketName);

  /**
   * @brief Triggers a heavy landing sound.
   * Uses a longer vertical trace to ensure ground detection.
   * Can be auto-bound to Character::OnLanded via DataAsset.
   */
  UFUNCTION(BlueprintCallable, Category = "Footsteps")
  void TriggerLand();

  /**
   * @brief Console Command: "TestFootstep"
   * Forces a footstep trigger immediately for debugging purposes.
   */
  UFUNCTION(Exec)
  void TestFootstep();

  /**
   * @brief Manually sets the distance interval (stride) at runtime.
   * Useful for changing stride when character speed changes (Walk vs Run).
   */
  UFUNCTION(BlueprintCallable, Category = "Footsteps")
  void SetFootIntervalDistance(float NewInterval);

  /**
   * @brief Gets the current distance interval (stride).
   */
  UFUNCTION(BlueprintCallable, Category = "Footsteps")
  float GetFootIntervalDistance() const;

  /**
   * @brief Delegate bound to ACharacter::LandedDelegate.
   * Automatically calls TriggerLand().
   */
  UFUNCTION()
  void OnLanded(const FHitResult &Hit);

protected:
  /**
   * Performs the trace logic.
   * Tries SphereTrace first, falls back to LineTrace if needed.
   */
  bool PerformTrace(const FVector &Start, const FVector &End,
                    FHitResult &OutHit);

  /**
   * Resolves the surface from HitResult.
   * Handles Physical Material and potentially Multi-Surface blending logic.
   */
  EPhysicalSurface GetSurfaceFromHit(const FHitResult &Hit);

  /** Spawns the sound optimized (Fire & Forget) */
  void PlayFootstepSound(EPhysicalSurface SurfaceType, const FVector &Location,
                         bool bIsHeavyLand = false);

private:
  // Optimization: Cached owner mesh for socket lookup
  UPROPERTY()
  TObjectPtr<USkeletalMeshComponent> OwnerMesh;

  /** Finds the first SkeletalMeshComponent on owner to define socket context */
  void CacheOwnerMesh();

  // --- Internal State (Distance Mode) ---

  /** Accumulated distance for stride calculation */
  double AccumulatedDistance = 0.0;

  /** Local copy of interval to allow runtime modification without dirtying
   * Asset */
  float InstanceDistanceInterval = 0.0f;

  /** Location of owner last frame */
  FVector LastLocation;

  /** Index of the current foot in the FootSockets array */
  int32 CurrentFootIndex = 0;

#if WITH_EDITOR
  void AutoAssignFootstepData();
#endif
};
