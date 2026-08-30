#include "SoccerKickoffConfigurationState.h"

#include "SoccerMatchManager.h"
#include "SoccerAICharacter.h"
#include "SoccerBall.h"
#include "SoccerDebugManager.h"

bool FSoccerKickoffConfigurationState::Enter(ASoccerMatchManager& Manager)
{
	if (!IsValid(Manager.SoccerBall))
	{
		Manager.FindSoccerBall();
	}

	if (!IsValid(Manager.SoccerBall))
	{
		return false;
	}

	// Configuration decides only who takes the kickoff and who receives it.
	// Ball placement and team movement belong to Preparation.
	Manager.CancelBallOutOfPlayDelay();
	Manager.CancelThrowInRestart();
	Manager.CancelGoalLineRestart();
	Manager.DestroyActiveRestartHumanRestrictionIndicator();
	Manager.ClearPendingOffsideSnapshot();
	Manager.ClearAttackRunRelease();
	Manager.ClearNoRetouchRestriction();

	Manager.PendingKickoffTeam = RestartTeam;
	Manager.PossessionTeam = ESoccerPossessionTeam::None;
	Manager.PossessingCharacter = nullptr;
	Manager.ClearAttackState();
	Manager.ClearAssignedAI();
	Manager.ReleaseAllAIBallPossessions();
	Manager.ReleaseAllHumanBallPossessions();
	Manager.KickoffTakerAI = nullptr;
	Manager.KickoffReceiverAI = nullptr;
	Manager.ResetKickoffRunUpState();
	Manager.ResetNonFreeKickHumanTakerRuntime();

	Manager.KickoffTakerAI = Manager.FindKickoffTaker(RestartTeam);
	Manager.KickoffReceiverAI = Manager.FindKickoffReceiver(
		RestartTeam,
		Manager.KickoffTakerAI
	);

	if (!IsValid(Manager.KickoffTakerAI) || !IsValid(Manager.KickoffReceiverAI))
	{
		Manager.KickoffTakerAI = nullptr;
		Manager.KickoffReceiverAI = nullptr;
		return false;
	}

	bConfigured = true;
	ASoccerDebugManager::Message(
		&Manager,
		ESoccerDebugCategory::Restarts,
		TEXT("KICKOFF CONFIGURATION: ejecutor y receptor listos"),
		FColor::Blue
	);
	return true;
}

void FSoccerKickoffConfigurationState::Tick(ASoccerMatchManager& Manager, float DeltaTime)
{
	(void)DeltaTime;
	if (bConfigured)
	{
		Manager.RequestMatchStateTransition(ESoccerMatchStateTransition::KickoffPreparation);
	}
}
