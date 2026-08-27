#include "SoccerGoalKickPreparationState.h"

#include "SoccerMatchManager.h"
#include "SoccerGoalLineRestart.h"
#include "SoccerBall.h"
#include "SoccerDebugManager.h"

bool FSoccerGoalKickPreparationState::Enter(ASoccerMatchManager& Manager)
{
	if (!Manager.GoalLineRestart.IsGoalKickConfigured() || !IsValid(Manager.SoccerBall))
	{
		Manager.CancelGoalLineRestart();
		return false;
	}

	Manager.PossessingCharacter = nullptr;
	Manager.PossessionTeam = ESoccerPossessionTeam::None;

	Manager.BeginRestartContext(
		ESoccerRestartType::GoalKick,
		Manager.GoalLineRestart.GetRestartTeam(),
		Manager.GoalLineRestart.GetBallLocation()
	);

	Manager.SoccerBall->SetActorEnableCollision(true);
	Manager.SoccerBall->SetPossessed(false);
	Manager.SoccerBall->StopBallKeepingPhysics();
	Manager.SoccerBall->SetActorLocation(
		Manager.GoalLineRestart.GetBallLocation(),
		false,
		nullptr,
		ETeleportType::TeleportPhysics
	);

	Manager.MatchPlayState = ESoccerMatchPlayState::GoalLineRestartSetup;
	Manager.CaptureActiveRestartAITargetLocations(false);

	ASoccerDebugManager::Message(
		&Manager,
		ESoccerDebugCategory::Restarts,
		TEXT("GOAL KICK PREPARATION: posicionando equipos"),
		FColor::Blue
	);
	return true;
}

void FSoccerGoalKickPreparationState::Tick(ASoccerMatchManager& Manager, float DeltaTime)
{
	if (
		!Manager.GoalLineRestart.IsGoalKickConfigured() ||
		!IsValid(Manager.SoccerBall)
	)
	{
		Manager.CancelGoalLineRestart();
		Manager.RequestMatchStateTransition(ESoccerMatchStateTransition::Playing);
		return;
	}

	if (Manager.MatchPlayState != ESoccerMatchPlayState::GoalLineRestartSetup)
	{
		return;
	}

	if (Manager.UpdateActiveRestartReadiness(
		Manager.GoalLineRestart.GetSetupStartTime(),
		Manager.GoalLineRestartMinSetupTime
	))
	{
		Manager.RequestMatchStateTransition(
			ESoccerMatchStateTransition::GoalKickExecution
		);
	}
}
