#include "SoccerOffsidePreparationState.h"

#include "SoccerMatchManager.h"
#include "SoccerFreeKickRestart.h"
#include "SoccerBall.h"
#include "SoccerDebugManager.h"

bool FSoccerOffsidePreparationState::Enter(ASoccerMatchManager& Manager)
{
	if (!Manager.FreeKickRestart.EnterPreparation(Manager))
	{
		return false;
	}

	ASoccerDebugManager::Message(
		&Manager,
		ESoccerDebugCategory::Restarts,
		TEXT("OFFSIDE PREPARATION: posicionando equipos"),
		FColor::Yellow
	);
	return true;
}

void FSoccerOffsidePreparationState::Tick(ASoccerMatchManager& Manager, float DeltaTime)
{
	(void)DeltaTime;
	if (!Manager.FreeKickRestart.IsRuntimeValid())
	{
		Manager.RequestMatchStateTransition(ESoccerMatchStateTransition::Playing);
		return;
	}

	// The human is allowed to move around the restart during Preparation so
	// they can claim or release taker responsibility, but the ball itself remains
	// authoritative at the restart spot until Execution.
	if (IsValid(Manager.SoccerBall))
	{
		Manager.SoccerBall->StopBallKeepingPhysics();
		Manager.SoccerBall->SetActorLocation(
			Manager.FreeKickRestart.GetRestartLocation(),
			false,
			nullptr,
			ETeleportType::TeleportPhysics
		);
	}

	if (!Manager.FreeKickRestart.IsPreparationReady(Manager))
	{
		return;
	}

	Manager.RequestMatchStateTransition(ESoccerMatchStateTransition::OffsideExecution);
}
