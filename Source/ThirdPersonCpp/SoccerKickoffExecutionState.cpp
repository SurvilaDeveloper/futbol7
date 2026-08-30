#include "SoccerKickoffExecutionState.h"

#include "SoccerMatchManager.h"
#include "SoccerAICharacter.h"
#include "SoccerCharacterBase.h"
#include "SoccerBall.h"
#include "SoccerDebugManager.h"
#include "Engine/Engine.h"

bool FSoccerKickoffExecutionState::Enter(ASoccerMatchManager& Manager)
{
	if (
		!IsValid(Manager.SoccerBall) ||
		!IsValid(Manager.KickoffTakerAI) ||
		!IsValid(Manager.KickoffReceiverAI) ||
		!Manager.IsRestartContextActive()
	)
	{
		return false;
	}

	Manager.MatchPlayState = ESoccerMatchPlayState::KickoffTaking;
	Manager.CaptureActiveRestartAITargetLocations(true);

	ASoccerDebugManager::Message(
		&Manager,
		ESoccerDebugCategory::Restarts,
		TEXT("KICKOFF EXECUTION: comienza la carrera"),
		FColor::Cyan
	);
	return true;
}

void FSoccerKickoffExecutionState::Tick(
	ASoccerMatchManager& Manager,
	float DeltaTime
)
{
	(void)DeltaTime;

	if (
		!IsValid(Manager.SoccerBall) ||
		!IsValid(Manager.KickoffTakerAI) ||
		!IsValid(Manager.KickoffReceiverAI)
	)
	{
		Manager.EndRestartContext();
		Manager.RequestMatchStateTransition(
			ESoccerMatchStateTransition::Playing
		);
		return;
	}

	Manager.SoccerBall->StopBallKeepingPhysics();

	if (!Manager.bKickoffFinalRunActive)
	{
		if (!Manager.UpdateActiveRestartReadiness(0.0f, 0.0f))
		{
			return;
		}

		const FVector BallLocation =
			Manager.SoccerBall->GetActorLocation();

		Manager.SelectActiveRestartExecutionReceiver(
			Manager.PendingKickoffTeam,
			Manager.KickoffTakerAI,
			Manager.KickoffReceiverAI,
			false
		);

		const FVector ExecutionReceiverLocation =
			Manager.GetActiveRestartExecutionTargetLocation(
				Manager.KickoffReceiverAI->GetActorLocation()
			);

		// Pass direction and run geometry remain separate: the taker can run
		// through the ball while the pass is aimed at the selected receiver.
		Manager.KickoffKickDirection =
			ExecutionReceiverLocation - BallLocation;
		Manager.KickoffKickDirection.Z = 0.0f;

		if (!Manager.KickoffKickDirection.Normalize())
		{
			Manager.KickoffKickDirection =
				Manager.GetFieldAttackDirectionForTeam(
					Manager.PendingKickoffTeam
				);
		}

		if (!Manager.BuildRestartKickRunGeometryFromCurrentTaker(
			Manager.KickoffTakerAI,
			Manager.KickoffRunThroughDistance,
			Manager.KickoffRunDirection,
			Manager.KickoffRunThroughLocation
		))
		{
			return;
		}

		Manager.bKickoffFinalRunActive = true;
		Manager.InitializeRestartKickContactTracking(
			Manager.KickoffKickContactTracker,
			Manager.KickoffTakerAI
		);
		Manager.ResetActiveRestartReadyHold();
		return;
	}

	const ERestartKickContactResult ContactResult =
		Manager.EvaluateRestartKickContact(
			Manager.KickoffTakerAI,
			Manager.KickoffRunDirection,
			Manager.KickoffMaxBallSurfaceGapForKick,
			Manager.KickoffMinimumFacingDot,
			Manager.KickoffKickContactTracker
		);

	if (ContactResult == ERestartKickContactResult::MissedBall)
	{
		Manager.bKickoffFinalRunActive = false;
		Manager.ResetRestartKickContactTracking(
			Manager.KickoffKickContactTracker
		);
		Manager.RecalculateKickoffRunUpGeometry();

		if (
			IsValid(Manager.KickoffTakerAI) &&
			!Manager.KickoffRunUpStartLocation.IsNearlyZero()
		)
		{
			Manager.ActiveRestartAITargetLocations.Add(
				Manager.KickoffTakerAI,
				Manager.KickoffRunUpStartLocation
			);
		}

		Manager.ResetActiveRestartReadyHold();
		return;
	}

	if (ContactResult != ERestartKickContactResult::Contact)
	{
		return;
	}

	FVector PassTargetLocation =
		Manager.GetActiveRestartExecutionTargetLocation(
			Manager.KickoffReceiverAI->GetActorLocation()
		);
	PassTargetLocation.Z =
		Manager.SoccerBall->GetActorLocation().Z;

	Manager.KickoffTakerAI->PlayAIKickAnimationForRestart();

	Manager.SoccerBall->KickToTarget(
		PassTargetLocation,
		Manager.KickoffPassHorizontalSpeed,
		Manager.KickoffPassMinTravelTime,
		Manager.KickoffPassMaxTravelTime
	);

	Manager.RegisterIntentionalBallTouch(
		Manager.KickoffTakerAI
	);

	ASoccerAICharacter* CompletedTaker =
		Manager.KickoffTakerAI;
	ASoccerCharacterBase* CompletedExecutionReceiver =
		Manager.GetActiveRestartExecutionReceiver();
	const bool bPassWasIntendedForHuman =
		Manager.IsActiveRestartExecutionReceiverHuman();

	Manager.EndRestartContext();

	// The taker may not touch the ball again until another player touches it.
	Manager.StartNoRetouchRestriction(CompletedTaker);
	Manager.StartAttackRunReleaseForTeam(
		CompletedTaker->GetTeam()
	);

	Manager.MatchPlayState = ESoccerMatchPlayState::Playing;
	Manager.ClearAssignedAI();

	if (bPassWasIntendedForHuman && IsValid(CompletedExecutionReceiver))
	{
		Manager.RegisterOpenPlayPassIntent(
			CompletedTaker,
			CompletedExecutionReceiver,
			PassTargetLocation
		);
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			1.2f,
			FColor::Green,
			TEXT("Saque realizado")
		);
	}

	Manager.RequestMatchStateTransition(
		ESoccerMatchStateTransition::Playing
	);
}
