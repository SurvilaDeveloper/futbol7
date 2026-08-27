#include "SoccerOffsidePreparationState.h"

#include "SoccerMatchManager.h"
#include "SoccerFreeKickRestart.h"
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

	if (!Manager.FreeKickRestart.IsPreparationReady(Manager))
	{
		return;
	}

	Manager.RequestMatchStateTransition(ESoccerMatchStateTransition::OffsideExecution);
}
