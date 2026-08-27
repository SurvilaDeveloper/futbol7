#include "SoccerFaultPreparationState.h"

#include "SoccerMatchManager.h"
#include "SoccerFreeKickRestart.h"
#include "SoccerBall.h"
#include "SoccerDebugManager.h"

bool FSoccerFaultPreparationState::Enter(ASoccerMatchManager& Manager)
{
	if (!Manager.FreeKickRestart.EnterPreparation(Manager))
	{
		return false;
	}

	ASoccerDebugManager::Message(
		&Manager,
		ESoccerDebugCategory::Restarts,
		TEXT("FAULT PREPARATION: posicionando equipos"),
		FColor::Yellow
	);
	return true;
}

void FSoccerFaultPreparationState::Tick(ASoccerMatchManager& Manager, float DeltaTime)
{
	(void)DeltaTime;
	if (!Manager.FreeKickRestart.IsRuntimeValid())
	{
		Manager.RequestMatchStateTransition(ESoccerMatchStateTransition::Playing);
		return;
	}

	// Fault owns the official restart spot. Keep the ball fixed there throughout
	// preparation so a late physics response from the original tackle cannot
	// move it away from the centre of the restriction circle.
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

	Manager.RequestMatchStateTransition(ESoccerMatchStateTransition::FaultExecution);
}
