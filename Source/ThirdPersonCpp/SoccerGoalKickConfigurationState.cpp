#include "SoccerGoalKickConfigurationState.h"

#include "SoccerMatchManager.h"
#include "SoccerGoalLineRestart.h"
#include "SoccerAICharacter.h"
#include "SoccerBall.h"
#include "SoccerDebugManager.h"
#include "Engine/World.h"

bool FSoccerGoalKickConfigurationState::Enter(ASoccerMatchManager& Manager)
{
	UWorld* World = Manager.GetWorld();
	if (World == nullptr || !IsValid(Manager.SoccerBall))
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

	const float NormalizedGoalLineSign = GoalLineSign >= 0.0f ? 1.0f : -1.0f;
	const FVector GoalKickBallLocation = Manager.BuildGoalKickBallLocation(
		CrossingLocation,
		NormalizedGoalLineSign
	);

	Manager.GoalLineRestart.ConfigureGoalKick(
		RestartTeam,
		NormalizedGoalLineSign,
		CrossingLocation,
		GoalKickBallLocation,
		World->GetTimeSeconds()
	);

	ASoccerAICharacter* GoalKickTakerAI = Manager.FindBestGoalLineRestartTakerForTeam(
		RestartTeam,
		ESoccerGoalLineRestartType::GoalKick,
		GoalKickBallLocation
	);
	ASoccerAICharacter* GoalKickReceiverAI = Manager.FindBestGoalLineRestartReceiverForTeam(
		RestartTeam,
		ESoccerGoalLineRestartType::GoalKick,
		GoalKickTakerAI,
		GoalKickBallLocation
	);

	Manager.GoalLineRestart.SetGoalKickParticipants(GoalKickTakerAI, GoalKickReceiverAI);

	if (!Manager.GoalLineRestart.IsGoalKickConfigured())
	{
		Manager.GoalLineRestart.ResetRuntime();
		return false;
	}

	Manager.RecalculateGoalLineRestartGeometry();
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
