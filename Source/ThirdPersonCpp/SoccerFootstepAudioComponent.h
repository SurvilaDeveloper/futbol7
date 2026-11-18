// Shared footstep audio for human and AI players. Triggered only by animation notifies.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SoccerFootstepAudioComponent.generated.h"

class USkeletalMeshComponent;
class USoundBase;
class USoundAttenuation;

UENUM(BlueprintType)
enum class ESoccerFootstepFoot : uint8
{
    Left UMETA(DisplayName = "Left Foot"),
    Right UMETA(DisplayName = "Right Foot")
};

UCLASS(ClassGroup = (Soccer), meta = (BlueprintSpawnableComponent))
class THIRDPERSONCPP_API USoccerFootstepAudioComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    USoccerFootstepAudioComponent();

    // Called by AnimNotify_SoccerFootstep at the actual foot-ground contact.
    void PlayFootstep(USkeletalMeshComponent* MeshComp, ESoccerFootstepFoot Foot);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footsteps")
    bool bEnableFootsteps = true;

    // Import the WAV files and add them to this array in the character Blueprint.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footsteps")
    TArray<USoundBase*> GrassFootstepSounds;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footsteps", meta = (ClampMin = "0.0", ClampMax = "2.0"))
    float FootstepVolume = 0.70f;

    // Unreal uses centimeters. 300 = 3 m; 2500 = 25 m.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footsteps", meta = (ClampMin = "0.0"))
    float FullVolumeDistanceCm = 300.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footsteps", meta = (ClampMin = "1.0"))
    float MaxAudibleDistanceCm = 2500.0f;

    // Avoid footsteps during idle animation blending and negligible movement.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footsteps", meta = (ClampMin = "0.0"))
    float MinimumMovingSpeedCmPerSec = 65.0f;

    // Guard against duplicate notifies emitted by blended animations.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footsteps", meta = (ClampMin = "0.0"))
    float MinimumTimeBetweenSteps = 0.13f;

    // Names used by the UE4 mannequin. Customize for your custom skeleton.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footsteps")
    FName LeftFootBoneOrSocket = TEXT("foot_l");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footsteps")
    FName RightFootBoneOrSocket = TEXT("foot_r");

protected:
    virtual void BeginPlay() override;

private:
    UPROPERTY(Transient)
    USoundAttenuation* FootstepSpatialization = nullptr;

    float LastFootstepWorldTime = -1000.0f;
    int32 LastSoundIndex = INDEX_NONE;
};
