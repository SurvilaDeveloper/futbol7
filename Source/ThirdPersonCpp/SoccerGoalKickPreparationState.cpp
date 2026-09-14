#include "SoccerGoalKickPreparationState.h"

#include "SoccerMatchManager.h"
#include "SoccerGoalLineRestart.h"
#include "SoccerAICharacter.h"
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

	const bool bUseStagedRestartContext =
		Manager.IsRestartContextActive() &&
		Manager.ActiveRestartType == ESoccerRestartType::GoalKick &&
		Manager.ActiveRestartTeam ==
			Manager.GoalLineRestart.GetRestartTeam() &&
		Manager.ActiveRestartLocation.Equals(
			Manager.GoalLineRestart.GetBallLocation(),
			1.0f
		);

	if (!bUseStagedRestartContext)
	{
		Manager.BeginRestartContext(
			ESoccerRestartType::GoalKick,
			Manager.GoalLineRestart.GetRestartTeam(),
			Manager.GoalLineRestart.GetBallLocation()
		);
	}

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
	const bool bHumanTakerClaimChanged =
		Manager.UpdateNonFreeKickHumanTakerClaimDuringPreparation(
			ESoccerRestartType::GoalKick
		);

	if (
		!bUseStagedRestartContext ||
		bHumanTakerClaimChanged ||
		Manager.ActiveRestartAITargetLocations.Num() == 0
	)
	{
		Manager.CaptureActiveRestartAITargetLocations(false);
	}

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

	Manager.SoccerBall->StopBallKeepingPhysics();
	Manager.SoccerBall->SetActorLocation(
		Manager.GoalLineRestart.GetBallLocation(),
		false,
		nullptr,
		ETeleportType::TeleportPhysics
	);

	if (Manager.UpdateNonFreeKickHumanTakerClaimDuringPreparation(
		ESoccerRestartType::GoalKick
	))
	{
		Manager.ResetActiveRestartLivePositioning();
		Manager.RecalculateGoalLineRestartGeometry();
		Manager.CaptureActiveRestartAITargetLocations(
			Manager.AreActiveRestartOpponentsLegal()
		);
		return;
	}

	// First complete the fixed legal setup, including the penalty-area escape.
	// The live phase starts only after every AI has remained settled long enough.
	if (!Manager.IsActiveRestartLivePositioningActive())
	{
		if (!Manager.UpdateActiveRestartReadiness(
			Manager.GoalLineRestart.GetSetupStartTime(),
			Manager.GoalLineRestartMinSetupTime
		))
		{
			return;
		}

		Manager.GoalLineRestart.ResetGoalKickFinalRunRuntime(Manager);
		Manager.RecalculateGoalLineRestartGeometry();
		Manager.BeginActiveRestartLivePositioning();
	}

	// The human remains fully manual. Bot plans continue adapting until the
	// player chooses a valid target; StoreKickTarget then locks those plans.
	if (Manager.IsNonFreeKickHumanTakerClaimedFor(ESoccerRestartType::GoalKick))
	{
		Manager.RequestMatchStateTransition(
			ESoccerMatchStateTransition::GoalKickExecution
		);
		return;
	}

	if (!Manager.IsActiveRestartAILivePositioningWaitComplete())
	{
		return;
	}

	// A dynamic mark is legal by construction, but the human opponent is only
	// observed. Do not start the run while that player remains inside the area.
	if (!Manager.AreActiveRestartOpponentsLegal())
	{
		return;
	}

	// Select the intended receiver at the start of the run, without freezing
	// either team's live destinations. The actual direction is sampled at contact.
	Manager.CommitBestActiveRestartLiveReceiver();

	Manager.RequestMatchStateTransition(
		ESoccerMatchStateTransition::GoalKickExecution
	);
}
