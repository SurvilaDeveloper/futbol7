#include "SoccerPenaltyExecutionState.h"

#include "SoccerMatchManager.h"
#include "SoccerPenaltyKickRestart.h"
#include "SoccerAICharacter.h"
#include "SoccerDebugManager.h"

bool FSoccerPenaltyExecutionState::Enter(ASoccerMatchManager& Manager)
{
	FSoccerPenaltyKickRestart& Penalty = Manager.PenaltyKickRestart;
	if (!Penalty.Taker.IsValid())
	{
		return false;
	}

	Manager.MatchPlayState = ESoccerMatchPlayState::PenaltyKickTaking;
	Manager.CaptureActiveRestartAITargetLocations(true);

	ASoccerDebugManager::Message(
		&Manager,
		ESoccerDebugCategory::Restarts,
		Penalty.bUsesHumanTaker
			? TEXT("PENAL EXECUTION: apunta y remata con el humano")
			: TEXT("PENAL EXECUTION: comienza la carrera del bot"),
		FColor::Cyan
	);

	return true;
}

void FSoccerPenaltyExecutionState::Tick(ASoccerMatchManager& Manager, float DeltaTime)
{
	(void)DeltaTime;
	FSoccerPenaltyKickRestart& Penalty = Manager.PenaltyKickRestart;

	if (Penalty.bUsesHumanTaker)
	{
		return;
	}

	if (!Penalty.TakerAI.IsValid())
	{
		Manager.RequestMatchStateTransition(ESoccerMatchStateTransition::Playing);
		return;
	}

	ASoccerAICharacter* AITaker = Penalty.TakerAI.Get();
	if (FVector::Dist2D(AITaker->GetActorLocation(), Penalty.SpotLocation) <=
		FMath::Max(20.0f, Manager.PenaltyKickBallContactDistance))
	{
		Penalty.CompleteByAI(Manager);
	}
}
