#include "SoccerPenaltyConfigurationState.h"

#include "SoccerMatchManager.h"
#include "SoccerPenaltyKickRestart.h"
#include "SoccerDebugManager.h"
#include "SoccerAICharacter.h"
#include "SoccerBall.h"
#include "ThirdPersonCppCharacter.h"
#include "Engine/World.h"

bool FSoccerPenaltyConfigurationState::Enter(ASoccerMatchManager& Manager)
{
	FSoccerPenaltyKickRestart& Penalty = Manager.PenaltyKickRestart;

	Manager.CancelBallOutOfPlayDelay();
	Manager.CancelThrowInRestart();
	Manager.CancelGoalLineRestart();
	Penalty.ResetRuntimeState();

	UWorld* World = Manager.GetWorld();
	if (World == nullptr || !IsValid(Manager.SoccerBall))
	{
		ASoccerDebugManager::Message(
			&Manager,
			ESoccerDebugCategory::Restarts,
			TEXT("PENAL CONFIGURATION: falta World o SoccerBall valido"),
			FColor::Red
		);
		return false;
	}

	Manager.DestroyActiveRestartHumanRestrictionIndicator();
	Manager.ClearPendingOffsideSnapshot();
	Manager.ClearNoRetouchRestriction();
	Manager.ClearAttackRunRelease();
	Manager.ClearAssignedAI();
	Manager.ReleaseAllAIBallPossessions();
	Manager.ReleaseAllHumanBallPossessions();

	Penalty.RestartTeam = RestartTeam;
	Penalty.IncidentLocation = IncidentLocation;
	Penalty.SpotLocation = Penalty.GetSpotLocation(Manager, RestartTeam);
	Penalty.bTaken = false;
	Penalty.SetupStartTime = 0.0f;

	Penalty.GoalkeeperAI = Manager.FindGoalkeeperForTeam(Manager.GetOppositeTeam(RestartTeam));
	Penalty.TakerHuman.Reset();
	Penalty.TakerAI.Reset();
	Penalty.Taker.Reset();
	Penalty.bUsesHumanTaker = false;

	if (RestartTeam == ESoccerTeam::PlayerTeam && Manager.bPlayerTeamPenaltyUsesHuman)
	{
		AThirdPersonCppCharacter* HumanTaker = Manager.FindHumanCharacterForTeam(RestartTeam);
		Penalty.TakerHuman = HumanTaker;
		Penalty.Taker = HumanTaker;
		Penalty.bUsesHumanTaker = IsValid(HumanTaker);
	}

	if (!Penalty.Taker.IsValid())
	{
		ASoccerAICharacter* AITaker = Penalty.FindAITaker(Manager, RestartTeam);
		Penalty.TakerAI = AITaker;
		Penalty.Taker = AITaker;
		Penalty.bUsesHumanTaker = false;
	}

	if (Penalty.GoalkeeperAI.IsValid())
	{
		// A penalty setup owns the goalkeeper from this point onward. Stop any
		// residual save montage/curve motion so it cannot keep displacing the
		// capsule after the restart has started positioning it.
		Penalty.GoalkeeperAI->StopGoalkeeperActionMontage(0.08f);
	}

	if (!Penalty.Taker.IsValid() || !Penalty.GoalkeeperAI.IsValid())
	{
		const bool bMissingTaker = !Penalty.Taker.IsValid();
		const bool bMissingGoalkeeper = !Penalty.GoalkeeperAI.IsValid();
		Penalty.ResetRuntimeState();

		ASoccerDebugManager::Message(
			&Manager,
			ESoccerDebugCategory::Restarts,
			FString::Printf(
				TEXT("PENAL CONFIGURATION FALLIDA: pateador=%s arquero=%s team=%d"),
				bMissingTaker ? TEXT("FALTA") : TEXT("OK"),
				bMissingGoalkeeper ? TEXT("FALTA") : TEXT("OK"),
				static_cast<int32>(RestartTeam)
			),
			FColor::Red
		);
		return false;
	}

	// Compatibility bridge until every legacy consumer reads the explicit
	// state object instead of ESoccerMatchPlayState.
	Manager.MatchPlayState = ESoccerMatchPlayState::PenaltyKickSetup;
	bConfigured = true;

	ASoccerDebugManager::Message(
		&Manager,
		ESoccerDebugCategory::Restarts,
		Penalty.bUsesHumanTaker
			? TEXT("PENAL CONFIGURATION: ejecutor humano seleccionado")
			: TEXT("PENAL CONFIGURATION: ejecutor bot seleccionado"),
		FColor::Yellow
	);

	return true;
}

void FSoccerPenaltyConfigurationState::Tick(ASoccerMatchManager& Manager, float DeltaTime)
{
	(void)DeltaTime;
	if (bConfigured)
	{
		Manager.RequestMatchStateTransition(ESoccerMatchStateTransition::PenaltyPreparation);
	}
}
