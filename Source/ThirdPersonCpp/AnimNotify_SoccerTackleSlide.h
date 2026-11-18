// Place this ONCE at first body/leg impact with turf in each tackle animation.
#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_SoccerTackleSlide.generated.h"

UCLASS(meta = (DisplayName = "Soccer Tackle Slide"))
class THIRDPERSONCPP_API UAnimNotify_SoccerTackleSlide : public UAnimNotify
{
    GENERATED_BODY()

public:
    virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation) override;
    virtual FString GetNotifyName_Implementation() const override;
};
