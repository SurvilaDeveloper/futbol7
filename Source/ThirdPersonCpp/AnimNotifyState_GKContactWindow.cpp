//AnimNotifyState_GKContactWindow.cpp

#include "AnimNotifyState_GKContactWindow.h"

#include "SoccerAICharacter.h"
#include "SoccerAIController.h"

#include "Components/SkeletalMeshComponent.h"

namespace
{
	ASoccerAICharacter* GetGoalkeeperCharacterFromMesh(
		USkeletalMeshComponent* MeshComp
	)
	{
		if (!IsValid(MeshComp))
		{
			return nullptr;
		}

		return Cast<ASoccerAICharacter>(
			MeshComp->GetOwner()
		);
	}

	ASoccerAIController* GetGoalkeeperControllerFromMesh(
		USkeletalMeshComponent* MeshComp
	)
	{
		ASoccerAICharacter* SoccerCharacter =
			GetGoalkeeperCharacterFromMesh(MeshComp);

		if (!IsValid(SoccerCharacter))
		{
			return nullptr;
		}

		return Cast<ASoccerAIController>(
			SoccerCharacter->GetController()
		);
	}
}

void UAnimNotifyState_GKContactWindow::NotifyBegin(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	float TotalDuration
)
{
	ASoccerAICharacter* SoccerCharacter =
		GetGoalkeeperCharacterFromMesh(MeshComp);

	ASoccerAIController* SoccerAIController =
		GetGoalkeeperControllerFromMesh(MeshComp);

	if (!IsValid(SoccerCharacter) || !IsValid(SoccerAIController))
	{
		return;
	}

	SoccerAIController->HandleGoalkeeperContactWindowBegin(
		SoccerCharacter
	);
}

void UAnimNotifyState_GKContactWindow::NotifyTick(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	float FrameDeltaTime
)
{
	ASoccerAICharacter* SoccerCharacter =
		GetGoalkeeperCharacterFromMesh(MeshComp);

	ASoccerAIController* SoccerAIController =
		GetGoalkeeperControllerFromMesh(MeshComp);

	if (!IsValid(SoccerCharacter) || !IsValid(SoccerAIController))
	{
		return;
	}

	SoccerAIController->HandleGoalkeeperContactWindowTick(
		SoccerCharacter
	);
}

void UAnimNotifyState_GKContactWindow::NotifyEnd(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation
)
{
	ASoccerAICharacter* SoccerCharacter =
		GetGoalkeeperCharacterFromMesh(MeshComp);

	ASoccerAIController* SoccerAIController =
		GetGoalkeeperControllerFromMesh(MeshComp);

	if (!IsValid(SoccerCharacter) || !IsValid(SoccerAIController))
	{
		return;
	}

	SoccerAIController->HandleGoalkeeperContactWindowEnd(
		SoccerCharacter
	);
}

FString UAnimNotifyState_GKContactWindow::GetNotifyName_Implementation() const
{
	return TEXT("GKContactWindow");
}
