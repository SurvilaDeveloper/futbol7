#include "SoccerFaultExecutionState.h"

#include "SoccerMatchManager.h"
#include "SoccerFreeKickRestart.h"
#include "SoccerBall.h"
#include "SoccerDebugManager.h"

bool FSoccerFaultExecutionState::Enter(ASoccerMatchManager& Manager)
{
	if (!Manager.FreeKickRestart.EnterExecution(Manager))
	{
		return false;
	}

	ASoccerDebugManager::Message(
		&Manager,
		ESoccerDebugCategory::Restarts,
		TEXT("FAULT EXECUTION: comienza la carrera del ejecutor"),
		FColor::Cyan
	);
	return true;
}

void FSoccerFaultExecutionState::Tick(ASoccerMatchManager& Manager, float DeltaTime)
{
	(void)DeltaTime;
	if (!Manager.FreeKickRestart.IsRuntimeValid())
	{
		Manager.RequestMatchStateTransition(ESoccerMatchStateTransition::Playing);
		return;
	}

	// The execution state keeps ownership of the stationary ball until the
	// designated taker reaches the real contact point. Only Complete() is allowed
	// to release it back to open-play physics.
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

	if (Manager.FreeKickRestart.TickExecutionAndCompleteIfNeeded(Manager))
	{
		Manager.RequestMatchStateTransition(ESoccerMatchStateTransition::Playing);
	}
}
