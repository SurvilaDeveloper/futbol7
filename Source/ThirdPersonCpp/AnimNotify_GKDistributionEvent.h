//AnimNotify_GKDistributionEvent.h

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "SoccerTeamTypes.h"
#include "AnimNotify_GKDistributionEvent.generated.h"

UCLASS(
	meta = (
		DisplayName = "GK Distribution Event"
		)
)
class THIRDPERSONCPP_API UAnimNotify_GKDistributionEvent
	: public UAnimNotify
{
	GENERATED_BODY()

public:
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Goalkeeper Distribution"
	)
		ESoccerGoalkeeperDistributionEvent EventType =
		ESoccerGoalkeeperDistributionEvent::ReleaseBall;

	virtual void Notify(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation
	) override;

	virtual FString GetNotifyName_Implementation() const override;
};
