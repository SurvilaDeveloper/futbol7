#include "AnimNotify_SoccerFootstep.h"

#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

void UAnimNotify_SoccerFootstep::Notify(
    USkeletalMeshComponent* MeshComp,
    UAnimSequenceBase* Animation)
{
    Super::Notify(MeshComp, Animation);

    if (!IsValid(MeshComp))
    {
        return;
    }

    AActor* Owner = MeshComp->GetOwner();
    if (!IsValid(Owner))
    {
        return;
    }

    if (USoccerFootstepAudioComponent* Audio =
        Owner->FindComponentByClass<USoccerFootstepAudioComponent>())
    {
        Audio->PlayFootstep(MeshComp, Foot);
    }
}

FString UAnimNotify_SoccerFootstep::GetNotifyName_Implementation() const
{
    return Foot == ESoccerFootstepFoot::Left
        ? TEXT("Soccer Footstep - Left")
        : TEXT("Soccer Footstep - Right");
}
