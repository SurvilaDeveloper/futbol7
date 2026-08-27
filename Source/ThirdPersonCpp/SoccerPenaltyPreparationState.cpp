#include "SoccerPenaltyPreparationState.h"

#include "SoccerMatchManager.h"
#include "SoccerPenaltyKickRestart.h"
#include "SoccerBall.h"
#include "SoccerDebugManager.h"
#include "ThirdPersonCppCharacter.h"
#include "Engine/World.h"

bool FSoccerPenaltyPreparationState::Enter(ASoccerMatchManager& Manager)
{
	FSoccerPenaltyKickRestart& Penalty = Manager.PenaltyKickRestart;
	UWorld* World = Manager.GetWorld();
	if (World == nullptr || !IsValid(Manager.SoccerBall) || !Penalty.Taker.IsValid())
	{
		return false;
	}

	Penalty.SetupStartTime = World->GetTimeSeconds();

	Manager.PossessingCharacter = nullptr;
	Manager.PossessionTeam = ESoccerPossessionTeam::None;
	Manager.SoccerBall->SetPossessed(false);
	Manager.SoccerBall->StopBallKeepingPhysics();
	Manager.SoccerBall->SetActorLocation(
		Penalty.SpotLocation,
		false,
		nullptr,
		ETeleportType::TeleportPhysics
	);

	Manager.bBallOutOfPlayLatched = false;
	Manager.PreviousBallBoundarySampleLocation = Penalty.SpotLocation;

	if (Penalty.bUsesHumanTaker && Penalty.TakerHuman.IsValid())
	{
		AThirdPersonCppCharacter* HumanTaker = Penalty.TakerHuman.Get();
		FVector HumanLocation = Penalty.GetRunUpLocation(Manager);
		HumanLocation.Z = HumanTaker->GetActorLocation().Z;
		HumanTaker->SetActorLocation(HumanLocation, false, nullptr, ETeleportType::TeleportPhysics);

		FVector Face = Penalty.SpotLocation - HumanLocation;
		Face.Z = 0.0f;
		if (!Face.IsNearlyZero())
		{
			HumanTaker->SetActorRotation(Face.Rotation());
		}
	}

	Manager.BeginRestartContext(
		ESoccerRestartType::PenaltyKick,
		Penalty.RestartTeam,
		Penalty.SpotLocation
	);

	// Compatibility bridge while the rest of the project is migrated to the
	// explicit state objects.
	Manager.MatchPlayState = ESoccerMatchPlayState::PenaltyKickSetup;
	Manager.CaptureActiveRestartAITargetLocations(true);

	ASoccerDebugManager::Message(
		&Manager,
		ESoccerDebugCategory::Restarts,
		TEXT("PENAL PREPARATION: posicionando jugadores"),
		FColor::Yellow
	);

	return true;
}

void FSoccerPenaltyPreparationState::Tick(ASoccerMatchManager& Manager, float DeltaTime)
{
	(void)DeltaTime;
	FSoccerPenaltyKickRestart& Penalty = Manager.PenaltyKickRestart;
	UWorld* World = Manager.GetWorld();
	if (World == nullptr || !Penalty.Taker.IsValid())
	{
		Manager.RequestMatchStateTransition(ESoccerMatchStateTransition::Playing);
		return;
	}

	const bool bReady = Manager.UpdateActiveRestartReadiness(
		Penalty.SetupStartTime,
		FMath::Max(0.0f, Manager.PenaltyKickMinimumSetupTime)
	);

	const float SetupElapsed = World->GetTimeSeconds() - Penalty.SetupStartTime;
	const bool bSetupTimedOut =
		SetupElapsed >= FMath::Max(1.0f, Manager.PenaltyKickMaximumSetupWaitTime);
	const bool bGoalkeeperReady = Penalty.IsDefendingGoalkeeperReady(Manager);

	if (!bGoalkeeperReady)
	{
		if (bSetupTimedOut)
		{
			// The generic timeout may forgive a non-critical outfield bot, but it
			// must never start a penalty with the defending goalkeeper stranded near
			// a post. Recover it to the exact geometric goal center and re-evaluate
			// on the next tick.
			Penalty.RecoverDefendingGoalkeeperToCenter(Manager);
			Manager.CaptureActiveRestartAITargetLocations(true);

			ASoccerDebugManager::Message(
				&Manager,
				ESoccerDebugCategory::Restarts,
				TEXT("PENAL PREPARATION: recuperacion del arquero al centro geometrico"),
				FColor::Orange
			);
		}
		return;
	}

	if (!bReady && !bSetupTimedOut)
	{
		return;
	}

	if (bSetupTimedOut && !bReady)
	{
		ASoccerDebugManager::Message(
			&Manager,
			ESoccerDebugCategory::Restarts,
			TEXT("PENAL PREPARATION: timeout de jugadores no criticos; arquero listo"),
			FColor::Orange
		);
	}

	Manager.RequestMatchStateTransition(ESoccerMatchStateTransition::PenaltyExecution);
}
