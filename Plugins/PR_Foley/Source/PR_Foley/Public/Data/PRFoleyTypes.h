#pragma once

#include "Chaos/ChaosEngineInterface.h"
#include "CoreMinimal.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundConcurrency.h"
#include "Sound/SoundEffectSource.h"

#include "PRFoleyTypes.generated.h"

class USoundBase;
class USoundEffectSourcePresetChain;
class USoundAttenuation;
class USoundConcurrency;
class UNiagaraSystem;
class UMaterialInterface;

UENUM(BlueprintType)
enum class EPRVelocityTier : uint8 { Idle, Walk, Jog, Sprint };

/** Event type used internally for network multicast and VFX spawning. */
UENUM(BlueprintType)
enum class EPRFoleyEventType : uint8 { Footstep, Jump, Land };

/**
 * A set of sound variations for a single purpose (e.g. one surface's
 * footstep). Fill with 3+ sounds for natural variation. The shuffle system
 * prevents the same sound from playing twice in a row.
 */
USTRUCT(BlueprintType)
struct FPRSurfaceSoundSet {
  GENERATED_BODY()

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound Set")
  TArray<TObjectPtr<USoundBase>> Sounds;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound Set")
  bool bShuffleNoRepeat = true;
};

/**
 * Shared audio playback settings used by each layer (Footstep, Voice).
 * Controls volume/pitch randomization, spatialization, DSP effects, and
 * concurrency limiting.
 */
USTRUCT(BlueprintType)
struct FPRFoleyAudioSettings {
  GENERATED_BODY()

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio",
            meta = (ClampMin = "0.0", ClampMax = "2.0"))
  FVector2D VolumeRange = FVector2D(0.8f, 1.0f);

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio",
            meta = (ClampMin = "0.1", ClampMax = "3.0"))
  FVector2D PitchRange = FVector2D(0.9f, 1.1f);

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
  TObjectPtr<USoundAttenuation> AttenuationSettings;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
  TObjectPtr<USoundEffectSourcePresetChain> EffectsChain;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
  TObjectPtr<USoundConcurrency> ConcurrencySettings;
};

/**
 * Per-surface VFX configuration. Maps particle effects to footstep,
 * jump and land events. Each field is optional — leave null for no VFX
 * on that event type. Scale multiplier adjusts particle size per surface
 * (e.g. bigger dust on sand, smaller on concrete).
 */
USTRUCT(BlueprintType)
struct FPRSurfaceVFXSet {
  GENERATED_BODY()

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
  TObjectPtr<UNiagaraSystem> FootstepVFX;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
  TObjectPtr<UNiagaraSystem> JumpVFX;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX")
  TObjectPtr<UNiagaraSystem> LandVFX;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VFX",
            meta = (ClampMin = "0.01", ClampMax = "10.0"))
  float ScaleMultiplier = 1.0f;
};

/**
 * Per-surface footprint decal configuration. Spawns a decal at each
 * footstep location to leave visible marks (snow, mud, sand, blood).
 * Each field is optional — leave DecalMaterial null for no decals.
 */
USTRUCT(BlueprintType)
struct FPRSurfaceDecalSet {
  GENERATED_BODY()

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Decals")
  TObjectPtr<UMaterialInterface> DecalMaterial;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Decals")
  FName FrameIndexParamName = FName("FrameIndex");

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Decals")
  TArray<int32> LeftFootFrames = {0, 2, 4, 6, 8, 10};

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Decals")
  TArray<int32> RightFootFrames = {1, 3, 5, 7, 9, 11};

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Decals")
  FVector DecalOffset = FVector::ZeroVector;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Decals")
  FVector2D DecalSize = FVector2D(20.0f, 40.0f);

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Decals",
            meta = (ClampMin = "0.5", ClampMax = "120.0"))
  float LifeSpan = 10.0f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Decals")
  float FadeScreenSize = 0.0f;
};

/**
 * Per-surface sound configuration. Maps a Physical Surface type to
 * footstep sounds, with optional per-surface jump/land overrides.
 */
USTRUCT(BlueprintType)
struct FPRSurfaceFoleyConfig {
  GENERATED_BODY()

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surfaces")
  TEnumAsByte<EPhysicalSurface> Surface = EPhysicalSurface::SurfaceType_Default;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surfaces")
  FPRSurfaceSoundSet Footstep;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surfaces",
            meta = (InlineEditConditionToggle))
  bool bOverrideJump = false;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surfaces")
  FPRSurfaceSoundSet JumpLaunch;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surfaces",
            meta = (InlineEditConditionToggle))
  bool bOverrideLand = false;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surfaces")
  FPRSurfaceSoundSet LandImpact;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surfaces")
  FPRSurfaceVFXSet VFX;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surfaces")
  FPRSurfaceDecalSet Decal;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surfaces")
  float VolumeMultiplier = 1.0f;

  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surfaces")
  float PitchMultiplier = 1.0f;
};
