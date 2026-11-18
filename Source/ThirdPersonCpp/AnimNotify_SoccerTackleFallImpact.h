// Notify for the *victim* of a sliding tackle: mark body contact with the turf.
#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_SoccerTackleFallImpact.generated.h"

UCLASS(meta = (DisplayName = "Soccer Tackle Fall Impact"))
class THIRDPERSONCPP_API UAnimNotify_SoccerTackleFallImpact : public UAnimNotify
{
    GENERATED_BODY()

public:
    virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation) override;
    virtual FString GetNotifyName_Implementation() const override;
};
