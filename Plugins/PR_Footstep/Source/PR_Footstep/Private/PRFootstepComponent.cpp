#include "PRFootstepComponent.h"
#include "Components/AudioComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h" // For Auto-Land
#include "Kismet/GameplayStatics.h"
#include "Modules/ModuleManager.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#endif

UPRFootstepComponent::UPRFootstepComponent() {
  PrimaryComponentTick.bCanEverTick = true;
  // Optimization: Ticking only needed for Distance Mode (future), disabled by
  // default if using AnimNotify
  PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UPRFootstepComponent::BeginPlay() {
  Super::BeginPlay();
  CacheOwnerMesh();

  // Initialize Distance Mode state
  if (GetOwner()) {
    LastLocation = GetOwner()->GetActorLocation();
  }

  if (FootstepData) {
    // Enable Tick if in Distance Mode
    if (FootstepData->TriggerMode == EPRFootstepTriggerMode::Distance) {
      SetComponentTickEnabled(true);
      // Init local interval
      InstanceDistanceInterval = FootstepData->FootIntervalDistance;
    }

    // Auto Bind to Landed
    if (FootstepData->bAutoTriggerLand) {
      if (ACharacter *OwnerChar = Cast<ACharacter>(GetOwner())) {
        OwnerChar->LandedDelegate.AddDynamic(this,
                                             &UPRFootstepComponent::OnLanded);
      }
    }
  }
}

#if WITH_EDITOR
void UPRFootstepComponent::OnComponentCreated() {
  Super::OnComponentCreated();
  AutoAssignFootstepData();
}
#endif

void UPRFootstepComponent::OnLanded(const FHitResult &Hit) { TriggerLand(); }

void UPRFootstepComponent::TestFootstep() {
  UE_LOG(LogTemp, Warning, TEXT("[PRFootstep] TestFootstep Command Executed!"));

  // Force trigger using first socket or override
  FName SocketToUse = NAME_None;
  if (FootstepData && FootstepData->FootSockets.Num() > 0) {
    SocketToUse = FootstepData->FootSockets[0];
  }

  TriggerFootstep(SocketToUse);
}

void UPRFootstepComponent::TickComponent(
    float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction *ThisTickFunction) {
  Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

  // Early out if Data invalid or Mode is NOT Distance
  if (!FootstepData || !GetOwner())
    return;
  if (FootstepData->TriggerMode != EPRFootstepTriggerMode::Distance)
    return;

  // LOD Check (Tick Optimization)
  if (FootstepData->MaxLODDistance > 0.0f) {
    if (APlayerCameraManager *CamManager =
            UGameplayStatics::GetPlayerCameraManager(this, 0)) {
      float DistSq = FVector::DistSquared(CamManager->GetCameraLocation(),
                                          GetOwner()->GetActorLocation());
      if (DistSq > FMath::Square(FootstepData->MaxLODDistance)) {
        LastLocation =
            GetOwner()->GetActorLocation(); // Keep updating location to avoid
                                            // huge jump when re-entering range
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
  // Check interval > 0 to avoid potential div/0 or infinite loop logical issues
  if (InstanceDistanceInterval > 0.0f &&
      AccumulatedDistance >= InstanceDistanceInterval) {
    // Consume distance
    AccumulatedDistance -= InstanceDistanceInterval;

    // Trigger next foot
    if (FootstepData->FootSockets.Num() > 0) {
      // Cycle index
      CurrentFootIndex =
          (CurrentFootIndex + 1) % FootstepData->FootSockets.Num();
      FName NextSocket = FootstepData->FootSockets[CurrentFootIndex];

      // Trigger!
      TriggerFootstep(NextSocket);
    } else {
      // Fallback if no sockets defined: Trace from Root center
      TriggerFootstep(NAME_None);
    }
  }
}

void UPRFootstepComponent::CacheOwnerMesh() {
  // Usually attached to a Character's mesh or root. find first compatible mesh.
  AActor *Owner = GetOwner();
  if (Owner) {
    OwnerMesh = Owner->FindComponentByClass<USkeletalMeshComponent>();
  }
}

#if WITH_EDITOR
void UPRFootstepComponent::AutoAssignFootstepData() {
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
      UE_LOG(LogTemp, Log, TEXT("[PRFootstep] Auto-assigned FootstepData: %s"),
             *GetNameSafe(FootstepData));
    }
    return;
  }

  if (Assets.Num() > 1) {
    const FName PreferredAssetName(TEXT("DA_Footstep_Config_A"));
    const FAssetData *Preferred =
        Assets.FindByPredicate([&](const FAssetData &Asset) {
          return Asset.AssetName == PreferredAssetName;
        });

    if (Preferred) {
      FootstepData = Cast<UPRFootstepData>(Preferred->GetAsset());
      if (FootstepData) {
        UE_LOG(LogTemp, Log,
               TEXT("[PRFootstep] Auto-assigned default FootstepData: %s"),
               *GetNameSafe(FootstepData));
      }
    }
  }
}
#endif

void UPRFootstepComponent::SetFootIntervalDistance(float NewInterval) {
  InstanceDistanceInterval = NewInterval;
  UE_LOG(LogTemp, Verbose,
         TEXT("[PRFootstep] Updated Instance Distance Interval to: %f"),
         NewInterval);
}

float UPRFootstepComponent::GetFootIntervalDistance() const {
  return InstanceDistanceInterval;
}

void UPRFootstepComponent::TriggerFootstep(FName SocketName) {
  if (!FootstepData) {
    UE_LOG(LogTemp, Error, TEXT("[PRFootstep] FootstepData is NULL!"));
    return;
  }

  // LOD Check
  if (FootstepData->MaxLODDistance > 0.0f) {
    if (APlayerCameraManager *CamManager =
            UGameplayStatics::GetPlayerCameraManager(this, 0)) {
      float DistSq = FVector::DistSquared(CamManager->GetCameraLocation(),
                                          GetOwner()->GetActorLocation());
      if (DistSq > FMath::Square(FootstepData->MaxLODDistance)) {
        return; // Too far, skip logic
      }
    }
  }

  // Determine Start Location
  FVector StartLocation = GetOwner()->GetActorLocation(); // Default safety

  bool bShouldUseSocket = FootstepData->bUseFootSockets;
  FName SelectedSocket = SocketName;

  // 1. Try Socket if enabled (Iterative List)
  if (bShouldUseSocket) {
    if (OwnerMesh && SelectedSocket != NAME_None &&
        OwnerMesh->DoesSocketExist(SelectedSocket)) {
      StartLocation = OwnerMesh->GetSocketLocation(SelectedSocket);
      // Small safety Z lift for sockets to avoid starting *in* the floor
      StartLocation.Z += 20.0f;
    } else {
      bShouldUseSocket = false; // Fallback to Reference
    }
  }

  // 2. Use Reference (Capsule/Root/SingleSocket) if List Mode disabled or
  // failed
  if (!bShouldUseSocket) {

    // CASE A: Single Socket Reference
    if (FootstepData->TraceStartRef == EPRTraceStartReference::Socket) {
      FName RefSocket = FootstepData->ReferenceSocketName;
      if (OwnerMesh && RefSocket != NAME_None &&
          OwnerMesh->DoesSocketExist(RefSocket)) {
        StartLocation = OwnerMesh->GetSocketLocation(RefSocket);
        StartLocation.Z += 20.0f; // Similar safety lift
        UE_LOG(LogTemp, Verbose,
               TEXT("[PRFootstep] Tracing from Reference Socket: %s"),
               *RefSocket.ToString());
      } else {
        UE_LOG(LogTemp, Warning,
               TEXT("[PRFootstep] Reference Socket '%s' invalid. Check Data "
                    "Asset."),
               *RefSocket.ToString());
      }
    }
    // CASE B: Root Component
    else if (FootstepData->TraceStartRef == EPRTraceStartReference::Root) {
      if (USceneComponent *RootComp = GetOwner()->GetRootComponent()) {
        StartLocation = RootComp->GetComponentLocation();
        StartLocation.Z += FootstepData->CapsuleZOffset;
      }
    }
    // CASE C: Capsule / Default
    else {
      // Default: Actor Location (Capsule Center) + Offset
      StartLocation = GetOwner()->GetActorLocation();
      StartLocation.Z += FootstepData->CapsuleZOffset;
    }
  }

  FVector EndLocation =
      StartLocation - FVector(0, 0, FootstepData->TraceLength);

  FHitResult Hit;
  if (PerformTrace(StartLocation, EndLocation, Hit)) {
    EPhysicalSurface Surface = GetSurfaceFromHit(Hit);
    PlayFootstepSound(Surface, Hit.Location, false);
  } else {
    // Only warn if debug is enabled to avoid spam
    if (bDebugTraces) {
      UE_LOG(LogTemp, Warning, TEXT("[PRFootstep] Trace MISSED from %s to %s"),
             *StartLocation.ToString(), *EndLocation.ToString());
    }
  }
}

void UPRFootstepComponent::TriggerLand() {
  if (!FootstepData)
    return;

  // Land trace is vertical from actor center down
  // Use LandingTraceOffset if non-zero, otherwise fallback to CapsuleZOffset
  // logic or similar. The user requested LandingTraceOffset specifically.

  float ZOffset = FootstepData->LandingTraceOffset;

  // Start logic similar to Reference trigger
  FVector Start = GetOwner()->GetActorLocation();
  Start.Z += ZOffset; // Apply specific landing offset (e.g. -50 usually)

  // Ensure we start high enough? Actually Offset should handle it.
  // User asked for "Trace Offset" in landing sound.

  FVector End = Start - FVector(0, 0,
                                FootstepData->TraceLength *
                                    2.0f); // Longer trace for landing

  FHitResult Hit;
  if (PerformTrace(Start, End, Hit)) {
    EPhysicalSurface Surface = GetSurfaceFromHit(Hit);
    PlayFootstepSound(Surface, Hit.Location, true);
  } else {
    if (bDebugTraces) {
      UE_LOG(LogTemp, Warning,
             TEXT("[PRFootstep] Land Trace MISSED! StartZ: %f"), Start.Z);
    }
  }
}

bool UPRFootstepComponent::PerformTrace(const FVector &Start,
                                        const FVector &End,
                                        FHitResult &OutHit) {
  FCollisionQueryParams Params;
  Params.AddIgnoredActor(GetOwner());
  Params.bReturnPhysicalMaterial = true; // Crucial

  bool bHit = false;

  // Select trace type based on DataAsset setting
  EPRTraceType TraceMode =
      FootstepData ? FootstepData->TraceType : EPRTraceType::Sphere;

  float ShapeRadius = FootstepData ? FootstepData->SphereRadius : 10.0f;
  FVector BoxExtent =
      FootstepData ? FootstepData->BoxHalfExtent : FVector(10.f);

  switch (TraceMode) {
  case EPRTraceType::Line:
    bHit = GetWorld()->LineTraceSingleByChannel(OutHit, Start, End,
                                                ECC_Visibility, Params);
    break;

  case EPRTraceType::Sphere:
    bHit = GetWorld()->SweepSingleByChannel(
        OutHit, Start, End, FQuat::Identity, ECC_Visibility,
        FCollisionShape::MakeSphere(ShapeRadius), Params);
    break;

  case EPRTraceType::Box:
    bHit = GetWorld()->SweepSingleByChannel(
        OutHit, Start, End, FQuat::Identity, ECC_Visibility,
        FCollisionShape::MakeBox(BoxExtent), Params);
    break;

  case EPRTraceType::Multi:
    // Multi-trace: Try sphere first, fallback to line
    bHit = GetWorld()->SweepSingleByChannel(
        OutHit, Start, End, FQuat::Identity, ECC_Visibility,
        FCollisionShape::MakeSphere(ShapeRadius), Params);
    if (!bHit) {
      bHit = GetWorld()->LineTraceSingleByChannel(OutHit, Start, End,
                                                  ECC_Visibility, Params);
    }
    break;
  }

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
  }

  return bHit;
}

EPhysicalSurface
UPRFootstepComponent::GetSurfaceFromHit(const FHitResult &Hit) {
  if (Hit.PhysMaterial.IsValid()) {
    return Hit.PhysMaterial->SurfaceType;
  }
  return SurfaceType_Default;
}

void UPRFootstepComponent::PlayFootstepSound(EPhysicalSurface SurfaceType,
                                             const FVector &Location,
                                             bool bIsHeavyLand) {
  if (!FootstepData)
    return;

  USoundBase *SoundToPlay = nullptr;

  // 1. Resolve Sound
  if (bIsHeavyLand && FootstepData->LandingSound) {
    SoundToPlay = FootstepData->LandingSound;
  } else if (FootstepData->SurfaceSounds.Contains(SurfaceType)) {
    SoundToPlay = FootstepData->SurfaceSounds[SurfaceType];
    UE_LOG(LogTemp, Log, TEXT("[PRFootstep] Surface %d mapped to Sound: %s"),
           (int32)SurfaceType, *GetNameSafe(SoundToPlay));
  } else {
    SoundToPlay = FootstepData->DefaultSound;
    UE_LOG(LogTemp, Warning,
           TEXT("[PRFootstep] Surface %d NOT mapped. Using Default: %s"),
           (int32)SurfaceType, *GetNameSafe(SoundToPlay));
  }

  if (!SoundToPlay) {
    UE_LOG(
        LogTemp, Error,
        TEXT("[PRFootstep] SoundToPlay is NULL! Check DataAsset assignments."));
    return;
  }

  // 2. Resolve Volume/Pitch
  float Volume = FMath::RandRange(FootstepData->VolumeRange.X,
                                  FootstepData->VolumeRange.Y);
  UE_LOG(LogTemp, Log, TEXT("[PRFootstep] Playing Sound at (%s) Vol: %f"),
         *Location.ToString(), Volume);
  float Pitch =
      FMath::RandRange(FootstepData->PitchRange.X, FootstepData->PitchRange.Y);

  // 3. Fire & Forget Optimization (No AudioComponent overhead)
  if (FootstepData->EffectsChain) {
    if (UAudioComponent *SpawnedAudio = UGameplayStatics::SpawnSoundAtLocation(
            this, SoundToPlay, Location, FRotator::ZeroRotator, Volume, Pitch,
            0.0f, FootstepData->AttenuationSettings, nullptr, false)) {
      SpawnedAudio->SetSourceEffectChain(FootstepData->EffectsChain);
      SpawnedAudio->Play();
    }
  } else {
    UGameplayStatics::PlaySoundAtLocation(this, SoundToPlay, Location, Volume,
                                          Pitch, 0.0f,
                                          FootstepData->AttenuationSettings);
  }
}
