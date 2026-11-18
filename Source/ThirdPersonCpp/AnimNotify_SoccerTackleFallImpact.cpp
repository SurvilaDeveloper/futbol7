#include "AnimNotify_SoccerTackleFallImpact.h"

#include "SoccerCharacterBase.h"
#include "Components/SkeletalMeshComponent.h"

void UAnimNotify_SoccerTackleFallImpact::Notify(
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
        Character->PlayTackleFallImpactAudioFromNotify(MeshComp);
    }
}

FString UAnimNotify_SoccerTackleFallImpact::GetNotifyName_Implementation() const
{
    return TEXT("Soccer Tackle - Victim Ground Impact");
}
