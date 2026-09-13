#include "SoccerGoalKickExecutionState.h"

#include "SoccerMatchManager.h"
#include "SoccerGoalLineRestart.h"
#include "SoccerAICharacter.h"
#include "SoccerBall.h"
#include "SoccerDebugManager.h"
#include "GameFramework/CharacterMovementComponent.h"

bool FSoccerGoalKickExecutionState::Enter(ASoccerMatchManager& Manager)
{
	ASoccerAICharacter* Taker = Manager.GoalLineRestart.GetTaker();

	if (
		Manager.GoalLineRestart.GetType() != ESoccerGoalLineRestartType::GoalKick ||
		!IsValid(Taker) ||
		!IsValid(Manager.SoccerBall)
	)
	{
		Manager.CancelGoalLineRestart();
		return false;
	}

	if (Manager.IsNonFreeKickHumanTakerClaimedFor(ESoccerRestartType::GoalKick))
	{
		Manager.GoalLineRestart.ResetGoalKickFinalRunRuntime(Manager);
		Manager.MatchPlayState = ESoccerMatchPlayState::GoalLineRestartTaking;
		Manager.AuthorizeNonFreeKickHumanTakerExecution(
			ESoccerRestartType::GoalKick
		);
		Manager.ResetActiveRestartReadyHold();

		ASoccerDebugManager::Message(
			&Manager,
			ESoccerDebugCategory::Restarts,
			TEXT("GOAL KICK EXECUTION: humano habilitado para sacar"),
			FColor::Cyan
		);
		return true;
	}

	Manager.SelectActiveRestartExecutionReceiver(
		Manager.GoalLineRestart.GetRestartTeam(),
		Taker,
		Manager.GoalLineRestart.GetReceiver(),
		true
	);

	// Preparation keeps using the tactical ReceiverAI. Recalculate only the
	// execution-facing geometry so the kick can now point at the human.
	Manager.RecalculateGoalLineRestartGeometry();

	FVector RunDirection = FVector::ForwardVector;
	FVector RunThroughLocation = FVector::ZeroVector;

	if (!Manager.BuildRestartKickRunGeometryFromCurrentTaker(
		Taker,
		Manager.GoalKickRunThroughDistance,
		RunDirection,
		RunThroughLocation
	))
	{
		Manager.CancelGoalLineRestart();
		return false;
	}

	Manager.GoalLineRestart.BeginGoalKickFinalRunRuntime(
		Manager,
		RunDirection,
		RunThroughLocation
	);

	Manager.MatchPlayState = ESoccerMatchPlayState::GoalLineRestartTaking;
	Manager.ResetActiveRestartReadyHold();

	ASoccerDebugManager::Message(
		&Manager,
		ESoccerDebugCategory::Restarts,
		TEXT("GOAL KICK EXECUTION: comienza la carrera final"),
		FColor::Cyan
	);
	return true;
}

void FSoccerGoalKickExecutionState::Tick(
	ASoccerMatchManager& Manager,
	float DeltaTime
)
{
	ASoccerAICharacter* Taker = Manager.GoalLineRestart.GetTaker();

	if (
		Manager.MatchPlayState == ESoccerMatchPlayState::Playing &&
		!Manager.IsRestartContextActive()
	)
	{
		Manager.RequestMatchStateTransition(ESoccerMatchStateTransition::Playing);
		return;
	}

	if (
		Manager.GoalLineRestart.GetType() != ESoccerGoalLineRestartType::GoalKick ||
		!IsValid(Taker) ||
		!IsValid(Manager.SoccerBall)
	)
	{
		Manager.CancelGoalLineRestart();
		Manager.RequestMatchStateTransition(ESoccerMatchStateTransition::Playing);
		return;
	}

	if (Manager.bActiveNonFreeKickHumanExecutionAuthorized)
	{
		Manager.SoccerBall->StopBallKeepingPhysics();
		Manager.SoccerBall->SetActorLocation(
			Manager.GoalLineRestart.GetBallLocation(),
			false,
			nullptr,
			ETeleportType::TeleportPhysics
		);

		if (Manager.ShouldNonFreeKickHumanTakerKeepExecutionClaim(
			ESoccerRestartType::GoalKick
		))
		{
			return;
		}

		Manager.ReleaseNonFreeKickHumanTakerClaim();
		Manager.ResetActiveRestartLivePositioning();
		Manager.RecalculateGoalLineRestartGeometry();
		Manager.MatchPlayState = ESoccerMatchPlayState::GoalLineRestartSetup;
		Manager.CaptureActiveRestartAITargetLocations(
			Manager.AreActiveRestartOpponentsLegal()
		);
		Manager.RequestMatchStateTransition(
			ESoccerMatchStateTransition::GoalKickPreparation
		);
		return;
	}

	if (Manager.GoalLineRestart.IsAIKickMontageStarted())
	{
		Taker->ClearScriptedLocomotionVelocity();
		Manager.CompleteGoalLineRestart();

		if (Manager.GoalLineRestart.GetType() == ESoccerGoalLineRestartType::None)
		{
			Manager.RequestMatchStateTransition(ESoccerMatchStateTransition::Playing);
		}
		return;
	}

	if (!Manager.GoalLineRestart.IsGoalKickFinalRunActive())
	{
		Manager.RequestMatchStateTransition(
			ESoccerMatchStateTransition::GoalKickPreparation
		);
		return;
	}

	const FRotator DesiredRotation =
		Manager.GoalLineRestart.GetGoalKickRunDirection().Rotation();

	Taker->SetActorRotation(
		FMath::RInterpConstantTo(
			Taker->GetActorRotation(),
			DesiredRotation,
			DeltaTime,
			540.0f
		)
	);

	const ERestartKickContactResult ContactResult =
		Manager.GoalLineRestart.EvaluateGoalKickContact(Manager);

	if (ContactResult == ERestartKickContactResult::MissedBall)
	{
		Manager.GoalLineRestart.ResetGoalKickFinalRunRuntime(Manager);
		Manager.RecalculateGoalLineRestartGeometry();

		const FVector& RunUpStartLocation =
			Manager.GoalLineRestart.GetGoalKickRunUpStartLocation();

		if (IsValid(Taker) && !RunUpStartLocation.IsNearlyZero())
		{
			Manager.ActiveRestartAITargetLocations.Add(
				Taker,
				RunUpStartLocation
			);
		}

		Manager.RequestMatchStateTransition(
			ESoccerMatchStateTransition::GoalKickPreparation
		);
		return;
	}

	if (ContactResult != ERestartKickContactResult::Contact)
	{
		return;
	}

	Taker->SetActorRotation(
		Manager.GoalLineRestart.GetKickDirection().Rotation()
	);
	Manager.CompleteGoalLineRestart();

	if (Manager.GoalLineRestart.GetType() == ESoccerGoalLineRestartType::None)
	{
		Manager.RequestMatchStateTransition(ESoccerMatchStateTransition::Playing);
	}
}
