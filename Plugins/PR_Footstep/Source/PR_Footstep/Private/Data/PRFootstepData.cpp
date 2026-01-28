#include "Data/PRFootstepData.h"

UPRFootstepData::UPRFootstepData() {
  // Default Values
  TraceLength = 150.0f;
  VolumeRange = FVector2D(0.8f, 1.0f);
  PitchRange = FVector2D(0.95f, 1.05f);
  PitchRange = FVector2D(0.95f, 1.05f);
  FootIntervalDistance = 120.0f;

  // Default Biped setup
  FootSockets = {FName("foot_l"), FName("foot_r")};
  FootBones = {FName("foot_l"), FName("foot_r")};

  // Trace Defaults
  SphereRadius = 10.0f;
  BoxHalfExtent = FVector(10.0f, 10.0f, 10.0f);
  LandingTraceOffset = 0.0f;
}

FPrimaryAssetId UPRFootstepData::GetPrimaryAssetId() const {
  return FPrimaryAssetId("FootstepData", GetFName());
}
