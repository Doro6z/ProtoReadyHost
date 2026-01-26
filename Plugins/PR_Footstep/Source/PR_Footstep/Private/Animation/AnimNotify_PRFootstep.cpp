#include "Animation/AnimNotify_PRFootstep.h"
#include "Components/SkeletalMeshComponent.h"
#include "PRFootstepComponent.h"

UAnimNotify_PRFootstep::UAnimNotify_PRFootstep() {
  FootSocketName = FName("foot_l");
}

void UAnimNotify_PRFootstep::Notify(
    USkeletalMeshComponent *MeshComp, UAnimSequenceBase *Animation,
    const FAnimNotifyEventReference &EventReference) {
  Super::Notify(MeshComp, Animation, EventReference);

  if (!MeshComp) {
    return;
  }

  AActor *Owner = MeshComp->GetOwner();
  if (!Owner) {
    return;
  }

  if (UPRFootstepComponent *FootstepComp =
          Owner->FindComponentByClass<UPRFootstepComponent>()) {
    FootstepComp->TriggerFootstep(FootSocketName);
  }
}

FString UAnimNotify_PRFootstep::GetNotifyName_Implementation() const {
  return FString::Printf(TEXT("PR Footstep (%s)"), *FootSocketName.ToString());
}
