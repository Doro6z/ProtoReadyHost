#include "PRFoleyComponent.h"
#include "Components/AudioComponent.h"
#include "Components/DecalComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h" // For GEngine
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "LandscapeComponent.h"
#include "LandscapeHeightfieldCollisionComponent.h"
#include "LandscapeLayerInfoObject.h"
#include "LandscapeProxy.h" // For Landscape detection
#include "Modules/ModuleManager.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "PRAudioLog.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "WorldCollision.h" // Correct header for FOverlapResult

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#endif

// ============================================================================
// Constructor
// ============================================================================

UPRFoleyComponent::UPRFoleyComponent() {
  PrimaryComponentTick.bCanEverTick = true;
  PrimaryComponentTick.bStartWithTickEnabled = false;
  SetIsReplicatedByDefault(true);
}

// ============================================================================
// Replication
// ============================================================================

void UPRFoleyComponent::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty> &OutLifetimeProps) const {
  Super::GetLifetimeReplicatedProps(OutLifetimeProps);
  // No replicated properties needed — we use RPCs only.
  // DataAssets are identical on all clients (loaded from package).
}

// ============================================================================
// BeginPlay
// ============================================================================

void UPRFoleyComponent::BeginPlay() {
  Super::BeginPlay();
  CacheOwnerMesh();

  // Initialize Distance Mode state
  if (GetOwner()) {
    LastLocation = GetOwner()->GetActorLocation();
  }

  bool bTickEnabled = false;

  if (FootstepData) {
    // Enable Tick if in Distance Mode (any active step-driven layer)
    if (FootstepData->TriggerMode == EPRFootstepTriggerMode::Distance &&
        (bEnableFootstepLayer || bEnableVoiceLayer)) {
      SetComponentTickEnabled(true);
      bTickEnabled = true;
      // Init local interval
      InstanceDistanceInterval = FootstepData->FootIntervalDistance;
    }

    // Auto Bind to Landed
    if (FootstepData->bAutoTriggerLand) {
      if (ACharacter *OwnerChar = Cast<ACharacter>(GetOwner())) {
        OwnerChar->LandedDelegate.AddDynamic(this,
                                             &UPRFoleyComponent::OnLanded);
      }
    }

    // Auto-detect jump: bind to MovementModeChanged delegate
    if (FootstepData->bAutoTriggerJump) {
      if (ACharacter *OwnerChar = Cast<ACharacter>(GetOwner())) {
        OwnerChar->MovementModeChangedDelegate.AddDynamic(
            this, &UPRFoleyComponent::OnMovementModeChanged);
      }
    }
  }

  if (bEnableVoiceLayer && VoiceData &&
      (VoiceData->BreathingLoops.Num() > 0 || VoiceData->BreathingMetaSound)) {
    if (!bTickEnabled) {
      SetComponentTickEnabled(true);
    }
    UpdateBreathingLoop();
  }
}

// ============================================================================
// Editor
// ============================================================================

#if WITH_EDITOR
void UPRFoleyComponent::OnComponentCreated() {
  Super::OnComponentCreated();
  AutoAssignFootstepData();
  AutoAssignVoiceData();
}
#endif

void UPRFoleyComponent::OnLanded(const FHitResult &Hit) { Landing(); }

void UPRFoleyComponent::OnMovementModeChanged(ACharacter *Character,
                                              EMovementMode PrevMode,
                                              uint8 PrevCustomMode) {
  // Detect jump: was walking/on ground, now falling with upward velocity
  if (PrevMode == MOVE_Walking || PrevMode == MOVE_NavWalking) {
    if (Character && Character->GetCharacterMovement() &&
        Character->GetCharacterMovement()->IsFalling()) {
      if (Character->GetVelocity().Z > 0.0f) {
        HandleJump();
      }
    }
  }
}

// ============================================================================
// Console Test Commands
// ============================================================================

void UPRFoleyComponent::TestFootstep() {
  UE_LOG(LogPRAudio, Warning, TEXT("[PRFoley] TestFootstep Command Executed!"));
  FName SocketToUse = NAME_None;
  if (FootstepData && FootstepData->FootSockets.Num() > 0) {
    SocketToUse = FootstepData->FootSockets[0];
  }
  TriggerFootstep(SocketToUse);
}

void UPRFoleyComponent::TestJump() {
  UE_LOG(LogPRAudio, Warning, TEXT("[PRFoley] TestJump Command Executed!"));
  HandleJump();
}

void UPRFoleyComponent::TestLand() {
  UE_LOG(LogPRAudio, Warning, TEXT("[PRFoley] TestLand Command Executed!"));
  Landing();
}

// ============================================================================
// Tick
// ============================================================================

void UPRFoleyComponent::TickComponent(
    float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction *ThisTickFunction) {
  Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

  if (!GetOwner()) {
    return;
  }

  if (bEnableVoiceLayer || BreathingComp_A || BreathingComp_B) {
    UpdateBreathingLoop();
  }

  // Early out if Data invalid or Mode is NOT Distance
  if (!FootstepData) {
    return;
  }
  if (FootstepData->TriggerMode != EPRFootstepTriggerMode::Distance) {
    return;
  }
  if (!bEnableFootstepLayer && !bEnableVoiceLayer) {
    return;
  }

  // LOD Check (Tick Optimization)
  if (FootstepData->MaxLODDistance > 0.0f) {
    if (APlayerCameraManager *CamManager =
            UGameplayStatics::GetPlayerCameraManager(this, 0)) {
      float DistSq = FVector::DistSquared(CamManager->GetCameraLocation(),
                                          GetOwner()->GetActorLocation());
      if (DistSq > FMath::Square(FootstepData->MaxLODDistance)) {
        LastLocation = GetOwner()->GetActorLocation();
        return;
      }
    }
  }

  // Calculate distance traveled
  FVector CurrentLocation = GetOwner()->GetActorLocation();
  double Dist = FVector::Dist(CurrentLocation, LastLocation);

  if (Dist > 0.1) {
    AccumulatedDistance += Dist;
  }

  LastLocation = CurrentLocation;

  // Check if distance interval reached
  if (InstanceDistanceInterval > 0.0f &&
      AccumulatedDistance >= InstanceDistanceInterval) {
    AccumulatedDistance -= InstanceDistanceInterval;

    if (FootstepData->FootSockets.Num() > 0) {
      CurrentFootIndex =
          (CurrentFootIndex + 1) % FootstepData->FootSockets.Num();
      FName NextSocket = FootstepData->FootSockets[CurrentFootIndex];
      TriggerFootstep(NextSocket);
    } else {
      TriggerFootstep(NAME_None);
    }
  }
}

// ============================================================================
// Cache
// ============================================================================

void UPRFoleyComponent::CacheOwnerMesh() {
  AActor *Owner = GetOwner();
  if (Owner) {
    OwnerMesh = Owner->FindComponentByClass<USkeletalMeshComponent>();
  }
}

// ============================================================================
// Editor Auto-Assign
// ============================================================================

#if WITH_EDITOR
void UPRFoleyComponent::AutoAssignFootstepData() {
  if (FootstepData) {
    return;
  }

  FAssetRegistryModule &AssetRegistryModule =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

  FARFilter Filter;
  Filter.ClassPaths.Add(UPRFootstepData::StaticClass()->GetClassPathName());
  Filter.bRecursiveClasses = true;

  TArray<FAssetData> Assets;
  AssetRegistryModule.Get().GetAssets(Filter, Assets);

  if (Assets.Num() == 1) {
    FootstepData = Cast<UPRFootstepData>(Assets[0].GetAsset());
    if (FootstepData) {
      UE_LOG(LogPRAudio, Log, TEXT("[PRFoley] Auto-assigned FootstepData: %s"),
             *GetNameSafe(FootstepData));
    }
    return;
  }

  if (Assets.Num() > 1) {
    const FName PreferredAssetNames[] = {FName(TEXT("DA_Footstep_Default")),
                                         FName(TEXT("DA_Footstep_Config_A"))};
    for (const FName &PreferredAssetName : PreferredAssetNames) {
      const FAssetData *Preferred =
          Assets.FindByPredicate([&](const FAssetData &Asset) {
            return Asset.AssetName == PreferredAssetName;
          });

      if (Preferred) {
        FootstepData = Cast<UPRFootstepData>(Preferred->GetAsset());
        if (FootstepData) {
          UE_LOG(LogPRAudio, Log,
                 TEXT("[PRFoley] Auto-assigned default FootstepData: %s"),
                 *GetNameSafe(FootstepData));
        }
        break;
      }
    }
  }
}

void UPRFoleyComponent::AutoAssignVoiceData() {
  if (VoiceData) {
    return;
  }

  FAssetRegistryModule &AssetRegistryModule =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

  FARFilter Filter;
  Filter.ClassPaths.Add(UPRVoiceData::StaticClass()->GetClassPathName());
  Filter.bRecursiveClasses = true;

  TArray<FAssetData> Assets;
  AssetRegistryModule.Get().GetAssets(Filter, Assets);

  if (Assets.Num() == 1) {
    VoiceData = Cast<UPRVoiceData>(Assets[0].GetAsset());
    if (VoiceData) {
      UE_LOG(LogPRAudio, Log, TEXT("[PRFoley] Auto-assigned VoiceData: %s"),
             *GetNameSafe(VoiceData));
    }
    return;
  }

  if (Assets.Num() > 1) {
    const FName PreferredNames[] = {FName(TEXT("DA_Voice_Default")),
                                    FName(TEXT("DA_Voice_Config_A"))};
    for (const FName &Name : PreferredNames) {
      const FAssetData *Preferred = Assets.FindByPredicate(
          [&](const FAssetData &Asset) { return Asset.AssetName == Name; });
      if (Preferred) {
        VoiceData = Cast<UPRVoiceData>(Preferred->GetAsset());
        if (VoiceData) {
          UE_LOG(LogPRAudio, Log,
                 TEXT("[PRFoley] Auto-assigned default VoiceData: %s"),
                 *GetNameSafe(VoiceData));
        }
        break;
      }
    }
  }
}

#endif

// ============================================================================
// Public Setters / Getters
// ============================================================================

void UPRFoleyComponent::SetFootIntervalDistance(float NewInterval) {
  InstanceDistanceInterval = NewInterval;
  UE_LOG(LogPRAudio, Verbose,
         TEXT("[PRFoley] Updated Instance Distance Interval to: %f"),
         NewInterval);
}

float UPRFoleyComponent::GetFootIntervalDistance() const {
  return InstanceDistanceInterval;
}

EPRVelocityTier UPRFoleyComponent::GetCurrentVelocityTier() const {
  return GetVelocityTier();
}

EPRVelocityTier UPRFoleyComponent::GetCurrentBreathingTier() const {
  return CurrentBreathingTier;
}

FString
UPRFoleyComponent::GetSurfaceDisplayName(EPhysicalSurface Surface) const {
  const UEnum *EnumPtr = StaticEnum<EPhysicalSurface>();
  if (!EnumPtr) {
    return TEXT("Invalid");
  }
  return EnumPtr->GetDisplayNameTextByValue((int64)Surface).ToString();
}

EPhysicalSurface UPRFoleyComponent::GetLastDetectedSurface() const {
  return LastDetectedSurface;
}

float UPRFoleyComponent::GetBreathingIntensity() const {
  return CachedBreathingIntensity;
}

void UPRFoleyComponent::SetBreathingIntensityOverride(float Intensity) {
  BreathingIntensityOverride = FMath::Clamp(Intensity, 0.0f, 1.0f);
}

void UPRFoleyComponent::ClearBreathingIntensityOverride() {
  BreathingIntensityOverride = -1.0f;
}

void UPRFoleyComponent::SetFootstepData(UPRFootstepData *NewData) {
  FootstepData = NewData;
  if (FootstepData) {
    InstanceDistanceInterval = FootstepData->FootIntervalDistance;
  }
}

void UPRFoleyComponent::SetVoiceData(UPRVoiceData *NewData) {
  // Fade out current breathing before swap
  if (BreathingComp_A && BreathingComp_A->IsPlaying()) {
    BreathingComp_A->FadeOut(0.5f, 0.0f);
  }
  if (BreathingComp_B && BreathingComp_B->IsPlaying()) {
    BreathingComp_B->FadeOut(0.5f, 0.0f);
  }
  VoiceData = NewData;
}

void UPRFoleyComponent::SetSpeedOverride(float Speed) { SpeedOverride = Speed; }

void UPRFoleyComponent::SetSpeedThresholds(float Walk, float Jog,
                                           float Sprint) {
  if (VoiceData) {
    VoiceData->WalkSpeedThreshold = FMath::Max(Walk, 0.0f);
    VoiceData->JogSpeedThreshold = FMath::Max(Jog, Walk);
    VoiceData->SprintSpeedThreshold = FMath::Max(Sprint, Jog);
    UE_LOG(
        LogPRAudio, Log,
        TEXT("[PRFoley] Speed thresholds set: Walk=%.0f Jog=%.0f Sprint=%.0f"),
        VoiceData->WalkSpeedThreshold, VoiceData->JogSpeedThreshold,
        VoiceData->SprintSpeedThreshold);
  }
}

void UPRFoleyComponent::SetBreathingDrive(float NormalizedIntensity) {
  BreathingIntensityOverride = NormalizedIntensity;
}

// ============================================================================
// Core: TriggerFootstep
// ============================================================================

void UPRFoleyComponent::TriggerFootstep(FName SocketName) {
  if (!FootstepData) {
    UE_LOG(LogPRAudio, Error, TEXT("[PRFoley] FootstepData is NULL!"));
    return;
  }

  if (!bEnableFootstepLayer && !bEnableVoiceLayer && !bEnableVFXLayer &&
      !bEnableDecalLayer) {
    return;
  }

  if (!IsInLODRadius()) {
    return;
  }

  if (!bEnableFootstepLayer) {
    const FVector OwnerLocation =
        GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
    PlayFootstepSound(SurfaceType_Default, OwnerLocation);
    return;
  }

  FHitResult Hit;
  FVector StartLocation = FVector::ZeroVector;
  FVector EndLocation = FVector::ZeroVector;
  if (TraceFootstep(SocketName, Hit, StartLocation, EndLocation)) {
    LastHitNormal = Hit.ImpactNormal;
    HandleFootstep(Hit);
  } else if (bDebugTraces) {
    UE_LOG(LogPRAudio, Warning, TEXT("[PRFoley] Trace MISSED from %s to %s"),
           *StartLocation.ToString(), *EndLocation.ToString());
  }
}

// ============================================================================
// Core: Landing
// ============================================================================

void UPRFoleyComponent::Landing() {
  if (!IsInLODRadius()) {
    return;
  }

  if (!FootstepData || !bEnableFootstepLayer) {
    HandleLand(FHitResult());
    return;
  }

  float ZOffset = FootstepData->LandingTraceOffset;
  FVector Start = GetOwner()->GetActorLocation();
  Start.Z += ZOffset;
  FVector End = Start - FVector(0, 0, FootstepData->TraceLength * 2.0f);

  FHitResult Hit;
  if (PerformTrace(Start, End, Hit)) {
    LastHitNormal = Hit.ImpactNormal;
    HandleLand(Hit);
    return;
  }

  if (bDebugTraces) {
    UE_LOG(LogPRAudio, Warning, TEXT("[PRFoley] Land Trace MISSED! StartZ: %f"),
           Start.Z);
  }

  HandleLand(FHitResult());
}

// ============================================================================
// Core: HandleJump
// ============================================================================

void UPRFoleyComponent::HandleJump() {
  if (!IsInLODRadius()) {
    return;
  }

  if (bEnableVoiceLayer && VoiceData) {
    if (AActor *Owner = GetOwner()) {
      const FVector OwnerLocation = Owner->GetActorLocation();
      float Vol = PlaySoundWithSettings(VoiceData->JumpEffort, OwnerLocation,
                                        VoiceData->VoiceAudio);
      if (VoiceData->JumpEffort) {
        OnVoicePlayed.Broadcast(GetVelocityTier(), Vol, VoiceData->JumpEffort);
      }
    }
  }

  if (!bEnableFootstepLayer || !FootstepData) {
    if (!FootstepData && bEnableFootstepLayer) {
      UE_LOG(LogPRAudio, Warning,
             TEXT("[PRFoley] FootstepData is NULL. Skipping jump surface."));
    }
    return;
  }

  FName SocketToUse = NAME_None;
  if (FootstepData->FootSockets.Num() > 0) {
    SocketToUse = FootstepData->FootSockets[0];
  }

  FHitResult Hit;
  FVector StartLocation = FVector::ZeroVector;
  FVector EndLocation = FVector::ZeroVector;
  if (TraceFootstep(SocketToUse, Hit, StartLocation, EndLocation)) {
    EPhysicalSurface Surface = GetSurfaceFromHit(Hit);
    LastHitNormal = Hit.ImpactNormal;
    PlaySurfaceJump(Surface, Hit.ImpactPoint);

    // VFX for jump
    PlaySurfaceVFX(Surface, Hit.ImpactPoint, Hit.ImpactNormal,
                   EPRFoleyEventType::Jump);

    // Network broadcast
    BroadcastNetworkFoleyEvent(Surface, Hit.ImpactPoint, Hit.ImpactNormal,
                               EPRFoleyEventType::Jump, false);
  } else if (bDebugTraces) {
    UE_LOG(LogPRAudio, Warning,
           TEXT("[PRFoley] Jump Trace MISSED from %s to %s"),
           *StartLocation.ToString(), *EndLocation.ToString());
  }
}

// ============================================================================
// Core: HandleLand
// ============================================================================

void UPRFoleyComponent::HandleLand(const FHitResult &Hit) {
  if (!IsInLODRadius()) {
    return;
  }

  AActor *Owner = GetOwner();
  const float FallVelocity = Owner ? FMath::Abs(Owner->GetVelocity().Z) : 0.0f;
  const bool bHeavyLand =
      FootstepData && FallVelocity > FootstepData->HeavyLandThresholdVelocity;

  if (bEnableVoiceLayer && VoiceData) {
    if (Owner) {
      const FVector OwnerLocation = Owner->GetActorLocation();
      USoundBase *ExhaleSound = bHeavyLand && VoiceData->HeavyLandExhale
                                    ? VoiceData->HeavyLandExhale
                                    : VoiceData->LandExhale;
      float Vol = PlaySoundWithSettings(ExhaleSound, OwnerLocation,
                                        VoiceData->VoiceAudio);
      if (ExhaleSound) {
        OnVoicePlayed.Broadcast(GetVelocityTier(), Vol, ExhaleSound);
      }
    }
  }

  if (!bEnableFootstepLayer || !FootstepData || !Hit.bBlockingHit) {
    return;
  }

  const EPhysicalSurface Surface = GetSurfaceFromHit(Hit);
  const FVector HitNormal =
      Hit.ImpactNormal.IsNearlyZero() ? FVector::UpVector : Hit.ImpactNormal;
  LastHitNormal = HitNormal;

  PlaySurfaceLand(Surface, Hit.ImpactPoint);

  // VFX for land
  PlaySurfaceVFX(Surface, Hit.ImpactPoint, HitNormal, EPRFoleyEventType::Land);

  // Network broadcast
  BroadcastNetworkFoleyEvent(Surface, Hit.ImpactPoint, HitNormal,
                             EPRFoleyEventType::Land, bHeavyLand);

  if (FootstepData->bPlayFootstepAfterLandImpact) {
    const float Delay = FootstepData->FootstepDelayAfterLand;
    const FVector SurfaceLocation = Hit.ImpactPoint;
    if (Delay <= 0.0f) {
      PlaySurfaceFootstep(Surface, SurfaceLocation);
    } else if (UWorld *World = GetWorld()) {
      World->GetTimerManager().ClearTimer(LandFootstepTimerHandle);
      FTimerDelegate Delegate;
      Delegate.BindWeakLambda(this, [this, Surface, SurfaceLocation]() {
        PlaySurfaceFootstep(Surface, SurfaceLocation);
      });
      World->GetTimerManager().SetTimer(LandFootstepTimerHandle, Delegate,
                                        Delay, false);
    }
  }
}

// ============================================================================
// HandleFootstep (from trace hit)
// ============================================================================

void UPRFoleyComponent::HandleFootstep(const FHitResult &Hit) {
  if (!IsInLODRadius()) {
    return;
  }
  if (Hit.bBlockingHit) {
    EPhysicalSurface Surface = GetSurfaceFromHit(Hit);
    LastDetectedSurface = Surface;
    const FVector HitNormal =
        Hit.ImpactNormal.IsNearlyZero() ? FVector::UpVector : Hit.ImpactNormal;

    PlayFootstepSound(Surface, Hit.ImpactPoint);

    // Landscape blending: play secondary surface sound at reduced volume
    if (FootstepData && FootstepData->bEnableLandscapeBlending) {
      EPhysicalSurface SecondarySurface;
      float SecondaryWeight;
      if (GetLandscapeBlendSurface(Hit, SecondarySurface, SecondaryWeight)) {
        PlaySurfaceFootstep(SecondarySurface, Hit.ImpactPoint);
      }
    }

    // VFX for footstep
    PlaySurfaceVFX(Surface, Hit.ImpactPoint, HitNormal,
                   EPRFoleyEventType::Footstep);

    // Decal for footstep
    SpawnFootprintDecal(Surface, Hit.ImpactPoint, HitNormal);

    // Network broadcast
    BroadcastNetworkFoleyEvent(Surface, Hit.ImpactPoint, HitNormal,
                               EPRFoleyEventType::Footstep, false);
  }
}

// ============================================================================
// Trace
// ============================================================================

bool UPRFoleyComponent::PerformTrace(const FVector &Start, const FVector &End,
                                     FHitResult &OutHit) {
  FCollisionQueryParams Params;
  Params.AddIgnoredActor(GetOwner());
  Params.bReturnPhysicalMaterial = true; // Crucial

  bool bHit = false;

  EPRTraceType TraceMode =
      FootstepData ? FootstepData->TraceType : EPRTraceType::Sphere;
  float ShapeRadius = FootstepData ? FootstepData->SphereRadius : 10.0f;
  FVector BoxExtent =
      FootstepData ? FootstepData->BoxHalfExtent : FVector(10.f);
  ECollisionChannel Channel =
      FootstepData ? FootstepData->TraceChannel.GetValue() : ECC_Visibility;

  switch (TraceMode) {
  case EPRTraceType::Line:
    bHit = GetWorld()->LineTraceSingleByChannel(OutHit, Start, End, Channel,
                                                Params);
    break;

  case EPRTraceType::Sphere:
    bHit = GetWorld()->SweepSingleByChannel(
        OutHit, Start, End, FQuat::Identity, Channel,
        FCollisionShape::MakeSphere(ShapeRadius), Params);
    break;

  case EPRTraceType::Box:
    bHit = GetWorld()->SweepSingleByChannel(
        OutHit, Start, End, FQuat::Identity, Channel,
        FCollisionShape::MakeBox(BoxExtent), Params);
    break;

  case EPRTraceType::Multi:
    bHit = GetWorld()->SweepSingleByChannel(
        OutHit, Start, End, FQuat::Identity, Channel,
        FCollisionShape::MakeSphere(ShapeRadius), Params);
    if (!bHit) {
      bHit = GetWorld()->LineTraceSingleByChannel(OutHit, Start, End, Channel,
                                                  Params);
    }
    break;
  }

#if !UE_BUILD_SHIPPING
  if (bDebugTraces) {
    if (TraceMode == EPRTraceType::Line) {
      DrawDebugLine(GetWorld(), Start, End, bHit ? FColor::Green : FColor::Red,
                    false, 1.0f);
    } else if (TraceMode == EPRTraceType::Box) {
      DrawDebugBox(GetWorld(), Start,
                   FootstepData ? FootstepData->BoxHalfExtent : FVector(10.f),
                   bHit ? FColor::Green : FColor::Red, false, 1.0f);
    } else {
      float Radius = FootstepData ? FootstepData->SphereRadius : 10.0f;
      DrawDebugCylinder(GetWorld(), Start, End, Radius, 8,
                        bHit ? FColor::Green : FColor::Red, false, 1.0f);
    }

    if (bHit && OutHit.GetActor()) {
      FString PhysMatName = OutHit.PhysMaterial.IsValid()
                                ? OutHit.PhysMaterial->GetName()
                                : TEXT("None");
      EPhysicalSurface SurfType =
          OutHit.PhysMaterial.IsValid()
              ? (EPhysicalSurface)OutHit.PhysMaterial->SurfaceType
              : SurfaceType_Default;
      FString Msg = FString::Printf(
          TEXT("Hit: %s | PhysMat: %s | SurfaceType: %d"),
          *OutHit.GetActor()->GetName(), *PhysMatName, (int32)SurfType);
      if (GEngine) {
        GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Yellow, Msg);
      }

      // Landscape-specific warning
      if (!OutHit.PhysMaterial.IsValid() &&
          OutHit.GetActor()->IsA<ALandscapeProxy>()) {
        UE_LOG(LogPRAudio, Warning,
               TEXT("[PRFoley] Landscape hit but NO PhysMat returned! "
                    "Check: 1) Landscape Collision Complexity = 'Use Complex "
                    "Collision As Simple' 2) Layer Info has Physical Material "
                    "assigned 3) PhysMat has correct SurfaceType."));
        if (GEngine) {
          GEngine->AddOnScreenDebugMessage(
              -1, 3.0f, FColor::Red,
              TEXT("PR_Foley: Landscape has NO PhysMat! Check Collision "
                   "Complexity & Layer Info."));
        }
      }
    }
  }
#endif

  return bHit;
}

EPhysicalSurface UPRFoleyComponent::GetSurfaceFromHit(const FHitResult &Hit) {
  if (Hit.PhysMaterial.IsValid()) {
    return Hit.PhysMaterial->SurfaceType;
  }
  return SurfaceType_Default;
}

// ============================================================================
// PlayFootstepSound (audio + voice step effort)
// ============================================================================

void UPRFoleyComponent::PlayFootstepSound(EPhysicalSurface SurfaceType,
                                          const FVector &Location) {
  if (!bEnableFootstepLayer && !bEnableVoiceLayer) {
    return;
  }

  if (bEnableFootstepLayer && FootstepData) {
    PlaySurfaceFootstep(SurfaceType, Location);
  }

  if (bEnableVoiceLayer && VoiceData) {
    // Voice step effort logic has been removed as per user requirements.
  }
}

// ============================================================================
// LOD
// ============================================================================

bool UPRFoleyComponent::IsInLODRadius() const {
  if (!FootstepData || FootstepData->MaxLODDistance <= 0.0f) {
    return true;
  }

  if (!GetOwner()) {
    return true;
  }

  if (APlayerCameraManager *CamManager =
          UGameplayStatics::GetPlayerCameraManager(this, 0)) {
    float DistSq = FVector::DistSquared(CamManager->GetCameraLocation(),
                                        GetOwner()->GetActorLocation());
    return DistSq <= FMath::Square(FootstepData->MaxLODDistance);
  }

  return true;
}

// ============================================================================
// TraceFootstep
// ============================================================================

bool UPRFoleyComponent::TraceFootstep(FName SocketName, FHitResult &OutHit,
                                      FVector &OutStart, FVector &OutEnd) {
  if (!FootstepData || !GetOwner()) {
    return false;
  }

  OutStart = GetOwner()->GetActorLocation();

  bool bShouldUseSocket = FootstepData->bUseFootSockets;
  FName SelectedSocket = SocketName;

  if (bShouldUseSocket) {
    if (OwnerMesh && SelectedSocket != NAME_None &&
        OwnerMesh->DoesSocketExist(SelectedSocket)) {
      OutStart = OwnerMesh->GetSocketLocation(SelectedSocket);
      OutStart.Z += FootstepData->FootSocketZOffset;
    } else {
      bShouldUseSocket = false;
    }
  }

  if (!bShouldUseSocket) {
    if (FootstepData->TraceStartRef == EPRTraceStartReference::Socket) {
      FName RefSocket = FootstepData->ReferenceSocketName;
      if (OwnerMesh && RefSocket != NAME_None &&
          OwnerMesh->DoesSocketExist(RefSocket)) {
        OutStart = OwnerMesh->GetSocketLocation(RefSocket);
        OutStart.Z += 20.0f;
        UE_LOG(LogPRAudio, Verbose,
               TEXT("[PRFoley] Tracing from Reference Socket: %s"),
               *RefSocket.ToString());
      } else {
        UE_LOG(LogPRAudio, Warning,
               TEXT("[PRFoley] Reference Socket '%s' invalid. Check Data "
                    "Asset."),
               *RefSocket.ToString());
      }
    } else if (FootstepData->TraceStartRef == EPRTraceStartReference::Root) {
      if (USceneComponent *RootComp = GetOwner()->GetRootComponent()) {
        OutStart = RootComp->GetComponentLocation();
        OutStart.Z += FootstepData->CapsuleZOffset;
      }
    } else {
      OutStart = GetOwner()->GetActorLocation();
      OutStart.Z += FootstepData->CapsuleZOffset;
    }
  }

  OutEnd = OutStart - FVector(0, 0, FootstepData->TraceLength);
  return PerformTrace(OutStart, OutEnd, OutHit);
}

// ============================================================================
// Audio Helpers
// ============================================================================

float UPRFoleyComponent::PlaySoundWithSettings(
    USoundBase *SoundToPlay, const FVector &Location,
    const FPRFoleyAudioSettings &AudioSettings) {
  if (!SoundToPlay) {
    return 0.0f;
  }

  float Volume = FMath::RandRange(AudioSettings.VolumeRange.X,
                                  AudioSettings.VolumeRange.Y);
  float Pitch =
      FMath::RandRange(AudioSettings.PitchRange.X, AudioSettings.PitchRange.Y);

  if (AudioSettings.EffectsChain || AudioSettings.ConcurrencySettings) {
    if (UAudioComponent *SpawnedAudio = UGameplayStatics::SpawnSoundAtLocation(
            this, SoundToPlay, Location, FRotator::ZeroRotator, Volume, Pitch,
            0.0f, AudioSettings.AttenuationSettings,
            AudioSettings.ConcurrencySettings, false)) {
      if (AudioSettings.EffectsChain) {
        SpawnedAudio->SetSourceEffectChain(AudioSettings.EffectsChain);
      }
      SpawnedAudio->Play();
    }
  } else {
    UGameplayStatics::PlaySoundAtLocation(
        this, SoundToPlay, Location, Volume, Pitch, 0.0f,
        AudioSettings.AttenuationSettings, AudioSettings.ConcurrencySettings);
  }

  return Volume;
}

const FPRSurfaceFoleyConfig *UPRFoleyComponent::ResolveSurfaceConfig(
    const TArray<FPRSurfaceFoleyConfig> &ConfigArray,
    EPhysicalSurface SurfaceType, bool &bOutDefault) const {
  bOutDefault = false;

  for (const FPRSurfaceFoleyConfig &Entry : ConfigArray) {
    if (Entry.Surface == SurfaceType) {
      return &Entry;
    }
  }

  for (const FPRSurfaceFoleyConfig &Entry : ConfigArray) {
    if (Entry.Surface == EPhysicalSurface::SurfaceType_Default) {
      bOutDefault = true;
      return &Entry;
    }
  }

  return nullptr;
}

USoundBase *
UPRFoleyComponent::SelectSoundFromSet(const FPRSurfaceSoundSet &SoundSet,
                                      int32 &LastIndex) const {
  const int32 NumSounds = SoundSet.Sounds.Num();
  if (NumSounds == 0) {
    return nullptr;
  }

  int32 NewIndex = 0;
  if (NumSounds == 1) {
    NewIndex = 0;
  } else if (SoundSet.bShuffleNoRepeat) {
    NewIndex = FMath::RandHelper(NumSounds);
    if (NewIndex == LastIndex) {
      NewIndex = (NewIndex + 1) % NumSounds;
    }
  } else {
    NewIndex = FMath::RandHelper(NumSounds);
  }

  LastIndex = NewIndex;
  return SoundSet.Sounds[NewIndex];
}

// ============================================================================
// Velocity Tier
// ============================================================================

EPRVelocityTier UPRFoleyComponent::GetVelocityTier() const {
  if (!VoiceData || !GetOwner()) {
    return EPRVelocityTier::Idle;
  }

  const float Speed2D = (SpeedOverride >= 0.0f)
                            ? SpeedOverride
                            : GetOwner()->GetVelocity().Size2D();
  const float H = VoiceData->TierHysteresis;

  EPRVelocityTier Target = EPRVelocityTier::Idle;
  if (Speed2D >=
      VoiceData->SprintSpeedThreshold -
          (CurrentBreathingTier >= EPRVelocityTier::Sprint ? H : -H)) {
    Target = EPRVelocityTier::Sprint;
  } else if (Speed2D >=
             VoiceData->JogSpeedThreshold -
                 (CurrentBreathingTier >= EPRVelocityTier::Jog ? H : -H)) {
    Target = EPRVelocityTier::Jog;
  } else if (Speed2D >=
             VoiceData->WalkSpeedThreshold -
                 (CurrentBreathingTier >= EPRVelocityTier::Walk ? H : -H)) {
    Target = EPRVelocityTier::Walk;
  }

  return Target;
}

USoundBase *
UPRFoleyComponent::ResolveBreathingLoopSound(EPRVelocityTier Tier) const {
  if (!VoiceData) {
    return nullptr;
  }

  if (const TObjectPtr<USoundBase> *Found =
          VoiceData->BreathingLoops.Find(Tier)) {
    return Found->Get();
  }

  return nullptr;
}

// ============================================================================
// Breathing
// ============================================================================

void UPRFoleyComponent::UpdateBreathingLoop() {
  if (!GetOwner()) {
    return;
  }

  if (!bEnableVoiceLayer || !VoiceData) {
    if (BreathingComp_A)
      BreathingComp_A->FadeOut(0.5f, 0.0f);
    if (BreathingComp_B)
      BreathingComp_B->FadeOut(0.5f, 0.0f);
    return;
  }

  if (VoiceData->BreathingMetaSound) {
    UpdateBreathingMetaSound();
    return;
  }

  if (VoiceData->BreathingLoops.Num() == 0) {
    if (BreathingComp_A)
      BreathingComp_A->FadeOut(0.5f, 0.0f);
    if (BreathingComp_B)
      BreathingComp_B->FadeOut(0.5f, 0.0f);
    return;
  }

  const EPRVelocityTier NewTier = GetVelocityTier();

  if (NewTier != CurrentBreathingTier) {
    CurrentBreathingTier = NewTier;
    OnBreathingTierChanged.Broadcast(CurrentBreathingTier);
  }

  USoundBase *TargetSound = ResolveBreathingLoopSound(NewTier);
  if (!TargetSound) {
    if (BreathingComp_A)
      BreathingComp_A->FadeOut(VoiceData->BreathingFadeTime, 0.0f);
    if (BreathingComp_B)
      BreathingComp_B->FadeOut(VoiceData->BreathingFadeTime, 0.0f);
    return;
  }

  UAudioComponent *ActiveComp =
      bBreathingPingPong ? BreathingComp_B : BreathingComp_A;

  if (ActiveComp && ActiveComp->IsPlaying() &&
      ActiveComp->GetSound() == TargetSound) {
    return;
  }

  bBreathingPingPong = !bBreathingPingPong;

  TObjectPtr<UAudioComponent> &NewComp =
      bBreathingPingPong ? BreathingComp_B : BreathingComp_A;
  TObjectPtr<UAudioComponent> &OldComp =
      bBreathingPingPong ? BreathingComp_A : BreathingComp_B;

  if (OldComp && OldComp->IsPlaying()) {
    OldComp->FadeOut(VoiceData->BreathingFadeTime, 0.0f);
  }

  const float Volume = FMath::RandRange(VoiceData->VoiceAudio.VolumeRange.X,
                                        VoiceData->VoiceAudio.VolumeRange.Y);
  const float Pitch = FMath::RandRange(VoiceData->VoiceAudio.PitchRange.X,
                                       VoiceData->VoiceAudio.PitchRange.Y);

  USceneComponent *AttachComp =
      OwnerMesh ? OwnerMesh.Get()
                : (GetOwner() ? GetOwner()->GetRootComponent() : nullptr);

  if (!NewComp) {
    if (AttachComp) {
      NewComp = UGameplayStatics::SpawnSoundAttached(
          TargetSound, AttachComp, NAME_None, FVector::ZeroVector,
          FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset, false,
          Volume, Pitch, 0.0f, VoiceData->VoiceAudio.AttenuationSettings,
          nullptr, false);
    }
  } else {
    NewComp->Stop();
    NewComp->SetSound(TargetSound);
    NewComp->SetVolumeMultiplier(Volume);
    NewComp->SetPitchMultiplier(Pitch);
    NewComp->Play();
  }

  if (NewComp) {
    if (VoiceData->VoiceAudio.EffectsChain) {
      NewComp->SetSourceEffectChain(VoiceData->VoiceAudio.EffectsChain);
    }
    NewComp->FadeIn(VoiceData->BreathingFadeTime, Volume);
  }
}

void UPRFoleyComponent::UpdateBreathingMetaSound() {
  if (!GetOwner()) {
    UE_LOG(LogPRAudio, Verbose,
           TEXT("[PRFoley] UpdateBreathingMetaSound failed: No Owner"));
    return;
  }
  if (!VoiceData) {
    UE_LOG(LogPRAudio, Verbose,
           TEXT("[PRFoley] UpdateBreathingMetaSound failed: No VoiceData"));
    return;
  }
  if (!VoiceData->BreathingMetaSound) {
    UE_LOG(LogPRAudio, Verbose,
           TEXT("[PRFoley] UpdateBreathingMetaSound failed: No "
                "BreathingMetaSound assigned in VoiceData"));
    return;
  }

  const float Speed = (SpeedOverride >= 0.0f)
                          ? SpeedOverride
                          : GetOwner()->GetVelocity().Size2D();
  const float MaxSpeed = FMath::Max(VoiceData->SprintSpeedThreshold, 1.0f);
  const float WalkThreshold = VoiceData->WalkSpeedThreshold;
  const float SprintThreshold = VoiceData->SprintSpeedThreshold;
  const float DeltaTime = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.016f;

  // ---- 1. Intensity (master, 0-1) ----
  float Intensity;
  if (BreathingIntensityOverride >= 0.0f) {
    Intensity = BreathingIntensityOverride;
  } else {
    Intensity = FMath::GetMappedRangeValueClamped(FVector2D(0.f, MaxSpeed),
                                                  FVector2D(0.f, 1.f), Speed);
  }
  CachedBreathingIntensity = Intensity;

  // ---- 2. BreathRate (0-1, maps to ~0.3-2.5 Hz in MetaSound) ----
  float BreathRate = FMath::GetMappedRangeValueClamped(
      FVector2D(0.f, MaxSpeed), FVector2D(0.f, 1.f), Speed);

  // ---- 3. EffortLevel (0-1, ramps faster once above walk speed) ----
  float EffortLevel = 0.0f;
  if (Speed > WalkThreshold) {
    EffortLevel = FMath::GetMappedRangeValueClamped(
        FVector2D(WalkThreshold, SprintThreshold), FVector2D(0.f, 1.f), Speed);
    // Apply curve to make effort ramp up faster at high speeds
    EffortLevel = FMath::Pow(EffortLevel, 0.7f);
  }

  // ---- 4. RecoveryPhase (0-1, activates on sprint→idle/walk transition) ----
  const EPRVelocityTier CurrentTier = GetVelocityTier();

  // Detect sprint→idle/walk drop to trigger recovery
  if (PreviousVelocityTier >= EPRVelocityTier::Sprint &&
      CurrentTier <= EPRVelocityTier::Walk) {
    // Trigger recovery phase
    if (VoiceData->RecoveryDuration > 0.0f) {
      RecoveryTimeRemaining = VoiceData->RecoveryDuration;
      RecoveryPhaseValue = 1.0f;
    }
  }

  // Fade recovery over time
  if (RecoveryTimeRemaining > 0.0f) {
    RecoveryTimeRemaining -= DeltaTime;
    if (RecoveryTimeRemaining <= 0.0f) {
      RecoveryTimeRemaining = 0.0f;
      RecoveryPhaseValue = 0.0f;
    } else {
      RecoveryPhaseValue = FMath::Clamp(
          RecoveryTimeRemaining / VoiceData->RecoveryDuration, 0.0f, 1.0f);
    }
  }

  // If sprinting again, cancel recovery
  if (CurrentTier >= EPRVelocityTier::Jog) {
    RecoveryTimeRemaining = 0.0f;
    RecoveryPhaseValue = 0.0f;
  }

  // Recovery boosts breath rate and intensity while active
  float FinalBreathRate = FMath::Max(BreathRate, RecoveryPhaseValue * 0.8f);
  float FinalIntensity = FMath::Max(Intensity, RecoveryPhaseValue * 0.6f);

  PreviousVelocityTier = CurrentTier;

  // ---- Spawn MetaSound component if needed ----
  if (!BreathingComp_A || !BreathingComp_A->IsValidLowLevel()) {
    USceneComponent *AttachComp =
        OwnerMesh ? OwnerMesh.Get()
                  : (GetOwner() ? GetOwner()->GetRootComponent() : nullptr);
    if (!AttachComp) {
      return;
    }

    BreathingComp_A = UGameplayStatics::SpawnSoundAttached(
        VoiceData->BreathingMetaSound, AttachComp, NAME_None,
        FVector::ZeroVector, FRotator::ZeroRotator,
        EAttachLocation::KeepRelativeOffset, true, 1.0f, 1.0f, 0.0f,
        VoiceData->VoiceAudio.AttenuationSettings, nullptr, false);

    if (BreathingComp_A) {
      BreathingComp_A->Play();
      UE_LOG(LogPRAudio, Log, TEXT("[PRFoley] MetaSound breathing started: %s"),
             *GetNameSafe(VoiceData->BreathingMetaSound));
    } else {
      UE_LOG(
          LogPRAudio, Error,
          TEXT("[PRFoley] Failed to spawn BreathingComp_A with MetaSound: %s"),
          *GetNameSafe(VoiceData->BreathingMetaSound));
    }
  }

  // ---- Send parameters (throttled) ----
  if (BreathingComp_A && BreathingComp_A->IsPlaying()) {
    const float Threshold = 0.01f;

    // Intensity (always sent)
    if (FMath::Abs(FinalIntensity - CachedBreathingIntensity) > Threshold) {
      BreathingComp_A->SetFloatParameter(VoiceData->IntensityParamName,
                                         FinalIntensity);
      CachedBreathingIntensity = FinalIntensity;
    }

    // BreathRate (optional param)
    if (VoiceData->BreathRateParamName != NAME_None) {
      if (FMath::Abs(FinalBreathRate - CachedBreathRate) > Threshold) {
        BreathingComp_A->SetFloatParameter(VoiceData->BreathRateParamName,
                                           FinalBreathRate);
        CachedBreathRate = FinalBreathRate;
      }
    }

    // EffortLevel (optional param)
    if (VoiceData->EffortLevelParamName != NAME_None) {
      if (FMath::Abs(EffortLevel - CachedEffortLevel) > Threshold) {
        BreathingComp_A->SetFloatParameter(VoiceData->EffortLevelParamName,
                                           EffortLevel);
        CachedEffortLevel = EffortLevel;
      }
    }

    // RecoveryPhase (optional param)
    if (VoiceData->RecoveryPhaseParamName != NAME_None) {
      if (FMath::Abs(RecoveryPhaseValue - CachedRecoveryPhase) > Threshold) {
        BreathingComp_A->SetFloatParameter(VoiceData->RecoveryPhaseParamName,
                                           RecoveryPhaseValue);
        CachedRecoveryPhase = RecoveryPhaseValue;
      }
    }
  }

  // ---- Tier changed event ----
  if (CurrentTier != CurrentBreathingTier) {
    CurrentBreathingTier = CurrentTier;
    OnBreathingTierChanged.Broadcast(CurrentBreathingTier);
  }
}

// ============================================================================
// Surface Sound Playback
// ============================================================================

void UPRFoleyComponent::PlaySurfaceFootstep(EPhysicalSurface SurfaceType,
                                            const FVector &SurfaceLocation) {
  if (!FootstepData || !bEnableFootstepLayer) {
    return;
  }

  bool bDefault = false;
  const FPRSurfaceFoleyConfig *Config =
      ResolveSurfaceConfig(FootstepData->Surfaces, SurfaceType, bDefault);
  if (!Config) {
    return;
  }

  const EPhysicalSurface Key = bDefault ? SurfaceType_Default : SurfaceType;
  int32 &LastIndex = LastSurfaceFootstepIndices.FindOrAdd(Key, INDEX_NONE);
  USoundBase *SurfaceSound = SelectSoundFromSet(Config->Footstep, LastIndex);

  FPRFoleyAudioSettings AdjustedSettings = FootstepData->SurfaceAudio;
  AdjustedSettings.VolumeRange *= Config->VolumeMultiplier;
  AdjustedSettings.PitchRange *= Config->PitchMultiplier;

  float Vol =
      PlaySoundWithSettings(SurfaceSound, SurfaceLocation, AdjustedSettings);
  if (SurfaceSound) {
    OnFootstepPlayed.Broadcast(SurfaceType, SurfaceLocation, Vol, SurfaceSound);
  }
}

void UPRFoleyComponent::PlaySurfaceJump(EPhysicalSurface SurfaceType,
                                        const FVector &SurfaceLocation) {
  if (!FootstepData || !bEnableFootstepLayer) {
    return;
  }

  bool bDefaultJump = false;
  const FPRSurfaceFoleyConfig *Config =
      ResolveSurfaceConfig(FootstepData->Surfaces, SurfaceType, bDefaultJump);

  const FPRSurfaceSoundSet *SoundSetToUse = &FootstepData->GlobalJumpLaunch;
  EPhysicalSurface IndexKey = SurfaceType_Default;

  if (Config && Config->bOverrideJump) {
    SoundSetToUse = &Config->JumpLaunch;
    IndexKey = bDefaultJump ? SurfaceType_Default : SurfaceType;
  }

  if (SoundSetToUse) {
    int32 &LastIndex = LastSurfaceJumpIndices.FindOrAdd(IndexKey, INDEX_NONE);
    USoundBase *JumpSound = SelectSoundFromSet(*SoundSetToUse, LastIndex);

    FPRFoleyAudioSettings AdjustedSettings = FootstepData->SurfaceAudio;
    if (Config) {
      AdjustedSettings.VolumeRange *= Config->VolumeMultiplier;
      AdjustedSettings.PitchRange *= Config->PitchMultiplier;
    }

    float Vol =
        PlaySoundWithSettings(JumpSound, SurfaceLocation, AdjustedSettings);
    if (JumpSound) {
      OnFootstepPlayed.Broadcast(SurfaceType, SurfaceLocation, Vol, JumpSound);
    }
  }

  PlaySurfaceFootstep(SurfaceType, SurfaceLocation);
}

void UPRFoleyComponent::PlaySurfaceLand(EPhysicalSurface SurfaceType,
                                        const FVector &SurfaceLocation) {
  if (!FootstepData || !bEnableFootstepLayer) {
    return;
  }

  bool bDefaultLand = false;
  const FPRSurfaceFoleyConfig *Config =
      ResolveSurfaceConfig(FootstepData->Surfaces, SurfaceType, bDefaultLand);

  const FPRSurfaceSoundSet *SoundSetToUse = &FootstepData->GlobalLandImpact;
  EPhysicalSurface IndexKey = SurfaceType_Default;

  if (Config && Config->bOverrideLand) {
    SoundSetToUse = &Config->LandImpact;
    IndexKey = bDefaultLand ? SurfaceType_Default : SurfaceType;
  }

  if (SoundSetToUse) {
    int32 &LastIndex = LastSurfaceLandIndices.FindOrAdd(IndexKey, INDEX_NONE);
    USoundBase *LandSound = SelectSoundFromSet(*SoundSetToUse, LastIndex);

    FPRFoleyAudioSettings AdjustedSettings = FootstepData->SurfaceAudio;
    if (Config) {
      AdjustedSettings.VolumeRange *= Config->VolumeMultiplier;
      AdjustedSettings.PitchRange *= Config->PitchMultiplier;
    }

    float Vol =
        PlaySoundWithSettings(LandSound, SurfaceLocation, AdjustedSettings);
    if (LandSound) {
      OnFootstepPlayed.Broadcast(SurfaceType, SurfaceLocation, Vol, LandSound);
    }
  }
}

// ============================================================================
// VFX Layer (Niagara)
// ============================================================================

const FPRSurfaceVFXSet *
UPRFoleyComponent::ResolveVFXSet(EPhysicalSurface SurfaceType) const {
  if (!FootstepData) {
    return nullptr;
  }

  // 1. Look for per-surface VFX
  bool bDefault = false;
  const FPRSurfaceFoleyConfig *Config =
      ResolveSurfaceConfig(FootstepData->Surfaces, SurfaceType, bDefault);

  if (Config) {
    const FPRSurfaceVFXSet &SurfVFX = Config->VFX;
    // If per-surface has at least one system configured, use it
    if (SurfVFX.FootstepVFX || SurfVFX.JumpVFX || SurfVFX.LandVFX) {
      return &SurfVFX;
    }
  }

  // 2. Fallback to global DefaultVFX
  const FPRSurfaceVFXSet &Def = FootstepData->DefaultVFX;
  if (Def.FootstepVFX || Def.JumpVFX || Def.LandVFX) {
    return &Def;
  }

  return nullptr;
}

float UPRFoleyComponent::ComputeVFXScale(const FPRSurfaceVFXSet *VFXSet,
                                         EPRFoleyEventType EventType) const {
  if (!FootstepData || !VFXSet) {
    return 1.0f;
  }

  // Surface multiplier
  float Scale = VFXSet->ScaleMultiplier;

  // Velocity tier multiplier
  const EPRVelocityTier Tier = GetVelocityTier();
  switch (Tier) {
  case EPRVelocityTier::Idle:
    Scale *= FootstepData->VFXScaleWalk * 0.5f; // Idle = half of walk
    break;
  case EPRVelocityTier::Walk:
    Scale *= FootstepData->VFXScaleWalk;
    break;
  case EPRVelocityTier::Jog:
    Scale *= FootstepData->VFXScaleJog;
    break;
  case EPRVelocityTier::Sprint:
    Scale *= FootstepData->VFXScaleSprint;
    break;
  }

  // Land extra multiplier
  if (EventType == EPRFoleyEventType::Land) {
    Scale *= FootstepData->VFXScaleLandMultiplier;
  }

  return Scale;
}

void UPRFoleyComponent::SpawnVFX(UNiagaraSystem *System,
                                 const FVector &Location, const FVector &Normal,
                                 float Scale, EPhysicalSurface SurfaceType) {
  if (!System || !GetWorld()) {
    return;
  }

  // Build rotation from surface normal (particles spawn "up" from surface)
  const FRotator Rotation = Normal.Rotation();

  UNiagaraComponent *NiagaraComp =
      UNiagaraFunctionLibrary::SpawnSystemAtLocation(
          GetWorld(), System, Location, Rotation, FVector(Scale), true, true,
          ENCPoolMethod::AutoRelease, true);

  if (NiagaraComp) {
    OnVFXSpawned.Broadcast(SurfaceType, Location, System);
  }
}

void UPRFoleyComponent::PlaySurfaceVFX(EPhysicalSurface SurfaceType,
                                       const FVector &Location,
                                       const FVector &Normal,
                                       EPRFoleyEventType EventType) {
  if (!bEnableVFXLayer || !FootstepData) {
    return;
  }

  const FPRSurfaceVFXSet *VFXSet = ResolveVFXSet(SurfaceType);
  if (!VFXSet) {
    return;
  }

  UNiagaraSystem *SystemToSpawn = nullptr;
  switch (EventType) {
  case EPRFoleyEventType::Footstep:
    SystemToSpawn = VFXSet->FootstepVFX;
    break;
  case EPRFoleyEventType::Jump:
    SystemToSpawn = VFXSet->JumpVFX;
    break;
  case EPRFoleyEventType::Land:
    SystemToSpawn = VFXSet->LandVFX;
    break;
  }

  if (!SystemToSpawn) {
    return;
  }

  const float Scale = ComputeVFXScale(VFXSet, EventType);
  SpawnVFX(SystemToSpawn, Location, Normal, Scale, SurfaceType);
}

// ============================================================================
// Decal Layer (Footprints)
// ============================================================================

const FPRSurfaceDecalSet *
UPRFoleyComponent::ResolveDecalSet(EPhysicalSurface SurfaceType) const {
  if (!FootstepData) {
    return nullptr;
  }

  // 1. Per-surface decal
  bool bDefault = false;
  const FPRSurfaceFoleyConfig *Config =
      ResolveSurfaceConfig(FootstepData->Surfaces, SurfaceType, bDefault);

  if (Config && Config->Decal.DecalMaterial) {
    return &Config->Decal;
  }

  // 2. Global fallback
  if (FootstepData->DefaultDecal.DecalMaterial) {
    return &FootstepData->DefaultDecal;
  }

  return nullptr;
}

void UPRFoleyComponent::SpawnFootprintDecal(EPhysicalSurface SurfaceType,
                                            const FVector &Location,
                                            const FVector &Normal) {
  if (!bEnableDecalLayer || !FootstepData) {
    return;
  }

  // Include DrawDebugHelpers.h at the top of the file ideally but it's likely
  // already there or implicit in this context. If not, we'll keep the debug
  // trace within the #if WITH_EDITOR or just rely on Engine.h

  const FPRSurfaceDecalSet *DecalSet = ResolveDecalSet(SurfaceType);
  if (!DecalSet || !DecalSet->DecalMaterial) {
    return;
  }

  AActor *Owner = GetOwner();
  if (!Owner || !GetWorld()) {
    return;
  }

  // Compute rotation: Decals project along their local X axis.
  // We want the X axis to point opposite to the surface normal (into the
  // ground).
  FVector ProjectionDirection = -Normal;

  // Choose a forward vector: oriented to movement direction if moving,
  // otherwise fallback to the actor's forward vector.
  FVector ForwardDir = Owner->GetActorForwardVector();
  const FVector Velocity = Owner->GetVelocity();
  if (!Velocity.IsNearlyZero(10.0f)) {
    ForwardDir = Velocity.GetSafeNormal();
  }

  // Construct a rotation matrix where X points into the surface, and Z points
  // 'forward' Or more standard for Unreal decals: X is projection dir, Y and Z
  // define the image plane. If we want the decal top to point in ForwardDir, Z
  // should align with ForwardDir.
  FRotator DecalRotation =
      FRotationMatrix::MakeFromXZ(ProjectionDirection, ForwardDir).Rotator();

  // Unreal Decal Extent constructor is (Z, Y, X) mapping to (Depth, Width,
  // Height) in local space Z is the projection distance. X and Y are the quad
  // size.
  const FVector DecalExtent(10.0f, DecalSet->DecalSize.X * 0.5f,
                            DecalSet->DecalSize.Y * 0.5f);

  // Calculate final spawn location
  FVector SpawnLocation = Location;

  // 1. Velocity-based offset to counteract visual latency when sprinting.
  if (!Velocity.IsNearlyZero(50.0f)) {
    SpawnLocation += Velocity * 0.033f;
  }

  // 2. DataAsset visual offset (rotated by the decal's intended orientation)
  if (!DecalSet->DecalOffset.IsNearlyZero()) {
    SpawnLocation += DecalRotation.RotateVector(DecalSet->DecalOffset);
  }

  UDecalComponent *Decal = UGameplayStatics::SpawnDecalAtLocation(
      this, DecalSet->DecalMaterial, DecalExtent, SpawnLocation, DecalRotation,
      DecalSet->LifeSpan);

  if (bDebugTraces) {
    DrawDebugBox(GetWorld(), SpawnLocation, DecalExtent,
                 DecalRotation.Quaternion(), FColor::Cyan, false, 2.0f);
    DrawDebugLine(GetWorld(), SpawnLocation,
                  SpawnLocation + ProjectionDirection * 20.0f, FColor::Magenta,
                  false, 2.0f, 0, 1.0f);
  }

  if (Decal) {
    // Hardcode FadeScreenSize to a tiny value to bypass aggressive engine
    // culling which causes decals to be invisible if not set perfectly in the
    // Data Asset or Project Settings.
    Decal->SetFadeScreenSize(0.001f);

    // Support Flipbook Texture Atlases
    if (!DecalSet->FrameIndexParamName.IsNone()) {
      const TArray<int32> &TargetFrames = bIsRightFootprint
                                              ? DecalSet->RightFootFrames
                                              : DecalSet->LeftFootFrames;
      if (TargetFrames.Num() > 0) {
        int32 RandomIndex = FMath::RandRange(0, TargetFrames.Num() - 1);
        int32 SelectedFrame = TargetFrames[RandomIndex];

        UMaterialInstanceDynamic *MID = Decal->CreateDynamicMaterialInstance();
        if (MID) {
          MID->SetScalarParameterValue(DecalSet->FrameIndexParamName,
                                       static_cast<float>(SelectedFrame));
        }
      }
    }

    // Toggle foot side for next step
    bIsRightFootprint = !bIsRightFootprint;

    // Pool management: enforce MaxActiveDecals
    ActiveDecals.Add(Decal);

    // Clean up expired weak pointers
    ActiveDecals.RemoveAll([](const TWeakObjectPtr<UDecalComponent> &Ptr) {
      return !Ptr.IsValid();
    });

    // Remove oldest if over limit
    while (ActiveDecals.Num() > FootstepData->MaxActiveDecals) {
      if (ActiveDecals[0].IsValid()) {
        ActiveDecals[0]->DestroyComponent();
      }
      ActiveDecals.RemoveAt(0);
    }

    OnDecalSpawned.Broadcast(SurfaceType, SpawnLocation);
  }
}

// ============================================================================
// Landscape Blending
// ============================================================================

bool UPRFoleyComponent::GetLandscapeBlendSurface(
    const FHitResult &Hit, EPhysicalSurface &OutSecondarySurface,
    float &OutSecondaryWeight) const {
  if (!FootstepData || !FootstepData->bEnableLandscapeBlending) {
    return false;
  }

  // Only applies to Landscape actors
  ALandscapeProxy *Landscape = Cast<ALandscapeProxy>(Hit.GetActor());
  if (!Landscape) {
    return false;
  }

  EPhysicalSurface PrimarySurface =
      Hit.PhysMaterial.IsValid() ? static_cast<EPhysicalSurface>(
                                       Hit.PhysMaterial->SurfaceType.GetValue())
                                 : SurfaceType_Default;

  // Multi-Trace Cluster Logic
  if (FootstepData->bEnableLandscapeMultiTrace) {
    FVector HitLocation = Hit.Location;
    FVector HitNormal =
        Hit.ImpactNormal.IsNearlyZero() ? FVector::UpVector : Hit.ImpactNormal;

    // Create a tangent basis
    FVector TangentX, TangentY;
    HitNormal.FindBestAxisVectors(TangentX, TangentY);

    const float Radius = FootstepData->LandscapeMultiTraceRadius;
    const float TraceZOffset = 50.0f; // Trace slightly above and below

    FVector Points[4] = {
        HitLocation + TangentX * Radius, HitLocation - TangentX * Radius,
        HitLocation + TangentY * Radius, HitLocation - TangentY * Radius};

    TMap<EPhysicalSurface, int32> SurfaceCounts;
    int32 TotalValidHits = 0;

    FCollisionQueryParams Params;
    Params.AddIgnoredActor(GetOwner());
    Params.bReturnPhysicalMaterial = true;
    ECollisionChannel Channel = FootstepData->TraceChannel.GetValue();

    UWorld *World = GetWorld();
    if (!World)
      return false;

    // We count the primary hit as having 2 'points' of weight at the very
    // center
    if (PrimarySurface != SurfaceType_Default) {
      SurfaceCounts.FindOrAdd(PrimarySurface, 0) += 2;
      TotalValidHits += 2;
    }

    // Cast 4 peripheral traces
    for (int i = 0; i < 4; ++i) {
      FVector Start = Points[i] + HitNormal * TraceZOffset;
      FVector End = Points[i] - HitNormal * TraceZOffset;
      FHitResult ClusterHit;
      if (World->LineTraceSingleByChannel(ClusterHit, Start, End, Channel,
                                          Params)) {
        if (ClusterHit.GetActor() == Landscape &&
            ClusterHit.PhysMaterial.IsValid()) {
          EPhysicalSurface HitSurface = static_cast<EPhysicalSurface>(
              ClusterHit.PhysMaterial->SurfaceType.GetValue());
          if (HitSurface != SurfaceType_Default) {
            SurfaceCounts.FindOrAdd(HitSurface, 0)++;
            TotalValidHits++;
          }
        }
      }
    }

    if (TotalValidHits == 0) {
      return false; // Very weird if this happens, but gracefully handle
    }

    // Elect the strongest secondary surface
    EPhysicalSurface BestSecondary = SurfaceType_Default;
    int32 MaxSecondaryCount = 0;

    for (const auto &Pair : SurfaceCounts) {
      if (Pair.Key != PrimarySurface && Pair.Value > MaxSecondaryCount) {
        MaxSecondaryCount = Pair.Value;
        BestSecondary = Pair.Key;
      }
    }

    if (BestSecondary == SurfaceType_Default || MaxSecondaryCount == 0) {
      return false; // Primary surface won 100% or no secondary hit
    }

    // Weight calculation (MaxSecondaryCount out of TotalValidHits, max is 4/6 =
    // 66% if peripheral is completely different)
    float SecondaryWeight = static_cast<float>(MaxSecondaryCount) /
                            static_cast<float>(TotalValidHits);

    if (SecondaryWeight >= FootstepData->LandscapeBlendThreshold) {
      OutSecondarySurface = BestSecondary;
      OutSecondaryWeight = SecondaryWeight;
      return true;
    }

    return false;
  }

  // Fallback to legacy layer allocation estimation
  ULandscapeHeightfieldCollisionComponent *CollisionComp =
      Cast<ULandscapeHeightfieldCollisionComponent>(Hit.GetComponent());
  if (!CollisionComp) {
    return false;
  }

  ULandscapeComponent *LandComp = CollisionComp->GetRenderComponent();
  if (!LandComp) {
    return false;
  }

  const TArray<FWeightmapLayerAllocationInfo> &Allocations =
      LandComp->GetWeightmapLayerAllocations();
  if (Allocations.Num() < 2) {
    return false;
  }

  struct FLayerWeight {
    EPhysicalSurface Surface;
    float Weight;
  };

  TArray<FLayerWeight> LayerWeights;
  for (const FWeightmapLayerAllocationInfo &Alloc : Allocations) {
    if (Alloc.LayerInfo) {
      UPhysicalMaterial *PhysMat = Alloc.LayerInfo->GetPhysicalMaterial();
      if (PhysMat) {
        FLayerWeight LW;
        LW.Surface = PhysMat->SurfaceType;
        LW.Weight = 0.0f;
        LayerWeights.Add(LW);
      }
    }
  }

  if (LayerWeights.Num() < 2) {
    return false;
  }

  for (const FLayerWeight &LW : LayerWeights) {
    if (LW.Surface != PrimarySurface && LW.Surface != SurfaceType_Default) {
      OutSecondarySurface = LW.Surface;
      // In old fallback, assume weight passes threshold
      OutSecondaryWeight = FootstepData->LandscapeBlendThreshold + 0.1f;
      return true;
    }
  }

  return false;
}

// ============================================================================
// Network Replication
// ============================================================================

void UPRFoleyComponent::BroadcastNetworkFoleyEvent(EPhysicalSurface SurfaceType,
                                                   const FVector &Location,
                                                   const FVector &Normal,
                                                   EPRFoleyEventType EventType,
                                                   bool bHeavyLand) {
  if (!bEnableNetworkReplication) {
    return;
  }

  AActor *Owner = GetOwner();
  if (!Owner) {
    return;
  }

  const EPRVelocityTier Tier = GetVelocityTier();

  // If we are the server (listen server or dedicated), call multicast directly
  if (Owner->HasAuthority()) {
    Multicast_FoleyEvent(SurfaceType, Location, Normal, EventType, Tier,
                         bHeavyLand);
  } else {
    // Client: send to server which will multicast
    Server_FoleyEvent(SurfaceType, Location, Normal, EventType, Tier,
                      bHeavyLand);
  }
}

void UPRFoleyComponent::Server_FoleyEvent_Implementation(
    EPhysicalSurface SurfaceType, FVector_NetQuantize Location,
    FVector_NetQuantizeNormal Normal, EPRFoleyEventType EventType,
    EPRVelocityTier VelocityTier, bool bHeavyLand) {
  // Server received from client → broadcast to all
  Multicast_FoleyEvent(SurfaceType, Location, Normal, EventType, VelocityTier,
                       bHeavyLand);
}

void UPRFoleyComponent::Multicast_FoleyEvent_Implementation(
    EPhysicalSurface SurfaceType, FVector_NetQuantize Location,
    FVector_NetQuantizeNormal Normal, EPRFoleyEventType EventType,
    EPRVelocityTier VelocityTier, bool bHeavyLand) {
  // Skip on the local client that originated the event (already played locally)
  if (APawn *OwnerPawn = Cast<APawn>(GetOwner())) {
    if (OwnerPawn->IsLocallyControlled()) {
      return;
    }
  }

  // Remote client: play sound + VFX from DataAsset
  PlayRemoteFoleyEvent(SurfaceType, FVector(Location), FVector(Normal),
                       EventType, VelocityTier, bHeavyLand);
}

void UPRFoleyComponent::PlayRemoteFoleyEvent(EPhysicalSurface SurfaceType,
                                             const FVector &Location,
                                             const FVector &Normal,
                                             EPRFoleyEventType EventType,
                                             EPRVelocityTier VelocityTier,
                                             bool bHeavyLand) {

  if (!IsInLODRadius()) {
    return;
  }

  // --- Audio ---
  if (bEnableFootstepLayer && FootstepData) {
    switch (EventType) {
    case EPRFoleyEventType::Footstep:
      PlaySurfaceFootstep(SurfaceType, Location);
      break;
    case EPRFoleyEventType::Jump:
      PlaySurfaceJump(SurfaceType, Location);
      break;
    case EPRFoleyEventType::Land:
      PlaySurfaceLand(SurfaceType, Location);
      break;
    }
  }

  // --- Voice (land exhale on remote) ---
  if (bEnableVoiceLayer && VoiceData && EventType == EPRFoleyEventType::Land) {
    USoundBase *ExhaleSound = bHeavyLand && VoiceData->HeavyLandExhale
                                  ? VoiceData->HeavyLandExhale
                                  : VoiceData->LandExhale;
    PlaySoundWithSettings(ExhaleSound, Location, VoiceData->VoiceAudio);
  }

  // --- Voice (jump effort on remote) ---
  if (bEnableVoiceLayer && VoiceData && EventType == EPRFoleyEventType::Jump) {
    PlaySoundWithSettings(VoiceData->JumpEffort, Location,
                          VoiceData->VoiceAudio);
  }

  // --- VFX ---
  PlaySurfaceVFX(SurfaceType, Location, Normal, EventType);
}
