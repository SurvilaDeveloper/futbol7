#include "SoccerThrowInExecutionState.h"

#include "SoccerMatchManager.h"
#include "SoccerAICharacter.h"
#include "SoccerCharacterBase.h"
#include "SoccerBall.h"
#include "SoccerDebugManager.h"
#include "Engine/Engine.h"

bool FSoccerThrowInExecutionState::Enter(ASoccerMatchManager& Manager)
{
	if (!IsValid(Manager.ThrowInTakerAI) || !IsValid(Manager.SoccerBall))
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
		// No montage: release immediately using the same rules as the normal
		// release frame, then let Playing own the accessory return-to-field.
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

		// Playing may resume immediately, but the throw-in montage is allowed to
		// finish before return-to-field locomotion begins. Keep curve motion alive
		// so the remaining in-place animation displacement is still reproduced.
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

	if (!IsValid(Manager.ThrowInTakerAI) || !IsValid(Manager.SoccerBall))
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

	// A direct receiver from a throw-in is never punishable for offside.
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

	// Do not start Jog locomotion while throw_in_in_place is still playing.
	// The Playing-state accessory updater finishes montage curve motion first.
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
