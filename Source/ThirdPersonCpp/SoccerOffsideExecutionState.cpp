#include "SoccerOffsideExecutionState.h"

#include "SoccerMatchManager.h"
#include "SoccerFreeKickRestart.h"
#include "SoccerBall.h"
#include "SoccerDebugManager.h"

bool FSoccerOffsideExecutionState::Enter(ASoccerMatchManager& Manager)
{
	if (!Manager.FreeKickRestart.EnterExecution(Manager))
	{
		return false;
	}

	ASoccerDebugManager::Message(
		&Manager,
		ESoccerDebugCategory::Restarts,
		Manager.FreeKickRestart.IsHumanTakerClaimed()
			? TEXT("OFFSIDE EXECUTION: humano habilitado para sacar")
			: TEXT("OFFSIDE EXECUTION: comienza la carrera del ejecutor bot"),
		FColor::Cyan
	);
	return true;
}

void FSoccerOffsideExecutionState::Tick(ASoccerMatchManager& Manager, float DeltaTime)
{
	(void)DeltaTime;
	if (!Manager.FreeKickRestart.IsRuntimeValid())
	{
		Manager.RequestMatchStateTransition(ESoccerMatchStateTransition::Playing);
		return;
	}

	// Keep the restart spot authoritative until the designated taker registers
	// the real first touch. This is especially important for a human taker:
	// simply walking into the stationary ball must not move it before the kick.
	if (
		IsValid(Manager.SoccerBall) &&
		Manager.FreeKickRestart.ShouldKeepBallFixedDuringExecution()
		)
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
