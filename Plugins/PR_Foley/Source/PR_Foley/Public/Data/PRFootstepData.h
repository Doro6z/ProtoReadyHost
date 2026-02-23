#pragma once

#include "Chaos/ChaosEngineInterface.h"
#include "CoreMinimal.h"
#include "Data/PRFoleyTypes.h"
#include "Engine/DataAsset.h"

#include "PRFootstepData.generated.h"

UENUM(BlueprintType)
enum class EPRFootstepTriggerMode : uint8 { AnimNotify, Distance };

UENUM(BlueprintType)
enum class EPRTraceType : uint8 { Line, Sphere, Box, Multi };

UENUM(BlueprintType)
enum class EPRTraceStartReference : uint8 { Capsule, Root, Socket };

class USoundBase;

/**
 * Footstep Data Asset — configures surface detection traces, per-surface
 * sound mappings, per-surface VFX, jump/land behavior, and audio playback
 * settings.
 *
 * Assign to UPRFoleyComponent::FootstepData on your character.
 */
UCLASS(BlueprintType, Const, meta = (DisplayName = "PR Footstep Data Asset"))
class PR_FOLEY_API UPRFootstepData : public UPrimaryDataAsset {
  GENERATED_BODY()

public:
  UPRFootstepData();

  virtual FPrimaryAssetId GetPrimaryAssetId() const override;

  // ==================================================================
  // Trigger Mode
  // ==================================================================

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PR Footstep|Trigger")
  EPRFootstepTriggerMode TriggerMode = EPRFootstepTriggerMode::AnimNotify;

  UPROPERTY(
      EditAnywhere, BlueprintReadWrite, Category = "PR Footstep|Trigger",
      meta = (ClampMin = "10.0",
              EditCondition = "TriggerMode==EPRFootstepTriggerMode::Distance",
              EditConditionHides))
  float FootIntervalDistance;

  // ==================================================================
  // Trace Settings
  // ==================================================================

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PR Footstep|Trace")
  EPRTraceType TraceType = EPRTraceType::Sphere;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PR Footstep|Trace")
  float TraceLength;

  // clang-format off
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PR Footstep|Trace",
            meta = (ClampMin = "1.0", EditCondition = "TraceType==EPRTraceType::Sphere || TraceType==EPRTraceType::Multi", EditConditionHides))
  float SphereRadius = 10.0f;
  // clang-format on

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PR Footstep|Trace",
            meta = (EditCondition = "TraceType==EPRTraceType::Box",
                    EditConditionHides))
  FVector BoxHalfExtent = FVector(10.0f, 10.0f, 10.0f);

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PR Footstep|Trace")
  bool bUseFootSockets = true;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PR Footstep|Trace")
  TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PR Footstep|Trace",
            meta = (EditCondition = "bUseFootSockets", EditConditionHides))
  TArray<FName> FootSockets;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PR Footstep|Trace",
            meta = (EditCondition = "!bUseFootSockets", EditConditionHides))
  EPRTraceStartReference TraceStartRef = EPRTraceStartReference::Capsule;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PR Footstep|Trace",
            meta = (EditCondition = "!bUseFootSockets", EditConditionHides))
  FName ReferenceSocketName = FName("root");

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PR Footstep|Trace",
            meta = (EditCondition = "bUseFootSockets", EditConditionHides))
  float FootSocketZOffset = 20.0f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PR Footstep|Trace",
            meta = (EditCondition = "!bUseFootSockets", EditConditionHides))
  float CapsuleZOffset = 0.0f;

  // ==================================================================
  // Surfaces
  // ==================================================================

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PR Footstep|Surfaces")
  FPRSurfaceSoundSet GlobalJumpLaunch;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PR Footstep|Surfaces")
  FPRSurfaceSoundSet GlobalLandImpact;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PR Footstep|Surfaces",
            meta = (TitleProperty = "Surface"))
  TArray<FPRSurfaceFoleyConfig> Surfaces;

  // ==================================================================
  // VFX (Niagara)
  // ==================================================================

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PR Footstep|VFX")
  FPRSurfaceVFXSet DefaultVFX;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PR Footstep|VFX",
            meta = (ClampMin = "0.1", ClampMax = "5.0"))
  float VFXScaleWalk = 0.6f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PR Footstep|VFX",
            meta = (ClampMin = "0.1", ClampMax = "5.0"))
  float VFXScaleJog = 1.0f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PR Footstep|VFX",
            meta = (ClampMin = "0.1", ClampMax = "5.0"))
  float VFXScaleSprint = 1.5f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PR Footstep|VFX",
            meta = (ClampMin = "1.0", ClampMax = "5.0"))
  float VFXScaleLandMultiplier = 1.5f;

  // ==================================================================
  // Decals (Footprints)
  // ==================================================================

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PR Footstep|Decals")
  FPRSurfaceDecalSet DefaultDecal;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PR Footstep|Decals",
            meta = (ClampMin = "1", ClampMax = "128"))
  int32 MaxActiveDecals = 32;

  // ==================================================================
  // Jump
  // ==================================================================

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PR Footstep|Jump")
  bool bAutoTriggerJump = true;

  // ==================================================================
  // Landscape Blending
  // ==================================================================

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PR Footstep|Surfaces")
  bool bEnableLandscapeBlending = true;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PR Footstep|Surfaces",
            meta = (EditCondition = "bEnableLandscapeBlending",
                    EditConditionHides))
  bool bEnableLandscapeMultiTrace = true;

  UPROPERTY(
      EditAnywhere, BlueprintReadWrite, Category = "PR Footstep|Surfaces",
      meta = (EditCondition =
                  "bEnableLandscapeBlending && bEnableLandscapeMultiTrace",
              EditConditionHides, ClampMin = "5.0", ClampMax = "50.0"))
  float LandscapeMultiTraceRadius = 20.0f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PR Footstep|Surfaces",
            meta = (EditCondition = "bEnableLandscapeBlending",
                    EditConditionHides, ClampMin = "0.0", ClampMax = "1.0"))
  float LandscapeBlendThreshold = 0.25f;

  // ==================================================================
  // Landing
  // ==================================================================

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PR Footstep|Landing")
  bool bAutoTriggerLand = true;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PR Footstep|Landing")
  float LandingTraceOffset = 0.0f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PR Footstep|Landing")
  bool bPlayFootstepAfterLandImpact = true;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PR Footstep|Landing",
            meta = (ClampMin = "0.0",
                    EditCondition = "bPlayFootstepAfterLandImpact",
                    EditConditionHides))
  float FootstepDelayAfterLand = 0.1f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PR Footstep|Landing",
            meta = (ClampMin = "0.0"))
  float HeavyLandThresholdVelocity = 500.0f;

  // Audio Playback
  // ==================================================================

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PR Footstep|Audio")
  FPRFoleyAudioSettings SurfaceAudio;

  // ==================================================================
  // Optimization
  // ==================================================================

  UPROPERTY(EditAnywhere, BlueprintReadWrite,
            Category = "PR Footstep|Optimization", meta = (ClampMin = "0.0"))
  float MaxLODDistance = 3000.0f;
};
