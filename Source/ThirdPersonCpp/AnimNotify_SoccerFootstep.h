// Place this notify on each actual foot contact in locomotion animations.
#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "SoccerFootstepAudioComponent.h"
#include "AnimNotify_SoccerFootstep.generated.h"

UCLASS(meta = (DisplayName = "Soccer Footstep"))
class THIRDPERSONCPP_API UAnimNotify_SoccerFootstep : public UAnimNotify
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soccer|Audio|Footsteps")
    ESoccerFootstepFoot Foot = ESoccerFootstepFoot::Left;

    virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation) override;
    virtual FString GetNotifyName_Implementation() const override;
};
