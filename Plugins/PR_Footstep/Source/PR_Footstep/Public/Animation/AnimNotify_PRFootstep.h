#pragma once

#include "Animation/AnimNotifies/AnimNotify.h"
#include "CoreMinimal.h"

#include "AnimNotify_PRFootstep.generated.h"

/**
 * AnimNotify for playing footstep sounds.
 * Calls UPRFootstepComponent::TriggerFootstep() on the owning actor.
 */
UCLASS(Blueprintable, meta = (DisplayName = "PR Footstep"))
class PR_FOOTSTEP_API UAnimNotify_PRFootstep : public UAnimNotify {
  GENERATED_BODY()

public:
  UAnimNotify_PRFootstep();

  virtual void Notify(USkeletalMeshComponent *MeshComp,
                      UAnimSequenceBase *Animation,
                      const FAnimNotifyEventReference &EventReference) override;

  virtual FString GetNotifyName_Implementation() const override;

  /** Socket name for foot position (e.g., "foot_l", "foot_r") */
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footstep")
  FName FootSocketName;
};
