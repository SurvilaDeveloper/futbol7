#include "SoccerOffsideConfigurationState.h"

#include "SoccerMatchManager.h"
#include "SoccerFreeKickRestart.h"
#include "SoccerDebugManager.h"
#include "Engine/World.h"

bool FSoccerOffsideConfigurationState::Enter(ASoccerMatchManager& Manager)
{
	// Configuration only chooses and validates the offside-specific actors/data.
	// Ball placement and tactical readiness belong to Preparation.
	Manager.CancelBallOutOfPlayDelay();
	Manager.CancelThrowInRestart();
	Manager.CancelGoalLineRestart();
	Manager.DestroyActiveRestartHumanRestrictionIndicator();
	Manager.ClearPendingOffsideSnapshot();
	Manager.ClearNoRetouchRestriction();
	Manager.ClearAttackRunRelease();
	Manager.ClearAssignedAI();
	Manager.ReleaseAllAIBallPossessions();
	Manager.ReleaseAllHumanBallPossessions();

	if (!Manager.FreeKickRestart.Configure(
		Manager,
		ESoccerRestartType::OffsideFreeKick,
		RestartTeam,
		RestartLocation
	))
	{
		ASoccerDebugManager::Message(
			&Manager,
			ESoccerDebugCategory::Restarts,
			TEXT("OFFSIDE CONFIGURATION FALLIDA"),
			FColor::Red
		);
		return false;
	}

	bConfigured = true;
	ASoccerDebugManager::Message(
		&Manager,
		ESoccerDebugCategory::Restarts,
		TEXT("OFFSIDE CONFIGURATION: ejecutor y receptor seleccionados"),
		FColor::Yellow
	);
	return true;
}

void FSoccerOffsideConfigurationState::Tick(ASoccerMatchManager& Manager, float DeltaTime)
{
	(void)DeltaTime;
	if (bConfigured)
	{
		Manager.RequestMatchStateTransition(ESoccerMatchStateTransition::OffsidePreparation);
	}
}
