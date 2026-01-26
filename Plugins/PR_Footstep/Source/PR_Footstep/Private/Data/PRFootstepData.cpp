#include "Data/PRFootstepData.h"

UPRFootstepData::UPRFootstepData() {
  // Default Values
  TraceLength = 150.0f;
  VolumeRange = FVector2D(0.8f, 1.0f);
  PitchRange = FVector2D(0.95f, 1.05f);
  DistanceInterval = 120.0f;

  // Default Biped setup
  FootSockets = {FName("foot_l"), FName("foot_r")};
  FootBones = {FName("foot_l"), FName("foot_r")};
}

FPrimaryAssetId UPRFootstepData::GetPrimaryAssetId() const {
  return FPrimaryAssetId("FootstepData", GetFName());
}
