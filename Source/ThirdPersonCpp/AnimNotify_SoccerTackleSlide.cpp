#include "AnimNotify_SoccerTackleSlide.h"

#include "SoccerCharacterBase.h"
#include "Components/SkeletalMeshComponent.h"

void UAnimNotify_SoccerTackleSlide::Notify(
    USkeletalMeshComponent* MeshComp,
    UAnimSequenceBase* Animation)
{
    Super::Notify(MeshComp, Animation);

    if (!IsValid(MeshComp))
    {
        return;
    }

    if (ASoccerCharacterBase* Character =
        Cast<ASoccerCharacterBase>(MeshComp->GetOwner()))
    {
        Character->PlayTackleSlideAudioFromNotify(MeshComp);
    }
}

FString UAnimNotify_SoccerTackleSlide::GetNotifyName_Implementation() const
{
    return TEXT("Soccer Tackle - Slide Contact");
}
