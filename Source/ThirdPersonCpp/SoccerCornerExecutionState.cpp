#include "SoccerCornerExecutionState.h"

#include "SoccerMatchManager.h"
#include "SoccerGoalLineRestart.h"
#include "SoccerAICharacter.h"
#include "SoccerBall.h"
#include "SoccerDebugManager.h"

bool FSoccerCornerExecutionState::Enter(ASoccerMatchManager& Manager)
{
	ASoccerAICharacter* Taker = Manager.GoalLineRestart.GetTaker();

	if (
		Manager.GoalLineRestart.GetType() != ESoccerGoalLineRestartType::CornerKick ||
		!IsValid(Taker) ||
		!IsValid(Manager.SoccerBall)
	)
	{
		Manager.CancelGoalLineRestart();
		return false;
	}

	if (Manager.IsNonFreeKickHumanTakerClaimedFor(ESoccerRestartType::CornerKick))
	{
		Taker->ClearScriptedLocomotionVelocity();
		Manager.GoalLineRestart.ResetCornerFinalRunRuntime(Manager);
		Manager.MatchPlayState = ESoccerMatchPlayState::GoalLineRestartTaking;
		Manager.AuthorizeNonFreeKickHumanTakerExecution(
			ESoccerRestartType::CornerKick
		);
		Manager.ResetActiveRestartReadyHold();

		ASoccerDebugManager::Message(
			&Manager,
			ESoccerDebugCategory::Restarts,
			TEXT("CORNER EXECUTION: humano habilitado para sacar"),
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

	FVector RunDirection = FVector::ForwardVector;
	FVector RunThroughLocation = FVector::ZeroVector;

	if (!Manager.BuildRestartKickRunGeometryFromCurrentTaker(
		Taker,
		Manager.CornerKickRunThroughDistance,
		RunDirection,
		RunThroughLocation
	))
	{
		Manager.CancelGoalLineRestart();
		return false;
	}

	Taker->ClearScriptedLocomotionVelocity();

	Manager.GoalLineRestart.BeginCornerFinalRunRuntime(
		Manager,
		RunDirection,
		RunThroughLocation
	);

	Manager.MatchPlayState = ESoccerMatchPlayState::GoalLineRestartTaking;

	Taker->SetActorRotation(RunDirection.Rotation());
	Taker->RequestAIMovementMode(
		ESoccerAIMovementMode::Run,
		ESoccerAIMovementReason::AttackRunIntoSpace,
		true
	);

	Manager.ResetActiveRestartReadyHold();

	ASoccerDebugManager::Message(
		&Manager,
		ESoccerDebugCategory::Restarts,
		TEXT("CORNER EXECUTION: comienza la carrera final"),
		FColor::Cyan
	);
	return true;
}

void FSoccerCornerExecutionState::Tick(
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
		Manager.GoalLineRestart.GetType() != ESoccerGoalLineRestartType::CornerKick ||
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
			ESoccerRestartType::CornerKick
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
			ESoccerMatchStateTransition::CornerPreparation
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

	if (!Manager.GoalLineRestart.IsCornerFinalRunActive())
	{
		Manager.RequestMatchStateTransition(
			ESoccerMatchStateTransition::CornerPreparation
		);
		return;
	}

	const FVector RunDirection =
		Manager.GoalLineRestart.GetCornerRunDirection();
	const FVector RunThroughLocation =
		Manager.GoalLineRestart.GetCornerRunThroughLocation();

	const FRotator DesiredRotation = RunDirection.Rotation();

	Taker->SetActorRotation(
		FMath::RInterpConstantTo(
			Taker->GetActorRotation(),
			DesiredRotation,
			DeltaTime,
			540.0f
		)
	);

	const FVector CurrentLocation = Taker->GetActorLocation();
	const FVector RequestedLocation = FMath::VInterpConstantTo(
		CurrentLocation,
		RunThroughLocation,
		DeltaTime,
		FMath::Max(1.0f, Manager.CornerKickFinalRunSpeed)
	);

	Taker->SetActorLocation(
		RequestedLocation,
		true,
		nullptr,
		ETeleportType::None
	);

	const FVector ActualLocation = Taker->GetActorLocation();

	FVector ScriptedVelocity =
		DeltaTime > KINDA_SMALL_NUMBER
		? (ActualLocation - CurrentLocation) / DeltaTime
		: FVector::ZeroVector;
	ScriptedVelocity.Z = 0.0f;

	Taker->SetScriptedLocomotionVelocity(
		ScriptedVelocity,
		ESoccerAIMovementMode::Run,
		ESoccerAIMovementReason::AttackRunIntoSpace
	);

	const ERestartKickContactResult ContactResult =
		Manager.GoalLineRestart.EvaluateCornerKickContact(Manager);

	if (ContactResult == ERestartKickContactResult::MissedBall)
	{
		Taker->ClearScriptedLocomotionVelocity();
		Manager.GoalLineRestart.ResetCornerFinalRunRuntime(Manager);
		Manager.RequestMatchStateTransition(
			ESoccerMatchStateTransition::CornerPreparation
		);
		return;
	}

	if (ContactResult != ERestartKickContactResult::Contact)
	{
		return;
	}

	Manager.CompleteGoalLineRestart();

	if (Manager.GoalLineRestart.GetType() == ESoccerGoalLineRestartType::None)
	{
		Manager.RequestMatchStateTransition(ESoccerMatchStateTransition::Playing);
	}
}
