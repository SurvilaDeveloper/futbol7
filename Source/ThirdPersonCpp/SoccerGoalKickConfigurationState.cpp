#include "SoccerGoalKickConfigurationState.h"

#include "SoccerMatchManager.h"
#include "SoccerGoalLineRestart.h"
#include "SoccerAICharacter.h"
#include "SoccerBall.h"
#include "SoccerDebugManager.h"
#include "Engine/World.h"

bool FSoccerGoalKickConfigurationState::Enter(ASoccerMatchManager& Manager)
{
	if (Manager.GetWorld() == nullptr || !IsValid(Manager.SoccerBall))
	{
		return false;
	}

	// Configuration decides the goal-kick-specific actors and geometry only.
	// Ball placement, restrictions and tactical movement belong to Preparation.
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

	if (!Manager.ConfigureGoalLineRestart(
		ESoccerGoalLineRestartType::GoalKick,
		RestartTeam,
		CrossingLocation,
		GoalLineSign
	))
	{
		return false;
	}

	bConfigured = true;

	ASoccerDebugManager::Message(
		&Manager,
		ESoccerDebugCategory::Restarts,
		TEXT("GOAL KICK CONFIGURATION: ejecutor, receptor y geometria listos"),
		FColor::Blue
	);
	return true;
}

void FSoccerGoalKickConfigurationState::Tick(ASoccerMatchManager& Manager, float DeltaTime)
{
	(void)DeltaTime;
	if (bConfigured)
	{
		Manager.RequestMatchStateTransition(ESoccerMatchStateTransition::GoalKickPreparation);
	}
}
