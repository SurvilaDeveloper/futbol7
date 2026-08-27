////AnimNotify_GKDistributionEvent.cpp

#include "AnimNotify_GKDistributionEvent.h"

#include "SoccerAICharacter.h"
#include "SoccerAIController.h"

#include "Components/SkeletalMeshComponent.h"

void UAnimNotify_GKDistributionEvent::Notify(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation
)
{
	Super::Notify(
		MeshComp,
		Animation
	);

	if (MeshComp == nullptr)
	{
		return;
	}

	ASoccerAICharacter* SoccerCharacter =
		Cast<ASoccerAICharacter>(
			MeshComp->GetOwner()
		);

	if (!IsValid(SoccerCharacter))
	{
		return;
	}

	ASoccerAIController* SoccerController =
		Cast<ASoccerAIController>(
			SoccerCharacter->GetController()
		);

	if (!IsValid(SoccerController))
	{
		return;
	}

	SoccerController->HandleGoalkeeperDistributionEvent(
		SoccerCharacter,
		EventType
	);
}

FString UAnimNotify_GKDistributionEvent::
GetNotifyName_Implementation() const
{
	switch (EventType)
	{
	case ESoccerGoalkeeperDistributionEvent::ReleaseBall:
		return TEXT("GK Release Ball");

	case ESoccerGoalkeeperDistributionEvent::PlaceBall:
		return TEXT("GK Place Ball");

	case ESoccerGoalkeeperDistributionEvent::KickBall:
		return TEXT("GK Kick Ball");

	case ESoccerGoalkeeperDistributionEvent::Finished:
		return TEXT("GK Distribution Finished");

	default:
		return TEXT("GK Distribution Event");
	}
}
