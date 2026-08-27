#include "SoccerOffsideExecutionState.h"

#include "SoccerMatchManager.h"
#include "SoccerFreeKickRestart.h"
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
		TEXT("OFFSIDE EXECUTION: comienza la carrera del ejecutor"),
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

	if (Manager.FreeKickRestart.TickExecutionAndCompleteIfNeeded(Manager))
	{
		Manager.RequestMatchStateTransition(ESoccerMatchStateTransition::Playing);
	}
}
