#include "SoccerKickoffPreparationState.h"

#include "SoccerMatchManager.h"
#include "SoccerAICharacter.h"
#include "SoccerBall.h"
#include "SoccerDebugManager.h"

bool FSoccerKickoffPreparationState::Enter(ASoccerMatchManager& Manager)
{
	if (
		!IsValid(Manager.SoccerBall) ||
		!IsValid(Manager.KickoffTakerAI) ||
		!IsValid(Manager.KickoffReceiverAI)
		)
	{
		return false;
	}

	Manager.ResetBallToCenter();
	Manager.PossessionTeam = ESoccerPossessionTeam::None;
	Manager.PossessingCharacter = nullptr;

	Manager.BeginRestartContext(
		ESoccerRestartType::Kickoff,
		Manager.PendingKickoffTeam,
		Manager.GetKickoffCenterLocation()
	);

	Manager.MatchPlayState = ESoccerMatchPlayState::KickoffSetup;
	Manager.CaptureActiveRestartAITargetLocations(false);

	ASoccerDebugManager::Message(
		&Manager,
		ESoccerDebugCategory::Restarts,
		TEXT("KICKOFF PREPARATION: posicionando equipos"),
		FColor::Yellow
	);
	return true;
}

void FSoccerKickoffPreparationState::Tick(ASoccerMatchManager& Manager, float DeltaTime)
{
	(void)DeltaTime;

	if (!IsValid(Manager.SoccerBall))
	{
		Manager.EndRestartContext();
		Manager.RequestMatchStateTransition(ESoccerMatchStateTransition::Playing);
		return;
	}

	Manager.SoccerBall->StopBallKeepingPhysics();

	if (!IsValid(Manager.KickoffTakerAI) || !IsValid(Manager.KickoffReceiverAI))
	{
		Manager.EndRestartContext();
		Manager.RequestMatchStateTransition(ESoccerMatchStateTransition::Playing);
		return;
	}

	if (!Manager.UpdateActiveRestartReadiness(0.0f, 0.0f))
	{
		return;
	}

	Manager.RecalculateKickoffRunUpGeometry();
	if (Manager.KickoffRunUpStartLocation.IsNearlyZero())
	{
		Manager.EndRestartContext();
		Manager.RequestMatchStateTransition(ESoccerMatchStateTransition::Playing);
		return;
	}

	Manager.RequestMatchStateTransition(ESoccerMatchStateTransition::KickoffExecution);
}
