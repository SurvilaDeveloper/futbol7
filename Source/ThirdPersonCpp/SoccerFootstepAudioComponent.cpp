#include "SoccerFootstepAudioComponent.h"
#include "SoccerCharacterBase.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"
#include "Engine/World.h"

USoccerFootstepAudioComponent::USoccerFootstepAudioComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void USoccerFootstepAudioComponent::BeginPlay()
{
    Super::BeginPlay();

    // Position is spatialized in 3D, but attenuation is calculated using the
    // actual human player pawn, consistent with the current ball-kick audio.
    FootstepSpatialization = NewObject<USoundAttenuation>(
        this, TEXT("FootstepAudioSpatialization"));
    if (FootstepSpatialization != nullptr)
    {
        FootstepSpatialization->Attenuation.bSpatialize = true;
        FootstepSpatialization->Attenuation.bAttenuate = false;
    }
}

void USoccerFootstepAudioComponent::PlayFootstep(
    USkeletalMeshComponent* MeshComp,
    ESoccerFootstepFoot Foot)
{
    if (!bEnableFootsteps || !IsValid(MeshComp) || !GetWorld() ||
        !GetWorld()->IsGameWorld() || GrassFootstepSounds.Num() == 0)
    {
        return;
    }

    ACharacter* Character = Cast<ACharacter>(GetOwner());
    if (!IsValid(Character) || MeshComp->GetOwner() != Character ||
        Character->IsHidden() || Character->IsActorBeingDestroyed())
    {
        return;
    }

    // Sliding tackles and fall reactions should not emit walking/running steps,
    // even if a locomotion notify survives briefly during montage blending.
    if (const ASoccerCharacterBase* SoccerCharacter = Cast<ASoccerCharacterBase>(Character))
    {
        if (SoccerCharacter->IsTackleActive() || SoccerCharacter->IsTackleFallReactionActive())
        {
            return;
        }
    }

    // Scripted AI locomotion can move characters without CharacterMovement's
    // ground-walking state. Reject only falling / airborne actions here.
    const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
    if (Movement != nullptr && Movement->IsFalling())
    {
        return;
    }

    const float Speed = Character->GetVelocity().Size2D();
    if (Speed < FMath::Max(0.0f, MinimumMovingSpeedCmPerSec))
    {
        return;
    }

    const float Now = GetWorld()->GetTimeSeconds();
    if (Now - LastFootstepWorldTime < FMath::Max(0.0f, MinimumTimeBetweenSteps))
    {
        return;
    }

    const APawn* HumanPawn = UGameplayStatics::GetPlayerPawn(this, 0);
    if (!IsValid(HumanPawn) || !HumanPawn->IsLocallyControlled())
    {
        return;
    }

    const float Near = FMath::Max(0.0f, FullVolumeDistanceCm);
    const float Far = FMath::Max(Near + 1.0f, MaxAudibleDistanceCm);
    const float Distance = FVector::Dist2D(
        Character->GetActorLocation(), HumanPawn->GetActorLocation());
    if (Distance >= Far)
    {
        return;
    }

    // Avoid replaying exactly the same sample when several alternatives exist.
    TArray<int32> ValidIndices;
    for (int32 Index = 0; Index < GrassFootstepSounds.Num(); ++Index)
    {
        if (IsValid(GrassFootstepSounds[Index]) &&
            (GrassFootstepSounds.Num() == 1 || Index != LastSoundIndex))
        {
            ValidIndices.Add(Index);
        }
    }
    // Single sample and gaps in the array are both supported.
    if (ValidIndices.Num() == 0)
    {
        for (int32 Index = 0; Index < GrassFootstepSounds.Num(); ++Index)
        {
            if (IsValid(GrassFootstepSounds[Index]))
            {
                ValidIndices.Add(Index);
            }
        }
    }
    if (ValidIndices.Num() == 0)
    {
        return;
    }

    const int32 SelectedIndex = ValidIndices[FMath::RandRange(0, ValidIndices.Num() - 1)];
    USoundBase* Sound = GrassFootstepSounds[SelectedIndex];

    const float DistanceAlpha = FMath::Clamp((Distance - Near) / (Far - Near), 0.0f, 1.0f);
    const float DistanceGain = FMath::Pow(1.0f - DistanceAlpha, 1.8f);
    const float SpeedAlpha = FMath::Clamp((Speed - 65.0f) / 550.0f, 0.0f, 1.0f);
    const float Volume = FMath::Clamp(FootstepVolume, 0.0f, 2.0f) *
        FMath::Lerp(0.52f, 1.0f, SpeedAlpha) * DistanceGain;
    if (Volume <= 0.001f)
    {
        return;
    }

    FVector FootPosition = Character->GetActorLocation();
    const FName FootName = Foot == ESoccerFootstepFoot::Left
        ? LeftFootBoneOrSocket : RightFootBoneOrSocket;
    if (!FootName.IsNone() &&
        (MeshComp->DoesSocketExist(FootName) || MeshComp->GetBoneIndex(FootName) != INDEX_NONE))
    {
        FootPosition = MeshComp->GetSocketLocation(FootName);
    }
    else if (const UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
    {
        FootPosition.Z -= Capsule->GetScaledCapsuleHalfHeight() - 5.0f;
    }

    // PlaySoundAtLocation is fire-and-forget; no component ticking is needed.
    UGameplayStatics::PlaySoundAtLocation(
        this,
        Sound,
        FootPosition,
        FRotator::ZeroRotator,
        Volume,
        FMath::FRandRange(0.96f, 1.04f),
        0.0f,
        FootstepSpatialization
    );

    LastFootstepWorldTime = Now;
    LastSoundIndex = SelectedIndex;
}
