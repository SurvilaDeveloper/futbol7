#include "SoccerThrowInExecutionState.h"

#include "SoccerMatchManager.h"
#include "SoccerAICharacter.h"
#include "ThirdPersonCppCharacter.h"
#include "SoccerCharacterBase.h"
#include "SoccerBall.h"
#include "SoccerDebugManager.h"
#include "Engine/Engine.h"

bool FSoccerThrowInExecutionState::Enter(ASoccerMatchManager& Manager)
{
	if (!IsValid(Manager.SoccerBall))
	{
		Manager.CancelThrowInRestart();
		return false;
	}

	// Human path. The ball was physically picked up during Preparation, which
	// commits the human to completing this restart. Execution now waits for the
	// left-click target before moving to the direction-dependent outside start.
	if (Manager.bThrowInHumanTakerCommitted)
	{
		if (
			!IsValid(Manager.ThrowInHumanTaker) ||
			!Manager.ThrowInHumanTaker->IsHoldingThrowInBall()
		)
		{
			Manager.CancelThrowInRestart();
			return false;
		}

		Manager.PossessingCharacter = Manager.ThrowInHumanTaker;
		Manager.PossessionTeam =
			Manager.ConvertTeamToPossessionTeam(Manager.ThrowInTeam);

		Manager.bThrowInExecutionActive = true;
		Manager.bThrowInBallReleased = false;
		Manager.bThrowInReturningToField = false;
		Manager.bThrowInCurveMotionInitialized = false;
		Manager.bThrowInHumanExecutionAuthorized = true;
		Manager.bThrowInHumanTargetSelected = false;
		Manager.bThrowInHumanRepositioningForTarget = false;
		Manager.bThrowInHumanMontageStarted = false;
		Manager.ThrowInHumanTargetLocation = FVector::ZeroVector;
		Manager.MatchPlayState = ESoccerMatchPlayState::ThrowInExecuting;

		Manager.ThrowInHumanTaker->ClearThrowInScriptedMovementVelocity();

		ASoccerDebugManager::Message(
			&Manager,
			ESoccerDebugCategory::Restarts,
			TEXT("THROW IN EXECUTION: humano habilitado, click izquierdo para lanzar"),
			FColor::Cyan
		);
		return true;
	}

	// Original AI path.
	if (!IsValid(Manager.ThrowInTakerAI))
	{
		Manager.CancelThrowInRestart();
		return false;
	}

	Manager.ThrowInTakerAI->SetActorRotation(
		Manager.ThrowInDirection.Rotation()
	);

	// The ball normally already travels in the thrower's hands during
	// Preparation. Keep the old fallback for an anomalous lost attachment.
	if (
		!Manager.ThrowInTakerAI->IsAIPossessingBall() ||
		Manager.ThrowInTakerAI->GetControlledAIBall() != Manager.SoccerBall
	)
	{
		if (!Manager.ThrowInTakerAI->HoldThrowInBall(Manager.SoccerBall))
		{
			Manager.CancelThrowInRestart();
			return false;
		}
	}

	Manager.PossessingCharacter = Manager.ThrowInTakerAI;
	Manager.PossessionTeam =
		Manager.ConvertTeamToPossessionTeam(Manager.ThrowInTeam);

	Manager.SelectActiveRestartExecutionReceiver(
		Manager.ThrowInTeam,
		Manager.ThrowInTakerAI,
		Manager.ThrowInReceiverAI,
		true
	);

	Manager.bThrowInExecutionActive = true;
	Manager.bThrowInBallReleased = false;
	Manager.bThrowInReturningToField = false;

	FVector2D InitialCurveDisplacement;
	Manager.bThrowInCurveMotionInitialized =
		Manager.bUseThrowInCurveMotion &&
		Manager.EvaluateThrowInLocalDisplacementNormalized(
			0.0f,
			InitialCurveDisplacement
		);
	Manager.ThrowInCurveMotionStartLocation =
		Manager.ThrowInTakerAI->GetActorLocation();

	Manager.MatchPlayState = ESoccerMatchPlayState::ThrowInExecuting;

	const float MontageDuration =
		Manager.ThrowInTakerAI->PlayThrowInMontage();

	if (MontageDuration <= KINDA_SMALL_NUMBER)
	{
		FVector TargetLocation =
			Manager.GetActiveRestartExecutionTargetLocation(
				IsValid(Manager.ThrowInReceiverAI)
					? Manager.ThrowInReceiverAI->GetActorLocation()
					: Manager.ThrowInReceiverMoveLocation
			);

		TargetLocation.Z = Manager.SoccerBall->GetActorLocation().Z;

		Manager.ClearPendingOffsideSnapshot();

		if (!Manager.TryRegisterIntentionalBallTouch(Manager.ThrowInTakerAI))
		{
			Manager.CancelThrowInRestart();
			return false;
		}

		ASoccerCharacterBase* CompletedExecutionReceiver =
			Manager.GetActiveRestartExecutionReceiver();
		const bool bPassWasIntendedForHuman =
			Manager.IsActiveRestartExecutionReceiverHuman();

		Manager.EndRestartContext();
		Manager.ClearPendingOffsideSnapshot();
		Manager.StartNoRetouchRestriction(Manager.ThrowInTakerAI);
		Manager.StartAttackRunReleaseForTeam(Manager.ThrowInTeam);

		const bool bReleased =
			Manager.ThrowInTakerAI->ReleaseHeldThrowInBallToAirTarget(
				TargetLocation,
				Manager.ThrowInPassHorizontalSpeed,
				Manager.ThrowInPassMinTravelTime,
				Manager.ThrowInPassMaxTravelTime
			);

		if (!bReleased)
		{
			Manager.CancelThrowInRestart();
			return false;
		}

		Manager.bThrowInBallReleased = true;
		Manager.PossessingCharacter = nullptr;
		Manager.PossessionTeam = ESoccerPossessionTeam::None;
		Manager.MatchPlayState = ESoccerMatchPlayState::Playing;
		Manager.ClearAssignedAI();

		if (bPassWasIntendedForHuman && IsValid(CompletedExecutionReceiver))
		{
			Manager.RegisterOpenPlayPassIntent(
				Manager.ThrowInTakerAI,
				CompletedExecutionReceiver,
				TargetLocation
			);
		}

		Manager.bThrowInReturningToField = true;
	}

	ASoccerDebugManager::Message(
		&Manager,
		ESoccerDebugCategory::Restarts,
		TEXT("THROW IN EXECUTION: comienza la ejecucion"),
		FColor::Cyan
	);
	return true;
}

void FSoccerThrowInExecutionState::Tick(
	ASoccerMatchManager& Manager,
	float DeltaTime
)
{
	if (!Manager.bThrowInExecutionActive)
	{
		Manager.RequestMatchStateTransition(
			ESoccerMatchStateTransition::Playing
		);
		return;
	}

	if (Manager.bThrowInBallReleased)
	{
		Manager.RequestMatchStateTransition(
			ESoccerMatchStateTransition::Playing
		);
		return;
	}

	if (!IsValid(Manager.SoccerBall))
	{
		Manager.CancelThrowInRestart();
		Manager.RequestMatchStateTransition(
			ESoccerMatchStateTransition::Playing
		);
		return;
	}

	// ------------------------------------------------------------
	// HUMAN THROW-IN EXECUTION
	// ------------------------------------------------------------
	if (Manager.bThrowInHumanTakerCommitted)
	{
		AThirdPersonCppCharacter* Human = Manager.ThrowInHumanTaker;
		if (
			!IsValid(Human) ||
			!Human->IsHoldingThrowInBall()
		)
		{
			Manager.CancelThrowInRestart();
			Manager.RequestMatchStateTransition(
				ESoccerMatchStateTransition::Playing
			);
			return;
		}

		// Before the click, simply wait with the ball secured in the hands.
		if (!Manager.bThrowInHumanTargetSelected)
		{
			Human->ClearThrowInScriptedMovementVelocity();
			return;
		}

		// The clicked target determines the throw direction, and therefore the
		// exact outside start needed for the in-place motion curve to reach the
		// touchline correctly at the release frame.
		if (!Manager.bThrowInHumanMontageStarted)
		{
			const FVector CurrentLocation = Human->GetActorLocation();
			FVector ToStart = Manager.ThrowInOutsideStartLocation - CurrentLocation;
			ToStart.Z = 0.0f;
			const float DistanceToStart = ToStart.Size();

			if (DistanceToStart > 6.0f)
			{
				const FVector MoveDirection = ToStart.GetSafeNormal();
				Human->SetActorRotation(MoveDirection.Rotation());
				Human->SetThrowInScriptedMovementVelocity(
					MoveDirection *
					FMath::Max(1.0f, Manager.ThrowInOutsidePositioningSpeed)
				);
				Manager.bThrowInHumanRepositioningForTarget = true;
				return;
			}

			Human->ClearThrowInScriptedMovementVelocity();
			Human->SetActorLocation(
				Manager.ThrowInOutsideStartLocation,
				false,
				nullptr,
				ETeleportType::TeleportPhysics
			);
			Human->SetActorRotation(Manager.ThrowInDirection.Rotation());
			Manager.bThrowInHumanRepositioningForTarget = false;

			FVector2D InitialCurveDisplacement;
			Manager.bThrowInCurveMotionInitialized =
				Manager.bUseThrowInCurveMotion &&
				Manager.EvaluateThrowInLocalDisplacementNormalized(
					0.0f,
					InitialCurveDisplacement
				);
			Manager.ThrowInCurveMotionStartLocation = Human->GetActorLocation();

			const float MontageDuration = Human->PlayThrowInMontage();
			Manager.bThrowInHumanMontageStarted = true;

			if (MontageDuration <= KINDA_SMALL_NUMBER)
			{
				// Missing montage is a safe gameplay fallback. Do not apply the full
				// authored displacement instantaneously; release from the prepared spot.
				Manager.bThrowInCurveMotionInitialized = false;
				Manager.CompleteHumanThrowInRelease();
			}
			return;
		}

		float MontagePosition = 0.0f;
		float MontageLength = 0.0f;
		const bool bMontageActive = Human->GetThrowInMontagePlaybackState(
			MontagePosition,
			MontageLength
		);

		bool bShouldRelease = false;
		if (!bMontageActive)
		{
			Manager.ApplyThrowInCurveMotionNormalized(1.0f);
			bShouldRelease = true;
		}
		else
		{
			const float NormalizedTime = FMath::Clamp(
				MontagePosition /
					FMath::Max(MontageLength, KINDA_SMALL_NUMBER),
				0.0f,
				1.0f
			);

			Manager.ApplyThrowInCurveMotionNormalized(NormalizedTime);
			bShouldRelease =
				NormalizedTime >= Manager.ThrowInReleaseNormalizedTime;
		}

		if (bShouldRelease)
		{
			Manager.CompleteHumanThrowInRelease();
		}
		return;
	}

	// ------------------------------------------------------------
	// AI THROW-IN EXECUTION (existing behavior)
	// ------------------------------------------------------------
	if (!IsValid(Manager.ThrowInTakerAI))
	{
		Manager.CancelThrowInRestart();
		Manager.RequestMatchStateTransition(
			ESoccerMatchStateTransition::Playing
		);
		return;
	}

	float MontagePosition = 0.0f;
	float MontageLength = 0.0f;

	const bool bMontageActive =
		Manager.ThrowInTakerAI->GetThrowInMontagePlaybackState(
			MontagePosition,
			MontageLength
		);

	bool bShouldRelease = false;

	if (!bMontageActive)
	{
		Manager.ApplyThrowInCurveMotionNormalized(1.0f);
		bShouldRelease = true;
	}
	else
	{
		const float NormalizedTime = FMath::Clamp(
			MontagePosition /
				FMath::Max(MontageLength, KINDA_SMALL_NUMBER),
			0.0f,
			1.0f
		);

		Manager.ApplyThrowInCurveMotionNormalized(NormalizedTime);

		bShouldRelease =
			NormalizedTime >= Manager.ThrowInReleaseNormalizedTime;
	}

	if (!bShouldRelease)
	{
		return;
	}

	FVector TargetLocation =
		Manager.GetActiveRestartExecutionTargetLocation(
			IsValid(Manager.ThrowInReceiverAI)
			? Manager.ThrowInReceiverAI->GetActorLocation()
			: Manager.ThrowInReceiverMoveLocation
		);

	TargetLocation.Z = Manager.SoccerBall->GetActorLocation().Z;

	Manager.ClearPendingOffsideSnapshot();

	if (!Manager.TryRegisterIntentionalBallTouch(Manager.ThrowInTakerAI))
	{
		return;
	}

	ASoccerCharacterBase* CompletedExecutionReceiver =
		Manager.GetActiveRestartExecutionReceiver();
	const bool bPassWasIntendedForHuman =
		Manager.IsActiveRestartExecutionReceiverHuman();

	Manager.EndRestartContext();
	Manager.ClearPendingOffsideSnapshot();
	Manager.StartNoRetouchRestriction(Manager.ThrowInTakerAI);
	Manager.StartAttackRunReleaseForTeam(Manager.ThrowInTeam);

	const bool bReleased =
		Manager.ThrowInTakerAI->ReleaseHeldThrowInBallToAirTarget(
			TargetLocation,
			Manager.ThrowInPassHorizontalSpeed,
			Manager.ThrowInPassMinTravelTime,
			Manager.ThrowInPassMaxTravelTime
		);

	if (!bReleased)
	{
		return;
	}

	Manager.bThrowInBallReleased = true;
	Manager.PossessingCharacter = nullptr;
	Manager.PossessionTeam = ESoccerPossessionTeam::None;
	Manager.MatchPlayState = ESoccerMatchPlayState::Playing;
	Manager.ClearAssignedAI();

	if (bPassWasIntendedForHuman && IsValid(CompletedExecutionReceiver))
	{
		Manager.RegisterOpenPlayPassIntent(
			Manager.ThrowInTakerAI,
			CompletedExecutionReceiver,
			TargetLocation
		);
	}

	Manager.bThrowInReturningToField = true;

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			1.2f,
			FColor::Green,
			TEXT("Saque lateral realizado")
		);
	}

	Manager.RequestMatchStateTransition(
		ESoccerMatchStateTransition::Playing
	);
}
