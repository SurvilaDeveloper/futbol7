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
		Manager.GoalLineRestart.GetType() != ESoccerGoalLineRestartType::CornerKick ||
		!Manager.GoalLineRestart.IsCornerFinalRunActive() ||
		!IsValid(Taker) ||
		!IsValid(Manager.SoccerBall)
	)
	{
		Manager.CancelGoalLineRestart();
		Manager.RequestMatchStateTransition(ESoccerMatchStateTransition::Playing);
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

	Taker->ClearScriptedLocomotionVelocity();
	Taker->SetActorRotation(
		Manager.GoalLineRestart.GetKickDirection().Rotation()
	);
	Manager.CompleteGoalLineRestart();

	if (Manager.GoalLineRestart.GetType() == ESoccerGoalLineRestartType::None)
	{
		Manager.RequestMatchStateTransition(ESoccerMatchStateTransition::Playing);
	}
}
