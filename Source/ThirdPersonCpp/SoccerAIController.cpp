//SoccerAIController.cpp

#include "SoccerAIController.h"

#include "SoccerAICharacter.h"
#include "SoccerBall.h"
#include "SoccerCharacterBase.h"
#include "SoccerDebugManager.h"
#include "SoccerField.h"
#include "SoccerFieldDimensions.h"
#include "SoccerMatchManager.h"
#include "SoccerTeamTypes.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"

#include "GameFramework/CharacterMovementComponent.h"

#include "DrawDebugHelpers.h"

#include "EngineUtils.h"
#include "Engine/CurveTable.h"
#include "Curves/RealCurve.h"

#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"

#include "AnimNotifyState_GKContactWindow.h"

#include "Animation/AnimMontage.h"
#include "ThirdPersonCppCharacter.h"

namespace
{
	template <typename TWorldContext>
	void SetGoalkeeperPersistentLineIfEnabled(
		bool bEnabled,
		TWorldContext* WorldContextObject,
		ESoccerDebugCategory Category,
		int32 LineId,
		const FString& Text,
		const FColor& Color,
		float TextScale = 1.0f
	)
	{
		if (!bEnabled)
		{
			return;
		}

		ASoccerDebugManager::SetPersistentLine(
			WorldContextObject,
			Category,
			LineId,
			Text,
			Color,
			TextScale
		);
	}
}

ASoccerAIController::ASoccerAIController()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ASoccerAIController::BeginPlay()
{
	Super::BeginPlay();

	FindMatchManager();
}

bool ASoccerAIController::
StartGoalkeeperDebugSaveTest(
	ASoccerAICharacter* SoccerCharacter,
	ASoccerBall* SoccerBall,
	ESoccerGoalkeeperAction GoalkeeperAction,
	bool bAllowEmergencyBodyContact
)
{
	if (
		!IsValid(SoccerCharacter) ||
		!IsValid(SoccerBall) ||
		GoalkeeperAction ==
		ESoccerGoalkeeperAction::None ||
		GetPawn() != SoccerCharacter
		)
	{
		return false;
	}

	FinishGoalkeeperDebugSaveTest(
		SoccerCharacter
	);

	ClearPendingGoalkeeperSaveAction();
	ClearPendingGoalkeeperSaveImpact();

	ClearGoalkeeperSweeperMode();
	ClearGoalkeeperRetreatMode();
	ClearGoalkeeperLooseBallClaimMode();
	ClearGoalkeeperDistribution();

	StopMovement();

	ClearFocus(
		EAIFocusPriority::Gameplay
	);

	if (SoccerCharacter->IsAIPossessingBall())
	{
		SoccerCharacter->ReleaseAIBall();
	}

	const bool bActionStarted =
		SoccerCharacter->StartGoalkeeperAction(
			GoalkeeperAction
		);

	if (!bActionStarted)
	{
		return false;
	}

	bGoalkeeperDebugSaveTestActive = true;

	bGoalkeeperDebugSaveTestAllowEmergencyBodyContact =
		bAllowEmergencyBodyContact;

	GoalkeeperDebugSaveTestCharacter =
		SoccerCharacter;

	PreparePendingGoalkeeperSaveImpact(
		SoccerCharacter,
		SoccerBall,
		GoalkeeperAction
	);

	SetGoalkeeperBehaviorMode(
		ESoccerGoalkeeperBehaviorMode::Saving
	);

	return true;
}

void ASoccerAIController::
FinishGoalkeeperDebugSaveTest(
	ASoccerAICharacter* SoccerCharacter
)
{
	if (!bGoalkeeperDebugSaveTestActive)
	{
		return;
	}

	if (
		IsValid(SoccerCharacter) &&
		IsValid(GoalkeeperDebugSaveTestCharacter) &&
		GoalkeeperDebugSaveTestCharacter !=
		SoccerCharacter
		)
	{
		return;
	}

	ClearPendingGoalkeeperSaveAction();
	ClearPendingGoalkeeperSaveImpact();

	bGoalkeeperDebugSaveTestActive = false;

	bGoalkeeperDebugSaveTestAllowEmergencyBodyContact =
		false;

	GoalkeeperDebugSaveTestCharacter =
		nullptr;

	if (
		CurrentGoalkeeperBehaviorMode ==
		ESoccerGoalkeeperBehaviorMode::Saving ||
		CurrentGoalkeeperBehaviorMode ==
		ESoccerGoalkeeperBehaviorMode::ShotPending
		)
	{
		SetGoalkeeperBehaviorMode(
			ESoccerGoalkeeperBehaviorMode::Positioning
		);
	}

	ClearFocus(
		EAIFocusPriority::Gameplay
	);
}

void ASoccerAIController::SetAerialDebugIsolation(
	bool bEnabled,
	ASoccerBall* DebugBall
)
{
	bAerialDebugIsolationEnabled = bEnabled;
	AerialDebugIsolationBall = bEnabled ? DebugBall : nullptr;

	StopMovement();
	ClearAerialBallInterceptionMovement();
	ClearPredictiveBallChaseMovement();

	if (!bEnabled)
	{
		ClearFocus(EAIFocusPriority::Gameplay);
	}
}

void ASoccerAIController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	ASoccerAICharacter* SoccerCharacter =
		Cast<ASoccerAICharacter>(GetPawn());

	if (SoccerCharacter == nullptr)
	{
		ClearNavigationRecoveryState();
		ClearPendingGoalkeeperSaveAction();
		ClearPendingGoalkeeperSaveImpact();
		ClearGoalkeeperSweeperMode();
		ClearGoalkeeperRetreatMode();
		ClearGoalkeeperLooseBallClaimMode();
		ClearGoalkeeperDistribution();
		ClearAerialBallInterceptionMovement();

		return;
	}

	if (
		IsValid(MatchManager) &&
		MatchManager->IsCharacterInVisualSubstitution(SoccerCharacter)
	)
	{
		StopMovement();
		ClearAerialBallInterceptionMovement();
		ClearPredictiveBallChaseMovement(SoccerCharacter);
		ClearFocus(EAIFocusPriority::Gameplay);
		SoccerCharacter->SetAIChasingBall(false);
		return;
	}

	if (SoccerCharacter->IsTackleFallReactionActive())
	{
		StopMovement();
		ClearAerialBallInterceptionMovement();
		ClearFocus(EAIFocusPriority::Gameplay);
		SoccerCharacter->SetAIChasingBall(false);
		return;
	}

	if (SoccerCharacter->IsTackleEvasionActive())
	{
		// CharacterBase owns world-space inertia during the in-place jump.
		// Do not StopMovement here because that would erase the preserved speed.
		ClearAerialBallInterceptionMovement();
		ClearPredictiveBallChaseMovement(SoccerCharacter);
		SoccerCharacter->SetAIChasingBall(false);
		return;
	}

	if (TryStartAITackleEvasion(SoccerCharacter))
	{
		return;
	}

	if (SoccerCharacter->IsTackleActive())
	{
		// TryStartTackle() already stopped path-following when the action began.
		// Do NOT call StopMovement() here: tackle inertia is authored by
		// ASoccerCharacterBase::UpdateTackle() through CharacterMovement velocity.
		ClearAerialBallInterceptionMovement();
		ClearPredictiveBallChaseMovement(SoccerCharacter);
		SoccerCharacter->SetAIChasingBall(false);
		return;
	}

	// A pass/shot montage owns the character until its timed foot impact and
	// blend-out finish. Do not let the tactical loop start a second action or
	// overwrite movement while the kick is still being authored.
	if (SoccerCharacter->ShouldAIKickMontageLockController())
	{
		StopMovement();
		ClearAerialBallInterceptionMovement();
		ClearPredictiveBallChaseMovement(SoccerCharacter);
		ClearFocus(EAIFocusPriority::Gameplay);
		SoccerCharacter->SetAIChasingBall(false);
		return;
	}

	if (bAerialDebugIsolationEnabled)
	{
		ASoccerBall* DebugBall = AerialDebugIsolationBall.Get();

		if (!IsValid(DebugBall))
		{
			StopMovement();
			ClearFocus(EAIFocusPriority::Gameplay);
			return;
		}

		if (
			SoccerCharacter->IsAerialActionLocked() ||
			SoccerCharacter->IsAerialActionWaitingToStart()
		)
		{
			StopMovement();
			SetFocus(DebugBall);
			return;
		}

		if (SoccerCharacter->IsAerialActionApproaching())
		{
			FVector MoveLocation = FVector::ZeroVector;
			float AcceptanceRadius = 10.0f;

			if (SoccerCharacter->GetAerialPreparationTarget(
				MoveLocation,
				AcceptanceRadius
			))
			{
				MoveLocation.Z = SoccerCharacter->GetActorLocation().Z;

				MoveToLocationWithAIMovement(
					ESoccerAIOrder::ChaseBall,
					MoveLocation,
					FMath::Max(5.0f, AcceptanceRadius),
					false
				);
			}
		}
		else
		{
			StopMovement();
		}

		SetFocus(DebugBall);
		return;
	}

	if (SoccerCharacter->IsAerialActionLocked())
	{
		StopMovement();
		ClearAerialBallInterceptionMovement();
		ClearFocus(EAIFocusPriority::Gameplay);
		SoccerCharacter->SetAIChasingBall(false);
		return;
	}

	if (SoccerCharacter->IsAIDribbleTurnAutoPassActive())
	{
		if (SoccerCharacter->GetPlayerRole() == ESoccerPlayerRole::Goalkeeper)
		{
			ClearPendingGoalkeeperSaveAction();
			ClearPendingGoalkeeperSaveImpact();
			ClearGoalkeeperSweeperMode();
			ClearGoalkeeperRetreatMode();
			ClearGoalkeeperLooseBallClaimMode();
		}

		// Antes del impacto la pelota sigue controlada por el bot y el giro
		// necesita una base estable. Despues del impacto, IsAIAutoPassActive()
		// ya es true: no esperamos a que termine todo el montage para empezar
		// a perseguir el toque que el propio bot planeo de antemano.
		if (!SoccerCharacter->IsAIAutoPassActive())
		{
			StopMovement();
			return;
		}

		if (!IsValid(MatchManager))
		{
			FindMatchManager();
		}

		if (UpdateAIAutoPassFollow(SoccerCharacter))
		{
			return;
		}

		// Mientras el montage de giro siga activo no abrimos una nueva
		// decision tactica. Si recupero su autopase, conserva la orden de
		// movimiento anterior hasta terminar el giro. Si la intencion quedo
		// invalidada por otro toque, frenamos y esperamos el fin del montage.
		if (!SoccerCharacter->IsAIPossessingBall())
		{
			StopMovement();
		}

		return;
	}

	if (!IsValid(MatchManager))
	{
		FindMatchManager();
	}

	if (!IsValid(MatchManager))
	{
		SoccerCharacter->SetAIChasingBall(false);

		ClearPendingMainActionAfterPreparation();
		ClearCurrentRecoveryIntent();
		ClearDelayedBallObservation();
		ClearPendingGoalkeeperSaveAction();
		ClearPendingGoalkeeperSaveImpact();
		ClearGoalkeeperSweeperMode();
		ClearGoalkeeperLooseBallClaimMode();
		ClearGoalkeeperDistribution();
		ClearAerialBallInterceptionMovement();

		ReturnToHomePosition();
		return;
	}

	if (MatchManager->IsHalfTimeFieldTransitionActive())
	{
		SoccerCharacter->SetAIChasingBall(false);

		if (SoccerCharacter->IsAIPossessingBall())
		{
			SoccerCharacter->ReleaseAIBall();
		}

		SoccerCharacter->ClearAIAutoPassState();
		ClearPendingMainActionAfterPreparation();
		ClearCurrentRecoveryIntent();
		ClearDelayedBallObservation();
		ClearAIPossessionStuckTracking();
		ClearPendingGoalkeeperSaveAction();
		ClearPendingGoalkeeperSaveImpact();
		ClearGoalkeeperSweeperMode();
		ClearGoalkeeperRetreatMode();
		ClearGoalkeeperLooseBallClaimMode();
		ClearGoalkeeperDistribution();
		ClearAerialBallInterceptionMovement();
		ClearFilteredMoveRequest();
		ClearFilteredDefenseMoveRequest();
		ClearFocus(EAIFocusPriority::Gameplay);

		FVector HalfTimeMoveLocation = FVector::ZeroVector;
		float HalfTimeAcceptanceRadius = MoveAcceptanceRadius;

		if (MatchManager->GetHalfTimeFieldTransitionMoveTarget(
			SoccerCharacter,
			HalfTimeMoveLocation,
			HalfTimeAcceptanceRadius
		))
		{
			MoveToLocationWithAIMovement(
				ESoccerAIOrder::ReturnHome,
				HalfTimeMoveLocation,
				FMath::Max(20.0f, HalfTimeAcceptanceRadius),
				false
			);
		}
		else
		{
			StopMovement();
		}

		return;
	}

	if (!MatchManager->IsMatchPeriodGameplayActive())
	{
		SoccerCharacter->SetAIChasingBall(false);

		if (SoccerCharacter->IsAIPossessingBall())
		{
			SoccerCharacter->ReleaseAIBall();
		}

		SoccerCharacter->ClearAIAutoPassState();
		ClearPendingMainActionAfterPreparation();
		ClearCurrentRecoveryIntent();
		ClearDelayedBallObservation();
		ClearAIPossessionStuckTracking();
		ClearPendingGoalkeeperSaveAction();
		ClearPendingGoalkeeperSaveImpact();
		ClearGoalkeeperSweeperMode();
		ClearGoalkeeperRetreatMode();
		ClearGoalkeeperLooseBallClaimMode();
		ClearGoalkeeperDistribution();
		ClearAerialBallInterceptionMovement();
		ClearFilteredMoveRequest();
		ClearFilteredDefenseMoveRequest();
		StopMovement();
		ClearFocus(EAIFocusPriority::Gameplay);
		return;
	}

	if (
		MatchManager->GetMatchPlayState() == ESoccerMatchPlayState::KickoffSetup ||
		MatchManager->GetMatchPlayState() == ESoccerMatchPlayState::KickoffTaking
		)
	{
		SoccerCharacter->SetAIChasingBall(false);

		if (SoccerCharacter->IsAIPossessingBall())
		{
			SoccerCharacter->ReleaseAIBall();
		}

		SoccerCharacter->ClearAIAutoPassState();

		ClearPendingMainActionAfterPreparation();
		ClearCurrentRecoveryIntent();
		ClearDelayedBallObservation();
		ClearAIPossessionStuckTracking();
		ClearPendingGoalkeeperSaveAction();
		ClearPendingGoalkeeperSaveImpact();
		ClearGoalkeeperSweeperMode();
		ClearGoalkeeperLooseBallClaimMode();
		ClearGoalkeeperDistribution();

		const FVector KickoffMoveLocation =
			MatchManager->GetActiveRestartMoveLocation(SoccerCharacter);

		if (!KickoffMoveLocation.IsNearlyZero())
		{
			const bool bKickoffFinalRun =
				MatchManager->IsKickoffFinalRunActiveForCharacter(
					SoccerCharacter
				);

			const bool bKickoffTaker =
				MatchManager->IsKickoffTaker(SoccerCharacter);

			const float KickoffAcceptanceRadius =
				bKickoffTaker && !bKickoffFinalRun
				? MatchManager->GetKickoffRunUpMoveAcceptanceRadius()
				: MoveAcceptanceRadius;

			MoveToLocationWithAIMovement(
				bKickoffFinalRun
				? ESoccerAIOrder::AttackRunIntoSpace
				: ESoccerAIOrder::ReturnHome,
				KickoffMoveLocation,
				KickoffAcceptanceRadius,
				false
			);
		}

		ASoccerBall* SoccerBall =
			MatchManager->GetSoccerBall();

		if (IsValid(SoccerBall))
		{
			SetFocus(SoccerBall);
		}
		else
		{
			ClearFocus(EAIFocusPriority::Gameplay);
		}

		return;
	}

	if (
		MatchManager->IsGoalLineRestartTakerMovementLocked(
			SoccerCharacter
		)
		)
	{
		SoccerCharacter->SetAIChasingBall(false);
		SoccerCharacter->ClearAIAutoPassState();

		ClearFilteredMoveRequest();
		ClearFilteredDefenseMoveRequest();
		ClearPendingMainActionAfterPreparation();
		ClearCurrentRecoveryIntent();
		ClearDelayedBallObservation();
		ClearAIPossessionStuckTracking();
		ClearPendingGoalkeeperSaveAction();
		ClearPendingGoalkeeperSaveImpact();
		ClearGoalkeeperSweeperMode();
		ClearGoalkeeperLooseBallClaimMode();
		ClearGoalkeeperDistribution();

		StopMovement();
		ClearFocus(EAIFocusPriority::Gameplay);
		return;
	}

	if (MatchManager->IsGoalLineRestartActive())
	{
		SoccerCharacter->SetAIChasingBall(false);

		if (SoccerCharacter->IsAIPossessingBall())
		{
			SoccerCharacter->ReleaseAIBall();
		}

		SoccerCharacter->ClearAIAutoPassState();

		ClearFilteredMoveRequest();
		ClearFilteredDefenseMoveRequest();
		ClearPendingMainActionAfterPreparation();
		ClearCurrentRecoveryIntent();
		ClearDelayedBallObservation();
		ClearAIPossessionStuckTracking();
		ClearPendingGoalkeeperSaveAction();
		ClearPendingGoalkeeperSaveImpact();
		ClearGoalkeeperSweeperMode();
		ClearGoalkeeperLooseBallClaimMode();
		ClearGoalkeeperDistribution();

		const bool bTakerMovementLocked =
			MatchManager->IsGoalLineRestartTakerMovementLocked(
				SoccerCharacter
			);

		const FVector RestartMoveLocation =
			MatchManager->GetActiveRestartMoveLocation(
				SoccerCharacter
			);
		const bool bLivePositioningParticipant =
			MatchManager->HasActiveRestartLivePositioningPlan(
				SoccerCharacter
			);

		if (!bTakerMovementLocked && !RestartMoveLocation.IsNearlyZero())
		{
			const bool bGoalKickFinalRun =
				MatchManager->IsGoalKickTakerFinalApproach(
					SoccerCharacter
				);

			const bool bGoalKickTaker =
				MatchManager->IsGoalLineRestartTaker(
					SoccerCharacter
				) &&
				MatchManager->GetGoalLineRestartType() ==
					ESoccerGoalLineRestartType::GoalKick;

			const float RestartAcceptanceRadius =
				bGoalKickTaker && !bGoalKickFinalRun
				? MatchManager->GetGoalKickRunUpMoveAcceptanceRadius()
				: bLivePositioningParticipant
					? MatchManager->
						GetActiveRestartLivePositioningMoveAcceptanceRadius()
					: MoveAcceptanceRadius;

			MoveToLocationWithAIMovement(
				bGoalKickFinalRun
					? ESoccerAIOrder::AttackRunIntoSpace
					: ESoccerAIOrder::ReturnHome,
				RestartMoveLocation,
				RestartAcceptanceRadius,
				false
			);
		}
		else
		{
			StopMovement();
		}

		ASoccerBall* SoccerBall = MatchManager->GetSoccerBall();

		if (
			MatchManager->IsGoalLineRestartTaker(SoccerCharacter) &&
			IsValid(SoccerBall)
			)
		{
			SetFocus(SoccerBall);
		}
		else
		{
			ClearFocus(EAIFocusPriority::Gameplay);
		}

		return;
	}

	if (MatchManager->IsThrowInTakerAnimationLocked(SoccerCharacter))
	{
		SoccerCharacter->SetAIChasingBall(false);
		SoccerCharacter->ClearAIAutoPassState();

		ClearFilteredMoveRequest();
		ClearFilteredDefenseMoveRequest();
		ClearPendingMainActionAfterPreparation();
		ClearCurrentRecoveryIntent();
		ClearDelayedBallObservation();
		ClearAIPossessionStuckTracking();
		ClearPendingGoalkeeperSaveAction();
		ClearPendingGoalkeeperSaveImpact();
		ClearGoalkeeperSweeperMode();
		ClearGoalkeeperLooseBallClaimMode();
		ClearGoalkeeperDistribution();

		StopMovement();
		ClearFocus(EAIFocusPriority::Gameplay);
		return;
	}

	if (MatchManager->IsThrowInRestartActive())
	{
		SoccerCharacter->SetAIChasingBall(false);

		if (
			SoccerCharacter->IsAIPossessingBall() &&
			!MatchManager->IsThrowInTaker(SoccerCharacter)
			)
		{
			SoccerCharacter->ReleaseAIBall();
		}

		SoccerCharacter->ClearAIAutoPassState();

		ClearFilteredMoveRequest();
		ClearFilteredDefenseMoveRequest();
		ClearPendingMainActionAfterPreparation();
		ClearCurrentRecoveryIntent();
		ClearDelayedBallObservation();
		ClearAIPossessionStuckTracking();
		ClearPendingGoalkeeperSaveAction();
		ClearPendingGoalkeeperSaveImpact();
		ClearGoalkeeperSweeperMode();
		ClearGoalkeeperLooseBallClaimMode();
		ClearGoalkeeperDistribution();

		const FVector ThrowInMoveLocation =
			MatchManager->GetActiveRestartMoveLocation(SoccerCharacter);
		const bool bThrowInLiveOffBallPositioning =
			MatchManager->HasActiveRestartLivePositioningPlan(
				SoccerCharacter
			);
		const bool bThrowInTacticalPositioning =
			MatchManager->GetMatchPlayState() ==
				ESoccerMatchPlayState::ThrowInSetup ||
			MatchManager->IsThrowInDelayPositioningActive() ||
			bThrowInLiveOffBallPositioning;

		if (
			bThrowInTacticalPositioning &&
			!ThrowInMoveLocation.IsNearlyZero()
			)
		{
			const float ThrowInAcceptanceRadius =
				MatchManager->IsThrowInTaker(SoccerCharacter)
				? MatchManager->GetThrowInPickupMoveAcceptanceRadius()
				: bThrowInLiveOffBallPositioning
					? MatchManager->
						GetActiveRestartLivePositioningMoveAcceptanceRadius()
					: MoveAcceptanceRadius;

			MoveToLocationWithAIMovement(
				ESoccerAIOrder::ReturnHome,
				ThrowInMoveLocation,
				ThrowInAcceptanceRadius,
				false
			);
		}
		else
		{
			StopMovement();
		}

		if (
			MatchManager->IsThrowInTaker(SoccerCharacter) &&
			bThrowInTacticalPositioning
			)
		{
			ASoccerBall* SoccerBall = MatchManager->GetSoccerBall();

			if (IsValid(SoccerBall))
			{
				SetFocus(SoccerBall);
			}
			else
			{
				ClearFocus(EAIFocusPriority::Gameplay);
			}
		}
		else
		{
			ClearFocus(EAIFocusPriority::Gameplay);
		}

		return;
	}

	if (MatchManager->IsOffsideRestartActive())
	{
		SoccerCharacter->SetAIChasingBall(false);

		if (SoccerCharacter->IsAIPossessingBall())
		{
			SoccerCharacter->ReleaseAIBall();
		}

		SoccerCharacter->ClearAIAutoPassState();

		ClearFilteredMoveRequest();
		ClearFilteredDefenseMoveRequest();

		ClearPendingMainActionAfterPreparation();
		ClearCurrentRecoveryIntent();
		ClearDelayedBallObservation();
		ClearAIPossessionStuckTracking();
		ClearPendingGoalkeeperSaveAction();
		ClearPendingGoalkeeperSaveImpact();
		ClearGoalkeeperSweeperMode();
		ClearGoalkeeperLooseBallClaimMode();
		ClearGoalkeeperDistribution();

		ASoccerBall* SoccerBall =
			MatchManager->GetSoccerBall();

		const FVector RestartMoveLocation =
			MatchManager->GetActiveRestartMoveLocation(SoccerCharacter);

		if (!RestartMoveLocation.IsNearlyZero())
		{
			const bool bOffsideFinalRun =
				MatchManager->IsOffsideRestartFinalRunActiveForCharacter(
					SoccerCharacter
				);

			const bool bOffsideTaker =
				MatchManager->IsOffsideRestartTaker(SoccerCharacter);
			const bool bDefensiveWallMember =
				MatchManager->IsFreeKickDefensiveWallMember(SoccerCharacter);

			const float OffsideAcceptanceRadius =
				bDefensiveWallMember
				? MatchManager->GetFreeKickWallMoveAcceptanceRadius()
				: bOffsideTaker && !bOffsideFinalRun
				? MatchManager->GetOffsideRestartRunUpMoveAcceptanceRadius()
				: MoveAcceptanceRadius;

			MoveToLocationWithAIMovement(
				bOffsideFinalRun
					? ESoccerAIOrder::AttackRunIntoSpace
					: ESoccerAIOrder::ReturnHome,
				RestartMoveLocation,
				OffsideAcceptanceRadius,
				false
			);
		}

		if (
			MatchManager->IsOffsideRestartTaker(SoccerCharacter) ||
			MatchManager->ShouldDefendingFreeKickCharacterFaceBall(
				SoccerCharacter
			)
		)
		{
			if (IsValid(SoccerBall))
			{
				SetFocus(SoccerBall);
			}
			else
			{
				ClearFocus(EAIFocusPriority::Gameplay);
			}

			if (
				MatchManager->IsFreeKickDefensiveWallMember(SoccerCharacter) &&
				SoccerCharacter->GetVelocity().SizeSquared2D() <=
					FMath::Square(35.0f) &&
				IsValid(SoccerBall)
			)
			{
				FVector FacingDirection =
					SoccerBall->GetActorLocation() -
					SoccerCharacter->GetActorLocation();
				FacingDirection.Z = 0.0f;

				if (FacingDirection.Normalize())
				{
					SoccerCharacter->SetActorRotation(
						FacingDirection.Rotation()
					);
				}
			}
		}
		else
		{
			ASoccerCharacterBase* LastTouchCharacter =
				MatchManager->GetLastTouchCharacter();

			if (IsValid(LastTouchCharacter))
			{
				SetFocus(LastTouchCharacter);
			}
			else if (IsValid(SoccerBall))
			{
				SetFocus(SoccerBall);
			}
			else
			{
				ClearFocus(EAIFocusPriority::Gameplay);
			}
		}

		return;
	}

	if (MatchManager->IsPenaltyKickRestartActive())
	{
		SoccerCharacter->SetAIChasingBall(false);

		if (SoccerCharacter->IsAIPossessingBall())
		{
			SoccerCharacter->ReleaseAIBall();
		}

		SoccerCharacter->ClearAIAutoPassState();
		ClearFilteredMoveRequest();
		ClearFilteredDefenseMoveRequest();
		ClearPendingMainActionAfterPreparation();
		ClearCurrentRecoveryIntent();
		ClearDelayedBallObservation();
		ClearAIPossessionStuckTracking();
		ClearPendingGoalkeeperSaveAction();
		ClearPendingGoalkeeperSaveImpact();
		ClearGoalkeeperSweeperMode();
		ClearGoalkeeperLooseBallClaimMode();
		ClearGoalkeeperDistribution();

		const FVector PenaltyMoveLocation =
			MatchManager->GetActiveRestartMoveLocation(SoccerCharacter);

		if (!PenaltyMoveLocation.IsNearlyZero())
		{
			const float AcceptanceRadius =
				MatchManager->IsPenaltyKickTaker(SoccerCharacter)
				? MatchManager->GetPenaltyKickRunUpMoveAcceptanceRadius()
				: MatchManager->IsPenaltyKickDefendingGoalkeeper(SoccerCharacter)
				? MatchManager->GetPenaltyKickGoalkeeperMoveAcceptanceRadius()
				: MoveAcceptanceRadius;

			MoveToLocationWithAIMovement(
				ESoccerAIOrder::ReturnHome,
				PenaltyMoveLocation,
				AcceptanceRadius,
				false
			);
		}
		else
		{
			StopMovement();
		}

		ASoccerBall* PenaltyBall = MatchManager->GetSoccerBall();
		if (
			(
				MatchManager->IsPenaltyKickTaker(SoccerCharacter) ||
				MatchManager->IsPenaltyKickDefendingGoalkeeper(SoccerCharacter)
			) &&
			IsValid(PenaltyBall)
		)
		{
			SetFocus(PenaltyBall);
		}
		else
		{
			ClearFocus(EAIFocusPriority::Gameplay);
		}

		return;
	}

	if (MatchManager->GetMatchPlayState() != ESoccerMatchPlayState::Playing)
	{
		SoccerCharacter->SetAIChasingBall(false);

		if (SoccerCharacter->IsAIPossessingBall())
		{
			SoccerCharacter->ReleaseAIBall();
		}

		ClearPendingMainActionAfterPreparation();
		ClearCurrentRecoveryIntent();
		ClearDelayedBallObservation();
		ClearPendingGoalkeeperSaveAction();
		ClearPendingGoalkeeperSaveImpact();
		ClearGoalkeeperSweeperMode();
		ClearGoalkeeperLooseBallClaimMode();
		ClearGoalkeeperDistribution();

		ClearFocus(EAIFocusPriority::Gameplay);

		ReturnToHomePosition();

		return;
	}

	const bool bShouldFreezeThisAI =
		MatchManager->ShouldDebugFreezeAICharacter(SoccerCharacter);

	if (bShouldFreezeThisAI)
	{
		SoccerCharacter->SetAIChasingBall(false);

		if (
			MatchManager->ShouldDebugFrozenAIReleaseBall() &&
			SoccerCharacter->IsAIPossessingBall()
			)
		{
			SoccerCharacter->ReleaseAIBall();
		}

		SoccerCharacter->ClearAIAutoPassState();

		ClearPendingMainActionAfterPreparation();
		ClearCurrentRecoveryIntent();
		ClearDelayedBallObservation();
		ClearPendingGoalkeeperSaveAction();
		ClearPendingGoalkeeperSaveImpact();
		ClearGoalkeeperSweeperMode();
		ClearGoalkeeperLooseBallClaimMode();
		ClearGoalkeeperDistribution();

		StopMovement();

		ClearFocus(EAIFocusPriority::Gameplay);

		return;
	}

	if (SoccerCharacter->GetPlayerRole() == ESoccerPlayerRole::Goalkeeper)
	{
		if (UpdateGoalkeeperBehavior(SoccerCharacter, DeltaTime))
		{
			return;
		}

		ClearGoalkeeperLooseBallClaimMode();
	}
	else
	{
		ClearPendingGoalkeeperSaveAction();
		ClearPendingGoalkeeperSaveImpact();
		ClearGoalkeeperSweeperMode();
		ClearGoalkeeperRetreatMode();
		ClearGoalkeeperLooseBallClaimMode();
		ClearGoalkeeperDistribution();
	}

	if (
		SoccerCharacter->IsAIPossessingBall() ||
		SoccerCharacter->IsAIAutoPassActive()
		)
	{
		ClearDefensivePressureOvertakeState();
	}

	if (UpdateAIAutoPassFollow(SoccerCharacter))
	{
		return;
	}

	if (
		!SoccerCharacter->IsAIPossessingBall() &&
		!SoccerCharacter->IsAIAutoPassActive()
		)
	{
		ClearPendingMainActionAfterPreparation();
	}

	if (SoccerCharacter->IsAIPossessingBall())
	{
		if (ReleasePhysicalPossessionIfBallEscaped(SoccerCharacter))
		{
			ClearInitialPossessionEscape();
			ClearAIPossessionStuckTracking();
			return;
		}

		ClearPredictiveBallChaseMovement(SoccerCharacter);
		ClearAerialBallInterceptionMovement();
		SoccerCharacter->SetAIChasingBall(false);

		DebugPrintAISituation(
			SoccerCharacter,
			ESoccerAIOrder::ReturnHome
		);

		UpdateAIPossessionStuckTracking(SoccerCharacter);

		if (TryExecutePendingMainActionAfterPreparation(SoccerCharacter))
		{
			ClearInitialPossessionEscape();
			ClearAIPossessionStuckTracking();
			return;
		}

		if (TryExecuteCurrentRecoveryIntent(SoccerCharacter))
		{
			ClearInitialPossessionEscape();
			ClearAIPossessionStuckTracking();
			return;
		}

		const ESoccerFieldZone CharacterZone =
			MatchManager->GetCharacterFieldZone(SoccerCharacter);

		if (
			TryExecuteZoneBasedPossessionDecision(
				SoccerCharacter,
				CharacterZone,
				DeltaTime
			)
			)
		{
			return;
		}

		// Las decisiones normales siguen teniendo prioridad. Solo cuando
		// ninguna pudo comenzar, el poseedor intenta una salida corta si
		// hay presion cercana. El defensor conserva su logica de robo.
		if (UpdateInitialPossessionEscape(SoccerCharacter, DeltaTime))
		{
			return;
		}

		if (TryForceAIPossessionActionIfStuck(SoccerCharacter))
		{
			return;
		}

		StopMovement();

		ASoccerBall* SoccerBall = MatchManager->GetSoccerBall();

		if (IsValid(SoccerBall))
		{
			SetFocus(SoccerBall);
		}

		return;
	}
	else
	{
		ClearInitialPossessionEscape();
		ClearAIPossessionStuckTracking();
	}

	const ESoccerAIOrder CurrentOrder =
		MatchManager->GetAIOrderForCharacter(SoccerCharacter);

	if (CurrentOrder != ESoccerAIOrder::PressBall)
	{
		ClearDefensivePressureOvertakeState();
	}

	const bool bProfileDefensiveShapeOrder =
		CurrentOrder == ESoccerAIOrder::DefendProtectGoalLane ||
		CurrentOrder == ESoccerAIOrder::DefendCoverCenter ||
		CurrentOrder == ESoccerAIOrder::DefendCompactShape ||
		CurrentOrder == ESoccerAIOrder::DefendMarkDangerousReceiver;

	if (!bProfileDefensiveShapeOrder)
	{
		ClearDefensiveProfileTargetExecutionState();
	}

	if (
		TryHandleGoalAreaAttackerHoldingRespect(
			SoccerCharacter,
			CurrentOrder
		)
		)
	{
		return;
	}

	DebugPrintAISituation(SoccerCharacter, CurrentOrder);

	const bool bShouldChaseOrPress =
		CurrentOrder == ESoccerAIOrder::ChaseBall ||
		CurrentOrder == ESoccerAIOrder::PressBall ||
		CurrentOrder == ESoccerAIOrder::AttackRecoverBall;

	SoccerCharacter->SetAIChasingBall(bShouldChaseOrPress);

	if (!bShouldChaseOrPress)
	{
		ClearDelayedBallObservation();
		ClearPredictiveBallChaseMovement(SoccerCharacter);
		ClearAerialBallInterceptionMovement();

		if (
			SoccerCharacter->IsAerialActionApproaching() ||
			SoccerCharacter->IsAerialActionWaitingToStart()
		)
		{
			SoccerCharacter->CancelAerialAction();
		}

		if (!SoccerCharacter->IsAIPossessingBall())
		{
			ClearCurrentRecoveryIntent();
		}
	}
	else
	{
		ASoccerBall* ChaseBall = MatchManager->GetSoccerBall();

		if (
			IsValid(ChaseBall) &&
			TryStartContestedAITackle(
				SoccerCharacter,
				ChaseBall,
				CurrentOrder
			)
		)
		{
			return;
		}

		if (
			IsValid(ChaseBall) &&
			TryStartBasicAITackleInterception(
				SoccerCharacter,
				ChaseBall,
				CurrentOrder
			)
		)
		{
			return;
		}

		ASoccerBall* AerialBall = ChaseBall;

		if (
			IsValid(AerialBall) &&
			UpdateAerialBallInterceptionMovement(
				SoccerCharacter,
				AerialBall,
				CurrentOrder
			)
		)
		{
			return;
		}
	}

	if (CurrentOrder == ESoccerAIOrder::ChaseBall)
	{
		ClearFilteredMoveRequest();
		ClearFilteredDefenseMoveRequest();

		ASoccerBall* SoccerBall = MatchManager->GetSoccerBall();

		if (IsValid(SoccerBall))
		{
			CreateOrUpdateRecoveryIntent(SoccerCharacter, SoccerBall);

			if (TryPossessBallIfClose(SoccerCharacter, SoccerBall))
			{
				if (TryExecuteCurrentRecoveryIntent(SoccerCharacter))
				{
					ClearAIPossessionStuckTracking();
					return;
				}

				const ESoccerFieldZone CharacterZone =
					MatchManager->GetCharacterFieldZone(SoccerCharacter);

				if (
					TryExecuteZoneBasedPossessionDecision(
						SoccerCharacter,
						CharacterZone,
						DeltaTime
					)
					)
				{
					ClearAIPossessionStuckTracking();
					return;
				}

				if (TryForceAIPossessionActionIfStuck(SoccerCharacter))
				{
					return;
				}

				return;
			}

			UpdatePredictiveBallChaseMovement(
				SoccerCharacter,
				SoccerBall,
				CurrentOrder
			);

			return;
		}
	}

	if (CurrentOrder == ESoccerAIOrder::PressBall)
	{
		ClearFilteredMoveRequest();
		ClearFilteredDefenseMoveRequest();

		ASoccerBall* SoccerBall = MatchManager->GetSoccerBall();

		if (IsValid(SoccerBall))
		{
			CreateOrUpdateRecoveryIntent(SoccerCharacter, SoccerBall);

			if (TryStealBallIfClose(SoccerCharacter, SoccerBall))
			{
				ClearDefensivePressureOvertakeState();
				ClearAIPossessionStuckTracking();
				return;
			}

			if (TryPossessBallIfClose(SoccerCharacter, SoccerBall))
			{
				ClearDefensivePressureOvertakeState();

				if (TryExecuteCurrentRecoveryIntent(SoccerCharacter))
				{
					ClearAIPossessionStuckTracking();
				}

				return;
			}

			if (
				UpdateDefensivePressureOvertakeMovement(
					SoccerCharacter,
					SoccerBall
				)
				)
			{
				return;
			}

			UpdatePredictiveBallChaseMovement(
				SoccerCharacter,
				SoccerBall,
				CurrentOrder
			);

			return;
		}
	}

	if (CurrentOrder == ESoccerAIOrder::AttackRecoverBall)
	{
		ClearFilteredMoveRequest();
		ClearFilteredDefenseMoveRequest();

		ASoccerBall* SoccerBall =
			MatchManager->GetSoccerBall();

		if (IsValid(SoccerBall))
		{
			if (TryPossessBallIfClose(SoccerCharacter, SoccerBall))
			{
				ClearAIPossessionStuckTracking();
				return;
			}

			UpdatePredictiveBallChaseMovement(
				SoccerCharacter,
				SoccerBall,
				CurrentOrder
			);

			return;
		}
	}

	if (
		CurrentOrder == ESoccerAIOrder::AttackSupportShort ||
		CurrentOrder == ESoccerAIOrder::AttackSupportForward ||
		CurrentOrder == ESoccerAIOrder::AttackRunIntoSpace ||
		CurrentOrder == ESoccerAIOrder::AttackWideSupport ||
		CurrentOrder == ESoccerAIOrder::AttackRestDefense ||
		CurrentOrder == ESoccerAIOrder::AttackCompensateCover
		)
	{
		ClearFilteredDefenseMoveRequest();

		const FVector IdealAttackMoveLocation =
			CurrentOrder == ESoccerAIOrder::AttackCompensateCover
			? MatchManager->GetAttackCompensateCoverMoveLocation(SoccerCharacter)
			: MatchManager->GetAttackShapeMoveLocation(SoccerCharacter);

		const FVector AttackMoveLocation =
			ApplyAttackingProfileTargetExecution(
				SoccerCharacter,
				IdealAttackMoveLocation,
				CurrentOrder
			);

		if (!AttackMoveLocation.IsNearlyZero())
		{
			MoveToLocationFilteredForAttack(
				AttackMoveLocation,
				MoveAcceptanceRadius,
				CurrentOrder
			);
		}

		ASoccerBall* SoccerBall =
			MatchManager->GetSoccerBall();

		if (IsValid(SoccerBall))
		{
			SetFocus(SoccerBall);
		}
		else
		{
			ClearFocus(EAIFocusPriority::Gameplay);
		}

		return;
	}

	if (
		CurrentOrder == ESoccerAIOrder::DefendProtectGoalLane ||
		CurrentOrder == ESoccerAIOrder::DefendCoverCenter ||
		CurrentOrder == ESoccerAIOrder::DefendCompactShape ||
		CurrentOrder == ESoccerAIOrder::DefendMarkDangerousReceiver
		)
	{
		ClearFilteredMoveRequest();

		FVector DefensiveMoveLocation =
			MatchManager->GetDefensiveMoveLocation(SoccerCharacter);

		const bool bGoalAreaDefenseEmergency =
			MatchManager->
			IsGoalAreaDefenderCoordinationActiveForCharacter(
				SoccerCharacter
			);

		DefensiveMoveLocation = ApplyDefensiveProfileTargetExecution(
			SoccerCharacter,
			DefensiveMoveLocation,
			CurrentOrder,
			bGoalAreaDefenseEmergency
		);

		if (!DefensiveMoveLocation.IsNearlyZero())
		{
			const float DefensiveAcceptanceRadius =
				bGoalAreaDefenseEmergency
				? GoalAreaDefenseMoveAcceptanceRadius
				: MoveAcceptanceRadius;

			MoveToLocationFilteredForDefense(
				DefensiveMoveLocation,
				DefensiveAcceptanceRadius,
				CurrentOrder,
				bGoalAreaDefenseEmergency
			);

			if (bGoalAreaDefenseEmergency)
			{
				const float DistanceToEmergencyTarget =
					FVector::Dist2D(
						SoccerCharacter->GetActorLocation(),
						DefensiveMoveLocation
					);

				const ESoccerAIMovementMode EmergencyMode =
					DistanceToEmergencyTarget >=
					GoalAreaDefenseFastRunDistance
					? ESoccerAIMovementMode::FastRun
					: ESoccerAIMovementMode::Run;

				SoccerCharacter->RequestAIMovementMode(
					EmergencyMode,
					ESoccerAIMovementReason::
					DefensiveEmergencyRecovery
				);
			}
		}

		ASoccerCharacterBase* PossessingCharacter =
			MatchManager->GetPossessingCharacter();

		if (IsValid(PossessingCharacter))
		{
			SetFocus(PossessingCharacter);
		}
		else
		{
			ASoccerBall* SoccerBall =
				MatchManager->GetSoccerBall();

			if (IsValid(SoccerBall))
			{
				SetFocus(SoccerBall);
			}
			else
			{
				ClearFocus(EAIFocusPriority::Gameplay);
			}
		}

		return;
	}

	if (CurrentOrder == ESoccerAIOrder::MaintainTeamShape)
	{
		const FVector ShapeMoveLocation =
			MatchManager->GetMaintainTeamShapeMoveLocation(SoccerCharacter);

		if (!ShapeMoveLocation.IsNearlyZero())
		{
			MoveToLocationWithAIMovement(
				CurrentOrder,
				ShapeMoveLocation,
				MoveAcceptanceRadius,
				false
			);
		}

		ASoccerCharacterBase* PossessingCharacter =
			MatchManager->GetPossessingCharacter();

		if (IsValid(PossessingCharacter))
		{
			SetFocus(PossessingCharacter);
		}
		else
		{
			ASoccerBall* SoccerBall = MatchManager->GetSoccerBall();

			if (IsValid(SoccerBall))
			{
				SetFocus(SoccerBall);
			}
		}

		return;
	}

	if (MatchManager->GetMatchPlayState() == ESoccerMatchPlayState::Playing)
	{
		const FVector ShapeMoveLocation =
			MatchManager->GetMaintainTeamShapeMoveLocation(SoccerCharacter);

		if (!ShapeMoveLocation.IsNearlyZero())
		{
			MoveToLocationWithAIMovement(
				ESoccerAIOrder::MaintainTeamShape,
				ShapeMoveLocation,
				MoveAcceptanceRadius,
				false
			);
		}

		ASoccerBall* SoccerBall =
			MatchManager->GetSoccerBall();

		if (IsValid(SoccerBall))
		{
			SetFocus(SoccerBall);
		}
		else
		{
			ClearFocus(EAIFocusPriority::Gameplay);
		}

		return;
	}

	ClearFocus(EAIFocusPriority::Gameplay);

	ReturnToHomePosition();
}

void ASoccerAIController::FindMatchManager()
{
	MatchManager = nullptr;

	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return;
	}

	for (TActorIterator<ASoccerMatchManager> It(World); It; ++It)
	{
		MatchManager = *It;
		return;
	}
}

bool ASoccerAIController::TryRegisterAIKickTouchForRules(
	ASoccerAICharacter* SoccerCharacter
)
{
	if (!IsValid(SoccerCharacter))
	{
		return false;
	}

	if (!IsValid(MatchManager))
	{
		FindMatchManager();
	}

	if (!IsValid(MatchManager))
	{
		return false;
	}

	if (!MatchManager->TryRegisterIntentionalBallTouch(SoccerCharacter))
	{
		return false;
	}

	MatchManager->StartAttackRunReleaseForTeam(
		SoccerCharacter->GetTeam()
	);

	return true;
}

void ASoccerAIController::UpdateAIMovementForMove(
	ESoccerAIOrder CurrentOrder,
	const FVector& MoveLocation,
	bool bHasMoveLocation
)
{
	ASoccerAICharacter* SoccerCharacter =
		Cast<ASoccerAICharacter>(GetPawn());

	if (!IsValid(SoccerCharacter))
	{
		return;
	}

	SoccerCharacter->UpdateAIMovementModeForOrder(
		CurrentOrder,
		MoveLocation,
		bHasMoveLocation
	);
}

void ASoccerAIController::MoveToLocationWithAIMovement(
	ESoccerAIOrder CurrentOrder,
	const FVector& MoveLocation,
	float AcceptanceRadius,
	bool bStopOnOverlap
)
{
	if (MoveLocation.ContainsNaN())
	{
		return;
	}

	UpdateAIMovementForMove(
		CurrentOrder,
		MoveLocation,
		true
	);

	if (TryRecoverPawnToNavigation(CurrentOrder, MoveLocation))
	{
		return;
	}

	const EPathFollowingRequestResult::Type MoveResult =
		MoveToLocation(
			MoveLocation,
			AcceptanceRadius,
			bStopOnOverlap
		);

	if (
		MoveResult == EPathFollowingRequestResult::Failed &&
		ASoccerDebugManager::IsEnabled(this, ESoccerDebugCategory::AI) &&
		GEngine
		)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			0.20f,
			FColor::Orange,
			TEXT("AI MoveTo fallo aun estando sobre NavMesh")
		);
	}
}

void ASoccerAIController::MoveToActorWithAIMovement(
	ESoccerAIOrder CurrentOrder,
	AActor* TargetActor,
	float AcceptanceRadius,
	bool bStopOnOverlap
)
{
	if (!IsValid(TargetActor))
	{
		APawn* ControlledPawn = GetPawn();

		const FVector FallbackLocation =
			IsValid(ControlledPawn)
			? ControlledPawn->GetActorLocation()
			: FVector::ZeroVector;

		UpdateAIMovementForMove(
			CurrentOrder,
			FallbackLocation,
			false
		);

		return;
	}

	UpdateAIMovementForMove(
		CurrentOrder,
		TargetActor->GetActorLocation(),
		true
	);

	if (
		TryRecoverPawnToNavigation(
			CurrentOrder,
			TargetActor->GetActorLocation()
		)
		)
	{
		return;
	}

	MoveToActor(
		TargetActor,
		AcceptanceRadius,
		bStopOnOverlap
	);
}

bool ASoccerAIController::TryRecoverPawnToNavigation(
	ESoccerAIOrder CurrentOrder,
	const FVector& DesiredMoveLocation
)
{
	if (!bEnableNavigationRecovery)
	{
		ClearNavigationRecoveryState();
		return false;
	}

	ASoccerAICharacter* SoccerCharacter =
		Cast<ASoccerAICharacter>(GetPawn());

	UWorld* World = GetWorld();

	if (!IsValid(SoccerCharacter) || World == nullptr)
	{
		ClearNavigationRecoveryState();
		return false;
	}

	UNavigationSystemV1* NavigationSystem =
		FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);

	if (NavigationSystem == nullptr)
	{
		ClearNavigationRecoveryState();
		return false;
	}

	const FVector CurrentLocation =
		SoccerCharacter->GetActorLocation();

	const FVector ProjectionExtent(
		FMath::Max(1.0f, NavigationRecoverySearchExtentXY),
		FMath::Max(1.0f, NavigationRecoverySearchExtentXY),
		FMath::Max(1.0f, NavigationRecoverySearchExtentZ)
	);

	FNavLocation ProjectedCurrentLocation;

	const bool bProjectedCurrentLocation =
		NavigationSystem->ProjectPointToNavigation(
			CurrentLocation,
			ProjectedCurrentLocation,
			ProjectionExtent
		);

	const float DistanceToCurrentNavMesh =
		bProjectedCurrentLocation
		? FVector::Dist2D(
			CurrentLocation,
			ProjectedCurrentLocation.Location
		)
		: BIG_NUMBER;

	const ASoccerField* SoccerField =
		IsValid(MatchManager)
		? MatchManager->GetSoccerField()
		: nullptr;

	const bool bOutsidePitch =
		IsValid(SoccerField) &&
		!SoccerField->IsWorldLocationInsidePitch(CurrentLocation);

	/*
	 * Estar a pocos centimetros de un poligono no significa necesariamente
	 * que la capsula ya tenga un inicio valido para path following. Esto era
	 * especialmente fragil en la linea lateral: la proyeccion caia sobre el
	 * borde, la recuperacion terminaba y el MoveTo siguiente podia volver a
	 * fallar. Si el jugador sigue geometricamente fuera de la cancha, nunca
	 * damos por finalizada la reincorporacion solo por esa tolerancia.
	 */
	if (
		bProjectedCurrentLocation &&
		DistanceToCurrentNavMesh <= NavigationRecoveryOnMeshTolerance &&
		!bOutsidePitch
		)
	{
		ClearNavigationRecoveryState();
		return false;
	}

	if (!bNavigationRecoveryActive)
	{
		FVector NewRecoveryTarget = FVector::ZeroVector;
		bool bHasNewRecoveryTarget = false;

		/*
		 * Si la accion fisica/animacion termino fuera de las lineas, preferimos
		 * un punto claramente INTERIOR al terreno y no el poligono mas cercano.
		 * ClampWorldLocationInsidePitch conserva, en lo posible, la coordenada
		 * paralela a la linea por la que salio: un jugador que cae por un lateral
		 * vuelve a entrar practicamente por el mismo lugar.
		 */
		if (bOutsidePitch && IsValid(SoccerField))
		{
			const float SafeReentryInset = FMath::Max(
				NavigationRecoveryFieldReentryInset,
				NavigationRecoveryOnMeshTolerance * 2.0f
			);

			FVector FieldReentryTarget =
				SoccerField->ClampWorldLocationInsidePitch(
					CurrentLocation,
					SafeReentryInset
				);
			FieldReentryTarget.Z = CurrentLocation.Z;

			FNavLocation ProjectedFieldReentryTarget;
			if (
				NavigationSystem->ProjectPointToNavigation(
					FieldReentryTarget,
					ProjectedFieldReentryTarget,
					ProjectionExtent
				)
				)
			{
				NewRecoveryTarget =
					ProjectedFieldReentryTarget.Location;
				NewRecoveryTarget.Z = CurrentLocation.Z;
				bHasNewRecoveryTarget = true;
			}
			else
			{
				// El clamp ya esta dentro de la geometria reglamentaria. Mantenerlo
				// como destino manual permite que el failsafe siga teniendo un punto
				// seguro incluso si Recast tarda un frame en devolver la proyeccion.
				NewRecoveryTarget = FieldReentryTarget;
				bHasNewRecoveryTarget = true;
			}
		}
		else if (bProjectedCurrentLocation)
		{
			NewRecoveryTarget = ProjectedCurrentLocation.Location;
			bHasNewRecoveryTarget = true;
		}
		else
		{
			FNavLocation ProjectedDesiredLocation;

			if (
				NavigationSystem->ProjectPointToNavigation(
					DesiredMoveLocation,
					ProjectedDesiredLocation,
					ProjectionExtent
				)
				)
			{
				NewRecoveryTarget = ProjectedDesiredLocation.Location;
				bHasNewRecoveryTarget = true;
			}
		}

		if (!bHasNewRecoveryTarget)
		{
			return false;
		}

		NewRecoveryTarget.Z = CurrentLocation.Z;

		bNavigationRecoveryActive = true;
		NavigationRecoveryTarget = NewRecoveryTarget;
		NavigationRecoveryStartTime = World->GetTimeSeconds();
		NavigationRecoveryLastUpdateTime = NavigationRecoveryStartTime;

		if (ASoccerDebugManager::IsEnabled(this, ESoccerDebugCategory::AI) && GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				1.5f,
				FColor::Yellow,
				bOutsidePitch
				? TEXT("AI fuera del campo: reincorporacion controlada")
				: TEXT("AI fuera del NavMesh: iniciando reincorporacion")
			);
		}
	}

	StopMovement();

	UpdateAIMovementForMove(
		CurrentOrder,
		NavigationRecoveryTarget,
		true
	);

	if (UCharacterMovementComponent* Movement =
		SoccerCharacter->GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->SetMovementMode(MOVE_Walking);
	}

	FVector ToRecoveryTarget =
		NavigationRecoveryTarget - CurrentLocation;
	ToRecoveryTarget.Z = 0.0f;

	const float DistanceToRecoveryTarget =
		ToRecoveryTarget.Size();

	if (DistanceToRecoveryTarget <= NavigationRecoveryOnMeshTolerance)
	{
		ClearNavigationRecoveryState();
		return false;
	}

	const float CurrentTime = World->GetTimeSeconds();

	const bool bCanEmergencyTeleport =
		NavigationRecoveryEmergencyTeleportDelay >= 0.0f &&
		CurrentTime - NavigationRecoveryStartTime >=
			NavigationRecoveryEmergencyTeleportDelay &&
		DistanceToRecoveryTarget <=
			NavigationRecoveryEmergencyTeleportMaxDistance;

	if (bCanEmergencyTeleport)
	{
		SoccerCharacter->SetActorLocation(
			NavigationRecoveryTarget,
			false,
			nullptr,
			ETeleportType::TeleportPhysics
		);

		if (UCharacterMovementComponent* Movement =
			SoccerCharacter->GetCharacterMovement())
		{
			Movement->StopMovementImmediately();
		}

		if (ASoccerDebugManager::IsEnabled(this, ESoccerDebugCategory::AI) && GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				1.5f,
				FColor::Orange,
				TEXT("AI reincorporada al campo mediante recuperacion de emergencia")
			);
		}

		ClearNavigationRecoveryState();
		return true;
	}

	const FVector RecoveryDirection =
		ToRecoveryTarget.GetSafeNormal();

	const float RecoveryDeltaTime = FMath::Clamp(
		CurrentTime - NavigationRecoveryLastUpdateTime,
		0.0f,
		0.25f
	);

	NavigationRecoveryLastUpdateTime = CurrentTime;

	const float RecoverySpeed =
		FMath::Max(1.0f, NavigationRecoveryManualMoveSpeed);

	const float RecoveryStep = FMath::Min(
		DistanceToRecoveryTarget,
		RecoverySpeed * RecoveryDeltaTime
	);

	const FVector PreviousLocation = CurrentLocation;

	SoccerCharacter->SetActorLocation(
		CurrentLocation + RecoveryDirection * RecoveryStep,
		true,
		nullptr,
		ETeleportType::None
	);

	const FVector ActualLocation = SoccerCharacter->GetActorLocation();
	FVector ActualScriptedVelocity =
		RecoveryDeltaTime > KINDA_SMALL_NUMBER
		? (ActualLocation - PreviousLocation) / RecoveryDeltaTime
		: FVector::ZeroVector;
	ActualScriptedVelocity.Z = 0.0f;

	/*
	 * SetActorLocation no alimenta CharacterMovement::Velocity. Publicar la
	 * velocidad real de esta fase evita que el Animation Blueprint muestre
	 * idle mientras el jugador esta volviendo a la cancha.
	 */
	SoccerCharacter->SetScriptedLocomotionVelocity(
		ActualScriptedVelocity,
		ESoccerAIMovementMode::Jog,
		ESoccerAIMovementReason::NearbyReposition
	);

	if (!RecoveryDirection.IsNearlyZero())
	{
		const FRotator CurrentRotation =
			SoccerCharacter->GetActorRotation();

		const FRotator DesiredRotation =
			RecoveryDirection.Rotation();

		SoccerCharacter->SetActorRotation(
			FRotator(
				CurrentRotation.Pitch,
				DesiredRotation.Yaw,
				CurrentRotation.Roll
			)
		);
	}

	return true;
}

void ASoccerAIController::ClearNavigationRecoveryState()
{
	const bool bWasRecoveryActive = bNavigationRecoveryActive;

	bNavigationRecoveryActive = false;
	NavigationRecoveryTarget = FVector::ZeroVector;
	NavigationRecoveryStartTime = -1000.0f;
	NavigationRecoveryLastUpdateTime = -1000.0f;

	if (bWasRecoveryActive)
	{
		if (ASoccerAICharacter* SoccerCharacter =
			Cast<ASoccerAICharacter>(GetPawn()))
		{
			SoccerCharacter->ClearScriptedLocomotionVelocity();
		}
	}
}

void ASoccerAIController::ReturnToHomePosition()
{
	ASoccerAICharacter* SoccerCharacter =
		Cast<ASoccerAICharacter>(GetPawn());

	if (SoccerCharacter == nullptr)
	{
		return;
	}

	AActor* HomePositionActor =
		SoccerCharacter->GetHomePositionActor();

	if (HomePositionActor == nullptr)
	{
		SoccerCharacter->UpdateAIMovementModeForOrder(
			ESoccerAIOrder::ReturnHome,
			SoccerCharacter->GetActorLocation(),
			false
		);

		return;
	}

	FVector HomeLocation = HomePositionActor->GetActorLocation();

	if (IsValid(MatchManager))
	{
		HomeLocation = MatchManager->GetTeamRebasedFieldReferenceLocation(
			SoccerCharacter->GetTeam(),
			HomeLocation
		);
	}

	MoveToLocationWithAIMovement(
		ESoccerAIOrder::ReturnHome,
		HomeLocation,
		MoveAcceptanceRadius,
		true
	);
}

void ASoccerAIController::FacePawnTowardLocation(
	const FVector& TargetLocation,
	float DeltaTime
)
{
	APawn* ControlledPawn = GetPawn();

	if (ControlledPawn == nullptr || DeltaTime <= 0.0f)
	{
		return;
	}

	FVector ToTarget = TargetLocation - ControlledPawn->GetActorLocation();
	ToTarget.Z = 0.0f;

	if (ToTarget.IsNearlyZero())
	{
		return;
	}

	const FRotator DesiredRotation = ToTarget.Rotation();

	const FRotator NewRotation = FMath::RInterpTo(
		ControlledPawn->GetActorRotation(),
		DesiredRotation,
		DeltaTime,
		GoalkeeperTurnSpeed
	);

	ControlledPawn->SetActorRotation(
		FRotator(
			0.0f,
			NewRotation.Yaw,
			0.0f
		)
	);
}

//arquero

bool ASoccerAIController::UpdateGoalkeeperBehavior(
	ASoccerAICharacter* SoccerCharacter,
	float DeltaTime
)
{
	if (!IsValid(SoccerCharacter))
	{
		return false;
	}

	SoccerCharacter->SetAIChasingBall(false);

	ClearPendingMainActionAfterPreparation();
	ClearCurrentRecoveryIntent();
	ClearDelayedBallObservation();
	ClearAIPossessionStuckTracking();

	ASoccerBall* SoccerBall =
		IsValid(MatchManager)
		? MatchManager->GetSoccerBall()
		: nullptr;

	UpdateGoalkeeperSaveTimingDebugPanel(
		SoccerCharacter,
		SoccerBall
	);

	if (
		bGoalkeeperDebugSaveTestActive &&
		GoalkeeperDebugSaveTestCharacter ==
		SoccerCharacter
		)
	{
		StopMovement();

		if (
			SoccerCharacter->
			IsGoalkeeperActionActive()
			)
		{
			SetGoalkeeperBehaviorMode(
				ESoccerGoalkeeperBehaviorMode::Saving
			);

			if (
				bGoalkeeperDebugSaveTestAllowEmergencyBodyContact
				)
			{
				TryExecuteGoalkeeperEmergencyBodyContact(
					SoccerCharacter
				);
			}
		}

		return true;
	}

	if (
		HasPendingGoalkeeperSaveImpactFor(SoccerCharacter) &&
		!SoccerCharacter->IsGoalkeeperActionActive()
		)
	{
		ClearPendingGoalkeeperSaveImpact();
	}

	if (
		HasPendingGoalkeeperSaveActionFor(SoccerCharacter) &&
		!IsValid(SoccerBall)
		)
	{
		ClearPendingGoalkeeperSaveAction();
	}

	if (HasPendingGoalkeeperSaveActionFor(SoccerCharacter))
	{
		if (
			UpdatePendingGoalkeeperSaveAction(
				SoccerCharacter,
				SoccerBall,
				DeltaTime
			)
			)
		{
			// Una seleccion de emergencia obtiene prioridad sobre el
			// preposicionamiento normal: si desde la posicion actual ninguna
			// animacion llega, intentamos ganar terreno lateral antes del montage.
			if (
				!SoccerCharacter->IsGoalkeeperActionActive() &&
				TryUpdateGoalkeeperEmergencySaveLateralRun(
					SoccerCharacter,
					SoccerBall,
					DeltaTime
				)
				)
			{
				return true;
			}

			// Mientras ShotPending siga esperando el instante exacto del montage,
			// el selector vuelve a evaluarse cada Tick. Podemos aprovechar ese
			// intervalo para seguir alineando lateralmente al arquero; en cuanto
			// StartGoalkeeperAction() comienza la atajada, se corta el movimiento.
			if (
				!SoccerCharacter->IsGoalkeeperActionActive() &&
				bUseGoalkeeperTrajectoryPrepositioning &&
				IsValid(SoccerBall) &&
				TryUpdateGoalkeeperTrajectoryPrepositioning(
					SoccerCharacter,
					SoccerBall,
					DeltaTime
				)
				)
			{
				return true;
			}

			StopMovement();

			const bool bHoldingBallInHands =
				SoccerCharacter->IsGoalkeeperHoldingBall();

			if (
				IsValid(SoccerBall) &&
				!bHoldingBallInHands
				)
			{
				SetFocus(SoccerBall);

				if (
					ShouldGoalkeeperFaceBallDuringAction(
						SoccerCharacter
					)
					)
				{
					FacePawnTowardLocation(
						SoccerBall->GetActorLocation(),
						DeltaTime
					);
				}
			}
			else
			{
				ClearFocus(
					EAIFocusPriority::Gameplay
				);
			}

			return true;
		}
	}

	if (bGoalkeeperDistributionActive)
	{
		UpdateGoalkeeperDistribution(
			SoccerCharacter,
			DeltaTime
		);

		return true;
	}

	if (SoccerCharacter->IsGoalkeeperActionActive())
	{
		if (
			CurrentGoalkeeperBehaviorMode !=
			ESoccerGoalkeeperBehaviorMode::Smothering
			)
		{
			SetGoalkeeperBehaviorMode(
				ESoccerGoalkeeperBehaviorMode::Saving
			);
		}

		TryExecuteGoalkeeperEmergencyBodyContact(
			SoccerCharacter
		);

		StopMovement();

		const bool bHoldingBallInHands =
			SoccerCharacter->IsGoalkeeperHoldingBall();

		if (
			IsValid(SoccerBall) &&
			!bHoldingBallInHands
			)
		{
			SetFocus(SoccerBall);

			if (
				ShouldGoalkeeperFaceBallDuringAction(
					SoccerCharacter
				)
				)
			{
				FacePawnTowardLocation(
					SoccerBall->GetActorLocation(),
					DeltaTime
				);
			}
		}
		else
		{
			// Una vez atrapada, la pelota est� unida al propio arquero.
			// No debe intentar orientarse hacia ella.
			ClearFocus(
				EAIFocusPriority::Gameplay
			);
		}

		return true;
	}

	if (SoccerCharacter->IsAIPossessingBall())
	{
		/*
		 * La posesion de manos y la posesion transitoria de pies no pueden
		 * compartir el mismo flujo. Una posesion de pies que no termino de patear
		 * era capaz de caer aqui y quedar detenida para siempre.
		 */
		if (!SoccerCharacter->IsGoalkeeperHoldingBall())
		{
			ASoccerBall* FootControlledBall =
				SoccerCharacter->GetControlledAIBall();

			if (!IsValid(FootControlledBall))
			{
				FootControlledBall = SoccerBall;
			}

			if (
				IsValid(FootControlledBall) &&
				TryGoalkeeperClearBallWithFeet(
					SoccerCharacter,
					FootControlledBall
				)
				)
			{
				StopMovement();
				return true;
			}

			// Failsafe: una posesion logica de pies que no pudo completar el
			// rechazo se libera. Asi el flujo normal puede volver a acercarse.
			SoccerCharacter->ReleaseAIBall();
		}
		else
		{
			SetGoalkeeperBehaviorMode(
				ESoccerGoalkeeperBehaviorMode::HoldingBall
			);

			ClearGoalkeeperSweeperMode();
			ClearGoalkeeperLooseBallClaimMode();

			StopMovement();

			TryGoalkeeperClearCaughtBall(
				SoccerCharacter
			);

			ClearFocus(
				EAIFocusPriority::Gameplay
			);

			return true;
		}
	}

	if (
		bUseGoalkeeperShotReaction &&
		IsValid(SoccerBall)
		)
	{
		if (
			TryStartGoalkeeperSaveForIncomingShot(
				SoccerCharacter,
				SoccerBall
			)
			)
		{
			ClearGoalkeeperSweeperMode();

			// Si la primera seleccion ya es una emergencia, aprovechamos incluso
			// este primer Tick de ShotPending para comenzar la carrera lateral.
			if (
				TryUpdateGoalkeeperEmergencySaveLateralRun(
					SoccerCharacter,
					SoccerBall,
					DeltaTime
				)
				)
			{
				return true;
			}

			// TryStart... crea ShotPending; el montage todavia no comenzo.
			// Seguimos alineando al arquero desde este primer Tick para que no
			// exista un microcorte entre la anticipacion y la espera del save.
			if (
				bUseGoalkeeperTrajectoryPrepositioning &&
				TryUpdateGoalkeeperTrajectoryPrepositioning(
					SoccerCharacter,
					SoccerBall,
					DeltaTime
				)
				)
			{
				return true;
			}

			StopMovement();

			SetFocus(SoccerBall);

			if (ShouldGoalkeeperFaceBallDuringAction(SoccerCharacter))
			{
				FacePawnTowardLocation(
					SoccerBall->GetActorLocation(),
					DeltaTime
				);
			}

			return true;
		}
	}

	// Una pelota libre cuya trayectoria actual atraviesa la boca del arco se
	// reserva para la defensa del arco. Esta prioridad evita que LooseBallClaim
	// o Sweeper interpreten el remate como una pelota ganable y hagan salir al
	// arquero hacia adelante justo antes de comenzar una atajada. Si la pelota
	// cambia de direccion y deja de apuntar al arco, esta funcion devuelve false
	// en el mismo Tick y las salidas vuelven a estar disponibles normalmente.
	if (
		bUseGoalkeeperTrajectoryPrepositioning &&
		IsValid(SoccerBall) &&
		TryUpdateGoalkeeperTrajectoryPrepositioning(
			SoccerCharacter,
			SoccerBall,
			DeltaTime
		)
		)
	{
		return true;
	}

	if (
		bUseGoalkeeperLooseBallClaim &&
		IsValid(SoccerBall) &&
		TryUpdateGoalkeeperLooseBallClaimBehavior(
			SoccerCharacter,
			SoccerBall,
			DeltaTime
		)
		)
	{
		return true;
	}

	if (
		bUseGoalkeeperSweeper &&
		IsValid(SoccerBall) &&
		TryUpdateGoalkeeperSweeperBehavior(
			SoccerCharacter,
			SoccerBall,
			DeltaTime
		)
		)
	{
		return true;
	}

	if (
		bUseGoalkeeperSmartRetreat &&
		TryUpdateGoalkeeperRetreatBehavior(
			SoccerCharacter,
			SoccerBall,
			DeltaTime
		)
		)
	{
		return true;
	}

	ClearGoalkeeperSweeperMode();
	ClearGoalkeeperRetreatMode();

	SetGoalkeeperBehaviorMode(
		ESoccerGoalkeeperBehaviorMode::Positioning
	);

	const FVector IdealGoalkeeperMoveLocation =
		MatchManager->GetGoalkeeperMoveLocation(SoccerCharacter);

	const FVector GoalkeeperMoveLocation =
		ApplyGoalkeeperProfilePositioningExecution(
			SoccerCharacter,
			IdealGoalkeeperMoveLocation
		);

	if (!GoalkeeperMoveLocation.IsNearlyZero())
	{
		MoveToLocationWithAIMovement(
			ESoccerAIOrder::MaintainTeamShape,
			GoalkeeperMoveLocation,
			FMath::Max(1.0f, GoalkeeperPositioningAcceptanceRadius),
			true
		);
	}

	if (IsValid(SoccerBall))
	{
		SetFocus(SoccerBall);

		FacePawnTowardLocation(
			SoccerBall->GetActorLocation(),
			DeltaTime
		);
	}
	else
	{
		ClearFocus(EAIFocusPriority::Gameplay);
	}

	return true;
}

void ASoccerAIController::
RecordGoalkeeperSaveTimingDebug(
	ASoccerAICharacter* SoccerCharacter,
	ASoccerBall* SoccerBall,
	ESoccerGoalkeeperAction GoalkeeperAction,
	const FVector& PredictedInterventionLocation,
	float TimeToIntervention,
	float SelectedContactTime,
	float TimeUntilMontageShouldStart
)
{
	if (!bDebugGoalkeeperSaveTiming)
	{
		return;
	}

	bGoalkeeperSaveTimingDebugHasData =
		true;

	GoalkeeperSaveTimingDebugAction =
		GoalkeeperAction;

	GoalkeeperSaveTimingDebugPredictedInterventionLocation =
		PredictedInterventionLocation;

	GoalkeeperSaveTimingDebugTimeToIntervention =
		TimeToIntervention;

	GoalkeeperSaveTimingDebugSelectedContactTime =
		SelectedContactTime;

	GoalkeeperSaveTimingDebugStartDelay =
		TimeUntilMontageShouldStart;

	const float CurrentWorldTime =
		GetWorld() != nullptr
		? GetWorld()->GetTimeSeconds()
		: 0.0f;

	/*
	 * Mientras la acci�n est� pendiente, este instante
	 * se actualiza con cada nueva predicci�n.
	 *
	 * Cuando el montage comienza, deja de actualizarse
	 * y queda guardado el impacto que se esperaba en el
	 * momento de comprometer la animaci�n.
	 */
	GoalkeeperSaveTimingDebugScheduledImpactWorldTime =
		CurrentWorldTime +
		FMath::Max(
			0.0f,
			TimeToIntervention
		);

	UpdateGoalkeeperSaveTimingDebugPanel(
		SoccerCharacter,
		SoccerBall
	);
}

void ASoccerAIController::
UpdateGoalkeeperSaveTimingDebugPanel(
	ASoccerAICharacter* SoccerCharacter,
	ASoccerBall* SoccerBall
)
{
	if (
		ASoccerDebugManager::IsEnabled(this, ESoccerDebugCategory::GoalkeeperSave) &&
		bDebugGoalkeeperSaveCoordination &&
		IsValid(SoccerCharacter)
		)
	{
		UpdateGoalkeeperSaveCoordinationDebug(
			SoccerCharacter,
			SoccerBall
		);
	}
	if (
		!ASoccerDebugManager::IsEnabled(this, ESoccerDebugCategory::GoalkeeperSave) ||
		!bDebugGoalkeeperSaveTiming ||
		!bGoalkeeperSaveTimingDebugHasData ||
		!IsValid(SoccerCharacter)
		)
	{
		return;
	}

	const float CurrentWorldTime =
		GetWorld() != nullptr
		? GetWorld()->GetTimeSeconds()
		: 0.0f;

	const bool bActionActive =
		SoccerCharacter->
		IsGoalkeeperActionActive();

	const bool bActionPending =
		HasPendingGoalkeeperSaveActionFor(
			SoccerCharacter
		);

	FString PhaseText =
		TEXT("ULTIMO REGISTRO");

	FColor PhaseColor =
		FColor::White;

	if (bActionActive)
	{
		PhaseText =
			TEXT("ANIMACION ACTIVA");

		PhaseColor =
			FColor::Orange;
	}
	else if (bActionPending)
	{
		PhaseText =
			TEXT("ESPERANDO INICIO");

		PhaseColor =
			FColor::Cyan;
	}

	ESoccerGoalkeeperAction DisplayAction =
		bActionActive
		? SoccerCharacter->
		GetCurrentGoalkeeperAction()
		: GoalkeeperSaveTimingDebugAction;

	FString ActionText =
		FString::FromInt(
			static_cast<int32>(
				DisplayAction
				)
		);

	const UEnum* GoalkeeperActionEnum =
		StaticEnum<
		ESoccerGoalkeeperAction
		>();

	if (GoalkeeperActionEnum != nullptr)
	{
		ActionText =
			GoalkeeperActionEnum->
			GetDisplayNameTextByValue(
				static_cast<int64>(
					DisplayAction
					)
			).ToString();
	}

	const float ScheduledTimeRemaining =
		GoalkeeperSaveTimingDebugScheduledImpactWorldTime -
		CurrentWorldTime;

	float BallDistance2D = 0.0f;
	float BallDistance3D = 0.0f;
	float BallSpeed = 0.0f;
	float BallToPredictedPointDistance = 0.0f;

	if (IsValid(SoccerBall))
	{
		BallDistance2D =
			FVector::Dist2D(
				SoccerBall->GetActorLocation(),
				SoccerCharacter->
				GetActorLocation()
			);

		BallDistance3D =
			FVector::Dist(
				SoccerBall->GetActorLocation(),
				SoccerCharacter->
				GetActorLocation()
			);

		BallSpeed =
			SoccerBall->
			GetVelocity().
			Size();

		BallToPredictedPointDistance =
			FVector::Dist(
				SoccerBall->GetActorLocation(),
				GoalkeeperSaveTimingDebugPredictedInterventionLocation
			);
	}

	/*
	 * Volvemos a predecir incluso despu�s de que comenz�
	 * el montage.
	 *
	 * De esta manera podemos comparar:
	 *
	 * - el impacto programado al iniciar;
	 * - lo que la predicci�n cree ahora.
	 */
	FVector LivePredictedLocation =
		FVector::ZeroVector;

	float LiveTimeToIntervention =
		0.0f;

	bool bLivePredictionValid =
		false;

	if (IsValid(SoccerBall))
	{
		bLivePredictionValid =
			TryPredictBallAtGoalkeeperCandidatePlane(
				SoccerBall,
				GoalkeeperSaveTimingDebugPredictedInterventionLocation,
				LiveTimeToIntervention,
				LivePredictedLocation
			);
	}

	const float PredictionDrift =
		bLivePredictionValid
		? LiveTimeToIntervention -
		ScheduledTimeRemaining
		: 0.0f;

	FColor PredictionColor =
		FColor::Green;

	if (
		bLivePredictionValid &&
		PredictionDrift > 0.08f
		)
	{
		/*
		 * La predicci�n actual dice que falta m�s tiempo
		 * del que hab�a quedado programado.
		 *
		 * Esto indica una animaci�n anticipada.
		 */
		PredictionColor =
			FColor::Red;
	}
	else if (
		bLivePredictionValid &&
		PredictionDrift < -0.08f
		)
	{
		PredictionColor =
			FColor::Yellow;
	}

	float MontagePosition = 0.0f;
	float MontageLength = 0.0f;

	bool bHasMontageState =
		false;

	if (bActionActive)
	{
		bHasMontageState =
			SoccerCharacter->
			GetGoalkeeperActionMontagePlaybackState(
				DisplayAction,
				MontagePosition,
				MontageLength
			);
	}

	/*
	 * Posici�n ideal del montage en este instante:
	 *
	 * contacto seleccionado
	 * menos tiempo restante hasta el impacto.
	 *
	 * Al llegar la pelota:
	 *
	 * ScheduledTimeRemaining = 0
	 *
	 * por lo tanto:
	 *
	 * IdealMontagePosition =
	 * SelectedContactTime
	 */
	const float IdealMontagePosition =
		GoalkeeperSaveTimingDebugSelectedContactTime -
		ScheduledTimeRemaining;

	const float MontageSyncError =
		bHasMontageState
		? MontagePosition -
		IdealMontagePosition
		: 0.0f;

	FColor MontageColor =
		FColor::Green;

	if (
		bHasMontageState &&
		MontageSyncError > 0.05f
		)
	{
		/*
		 * Positivo:
		 * la animaci�n est� adelantada.
		 */
		MontageColor =
			FColor::Red;
	}
	else if (
		bHasMontageState &&
		MontageSyncError < -0.05f
		)
	{
		/*
		 * Negativo:
		 * la animaci�n est� atrasada.
		 */
		MontageColor =
			FColor::Yellow;
	}

	SetGoalkeeperPersistentLineIfEnabled(
		bDebugGoalkeeperSaveTiming ||
		bDebugGoalkeeperSaveCoordination ||
		bDebugGoalkeeperAnimatedContact ||
		bDebugGoalkeeperHandTrackSelector,

		this,
		ESoccerDebugCategory::GoalkeeperSave,
		300,
		TEXT("=== GK SAVE TIMING ==="),
		FColor::White,
		1.1f
	);

	SetGoalkeeperPersistentLineIfEnabled(
		bDebugGoalkeeperSaveTiming ||
		bDebugGoalkeeperSaveCoordination ||
		bDebugGoalkeeperAnimatedContact ||
		bDebugGoalkeeperHandTrackSelector,

		this,
		ESoccerDebugCategory::GoalkeeperSave,
		310,
		FString::Printf(
			TEXT("Estado: %s"),
			*PhaseText
		),
		PhaseColor
	);

	SetGoalkeeperPersistentLineIfEnabled(
		bDebugGoalkeeperSaveTiming ||
		bDebugGoalkeeperSaveCoordination ||
		bDebugGoalkeeperAnimatedContact ||
		bDebugGoalkeeperHandTrackSelector,

		this,
		ESoccerDebugCategory::GoalkeeperSave,
		320,
		FString::Printf(
			TEXT("Accion: %s"),
			*ActionText
		),
		FColor::White
	);

	SetGoalkeeperPersistentLineIfEnabled(
		bDebugGoalkeeperSaveTiming ||
		bDebugGoalkeeperSaveCoordination ||
		bDebugGoalkeeperAnimatedContact ||
		bDebugGoalkeeperHandTrackSelector,

		this,
		ESoccerDebugCategory::GoalkeeperSave,
		330,
		FString::Printf(
			TEXT(
				"Pelota -> GK: %.0f cm 2D | "
				"%.0f cm 3D | velocidad %.0f cm/s"
			),
			BallDistance2D,
			BallDistance3D,
			BallSpeed
		),
		FColor::White
	);

	SetGoalkeeperPersistentLineIfEnabled(
		bDebugGoalkeeperSaveTiming ||
		bDebugGoalkeeperSaveCoordination ||
		bDebugGoalkeeperAnimatedContact ||
		bDebugGoalkeeperHandTrackSelector,

		this,
		ESoccerDebugCategory::GoalkeeperSave,
		340,
		FString::Printf(
			TEXT(
				"Prediccion guardada: %.3f s | "
				"restante programado: %.3f s"
			),
			GoalkeeperSaveTimingDebugTimeToIntervention,
			ScheduledTimeRemaining
		),
		FColor::Cyan
	);

	SetGoalkeeperPersistentLineIfEnabled(
		bDebugGoalkeeperSaveTiming ||
		bDebugGoalkeeperSaveCoordination ||
		bDebugGoalkeeperAnimatedContact ||
		bDebugGoalkeeperHandTrackSelector,

		this,
		ESoccerDebugCategory::GoalkeeperSave,
		350,
		bLivePredictionValid
		? FString::Printf(
			TEXT(
				"Prediccion viva: %.3f s | "
				"deriva: %+.3f s"
			),
			LiveTimeToIntervention,
			PredictionDrift
		)
		: TEXT(
			"Prediccion viva: NO VALIDA"
		),
		bLivePredictionValid
		? PredictionColor
		: FColor::Yellow
	);

	SetGoalkeeperPersistentLineIfEnabled(
		bDebugGoalkeeperSaveTiming ||
		bDebugGoalkeeperSaveCoordination ||
		bDebugGoalkeeperAnimatedContact ||
		bDebugGoalkeeperHandTrackSelector,

		this,
		ESoccerDebugCategory::GoalkeeperSave,
		360,
		FString::Printf(
			TEXT(
				"Contacto elegido: %.3f s | "
				"delay calculado: %.3f s"
			),
			GoalkeeperSaveTimingDebugSelectedContactTime,
			GoalkeeperSaveTimingDebugStartDelay
		),
		FColor::Cyan
	);

	SetGoalkeeperPersistentLineIfEnabled(
		bDebugGoalkeeperSaveTiming ||
		bDebugGoalkeeperSaveCoordination ||
		bDebugGoalkeeperAnimatedContact ||
		bDebugGoalkeeperHandTrackSelector,

		this,
		ESoccerDebugCategory::GoalkeeperSave,
		370,
		bHasMontageState
		? FString::Printf(
			TEXT(
				"Montage real: %.3f / %.3f | "
				"ideal ahora: %.3f | "
				"error: %+.3f"
			),
			MontagePosition,
			MontageLength,
			IdealMontagePosition,
			MontageSyncError
		)
		: FString::Printf(
			TEXT(
				"Montage: esperando | "
				"ideal ahora: %.3f"
			),
			IdealMontagePosition
		),
		bHasMontageState
		? MontageColor
		: FColor::White
	);

	SetGoalkeeperPersistentLineIfEnabled(
		bDebugGoalkeeperSaveTiming ||
		bDebugGoalkeeperSaveCoordination ||
		bDebugGoalkeeperAnimatedContact ||
		bDebugGoalkeeperHandTrackSelector,

		this,
		ESoccerDebugCategory::GoalkeeperSave,
		380,
		FString::Printf(
			TEXT(
				"Punto previsto: "
				"X %.0f | Y %.0f | Z %.0f"
			),
			GoalkeeperSaveTimingDebugPredictedInterventionLocation.X,
			GoalkeeperSaveTimingDebugPredictedInterventionLocation.Y,
			GoalkeeperSaveTimingDebugPredictedInterventionLocation.Z
		),
		FColor::White
	);

	SetGoalkeeperPersistentLineIfEnabled(
		bDebugGoalkeeperSaveTiming ||
		bDebugGoalkeeperSaveCoordination ||
		bDebugGoalkeeperAnimatedContact ||
		bDebugGoalkeeperHandTrackSelector,

		this,
		ESoccerDebugCategory::GoalkeeperSave,
		390,
		FString::Printf(
			TEXT(
				"Pelota -> punto previsto: %.0f cm"
			),
			BallToPredictedPointDistance
		),
		FColor::White
	);
}

void ASoccerAIController::
ResetGoalkeeperSaveCoordinationDebug()
{
	bGoalkeeperSaveCoordinationDebugHasSnapshot =
		false;

	GoalkeeperSaveCoordinationDebugAction =
		ESoccerGoalkeeperAction::None;

	bGoalkeeperSaveCoordinationDebugSelectedLeftHand =
		false;

	bGoalkeeperSaveCoordinationDebugUsesTwoHandCatchZone =
		false;

	GoalkeeperSaveCoordinationDebugCatchZoneAlpha =
		0.5f;

	GoalkeeperSaveCoordinationDebugCommitWorldTime =
		-1000.0f;

	GoalkeeperSaveCoordinationDebugBallTimeToContact =
		0.0f;

	GoalkeeperSaveCoordinationDebugPredictedContactWorldTime =
		-1000.0f;

	GoalkeeperSaveCoordinationDebugSelectedContactMontageTime =
		0.0f;

	GoalkeeperSaveCoordinationDebugRequiredStartDelay =
		0.0f;

	GoalkeeperSaveCoordinationDebugPredictedBallLocation =
		FVector::ZeroVector;

	GoalkeeperSaveCoordinationDebugPredictedSelectedHandLocation =
		FVector::ZeroVector;

	GoalkeeperSaveCoordinationDebugBaseLeftHandLocation =
		FVector::ZeroVector;

	GoalkeeperSaveCoordinationDebugBaseRightHandLocation =
		FVector::ZeroVector;

	GoalkeeperSaveCoordinationDebugBaseMeshOriginLocation =
		FVector::ZeroVector;

	GoalkeeperSaveCoordinationDebugForwardDirection =
		FVector::ForwardVector;

	GoalkeeperSaveCoordinationDebugRightDirection =
		FVector::RightVector;

	bGoalkeeperSaveCoordinationDebugCapturedPredictedTime =
		false;


	GoalkeeperSaveCoordinationDebugRealBallAtPredictedTime =
		FVector::ZeroVector;

	GoalkeeperSaveCoordinationDebugRealHandAtPredictedTime =
		FVector::ZeroVector;

	bGoalkeeperSaveCoordinationDebugCapturedMontageContact =
		false;

	GoalkeeperSaveCoordinationDebugRealBallAtMontageContact =
		FVector::ZeroVector;

	GoalkeeperSaveCoordinationDebugRealHandAtMontageContact =
		FVector::ZeroVector;

	bGoalkeeperSaveCoordinationDebugLiveHandValid =
		false;

	GoalkeeperSaveCoordinationDebugLiveMontagePosition =
		0.0f;

	GoalkeeperSaveCoordinationDebugLiveHandError =
		0.0f;

	GoalkeeperSaveCoordinationDebugLivePredictedHandLocation =
		FVector::ZeroVector;

	GoalkeeperSaveCoordinationDebugLiveActualHandLocation =
		FVector::ZeroVector;

	GoalkeeperSaveCoordinationDebugContactPlaneLocation =
		FVector::ZeroVector;

	GoalkeeperSaveCoordinationDebugContactPlaneNormal =
		FVector::ForwardVector;

	bGoalkeeperSaveCoordinationDebugPlaneTrackingInitialized =
		false;

	bGoalkeeperSaveCoordinationDebugPlaneTrackingInvalidated =
		false;

	GoalkeeperSaveCoordinationDebugPreviousBallLocation =
		FVector::ZeroVector;

	GoalkeeperSaveCoordinationDebugPreviousSelectedHandLocation =
		FVector::ZeroVector;

	GoalkeeperSaveCoordinationDebugPreviousBallSignedDistance =
		0.0f;

	GoalkeeperSaveCoordinationDebugPreviousWorldTime =
		-1000.0f;

	GoalkeeperSaveCoordinationDebugPreviousMontagePosition =
		0.0f;

	bGoalkeeperSaveCoordinationDebugPreviousHadMontage =
		false;

	GoalkeeperSaveCoordinationDebugPreviousHorizontalSpeed =
		0.0f;

	bGoalkeeperSaveCoordinationDebugCapturedActualPlaneCrossing =
		false;

	GoalkeeperSaveCoordinationDebugActualCrossingElapsedTime =
		0.0f;

	GoalkeeperSaveCoordinationDebugCrossingTimeError =
		0.0f;

	GoalkeeperSaveCoordinationDebugRealBallAtPlaneCrossing =
		FVector::ZeroVector;

	GoalkeeperSaveCoordinationDebugRealHandAtPlaneCrossing =
		FVector::ZeroVector;

	GoalkeeperSaveCoordinationDebugBallErrorAtPlaneCrossing =
		0.0f;

	GoalkeeperSaveCoordinationDebugBallHandDistanceAtPlaneCrossing =
		0.0f;

	bGoalkeeperSaveCoordinationDebugHadMontageAtPlaneCrossing =
		false;

	GoalkeeperSaveCoordinationDebugMontagePositionAtPlaneCrossing =
		0.0f;

	GoalkeeperSaveCoordinationDebugMontageErrorAtPlaneCrossing =
		0.0f;

	GoalkeeperSaveCoordinationDebugCurveHandErrorAtPlaneCrossing =
		0.0f;

	GoalkeeperSaveCoordinationDebugPreviousApproachSpeed =
		0.0f;

	GoalkeeperSaveCoordinationDebugMotionSampleCount =
		0;

	GoalkeeperSaveCoordinationDebugMinReportedApproachSpeed =
		0.0f;

	GoalkeeperSaveCoordinationDebugMaxReportedApproachSpeed =
		0.0f;

	GoalkeeperSaveCoordinationDebugMinMeasuredApproachSpeed =
		0.0f;

	GoalkeeperSaveCoordinationDebugMaxMeasuredApproachSpeed =
		0.0f;

	GoalkeeperSaveCoordinationDebugMaxMotionSampleDeltaTime =
		0.0f;

	GoalkeeperSaveCoordinationDebugMaxApproachSpeedDifference =
		0.0f;

	bGoalkeeperSaveCoordinationDebugCapturedFirstMontageHandSample =
		false;

	GoalkeeperSaveCoordinationDebugFirstMontageHandSampleTime =
		0.0f;

	GoalkeeperSaveCoordinationDebugFirstMontageHandError =
		0.0f;

	GoalkeeperSaveCoordinationDebugFirstMontageHandErrorLocal =
		FVector::ZeroVector;

	GoalkeeperSaveCoordinationDebugFirstActualHandMotionLocal =
		FVector::ZeroVector;

	GoalkeeperSaveCoordinationDebugFirstCurveHandMotionLocal =
		FVector::ZeroVector;

	GoalkeeperSaveCoordinationDebugFirstCapsuleMotionLocal =
		FVector::ZeroVector;

	GoalkeeperSaveCoordinationDebugLiveHandErrorLocal =
		FVector::ZeroVector;

	GoalkeeperSaveCoordinationDebugLiveActualHandMotionLocal =
		FVector::ZeroVector;

	GoalkeeperSaveCoordinationDebugLiveCurveHandMotionLocal =
		FVector::ZeroVector;

	GoalkeeperSaveCoordinationDebugLiveCapsuleMotionLocal =
		FVector::ZeroVector;
}

bool ASoccerAIController::
TryGetGoalkeeperSaveDebugActualHands(
	const ASoccerAICharacter* SoccerCharacter,
	FVector& OutLeftHandLocation,
	FVector& OutRightHandLocation
) const
{
	OutLeftHandLocation =
		FVector::ZeroVector;

	OutRightHandLocation =
		FVector::ZeroVector;

	if (!IsValid(SoccerCharacter))
	{
		return false;
	}

	USkeletalMeshComponent* CharacterMesh =
		SoccerCharacter->GetMesh();

	if (CharacterMesh == nullptr)
	{
		return false;
	}

	static const FName LeftHandBone(
		TEXT("LeftHand")
	);

	static const FName RightHandBone(
		TEXT("RightHand")
	);

	if (
		CharacterMesh->GetBoneIndex(
			LeftHandBone
		) == INDEX_NONE ||
		CharacterMesh->GetBoneIndex(
			RightHandBone
		) == INDEX_NONE
		)
	{
		return false;
	}

	OutLeftHandLocation =
		CharacterMesh->GetBoneLocation(
			LeftHandBone,
			EBoneSpaces::WorldSpace
		);

	OutRightHandLocation =
		CharacterMesh->GetBoneLocation(
			RightHandBone,
			EBoneSpaces::WorldSpace
		);

	return true;
}

bool ASoccerAIController::
TryEvaluateGoalkeeperSaveDebugPredictedHands(
	const ASoccerAICharacter* SoccerCharacter,
	float MontageTime,
	FVector& OutPredictedLeftHandLocation,
	FVector& OutPredictedRightHandLocation
) const
{
	OutPredictedLeftHandLocation =
		FVector::ZeroVector;

	OutPredictedRightHandLocation =
		FVector::ZeroVector;

	if (
		!bGoalkeeperSaveCoordinationDebugHasSnapshot ||
		!IsValid(SoccerCharacter) ||
		GoalkeeperSaveCoordinationDebugAction ==
		ESoccerGoalkeeperAction::None
		)
	{
		return false;
	}

	const float SafeMontageTime =
		FMath::Max(
			0.0f,
			MontageTime
		);

	FVector CurrentLeftTrackPosition =
		FVector::ZeroVector;

	FVector CurrentRightTrackPosition =
		FVector::ZeroVector;

	if (
		!EvaluateGoalkeeperSaveHandTrack(
			GoalkeeperSaveCoordinationDebugAction,
			SafeMontageTime,
			CurrentLeftTrackPosition,
			CurrentRightTrackPosition
		)
		)
	{
		return false;
	}

	/*
	 * Las CurveTables contienen posiciones absolutas
	 * de las palmas respecto del origen del Armature:
	 *
	 * X = Forward
	 * Y = Lateral
	 * Z = Up
	 *
	 * Ya no restamos Track(0) ni usamos como origen
	 * la mano perteneciente al idle o a la preparaci�n.
	 */
	const FVector ScaledLeftTrackPosition =
		CurrentLeftTrackPosition *
		GoalkeeperHandTrackScale;

	const FVector ScaledRightTrackPosition =
		CurrentRightTrackPosition *
		GoalkeeperHandTrackScale;

	const FVector LeftTrackWorldOffset =
		GoalkeeperSaveCoordinationDebugForwardDirection *
		ScaledLeftTrackPosition.X
		+
		GoalkeeperSaveCoordinationDebugRightDirection *
		ScaledLeftTrackPosition.Y
		+
		FVector::UpVector *
		ScaledLeftTrackPosition.Z;

	const FVector RightTrackWorldOffset =
		GoalkeeperSaveCoordinationDebugForwardDirection *
		ScaledRightTrackPosition.X
		+
		GoalkeeperSaveCoordinationDebugRightDirection *
		ScaledRightTrackPosition.Y
		+
		FVector::UpVector *
		ScaledRightTrackPosition.Z;

	FVector2D CapsuleLocalOffset =
		FVector2D::ZeroVector;

	if (
		!SoccerCharacter->
		TryGetGoalkeeperSaveCurveMotionLocalOffset(
			GoalkeeperSaveCoordinationDebugAction,
			SafeMontageTime,
			CapsuleLocalOffset
		)
		)
	{
		return false;
	}

	const FVector CapsuleWorldDelta =
		GoalkeeperSaveCoordinationDebugForwardDirection *
		CapsuleLocalOffset.X
		+
		GoalkeeperSaveCoordinationDebugRightDirection *
		CapsuleLocalOffset.Y;

	/*
	 * El origen del Skeletal Mesh es el origen com�n del espacio exportado.
	 * Las curvas ya est�n expresadas como
	 * Forward/Lateral/Up, por eso usamos los ejes de
	 * juego congelados durante COMMIT.
	 */
	OutPredictedLeftHandLocation =
		GoalkeeperSaveCoordinationDebugBaseMeshOriginLocation +
		LeftTrackWorldOffset +
		CapsuleWorldDelta;

	OutPredictedRightHandLocation =
		GoalkeeperSaveCoordinationDebugBaseMeshOriginLocation +
		RightTrackWorldOffset +
		CapsuleWorldDelta;

	return true;
}

void ASoccerAIController::
BeginGoalkeeperSaveCoordinationDebug(
	ASoccerAICharacter* SoccerCharacter,
	ASoccerBall* SoccerBall,
	const FGoalkeeperSaveSelectionResult& Selection
)
{
	if (
		!bDebugGoalkeeperSaveCoordination ||
		!IsValid(SoccerCharacter) ||
		!IsValid(SoccerBall) ||
		!Selection.IsValid()
		)
	{
		return;
	}

	UWorld* World =
		GetWorld();

	if (World == nullptr)
	{
		return;
	}

	/*
	 * Limpiamos primero el registro anterior.
	 *
	 * Antes se guardaban las dilataciones temporales y
	 * luego ResetGoalkeeperSaveCoordinationDebug() las
	 * volv�a a colocar en 1.0.
	 */
	ResetGoalkeeperSaveCoordinationDebug();

	FVector CurrentLeftHandLocation =
		FVector::ZeroVector;

	FVector CurrentRightHandLocation =
		FVector::ZeroVector;

	if (
		!TryGetGoalkeeperSaveDebugActualHands(
			SoccerCharacter,
			CurrentLeftHandLocation,
			CurrentRightHandLocation
		)
		)
	{
		return;
	}

	USkeletalMeshComponent* CharacterMesh =
		SoccerCharacter->GetMesh();

	if (CharacterMesh == nullptr)
	{
		return;
	}

	/*
	 * Los CSV absolutos fueron exportados respecto del
	 * origen del Armature/Skeletal Mesh, no respecto de
	 * la posici�n mundial del hueso Hips.
	 *
	 * Usar Hips duplicaba aproximadamente 100 cm en Up.
	 */
	const FVector CurrentMeshOriginLocation =
		CharacterMesh->GetComponentLocation();

	FVector ForwardDirection =
		SoccerCharacter->GetActorForwardVector();

	ForwardDirection.Z =
		0.0f;

	ForwardDirection =
		ForwardDirection.GetSafeNormal();

	FVector RightDirection =
		SoccerCharacter->GetActorRightVector();

	RightDirection.Z =
		0.0f;

	RightDirection =
		RightDirection.GetSafeNormal();

	if (
		ForwardDirection.IsNearlyZero() ||
		RightDirection.IsNearlyZero()
		)
	{
		return;
	}

	bGoalkeeperSaveCoordinationDebugHasSnapshot =
		true;

	GoalkeeperSaveCoordinationDebugAction =
		Selection.Action;

	bGoalkeeperSaveCoordinationDebugSelectedLeftHand =
		Selection.bSelectedLeftHand;

	bGoalkeeperSaveCoordinationDebugUsesTwoHandCatchZone =
		IsGoalkeeperCatchAction(
			Selection.Action
		);

	if (
		bGoalkeeperSaveCoordinationDebugUsesTwoHandCatchZone
		)
	{
		const FVector PredictedHandSegment =
			Selection.PredictedRightHandLocation -
			Selection.PredictedLeftHandLocation;

		const float PredictedHandSegmentSizeSquared =
			PredictedHandSegment.SizeSquared();

		GoalkeeperSaveCoordinationDebugCatchZoneAlpha =
			PredictedHandSegmentSizeSquared >
			KINDA_SMALL_NUMBER
			? FMath::Clamp(
				FVector::DotProduct(
					Selection.PredictedSelectedHandLocation -
					Selection.PredictedLeftHandLocation,
					PredictedHandSegment
				) /
				PredictedHandSegmentSizeSquared,
				0.0f,
				1.0f
			)
			: 0.5f;
	}

	GoalkeeperSaveCoordinationDebugCommitWorldTime =
		World->GetTimeSeconds();

	GoalkeeperSaveCoordinationDebugBallTimeToContact =
		FMath::Max(
			0.0f,
			Selection.BallTimeToContactPlane
		);

	GoalkeeperSaveCoordinationDebugPredictedContactWorldTime =
		GoalkeeperSaveCoordinationDebugCommitWorldTime +
		GoalkeeperSaveCoordinationDebugBallTimeToContact;

	GoalkeeperSaveCoordinationDebugSelectedContactMontageTime =
		FMath::Max(
			0.0f,
			Selection.SelectedContactTime
		);

	GoalkeeperSaveCoordinationDebugRequiredStartDelay =
		Selection.RequiredMontageStartDelay;

	GoalkeeperSaveCoordinationDebugPredictedBallLocation =
		Selection.PredictedBallLocation;

	GoalkeeperSaveCoordinationDebugPredictedSelectedHandLocation =
		Selection.PredictedSelectedHandLocation;

	GoalkeeperSaveCoordinationDebugBaseLeftHandLocation =
		CurrentLeftHandLocation;

	GoalkeeperSaveCoordinationDebugBaseRightHandLocation =
		CurrentRightHandLocation;

	GoalkeeperSaveCoordinationDebugBaseMeshOriginLocation =
		CurrentMeshOriginLocation;

	GoalkeeperSaveCoordinationDebugForwardDirection =
		ForwardDirection;

	GoalkeeperSaveCoordinationDebugRightDirection =
		RightDirection;

	/*
 * El plano pasa por la posici�n prevista de la mano
 * elegida y es perpendicular a la direcci�n horizontal
 * de la pelota en el momento de COMMIT.
 */
	FVector BallHorizontalVelocity =
		SoccerBall->GetVelocity();

	BallHorizontalVelocity.Z =
		0.0f;

	const float CommitHorizontalSpeed =
		BallHorizontalVelocity.Size();

	const FVector ContactPlaneNormal =
		BallHorizontalVelocity.GetSafeNormal();

	if (
		ContactPlaneNormal.IsNearlyZero()
		)
	{
		ResetGoalkeeperSaveCoordinationDebug();
		return;
	}

	const float CommitApproachSpeed =
		FVector::DotProduct(
			BallHorizontalVelocity,
			ContactPlaneNormal
		);

	GoalkeeperSaveCoordinationDebugContactPlaneLocation =
		Selection.PredictedSelectedHandLocation;

	GoalkeeperSaveCoordinationDebugContactPlaneNormal =
		ContactPlaneNormal;

	const FVector CurrentBallLocation =
		SoccerBall->GetActorLocation();

	const FVector CurrentSelectedHandLocation =
		bGoalkeeperSaveCoordinationDebugUsesTwoHandCatchZone
		? FMath::Lerp(
			CurrentLeftHandLocation,
			CurrentRightHandLocation,
			GoalkeeperSaveCoordinationDebugCatchZoneAlpha
		)
		: (
			Selection.bSelectedLeftHand
			? CurrentLeftHandLocation
			: CurrentRightHandLocation
			);

	/*
 * Distancia firmada de la pelota al plano.
 *
 * Normalmente ser� negativa porque la pelota todav�a
 * se encuentra antes del plano.
 */
	const float CommitSignedDistanceToPlane =
		FVector::DotProduct(
			CurrentBallLocation -
			GoalkeeperSaveCoordinationDebugContactPlaneLocation,
			GoalkeeperSaveCoordinationDebugContactPlaneNormal
		);

	/*
	 * La convertimos en una distancia positiva hacia
	 * el plano.
	 */

	GoalkeeperSaveCoordinationDebugPreviousBallLocation =
		CurrentBallLocation;

	GoalkeeperSaveCoordinationDebugPreviousSelectedHandLocation =
		CurrentSelectedHandLocation;

	GoalkeeperSaveCoordinationDebugPreviousBallSignedDistance =
		CommitSignedDistanceToPlane;

	GoalkeeperSaveCoordinationDebugPreviousWorldTime =
		World->GetTimeSeconds();

	GoalkeeperSaveCoordinationDebugPreviousMontagePosition =
		0.0f;

	bGoalkeeperSaveCoordinationDebugPreviousHadMontage =
		false;

	GoalkeeperSaveCoordinationDebugPreviousHorizontalSpeed =
		CommitHorizontalSpeed;

	GoalkeeperSaveCoordinationDebugPreviousApproachSpeed =
		CommitApproachSpeed;

	/*
	 * La pelota normalmente debe estar antes del plano.
	 *
	 * Un peque�o valor positivo puede aparecer por la
	 * tolerancia del Tick, por eso aceptamos hasta 2 cm.
	 */
	bGoalkeeperSaveCoordinationDebugPlaneTrackingInitialized =
		GoalkeeperSaveCoordinationDebugPreviousBallSignedDistance <=
		2.0f;
}

void ASoccerAIController::
UpdateGoalkeeperSaveCoordinationDebug(
	ASoccerAICharacter* SoccerCharacter,
	ASoccerBall* SoccerBall
)
{
	if (
		!bDebugGoalkeeperSaveCoordination ||
		!bGoalkeeperSaveCoordinationDebugHasSnapshot ||
		!IsValid(SoccerCharacter)
		)
	{
		return;
	}

	UWorld* World =
		GetWorld();

	if (World == nullptr)
	{
		return;
	}

	const float CurrentWorldTime =
		World->GetTimeSeconds();

	FVector CurrentLeftHandLocation =
		FVector::ZeroVector;

	FVector CurrentRightHandLocation =
		FVector::ZeroVector;

	const bool bHasActualHands =
		TryGetGoalkeeperSaveDebugActualHands(
			SoccerCharacter,
			CurrentLeftHandLocation,
			CurrentRightHandLocation
		);

	const FVector CurrentSelectedHandLocation =
		bGoalkeeperSaveCoordinationDebugUsesTwoHandCatchZone
		? FMath::Lerp(
			CurrentLeftHandLocation,
			CurrentRightHandLocation,
			GoalkeeperSaveCoordinationDebugCatchZoneAlpha
		)
		: (
			bGoalkeeperSaveCoordinationDebugSelectedLeftHand
			? CurrentLeftHandLocation
			: CurrentRightHandLocation
			);

	float MontagePosition =
		0.0f;

	float MontageLength =
		0.0f;

	const bool bHasMontageState =
		SoccerCharacter->
		GetGoalkeeperActionMontagePlaybackState(
			GoalkeeperSaveCoordinationDebugAction,
			MontagePosition,
			MontageLength
		);

	/*
	 * EVENTO C:
	 * detectamos el primer cruce real de la pelota por
	 * el plano congelado durante COMMIT.
	 */
	if (
		bGoalkeeperSaveCoordinationDebugPlaneTrackingInitialized &&
		!bGoalkeeperSaveCoordinationDebugPlaneTrackingInvalidated &&
		!bGoalkeeperSaveCoordinationDebugCapturedActualPlaneCrossing &&
		IsValid(SoccerBall) &&
		bHasActualHands
		)
	{
		const FVector CurrentBallLocation =
			SoccerBall->GetActorLocation();

		const FVector CurrentBallVelocity =
			SoccerBall->GetVelocity();

		const float CurrentHorizontalSpeed =
			CurrentBallVelocity.Size2D();

		/*
		 * Velocidad firmada con la que la pelota avanza
		 * hacia el plano congelado.
		 *
		 * Positiva: se acerca y atraviesa el plano.
		 * Cero: se mueve paralela al plano.
		 * Negativa: se aleja del plano.
		 */
		const float CurrentApproachSpeed =
			FVector::DotProduct(
				CurrentBallVelocity,
				GoalkeeperSaveCoordinationDebugContactPlaneNormal
			);

		const float CurrentSignedDistance =
			FVector::DotProduct(
				CurrentBallLocation -
				GoalkeeperSaveCoordinationDebugContactPlaneLocation,
				GoalkeeperSaveCoordinationDebugContactPlaneNormal
			);

		const float StepDuration =
			FMath::Max(
				0.0f,
				CurrentWorldTime -
				GoalkeeperSaveCoordinationDebugPreviousWorldTime
			);

		const float BallStepDistance =
			FVector::Dist(
				GoalkeeperSaveCoordinationDebugPreviousBallLocation,
				CurrentBallLocation
			);

		const float MaximumMeasuredSpeed =
			FMath::Max(
				GoalkeeperSaveCoordinationDebugPreviousHorizontalSpeed,
				CurrentHorizontalSpeed
			);

		const float ExpectedStepDistance =
			MaximumMeasuredSpeed *
			StepDuration;

		const float MaximumReasonableStepDistance =
			FMath::Max(
				GoalkeeperSaveCoordinationMinimumTeleportDistance,
				ExpectedStepDistance * 4.0f +
				100.0f
			);

		const bool bLooksLikeTeleport =
			StepDuration > KINDA_SMALL_NUMBER &&
			BallStepDistance >
			MaximumReasonableStepDistance;

		if (bLooksLikeTeleport)
		{
			bGoalkeeperSaveCoordinationDebugPlaneTrackingInvalidated =
				true;
		}
		else
		{
			const float PreviousSignedDistance =
				GoalkeeperSaveCoordinationDebugPreviousBallSignedDistance;

			const bool bCrossedPlane =
				PreviousSignedDistance <= 0.0f &&
				CurrentSignedDistance >= 0.0f;

			/*
			 * Fracci�n del paso que pertenece al recorrido
			 * anterior al cruce. Cuando no hubo cruce,
			 * usamos el paso completo.
			 */
			float CrossingAlphaForStep =
				1.0f;

			if (bCrossedPlane)
			{
				const float SignedDistanceChange =
					CurrentSignedDistance -
					PreviousSignedDistance;

				CrossingAlphaForStep =
					SignedDistanceChange >
					KINDA_SMALL_NUMBER
					? FMath::Clamp(
						-PreviousSignedDistance /
						SignedDistanceChange,
						0.0f,
						1.0f
					)
					: 1.0f;
			}

			/*
			 * Comparamos dos velocidades para el mismo paso:
			 *
			 * INFORMADA:
			 * promedio de GetVelocity() proyectado sobre
			 * la normal del plano.
			 *
			 * MEDIDA:
			 * cambio real de distancia firmada al plano
			 * dividido por el tiempo entre muestras.
			 */
			const float EffectiveSampleDuration =
				StepDuration *
				CrossingAlphaForStep;

			const float ReportedApproachAtEffectiveEnd =
				FMath::Lerp(
					GoalkeeperSaveCoordinationDebugPreviousApproachSpeed,
					CurrentApproachSpeed,
					CrossingAlphaForStep
				);

			const float ReportedApproachForStep =
				0.5f *
				(
					GoalkeeperSaveCoordinationDebugPreviousApproachSpeed +
					ReportedApproachAtEffectiveEnd
					);

			const float EffectiveSignedDistanceChange =
				bCrossedPlane
				? -PreviousSignedDistance
				: CurrentSignedDistance -
				PreviousSignedDistance;

			const float MeasuredApproachForStep =
				EffectiveSampleDuration >
				KINDA_SMALL_NUMBER
				? EffectiveSignedDistanceChange /
				EffectiveSampleDuration
				: 0.0f;

			if (
				EffectiveSampleDuration >
				KINDA_SMALL_NUMBER
				)
			{
				const bool bFirstMotionSample =
					GoalkeeperSaveCoordinationDebugMotionSampleCount ==
					0;

				if (bFirstMotionSample)
				{
					GoalkeeperSaveCoordinationDebugMinReportedApproachSpeed =
						ReportedApproachForStep;

					GoalkeeperSaveCoordinationDebugMaxReportedApproachSpeed =
						ReportedApproachForStep;

					GoalkeeperSaveCoordinationDebugMinMeasuredApproachSpeed =
						MeasuredApproachForStep;

					GoalkeeperSaveCoordinationDebugMaxMeasuredApproachSpeed =
						MeasuredApproachForStep;
				}
				else
				{
					GoalkeeperSaveCoordinationDebugMinReportedApproachSpeed =
						FMath::Min(
							GoalkeeperSaveCoordinationDebugMinReportedApproachSpeed,
							ReportedApproachForStep
						);

					GoalkeeperSaveCoordinationDebugMaxReportedApproachSpeed =
						FMath::Max(
							GoalkeeperSaveCoordinationDebugMaxReportedApproachSpeed,
							ReportedApproachForStep
						);

					GoalkeeperSaveCoordinationDebugMinMeasuredApproachSpeed =
						FMath::Min(
							GoalkeeperSaveCoordinationDebugMinMeasuredApproachSpeed,
							MeasuredApproachForStep
						);

					GoalkeeperSaveCoordinationDebugMaxMeasuredApproachSpeed =
						FMath::Max(
							GoalkeeperSaveCoordinationDebugMaxMeasuredApproachSpeed,
							MeasuredApproachForStep
						);
				}


				GoalkeeperSaveCoordinationDebugMaxMotionSampleDeltaTime =
					FMath::Max(
						GoalkeeperSaveCoordinationDebugMaxMotionSampleDeltaTime,
						EffectiveSampleDuration
					);

				GoalkeeperSaveCoordinationDebugMaxApproachSpeedDifference =
					FMath::Max(
						GoalkeeperSaveCoordinationDebugMaxApproachSpeedDifference,
						FMath::Abs(
							MeasuredApproachForStep -
							ReportedApproachForStep
						)
					);

				++GoalkeeperSaveCoordinationDebugMotionSampleCount;
			}

			if (bCrossedPlane)
			{
				const float CrossingAlpha =
					CrossingAlphaForStep;

				const float CrossingWorldTime =
					FMath::Lerp(
						GoalkeeperSaveCoordinationDebugPreviousWorldTime,
						CurrentWorldTime,
						CrossingAlpha
					);

				const FVector CrossingBallLocation =
					FMath::Lerp(
						GoalkeeperSaveCoordinationDebugPreviousBallLocation,
						CurrentBallLocation,
						CrossingAlpha
					);

				const FVector CrossingHandLocation =
					FMath::Lerp(
						GoalkeeperSaveCoordinationDebugPreviousSelectedHandLocation,
						CurrentSelectedHandLocation,
						CrossingAlpha
					);

				const bool bHadMontageThroughoutStep =
					bGoalkeeperSaveCoordinationDebugPreviousHadMontage &&
					bHasMontageState;

				float CrossingMontagePosition =
					MontagePosition;

				if (bHadMontageThroughoutStep)
				{
					CrossingMontagePosition =
						FMath::Lerp(
							GoalkeeperSaveCoordinationDebugPreviousMontagePosition,
							MontagePosition,
							CrossingAlpha
						);
				}

				bGoalkeeperSaveCoordinationDebugCapturedActualPlaneCrossing =
					true;

				GoalkeeperSaveCoordinationDebugActualCrossingElapsedTime =
					CrossingWorldTime -
					GoalkeeperSaveCoordinationDebugCommitWorldTime;

				GoalkeeperSaveCoordinationDebugCrossingTimeError =
					GoalkeeperSaveCoordinationDebugActualCrossingElapsedTime -
					GoalkeeperSaveCoordinationDebugBallTimeToContact;

				GoalkeeperSaveCoordinationDebugRealBallAtPlaneCrossing =
					CrossingBallLocation;

				GoalkeeperSaveCoordinationDebugRealHandAtPlaneCrossing =
					CrossingHandLocation;

				GoalkeeperSaveCoordinationDebugBallErrorAtPlaneCrossing =
					FVector::Dist(
						CrossingBallLocation,
						GoalkeeperSaveCoordinationDebugPredictedBallLocation
					);

				GoalkeeperSaveCoordinationDebugBallHandDistanceAtPlaneCrossing =
					FVector::Dist(
						CrossingBallLocation,
						CrossingHandLocation
					);

				bGoalkeeperSaveCoordinationDebugHadMontageAtPlaneCrossing =
					bHasMontageState;

				GoalkeeperSaveCoordinationDebugMontagePositionAtPlaneCrossing =
					CrossingMontagePosition;

				if (bHasMontageState)
				{
					GoalkeeperSaveCoordinationDebugMontageErrorAtPlaneCrossing =
						CrossingMontagePosition -
						GoalkeeperSaveCoordinationDebugSelectedContactMontageTime;

					FVector PredictedLeftHandAtCrossing =
						FVector::ZeroVector;

					FVector PredictedRightHandAtCrossing =
						FVector::ZeroVector;

					if (
						TryEvaluateGoalkeeperSaveDebugPredictedHands(
							SoccerCharacter,
							CrossingMontagePosition,
							PredictedLeftHandAtCrossing,
							PredictedRightHandAtCrossing
						)
						)
					{
						const FVector PredictedSelectedHandAtCrossing =
							bGoalkeeperSaveCoordinationDebugUsesTwoHandCatchZone
							? FMath::Lerp(
								PredictedLeftHandAtCrossing,
								PredictedRightHandAtCrossing,
								GoalkeeperSaveCoordinationDebugCatchZoneAlpha
							)
							: (
								bGoalkeeperSaveCoordinationDebugSelectedLeftHand
								? PredictedLeftHandAtCrossing
								: PredictedRightHandAtCrossing
								);

						GoalkeeperSaveCoordinationDebugCurveHandErrorAtPlaneCrossing =
							FVector::Dist(
								PredictedSelectedHandAtCrossing,
								CrossingHandLocation
							);
					}
				}

			}

			if (
				!bGoalkeeperSaveCoordinationDebugCapturedActualPlaneCrossing
				)
			{
				GoalkeeperSaveCoordinationDebugPreviousBallLocation =
					CurrentBallLocation;

				GoalkeeperSaveCoordinationDebugPreviousSelectedHandLocation =
					CurrentSelectedHandLocation;

				GoalkeeperSaveCoordinationDebugPreviousBallSignedDistance =
					CurrentSignedDistance;

				GoalkeeperSaveCoordinationDebugPreviousWorldTime =
					CurrentWorldTime;

				GoalkeeperSaveCoordinationDebugPreviousMontagePosition =
					MontagePosition;

				bGoalkeeperSaveCoordinationDebugPreviousHadMontage =
					bHasMontageState;

				GoalkeeperSaveCoordinationDebugPreviousHorizontalSpeed =
					CurrentHorizontalSpeed;

				GoalkeeperSaveCoordinationDebugPreviousApproachSpeed =
					CurrentApproachSpeed;
			}
		}
	}

	/*
	 * Comparaci�n viva entre las curvas y el hueso real.
	 *
	 * Adem�s de la distancia total, descomponemos todo en
	 * el sistema local congelado durante COMMIT:
	 *
	 * X = Forward
	 * Y = Lateral hacia la derecha
	 * Z = Up
	 *
	 * Esto permite distinguir entre:
	 *
	 * - una base inicial equivocada;
	 * - una curva que crece con una escala incorrecta;
	 * - un movimiento de c�psula incorrecto.
	 */
	bGoalkeeperSaveCoordinationDebugLiveHandValid =
		false;

	if (
		bHasMontageState &&
		bHasActualHands
		)
	{
		FVector PredictedLeftHandLocation =
			FVector::ZeroVector;

		FVector PredictedRightHandLocation =
			FVector::ZeroVector;

		if (
			TryEvaluateGoalkeeperSaveDebugPredictedHands(
				SoccerCharacter,
				MontagePosition,
				PredictedLeftHandLocation,
				PredictedRightHandLocation
			)
			)
		{
			GoalkeeperSaveCoordinationDebugLivePredictedHandLocation =
				bGoalkeeperSaveCoordinationDebugUsesTwoHandCatchZone
				? FMath::Lerp(
					PredictedLeftHandLocation,
					PredictedRightHandLocation,
					GoalkeeperSaveCoordinationDebugCatchZoneAlpha
				)
				: (
					bGoalkeeperSaveCoordinationDebugSelectedLeftHand
					? PredictedLeftHandLocation
					: PredictedRightHandLocation
					);

			GoalkeeperSaveCoordinationDebugLiveActualHandLocation =
				CurrentSelectedHandLocation;

			const FVector BaseSelectedHandLocation =
				bGoalkeeperSaveCoordinationDebugUsesTwoHandCatchZone
				? FMath::Lerp(
					GoalkeeperSaveCoordinationDebugBaseLeftHandLocation,
					GoalkeeperSaveCoordinationDebugBaseRightHandLocation,
					GoalkeeperSaveCoordinationDebugCatchZoneAlpha
				)
				: (
					bGoalkeeperSaveCoordinationDebugSelectedLeftHand
					? GoalkeeperSaveCoordinationDebugBaseLeftHandLocation
					: GoalkeeperSaveCoordinationDebugBaseRightHandLocation
					);

			const FVector HandWorldError =
				GoalkeeperSaveCoordinationDebugLiveActualHandLocation -
				GoalkeeperSaveCoordinationDebugLivePredictedHandLocation;

			GoalkeeperSaveCoordinationDebugLiveHandError =
				HandWorldError.Size();

			GoalkeeperSaveCoordinationDebugLiveHandErrorLocal =
				FVector(
					FVector::DotProduct(
						HandWorldError,
						GoalkeeperSaveCoordinationDebugForwardDirection
					),
					FVector::DotProduct(
						HandWorldError,
						GoalkeeperSaveCoordinationDebugRightDirection
					),
					HandWorldError.Z
				);

			const FVector ActualHandWorldMotion =
				GoalkeeperSaveCoordinationDebugLiveActualHandLocation -
				BaseSelectedHandLocation;

			GoalkeeperSaveCoordinationDebugLiveActualHandMotionLocal =
				FVector(
					FVector::DotProduct(
						ActualHandWorldMotion,
						GoalkeeperSaveCoordinationDebugForwardDirection
					),
					FVector::DotProduct(
						ActualHandWorldMotion,
						GoalkeeperSaveCoordinationDebugRightDirection
					),
					ActualHandWorldMotion.Z
				);

			FVector CurrentLeftTrackPosition =
				FVector::ZeroVector;

			FVector CurrentRightTrackPosition =
				FVector::ZeroVector;

			FVector2D CapsuleLocalOffset =
				FVector2D::ZeroVector;

			const bool bHasMotionBreakdown =
				EvaluateGoalkeeperSaveHandTrack(
					GoalkeeperSaveCoordinationDebugAction,
					MontagePosition,
					CurrentLeftTrackPosition,
					CurrentRightTrackPosition
				) &&
				SoccerCharacter->
				TryGetGoalkeeperSaveCurveMotionLocalOffset(
					GoalkeeperSaveCoordinationDebugAction,
					MontagePosition,
					CapsuleLocalOffset
				);

			if (bHasMotionBreakdown)
			{
				const FVector SelectedCurrentTrackPosition =
					(
						bGoalkeeperSaveCoordinationDebugUsesTwoHandCatchZone
						? FMath::Lerp(
							CurrentLeftTrackPosition,
							CurrentRightTrackPosition,
							GoalkeeperSaveCoordinationDebugCatchZoneAlpha
						)
						: (
							bGoalkeeperSaveCoordinationDebugSelectedLeftHand
							? CurrentLeftTrackPosition
							: CurrentRightTrackPosition
							)
						) *
					GoalkeeperHandTrackScale;

				const FVector SelectedTrackWorldOffset =
					GoalkeeperSaveCoordinationDebugForwardDirection *
					SelectedCurrentTrackPosition.X
					+
					GoalkeeperSaveCoordinationDebugRightDirection *
					SelectedCurrentTrackPosition.Y
					+
					FVector::UpVector *
					SelectedCurrentTrackPosition.Z;

				const FVector PredictedHandWithoutCapsule =
					GoalkeeperSaveCoordinationDebugBaseMeshOriginLocation +
					SelectedTrackWorldOffset;

				const FVector CurveAndBaseWorldMotion =
					PredictedHandWithoutCapsule -
					BaseSelectedHandLocation;

				/*
				 * CURVA+BASE representa el movimiento previsto
				 * respecto de la mano que hab�a en COMMIT,
				 * todav�a sin sumar el movimiento de c�psula.
				 */
				GoalkeeperSaveCoordinationDebugLiveCurveHandMotionLocal =
					FVector(
						FVector::DotProduct(
							CurveAndBaseWorldMotion,
							GoalkeeperSaveCoordinationDebugForwardDirection
						),
						FVector::DotProduct(
							CurveAndBaseWorldMotion,
							GoalkeeperSaveCoordinationDebugRightDirection
						),
						CurveAndBaseWorldMotion.Z
					);

				GoalkeeperSaveCoordinationDebugLiveCapsuleMotionLocal =
					FVector(
						CapsuleLocalOffset.X,
						CapsuleLocalOffset.Y,
						0.0f
					);
			}
			else
			{
				GoalkeeperSaveCoordinationDebugLiveCurveHandMotionLocal =
					FVector::ZeroVector;

				GoalkeeperSaveCoordinationDebugLiveCapsuleMotionLocal =
					FVector::ZeroVector;
			}

			GoalkeeperSaveCoordinationDebugLiveMontagePosition =
				MontagePosition;

			bGoalkeeperSaveCoordinationDebugLiveHandValid =
				true;

			/*
			 * Congelamos la primera muestra v�lida del montage.
			 *
			 * Si el error ya es grande aqu�, la base tomada antes
			 * de iniciar el montage no coincide con la pose inicial
			 * que realmente usa la animaci�n.
			 *
			 * Si empieza cerca de cero y crece despu�s, debemos
			 * investigar escala, ejes, retargeting o c�psula.
			 */
			if (
				!bGoalkeeperSaveCoordinationDebugCapturedFirstMontageHandSample
				)
			{
				bGoalkeeperSaveCoordinationDebugCapturedFirstMontageHandSample =
					true;

				GoalkeeperSaveCoordinationDebugFirstMontageHandSampleTime =
					MontagePosition;

				GoalkeeperSaveCoordinationDebugFirstMontageHandError =
					GoalkeeperSaveCoordinationDebugLiveHandError;

				GoalkeeperSaveCoordinationDebugFirstMontageHandErrorLocal =
					GoalkeeperSaveCoordinationDebugLiveHandErrorLocal;

				GoalkeeperSaveCoordinationDebugFirstActualHandMotionLocal =
					GoalkeeperSaveCoordinationDebugLiveActualHandMotionLocal;

				GoalkeeperSaveCoordinationDebugFirstCurveHandMotionLocal =
					GoalkeeperSaveCoordinationDebugLiveCurveHandMotionLocal;

				GoalkeeperSaveCoordinationDebugFirstCapsuleMotionLocal =
					GoalkeeperSaveCoordinationDebugLiveCapsuleMotionLocal;
			}
		}
	}

	/*
	 * EVENTO A:
	 * lleg� el instante mundial que se hab�a previsto
	 * para el contacto con la pelota.
	 */
	if (
		!bGoalkeeperSaveCoordinationDebugCapturedPredictedTime &&
		CurrentWorldTime >=
		GoalkeeperSaveCoordinationDebugPredictedContactWorldTime &&
		IsValid(SoccerBall) &&
		bHasActualHands
		)
	{
		bGoalkeeperSaveCoordinationDebugCapturedPredictedTime =
			true;

		GoalkeeperSaveCoordinationDebugRealBallAtPredictedTime =
			SoccerBall->GetActorLocation();

		GoalkeeperSaveCoordinationDebugRealHandAtPredictedTime =
			CurrentSelectedHandLocation;


	}

	/*
	 * EVENTO B:
	 * el montage alcanz� el tiempo de contacto elegido.
	 */
	if (
		!bGoalkeeperSaveCoordinationDebugCapturedMontageContact &&
		bHasMontageState &&
		MontagePosition >=
		GoalkeeperSaveCoordinationDebugSelectedContactMontageTime &&
		IsValid(SoccerBall) &&
		bHasActualHands
		)
	{
		bGoalkeeperSaveCoordinationDebugCapturedMontageContact =
			true;

		GoalkeeperSaveCoordinationDebugRealBallAtMontageContact =
			SoccerBall->GetActorLocation();

		GoalkeeperSaveCoordinationDebugRealHandAtMontageContact =
			CurrentSelectedHandLocation;

	}

	/*
	 * Los marcadores no son persistentes.
	 * Se vuelven a dibujar en cada Tick y no se acumulan.
	 */
	const float PredictedRadius =
		GoalkeeperSaveCoordinationPredictedSphereRadius;

	const float ActualRadius =
		GoalkeeperSaveCoordinationActualSphereRadius;

	const float Thickness =
		GoalkeeperSaveCoordinationLineThickness;

	/*
	 * Predicci�n congelada en COMMIT.
	 *
	 * Amarillo: pelota prevista.
	 * Verde: mano prevista.
	 */
	DrawDebugSphere(
		World,
		GoalkeeperSaveCoordinationDebugPredictedBallLocation,
		PredictedRadius,
		16,
		FColor::Yellow,
		false,
		0.0f,
		0,
		Thickness
	);

	DrawDebugSphere(
		World,
		GoalkeeperSaveCoordinationDebugPredictedSelectedHandLocation,
		PredictedRadius,
		16,
		FColor::Green,
		false,
		0.0f,
		0,
		Thickness
	);

	DrawDebugLine(
		World,
		GoalkeeperSaveCoordinationDebugPredictedBallLocation,
		GoalkeeperSaveCoordinationDebugPredictedSelectedHandLocation,
		FColor::White,
		false,
		0.0f,
		0,
		Thickness
	);

	/*
	 * Resultado real en el instante mundial previsto.
	 *
	 * Naranja: pelota real.
	 * Gris: mano real.
	 */
	if (
		bGoalkeeperSaveCoordinationDebugCapturedPredictedTime
		)
	{
		DrawDebugSphere(
			World,
			GoalkeeperSaveCoordinationDebugRealBallAtPredictedTime,
			ActualRadius,
			12,
			FColor(255, 165, 0),
			false,
			0.0f,
			0,
			Thickness
		);

		DrawDebugSphere(
			World,
			GoalkeeperSaveCoordinationDebugRealHandAtPredictedTime,
			ActualRadius,
			12,
			FColor(190, 190, 190),
			false,
			0.0f,
			0,
			Thickness
		);
	}

	/*
	 * Resultado real cuando el montage alcanz�
	 * el contacto seleccionado.
	 *
	 * Rojo: pelota real.
	 * Celeste: mano real.
	 */
	if (
		bGoalkeeperSaveCoordinationDebugCapturedMontageContact
		)
	{
		DrawDebugSphere(
			World,
			GoalkeeperSaveCoordinationDebugRealBallAtMontageContact,
			ActualRadius,
			12,
			FColor::Red,
			false,
			0.0f,
			0,
			Thickness
		);

		DrawDebugSphere(
			World,
			GoalkeeperSaveCoordinationDebugRealHandAtMontageContact,
			ActualRadius,
			12,
			FColor::Cyan,
			false,
			0.0f,
			0,
			Thickness
		);
	}

	/*
	 * EVENTO C:
	 *
	 * Violeta:
	 * posici�n real de la pelota al cruzar el plano.
	 *
	 * Turquesa:
	 * posici�n real de la mano elegida en ese instante.
	 */
	if (
		bGoalkeeperSaveCoordinationDebugCapturedActualPlaneCrossing
		)
	{
		const FColor CrossingBallColor(
			170,
			50,
			255
		);

		const FColor CrossingHandColor(
			40,
			230,
			200
		);

		DrawDebugSphere(
			World,
			GoalkeeperSaveCoordinationDebugRealBallAtPlaneCrossing,
			ActualRadius * 1.15f,
			16,
			CrossingBallColor,
			false,
			0.0f,
			0,
			Thickness
		);

		DrawDebugSphere(
			World,
			GoalkeeperSaveCoordinationDebugRealHandAtPlaneCrossing,
			ActualRadius,
			14,
			CrossingHandColor,
			false,
			0.0f,
			0,
			Thickness
		);

		DrawDebugLine(
			World,
			GoalkeeperSaveCoordinationDebugRealBallAtPlaneCrossing,
			GoalkeeperSaveCoordinationDebugRealHandAtPlaneCrossing,
			CrossingBallColor,
			false,
			0.0f,
			0,
			Thickness
		);
	}

	/*
	 * Cruz que representa el plano de contacto congelado.
	 *
	 * La cruz queda orientada dentro del plano:
	 * un eje lateral y otro vertical.
	 */
	{
		FVector PlaneRightDirection =
			FVector::CrossProduct(
				FVector::UpVector,
				GoalkeeperSaveCoordinationDebugContactPlaneNormal
			).GetSafeNormal();

		if (!PlaneRightDirection.IsNearlyZero())
		{
			const float PlaneMarkerHalfSize =
				80.0f;

			DrawDebugLine(
				World,
				GoalkeeperSaveCoordinationDebugContactPlaneLocation -
				PlaneRightDirection *
				PlaneMarkerHalfSize,
				GoalkeeperSaveCoordinationDebugContactPlaneLocation +
				PlaneRightDirection *
				PlaneMarkerHalfSize,
				FColor::Yellow,
				false,
				0.0f,
				0,
				1.5f
			);

			DrawDebugLine(
				World,
				GoalkeeperSaveCoordinationDebugContactPlaneLocation -
				FVector::UpVector *
				PlaneMarkerHalfSize,
				GoalkeeperSaveCoordinationDebugContactPlaneLocation +
				FVector::UpVector *
				PlaneMarkerHalfSize,
				FColor::Yellow,
				false,
				0.0f,
				0,
				1.5f
			);
		}
	}

	/*
	 * Comparaci�n viva en el mismo MontagePosition.
	 *
	 * Magenta: mano calculada mediante las curvas.
	 * Azul: hueso real.
	 */
	if (
		bGoalkeeperSaveCoordinationDebugLiveHandValid
		)
	{
		DrawDebugSphere(
			World,
			GoalkeeperSaveCoordinationDebugLivePredictedHandLocation,
			ActualRadius * 0.8f,
			10,
			FColor::Magenta,
			false,
			0.0f,
			0,
			Thickness
		);

		DrawDebugSphere(
			World,
			GoalkeeperSaveCoordinationDebugLiveActualHandLocation,
			ActualRadius * 0.65f,
			10,
			FColor::Blue,
			false,
			0.0f,
			0,
			Thickness
		);

		DrawDebugLine(
			World,
			GoalkeeperSaveCoordinationDebugLivePredictedHandLocation,
			GoalkeeperSaveCoordinationDebugLiveActualHandLocation,
			FColor::Magenta,
			false,
			0.0f,
			0,
			Thickness
		);
	}

	const UEnum* ActionEnum =
		StaticEnum<ESoccerGoalkeeperAction>();

	FString ActionText =
		FString::FromInt(
			static_cast<int32>(
				GoalkeeperSaveCoordinationDebugAction
				)
		);

	if (ActionEnum != nullptr)
	{
		ActionText =
			ActionEnum->
			GetDisplayNameTextByValue(
				static_cast<int64>(
					GoalkeeperSaveCoordinationDebugAction
					)
			).ToString();
	}

	const float FrozenSpatialError =
		FVector::Dist(
			GoalkeeperSaveCoordinationDebugPredictedBallLocation,
			GoalkeeperSaveCoordinationDebugPredictedSelectedHandLocation
		);

	SetGoalkeeperPersistentLineIfEnabled(
		bDebugGoalkeeperSaveTiming ||
		bDebugGoalkeeperSaveCoordination ||
		bDebugGoalkeeperAnimatedContact ||
		bDebugGoalkeeperHandTrackSelector,

		this,
		ESoccerDebugCategory::GoalkeeperSave,
		500,
		TEXT("=== GK SAVE COORDINATION ==="),
		FColor::White,
		1.1f
	);

	const FString CoordinationContactTargetText =
		bGoalkeeperSaveCoordinationDebugUsesTwoHandCatchZone
		? TEXT("zona MANOS")
		: FString::Printf(
			TEXT("mano %s"),
			bGoalkeeperSaveCoordinationDebugSelectedLeftHand
			? TEXT("IZQ")
			: TEXT("DER")
		);

	SetGoalkeeperPersistentLineIfEnabled(
		bDebugGoalkeeperSaveTiming ||
		bDebugGoalkeeperSaveCoordination ||
		bDebugGoalkeeperAnimatedContact ||
		bDebugGoalkeeperHandTrackSelector,

		this,
		ESoccerDebugCategory::GoalkeeperSave,
		510,
		FString::Printf(
			TEXT(
				"COMMIT: %s | %s | "
				"contacto %.3f | pelota %.3f | delay %+.3f"
			),
			*ActionText,
			*CoordinationContactTargetText,
			GoalkeeperSaveCoordinationDebugSelectedContactMontageTime,
			GoalkeeperSaveCoordinationDebugBallTimeToContact,
			GoalkeeperSaveCoordinationDebugRequiredStartDelay
		),
		FColor::Cyan
	);

	SetGoalkeeperPersistentLineIfEnabled(
		bDebugGoalkeeperSaveTiming ||
		bDebugGoalkeeperSaveCoordination ||
		bDebugGoalkeeperAnimatedContact ||
		bDebugGoalkeeperHandTrackSelector,

		this,
		ESoccerDebugCategory::GoalkeeperSave,
		520,
		FString::Printf(
			TEXT(
				"Error espacial elegido: pelota-%s %.1f cm"
			),
			bGoalkeeperSaveCoordinationDebugUsesTwoHandCatchZone
			? TEXT("zona")
			: TEXT("mano"),
			FrozenSpatialError
		),
		FColor::White
	);

	const FString ActualCrossingStatusText =
		bGoalkeeperSaveCoordinationDebugCapturedActualPlaneCrossing
		? FString::Printf(
			TEXT(
				"Cruce real: previsto %.3f s | "
				"real %.3f s | error %+.3f s"
			),
			GoalkeeperSaveCoordinationDebugBallTimeToContact,
			GoalkeeperSaveCoordinationDebugActualCrossingElapsedTime,
			GoalkeeperSaveCoordinationDebugCrossingTimeError
		)
		: (
			bGoalkeeperSaveCoordinationDebugPlaneTrackingInvalidated
			? TEXT(
				"Cruce real: seguimiento invalidado "
				"por salto o reinicio de pelota"
			)
			: (
				bGoalkeeperSaveCoordinationDebugPlaneTrackingInitialized
				? TEXT(
					"Cruce real: esperando que la pelota "
					"alcance el plano"
				)
				: TEXT(
					"Cruce real: seguimiento no iniciado; "
					"la pelota ya estaba delante del plano"
				)
				)
			);

	SetGoalkeeperPersistentLineIfEnabled(
		bDebugGoalkeeperSaveTiming ||
		bDebugGoalkeeperSaveCoordination ||
		bDebugGoalkeeperAnimatedContact ||
		bDebugGoalkeeperHandTrackSelector,

		this,
		ESoccerDebugCategory::GoalkeeperSave,
		570,
		ActualCrossingStatusText,
		bGoalkeeperSaveCoordinationDebugCapturedActualPlaneCrossing
		? (
			FMath::Abs(
				GoalkeeperSaveCoordinationDebugCrossingTimeError
			) <= 0.05f
			? FColor::Green
			: FColor::Red
			)
		: FColor::White
	);

	SetGoalkeeperPersistentLineIfEnabled(
		bDebugGoalkeeperSaveTiming ||
		bDebugGoalkeeperSaveCoordination ||
		bDebugGoalkeeperAnimatedContact ||
		bDebugGoalkeeperHandTrackSelector,

		this,
		ESoccerDebugCategory::GoalkeeperSave,
		580,
		bGoalkeeperSaveCoordinationDebugCapturedActualPlaneCrossing
		? FString::Printf(
			TEXT(
				"En cruce real: error pelota %.1f cm | "
				"pelota-mano %.1f cm"
			),
			GoalkeeperSaveCoordinationDebugBallErrorAtPlaneCrossing,
			GoalkeeperSaveCoordinationDebugBallHandDistanceAtPlaneCrossing
		)
		: TEXT(
			"En cruce real: evento aun no capturado"
		),
		bGoalkeeperSaveCoordinationDebugCapturedActualPlaneCrossing
		? FColor::Cyan
		: FColor::White
	);

	SetGoalkeeperPersistentLineIfEnabled(
		bDebugGoalkeeperSaveTiming ||
		bDebugGoalkeeperSaveCoordination ||
		bDebugGoalkeeperAnimatedContact ||
		bDebugGoalkeeperHandTrackSelector,

		this,
		ESoccerDebugCategory::GoalkeeperSave,
		590,
		bGoalkeeperSaveCoordinationDebugCapturedActualPlaneCrossing
		? (
			bGoalkeeperSaveCoordinationDebugHadMontageAtPlaneCrossing
			? FString::Printf(
				TEXT(
					"En cruce real: montage %.3f | "
					"error contacto %+.3f s | "
					"curva-hueso %.1f cm"
				),
				GoalkeeperSaveCoordinationDebugMontagePositionAtPlaneCrossing,
				GoalkeeperSaveCoordinationDebugMontageErrorAtPlaneCrossing,
				GoalkeeperSaveCoordinationDebugCurveHandErrorAtPlaneCrossing
			)
			: TEXT(
				"En cruce real: montage NO ACTIVO"
			)
			)
		: TEXT(
			"En cruce real: esperando datos de montage"
		),
		bGoalkeeperSaveCoordinationDebugCapturedActualPlaneCrossing
		? (
			bGoalkeeperSaveCoordinationDebugHadMontageAtPlaneCrossing
			? FColor::Yellow
			: FColor::Red
			)
		: FColor::White
	);

	bool bHasCrossingCurveHandLocalError =
		false;

	FVector CrossingCurveHandErrorLocal =
		FVector::ZeroVector;

	if (
		bGoalkeeperSaveCoordinationDebugCapturedActualPlaneCrossing &&
		bGoalkeeperSaveCoordinationDebugHadMontageAtPlaneCrossing
		)
	{
		FVector PredictedLeftHandAtCrossing =
			FVector::ZeroVector;

		FVector PredictedRightHandAtCrossing =
			FVector::ZeroVector;

		if (
			TryEvaluateGoalkeeperSaveDebugPredictedHands(
				SoccerCharacter,
				GoalkeeperSaveCoordinationDebugMontagePositionAtPlaneCrossing,
				PredictedLeftHandAtCrossing,
				PredictedRightHandAtCrossing
			)
			)
		{
			const FVector PredictedSelectedHandAtCrossing =
				bGoalkeeperSaveCoordinationDebugSelectedLeftHand
				? PredictedLeftHandAtCrossing
				: PredictedRightHandAtCrossing;

			const FVector CrossingCurveHandWorldError =
				GoalkeeperSaveCoordinationDebugRealHandAtPlaneCrossing -
				PredictedSelectedHandAtCrossing;

			CrossingCurveHandErrorLocal =
				FVector(
					FVector::DotProduct(
						CrossingCurveHandWorldError,
						GoalkeeperSaveCoordinationDebugForwardDirection
					),
					FVector::DotProduct(
						CrossingCurveHandWorldError,
						GoalkeeperSaveCoordinationDebugRightDirection
					),
					CrossingCurveHandWorldError.Z
				);

			bHasCrossingCurveHandLocalError =
				true;
		}
	}

	/*
	 * Deltas XYZ mundiales ocultos:
	 * pueden confundir seg�n el arco defendido.
	 */

	/*
	 * Diagn�stico de la base inicial y de los ejes locales.
	 *
	 * Los valores son Actual - Previsto:
	 *
	 * Forward positivo: la mano real qued� m�s adelante.
	 * Forward negativo: la mano real qued� m�s atr�s.
	 */
	SetGoalkeeperPersistentLineIfEnabled(
		bDebugGoalkeeperSaveTiming ||
		bDebugGoalkeeperSaveCoordination ||
		bDebugGoalkeeperAnimatedContact ||
		bDebugGoalkeeperHandTrackSelector,

		this,
		ESoccerDebugCategory::GoalkeeperSave,
		530,
		bGoalkeeperSaveCoordinationDebugCapturedFirstMontageHandSample
		? FString::Printf(
			TEXT(
				"Primera mano t %.3f | error %.1f | "
				"F %+.1f L %+.1f U %+.1f"
			),
			GoalkeeperSaveCoordinationDebugFirstMontageHandSampleTime,
			GoalkeeperSaveCoordinationDebugFirstMontageHandError,
			GoalkeeperSaveCoordinationDebugFirstMontageHandErrorLocal.X,
			GoalkeeperSaveCoordinationDebugFirstMontageHandErrorLocal.Y,
			GoalkeeperSaveCoordinationDebugFirstMontageHandErrorLocal.Z
		)
		: TEXT(
			"Primera mano del montage: esperando"
		),
		bGoalkeeperSaveCoordinationDebugCapturedFirstMontageHandSample
		? (
			GoalkeeperSaveCoordinationDebugFirstMontageHandError <= 8.0f
			? FColor::Green
			: FColor::Red
			)
		: FColor::White
	);

	SetGoalkeeperPersistentLineIfEnabled(
		bDebugGoalkeeperSaveTiming ||
		bDebugGoalkeeperSaveCoordination ||
		bDebugGoalkeeperAnimatedContact ||
		bDebugGoalkeeperHandTrackSelector,

		this,
		ESoccerDebugCategory::GoalkeeperSave,
		540,
		bGoalkeeperSaveCoordinationDebugCapturedFirstMontageHandSample
		? FString::Printf(
			TEXT(
				"Primera muestra movimiento REAL: "
				"F %+.1f L %+.1f U %+.1f"
			),
			GoalkeeperSaveCoordinationDebugFirstActualHandMotionLocal.X,
			GoalkeeperSaveCoordinationDebugFirstActualHandMotionLocal.Y,
			GoalkeeperSaveCoordinationDebugFirstActualHandMotionLocal.Z
		)
		: TEXT(
			"Primera muestra movimiento REAL: esperando"
		),
		FColor::Cyan
	);

	SetGoalkeeperPersistentLineIfEnabled(
		bDebugGoalkeeperSaveTiming ||
		bDebugGoalkeeperSaveCoordination ||
		bDebugGoalkeeperAnimatedContact ||
		bDebugGoalkeeperHandTrackSelector,

		this,
		ESoccerDebugCategory::GoalkeeperSave,
		550,
		bGoalkeeperSaveCoordinationDebugCapturedFirstMontageHandSample
		? FString::Printf(
			TEXT(
				"Primera muestra CURVA+BASE F/L/U %+.1f/%+.1f/%+.1f | "
				"CAPS F/L %+.1f/%+.1f"
			),
			GoalkeeperSaveCoordinationDebugFirstCurveHandMotionLocal.X,
			GoalkeeperSaveCoordinationDebugFirstCurveHandMotionLocal.Y,
			GoalkeeperSaveCoordinationDebugFirstCurveHandMotionLocal.Z,
			GoalkeeperSaveCoordinationDebugFirstCapsuleMotionLocal.X,
			GoalkeeperSaveCoordinationDebugFirstCapsuleMotionLocal.Y
		)
		: TEXT(
			"Primera muestra CURVA+BASE/CAPS: esperando"
		),
		FColor::Magenta
	);

	SetGoalkeeperPersistentLineIfEnabled(
		bDebugGoalkeeperSaveTiming ||
		bDebugGoalkeeperSaveCoordination ||
		bDebugGoalkeeperAnimatedContact ||
		bDebugGoalkeeperHandTrackSelector,

		this,
		ESoccerDebugCategory::GoalkeeperSave,
		560,
		bHasCrossingCurveHandLocalError
		? FString::Printf(
			TEXT(
				"Curva-hueso local en cruce: "
				"F %+.1f L %+.1f U %+.1f"
			),
			CrossingCurveHandErrorLocal.X,
			CrossingCurveHandErrorLocal.Y,
			CrossingCurveHandErrorLocal.Z
		)
		: TEXT(
			"Curva-hueso local en cruce: esperando"
		),
		bHasCrossingCurveHandLocalError
		? FColor::Orange
		: FColor::White
	);

	SetGoalkeeperPersistentLineIfEnabled(
		bDebugGoalkeeperSaveTiming ||
		bDebugGoalkeeperSaveCoordination ||
		bDebugGoalkeeperAnimatedContact ||
		bDebugGoalkeeperHandTrackSelector,

		this,
		ESoccerDebugCategory::GoalkeeperSave,
		565,
		bGoalkeeperSaveCoordinationDebugLiveHandValid
		? FString::Printf(
			TEXT(
				"Mano viva t %.3f | error local "
				"F %+.1f L %+.1f U %+.1f"
			),
			GoalkeeperSaveCoordinationDebugLiveMontagePosition,
			GoalkeeperSaveCoordinationDebugLiveHandErrorLocal.X,
			GoalkeeperSaveCoordinationDebugLiveHandErrorLocal.Y,
			GoalkeeperSaveCoordinationDebugLiveHandErrorLocal.Z
		)
		: TEXT(
			"Mano viva local: montage no activo"
		),
		bGoalkeeperSaveCoordinationDebugLiveHandValid
		? FColor::Blue
		: FColor::White
	);
}

bool ASoccerAIController::
TryUpdateGoalkeeperEmergencySaveLateralRun(
	ASoccerAICharacter* SoccerCharacter,
	ASoccerBall* SoccerBall,
	float DeltaTime
)
{
	if (
		!bUseGoalkeeperEmergencyLateralRun ||
		!IsValid(SoccerCharacter) ||
		!IsValid(SoccerBall) ||
		SoccerCharacter->IsGoalkeeperActionActive() ||
		!HasPendingGoalkeeperSaveActionFor(SoccerCharacter) ||
		!bPendingGoalkeeperSaveActionEmergencySelection
		)
	{
		return false;
	}

	const float RequiredStartDelay =
		PendingGoalkeeperSaveActionTimeToIntervention -
		PendingGoalkeeperSaveActionContactWindowStartTime;

	// Si ya llego el instante de iniciar el montage, no robamos ni un Tick
	// adicional. UpdatePendingGoalkeeperSaveAction sera quien haga el commit.
	if (
		RequiredStartDelay <=
		GoalkeeperSaveMontageStartTolerance
		)
	{
		return false;
	}

	FVector CharacterRight =
		SoccerCharacter->GetActorRightVector();

	CharacterRight.Z = 0.0f;
	CharacterRight = CharacterRight.GetSafeNormal();

	if (CharacterRight.IsNearlyZero())
	{
		return false;
	}

	FVector ToPredictedBall =
		PendingGoalkeeperSaveActionPredictedInterventionLocation -
		SoccerCharacter->GetActorLocation();

	ToPredictedBall.Z = 0.0f;

	float SideOffset =
		FVector::DotProduct(
			ToPredictedBall,
			CharacterRight
		);

	const float SafeMinSideOffset =
		FMath::Max(
			0.0f,
			GoalkeeperEmergencyLateralRunMinSideOffset
		);

	float SideSign = 0.0f;

	if (FMath::Abs(SideOffset) > SafeMinSideOffset)
	{
		SideSign = FMath::Sign(SideOffset);
	}
	else if (
		IsGoalkeeperActionToRight(
			PendingGoalkeeperScheduledSaveAction
		)
		)
	{
		SideSign = 1.0f;
	}
	else if (
		IsGoalkeeperActionToLeft(
			PendingGoalkeeperScheduledSaveAction
		)
		)
	{
		SideSign = -1.0f;
	}

	if (FMath::IsNearlyZero(SideSign))
	{
		return false;
	}

	// Si la accion elegida ya indica claramente un lado, no permitimos que
	// un pequeno error numerico del punto previsto mande al lado contrario.
	if (
		IsGoalkeeperActionToRight(
			PendingGoalkeeperScheduledSaveAction
		)
		)
	{
		SideSign = 1.0f;
	}
	else if (
		IsGoalkeeperActionToLeft(
			PendingGoalkeeperScheduledSaveAction
		)
		)
	{
		SideSign = -1.0f;
	}

	const float LateralDistanceToPredictedBall =
		FMath::Abs(SideOffset);

	const float TargetTravelDistance =
		FMath::Max(
			SafeMinSideOffset + 5.0f,
			LateralDistanceToPredictedBall
		);

	FVector MoveTarget =
		SoccerCharacter->GetActorLocation() +
		CharacterRight *
		SideSign *
		TargetTravelDistance;

	MoveTarget.Z =
		SoccerCharacter->GetActorLocation().Z;

	ClearGoalkeeperLooseBallClaimMode();
	ClearGoalkeeperSweeperMode();
	ClearGoalkeeperRetreatMode();

	SetGoalkeeperBehaviorMode(
		ESoccerGoalkeeperBehaviorMode::ShotPending
	);

	MoveToLocationWithAIMovement(
		ESoccerAIOrder::MaintainTeamShape,
		MoveTarget,
		FMath::Max(
			1.0f,
			GoalkeeperEmergencyLateralRunAcceptanceRadius
		),
		false
	);

	// MoveToLocationWithAIMovement actualiza primero el modo segun la orden.
	// Forzamos FastRun despues para que esta reaccion conserve caracter de
	// emergencia sin saltarse los limites de energia del propio AICharacter.
	SoccerCharacter->RequestAIMovementMode(
		ESoccerAIMovementMode::FastRun,
		ESoccerAIMovementReason::GoalkeeperEmergency,
		true
	);

	SetFocus(SoccerBall);
	FacePawnTowardLocation(
		SoccerBall->GetActorLocation(),
		DeltaTime
	);

	return true;
}

bool ASoccerAIController::
TryBuildGoalkeeperTrajectoryPrepositionTarget(
	const ASoccerAICharacter* SoccerCharacter,
	const ASoccerBall* SoccerBall,
	FVector& OutMoveTarget,
	FVector& OutGoalLineCrossing
) const
{
	OutMoveTarget = FVector::ZeroVector;
	OutGoalLineCrossing = FVector::ZeroVector;

	if (
		!IsValid(SoccerCharacter) ||
		!IsValid(SoccerBall) ||
		!IsValid(MatchManager)
		)
	{
		return false;
	}

	if (IsValid(MatchManager->GetPossessingCharacter()))
	{
		return false;
	}

	const ASoccerField* SoccerField = MatchManager->GetSoccerField();
	if (!IsValid(SoccerField))
	{
		return false;
	}

	const FVector BallLocation = SoccerBall->GetActorLocation();
	FVector HorizontalVelocity = SoccerBall->GetVelocity();
	HorizontalVelocity.Z = 0.0f;

	const float HorizontalSpeed = HorizontalVelocity.Size();
	if (
		HorizontalSpeed <
		FMath::Max(0.0f, GoalkeeperTrajectoryPrepositionMinBallSpeed)
		)
	{
		return false;
	}

	const FVector HomeLocation = GetGoalkeeperHomeLocation(SoccerCharacter);
	if (HomeLocation.IsNearlyZero())
	{
		return false;
	}

	const ESoccerTeam Team = SoccerCharacter->GetTeam();
	const float OwnGoalLineSign = MatchManager->GetOwnGoalLineSign(Team);
	const FVector OwnGoalCenter = SoccerField->GetGoalCenterWorldLocation(
		OwnGoalLineSign,
		0.0f
	);
	const FVector PitchLengthDirection =
		SoccerField->GetPitchLengthWorldDirection();
	const FVector LateralDirection =
		SoccerField->GetPitchWidthWorldDirection();
	const FVector InwardDirection =
		PitchLengthDirection * -SoccerFieldDimensions::NormalizeGoalLineSign(
			OwnGoalLineSign
		);

	const float BallDepthFromGoalLine = FVector::DotProduct(
		BallLocation - OwnGoalCenter,
		InwardDirection
	);

	const float ScaledTrajectoryPrepositionMaxDepth =
		SoccerFieldDimensions::ScaleAuthoredLongitudinalDistance(
			GoalkeeperTrajectoryPrepositionMaxDepthFromGoal
		);

	if (
		BallDepthFromGoalLine <= 1.0f ||
		BallDepthFromGoalLine >
		FMath::Max(1.0f, ScaledTrajectoryPrepositionMaxDepth)
		)
	{
		return false;
	}

	const float VelocityAlongInward = FVector::DotProduct(
		HorizontalVelocity,
		InwardDirection
	);

	// Hacia el arco propio significa reducir la profundidad medida desde la
	// linea de gol hacia el campo.
	if (VelocityAlongInward >= -KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const float TimeToGoalLine =
		-BallDepthFromGoalLine / VelocityAlongInward;

	if (!FMath::IsFinite(TimeToGoalLine) || TimeToGoalLine <= 0.0f)
	{
		return false;
	}

	OutGoalLineCrossing =
		BallLocation + HorizontalVelocity * TimeToGoalLine;
	// Elimina cualquier residuo numerico en la normal del plano del arco.
	const float CrossingDepth = FVector::DotProduct(
		OutGoalLineCrossing - OwnGoalCenter,
		InwardDirection
	);
	OutGoalLineCrossing -= InwardDirection * CrossingDepth;

	const float GoalSideMargin =
		FMath::Max(0.0f, GoalkeeperTrajectoryPrepositionGoalSideMargin);
	const float AllowedGoalHalfWidth =
		SoccerFieldDimensions::GoalHalfWidthCm + GoalSideMargin;
	const float CrossingLateralOffset = FVector::DotProduct(
		OutGoalLineCrossing - OwnGoalCenter,
		LateralDirection
	);

	if (FMath::Abs(CrossingLateralOffset) > AllowedGoalHalfWidth)
	{
		return false;
	}

	FVector BasePositioningLocation =
		MatchManager->GetGoalkeeperMoveLocation(SoccerCharacter);

	if (BasePositioningLocation.IsNearlyZero())
	{
		return false;
	}

	const float PostSafetyMargin = FMath::Clamp(
		GoalkeeperTrajectoryPrepositionPostSafetyMargin,
		0.0f,
		SoccerFieldDimensions::GoalHalfWidthCm - 5.0f
	);
	const float SafeMaximumLateralOffset =
		SoccerFieldDimensions::GoalHalfWidthCm - PostSafetyMargin;
	const float SafeGoalLineLateralOffset = FMath::Clamp(
		CrossingLateralOffset,
		-SafeMaximumLateralOffset,
		SafeMaximumLateralOffset
	);

	const float CurrentDepthFromGoalLine = FMath::Max(
		0.0f,
		FVector::DotProduct(
			BasePositioningLocation - OwnGoalCenter,
			InwardDirection
		)
	);
	const float TimeToGoalkeeperPlane =
		(CurrentDepthFromGoalLine - BallDepthFromGoalLine) /
		VelocityAlongInward;

	float TargetLateralOffset = SafeGoalLineLateralOffset;

	if (
		FMath::IsFinite(TimeToGoalkeeperPlane) &&
		TimeToGoalkeeperPlane > 0.0f &&
		TimeToGoalkeeperPlane <= TimeToGoalLine + KINDA_SMALL_NUMBER
		)
	{
		const FVector TrajectoryAtGoalkeeperDepth =
			BallLocation + HorizontalVelocity * TimeToGoalkeeperPlane;
		const float TrajectoryLateralOffset = FVector::DotProduct(
			TrajectoryAtGoalkeeperDepth - OwnGoalCenter,
			LateralDirection
		);
		const float HomeDepthFromGoalLine = FMath::Max(
			0.0f,
			FVector::DotProduct(
				HomeLocation - OwnGoalCenter,
				InwardDirection
			)
		);
		const float DepthBlendDenominator = FMath::Max(
			1.0f,
			SoccerFieldDimensions::PenaltySpotDistanceCm -
			HomeDepthFromGoalLine
		);
		const float DepthAwareBlendAlpha = FMath::Clamp(
			(CurrentDepthFromGoalLine - HomeDepthFromGoalLine) /
			DepthBlendDenominator,
			0.0f,
			1.0f
		);
		const float AdvancedMaximumLateralOffset = FMath::Lerp(
			SafeMaximumLateralOffset,
			SoccerFieldDimensions::PenaltyAreaHalfWidthCm,
			DepthAwareBlendAlpha
		);
		const float SafeTrajectoryLateralOffset = FMath::Clamp(
			TrajectoryLateralOffset,
			-AdvancedMaximumLateralOffset,
			AdvancedMaximumLateralOffset
		);

		TargetLateralOffset = FMath::Lerp(
			SafeGoalLineLateralOffset,
			SafeTrajectoryLateralOffset,
			DepthAwareBlendAlpha
		);
	}

	OutMoveTarget =
		OwnGoalCenter +
		InwardDirection * CurrentDepthFromGoalLine +
		LateralDirection * TargetLateralOffset;
	OutMoveTarget.Z = BasePositioningLocation.Z;

	return !OutMoveTarget.ContainsNaN();
}

bool ASoccerAIController::
TryUpdateGoalkeeperTrajectoryPrepositioning(
	ASoccerAICharacter* SoccerCharacter,
	ASoccerBall* SoccerBall,
	float DeltaTime
)
{
	FVector MoveTarget = FVector::ZeroVector;
	FVector GoalLineCrossing = FVector::ZeroVector;

	if (
		!TryBuildGoalkeeperTrajectoryPrepositionTarget(
			SoccerCharacter,
			SoccerBall,
			MoveTarget,
			GoalLineCrossing
		)
		)
	{
		return false;
	}

	(void)GoalLineCrossing;

	// Si venia de una salida iniciada en el Tick anterior, cancelamos tambien
	// ese estado persistente. La trayectoria hacia el arco pasa a ser prioritaria.
	ClearGoalkeeperLooseBallClaimMode();
	ClearGoalkeeperSweeperMode();
	ClearGoalkeeperRetreatMode();

	// Si ya existe una seleccion de atajada pendiente conservamos ShotPending;
	// el movimiento lateral no convierte esa seleccion en otra clase de estado.
	// Fuera de esa ventana seguimos siendo simplemente Positioning.
	SetGoalkeeperBehaviorMode(
		HasPendingGoalkeeperSaveActionFor(SoccerCharacter)
		? ESoccerGoalkeeperBehaviorMode::ShotPending
		: ESoccerGoalkeeperBehaviorMode::Positioning
	);

	MoveToLocationWithAIMovement(
		ESoccerAIOrder::MaintainTeamShape,
		MoveTarget,
		FMath::Max(
			1.0f,
			GoalkeeperTrajectoryPrepositionAcceptanceRadius
		),
		true
	);

	SetFocus(SoccerBall);
	FacePawnTowardLocation(
		SoccerBall->GetActorLocation(),
		DeltaTime
	);

	return true;
}

bool ASoccerAIController::
TryStartGoalkeeperSaveForIncomingShot(
	ASoccerAICharacter* SoccerCharacter,
	ASoccerBall* SoccerBall
)
{
	if (
		!IsValid(SoccerCharacter) ||
		!IsValid(SoccerBall)
		)
	{
		return false;
	}

	if (
		SoccerCharacter->
		IsGoalkeeperActionActive() ||
		HasPendingGoalkeeperSaveActionFor(
			SoccerCharacter
		) ||
		HasPendingGoalkeeperSaveImpactFor(
			SoccerCharacter
		)
		)
	{
		return false;
	}

	/*
	 * Esta predicci�n solo determina si la pelota ya
	 * entr� en el horizonte temporal de decisi�n.
	 *
	 * Todav�a no elige una animaci�n.
	 */
	FVector ReferencePredictedLocation =
		FVector::ZeroVector;

	float TimeToReferencePlane =
		0.0f;

	const float ProfileDecisionHorizon =
		GetGoalkeeperProfileDecisionHorizon(SoccerCharacter);

	if (
		!TryPredictGoalkeeperReferencePlaneCrossing(
			SoccerCharacter,
			SoccerBall,
			ReferencePredictedLocation,
			TimeToReferencePlane,
			ProfileDecisionHorizon
		)
		)
	{
		/*
		 * Todav�a falta m�s tiempo que el horizonte
		 * configurado, o la pelota no viaja hacia una
		 * zona peligrosa.
		 */
		return false;
	}

	const bool bCanUseHands =
		IsGoalkeeperHandlingAllowedAtLocation(
			SoccerCharacter,
			ReferencePredictedLocation
		);

	/*
	 * El selector nuevo eval�a, para cada acci�n:
	 *
	 * - posiciones de ambas manos;
	 * - movimiento horizontal de la c�psula;
	 * - plano propio de cada mano;
	 * - posici�n de la pelota al cruzar ese plano;
	 * - tiempo hasta el plano;
	 * - momento de contacto del montage.
	 */
	const FGoalkeeperSaveSelectionResult
		SaveSelection =
		SelectGoalkeeperActionForIncomingBall(
			SoccerCharacter,
			SoccerBall,
			bCanUseHands
		);

	if (!SaveSelection.IsValid())
	{
		return false;
	}

	if (
		SaveSelection.BallTimeToContactPlane ==
		BIG_NUMBER ||
		SaveSelection.RequiredMontageStartDelay ==
		BIG_NUMBER
		)
	{
		return false;
	}

	ResetGoalkeeperSaveCoordinationDebug();

	const float BallTimeToContactPlane =
		FMath::Max(
			0.0f,
			SaveSelection.
			BallTimeToContactPlane
		);

	const float SelectedContactTime =
		FMath::Max(
			0.0f,
			SaveSelection.
			SelectedContactTime
		);

	const float RequiredMontageStartDelay =
		SaveSelection.RequiredMontageStartDelay;

	RecordGoalkeeperSaveTimingDebug(
		SoccerCharacter,
		SoccerBall,
		SaveSelection.Action,
		SaveSelection.PredictedBallLocation,
		BallTimeToContactPlane,
		SelectedContactTime,
		RequiredMontageStartDelay
	);

	/*
	 * La primera evaluaci�n siempre queda pendiente.
	 *
	 * Esto deja una �nica ruta responsable de comenzar
	 * el montage: UpdatePendingGoalkeeperSaveAction().
	 *
	 * Para un remate cercano, el montaje comenzar� en el
	 * Tick siguiente.
	 */
	PreparePendingGoalkeeperSaveAction(
		SoccerCharacter,
		SoccerBall,
		SaveSelection.Action,
		SaveSelection.PredictedBallLocation,
		BallTimeToContactPlane,
		SelectedContactTime,
		SaveSelection.bEmergencySelection
	);

	SetGoalkeeperBehaviorMode(
		ESoccerGoalkeeperBehaviorMode::ShotPending
	);

	if (bDebugGoalkeeperSaveTiming)
	{
		SetGoalkeeperPersistentLineIfEnabled(
			bDebugGoalkeeperSaveTiming ||
			bDebugGoalkeeperSaveCoordination ||
			bDebugGoalkeeperAnimatedContact ||
			bDebugGoalkeeperHandTrackSelector,

			this,
			ESoccerDebugCategory::GoalkeeperSave,
			400,
			FString::Printf(
				TEXT(
					"Decision: plano GK actual %.3f s | "
					"horizonte %.3f s"
				),
				TimeToReferencePlane,
				GoalkeeperSaveDecisionTimeHorizon
			),
			FColor::Cyan
		);

		SetGoalkeeperPersistentLineIfEnabled(
			bDebugGoalkeeperSaveTiming ||
			bDebugGoalkeeperSaveCoordination ||
			bDebugGoalkeeperAnimatedContact ||
			bDebugGoalkeeperHandTrackSelector,

			this,
			ESoccerDebugCategory::GoalkeeperSave,
			410,
			FString::Printf(
				TEXT(
					"Plano contacto: pelota %.3f s | "
					"contacto %.3f s | "
					"inicio %+0.3f s"
				),
				BallTimeToContactPlane,
				SelectedContactTime,
				RequiredMontageStartDelay
			),
			RequiredMontageStartDelay >
			GoalkeeperSaveMontageStartTolerance
			? FColor::Cyan
			: FColor::Orange
		);

		SetGoalkeeperPersistentLineIfEnabled(
			bDebugGoalkeeperSaveTiming ||
			bDebugGoalkeeperSaveCoordination ||
			bDebugGoalkeeperAnimatedContact ||
			bDebugGoalkeeperHandTrackSelector,

			this,
			ESoccerDebugCategory::GoalkeeperSave,
			420,
			FString::Printf(
				TEXT(
					"Seleccion: error %.1f -> %.1f -> %.1f cm | "
					"lat x%.3f (%+.1f) | corr F/L/U %+.1f/%+.1f/%+.1f | "
					"rate x%.3f | mano %s | viable %s | emergencia %s"
				),
				SaveSelection.RawHandDistanceBeforeAdaptiveLateral,
				SaveSelection.RawHandDistanceBeforeNearPerfectCorrection,
				SaveSelection.RawHandDistance,
				SaveSelection.AdaptiveLateralScale,
				SaveSelection.AdaptiveLateralExtraDistance,
				SaveSelection.NearPerfectContactCorrectionLocal.X,
				SaveSelection.NearPerfectContactCorrectionLocal.Y,
				SaveSelection.NearPerfectContactCorrectionLocal.Z,
				SaveSelection.MontagePlayRate,
				IsGoalkeeperCatchAction(
					SaveSelection.Action
				)
				? TEXT("ZONA MANOS")
				: (
					SaveSelection.bSelectedLeftHand
					? TEXT("IZQUIERDA")
					: TEXT("DERECHA")
					),
				SaveSelection.bTemporallyFeasible
				? TEXT("SI")
				: TEXT("NO"),
				SaveSelection.bEmergencySelection
				? TEXT("SI")
				: TEXT("NO")
			),
			SaveSelection.bEmergencySelection
			? FColor::Orange
			: FColor::Green
		);
	}

	return true;
}

ESoccerGoalkeeperAction ASoccerAIController::ChooseGoalkeeperActionLegacy(
	const ASoccerAICharacter* SoccerCharacter,
	const FVector& PredictedInterventionLocation,
	bool bCanUseHands
) const
{
	if (!IsValid(SoccerCharacter))
	{
		return ESoccerGoalkeeperAction::None;
	}

	if (
		bForceGoalkeeperSaveAction &&
		ForcedGoalkeeperSaveAction !=
		ESoccerGoalkeeperAction::None
		)
	{
		return ForcedGoalkeeperSaveAction;
	}

	const UCapsuleComponent* CapsuleComponent =
		SoccerCharacter->GetCapsuleComponent();

	const float CharacterGroundZ =
		CapsuleComponent != nullptr
		? SoccerCharacter->GetActorLocation().Z -
		CapsuleComponent->GetScaledCapsuleHalfHeight()
		: SoccerCharacter->GetActorLocation().Z;

	const float PredictedBallHeight =
		PredictedInterventionLocation.Z - CharacterGroundZ;

	if (PredictedBallHeight < -25.0f)
	{
		return ESoccerGoalkeeperAction::None;
	}

	const FVector ToPredictedLocation =
		PredictedInterventionLocation -
		SoccerCharacter->GetActorLocation();

	FVector CharacterRight =
		SoccerCharacter->GetActorRightVector();

	CharacterRight.Z = 0.0f;
	CharacterRight = CharacterRight.GetSafeNormal();

	const float SideOffset =
		!CharacterRight.IsNearlyZero()
		? FVector::DotProduct(ToPredictedLocation, CharacterRight)
		: PredictedInterventionLocation.Y - SoccerCharacter->GetActorLocation().Y;

	const float AbsSideOffset =
		FMath::Abs(SideOffset);

	const bool bShotToGoalkeeperRight =
		SideOffset > 0.0f;

	if (!bCanUseHands)
	{
		if (PredictedBallHeight > GoalkeeperChestCatchMaxHeight)
		{
			return ESoccerGoalkeeperAction::Miss;
		}

		return bShotToGoalkeeperRight
			? ESoccerGoalkeeperAction::BodyBlockRight
			: ESoccerGoalkeeperAction::BodyBlockLeft;
	}

	if (PredictedBallHeight <= GoalkeeperLowBallMaxHeight)
	{
		if (AbsSideOffset <= GoalkeeperCentralCatchHalfWidth)
		{
			return ESoccerGoalkeeperAction::CatchLow;
		}

		return bShotToGoalkeeperRight
			? ESoccerGoalkeeperAction::BodyBlockRight
			: ESoccerGoalkeeperAction::BodyBlockLeft;
	}

	if (AbsSideOffset <= GoalkeeperCentralCatchHalfWidth)
	{
		if (PredictedBallHeight <= GoalkeeperCrotchCatchMaxHeight)
		{
			return ESoccerGoalkeeperAction::CatchLow;
		}

		if (PredictedBallHeight <= GoalkeeperChestCatchMaxHeight)
		{
			return ESoccerGoalkeeperAction::CatchChest;
		}

		return ESoccerGoalkeeperAction::CatchHighForward;
	}

	if (
		AbsSideOffset <= GoalkeeperStandingCatchHalfWidth &&
		PredictedBallHeight <= GoalkeeperChestCatchMaxHeight
		)
	{
		return ESoccerGoalkeeperAction::CatchChest;
	}

	if (
		AbsSideOffset <= GoalkeeperStandingCatchHalfWidth &&
		PredictedBallHeight <= GoalkeeperHighCatchMaxHeight
		)
	{
		return bShotToGoalkeeperRight
			? ESoccerGoalkeeperAction::CatchHighRight
			: ESoccerGoalkeeperAction::CatchHighForward;
	}

	if (PredictedBallHeight <= GoalkeeperHighCatchMaxHeight)
	{
		return bShotToGoalkeeperRight
			? ESoccerGoalkeeperAction::DivingSaveRight
			: ESoccerGoalkeeperAction::DivingSaveLeft;
	}

	return ESoccerGoalkeeperAction::Miss;
}

bool ASoccerAIController::ShouldGoalkeeperFaceBallDuringAction(
	const ASoccerAICharacter* SoccerCharacter
) const
{
	if (!IsValid(SoccerCharacter))
	{
		return true;
	}

	/*
	 * Durante cualquier montage de atajada conservamos la
	 * orientaci�n con la que comenz� la animaci�n.
	 *
	 * Esto evita que FacePawnTowardLocation rote la c�psula
	 * mientras el cuerpo se arroja hacia un lateral.
	 */
	return !SoccerCharacter->IsGoalkeeperActionActive();
}

bool ASoccerAIController::TryUpdateGoalkeeperLooseBallClaimBehavior(
	ASoccerAICharacter* SoccerCharacter,
	ASoccerBall* SoccerBall,
	float DeltaTime
)
{
	if (
		!bUseGoalkeeperLooseBallClaim ||
		!IsValid(SoccerCharacter) ||
		!IsValid(SoccerBall)
		)
	{
		ClearGoalkeeperLooseBallClaimMode();
		return false;
	}

	if (
		SoccerCharacter->IsGoalkeeperActionActive() ||
		SoccerCharacter->IsAIPossessingBall() ||
		HasPendingGoalkeeperSaveActionFor(SoccerCharacter) ||
		HasPendingGoalkeeperSaveImpactFor(SoccerCharacter)
		)
	{
		ClearGoalkeeperLooseBallClaimMode();
		return false;
	}

	const float DistanceToBall =
		FVector::Dist2D(
			SoccerCharacter->GetActorLocation(),
			SoccerBall->GetActorLocation()
		);

	const bool bBallAtFootHeight =
		IsGoalkeeperClaimBallAtFootHeight(
			SoccerCharacter,
			SoccerBall
		);

	// El compromiso no autoriza a quitarle la pelota a un companero que ya
	// tomo posesion. En ese caso la salida se cancela normalmente.
	ASoccerCharacterBase* CurrentPossessingCharacter =
		IsValid(MatchManager)
		? MatchManager->GetPossessingCharacter()
		: nullptr;

	if (
		IsValid(CurrentPossessingCharacter) &&
		CurrentPossessingCharacter != SoccerCharacter &&
		CurrentPossessingCharacter->GetTeam() == SoccerCharacter->GetTeam()
		)
	{
		ClearGoalkeeperLooseBallClaimMode();
		return false;
	}

	/*
	 * Si la salida ya estaba activa y el arquero entro en la zona final de
	 * despeje, deja de reevaluar si "conviene" ir. Ya tomo la decision: ahora
	 * debe llegar al contacto real y patear.
	 */
	bool bUseFinalClearanceApproach =
		bGoalkeeperLooseBallClaimActive &&
		DistanceToBall <= GoalkeeperLooseBallClaimKickDistance &&
		bBallAtFootHeight;

	if (bUseFinalClearanceApproach)
	{
		if (
			TryGoalkeeperClearBallWithFeet(
				SoccerCharacter,
				SoccerBall
			)
			)
		{
			ClearGoalkeeperLooseBallClaimMode();

			SetGoalkeeperBehaviorMode(
				ESoccerGoalkeeperBehaviorMode::Retreating
			);

			StopMovement();
			return true;
		}
	}

	FVector ClaimTargetLocation = FVector::ZeroVector;

	if (!bUseFinalClearanceApproach)
	{
		if (
			!ShouldGoalkeeperClaimLooseBall(
				SoccerCharacter,
				SoccerBall,
				ClaimTargetLocation
			)
			)
		{
			ClearGoalkeeperLooseBallClaimMode();
			return false;
		}

		bGoalkeeperLooseBallClaimActive = true;

		// Si la primera evaluacion ya lo encuentra cerca, intenta patear sin
		// esperar otro Tick. Si aun falta contacto, entra en aproximacion final.
		if (
			DistanceToBall <= GoalkeeperLooseBallClaimKickDistance &&
			bBallAtFootHeight
			)
		{
			if (
				TryGoalkeeperClearBallWithFeet(
					SoccerCharacter,
					SoccerBall
				)
				)
			{
				ClearGoalkeeperLooseBallClaimMode();

				SetGoalkeeperBehaviorMode(
					ESoccerGoalkeeperBehaviorMode::Retreating
				);

				StopMovement();
				return true;
			}

			bUseFinalClearanceApproach = true;
		}
	}

	ClearGoalkeeperSweeperMode();
	ClearGoalkeeperRetreatMode();

	SetGoalkeeperBehaviorMode(
		ESoccerGoalkeeperBehaviorMode::Sweeping
	);

	SoccerCharacter->RequestAIMovementMode(
		ESoccerAIMovementMode::FastRun,
		ESoccerAIMovementReason::GoalkeeperEmergency,
		true
	);

	if (bUseFinalClearanceApproach)
	{
		// En los ultimos centimetros seguimos la pelota real, no su prediccion.
		ClaimTargetLocation = SoccerBall->GetActorLocation();
	}

	ClaimTargetLocation.Z =
		SoccerCharacter->GetActorLocation().Z;

	const float CurrentTime =
		GetWorld() != nullptr
		? GetWorld()->GetTimeSeconds()
		: 0.0f;

	const bool bShouldRefreshMove =
		bUseFinalClearanceApproach ||
		LastGoalkeeperLooseBallClaimMoveTarget.IsNearlyZero() ||
		FVector::Dist2D(
			LastGoalkeeperLooseBallClaimMoveTarget,
			ClaimTargetLocation
		) >= GoalkeeperLooseBallClaimRepathDistanceThreshold ||
		CurrentTime - LastGoalkeeperLooseBallClaimMoveRequestTime >=
		GoalkeeperLooseBallClaimMoveRefreshInterval;

	if (bShouldRefreshMove)
	{
		const float AcceptanceRadius =
			bUseFinalClearanceApproach
			? GoalkeeperLooseBallClaimFinalApproachAcceptanceRadius
			: GoalkeeperLooseBallClaimAcceptanceRadius;

		MoveToLocationWithAIMovement(
			ESoccerAIOrder::ChaseBall,
			ClaimTargetLocation,
			AcceptanceRadius,
			false
		);

		LastGoalkeeperLooseBallClaimMoveTarget =
			ClaimTargetLocation;

		LastGoalkeeperLooseBallClaimMoveRequestTime =
			CurrentTime;
	}

	SetFocus(SoccerBall);

	FacePawnTowardLocation(
		SoccerBall->GetActorLocation(),
		DeltaTime
	);

	return true;
}

bool ASoccerAIController::ShouldGoalkeeperClaimLooseBall(
	const ASoccerAICharacter* SoccerCharacter,
	const ASoccerBall* SoccerBall,
	FVector& OutClaimTargetLocation
) const
{
	OutClaimTargetLocation = FVector::ZeroVector;

	if (
		!bUseGoalkeeperLooseBallClaim ||
		!IsValid(SoccerCharacter) ||
		!IsValid(SoccerBall) ||
		!IsValid(MatchManager)
		)
	{
		return false;
	}

	ASoccerCharacterBase* PossessingCharacter =
		MatchManager->GetPossessingCharacter();

	// Si la pelota es de un compa�ero, el arquero no sale a quit�rsela.
	if (
		IsValid(PossessingCharacter) &&
		PossessingCharacter->GetTeam() == SoccerCharacter->GetTeam()
		)
	{
		return false;
	}

	FVector ClaimTargetLocation =
		PredictGoalkeeperSweeperBallLocation(SoccerBall);

	if (ClaimTargetLocation.IsNearlyZero())
	{
		ClaimTargetLocation =
			SoccerBall->GetActorLocation();
	}

	const float TargetDepth =
		GetGoalkeeperDepthFromGoal(
			SoccerCharacter,
			ClaimTargetLocation
		);

	if (
		TargetDepth < 0.0f ||
		TargetDepth > GoalkeeperLooseBallClaimMaxDepthFromGoal
		)
	{
		return false;
	}

	const float TargetLateralOffset =
		FMath::Abs(
			GetGoalkeeperLateralOffsetFromGoal(
				SoccerCharacter,
				ClaimTargetLocation
			)
		);

	if (TargetLateralOffset > GoalkeeperLooseBallClaimHalfWidth)
	{
		return false;
	}

	float GoalkeeperTime = 0.0f;
	float BestOpponentTime = 0.0f;

	if (
		!CanGoalkeeperReachBallBeforeOpponents(
			SoccerCharacter,
			ClaimTargetLocation,
			GoalkeeperTime,
			BestOpponentTime
		)
		)
	{
		return false;
	}

	if (
		!ShouldGoalkeeperClaimLooseBallConsideringOwnTeam(
			SoccerCharacter,
			ClaimTargetLocation
		)
		)
	{
		return false;
	}

	OutClaimTargetLocation =
		ClaimTargetLocation;

	return true;
}

bool ASoccerAIController::CanGoalkeeperReachBallBeforeOpponents(
	const ASoccerAICharacter* SoccerCharacter,
	const FVector& BallTargetLocation,
	float& OutGoalkeeperTime,
	float& OutBestOpponentTime
) const
{
	OutGoalkeeperTime = 1000000.0f;
	OutBestOpponentTime = 1000000.0f;

	if (
		!IsValid(SoccerCharacter) ||
		BallTargetLocation.IsNearlyZero()
		)
	{
		return false;
	}

	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return false;
	}

	OutGoalkeeperTime =
		EstimateGoalkeeperGroundTravelTime(
			SoccerCharacter->GetActorLocation(),
			BallTargetLocation
		);

	bool bFoundOpponent = false;

	for (TActorIterator<ASoccerCharacterBase> It(World); It; ++It)
	{
		const ASoccerCharacterBase* Candidate = *It;

		if (!IsValid(Candidate))
		{
			continue;
		}

		if (Candidate == SoccerCharacter)
		{
			continue;
		}

		if (Candidate->GetTeam() == SoccerCharacter->GetTeam())
		{
			continue;
		}

		const float CandidateTime =
			EstimateThreatGroundTravelTime(
				Candidate->GetActorLocation(),
				BallTargetLocation
			);

		OutBestOpponentTime =
			FMath::Min(
				OutBestOpponentTime,
				CandidateTime
			);

		bFoundOpponent = true;
	}

	// Si no encontramos rivales, no bloqueamos la salida.
	if (!bFoundOpponent)
	{
		return true;
	}

	const float RequiredArrivalMargin =
		IsGoalkeeperGoalAreaAuthorityActiveAtLocation(
			SoccerCharacter,
			BallTargetLocation
		)
		? GoalkeeperGoalAreaLooseBallClaimArrivalMargin
		: GoalkeeperLooseBallClaimArrivalMargin;

	return
		OutGoalkeeperTime + RequiredArrivalMargin <=
		OutBestOpponentTime;
}

bool ASoccerAIController::IsGoalkeeperClaimBallAtFootHeight(
	const ASoccerAICharacter* SoccerCharacter,
	const ASoccerBall* SoccerBall
) const
{
	if (!IsValid(SoccerCharacter) || !IsValid(SoccerBall))
	{
		return false;
	}

	const UCapsuleComponent* CapsuleComponent =
		SoccerCharacter->GetCapsuleComponent();

	const float CharacterGroundZ =
		CapsuleComponent != nullptr
		? SoccerCharacter->GetActorLocation().Z -
		CapsuleComponent->GetScaledCapsuleHalfHeight()
		: SoccerCharacter->GetActorLocation().Z;

	const float BallHeight =
		SoccerBall->GetActorLocation().Z - CharacterGroundZ;

	return
		BallHeight >= -20.0f &&
		BallHeight <= GoalkeeperLooseBallClaimMaxBallHeightForFeet;
}

void ASoccerAIController::ClearGoalkeeperLooseBallClaimMode()
{
	bGoalkeeperLooseBallClaimActive = false;

	LastGoalkeeperLooseBallClaimMoveTarget =
		FVector::ZeroVector;

	LastGoalkeeperLooseBallClaimMoveRequestTime =
		-1000.0f;
}

bool ASoccerAIController::ShouldGoalkeeperClaimLooseBallConsideringOwnTeam(
	const ASoccerAICharacter* SoccerCharacter,
	const FVector& BallTargetLocation
) const
{
	if (
		!IsValid(SoccerCharacter) ||
		BallTargetLocation.IsNearlyZero()
		)
	{
		return false;
	}

	if (
		bGoalkeeperGoalAreaIgnoreOwnTeamLooseBallSuppression &&
		IsGoalkeeperGoalAreaAuthorityActiveAtLocation(
			SoccerCharacter,
			BallTargetLocation
		)
		)
	{
		/*
		 * La posesi�n clara de un compa�ero ya fue comprobada antes.
		 * Si la pelota sigue libre dentro del �rea chica, la mera
		 * cercan�a de un defensor no debe cancelar la salida del arquero.
		 */
		return true;
	}

	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return false;
	}

	const float GoalkeeperDistanceToBall =
		FVector::Dist2D(
			SoccerCharacter->GetActorLocation(),
			BallTargetLocation
		);

	const float BallDepthFromGoal =
		GetGoalkeeperDepthFromGoal(
			SoccerCharacter,
			BallTargetLocation
		);

	for (TActorIterator<ASoccerCharacterBase> It(World); It; ++It)
	{
		const ASoccerCharacterBase* Teammate = *It;

		if (!IsValid(Teammate))
		{
			continue;
		}

		if (Teammate == SoccerCharacter)
		{
			continue;
		}

		if (Teammate->GetTeam() != SoccerCharacter->GetTeam())
		{
			continue;
		}

		const float TeammateDistanceToBall =
			FVector::Dist2D(
				Teammate->GetActorLocation(),
				BallTargetLocation
			);

		const bool bTeammateClearlyCloserToBall =
			TeammateDistanceToBall + GoalkeeperLooseBallClaimOwnTeamCloserMargin <
			GoalkeeperDistanceToBall;

		if (!bTeammateClearlyCloserToBall)
		{
			continue;
		}

		const float TeammateDepthFromGoal =
			GetGoalkeeperDepthFromGoal(
				SoccerCharacter,
				Teammate->GetActorLocation()
			);

		const bool bTeammateIsGoalSideOfBall =
			TeammateDepthFromGoal <=
			BallDepthFromGoal + GoalkeeperLooseBallClaimOwnTeamGoalSideDepthMargin;

		if (bTeammateIsGoalSideOfBall)
		{
			// Hay un compa�ero m�s cerca de la pelota y adem�s est� mejor ubicado
			// defensivamente, entre la pelota y el arco. El arquero no deber�a salir.
			return false;
		}

		// Si el compa�ero est� m�s lejos del arco que la pelota,
		// significa que qued� por delante/pasado respecto de la jugada.
		// En ese caso no bloquea la salida del arquero.
	}

	return true;
}

bool ASoccerAIController::IsGoalkeeperHandlingAllowedAtLocation(
	const ASoccerAICharacter* SoccerCharacter,
	const FVector& Location
) const
{
	if (!IsValid(SoccerCharacter))
	{
		return false;
	}

	return
		IsLocationInsideGoalkeeperPenaltyArea(
			SoccerCharacter,
			SoccerCharacter->GetActorLocation()
		)
		&&
		IsLocationInsideGoalkeeperPenaltyArea(
			SoccerCharacter,
			Location
		);
}

bool ASoccerAIController::TryUpdateGoalkeeperSweeperBehavior(
	ASoccerAICharacter* SoccerCharacter,
	ASoccerBall* SoccerBall,
	float DeltaTime
)
{
	if (!IsValid(SoccerCharacter) || !IsValid(SoccerBall))
	{
		ClearGoalkeeperSweeperMode();
		return false;
	}

	FVector SweepTargetLocation = FVector::ZeroVector;

	if (
		!ShouldStartOrKeepGoalkeeperSweeping(
			SoccerCharacter,
			SoccerBall,
			SweepTargetLocation
		)
		)
	{
		ClearGoalkeeperSweeperMode();
		return false;
	}

	if (TryResolveGoalkeeperSweeperBallContact(SoccerCharacter, SoccerBall))
	{
		return true;
	}

	ASoccerCharacterBase* ThreatCharacter =
		GoalkeeperSweeperThreatCharacter.Get();

	StartGoalkeeperSweeperMode(
		ThreatCharacter,
		SweepTargetLocation
	);

	SoccerCharacter->RequestAIMovementMode(
		ESoccerAIMovementMode::FastRun,
		ESoccerAIMovementReason::GoalkeeperEmergency,
		true
	);

	MoveToLocationWithAIMovement(
		ESoccerAIOrder::ChaseBall,
		SweepTargetLocation,
		GoalkeeperSweeperAcceptanceRadius,
		false
	);

	SetFocus(SoccerBall);

	FacePawnTowardLocation(
		SoccerBall->GetActorLocation(),
		DeltaTime
	);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			0.12f,
			FColor::Cyan,
			TEXT("GK sweeper: saliendo a cortar")
		);
	}

	return true;
}

bool ASoccerAIController::ShouldStartOrKeepGoalkeeperSweeping(
	ASoccerAICharacter* SoccerCharacter,
	ASoccerBall* SoccerBall,
	FVector& OutSweepTargetLocation
)
{
	OutSweepTargetLocation = FVector::ZeroVector;

	if (!IsValid(SoccerCharacter) || !IsValid(SoccerBall))
	{
		return false;
	}

	const float CurrentTime =
		GetWorld() != nullptr
		? GetWorld()->GetTimeSeconds()
		: 0.0f;

	ASoccerCharacterBase* ThreatCharacter =
		FindGoalkeeperBreakawayThreat(
			SoccerCharacter,
			SoccerBall
		);

	const bool bCommitmentStillActive =
		bGoalkeeperSweeperActive &&
		CurrentTime - GoalkeeperSweeperStartTime <
		GoalkeeperSweeperMinimumCommitTime;

	if (
		!bGoalkeeperSweeperActive &&
		CurrentTime - LastGoalkeeperSweeperResolvedTime <
		GoalkeeperSweeperPostActionCooldown
		)
	{
		return false;
	}

	if (!IsValid(ThreatCharacter))
	{
		if (bCommitmentStillActive)
		{
			OutSweepTargetLocation =
				GoalkeeperSweeperTargetLocation;

			return !OutSweepTargetLocation.IsNearlyZero();
		}

		return false;
	}

	const FVector ThreatLocation =
		ThreatCharacter->GetActorLocation();

	if (
		!IsGoalkeeperThreatInsideSweeperTriggerZone(
			SoccerCharacter,
			ThreatLocation
		)
		)
	{
		if (bCommitmentStillActive)
		{
			OutSweepTargetLocation =
				GoalkeeperSweeperTargetLocation;

			return !OutSweepTargetLocation.IsNearlyZero();
		}

		return false;
	}

	if (
		bUseGoalkeeperSweeperThreatDirectionCheck &&
		!IsGoalkeeperThreatDrivingTowardGoal(
			SoccerCharacter,
			ThreatCharacter
		)
		)
	{
		if (bCommitmentStillActive)
		{
			OutSweepTargetLocation =
				GoalkeeperSweeperTargetLocation;

			return !OutSweepTargetLocation.IsNearlyZero();
		}

		return false;
	}

	if (
		HasDefensiveHelpAgainstGoalkeeperThreat(
			SoccerCharacter,
			ThreatCharacter,
			ThreatLocation
		)
		)
	{
		if (bCommitmentStillActive)
		{
			OutSweepTargetLocation =
				GoalkeeperSweeperTargetLocation;

			return !OutSweepTargetLocation.IsNearlyZero();
		}

		return false;
	}

	const FVector HomeLocation =
		GetGoalkeeperHomeLocation(SoccerCharacter);

	if (
		!HomeLocation.IsNearlyZero() &&
		FVector::Dist2D(
			HomeLocation,
			SoccerCharacter->GetActorLocation()
		) > GoalkeeperSweeperMaxDistanceFromHome
		)
	{
		return false;
	}

	OutSweepTargetLocation =
		BuildGoalkeeperSweepTargetLocation(
			SoccerCharacter,
			SoccerBall,
			ThreatCharacter
		);

	if (OutSweepTargetLocation.IsNearlyZero())
	{
		return false;
	}

	if (
		bUseGoalkeeperSweeperRaceCheck &&
		!CanGoalkeeperWinSweeperRace(
			SoccerCharacter,
			SoccerBall,
			ThreatCharacter,
			OutSweepTargetLocation
		)
		)
	{
		if (bCommitmentStillActive)
		{
			OutSweepTargetLocation =
				GoalkeeperSweeperTargetLocation;

			return !OutSweepTargetLocation.IsNearlyZero();
		}

		return false;
	}

	GoalkeeperSweeperThreatCharacter =
		ThreatCharacter;

	return true;
}

ASoccerCharacterBase* ASoccerAIController::FindGoalkeeperBreakawayThreat(
	const ASoccerAICharacter* SoccerCharacter,
	const ASoccerBall* SoccerBall
) const
{
	if (
		!IsValid(SoccerCharacter) ||
		!IsValid(SoccerBall) ||
		!IsValid(MatchManager)
		)
	{
		return nullptr;
	}

	ASoccerCharacterBase* PossessingCharacter =
		MatchManager->GetPossessingCharacter();

	if (
		IsValid(PossessingCharacter) &&
		PossessingCharacter->GetTeam() != SoccerCharacter->GetTeam()
		)
	{
		const float DistanceThreatToBall =
			FVector::Dist2D(
				PossessingCharacter->GetActorLocation(),
				SoccerBall->GetActorLocation()
			);

		if (
			DistanceThreatToBall <=
			GoalkeeperSweeperAttackerMaxDistanceToBall
			)
		{
			return PossessingCharacter;
		}
	}

	ASoccerCharacterBase* LastTouchCharacter =
		MatchManager->GetLastTouchCharacter();

	if (
		IsValid(LastTouchCharacter) &&
		LastTouchCharacter->GetTeam() != SoccerCharacter->GetTeam()
		)
	{
		const float BallDepth =
			GetGoalkeeperDepthFromGoal(
				SoccerCharacter,
				SoccerBall->GetActorLocation()
			);

		if (
			BallDepth <=
			GoalkeeperSweeperTriggerDepthFromGoal +
			GoalkeeperSweeperLooseBallExtraTriggerDepth
			)
		{
			return LastTouchCharacter;
		}
	}

	return nullptr;
}

bool ASoccerAIController::IsGoalkeeperThreatInsideSweeperTriggerZone(
	const ASoccerAICharacter* SoccerCharacter,
	const FVector& ThreatLocation
) const
{
	if (!IsValid(SoccerCharacter))
	{
		return false;
	}

	const float ThreatDepth =
		GetGoalkeeperDepthFromGoal(
			SoccerCharacter,
			ThreatLocation
		);

	if (
		ThreatDepth < 0.0f ||
		ThreatDepth > GoalkeeperSweeperTriggerDepthFromGoal
		)
	{
		return false;
	}

	const float ThreatLateralOffset =
		FMath::Abs(
			GetGoalkeeperLateralOffsetFromGoal(
				SoccerCharacter,
				ThreatLocation
			)
		);

	if (ThreatLateralOffset > GoalkeeperSweeperTriggerHalfWidth)
	{
		return false;
	}

	return true;
}

bool ASoccerAIController::HasDefensiveHelpAgainstGoalkeeperThreat(
	const ASoccerAICharacter* SoccerCharacter,
	const ASoccerCharacterBase* ThreatCharacter,
	const FVector& ThreatLocation
) const
{
	if (
		!IsValid(SoccerCharacter) ||
		!IsValid(ThreatCharacter)
		)
	{
		return false;
	}

	if (
		bGoalkeeperGoalAreaIgnoreDefensiveHelpSuppression &&
		IsGoalkeeperGoalAreaAuthorityActiveAtLocation(
			SoccerCharacter,
			ThreatLocation
		)
		)
	{
		/*
		 * Dentro del �rea chica el arquero no delega una amenaza
		 * inmediata solo porque haya una camiseta propia cerca.
		 * Las comprobaciones de direcci�n, carrera y alcance siguen
		 * activas, por lo que esto no garantiza que consiga intervenir.
		 */
		return false;
	}

	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return false;
	}

	const FVector HomeLocation =
		GetGoalkeeperHomeLocation(SoccerCharacter);

	if (HomeLocation.IsNearlyZero())
	{
		return false;
	}

	const float ThreatDepth =
		GetGoalkeeperDepthFromGoal(
			SoccerCharacter,
			ThreatLocation
		);

	for (TActorIterator<ASoccerAICharacter> It(World); It; ++It)
	{
		const ASoccerAICharacter* Candidate = *It;

		if (!IsValid(Candidate))
		{
			continue;
		}

		if (Candidate == SoccerCharacter)
		{
			continue;
		}

		if (Candidate->GetTeam() != SoccerCharacter->GetTeam())
		{
			continue;
		}

		if (Candidate->GetPlayerRole() == ESoccerPlayerRole::Goalkeeper)
		{
			continue;
		}

		const FVector CandidateLocation =
			Candidate->GetActorLocation();

		const float CandidateDepth =
			GetGoalkeeperDepthFromGoal(
				SoccerCharacter,
				CandidateLocation
			);

		const bool bCandidateGoalSide =
			CandidateDepth <=
			ThreatDepth +
			GoalkeeperSweeperDefensiveHelpGoalSideDepthMargin;

		if (!bCandidateGoalSide)
		{
			continue;
		}

		const float DistanceCandidateToThreat =
			FVector::Dist2D(
				CandidateLocation,
				ThreatLocation
			);

		if (
			DistanceCandidateToThreat <=
			GoalkeeperSweeperDefensiveHelpMaxDistanceToThreat
			)
		{
			return true;
		}

		const FVector ThreatToGoal =
			HomeLocation - ThreatLocation;

		FVector ThreatToGoal2D = ThreatToGoal;
		ThreatToGoal2D.Z = 0.0f;

		const float LaneLength =
			ThreatToGoal2D.Size2D();

		if (LaneLength <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		const FVector LaneDirection =
			ThreatToGoal2D / LaneLength;

		const FVector ThreatToCandidate =
			CandidateLocation - ThreatLocation;

		const float CandidateAlongLane =
			FVector::DotProduct(
				ThreatToCandidate,
				LaneDirection
			);

		if (
			CandidateAlongLane < 0.0f ||
			CandidateAlongLane > LaneLength
			)
		{
			continue;
		}

		const FVector ClosestLanePoint =
			ThreatLocation + LaneDirection * CandidateAlongLane;

		const float CandidateDistanceToLane =
			FVector::Dist2D(
				CandidateLocation,
				ClosestLanePoint
			);

		if (
			CandidateDistanceToLane <=
			GoalkeeperSweeperDefensiveHelpLaneHalfWidth
			)
		{
			return true;
		}
	}

	return false;
}

bool ASoccerAIController::IsGoalkeeperThreatDrivingTowardGoal(
	const ASoccerAICharacter* SoccerCharacter,
	const ASoccerCharacterBase* ThreatCharacter
) const
{
	if (!IsValid(SoccerCharacter) || !IsValid(ThreatCharacter))
	{
		return false;
	}

	const FVector ThreatVelocity =
		ThreatCharacter->GetVelocity();

	FVector ThreatVelocity2D =
		ThreatVelocity;

	ThreatVelocity2D.Z = 0.0f;

	if (
		ThreatVelocity2D.Size2D() <
		GoalkeeperSweeperThreatMinSpeedForDirectionCheck
		)
	{
		// Si est� casi quieto pero en zona peligrosa, no bloqueamos la salida.
		return true;
	}

	ThreatVelocity2D =
		ThreatVelocity2D.GetSafeNormal();

	const FVector HomeLocation =
		GetGoalkeeperHomeLocation(SoccerCharacter);

	if (HomeLocation.IsNearlyZero())
	{
		return false;
	}

	FVector ThreatToGoal =
		HomeLocation - ThreatCharacter->GetActorLocation();

	ThreatToGoal.Z = 0.0f;
	ThreatToGoal = ThreatToGoal.GetSafeNormal();

	if (ThreatToGoal.IsNearlyZero())
	{
		return true;
	}

	const float TowardGoalDot =
		FVector::DotProduct(
			ThreatVelocity2D,
			ThreatToGoal
		);

	return TowardGoalDot >= GoalkeeperSweeperThreatTowardGoalMinDot;
}

bool ASoccerAIController::CanGoalkeeperWinSweeperRace(
	const ASoccerAICharacter* SoccerCharacter,
	const ASoccerBall* SoccerBall,
	const ASoccerCharacterBase* ThreatCharacter,
	const FVector& SweepTargetLocation
) const
{
	if (
		!IsValid(SoccerCharacter) ||
		!IsValid(SoccerBall) ||
		!IsValid(ThreatCharacter) ||
		SweepTargetLocation.IsNearlyZero()
		)
	{
		return false;
	}

	ASoccerCharacterBase* PossessingCharacter =
		IsValid(MatchManager)
		? MatchManager->GetPossessingCharacter()
		: nullptr;

	const bool bThreatPossessingBall =
		PossessingCharacter == ThreatCharacter;

	const FVector RaceTargetLocation =
		bThreatPossessingBall
		? SweepTargetLocation
		: PredictGoalkeeperSweeperBallLocation(SoccerBall);

	const float GoalkeeperTime =
		EstimateGoalkeeperGroundTravelTime(
			SoccerCharacter->GetActorLocation(),
			RaceTargetLocation
		);

	const float ThreatTime =
		EstimateThreatGroundTravelTime(
			ThreatCharacter->GetActorLocation(),
			RaceTargetLocation
		);

	const float DefenderTime =
		EstimateClosestDefenderGroundTravelTime(
			SoccerCharacter,
			RaceTargetLocation
		);

	const bool bIgnoreDefenderRaceSuppression =
		bGoalkeeperGoalAreaIgnoreDefensiveHelpSuppression &&
		(
			IsGoalkeeperGoalAreaAuthorityActiveAtLocation(
				SoccerCharacter,
				RaceTargetLocation
			)
			||
			IsGoalkeeperGoalAreaAuthorityActiveAtLocation(
				SoccerCharacter,
				ThreatCharacter->GetActorLocation()
			)
		);

	// Si un defensor llega claramente antes al punto de riesgo, el arquero no necesita salir.
	// Dentro del �rea chica esa cercan�a no cancela por s� sola la intervenci�n.
	if (
		!bIgnoreDefenderRaceSuppression &&
		DefenderTime < 100000.0f &&
		DefenderTime + GoalkeeperSweeperDefenderHelpArrivalMargin <= GoalkeeperTime &&
		DefenderTime <= ThreatTime + GoalkeeperSweeperDefenderHelpArrivalMargin
		)
	{
		return false;
	}

	// Pelota suelta: el arquero deber�a salir solo si puede ganarla o llegar casi al mismo tiempo.
	if (!bThreatPossessingBall)
	{
		return
			GoalkeeperTime <=
			ThreatTime + GoalkeeperSweeperRaceLoseTolerance;
	}

	// Atacante con pelota: no buscamos ganar una carrera limpia a la pelota,
	// buscamos achicar antes de que pueda definir c�modo.
	return
		GoalkeeperTime <=
		ThreatTime + GoalkeeperSweeperRaceLoseTolerance + GoalkeeperSweeperRaceWinMargin;
}

float ASoccerAIController::EstimateGoalkeeperGroundTravelTime(
	const FVector& FromLocation,
	const FVector& ToLocation
) const
{
	const float Speed =
		FMath::Max(GoalkeeperSweeperGoalkeeperTravelSpeed, 1.0f);

	return FVector::Dist2D(FromLocation, ToLocation) / Speed;
}

float ASoccerAIController::EstimateThreatGroundTravelTime(
	const FVector& FromLocation,
	const FVector& ToLocation
) const
{
	const float Speed =
		FMath::Max(GoalkeeperSweeperThreatTravelSpeed, 1.0f);

	return FVector::Dist2D(FromLocation, ToLocation) / Speed;
}

float ASoccerAIController::EstimateClosestDefenderGroundTravelTime(
	const ASoccerAICharacter* SoccerCharacter,
	const FVector& TargetLocation
) const
{
	if (!IsValid(SoccerCharacter) || TargetLocation.IsNearlyZero())
	{
		return 1000000.0f;
	}

	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return 1000000.0f;
	}

	float BestTime = 1000000.0f;

	for (TActorIterator<ASoccerAICharacter> It(World); It; ++It)
	{
		const ASoccerAICharacter* Candidate = *It;

		if (!IsValid(Candidate))
		{
			continue;
		}

		if (Candidate == SoccerCharacter)
		{
			continue;
		}

		if (Candidate->GetTeam() != SoccerCharacter->GetTeam())
		{
			continue;
		}

		if (Candidate->GetPlayerRole() == ESoccerPlayerRole::Goalkeeper)
		{
			continue;
		}

		const float CandidateTime =
			FVector::Dist2D(
				Candidate->GetActorLocation(),
				TargetLocation
			) /
			FMath::Max(GoalkeeperSweeperDefenderTravelSpeed, 1.0f);

		BestTime = FMath::Min(BestTime, CandidateTime);
	}

	return BestTime;
}

FVector ASoccerAIController::PredictGoalkeeperSweeperBallLocation(
	const ASoccerBall* SoccerBall
) const
{
	if (!IsValid(SoccerBall))
	{
		return FVector::ZeroVector;
	}

	FVector PredictedLocation =
		SoccerBall->GetActorLocation();

	FVector BallVelocity =
		SoccerBall->GetVelocity();

	BallVelocity.Z = 0.0f;

	PredictedLocation +=
		BallVelocity * GoalkeeperSweeperLooseBallPredictionTime;

	return PredictedLocation;
}

FVector ASoccerAIController::BuildGoalkeeperSweepTargetLocation(
	const ASoccerAICharacter* SoccerCharacter,
	const ASoccerBall* SoccerBall,
	const ASoccerCharacterBase* ThreatCharacter
) const
{
	if (!IsValid(SoccerCharacter) || !IsValid(SoccerBall))
	{
		return FVector::ZeroVector;
	}

	const FVector HomeLocation =
		GetGoalkeeperHomeLocation(SoccerCharacter);

	if (HomeLocation.IsNearlyZero())
	{
		return FVector::ZeroVector;
	}

	const FVector OutfieldDirection =
		GetGoalkeeperOutfieldDirection(SoccerCharacter);

	if (OutfieldDirection.IsNearlyZero())
	{
		return FVector::ZeroVector;
	}

	const FVector RightDirection =
		GetGoalkeeperRightDirection(SoccerCharacter);

	ASoccerCharacterBase* PossessingCharacter =
		IsValid(MatchManager)
		? MatchManager->GetPossessingCharacter()
		: nullptr;

	const bool bThreatPossessingBall =
		IsValid(ThreatCharacter) &&
		PossessingCharacter == ThreatCharacter;

	const FVector ReferenceLocation =
		bThreatPossessingBall
		? ThreatCharacter->GetActorLocation()
		: PredictGoalkeeperSweeperBallLocation(SoccerBall);

	FVector DirectionFromThreatToGoal =
		HomeLocation - ReferenceLocation;

	DirectionFromThreatToGoal.Z = 0.0f;
	DirectionFromThreatToGoal =
		DirectionFromThreatToGoal.GetSafeNormal();

	if (DirectionFromThreatToGoal.IsNearlyZero())
	{
		DirectionFromThreatToGoal =
			-OutfieldDirection;
	}

	FVector TargetLocation =
		ReferenceLocation +
		DirectionFromThreatToGoal *
		GoalkeeperSweeperCutTowardGoalOffset;

	float TargetDepth =
		GetGoalkeeperDepthFromGoal(
			SoccerCharacter,
			TargetLocation
		);

	TargetDepth =
		FMath::Clamp(
			TargetDepth,
			180.0f,
			GoalkeeperSweeperTriggerDepthFromGoal
		);

	const float TargetLateral =
		FMath::Clamp(
			GetGoalkeeperLateralOffsetFromGoal(
				SoccerCharacter,
				TargetLocation
			),
			-GoalkeeperSweeperTriggerHalfWidth,
			GoalkeeperSweeperTriggerHalfWidth
		);

	TargetLocation =
		HomeLocation +
		OutfieldDirection * TargetDepth +
		RightDirection * TargetLateral;

	TargetLocation.Z =
		SoccerCharacter->GetActorLocation().Z;

	return TargetLocation;
}

bool ASoccerAIController::TryResolveGoalkeeperSweeperBallContact(
	ASoccerAICharacter* SoccerCharacter,
	ASoccerBall* SoccerBall
)
{
	if (!IsValid(SoccerCharacter) || !IsValid(SoccerBall))
	{
		return false;
	}

	ASoccerCharacterBase* ThreatCharacter =
		GoalkeeperSweeperThreatCharacter.Get();

	if (
		TryStartGoalkeeperSweeperSmotherAction(
			SoccerCharacter,
			SoccerBall,
			ThreatCharacter
		)
		)
	{
		return true;
	}

	if (
		TryGoalkeeperCollectLooseBallWithHands(
			SoccerCharacter,
			SoccerBall
		)
		)
	{
		return true;
	}

	const float DistanceToBall =
		FVector::Dist2D(
			SoccerCharacter->GetActorLocation(),
			SoccerBall->GetActorLocation()
		);

	if (DistanceToBall > GoalkeeperSweeperFootCutDistance)
	{
		return false;
	}

	const UCapsuleComponent* CapsuleComponent =
		SoccerCharacter->GetCapsuleComponent();

	const float CharacterGroundZ =
		CapsuleComponent != nullptr
		? SoccerCharacter->GetActorLocation().Z -
		CapsuleComponent->GetScaledCapsuleHalfHeight()
		: SoccerCharacter->GetActorLocation().Z;

	const float BallHeight =
		SoccerBall->GetActorLocation().Z - CharacterGroundZ;

	if (
		BallHeight < -20.0f ||
		BallHeight > GoalkeeperSweeperMaxBallHeightForFeet
		)
	{
		return false;
	}

	return TryGoalkeeperClearBallWithFeet(
		SoccerCharacter,
		SoccerBall
	);
}

bool ASoccerAIController::TryStartGoalkeeperSweeperSmotherAction(
	ASoccerAICharacter* SoccerCharacter,
	ASoccerBall* SoccerBall,
	ASoccerCharacterBase* ThreatCharacter
)
{
	if (
		!bUseGoalkeeperSweeperSmother ||
		!IsValid(SoccerCharacter) ||
		!IsValid(SoccerBall) ||
		!IsValid(ThreatCharacter) ||
		!IsValid(MatchManager)
		)
	{
		return false;
	}

	if (
		SoccerCharacter->IsGoalkeeperActionActive() ||
		HasPendingGoalkeeperSaveActionFor(SoccerCharacter) ||
		HasPendingGoalkeeperSaveImpactFor(SoccerCharacter)
		)
	{
		return false;
	}

	if (ThreatCharacter->GetTeam() == SoccerCharacter->GetTeam())
	{
		return false;
	}

	const float CurrentTime =
		GetWorld() != nullptr
		? GetWorld()->GetTimeSeconds()
		: 0.0f;

	if (
		CurrentTime - LastGoalkeeperSweeperSmotherTime <
		GoalkeeperSweeperSmotherCooldown
		)
	{
		return false;
	}

	const float DistanceGoalkeeperToBall =
		FVector::Dist2D(
			SoccerCharacter->GetActorLocation(),
			SoccerBall->GetActorLocation()
		);

	if (DistanceGoalkeeperToBall > GoalkeeperSweeperSmotherBallDistance)
	{
		return false;
	}

	const float DistanceGoalkeeperToThreat =
		FVector::Dist2D(
			SoccerCharacter->GetActorLocation(),
			ThreatCharacter->GetActorLocation()
		);

	if (DistanceGoalkeeperToThreat > GoalkeeperSweeperSmotherStartDistance)
	{
		return false;
	}

	const bool bCanUseHands =
		IsGoalkeeperHandlingAllowedAtLocation(
			SoccerCharacter,
			SoccerBall->GetActorLocation()
		);

	const ESoccerGoalkeeperAction GoalkeeperAction =
		ChooseGoalkeeperSweeperSmotherAction(
			SoccerCharacter,
			SoccerBall,
			ThreatCharacter,
			bCanUseHands
		);

	if (
		GoalkeeperAction == ESoccerGoalkeeperAction::None ||
		GoalkeeperAction == ESoccerGoalkeeperAction::Miss
		)
	{
		return false;
	}

	const bool bActionStarted =
		SoccerCharacter->StartGoalkeeperAction(
			GoalkeeperAction
		);

	if (!bActionStarted)
	{
		return false;
	}

	PreparePendingGoalkeeperSweeperSaveImpact(
		SoccerCharacter,
		SoccerBall,
		GoalkeeperAction
	);

	LastGoalkeeperSweeperSmotherTime =
		CurrentTime;

	SetGoalkeeperBehaviorMode(
		ESoccerGoalkeeperBehaviorMode::Smothering
	);

	StopMovement();
	SetFocus(SoccerBall);

	if (GEngine)
	{
		const FString DebugText =
			FString::Printf(
				TEXT("GK sweeper smother: action %d | hands %s"),
				static_cast<int32>(GoalkeeperAction),
				bCanUseHands ? TEXT("true") : TEXT("false")
			);

		GEngine->AddOnScreenDebugMessage(
			-1,
			0.9f,
			FColor::Orange,
			DebugText
		);
	}

	return true;
}

ESoccerGoalkeeperAction ASoccerAIController::ChooseGoalkeeperSweeperSmotherAction(
	const ASoccerAICharacter* SoccerCharacter,
	const ASoccerBall* SoccerBall,
	const ASoccerCharacterBase* ThreatCharacter,
	bool bCanUseHands
) const
{
	if (!IsValid(SoccerCharacter) || !IsValid(SoccerBall))
	{
		return ESoccerGoalkeeperAction::None;
	}

	const UCapsuleComponent* CapsuleComponent =
		SoccerCharacter->GetCapsuleComponent();

	const float CharacterGroundZ =
		CapsuleComponent != nullptr
		? SoccerCharacter->GetActorLocation().Z -
		CapsuleComponent->GetScaledCapsuleHalfHeight()
		: SoccerCharacter->GetActorLocation().Z;

	const float BallHeight =
		SoccerBall->GetActorLocation().Z - CharacterGroundZ;

	if (BallHeight < -25.0f)
	{
		return ESoccerGoalkeeperAction::None;
	}

	const bool bLooseEnoughForHands =
		IsGoalkeeperBallLooseEnoughForHandSmother(
			SoccerCharacter,
			SoccerBall,
			ThreatCharacter
		);

	if (
		bCanUseHands &&
		bLooseEnoughForHands &&
		BallHeight <= GoalkeeperSweeperSmotherMaxBallHeightForHands
		)
	{
		return BallHeight <= GoalkeeperLowBallMaxHeight
			? ESoccerGoalkeeperAction::CatchLow
			: ESoccerGoalkeeperAction::CatchChest;
	}

	if (BallHeight > GoalkeeperSweeperSmotherMaxBallHeightForBodyBlock)
	{
		return ESoccerGoalkeeperAction::Miss;
	}

	const FVector ToBall =
		SoccerBall->GetActorLocation() -
		SoccerCharacter->GetActorLocation();

	const float SideOffset =
		FVector::DotProduct(
			ToBall,
			SoccerCharacter->GetActorRightVector()
		);

	return SideOffset > 0.0f
		? ESoccerGoalkeeperAction::BodyBlockRight
		: ESoccerGoalkeeperAction::BodyBlockLeft;
}

bool ASoccerAIController::IsGoalkeeperBallLooseEnoughForHandSmother(
	const ASoccerAICharacter* SoccerCharacter,
	const ASoccerBall* SoccerBall,
	const ASoccerCharacterBase* ThreatCharacter
) const
{
	if (!IsValid(SoccerCharacter) || !IsValid(SoccerBall))
	{
		return false;
	}

	ASoccerCharacterBase* PossessingCharacter =
		IsValid(MatchManager)
		? MatchManager->GetPossessingCharacter()
		: nullptr;

	if (!IsValid(PossessingCharacter))
	{
		return true;
	}

	if (PossessingCharacter == SoccerCharacter)
	{
		return true;
	}

	if (PossessingCharacter->GetTeam() == SoccerCharacter->GetTeam())
	{
		return false;
	}

	if (!IsValid(ThreatCharacter))
	{
		return false;
	}

	const float DistanceThreatToBall =
		FVector::Dist2D(
			ThreatCharacter->GetActorLocation(),
			SoccerBall->GetActorLocation()
		);

	return
		DistanceThreatToBall >=
		GoalkeeperSweeperSmotherLooseBallFromThreatDistance;
}

void ASoccerAIController::PreparePendingGoalkeeperSweeperSaveImpact(
	ASoccerAICharacter* SoccerCharacter,
	ASoccerBall* SoccerBall,
	ESoccerGoalkeeperAction GoalkeeperAction
)
{
	PreparePendingGoalkeeperSaveImpact(
		SoccerCharacter,
		SoccerBall,
		GoalkeeperAction
	);

}

void ASoccerAIController::ReleaseGoalkeeperOpponentBallPossession(
	ASoccerAICharacter* SoccerCharacter,
	ASoccerBall* SoccerBall
)
{
	if (
		!IsValid(SoccerCharacter) ||
		!IsValid(SoccerBall) ||
		!IsValid(MatchManager)
		)
	{
		return;
	}

	ASoccerCharacterBase* PossessingCharacter =
		MatchManager->GetPossessingCharacter();

	if (!IsValid(PossessingCharacter))
	{
		return;
	}

	if (PossessingCharacter == SoccerCharacter)
	{
		return;
	}

	if (PossessingCharacter->GetTeam() == SoccerCharacter->GetTeam())
	{
		return;
	}

	ASoccerAICharacter* PossessingAICharacter =
		Cast<ASoccerAICharacter>(PossessingCharacter);

	if (IsValid(PossessingAICharacter))
	{
		PossessingAICharacter->ReleaseAIBall();
		return;
	}

	AThirdPersonCppCharacter* PossessingHumanCharacter =
		Cast<AThirdPersonCppCharacter>(PossessingCharacter);

	if (IsValid(PossessingHumanCharacter))
	{
		PossessingHumanCharacter->ReleaseBallForAISteal();
	}
}

bool ASoccerAIController::TryGoalkeeperCollectLooseBallWithHands(
	ASoccerAICharacter* SoccerCharacter,
	ASoccerBall* SoccerBall
)
{
	if (!IsValid(SoccerCharacter) || !IsValid(SoccerBall))
	{
		return false;
	}

	if (
		!IsGoalkeeperHandlingAllowedAtLocation(
			SoccerCharacter,
			SoccerBall->GetActorLocation()
		)
		)
	{
		return false;
	}

	if (
		IsValid(MatchManager) &&
		MatchManager->GetPossessionTeam() != ESoccerPossessionTeam::None
		)
	{
		return false;
	}

	const float DistanceToBall =
		FVector::Dist2D(
			SoccerCharacter->GetActorLocation(),
			SoccerBall->GetActorLocation()
		);

	if (DistanceToBall > GoalkeeperSweeperHandCollectDistance)
	{
		return false;
	}

	const UCapsuleComponent* CapsuleComponent =
		SoccerCharacter->GetCapsuleComponent();

	const float CharacterGroundZ =
		CapsuleComponent != nullptr
		? SoccerCharacter->GetActorLocation().Z -
		CapsuleComponent->GetScaledCapsuleHalfHeight()
		: SoccerCharacter->GetActorLocation().Z;

	const float BallHeight =
		SoccerBall->GetActorLocation().Z - CharacterGroundZ;

	if (
		BallHeight < -20.0f ||
		BallHeight > GoalkeeperSweeperMaxBallHeightForHands
		)
	{
		return false;
	}

	if (
		IsValid(MatchManager) &&
		!MatchManager->CanCharacterTouchBallNow(SoccerCharacter)
		)
	{
		return false;
	}

	const bool bBallAttachedToHands =
		SoccerCharacter->HoldGoalkeeperBallInHands(
			SoccerBall
		);

	if (!bBallAttachedToHands)
	{
		// Mantiene el comportamiento anterior como fallback si el
		// socket de mano todav�a no est� configurado en esta malla.
		SoccerBall->SetPossessed(false);
		SoccerBall->StopBallKeepingPhysics();
		SoccerCharacter->PossessAIBall(SoccerBall);
	}

	if (IsValid(MatchManager))
	{
		MatchManager->RegisterControlledBallPossession(
			SoccerCharacter
		);
	}

	ClearGoalkeeperSweeperMode();

	SetGoalkeeperBehaviorMode(
		ESoccerGoalkeeperBehaviorMode::HoldingBall
	);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			0.9f,
			FColor::Green,
			TEXT("GK sweeper: agarro pelota suelta")
		);
	}

	return true;
}

bool ASoccerAIController::IsGoalkeeperAtFootClearanceContact(
	const ASoccerAICharacter* SoccerCharacter,
	const ASoccerBall* SoccerBall
) const
{
	if (!IsValid(SoccerCharacter) || !IsValid(SoccerBall))
	{
		return false;
	}

	const UCapsuleComponent* CapsuleComponent =
		SoccerCharacter->GetCapsuleComponent();

	const float CapsuleRadius =
		CapsuleComponent != nullptr
		? CapsuleComponent->GetScaledCapsuleRadius()
		: 42.0f;

	const float BallRadius =
		SoccerBall->GetBallRadiusCm();

	const float CenterDistance =
		FVector::Dist2D(
			SoccerCharacter->GetActorLocation(),
			SoccerBall->GetActorLocation()
		);

	const float SurfaceGap =
		FMath::Max(
			0.0f,
			CenterDistance - CapsuleRadius - BallRadius
		);

	if (SurfaceGap > GoalkeeperFootClearanceMaxSurfaceGap)
	{
		return false;
	}

	const float CharacterGroundZ =
		CapsuleComponent != nullptr
		? SoccerCharacter->GetActorLocation().Z -
			CapsuleComponent->GetScaledCapsuleHalfHeight()
		: SoccerCharacter->GetActorLocation().Z;

	const float BallHeight =
		SoccerBall->GetActorLocation().Z - CharacterGroundZ;

	if (
		BallHeight < GoalkeeperFootClearanceMinBallHeight ||
		BallHeight > GoalkeeperFootClearanceMaxBallHeight
		)
	{
		return false;
	}

	FVector ToBall =
		SoccerBall->GetActorLocation() -
		SoccerCharacter->GetActorLocation();

	ToBall.Z = 0.0f;
	ToBall = ToBall.GetSafeNormal();

	FVector ForwardDirection =
		SoccerCharacter->GetActorForwardVector();

	ForwardDirection.Z = 0.0f;
	ForwardDirection = ForwardDirection.GetSafeNormal();

	if (
		!ToBall.IsNearlyZero() &&
		!ForwardDirection.IsNearlyZero() &&
		FVector::DotProduct(ForwardDirection, ToBall) <
			GoalkeeperFootClearanceMinFacingDot
		)
	{
		return false;
	}

	return true;
}

bool ASoccerAIController::TryGoalkeeperClearBallWithFeet(
	ASoccerAICharacter* SoccerCharacter,
	ASoccerBall* SoccerBall
)
{
	if (!IsValid(SoccerCharacter) || !IsValid(SoccerBall))
	{
		return false;
	}

	if (
		!IsGoalkeeperAtFootClearanceContact(
			SoccerCharacter,
			SoccerBall
		)
		)
	{
		return false;
	}

	/*
	 * Un rechazo de emergencia con los pies siempre despeja largo. No depende
	 * de que exista un pase seguro: si el arquero llego a esta pelota, la saca.
	 */
	const FVector ClearanceTargetLocation =
		BuildGoalkeeperLongClearanceTargetLocation(
			SoccerCharacter,
			true
		);

	if (ClearanceTargetLocation.IsNearlyZero())
	{
		return false;
	}

	if (
		!TryGoalkeeperTakeBallForFootClearance(
			SoccerCharacter,
			SoccerBall
		)
		)
	{
		return false;
	}

	/*
	 * Desde aqui existe una posesion logica transitoria de pies. Cualquier
	 * fallo debe liberarla antes de salir para que nunca quede atrapada en el
	 * flujo de HoldingBall (que exige una pelota realmente en las manos).
	 */
	if (!TryRegisterAIKickTouchForRules(SoccerCharacter))
	{
		SoccerCharacter->ReleaseAIBall();
		return false;
	}

	if (
		!SoccerCharacter->IsAIPossessingBall() ||
		SoccerCharacter->GetControlledAIBall() != SoccerBall
		)
	{
		SoccerCharacter->ReleaseAIBall();
		return false;
	}

	SoccerCharacter->KickAIBallToTarget(
		ClearanceTargetLocation,
		GoalkeeperSweeperClearanceHorizontalSpeed,
		GoalkeeperSweeperClearanceMinTravelTime,
		GoalkeeperSweeperClearanceMaxTravelTime
	);

	if (SoccerCharacter->IsAIPossessingBall())
	{
		SoccerCharacter->ReleaseAIBall();
		return false;
	}

	ClearGoalkeeperLooseBallClaimMode();
	ClearGoalkeeperSweeperMode();

	SetGoalkeeperBehaviorMode(
		ESoccerGoalkeeperBehaviorMode::Retreating
	);

	MarkGoalkeeperSweeperResolved();

	return true;
}

bool ASoccerAIController::TryGoalkeeperTakeBallForFootClearance(
	ASoccerAICharacter* SoccerCharacter,
	ASoccerBall* SoccerBall
)
{
	if (
		!IsValid(SoccerCharacter) ||
		!IsValid(SoccerBall) ||
		!IsValid(MatchManager)
		)
	{
		return false;
	}

	if (!MatchManager->CanCharacterTouchBallNow(SoccerCharacter))
	{
		return false;
	}

	ASoccerCharacterBase* PossessingCharacter =
		MatchManager->GetPossessingCharacter();

	if (IsValid(PossessingCharacter))
	{
		if (PossessingCharacter == SoccerCharacter)
		{
			// El MatchManager puede conservar la posesion un Tick mas que el
			// personaje. Sincronizamos tambien el estado local que necesita Kick.
			if (!SoccerCharacter->IsAIPossessingBall())
			{
				SoccerCharacter->PossessAIBall(SoccerBall);
			}

			return
				SoccerCharacter->IsAIPossessingBall() &&
				SoccerCharacter->GetControlledAIBall() == SoccerBall;
		}

		if (PossessingCharacter->GetTeam() == SoccerCharacter->GetTeam())
		{
			return false;
		}

		ASoccerAICharacter* PossessingAICharacter =
			Cast<ASoccerAICharacter>(PossessingCharacter);

		if (IsValid(PossessingAICharacter))
		{
			PossessingAICharacter->ReleaseAIBall();
		}
		else
		{
			AThirdPersonCppCharacter* PossessingHumanCharacter =
				Cast<AThirdPersonCppCharacter>(PossessingCharacter);

			if (IsValid(PossessingHumanCharacter))
			{
				PossessingHumanCharacter->ReleaseBallForAISteal();
			}
			else
			{
				return false;
			}
		}
	}

	if (!SoccerCharacter->IsAIPossessingBall())
	{
		SoccerCharacter->PossessAIBall(SoccerBall);
	}

	return SoccerCharacter->IsAIPossessingBall();
}

void ASoccerAIController::StartGoalkeeperSweeperMode(
	ASoccerCharacterBase* ThreatCharacter,
	const FVector& SweepTargetLocation
)
{
	if (!bGoalkeeperSweeperActive)
	{
		GoalkeeperSweeperStartTime =
			GetWorld() != nullptr
			? GetWorld()->GetTimeSeconds()
			: 0.0f;
	}

	bGoalkeeperSweeperActive = true;
	GoalkeeperSweeperTargetLocation =
		SweepTargetLocation;
	GoalkeeperSweeperThreatCharacter =
		ThreatCharacter;

	SetGoalkeeperBehaviorMode(
		ESoccerGoalkeeperBehaviorMode::Sweeping
	);
}

void ASoccerAIController::ClearGoalkeeperSweeperMode()
{
	bGoalkeeperSweeperActive = false;
	GoalkeeperSweeperStartTime = -1000.0f;
	GoalkeeperSweeperTargetLocation = FVector::ZeroVector;
	GoalkeeperSweeperThreatCharacter = nullptr;
}

bool ASoccerAIController::TryUpdateGoalkeeperRetreatBehavior(
	ASoccerAICharacter* SoccerCharacter,
	ASoccerBall* SoccerBall,
	float DeltaTime
)
{
	if (!ShouldGoalkeeperRetreatToHome(SoccerCharacter))
	{
		ClearGoalkeeperRetreatMode();
		return false;
	}

	const FVector RetreatTargetLocation =
		GetGoalkeeperRetreatTargetLocation(SoccerCharacter);

	if (RetreatTargetLocation.IsNearlyZero())
	{
		ClearGoalkeeperRetreatMode();
		return false;
	}

	FVector RetreatLocation = RetreatTargetLocation;
	RetreatLocation.Z = SoccerCharacter->GetActorLocation().Z;

	const float DistanceToRetreatTarget =
		FVector::Dist2D(
			SoccerCharacter->GetActorLocation(),
			RetreatLocation
		);

	if (
		bGoalkeeperRetreatActive &&
		DistanceToRetreatTarget <= GoalkeeperRetreatFinishedDistanceFromHome
		)
	{
		ClearGoalkeeperRetreatMode();
		return false;
	}

	bGoalkeeperRetreatActive = true;

	ClearGoalkeeperSweeperMode();

	SetGoalkeeperBehaviorMode(
		ESoccerGoalkeeperBehaviorMode::Retreating
	);

	SoccerCharacter->RequestAIMovementMode(
		ESoccerAIMovementMode::FastRun,
		ESoccerAIMovementReason::GoalkeeperEmergency,
		true
	);

	const float CurrentTime =
		GetWorld() != nullptr
		? GetWorld()->GetTimeSeconds()
		: 0.0f;

	const bool bShouldRefreshMove =
		LastGoalkeeperRetreatMoveTarget.IsNearlyZero() ||
		FVector::Dist2D(
			LastGoalkeeperRetreatMoveTarget,
			RetreatLocation
		) >= GoalkeeperRetreatRepathDistanceThreshold ||
		CurrentTime - LastGoalkeeperRetreatMoveRequestTime >=
		GoalkeeperRetreatMoveRefreshInterval;

	if (bShouldRefreshMove)
	{
		MoveToLocationWithAIMovement(
			ESoccerAIOrder::ReturnHome,
			RetreatLocation,
			GoalkeeperRetreatAcceptanceRadius,
			true
		);

		LastGoalkeeperRetreatMoveTarget = RetreatLocation;
		LastGoalkeeperRetreatMoveRequestTime = CurrentTime;
	}

	if (IsValid(SoccerBall))
	{
		const float DistanceToBall =
			FVector::Dist2D(
				SoccerCharacter->GetActorLocation(),
				SoccerBall->GetActorLocation()
			);

		if (
			bGoalkeeperRetreatActive ||
			DistanceToBall <= GoalkeeperRetreatFaceBallMaxDistance
			)
		{
			SetFocus(SoccerBall);

			FacePawnTowardLocation(
				SoccerBall->GetActorLocation(),
				DeltaTime
			);
		}
		else
		{
			SetFocalPoint(
				RetreatLocation,
				EAIFocusPriority::Gameplay
			);
		}
	}
	else
	{
		SetFocalPoint(
			RetreatLocation,
			EAIFocusPriority::Gameplay
		);
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			0.12f,
			FColor::Silver,
			TEXT("GK retreat: recuperando posicion dinamica")
		);
	}

	return true;
}

bool ASoccerAIController::ShouldGoalkeeperRetreatToHome(
	const ASoccerAICharacter* SoccerCharacter
) const
{
	if (
		!bUseGoalkeeperSmartRetreat ||
		!IsValid(SoccerCharacter) ||
		SoccerCharacter->IsGoalkeeperActionActive() ||
		SoccerCharacter->IsAIPossessingBall() ||
		HasPendingGoalkeeperSaveActionFor(SoccerCharacter) ||
		HasPendingGoalkeeperSaveImpactFor(SoccerCharacter)
		)
	{
		return false;
	}

	const FVector RetreatTargetLocation =
		GetGoalkeeperRetreatTargetLocation(SoccerCharacter);

	if (RetreatTargetLocation.IsNearlyZero())
	{
		return false;
	}

	const float DistanceToRetreatTarget =
		FVector::Dist2D(
			SoccerCharacter->GetActorLocation(),
			RetreatTargetLocation
		);

	if (bGoalkeeperRetreatActive)
	{
		return DistanceToRetreatTarget > GoalkeeperRetreatFinishedDistanceFromHome;
	}

	return DistanceToRetreatTarget >= GoalkeeperRetreatStartDistanceFromHome;
}

void ASoccerAIController::ClearGoalkeeperRetreatMode()
{
	bGoalkeeperRetreatActive = false;
	LastGoalkeeperRetreatMoveTarget = FVector::ZeroVector;
	LastGoalkeeperRetreatMoveRequestTime = -1000.0f;
}

void ASoccerAIController::MarkGoalkeeperSweeperResolved()
{
	LastGoalkeeperSweeperResolvedTime =
		GetWorld() != nullptr
		? GetWorld()->GetTimeSeconds()
		: 0.0f;

	bGoalkeeperRetreatActive = true;
	LastGoalkeeperRetreatMoveTarget = FVector::ZeroVector;
	LastGoalkeeperRetreatMoveRequestTime = -1000.0f;
}

bool ASoccerAIController::IsGoalkeeperActivelyClaimingBall() const
{
	return
		CurrentGoalkeeperBehaviorMode ==
			ESoccerGoalkeeperBehaviorMode::ShotPending ||
		CurrentGoalkeeperBehaviorMode ==
			ESoccerGoalkeeperBehaviorMode::Saving ||
		CurrentGoalkeeperBehaviorMode ==
			ESoccerGoalkeeperBehaviorMode::Sweeping ||
		CurrentGoalkeeperBehaviorMode ==
			ESoccerGoalkeeperBehaviorMode::Smothering;
}

void ASoccerAIController::SetGoalkeeperBehaviorMode(
	ESoccerGoalkeeperBehaviorMode NewMode
)
{
	CurrentGoalkeeperBehaviorMode = NewMode;
}

FVector ASoccerAIController::GetGoalkeeperHomeLocation(
	const ASoccerAICharacter* SoccerCharacter
) const
{
	if (!IsValid(SoccerCharacter))
	{
		return FVector::ZeroVector;
	}

	AActor* HomePositionActor =
		SoccerCharacter->GetHomePositionActor();

	FVector HomeLocation =
		IsValid(HomePositionActor)
		? HomePositionActor->GetActorLocation()
		: SoccerCharacter->GetActorLocation();

	if (IsValid(MatchManager))
	{
		HomeLocation = MatchManager->GetTeamRebasedFieldReferenceLocation(
			SoccerCharacter->GetTeam(),
			HomeLocation
		);
	}

	return HomeLocation;
}

FVector ASoccerAIController::GetGoalkeeperRetreatTargetLocation(
	const ASoccerAICharacter* SoccerCharacter
) const
{
	if (!IsValid(SoccerCharacter))
	{
		return FVector::ZeroVector;
	}

	/*
	 * El modo Retreat existe para que el arquero vuelva rapido despues de
	 * una salida, un rechazo o una distribucion. Antes usaba directamente
	 * HomePositionActor, por lo que mientras Retreat estaba activo se
	 * salteaba por completo el posicionamiento geometrico normal.
	 *
	 * La recuperacion debe apuntar al MISMO destino dinamico que usara el
	 * arquero al volver a Positioning. De ese modo Retreat solo cambia la
	 * urgencia/velocidad de regreso y nunca reemplaza la logica lateral.
	 */
	if (IsValid(MatchManager))
	{
		const FVector DynamicPositioningLocation =
			MatchManager->GetGoalkeeperMoveLocation(SoccerCharacter);

		if (!DynamicPositioningLocation.IsNearlyZero())
		{
			return ApplyGoalkeeperProfilePositioningExecution(
				SoccerCharacter,
				DynamicPositioningLocation
			);
		}
	}

	return GetGoalkeeperHomeLocation(SoccerCharacter);
}

FVector ASoccerAIController::GetGoalkeeperOutfieldDirection(
	const ASoccerAICharacter* SoccerCharacter
) const
{
	if (!IsValid(SoccerCharacter))
	{
		return FVector::ZeroVector;
	}

	if (IsValid(MatchManager))
	{
		FVector OutfieldDirection =
			MatchManager->GetFieldAttackDirectionForTeam(
				SoccerCharacter->GetTeam()
			);
		OutfieldDirection.Z = 0.0f;
		OutfieldDirection = OutfieldDirection.GetSafeNormal();
		if (!OutfieldDirection.IsNearlyZero())
		{
			return OutfieldDirection;
		}
	}

	AActor* HomePositionActor = SoccerCharacter->GetHomePositionActor();
	if (IsValid(HomePositionActor))
	{
		FVector OutfieldDirection = HomePositionActor->GetActorForwardVector();
		OutfieldDirection.Z = 0.0f;
		OutfieldDirection = OutfieldDirection.GetSafeNormal();
		if (!OutfieldDirection.IsNearlyZero())
		{
			return OutfieldDirection;
		}
	}

	FVector OutfieldDirection = SoccerCharacter->GetActorForwardVector();
	OutfieldDirection.Z = 0.0f;
	return OutfieldDirection.GetSafeNormal();
}

FVector ASoccerAIController::GetGoalkeeperRightDirection(
	const ASoccerAICharacter* SoccerCharacter
) const
{
	if (!IsValid(SoccerCharacter))
	{
		return FVector::RightVector;
	}

	const FVector OutfieldDirection =
		GetGoalkeeperOutfieldDirection(SoccerCharacter);

	if (OutfieldDirection.IsNearlyZero())
	{
		return FVector::RightVector;
	}

	FVector RightDirection =
		FVector::CrossProduct(
			FVector::UpVector,
			OutfieldDirection
		);

	RightDirection.Z = 0.0f;
	RightDirection = RightDirection.GetSafeNormal();

	if (RightDirection.IsNearlyZero())
	{
		return FVector::RightVector;
	}

	return RightDirection;
}

float ASoccerAIController::GetGoalkeeperDepthFromGoal(
	const ASoccerAICharacter* SoccerCharacter,
	const FVector& Location
) const
{
	if (!IsValid(SoccerCharacter))
	{
		return 0.0f;
	}

	const FVector OutfieldDirection =
		GetGoalkeeperOutfieldDirection(SoccerCharacter);
	if (OutfieldDirection.IsNearlyZero())
	{
		return 0.0f;
	}

	FVector GoalReference = GetGoalkeeperHomeLocation(SoccerCharacter);
	if (IsValid(MatchManager))
	{
		const ASoccerField* SoccerField = MatchManager->GetSoccerField();
		if (IsValid(SoccerField))
		{
			GoalReference = SoccerField->GetGoalCenterWorldLocation(
				MatchManager->GetOwnGoalLineSign(SoccerCharacter->GetTeam())
			);
		}
	}

	if (GoalReference.IsNearlyZero())
	{
		return 0.0f;
	}

	return FVector::DotProduct(
		Location - GoalReference,
		OutfieldDirection
	);
}

float ASoccerAIController::GetGoalkeeperLateralOffsetFromGoal(
	const ASoccerAICharacter* SoccerCharacter,
	const FVector& Location
) const
{
	if (!IsValid(SoccerCharacter))
	{
		return 0.0f;
	}

	const FVector RightDirection =
		GetGoalkeeperRightDirection(SoccerCharacter);
	if (RightDirection.IsNearlyZero())
	{
		return 0.0f;
	}

	FVector GoalReference = GetGoalkeeperHomeLocation(SoccerCharacter);
	if (IsValid(MatchManager))
	{
		const ASoccerField* SoccerField = MatchManager->GetSoccerField();
		if (IsValid(SoccerField))
		{
			GoalReference = SoccerField->GetGoalCenterWorldLocation(
				MatchManager->GetOwnGoalLineSign(SoccerCharacter->GetTeam())
			);
		}
	}

	if (GoalReference.IsNearlyZero())
	{
		return 0.0f;
	}

	return FVector::DotProduct(
		Location - GoalReference,
		RightDirection
	);
}

bool ASoccerAIController::IsLocationInsideGoalkeeperPenaltyArea(
	const ASoccerAICharacter* SoccerCharacter,
	const FVector& Location
) const
{
	if (!IsValid(SoccerCharacter))
	{
		return false;
	}

	if (IsValid(MatchManager))
	{
		const float SafeMargin = FMath::Max(
			0.0f,
			GoalkeeperPenaltyAreaHandlingMargin
		);
		return MatchManager->IsLocationInsidePenaltyAreaForTeam(
			Location,
			SoccerCharacter->GetTeam(),
			SafeMargin,
			SafeMargin
		);
	}

	const float Depth =
		GetGoalkeeperDepthFromGoal(
			SoccerCharacter,
			Location
		);

	if (
		Depth < -GoalkeeperPenaltyAreaHandlingMargin ||
		Depth > GoalkeeperPenaltyAreaDepth + GoalkeeperPenaltyAreaHandlingMargin
		)
	{
		return false;
	}

	const float LateralOffset =
		FMath::Abs(
			GetGoalkeeperLateralOffsetFromGoal(
				SoccerCharacter,
				Location
			)
		);

	return
		LateralOffset <=
		GoalkeeperPenaltyAreaHalfWidth +
		GoalkeeperPenaltyAreaHandlingMargin;
}

bool ASoccerAIController::IsLocationInsideGoalkeeperGoalArea(
	const ASoccerAICharacter* SoccerCharacter,
	const FVector& Location,
	float ExtraMargin
) const
{
	if (!IsValid(SoccerCharacter))
	{
		return false;
	}

	const float SafeMargin =
		FMath::Max(0.0f, ExtraMargin);

	if (IsValid(MatchManager))
	{
		return MatchManager->IsLocationInsideGoalAreaForTeam(
			Location,
			SoccerCharacter->GetTeam(),
			SafeMargin
		);
	}

	const float Depth =
		GetGoalkeeperDepthFromGoal(
			SoccerCharacter,
			Location
		);

	if (
		Depth < -SafeMargin ||
		Depth >
		SoccerFieldDimensions::GoalAreaDepthCm +
		SafeMargin
		)
	{
		return false;
	}

	const float LateralOffset =
		FMath::Abs(
			GetGoalkeeperLateralOffsetFromGoal(
				SoccerCharacter,
				Location
			)
		);

	return
		LateralOffset <=
		SoccerFieldDimensions::GoalAreaHalfWidthCm +
		SafeMargin;
}

bool ASoccerAIController::
IsGoalkeeperGoalAreaAuthorityActiveAtLocation(
	const ASoccerAICharacter* SoccerCharacter,
	const FVector& Location
) const
{
	return
		bUseGoalkeeperGoalAreaAuthority &&
		IsLocationInsideGoalkeeperGoalArea(
			SoccerCharacter,
			Location,
			GoalkeeperGoalAreaAuthorityMargin
		);
}

float ASoccerAIController::GetGoalkeeperRequiredShotMinSpeed(
	const ASoccerAICharacter* SoccerCharacter,
	const FVector& BallLocation
) const
{
	if (
		IsGoalkeeperGoalAreaAuthorityActiveAtLocation(
			SoccerCharacter,
			BallLocation
		)
		)
	{
		return FMath::Min(
			FMath::Max(0.0f, GoalkeeperShotMinSpeed),
			FMath::Max(0.0f, GoalkeeperGoalAreaShotMinSpeed)
		);
	}

	return FMath::Max(0.0f, GoalkeeperShotMinSpeed);
}

bool ASoccerAIController::
TryPredictGoalkeeperReferencePlaneCrossing(
	const ASoccerAICharacter* SoccerCharacter,
	const ASoccerBall* SoccerBall,
	FVector& OutPredictedLocation,
	float& OutTimeToReferencePlane,
	float MaxReferenceTimeOverride
) const
{
	OutPredictedLocation =
		FVector::ZeroVector;

	OutTimeToReferencePlane =
		0.0f;

	if (
		!IsValid(SoccerCharacter) ||
		!IsValid(SoccerBall)
		)
	{
		return false;
	}

	const FVector BallLocation =
		SoccerBall->GetActorLocation();

	const FVector BallVelocity =
		SoccerBall->GetVelocity();

	const float RequiredShotMinSpeed =
		GetGoalkeeperRequiredShotMinSpeed(
			SoccerCharacter,
			BallLocation
		);

	if (
		BallVelocity.Size() <
		RequiredShotMinSpeed
		)
	{
		return false;
	}

	FVector HorizontalBallVelocity =
		BallVelocity;

	HorizontalBallVelocity.Z =
		0.0f;

	const FVector HorizontalBallDirection =
		HorizontalBallVelocity.
		GetSafeNormal();

	if (
		HorizontalBallDirection.
		IsNearlyZero()
		)
	{
		return false;
	}

	const FVector HomeLocation =
		GetGoalkeeperHomeLocation(
			SoccerCharacter
		);

	if (HomeLocation.IsNearlyZero())
	{
		return false;
	}

	FVector TowardGoal =
		HomeLocation -
		BallLocation;

	TowardGoal.Z =
		0.0f;

	TowardGoal =
		TowardGoal.GetSafeNormal();

	if (TowardGoal.IsNearlyZero())
	{
		return false;
	}

	const float TowardGoalDot =
		FVector::DotProduct(
			HorizontalBallDirection,
			TowardGoal
		);

	if (
		TowardGoalDot <
		GoalkeeperInterventionMinTowardGoalDot
		)
	{
		return false;
	}

	const FVector GoalOutfieldDirection =
		GetGoalkeeperOutfieldDirection(
			SoccerCharacter
		);

	if (GoalOutfieldDirection.IsNearlyZero())
	{
		return false;
	}

	/*
	 * La normal apunta desde el campo hacia adentro del arco.
	 * As� la predicci�n se hace sobre la l�nea real del
	 * arco y no sobre la posici�n moment�nea del arquero.
	 */
	const FVector GoalPlaneNormal =
		-GoalOutfieldDirection;

	const float MaximumReferenceTime =
		MaxReferenceTimeOverride > 0.0f
		? MaxReferenceTimeOverride
		: GoalkeeperSaveDecisionTimeHorizon;

	/*
	 * Separamos dos referencias:
	 *
	 * - la LINEA DE GOL confirma que la trayectoria amenaza
	 *   realmente la boca del arco;
	 * - la POSICION ACTUAL DEL ARQUERO determina cuando entra
	 *   la pelota en el horizonte temporal de decision.
	 *
	 * Esto evita retrasar el selector cuando el arquero esta
	 * varios metros adelantado respecto de HomeLocation.
	 */
	const float GoalThreatPredictionTime =
		FMath::Max(
			MaximumReferenceTime,
			GoalkeeperLivePendingSaveMaxPredictionTime
		);

	FVector GoalLinePredictedLocation =
		FVector::ZeroVector;

	float TimeToGoalLine =
		0.0f;

	if (
		!SoccerBall->
		PredictCrossingOfHorizontalPlane(
			HomeLocation,
			GoalThreatPredictionTime,
			TimeToGoalLine,
			GoalLinePredictedLocation,
			GoalPlaneNormal
		)
		)
	{
		return false;
	}

	const FVector GoalRightDirection =
		GetGoalkeeperRightDirection(
			SoccerCharacter
		);

	if (GoalRightDirection.IsNearlyZero())
	{
		return false;
	}

	const float PredictedGoalLineLateralOffset =
		FVector::DotProduct(
			GoalLinePredictedLocation -
			HomeLocation,
			GoalRightDirection
		);

	const float DangerousHalfWidth =
		FMath::Max(
			0.0f,
			GoalkeeperGoalHalfWidth
		) +
		FMath::Max(
			0.0f,
			GoalkeeperShotDangerSideMargin
		);

	/*
	 * La amenaza sigue definiendose exclusivamente sobre la
	 * linea de gol. Adelantar al arquero no convierte un tiro
	 * desviado en una pelota dirigida al arco.
	 */
	if (
		FMath::Abs(
			PredictedGoalLineLateralOffset
		) >
		DangerousHalfWidth
		)
	{
		return false;
	}

	/*
	 * Confirmada la amenaza, el reloj se mide contra un plano
	 * paralelo a la linea de gol que pasa por la posicion ACTUAL
	 * del arquero.
	 */
	const FVector CurrentGoalkeeperLocation =
		SoccerCharacter->GetActorLocation();

	if (
		!SoccerBall->
		PredictCrossingOfHorizontalPlane(
			CurrentGoalkeeperLocation,
			MaximumReferenceTime,
			OutTimeToReferencePlane,
			OutPredictedLocation,
			GoalPlaneNormal
		)
		)
	{
		return false;
	}

	return true;
}

bool ASoccerAIController::
TryPredictBallAtGoalkeeperCandidatePlane(
	const ASoccerBall* SoccerBall,
	const FVector& CandidatePlaneLocation,
	float& OutTimeToCandidatePlane,
	FVector& OutPredictedBallLocation
) const
{
	OutTimeToCandidatePlane =
		0.0f;

	OutPredictedBallLocation =
		FVector::ZeroVector;

	if (!IsValid(SoccerBall))
	{
		return false;
	}

	const float MaximumCandidateTime =
		FMath::Max(
			GoalkeeperLivePendingSaveMaxPredictionTime,
			GoalkeeperSaveDecisionTimeHorizon +
			0.75f
		);

	return SoccerBall->
		PredictCrossingOfHorizontalPlane(
			CandidatePlaneLocation,
			MaximumCandidateTime,
			OutTimeToCandidatePlane,
			OutPredictedBallLocation
		);
}

bool ASoccerAIController::
IsGoalkeeperOverheadMissSituation(
	const ASoccerAICharacter* SoccerCharacter,
	const FVector& ReferencePredictedLocation
) const
{
	if (!IsValid(SoccerCharacter))
	{
		return false;
	}

	USkeletalMeshComponent* CharacterMesh =
		SoccerCharacter->GetMesh();

	if (CharacterMesh == nullptr)
	{
		return false;
	}

	static const FName HeadBone(
		TEXT("Head")
	);

	if (
		CharacterMesh->GetBoneIndex(
			HeadBone
		) == INDEX_NONE
		)
	{
		return false;
	}

	const FVector HeadLocation =
		CharacterMesh->GetBoneLocation(
			HeadBone,
			EBoneSpaces::WorldSpace
		);

	FVector RightDirection =
		SoccerCharacter->
		GetActorRightVector();

	RightDirection.Z =
		0.0f;

	RightDirection =
		RightDirection.GetSafeNormal();

	if (RightDirection.IsNearlyZero())
	{
		return false;
	}

	const FVector FromGoalkeeperToBall =
		ReferencePredictedLocation -
		SoccerCharacter->GetActorLocation();

	const float LateralOffset =
		FVector::DotProduct(
			FromGoalkeeperToBall,
			RightDirection
		);

	const float HeightAboveHead =
		ReferencePredictedLocation.Z -
		HeadLocation.Z;

	const bool bNearlyCentral =
		FMath::Abs(LateralOffset) <=
		GoalkeeperMissMaximumLateralOffset;

	const bool bHighEnough =
		HeightAboveHead >=
		GoalkeeperMissMinimumHeightAboveHead;

	const bool bNotAbsurdlyHigh =
		HeightAboveHead <=
		GoalkeeperMissMaximumHeightAboveHead;

	return
		bNearlyCentral &&
		bHighEnough &&
		bNotAbsurdlyHigh;
}

bool ASoccerAIController::
TryScoreGoalkeeperSaveActionAgainstBall(
	const ASoccerAICharacter* SoccerCharacter,
	const ASoccerBall* SoccerBall,
	ESoccerGoalkeeperAction GoalkeeperAction,
	FGoalkeeperSaveSelectionResult&
	OutBestFeasibleSelection,
	FGoalkeeperSaveSelectionResult&
	OutBestVisualSelection
) const
{
	OutBestFeasibleSelection =
		FGoalkeeperSaveSelectionResult();

	OutBestVisualSelection =
		FGoalkeeperSaveSelectionResult();

	if (
		!IsValid(SoccerCharacter) ||
		!IsValid(SoccerBall) ||
		GoalkeeperAction ==
		ESoccerGoalkeeperAction::None
		)
	{
		return false;
	}

	USkeletalMeshComponent* CharacterMesh =
		SoccerCharacter->GetMesh();

	if (CharacterMesh == nullptr)
	{
		return false;
	}

	float ContactWindowStartTime = 0.0f;
	float ContactWindowEndTime = 0.0f;

	if (
		!TryGetGoalkeeperContactWindowRangeForAction(
			SoccerCharacter,
			GoalkeeperAction,
			ContactWindowStartTime,
			ContactWindowEndTime
		)
		)
	{
		return false;
	}

	/*
	 * Origen absoluto usado por las CurveTables:
	 * origen mundial del Skeletal Mesh durante la
	 * evaluaci�n, antes de sumar el movimiento futuro
	 * de la c�psula.
	 */
	const FVector BaseMeshOriginLocation =
		CharacterMesh->GetComponentLocation();

	FVector ForwardDirection =
		SoccerCharacter->
		GetActorForwardVector();

	ForwardDirection.Z =
		0.0f;

	ForwardDirection =
		ForwardDirection.GetSafeNormal();

	FVector RightDirection =
		SoccerCharacter->
		GetActorRightVector();

	RightDirection.Z =
		0.0f;

	RightDirection =
		RightDirection.GetSafeNormal();

	if (
		ForwardDirection.IsNearlyZero() ||
		RightDirection.IsNearlyZero()
		)
	{
		return false;
	}

	const bool bUseTwoHandCatchZone =
		IsGoalkeeperCatchAction(
			GoalkeeperAction
		);

	const int32 SampleCount =
		FMath::Clamp(
			GoalkeeperHandTrackWindowSamples,
			2,
			11
		);

	const float SafeWindowEnd =
		FMath::Max(
			ContactWindowStartTime,
			ContactWindowEndTime -
			0.001f
		);

	/*
	 * Guarda un candidato ya resuelto.
	 *
	 * Para Catch, PredictedContactLocation es el punto
	 * mas cercano dentro del segmento entre ambas manos.
	 *
	 * Para Deflect y Miss, continua siendo una mano
	 * individual.
	 *
	 * Antes de puntuar podemos extender, de manera limitada, el movimiento
	 * lateral que ya describe la CurveTable de la capsula. Como esa extension
	 * es perpendicular al plano de contacto, no altera el tiempo de cruce de
	 * la pelota: solo traslada lateralmente manos y zona de captura.
	 */
	auto CommitCandidate =
		[
			this,
			SoccerCharacter,
			SoccerBall,
			GoalkeeperAction,
			&ForwardDirection,
			&RightDirection,
			&OutBestFeasibleSelection,
			&OutBestVisualSelection
		](
			float SampleTime,
			const FVector& PredictedContactLocation,
			const FVector& PredictedLeftHandLocation,
			const FVector& PredictedRightHandLocation,
			bool bLeftHand,
			float BallTimeToPlane,
			const FVector& PredictedBallLocation,
			float CapsuleLateralOffset
			)
	{
		const float SpatialErrorBeforeAdaptive =
			FVector::Dist(
				PredictedBallLocation,
				PredictedContactLocation
			);

		FVector AdaptedLeftHandLocation =
			PredictedLeftHandLocation;

		FVector AdaptedRightHandLocation =
			PredictedRightHandLocation;

		FVector AdaptedContactLocation =
			PredictedContactLocation;

		float EffectiveBallTimeToPlane =
			BallTimeToPlane;

		FVector EffectivePredictedBallLocation =
			PredictedBallLocation;

		float AdaptiveLateralScale = 1.0f;
		float AdaptiveLateralExtraDistance = 0.0f;

		const float MinimumAdaptiveLateralScale =
			FMath::Clamp(
				GoalkeeperAdaptiveLateralMinimumScale,
				0.0f,
				1.0f
			);

		const float MaximumAdaptiveLateralScale =
			FMath::Max(
				1.0f,
				GoalkeeperAdaptiveLateralMaximumScale
			);

		const bool bAdaptiveScaleCanChange =
			MinimumAdaptiveLateralScale < 1.0f - KINDA_SMALL_NUMBER ||
			MaximumAdaptiveLateralScale > 1.0f + KINDA_SMALL_NUMBER;

		const float ProfileAdaptiveMaximumExtraDistance =
			GetGoalkeeperProfileAdaptiveLateralMaximumExtraDistance(
				SoccerCharacter
			);

		if (
			bUseGoalkeeperAdaptiveLateralReach &&
			bAdaptiveScaleCanChange &&
			ProfileAdaptiveMaximumExtraDistance > 0.0f
			)
		{
			const float AbsCapsuleLateralOffset =
				FMath::Abs(CapsuleLateralOffset);

			const float InitialLateralError =
				FVector::DotProduct(
					PredictedBallLocation -
						PredictedContactLocation,
					RightDirection
				);

			const bool bCurveHasUsefulLateralMotion =
				AbsCapsuleLateralOffset >=
					GoalkeeperAdaptiveLateralMinimumCurveOffset;

			const bool bCorrectionIsUseful =
				FMath::Abs(InitialLateralError) >=
					GoalkeeperAdaptiveLateralMinimumCorrectionDistance;

			if (
				bCurveHasUsefulLateralMotion &&
				bCorrectionIsUseful
				)
			{
				const float ExtraAtMinimumScale =
					CapsuleLateralOffset *
					(MinimumAdaptiveLateralScale - 1.0f);

				const float ExtraAtMaximumScale =
					CapsuleLateralOffset *
					(MaximumAdaptiveLateralScale - 1.0f);

				const float MaximumAbsoluteExtraDistance =
					FMath::Max(
						0.0f,
						ProfileAdaptiveMaximumExtraDistance
					);

				const float MinimumAllowedExtraDistance =
					FMath::Max(
						FMath::Min(
							ExtraAtMinimumScale,
							ExtraAtMaximumScale
						),
						-MaximumAbsoluteExtraDistance
					);

				const float MaximumAllowedExtraDistance =
					FMath::Min(
						FMath::Max(
							ExtraAtMinimumScale,
							ExtraAtMaximumScale
						),
						MaximumAbsoluteExtraDistance
					);

				bool bAdaptivePredictionValid = true;

				// Dos correcciones son suficientes para absorber el pequeno cambio de
				// tiempo que produce trasladar el plano de contacto en un tiro diagonal.
				for (int32 Iteration = 0; Iteration < 2; ++Iteration)
				{
					const FVector CurrentShift =
						RightDirection *
						AdaptiveLateralExtraDistance;

					AdaptedLeftHandLocation =
						PredictedLeftHandLocation +
						CurrentShift;

					AdaptedRightHandLocation =
						PredictedRightHandLocation +
						CurrentShift;

					if (IsGoalkeeperCatchAction(GoalkeeperAction))
					{
						AdaptedContactLocation =
							FMath::ClosestPointOnSegment(
								EffectivePredictedBallLocation,
								AdaptedLeftHandLocation,
								AdaptedRightHandLocation
							);
					}
					else
					{
						AdaptedContactLocation =
							PredictedContactLocation +
							CurrentShift;
					}

					const float ResidualLateralError =
						FVector::DotProduct(
							EffectivePredictedBallLocation -
								AdaptedContactLocation,
							RightDirection
						);

					AdaptiveLateralExtraDistance =
						FMath::Clamp(
							AdaptiveLateralExtraDistance +
							ResidualLateralError,
							MinimumAllowedExtraDistance,
							MaximumAllowedExtraDistance
						);

					const FVector UpdatedShift =
						RightDirection *
						AdaptiveLateralExtraDistance;

					AdaptedLeftHandLocation =
						PredictedLeftHandLocation +
						UpdatedShift;

					AdaptedRightHandLocation =
						PredictedRightHandLocation +
						UpdatedShift;

					if (IsGoalkeeperCatchAction(GoalkeeperAction))
					{
						AdaptedContactLocation =
							FMath::ClosestPointOnSegment(
								EffectivePredictedBallLocation,
								AdaptedLeftHandLocation,
								AdaptedRightHandLocation
							);
					}
					else
					{
						AdaptedContactLocation =
							PredictedContactLocation +
							UpdatedShift;
					}

					float UpdatedBallTimeToPlane = 0.0f;
					FVector UpdatedPredictedBallLocation =
						FVector::ZeroVector;

					if (
						!TryPredictBallAtGoalkeeperCandidatePlane(
							SoccerBall,
							AdaptedContactLocation,
							UpdatedBallTimeToPlane,
							UpdatedPredictedBallLocation
						)
						)
					{
						bAdaptivePredictionValid = false;
						break;
					}

					EffectiveBallTimeToPlane =
						UpdatedBallTimeToPlane;

					EffectivePredictedBallLocation =
						UpdatedPredictedBallLocation;
				}

				if (
					bAdaptivePredictionValid &&
					FMath::Abs(AdaptiveLateralExtraDistance) >
						KINDA_SMALL_NUMBER
					)
				{
					AdaptiveLateralScale =
						FMath::Clamp(
							1.0f +
							AdaptiveLateralExtraDistance /
							CapsuleLateralOffset,
							MinimumAdaptiveLateralScale,
							MaximumAdaptiveLateralScale
						);

					// La ultima prediccion ya corresponde a las manos desplazadas. Para Catch
					// actualizamos una vez mas el punto dentro de la zona entre ambas manos.
					if (IsGoalkeeperCatchAction(GoalkeeperAction))
					{
						AdaptedContactLocation =
							FMath::ClosestPointOnSegment(
								EffectivePredictedBallLocation,
								AdaptedLeftHandLocation,
								AdaptedRightHandLocation
							);
					}
				}
				else if (!bAdaptivePredictionValid)
				{
					// Ante cualquier fallo de la prediccion adaptada conservamos intacto el
					// candidato original. La ayuda lateral nunca debe invalidar una atajada.
					AdaptedLeftHandLocation = PredictedLeftHandLocation;
					AdaptedRightHandLocation = PredictedRightHandLocation;
					AdaptedContactLocation = PredictedContactLocation;
					EffectiveBallTimeToPlane = BallTimeToPlane;
					EffectivePredictedBallLocation = PredictedBallLocation;
					AdaptiveLateralExtraDistance = 0.0f;
				}
			}
		}

		const float SpatialErrorAfterAdaptiveLateral =
			FVector::Dist(
				EffectivePredictedBallLocation,
				AdaptedContactLocation
			);

		// ------------------------------------------------------------
		// ETAPA 15B - CORRECCION PROCEDURAL 3D CASI PERFECTA
		// ------------------------------------------------------------
		// Adaptive Lateral conserva la forma authored de la curva. Esta segunda
		// capa corrige solamente el residuo que queda en el instante de contacto.
		// La correccion se congela al commit y el Character la mezcla alrededor
		// de SampleTime; no persigue la pelota durante el montage.
		FVector NearPerfectContactCorrectionLocal = FVector::ZeroVector;

		const bool bNearPerfectActionSupported =
			IsGoalkeeperCatchAction(GoalkeeperAction) ||
			IsGoalkeeperDeflectAction(GoalkeeperAction);

		if (
			bUseGoalkeeperNearPerfectSaveAssist &&
			bNearPerfectActionSupported &&
			SpatialErrorAfterAdaptiveLateral >=
				GoalkeeperNearPerfectMinimumCorrectionDistance
			)
		{
			const FVector BaseAdaptedLeftHandLocation = AdaptedLeftHandLocation;
			const FVector BaseAdaptedRightHandLocation = AdaptedRightHandLocation;
			const FVector BaseAdaptedContactLocation = AdaptedContactLocation;
			const float BaseEffectiveBallTimeToPlane = EffectiveBallTimeToPlane;
			const FVector BaseEffectivePredictedBallLocation =
				EffectivePredictedBallLocation;

			const float MaximumForwardCorrection =
				FMath::Max(0.0f, GoalkeeperNearPerfectMaximumForwardCorrection);

			const float MaximumLateralCorrection =
				FMath::Max(0.0f, GoalkeeperNearPerfectMaximumLateralCorrection);

			const float MaximumVerticalCorrection =
				FMath::Max(0.0f, GoalkeeperNearPerfectMaximumVerticalCorrection);

			bool bNearPerfectPredictionValid = true;

			// Repetimos unas pocas veces porque mover Forward/Lateral tambien mueve
			// el plano de intercepcion para remates diagonales.
			for (int32 Iteration = 0; Iteration < 3; ++Iteration)
			{
				const FVector CurrentCorrectionWorld =
					ForwardDirection * NearPerfectContactCorrectionLocal.X +
					RightDirection * NearPerfectContactCorrectionLocal.Y +
					FVector::UpVector * NearPerfectContactCorrectionLocal.Z;

				AdaptedLeftHandLocation =
					BaseAdaptedLeftHandLocation + CurrentCorrectionWorld;

				AdaptedRightHandLocation =
					BaseAdaptedRightHandLocation + CurrentCorrectionWorld;

				if (IsGoalkeeperCatchAction(GoalkeeperAction))
				{
					AdaptedContactLocation =
						FMath::ClosestPointOnSegment(
							EffectivePredictedBallLocation,
							AdaptedLeftHandLocation,
							AdaptedRightHandLocation
						);
				}
				else
				{
					AdaptedContactLocation =
						BaseAdaptedContactLocation + CurrentCorrectionWorld;
				}

				const FVector ResidualWorld =
					EffectivePredictedBallLocation - AdaptedContactLocation;

				NearPerfectContactCorrectionLocal.X =
					FMath::Clamp(
						NearPerfectContactCorrectionLocal.X +
							FVector::DotProduct(ResidualWorld, ForwardDirection),
						-MaximumForwardCorrection,
						MaximumForwardCorrection
					);

				NearPerfectContactCorrectionLocal.Y =
					FMath::Clamp(
						NearPerfectContactCorrectionLocal.Y +
							FVector::DotProduct(ResidualWorld, RightDirection),
						-MaximumLateralCorrection,
						MaximumLateralCorrection
					);

				NearPerfectContactCorrectionLocal.Z =
					FMath::Clamp(
						NearPerfectContactCorrectionLocal.Z + ResidualWorld.Z,
						-MaximumVerticalCorrection,
						MaximumVerticalCorrection
					);

				const FVector UpdatedCorrectionWorld =
					ForwardDirection * NearPerfectContactCorrectionLocal.X +
					RightDirection * NearPerfectContactCorrectionLocal.Y +
					FVector::UpVector * NearPerfectContactCorrectionLocal.Z;

				AdaptedLeftHandLocation =
					BaseAdaptedLeftHandLocation + UpdatedCorrectionWorld;

				AdaptedRightHandLocation =
					BaseAdaptedRightHandLocation + UpdatedCorrectionWorld;

				if (IsGoalkeeperCatchAction(GoalkeeperAction))
				{
					AdaptedContactLocation =
						FMath::ClosestPointOnSegment(
							EffectivePredictedBallLocation,
							AdaptedLeftHandLocation,
							AdaptedRightHandLocation
						);
				}
				else
				{
					AdaptedContactLocation =
						BaseAdaptedContactLocation + UpdatedCorrectionWorld;
				}

				float UpdatedBallTimeToPlane = 0.0f;
				FVector UpdatedPredictedBallLocation = FVector::ZeroVector;

				if (
					!TryPredictBallAtGoalkeeperCandidatePlane(
						SoccerBall,
						AdaptedContactLocation,
						UpdatedBallTimeToPlane,
						UpdatedPredictedBallLocation
					)
					)
				{
					bNearPerfectPredictionValid = false;
					break;
				}

				EffectiveBallTimeToPlane = UpdatedBallTimeToPlane;
				EffectivePredictedBallLocation = UpdatedPredictedBallLocation;
			}

			if (!bNearPerfectPredictionValid)
			{
				NearPerfectContactCorrectionLocal = FVector::ZeroVector;
				AdaptedLeftHandLocation = BaseAdaptedLeftHandLocation;
				AdaptedRightHandLocation = BaseAdaptedRightHandLocation;
				AdaptedContactLocation = BaseAdaptedContactLocation;
				EffectiveBallTimeToPlane = BaseEffectiveBallTimeToPlane;
				EffectivePredictedBallLocation =
					BaseEffectivePredictedBallLocation;
			}
			else if (IsGoalkeeperCatchAction(GoalkeeperAction))
			{
				AdaptedContactLocation =
					FMath::ClosestPointOnSegment(
						EffectivePredictedBallLocation,
						AdaptedLeftHandLocation,
						AdaptedRightHandLocation
					);
			}
		}

		const float SpatialError =
			FVector::Dist(
				EffectivePredictedBallLocation,
				AdaptedContactLocation
			);

		// Timing perfecto primero espera como antes. Solo si la pelota ya llega
		// antes del SampleTime intentamos recuperar unas centesimas acelerando el
		// montage, dentro de limites configurables.
		float MontagePlayRate = 1.0f;
		float EffectiveRealContactTime = SampleTime;
		float RequiredStartDelay =
			EffectiveBallTimeToPlane - EffectiveRealContactTime;

		if (
			bUseGoalkeeperNearPerfectSaveAssist &&
			RequiredStartDelay < 0.0f &&
			SampleTime > KINDA_SMALL_NUMBER &&
			EffectiveBallTimeToPlane > KINDA_SMALL_NUMBER
			)
		{
			const float LateTime = -RequiredStartDelay;
			const float MaximumLateCorrection =
				FMath::Max(0.0f, GoalkeeperNearPerfectMaximumLateTimingCorrection);

			if (LateTime <= MaximumLateCorrection)
			{
				const float MaximumPlayRate =
					FMath::Max(1.0f, GoalkeeperNearPerfectMaximumMontagePlayRate);

				const float DesiredPlayRate =
					SampleTime / EffectiveBallTimeToPlane;

				MontagePlayRate =
					FMath::Clamp(DesiredPlayRate, 1.0f, MaximumPlayRate);

				EffectiveRealContactTime =
					SampleTime / MontagePlayRate;

				RequiredStartDelay =
					EffectiveBallTimeToPlane - EffectiveRealContactTime;
			}
		}

		const bool bTemporallyFeasible =
			RequiredStartDelay >=
			-GoalkeeperSaveMontageStartTolerance;

		float FinalScore =
			SpatialError +
			FMath::Abs(AdaptiveLateralExtraDistance) *
			GoalkeeperAdaptiveLateralExtraDistancePenalty +
			NearPerfectContactCorrectionLocal.Size() *
			GoalkeeperNearPerfectCorrectionDistancePenalty;

		if (
			IsGoalkeeperDeflectAction(
				GoalkeeperAction
			)
			)
		{
			FinalScore +=
				GoalkeeperHandTrackDeflectPenalty;
		}

		const FVector ToPredictedBall =
			EffectivePredictedBallLocation -
			SoccerCharacter->
			GetActorLocation();

		const float PredictedSideOffset =
			FVector::DotProduct(
				ToPredictedBall,
				RightDirection
			);

		const bool bClearlyLeft =
			PredictedSideOffset <
			-GoalkeeperHandTrackSideDeadZone;

		const bool bClearlyRight =
			PredictedSideOffset >
			GoalkeeperHandTrackSideDeadZone;

		const bool bWrongSide =
			(
				bClearlyLeft &&
				IsGoalkeeperActionToRight(
					GoalkeeperAction
				)
				)
			||
			(
				bClearlyRight &&
				IsGoalkeeperActionToLeft(
					GoalkeeperAction
				)
				);

		if (bWrongSide)
		{
			FinalScore +=
				GoalkeeperHandTrackWrongSidePenalty;
		}

		if (!bTemporallyFeasible)
		{
			const float LateTime =
				FMath::Max(
					0.0f,
					-RequiredStartDelay
				);

			FinalScore +=
				LateTime *
				GoalkeeperSaveEmergencyLatePenaltyPerSecond;
		}

		FGoalkeeperSaveSelectionResult Candidate;

		Candidate.Action =
			GoalkeeperAction;

		Candidate.SelectedContactTime =
			SampleTime;

		Candidate.BallTimeToContactPlane =
			EffectiveBallTimeToPlane;

		Candidate.RequiredMontageStartDelay =
			RequiredStartDelay;

		/*
		 * El nombre RawHandDistance se mantiene por
		 * compatibilidad.
		 *
		 * En Catch representa distancia a la zona entre
		 * ambas manos.
		 */
		Candidate.RawHandDistance =
			SpatialError;

		Candidate.RawHandDistanceBeforeAdaptiveLateral =
			SpatialErrorBeforeAdaptive;

		Candidate.RawHandDistanceBeforeNearPerfectCorrection =
			SpatialErrorAfterAdaptiveLateral;

		Candidate.AdaptiveLateralScale =
			AdaptiveLateralScale;

		Candidate.AdaptiveLateralExtraDistance =
			AdaptiveLateralExtraDistance;

		Candidate.NearPerfectContactCorrectionLocal =
			NearPerfectContactCorrectionLocal;

		Candidate.MontagePlayRate = MontagePlayRate;

		Candidate.FinalScore =
			FinalScore;

		Candidate.PredictedBallLocation =
			EffectivePredictedBallLocation;

		Candidate.PredictedSelectedHandLocation =
			AdaptedContactLocation;

		Candidate.PredictedLeftHandLocation =
			AdaptedLeftHandLocation;

		Candidate.PredictedRightHandLocation =
			AdaptedRightHandLocation;

		/*
		 * Para Catch solamente se conserva como dato de
		 * compatibilidad y diagn�stico.
		 *
		 * Indica qu� extremo del segmento qued� m�s cerca
		 * del punto de captura.
		 */
		Candidate.bSelectedLeftHand =
			bLeftHand;

		Candidate.bTemporallyFeasible =
			bTemporallyFeasible;

		if (
			!OutBestVisualSelection.IsValid() ||
			Candidate.FinalScore <
			OutBestVisualSelection.FinalScore
			)
		{
			OutBestVisualSelection =
				Candidate;
		}

		if (
			bTemporallyFeasible &&
			(
				!OutBestFeasibleSelection.IsValid() ||
				Candidate.FinalScore <
				OutBestFeasibleSelection.FinalScore
				)
			)
		{
			OutBestFeasibleSelection =
				Candidate;
		}
	};

	auto ConsiderIndividualHandCandidate =
		[
			this,
			SoccerBall,
			&CommitCandidate
		](
			float SampleTime,
			const FVector& PredictedHandLocation,
			const FVector& PredictedLeftHandLocation,
			const FVector& PredictedRightHandLocation,
			bool bLeftHand,
			float CapsuleLateralOffset
			)
	{
		float BallTimeToPlane = 0.0f;

		FVector PredictedBallLocation =
			FVector::ZeroVector;

		if (
			!TryPredictBallAtGoalkeeperCandidatePlane(
				SoccerBall,
				PredictedHandLocation,
				BallTimeToPlane,
				PredictedBallLocation
			)
			)
		{
			return;
		}

		CommitCandidate(
			SampleTime,
			PredictedHandLocation,
			PredictedLeftHandLocation,
			PredictedRightHandLocation,
			bLeftHand,
			BallTimeToPlane,
			PredictedBallLocation,
			CapsuleLateralOffset
		);
	};

	auto ConsiderTwoHandCatchZoneCandidate =
		[
			this,
			SoccerBall,
			&CommitCandidate
		](
			float SampleTime,
			const FVector& PredictedLeftHandLocation,
			const FVector& PredictedRightHandLocation,
			float CapsuleLateralOffset
			)
	{
		/*
		 * Primera aproximaci�n:
		 * usamos el centro de ambas manos para obtener
		 * d�nde estar� la pelota en ese plano.
		 */
		FVector CatchPoint =
			FMath::Lerp(
				PredictedLeftHandLocation,
				PredictedRightHandLocation,
				0.5f
			);

		float PreliminaryBallTime = 0.0f;

		FVector PreliminaryBallLocation =
			FVector::ZeroVector;

		if (
			!TryPredictBallAtGoalkeeperCandidatePlane(
				SoccerBall,
				CatchPoint,
				PreliminaryBallTime,
				PreliminaryBallLocation
			)
			)
		{
			return;
		}

		/*
		 * Resolvemos el punto efectivo de captura como el
		 * punto del segmento entre manos m�s cercano a la
		 * pelota preliminar.
		 */
		CatchPoint =
			FMath::ClosestPointOnSegment(
				PreliminaryBallLocation,
				PredictedLeftHandLocation,
				PredictedRightHandLocation
			);

		/*
		 * Segunda predicci�n:
		 * ahora el plano pasa por el punto efectivo de la
		 * zona de captura, no necesariamente por su centro.
		 */
		float BallTimeToPlane = 0.0f;

		FVector PredictedBallLocation =
			FVector::ZeroVector;

		if (
			!TryPredictBallAtGoalkeeperCandidatePlane(
				SoccerBall,
				CatchPoint,
				BallTimeToPlane,
				PredictedBallLocation
			)
			)
		{
			return;
		}

		const bool bClosestToLeftHand =
			FVector::DistSquared(
				CatchPoint,
				PredictedLeftHandLocation
			) <=
			FVector::DistSquared(
				CatchPoint,
				PredictedRightHandLocation
			);

		CommitCandidate(
			SampleTime,
			CatchPoint,
			PredictedLeftHandLocation,
			PredictedRightHandLocation,
			bClosestToLeftHand,
			BallTimeToPlane,
			PredictedBallLocation,
			CapsuleLateralOffset
		);
	};

	for (
		int32 SampleIndex = 0;
		SampleIndex < SampleCount;
		++SampleIndex
		)
	{
		const float SampleAlpha =
			SampleCount > 1
			? static_cast<float>(SampleIndex) /
			static_cast<float>(
				SampleCount - 1
				)
			: 0.5f;

		const float SampleTime =
			FMath::Lerp(
				ContactWindowStartTime,
				SafeWindowEnd,
				SampleAlpha
			);

		FVector SampleLeftTrackPosition;
		FVector SampleRightTrackPosition;

		if (
			!EvaluateGoalkeeperSaveHandTrack(
				GoalkeeperAction,
				SampleTime,
				SampleLeftTrackPosition,
				SampleRightTrackPosition
			)
			)
		{
			continue;
		}

		const FVector ScaledLeftTrackPosition =
			SampleLeftTrackPosition *
			GoalkeeperHandTrackScale;

		const FVector ScaledRightTrackPosition =
			SampleRightTrackPosition *
			GoalkeeperHandTrackScale;

		const FVector LeftTrackWorldOffset =
			ForwardDirection *
			ScaledLeftTrackPosition.X
			+
			RightDirection *
			ScaledLeftTrackPosition.Y
			+
			FVector::UpVector *
			ScaledLeftTrackPosition.Z;

		const FVector RightTrackWorldOffset =
			ForwardDirection *
			ScaledRightTrackPosition.X
			+
			RightDirection *
			ScaledRightTrackPosition.Y
			+
			FVector::UpVector *
			ScaledRightTrackPosition.Z;

		FVector2D CapsuleLocalOffset =
			FVector2D::ZeroVector;

		if (
			!SoccerCharacter->
			TryGetGoalkeeperSaveCurveMotionLocalOffset(
				GoalkeeperAction,
				SampleTime,
				CapsuleLocalOffset
			)
			)
		{
			continue;
		}

		const FVector CapsuleWorldDelta =
			ForwardDirection *
			CapsuleLocalOffset.X
			+
			RightDirection *
			CapsuleLocalOffset.Y;

		const FVector PredictedLeftHandLocation =
			BaseMeshOriginLocation +
			LeftTrackWorldOffset +
			CapsuleWorldDelta;

		const FVector PredictedRightHandLocation =
			BaseMeshOriginLocation +
			RightTrackWorldOffset +
			CapsuleWorldDelta;

		if (bUseTwoHandCatchZone)
		{
			ConsiderTwoHandCatchZoneCandidate(
				SampleTime,
				PredictedLeftHandLocation,
				PredictedRightHandLocation,
				CapsuleLocalOffset.Y
			);
		}
		else
		{
			/*
			 * Deflect y Miss conservan contacto por mano
			 * individual.
			 */
			ConsiderIndividualHandCandidate(
				SampleTime,
				PredictedLeftHandLocation,
				PredictedLeftHandLocation,
				PredictedRightHandLocation,
				true,
				CapsuleLocalOffset.Y
			);

			ConsiderIndividualHandCandidate(
				SampleTime,
				PredictedRightHandLocation,
				PredictedLeftHandLocation,
				PredictedRightHandLocation,
				false,
				CapsuleLocalOffset.Y
			);
		}
	}

	return
		OutBestFeasibleSelection.IsValid() ||
		OutBestVisualSelection.IsValid();
}

FGoalkeeperSaveSelectionResult
ASoccerAIController::
SelectGoalkeeperActionForIncomingBall(
	const ASoccerAICharacter* SoccerCharacter,
	const ASoccerBall* SoccerBall,
	bool bCanUseHands
) const
{
	FGoalkeeperSaveSelectionResult Result;

	if (
		!IsValid(SoccerCharacter) ||
		!IsValid(SoccerBall)
		)
	{
		return Result;
	}

	static const TArray<
		ESoccerGoalkeeperAction
	> CandidateActions =
	{
		ESoccerGoalkeeperAction::
			BodyBlockCatchToLeft,

		ESoccerGoalkeeperAction::
			BodyBlockCatchToRight,

		ESoccerGoalkeeperAction::
			BodyBlockDeflectToLeft,

		ESoccerGoalkeeperAction::
			BodyBlockDeflectToRight,

		ESoccerGoalkeeperAction::
			CatchAbdomen,

		ESoccerGoalkeeperAction::
			CatchFaceToLeft,

		ESoccerGoalkeeperAction::
			CatchFaceToRight,

		ESoccerGoalkeeperAction::
			CatchOverHeadJumpToLeft,

		ESoccerGoalkeeperAction::
			CatchOverHeadJumpToRight,

		ESoccerGoalkeeperAction::
			CatchOverHeadRunJump,

		ESoccerGoalkeeperAction::
			DivingSaveFloorToLeft,

		ESoccerGoalkeeperAction::
			DivingSaveFloorToRight,

		ESoccerGoalkeeperAction::
			DivingSaveOneMeterToLeft,

		ESoccerGoalkeeperAction::
			DivingSaveOneMeterToRight,

		ESoccerGoalkeeperAction::
			ScoopToLeft,

		ESoccerGoalkeeperAction::
			ScoopToRight
	};

	FGoalkeeperSaveSelectionResult
		BestReachableSelection;

	FGoalkeeperSaveSelectionResult
		BestVisualEmergencySelection;

	bool bScoredAtLeastOneAction =
		false;

	for (
		const ESoccerGoalkeeperAction CandidateAction :
	CandidateActions
		)
	{
		/*
		 * Fuera del �rea solamente pueden competir
		 * acciones de rechazo.
		 */
		if (
			!bCanUseHands &&
			!IsGoalkeeperDeflectAction(
				CandidateAction
			)
			)
		{
			continue;
		}

		FGoalkeeperSaveSelectionResult
			BestActionFeasible;

		FGoalkeeperSaveSelectionResult
			BestActionVisual;

		if (
			!TryScoreGoalkeeperSaveActionAgainstBall(
				SoccerCharacter,
				SoccerBall,
				CandidateAction,
				BestActionFeasible,
				BestActionVisual
			)
			)
		{
			continue;
		}

		bScoredAtLeastOneAction =
			true;

		if (
			BestActionFeasible.IsValid() &&
			BestActionFeasible.RawHandDistance <=
			GoalkeeperHandTrackMaxSelectionDistance &&
			(
				!BestReachableSelection.IsValid() ||
				BestActionFeasible.FinalScore <
				BestReachableSelection.FinalScore
				)
			)
		{
			BestReachableSelection =
				BestActionFeasible;
		}

		if (
			BestActionVisual.IsValid() &&
			(
				!BestVisualEmergencySelection.IsValid() ||
				BestActionVisual.FinalScore <
				BestVisualEmergencySelection.FinalScore
				)
			)
		{
			BestVisualEmergencySelection =
				BestActionVisual;
		}
	}

	/*
	 * Una atajada normal temporal y espacialmente posible
	 * siempre tiene prioridad.
	 */
	if (BestReachableSelection.IsValid())
	{
		return BestReachableSelection;
	}

	FVector ReferencePredictedLocation =
		FVector::ZeroVector;

	float TimeToReferencePlane =
		0.0f;

	const bool bHasReferencePrediction =
		TryPredictGoalkeeperReferencePlaneCrossing(
			SoccerCharacter,
			SoccerBall,
			ReferencePredictedLocation,
			TimeToReferencePlane
		);

	/*
	 * Miss solamente representa una pelota alta y casi
	 * central que pasa por arriba del arquero.
	 */
	if (
		bHasReferencePrediction &&
		IsGoalkeeperOverheadMissSituation(
			SoccerCharacter,
			ReferencePredictedLocation
		)
		)
	{
		FGoalkeeperSaveSelectionResult
			MissFeasible;

		FGoalkeeperSaveSelectionResult
			MissVisual;

		if (
			TryScoreGoalkeeperSaveActionAgainstBall(
				SoccerCharacter,
				SoccerBall,
				ESoccerGoalkeeperAction::Miss,
				MissFeasible,
				MissVisual
			)
			)
		{
			Result =
				MissFeasible.IsValid()
				? MissFeasible
				: MissVisual;

			Result.bEmergencySelection =
				!Result.bTemporallyFeasible;

			return Result;
		}
	}

	/*
	 * Ninguna animaci�n llega correctamente.
	 *
	 * Elegimos la reacci�n visualmente m�s coherente,
	 * pero no convertimos el fallo gen�rico en Miss.
	 */
	if (BestVisualEmergencySelection.IsValid())
	{
		BestVisualEmergencySelection.
			bEmergencySelection = true;

		return BestVisualEmergencySelection;
	}

	/*
	 * El fallback antiguo solo se conserva como protecci�n
	 * ante tablas ausentes o ilegibles.
	 */
	if (
		!bScoredAtLeastOneAction &&
		bFallbackToLegacyGoalkeeperSaveSelector &&
		bHasReferencePrediction
		)
	{
		Result.Action =
			ChooseGoalkeeperActionLegacy(
				SoccerCharacter,
				ReferencePredictedLocation,
				bCanUseHands
			);

		float WindowStart = 0.0f;
		float WindowEnd = 0.0f;

		if (
			TryGetGoalkeeperContactWindowRangeForAction(
				SoccerCharacter,
				Result.Action,
				WindowStart,
				WindowEnd
			)
			)
		{
			Result.SelectedContactTime =
				0.5f *
				(
					WindowStart +
					WindowEnd
					);
		}
		else
		{
			Result.SelectedContactTime =
				GoalkeeperFallbackContactWindowStartTime;
		}

		Result.BallTimeToContactPlane =
			TimeToReferencePlane;

		Result.RequiredMontageStartDelay =
			TimeToReferencePlane -
			Result.SelectedContactTime;

		Result.PredictedBallLocation =
			ReferencePredictedLocation;

		Result.bEmergencySelection =
			Result.RequiredMontageStartDelay < 0.0f;
	}

	return Result;
}

bool ASoccerAIController::
TryGetGoalkeeperContactWindowRangeForAction(
	const ASoccerAICharacter* SoccerCharacter,
	ESoccerGoalkeeperAction GoalkeeperAction,
	float& OutContactWindowStartTime,
	float& OutContactWindowEndTime
) const
{
	OutContactWindowStartTime = 0.0f;
	OutContactWindowEndTime = 0.0f;

	if (!IsValid(SoccerCharacter))
	{
		return false;
	}

	UAnimMontage* GoalkeeperMontage =
		SoccerCharacter->
		GetGoalkeeperMontageForAction(
			GoalkeeperAction
		);

	if (!IsValid(GoalkeeperMontage))
	{
		return false;
	}

	for (
		const FAnimNotifyEvent& NotifyEvent :
		GoalkeeperMontage->Notifies
		)
	{
		if (
			NotifyEvent.NotifyStateClass != nullptr &&
			NotifyEvent.NotifyStateClass->
			IsA<
			UAnimNotifyState_GKContactWindow
			>()
			)
		{
			OutContactWindowStartTime =
				NotifyEvent.GetTriggerTime();

			OutContactWindowEndTime =
				NotifyEvent.GetEndTriggerTime();

			return
				OutContactWindowEndTime >=
				OutContactWindowStartTime;
		}
	}

	return false;
}

UCurveTable*
ASoccerAIController::
GetGoalkeeperSaveHandTrackCurveTable(
	ESoccerGoalkeeperAction GoalkeeperAction
) const
{
	UCurveTable* const* FoundCurveTable =
		GoalkeeperSaveHandTrackCurveTables.Find(
			GoalkeeperAction
		);

	return
		FoundCurveTable != nullptr
		? *FoundCurveTable
		: nullptr;
}

bool ASoccerAIController::
EvaluateGoalkeeperSaveHandTrack(
	ESoccerGoalkeeperAction GoalkeeperAction,
	float MontageTime,
	FVector& OutLeftHandLocalPosition,
	FVector& OutRightHandLocalPosition
) const
{
	OutLeftHandLocalPosition =
		FVector::ZeroVector;

	OutRightHandLocalPosition =
		FVector::ZeroVector;

	UCurveTable* CurveTable =
		GetGoalkeeperSaveHandTrackCurveTable(
			GoalkeeperAction
		);

	if (CurveTable == nullptr)
	{
		return false;
	}

	const FString ContextString =
		TEXT("Goalkeeper Save Hand Track");

	auto FindTrackCurve =
		[
			CurveTable,
			&ContextString
		](
			const TCHAR* RowName
			) -> const FRealCurve*
	{
		return CurveTable->FindCurve(
			FName(RowName),
			ContextString,
			false
		);
	};

	const FRealCurve* LeftForwardCurve =
		FindTrackCurve(TEXT("LeftForward"));

	const FRealCurve* LeftLateralCurve =
		FindTrackCurve(TEXT("LeftLateral"));

	const FRealCurve* LeftUpCurve =
		FindTrackCurve(TEXT("LeftUp"));

	const FRealCurve* RightForwardCurve =
		FindTrackCurve(TEXT("RightForward"));

	const FRealCurve* RightLateralCurve =
		FindTrackCurve(TEXT("RightLateral"));

	const FRealCurve* RightUpCurve =
		FindTrackCurve(TEXT("RightUp"));

	if (
		LeftForwardCurve == nullptr ||
		LeftLateralCurve == nullptr ||
		LeftUpCurve == nullptr ||
		RightForwardCurve == nullptr ||
		RightLateralCurve == nullptr ||
		RightUpCurve == nullptr
		)
	{
		return false;
	}

	const float SafeMontageTime =
		FMath::Max(
			0.0f,
			MontageTime
		);

	/*
	 * X local = Forward.
	 * Y local = Lateral, derecha positiva.
	 * Z local = Up.
	 */
	OutLeftHandLocalPosition =
		FVector(
			LeftForwardCurve->Eval(
				SafeMontageTime
			),
			LeftLateralCurve->Eval(
				SafeMontageTime
			),
			LeftUpCurve->Eval(
				SafeMontageTime
			)
		);

	OutRightHandLocalPosition =
		FVector(
			RightForwardCurve->Eval(
				SafeMontageTime
			),
			RightLateralCurve->Eval(
				SafeMontageTime
			),
			RightUpCurve->Eval(
				SafeMontageTime
			)
		);

	return true;
}

bool ASoccerAIController::
IsGoalkeeperActionToLeft(
	ESoccerGoalkeeperAction GoalkeeperAction
) const
{
	switch (GoalkeeperAction)
	{
		case ESoccerGoalkeeperAction::
		BodyBlockCatchToLeft:

			case ESoccerGoalkeeperAction::
			BodyBlockDeflectToLeft:

				case ESoccerGoalkeeperAction::
				CatchFaceToLeft:

					case ESoccerGoalkeeperAction::
					CatchOverHeadJumpToLeft:

						case ESoccerGoalkeeperAction::
						DivingSaveFloorToLeft:

							case ESoccerGoalkeeperAction::
							DivingSaveOneMeterToLeft:

								case ESoccerGoalkeeperAction::
								ScoopToLeft:
									return true;

								default:
									return false;
	}
}

bool ASoccerAIController::
IsGoalkeeperActionToRight(
	ESoccerGoalkeeperAction GoalkeeperAction
) const
{
	switch (GoalkeeperAction)
	{
		case ESoccerGoalkeeperAction::
		BodyBlockCatchToRight:

			case ESoccerGoalkeeperAction::
			BodyBlockDeflectToRight:

				case ESoccerGoalkeeperAction::
				CatchFaceToRight:

					case ESoccerGoalkeeperAction::
					CatchOverHeadJumpToRight:

						case ESoccerGoalkeeperAction::
						DivingSaveFloorToRight:

							case ESoccerGoalkeeperAction::
							DivingSaveOneMeterToRight:

								case ESoccerGoalkeeperAction::
								ScoopToRight:
									return true;

								default:
									return false;
	}
}

void ASoccerAIController::PreparePendingGoalkeeperSaveAction(
	ASoccerAICharacter* SoccerCharacter,
	ASoccerBall* SoccerBall,
	ESoccerGoalkeeperAction GoalkeeperAction,
	const FVector& PredictedInterventionLocation,
	float TimeToIntervention,
	float ContactWindowStartTime,
	bool bEmergencySelection
)
{
	ClearGoalkeeperLooseBallClaimMode();

	ClearPendingGoalkeeperSaveAction();
	ClearPendingGoalkeeperSaveImpact();

	if (!IsValid(SoccerCharacter) || !IsValid(SoccerBall))
	{
		return;
	}

	PendingGoalkeeperSaveActionCharacter = SoccerCharacter;
	PendingGoalkeeperSaveActionBall = SoccerBall;
	PendingGoalkeeperScheduledSaveAction = GoalkeeperAction;

	PendingGoalkeeperSaveActionPredictedInterventionLocation =
		PredictedInterventionLocation;

	PendingGoalkeeperSaveActionTimeToIntervention =
		TimeToIntervention;

	PendingGoalkeeperSaveActionContactWindowStartTime =
		ContactWindowStartTime;

	bPendingGoalkeeperSaveActionEmergencySelection =
		bEmergencySelection;

}

bool ASoccerAIController::
UpdatePendingGoalkeeperSaveAction(
	ASoccerAICharacter* SoccerCharacter,
	ASoccerBall* SoccerBall,
	float DeltaTime
)
{
	(void)DeltaTime;

	if (
		!HasPendingGoalkeeperSaveActionFor(
			SoccerCharacter
		)
		)
	{
		return false;
	}

	if (
		!IsValid(SoccerCharacter) ||
		!IsValid(SoccerBall)
		)
	{
		ClearPendingGoalkeeperSaveAction();
		return false;
	}

	if (
		SoccerCharacter->
		IsGoalkeeperActionActive()
		)
	{
		ClearPendingGoalkeeperSaveAction();
		return false;
	}

	/*
	 * Una vez iniciado el seguimiento usamos un horizonte
	 * m�s amplio que el tiempo de decisi�n inicial.
	 *
	 * De esta manera, si la pelota desacelera, el estado
	 * pendiente no desaparece.
	 */
	FVector ReferencePredictedLocation =
		FVector::ZeroVector;

	float TimeToReferencePlane =
		0.0f;

	const bool bReferencePredictionStillValid =
		TryPredictGoalkeeperReferencePlaneCrossing(
			SoccerCharacter,
			SoccerBall,
			ReferencePredictedLocation,
			TimeToReferencePlane,
			GoalkeeperLivePendingSaveMaxPredictionTime
		);

	if (!bReferencePredictionStillValid)
	{
		ClearPendingGoalkeeperSaveAction();
		return false;
	}

	const bool bCanUseHands =
		IsGoalkeeperHandlingAllowedAtLocation(
			SoccerCharacter,
			ReferencePredictedLocation
		);

	/*
	 * Mientras el montage todav�a no comenz�, volvemos
	 * a evaluar todas las acciones y todas sus muestras.
	 *
	 * Por eso pueden cambiar:
	 *
	 * - la acci�n;
	 * - la mano seleccionada;
	 * - el instante dentro de la ventana;
	 * - el punto de contacto;
	 * - el momento programado de inicio.
	 */
	const FGoalkeeperSaveSelectionResult
		UpdatedSelection =
		SelectGoalkeeperActionForIncomingBall(
			SoccerCharacter,
			SoccerBall,
			bCanUseHands
		);

	if (!UpdatedSelection.IsValid())
	{
		ClearPendingGoalkeeperSaveAction();
		return false;
	}

	if (
		UpdatedSelection.BallTimeToContactPlane ==
		BIG_NUMBER ||
		UpdatedSelection.RequiredMontageStartDelay ==
		BIG_NUMBER
		)
	{
		ClearPendingGoalkeeperSaveAction();
		return false;
	}

	const ESoccerGoalkeeperAction
		UpdatedAction =
		UpdatedSelection.Action;

	const float UpdatedBallTimeToContactPlane =
		FMath::Max(
			0.0f,
			UpdatedSelection.
			BallTimeToContactPlane
		);

	const float UpdatedSelectedContactTime =
		FMath::Max(
			0.0f,
			UpdatedSelection.
			SelectedContactTime
		);

	const float UpdatedRequiredStartDelay =
		UpdatedSelection.RequiredMontageStartDelay;

	// Reflexes does not alter the authored contact time. A low-reflex goalkeeper
	// simply commits a little later than the ideal instant; a 100-rated keeper
	// adds no extra delay. Characters without a profile keep legacy timing.
	const float GoalkeeperProfileReactionDelay =
		GetGoalkeeperProfileAdditionalReactionDelay(SoccerCharacter);

	const float ProfileAdjustedRequiredStartDelay =
		UpdatedRequiredStartDelay + GoalkeeperProfileReactionDelay;

	/*
	 * Actualizamos el estado pendiente con los valores
	 * del candidato concreto.
	 *
	 * Las variables conservan temporalmente sus nombres
	 * antiguos, pero ahora contienen:
	 *
	 * TimeToIntervention
	 *     = tiempo al plano de la mano.
	 *
	 * ContactWindowStartTime
	 *     = tiempo de contacto seleccionado.
	 */
	PendingGoalkeeperScheduledSaveAction =
		UpdatedAction;

	PendingGoalkeeperSaveActionPredictedInterventionLocation =
		UpdatedSelection.PredictedBallLocation;

	PendingGoalkeeperSaveActionTimeToIntervention =
		UpdatedBallTimeToContactPlane;

	PendingGoalkeeperSaveActionContactWindowStartTime =
		UpdatedSelectedContactTime;

	bPendingGoalkeeperSaveActionEmergencySelection =
		UpdatedSelection.bEmergencySelection;

	RecordGoalkeeperSaveTimingDebug(
		SoccerCharacter,
		SoccerBall,
		UpdatedAction,
		UpdatedSelection.PredictedBallLocation,
		UpdatedBallTimeToContactPlane,
		UpdatedSelectedContactTime,
		UpdatedRequiredStartDelay
	);

	if (bDebugGoalkeeperSaveTiming)
	{
		SetGoalkeeperPersistentLineIfEnabled(
			bDebugGoalkeeperSaveTiming ||
			bDebugGoalkeeperSaveCoordination ||
			bDebugGoalkeeperAnimatedContact ||
			bDebugGoalkeeperHandTrackSelector,

			this,
			ESoccerDebugCategory::GoalkeeperSave,
			400,
			FString::Printf(
				TEXT(
					"Decision: plano GK actual %.3f s | "
					"horizonte %.3f s"
				),
				TimeToReferencePlane,
				GoalkeeperSaveDecisionTimeHorizon
			),
			FColor::Cyan
		);

		SetGoalkeeperPersistentLineIfEnabled(
			bDebugGoalkeeperSaveTiming ||
			bDebugGoalkeeperSaveCoordination ||
			bDebugGoalkeeperAnimatedContact ||
			bDebugGoalkeeperHandTrackSelector,

			this,
			ESoccerDebugCategory::GoalkeeperSave,
			410,
			FString::Printf(
				TEXT(
					"Plano contacto: pelota %.3f s | "
					"contacto %.3f s | "
					"inicio %+0.3f s"
				),
				UpdatedBallTimeToContactPlane,
				UpdatedSelectedContactTime,
				UpdatedRequiredStartDelay
			),
			UpdatedRequiredStartDelay >
			GoalkeeperSaveMontageStartTolerance
			? FColor::Cyan
			: FColor::Orange
		);

		SetGoalkeeperPersistentLineIfEnabled(
			bDebugGoalkeeperSaveTiming ||
			bDebugGoalkeeperSaveCoordination ||
			bDebugGoalkeeperAnimatedContact ||
			bDebugGoalkeeperHandTrackSelector,

			this,
			ESoccerDebugCategory::GoalkeeperSave,
			420,
			FString::Printf(
				TEXT(
					"Seleccion: error %.1f -> %.1f -> %.1f cm | "
					"lat x%.3f (%+.1f) | corr F/L/U %+.1f/%+.1f/%+.1f | "
					"rate x%.3f | mano %s | viable %s | emergencia %s"
				),
				UpdatedSelection.RawHandDistanceBeforeAdaptiveLateral,
				UpdatedSelection.RawHandDistanceBeforeNearPerfectCorrection,
				UpdatedSelection.RawHandDistance,
				UpdatedSelection.AdaptiveLateralScale,
				UpdatedSelection.AdaptiveLateralExtraDistance,
				UpdatedSelection.NearPerfectContactCorrectionLocal.X,
				UpdatedSelection.NearPerfectContactCorrectionLocal.Y,
				UpdatedSelection.NearPerfectContactCorrectionLocal.Z,
				UpdatedSelection.MontagePlayRate,
				IsGoalkeeperCatchAction(
					UpdatedSelection.Action
				)
				? TEXT("ZONA MANOS")
				: (
					UpdatedSelection.bSelectedLeftHand
					? TEXT("IZQUIERDA")
					: TEXT("DERECHA")
					),
				UpdatedSelection.bTemporallyFeasible
				? TEXT("SI")
				: TEXT("NO"),
				UpdatedSelection.bEmergencySelection
				? TEXT("SI")
				: TEXT("NO")
			),
			UpdatedSelection.bEmergencySelection
			? FColor::Orange
			: FColor::Green
		);

		SetGoalkeeperPersistentLineIfEnabled(
			bDebugGoalkeeperSaveTiming ||
			bDebugGoalkeeperSaveCoordination ||
			bDebugGoalkeeperAnimatedContact ||
			bDebugGoalkeeperHandTrackSelector,

			this,
			ESoccerDebugCategory::GoalkeeperSave,
			430,
			FString::Printf(
				TEXT(
					"Pelota prevista: "
					"X %.0f | Y %.0f | Z %.0f"
				),
				UpdatedSelection.
				PredictedBallLocation.X,
				UpdatedSelection.
				PredictedBallLocation.Y,
				UpdatedSelection.
				PredictedBallLocation.Z
			),
			FColor::White
		);

		SetGoalkeeperPersistentLineIfEnabled(
			bDebugGoalkeeperSaveTiming ||
			bDebugGoalkeeperSaveCoordination ||
			bDebugGoalkeeperAnimatedContact ||
			bDebugGoalkeeperHandTrackSelector,

			this,
			ESoccerDebugCategory::GoalkeeperSave,
			440,
			FString::Printf(
				TEXT(
					"Mano prevista: "
					"X %.0f | Y %.0f | Z %.0f"
				),
				UpdatedSelection.
				PredictedSelectedHandLocation.X,
				UpdatedSelection.
				PredictedSelectedHandLocation.Y,
				UpdatedSelection.
				PredictedSelectedHandLocation.Z
			),
			FColor::White
		);
	}

	/*
	 * Todav�a falta tiempo para comenzar.
	 *
	 * La selecci�n seguir� recalcul�ndose en cada Tick.
	 */
	if (
		ProfileAdjustedRequiredStartDelay >
		GoalkeeperSaveMontageStartTolerance
		)
	{
		SetGoalkeeperBehaviorMode(
			ESoccerGoalkeeperBehaviorMode::ShotPending
		);

		return true;
	}

	/*
	 * Lleg� el instante programado.
	 *
	 * Tambi�n entran aqu� las selecciones de emergencia,
	 * cuyo delay puede ser negativo porque la animaci�n
	 * ya no alcanza a llegar a tiempo.
	 */
	const ESoccerGoalkeeperAction ActionToStart =
		UpdatedAction;

	const FVector PredictedImpactLocation =
		UpdatedSelection.PredictedBallLocation;

	const bool bEmergencySelection =
		UpdatedSelection.bEmergencySelection;

	/*
	 * Congelamos exactamente la selecci�n que est�
	 * provocando el inicio del montage.
	 */
	BeginGoalkeeperSaveCoordinationDebug(
		SoccerCharacter,
		SoccerBall,
		UpdatedSelection
	);

	ClearPendingGoalkeeperSaveAction();

	// Congelamos el factor junto con la seleccion. Desde este instante la
	// atajada reproduce una curva fija; no persigue lateralmente la pelota.
	SoccerCharacter->SetGoalkeeperSaveAdaptiveLateralScale(
		UpdatedSelection.AdaptiveLateralScale,
		GetGoalkeeperProfileAdaptiveLateralMaximumExtraDistance(
			SoccerCharacter
		)
	);

	SoccerCharacter->SetGoalkeeperSaveNearPerfectContactCorrection(
		UpdatedSelection.NearPerfectContactCorrectionLocal,
		UpdatedSelection.SelectedContactTime,
		GoalkeeperNearPerfectCorrectionBlendInTime,
		GoalkeeperNearPerfectCorrectionReleaseTime
	);

	const bool bActionStarted =
		SoccerCharacter->StartGoalkeeperAction(
			ActionToStart,
			UpdatedSelection.MontagePlayRate
		);

	if (!bActionStarted)
	{
		SoccerCharacter->SetGoalkeeperSaveAdaptiveLateralScale(1.0f, 0.0f);
		SoccerCharacter->ClearGoalkeeperSaveNearPerfectContactCorrection();
		ResetGoalkeeperSaveCoordinationDebug();
		return false;
	}

	PreparePendingGoalkeeperSaveImpact(
		SoccerCharacter,
		SoccerBall,
		ActionToStart
	);

	SetGoalkeeperBehaviorMode(
		ESoccerGoalkeeperBehaviorMode::Saving
	);

	if (bDebugGoalkeeperSaveTiming)
	{
		SetGoalkeeperPersistentLineIfEnabled(
			bDebugGoalkeeperSaveTiming ||
			bDebugGoalkeeperSaveCoordination ||
			bDebugGoalkeeperAnimatedContact ||
			bDebugGoalkeeperHandTrackSelector,

			this,
			ESoccerDebugCategory::GoalkeeperSave,
			450,
			FString::Printf(
				TEXT(
					"COMMIT: action %d | delay %+0.3f | rate x%.3f | "
					"lat x%.3f (%+.1f cm) | corr %+.1f/%+.1f/%+.1f | emergencia %s"
				),
				static_cast<int32>(
					ActionToStart
					),
				UpdatedRequiredStartDelay,
				UpdatedSelection.MontagePlayRate,
				UpdatedSelection.AdaptiveLateralScale,
				UpdatedSelection.AdaptiveLateralExtraDistance,
				UpdatedSelection.NearPerfectContactCorrectionLocal.X,
				UpdatedSelection.NearPerfectContactCorrectionLocal.Y,
				UpdatedSelection.NearPerfectContactCorrectionLocal.Z,
				bEmergencySelection
				? TEXT("SI")
				: TEXT("NO")
			),
			bEmergencySelection
			? FColor::Orange
			: FColor::Green
		);
	}

	return true;
}

void ASoccerAIController::ClearPendingGoalkeeperSaveAction()
{
	PendingGoalkeeperSaveActionCharacter = nullptr;
	PendingGoalkeeperSaveActionBall = nullptr;

	PendingGoalkeeperScheduledSaveAction =
		ESoccerGoalkeeperAction::None;

	PendingGoalkeeperSaveActionPredictedInterventionLocation =
		FVector::ZeroVector;

	PendingGoalkeeperSaveActionTimeToIntervention = 0.0f;
	PendingGoalkeeperSaveActionContactWindowStartTime = 0.0f;
	bPendingGoalkeeperSaveActionEmergencySelection = false;

}

bool ASoccerAIController::HasPendingGoalkeeperSaveActionFor(
	const ASoccerAICharacter* SoccerCharacter
) const
{
	return
		IsValid(SoccerCharacter) &&
		PendingGoalkeeperSaveActionCharacter == SoccerCharacter &&
		IsValid(PendingGoalkeeperSaveActionBall) &&
		PendingGoalkeeperScheduledSaveAction != ESoccerGoalkeeperAction::None;
}

bool ASoccerAIController::TryExecuteGoalkeeperEmergencyBodyContact(
	ASoccerAICharacter* SoccerCharacter
)
{
	if (!bEnableGoalkeeperEmergencyBodyContact)
	{
		return false;
	}

	if (!HasPendingGoalkeeperSaveImpactFor(SoccerCharacter))
	{
		return false;
	}

	ASoccerBall* SoccerBall =
		PendingGoalkeeperSaveBall;

	if (!IsValid(SoccerBall))
	{
		ClearPendingGoalkeeperSaveImpact();
		return false;
	}

	FGoalkeeperAnimatedContactResult ContactResult;

	const bool bAnyContact =
		DetectGoalkeeperAnimatedContact(
			SoccerCharacter,
			SoccerBall,
			ContactResult
		);

	/*
	 * Fuera de GKContactWindow ignoramos las manos.
	 *
	 * El agarre y el rechazo intencional solo pueden suceder
	 * durante la ventana configurada en el montage.
	 */
	if (
		!bAnyContact ||
		!ContactResult.bBodyContact
		)
	{
		return false;
	}

	ApplyGoalkeeperBodyReboundEffect(
		SoccerCharacter,
		SoccerBall
	);

	const FName ContactBone =
		ContactResult.ClosestBodyBone;

	ClearPendingGoalkeeperSaveImpact();

	SetGoalkeeperPersistentLineIfEnabled(
		bDebugGoalkeeperSaveTiming ||
		bDebugGoalkeeperSaveCoordination ||
		bDebugGoalkeeperAnimatedContact ||
		bDebugGoalkeeperHandTrackSelector,

		this,
		ESoccerDebugCategory::GoalkeeperSave,
		40,
		TEXT("Ventana: FUERA DE VENTANA"),
		FColor::Orange
	);

	SetGoalkeeperPersistentLineIfEnabled(
		bDebugGoalkeeperSaveTiming ||
		bDebugGoalkeeperSaveCoordination ||
		bDebugGoalkeeperAnimatedContact ||
		bDebugGoalkeeperHandTrackSelector,

		this,
		ESoccerDebugCategory::GoalkeeperSave,
		60,
		FString::Printf(
			TEXT("Hueso: %s"),
			*ContactBone.ToString()
		),
		FColor::White
	);

	SetGoalkeeperPersistentLineIfEnabled(
		bDebugGoalkeeperSaveTiming ||
		bDebugGoalkeeperSaveCoordination ||
		bDebugGoalkeeperAnimatedContact ||
		bDebugGoalkeeperHandTrackSelector,

		this,
		ESoccerDebugCategory::GoalkeeperSave,
		90,
		TEXT(
			"Resultado: REBOTE CORPORAL FUERA DE VENTANA"
		),
		FColor::Yellow,
		1.1f
	);

	return true;
}

void ASoccerAIController::PreparePendingGoalkeeperSaveImpact(
	ASoccerAICharacter* SoccerCharacter,
	ASoccerBall* SoccerBall,
	ESoccerGoalkeeperAction GoalkeeperAction
)
{
	ClearGoalkeeperLooseBallClaimMode();

	ClearPendingGoalkeeperSaveImpact();

	if (!IsValid(SoccerCharacter) || !IsValid(SoccerBall))
	{
		return;
	}

	if (
		GoalkeeperAction ==
		ESoccerGoalkeeperAction::None
		)
	{
		return;
	}

	PendingGoalkeeperSaveCharacter = SoccerCharacter;
	PendingGoalkeeperSaveBall = SoccerBall;
	PendingGoalkeeperSaveAction = GoalkeeperAction;

	ResetGoalkeeperBallContactTracking();

	SetGoalkeeperPersistentLineIfEnabled(
		bDebugGoalkeeperSaveTiming ||
		bDebugGoalkeeperSaveCoordination ||
		bDebugGoalkeeperAnimatedContact ||
		bDebugGoalkeeperHandTrackSelector,

		this,
		ESoccerDebugCategory::GoalkeeperSave,
		30,
		TEXT(
			"Impacto: preparado; esperando ventana"
		),
		FColor::Cyan
	);
}

void ASoccerAIController::HandleGoalkeeperContactWindowBegin(
	ASoccerAICharacter* SoccerCharacter
)
{
	if (!IsValid(SoccerCharacter))
	{
		return;
	}

	if (!HasPendingGoalkeeperSaveImpactFor(SoccerCharacter))
	{
		SetGoalkeeperPersistentLineIfEnabled(
			bDebugGoalkeeperSaveTiming ||
			bDebugGoalkeeperSaveCoordination ||
			bDebugGoalkeeperAnimatedContact ||
			bDebugGoalkeeperHandTrackSelector,

			this,
			ESoccerDebugCategory::GoalkeeperSave,
			40,
			TEXT(
				"Ventana: BEGIN sin impacto pendiente"
			),
			FColor::Yellow
		);

		SetGoalkeeperPersistentLineIfEnabled(
			bDebugGoalkeeperSaveTiming ||
			bDebugGoalkeeperSaveCoordination ||
			bDebugGoalkeeperAnimatedContact ||
			bDebugGoalkeeperHandTrackSelector,

			this,
			ESoccerDebugCategory::GoalkeeperSave,
			90,
			TEXT(
				"Resultado: SIN IMPACTO PENDIENTE"
			),
			FColor::Yellow
		);

		return;
	}

	ASoccerBall* SoccerBall =
		PendingGoalkeeperSaveBall;

	if (IsValid(SoccerBall))
	{
		PendingGoalkeeperPreviousBallLocation =
			SoccerBall->GetActorLocation();

		bHasPendingGoalkeeperPreviousBallLocation =
			true;
	}
	else
	{
		ResetGoalkeeperBallContactTracking();
	}

	SetGoalkeeperPersistentLineIfEnabled(
		bDebugGoalkeeperSaveTiming ||
		bDebugGoalkeeperSaveCoordination ||
		bDebugGoalkeeperAnimatedContact ||
		bDebugGoalkeeperHandTrackSelector,

		this,
		ESoccerDebugCategory::GoalkeeperSave,
		40,
		TEXT("Ventana: ACTIVA"),
		FColor::Green
	);
}

void ASoccerAIController::HandleGoalkeeperContactWindowTick(
	ASoccerAICharacter* SoccerCharacter
)
{
	if (!IsValid(SoccerCharacter))
	{
		return;
	}

	TryExecutePendingGoalkeeperSaveImpact(
		SoccerCharacter
	);
}

void ASoccerAIController::HandleGoalkeeperContactWindowEnd(
	ASoccerAICharacter* SoccerCharacter
)
{
	if (!IsValid(SoccerCharacter))
	{
		return;
	}

	if (HasPendingGoalkeeperSaveImpactFor(SoccerCharacter))
	{
		ClearPendingGoalkeeperSaveImpact();

		SetGoalkeeperPersistentLineIfEnabled(
			bDebugGoalkeeperSaveTiming ||
			bDebugGoalkeeperSaveCoordination ||
			bDebugGoalkeeperAnimatedContact ||
			bDebugGoalkeeperHandTrackSelector,

			this,
			ESoccerDebugCategory::GoalkeeperSave,
			40,
			TEXT("Ventana: FINALIZADA"),
			FColor::Orange
		);

		SetGoalkeeperPersistentLineIfEnabled(
			bDebugGoalkeeperSaveTiming ||
			bDebugGoalkeeperSaveCoordination ||
			bDebugGoalkeeperAnimatedContact ||
			bDebugGoalkeeperHandTrackSelector,

			this,
			ESoccerDebugCategory::GoalkeeperSave,
			90,
			TEXT("Resultado: SIN CONTACTO"),
			FColor::Orange
		);
	}
}

bool ASoccerAIController::DetectGoalkeeperAnimatedContact(
	const ASoccerAICharacter* SoccerCharacter,
	const ASoccerBall* SoccerBall,
	FGoalkeeperAnimatedContactResult& OutContactResult
)
{
	OutContactResult =
		FGoalkeeperAnimatedContactResult();

	if (
		!IsValid(SoccerCharacter) ||
		!IsValid(SoccerBall)
		)
	{
		return false;
	}

	USkeletalMeshComponent* CharacterMesh =
		SoccerCharacter->GetMesh();

	if (CharacterMesh == nullptr)
	{
		return false;
	}

	const FVector CurrentBallLocation =
		SoccerBall->GetActorLocation();

	const FVector PreviousBallLocation =
		bHasPendingGoalkeeperPreviousBallLocation
		? PendingGoalkeeperPreviousBallLocation
		: CurrentBallLocation;

	const float BallTravelDistance =
		FVector::Dist(
			PreviousBallLocation,
			CurrentBallLocation
		);

	const bool bCanUseSweptContact =
		bHasPendingGoalkeeperPreviousBallLocation &&
		BallTravelDistance <=
		GoalkeeperMaxContactSweepDistance;

	auto TestBone =
		[
			this,
			CharacterMesh,
			&PreviousBallLocation,
			&CurrentBallLocation,
			bCanUseSweptContact,
			&OutContactResult
		](
			const FName BoneName,
			const float ContactRadius,
			const bool bIsHand
			)
	{
		if (
			BoneName.IsNone() ||
			ContactRadius <= 0.0f
			)
		{
			return;
		}

		if (
			CharacterMesh->GetBoneIndex(BoneName) ==
			INDEX_NONE
			)
		{
			if (bDebugGoalkeeperAnimatedContact)
			{
				ASoccerDebugManager::Message(
					this,
					ESoccerDebugCategory::GoalkeeperSave,
					FString::Printf(
						TEXT("Hueso inexistente: %s"),
						*BoneName.ToString()
					),
					FColor::Red,
					2001
				);
			}

			return;
		}

		const FVector BoneLocation =
			CharacterMesh->GetBoneLocation(
				BoneName,
				EBoneSpaces::WorldSpace
			);

		float DistanceToBallPath =
			FVector::Dist(
				CurrentBallLocation,
				BoneLocation
			);

		if (bCanUseSweptContact)
		{
			const float SweptDistance =
				FMath::PointDistToSegment(
					BoneLocation,
					PreviousBallLocation,
					CurrentBallLocation
				);

			DistanceToBallPath =
				FMath::Min(
					DistanceToBallPath,
					SweptDistance
				);
		}

		const bool bThisBoneTouched =
			DistanceToBallPath <= ContactRadius;

		if (bIsHand)
		{
			if (
				DistanceToBallPath <
				OutContactResult.ClosestHandDistance
				)
			{
				OutContactResult.ClosestHandDistance =
					DistanceToBallPath;

				OutContactResult.ClosestHandBone =
					BoneName;
			}

			if (bThisBoneTouched)
			{
				OutContactResult.bHandContact = true;
			}
		}
		else
		{
			if (
				DistanceToBallPath <
				OutContactResult.ClosestBodyDistance
				)
			{
				OutContactResult.ClosestBodyDistance =
					DistanceToBallPath;

				OutContactResult.ClosestBodyBone =
					BoneName;
			}

			if (bThisBoneTouched)
			{
				OutContactResult.bBodyContact = true;
			}
		}

		if (bDebugGoalkeeperAnimatedContact)
		{
			const bool bShouldDrawThisVolume =
				bThisBoneTouched
				? ASoccerDebugManager::
				ShouldShowTouchedGoalkeeperContactVolumes(
					this
				)
				: ASoccerDebugManager::
				ShouldShowAllGoalkeeperContactVolumes(
					this
				);

			if (bShouldDrawThisVolume)
			{
				const FColor DebugColor =
					bThisBoneTouched
					? FColor::Green
					: (
						bIsHand
						? FColor::Cyan
						: FColor::Yellow
						);

				ASoccerDebugManager::DrawSphere(
					this,
					ESoccerDebugCategory::GoalkeeperSave,
					BoneLocation,
					ContactRadius,
					DebugColor,
					0.75f,
					10,
					1.0f
				);
			}
		}
	};

	const FName LeftHandBone(
		TEXT("LeftHand")
	);

	const FName RightHandBone(
		TEXT("RightHand")
	);

	const bool bCatchAction =
		IsGoalkeeperCatchAction(
			PendingGoalkeeperSaveAction
		);

	const bool bCanBuildTwoHandCatchZone =
		bCatchAction &&
		CharacterMesh->GetBoneIndex(
			LeftHandBone
		) != INDEX_NONE &&
		CharacterMesh->GetBoneIndex(
			RightHandBone
		) != INDEX_NONE;

	bool bCatchZoneEvaluated =
		false;

	FVector CatchZoneClosestPointForDebug =
		FVector::ZeroVector;

	if (bCanBuildTwoHandCatchZone)
	{
		const FVector LeftHandLocation =
			CharacterMesh->GetBoneLocation(
				LeftHandBone,
				EBoneSpaces::WorldSpace
			);

		const FVector RightHandLocation =
			CharacterMesh->GetBoneLocation(
				RightHandBone,
				EBoneSpaces::WorldSpace
			);

		float ClosestCatchZoneDistance =
			BIG_NUMBER;

		FVector ClosestCatchZonePoint =
			FMath::Lerp(
				LeftHandLocation,
				RightHandLocation,
				0.5f
			);

		auto ConsiderBallPointForCatchZone =
			[
				&LeftHandLocation,
				&RightHandLocation,
				&ClosestCatchZoneDistance,
				&ClosestCatchZonePoint
			](
				const FVector& BallPoint
				)
		{
			const FVector ZonePoint =
				FMath::ClosestPointOnSegment(
					BallPoint,
					LeftHandLocation,
					RightHandLocation
				);

			const float Distance =
				FVector::Dist(
					BallPoint,
					ZonePoint
				);

			if (
				Distance <
				ClosestCatchZoneDistance
				)
			{
				ClosestCatchZoneDistance =
					Distance;

				ClosestCatchZonePoint =
					ZonePoint;
			}
		};

		/*
		 * Posici�n actual.
		 */
		ConsiderBallPointForCatchZone(
			CurrentBallLocation
		);

		/*
		 * Segmento recorrido por la pelota.
		 *
		 * Usamos varias muestras porque las manos tambi�n
		 * forman un segmento. Esto evita perder una pelota
		 * r�pida que atraviesa la zona entre dos ticks.
		 */
		if (bCanUseSweptContact)
		{
			constexpr int32 CatchSweepSamples =
				6;

			for (
				int32 SampleIndex = 0;
				SampleIndex <= CatchSweepSamples;
				++SampleIndex
				)
			{
				const float SampleAlpha =
					static_cast<float>(
						SampleIndex
						) /
					static_cast<float>(
						CatchSweepSamples
						);

				ConsiderBallPointForCatchZone(
					FMath::Lerp(
						PreviousBallLocation,
						CurrentBallLocation,
						SampleAlpha
					)
				);
			}
		}

		const float EffectiveCatchRadius =
			GetGoalkeeperProfileCatchRadius(SoccerCharacter);

		const bool bCatchZoneTouched =
			ClosestCatchZoneDistance <=
			EffectiveCatchRadius;

		OutContactResult.ClosestHandDistance =
			ClosestCatchZoneDistance;

		/*
		 * Se conserva un hueso real para compatibilidad.
		 * El efecto de captura usa la zona completa.
		 */
		OutContactResult.ClosestHandBone =
			FVector::DistSquared(
				ClosestCatchZonePoint,
				LeftHandLocation
			) <=
			FVector::DistSquared(
				ClosestCatchZonePoint,
				RightHandLocation
			)
			? LeftHandBone
			: RightHandBone;

		OutContactResult.bHandContact =
			bCatchZoneTouched;

		bCatchZoneEvaluated =
			true;

		CatchZoneClosestPointForDebug =
			ClosestCatchZonePoint;

		if (bDebugGoalkeeperAnimatedContact)
		{
			const bool bShouldDrawCatchZone =
				bCatchZoneTouched
				? ASoccerDebugManager::
				ShouldShowTouchedGoalkeeperContactVolumes(
					this
				)
				: ASoccerDebugManager::
				ShouldShowAllGoalkeeperContactVolumes(
					this
				);

			if (bShouldDrawCatchZone)
			{
				const FColor ZoneColor =
					bCatchZoneTouched
					? FColor::Green
					: FColor::Cyan;

				ASoccerDebugManager::DrawLine(
					this,
					ESoccerDebugCategory::GoalkeeperSave,
					LeftHandLocation,
					RightHandLocation,
					ZoneColor,
					0.75f,
					3.0f
				);

				/*
				 * Varias esferas permiten visualizar la
				 * c�psula de captura formada por el segmento.
				 */
				constexpr int32 ZoneDebugSamples =
					4;

				for (
					int32 ZoneSampleIndex = 0;
					ZoneSampleIndex <=
					ZoneDebugSamples;
					++ZoneSampleIndex
					)
				{
					const float ZoneAlpha =
						static_cast<float>(
							ZoneSampleIndex
							) /
						static_cast<float>(
							ZoneDebugSamples
							);

					ASoccerDebugManager::DrawSphere(
						this,
						ESoccerDebugCategory::GoalkeeperSave,
						FMath::Lerp(
							LeftHandLocation,
							RightHandLocation,
							ZoneAlpha
						),
						EffectiveCatchRadius,
						ZoneColor,
						0.75f,
						10,
						1.0f
					);
				}
			}
		}
	}
	else
	{
		/*
		 * Deflect, Miss y compatibilidad:
		 * cada mano conserva su esfera individual.
		 */
		TestBone(
			LeftHandBone,
			GoalkeeperHandContactRadius,
			true
		);

		TestBone(
			RightHandBone,
			GoalkeeperHandContactRadius,
			true
		);
	}

	// ========================================================
	// BRAZOS Y HOMBROS
	// ========================================================

	TestBone(
		FName(TEXT("LeftForeArm")),
		GoalkeeperArmContactRadius,
		false
	);

	TestBone(
		FName(TEXT("RightForeArm")),
		GoalkeeperArmContactRadius,
		false
	);

	TestBone(
		FName(TEXT("LeftArm")),
		GoalkeeperArmContactRadius,
		false
	);

	TestBone(
		FName(TEXT("RightArm")),
		GoalkeeperArmContactRadius,
		false
	);

	TestBone(
		FName(TEXT("LeftShoulder")),
		GoalkeeperArmContactRadius,
		false
	);

	TestBone(
		FName(TEXT("RightShoulder")),
		GoalkeeperArmContactRadius,
		false
	);

	// ========================================================
	// TORSO Y CABEZA
	// ========================================================

	TestBone(
		FName(TEXT("Spine")),
		GoalkeeperTorsoContactRadius,
		false
	);

	TestBone(
		FName(TEXT("Spine1")),
		GoalkeeperTorsoContactRadius,
		false
	);

	TestBone(
		FName(TEXT("Spine2")),
		GoalkeeperTorsoContactRadius,
		false
	);

	TestBone(
		FName(TEXT("Hips")),
		GoalkeeperTorsoContactRadius,
		false
	);

	TestBone(
		FName(TEXT("Head")),
		GoalkeeperTorsoContactRadius,
		false
	);

	// ========================================================
	// PIERNAS Y PIES
	// ========================================================

	TestBone(
		FName(TEXT("LeftUpLeg")),
		GoalkeeperLegContactRadius,
		false
	);

	TestBone(
		FName(TEXT("RightUpLeg")),
		GoalkeeperLegContactRadius,
		false
	);

	TestBone(
		FName(TEXT("LeftLeg")),
		GoalkeeperLegContactRadius,
		false
	);

	TestBone(
		FName(TEXT("RightLeg")),
		GoalkeeperLegContactRadius,
		false
	);

	TestBone(
		FName(TEXT("LeftFoot")),
		GoalkeeperLegContactRadius,
		false
	);

	TestBone(
		FName(TEXT("RightFoot")),
		GoalkeeperLegContactRadius,
		false
	);

	if (
		bDebugGoalkeeperAnimatedContact &&
		ASoccerDebugManager::
		ShouldShowGoalkeeperContactPath(this)
		)
	{
		ASoccerDebugManager::DrawLine(
			this,
			ESoccerDebugCategory::GoalkeeperSave,
			PreviousBallLocation,
			CurrentBallLocation,
			FColor::Cyan,
			0.75f,
			2.0f
		);

		if (
			bCatchZoneEvaluated &&
			OutContactResult.bHandContact
			)
		{
			ASoccerDebugManager::DrawLine(
				this,
				ESoccerDebugCategory::GoalkeeperSave,
				CurrentBallLocation,
				CatchZoneClosestPointForDebug,
				FColor::Green,
				0.75f,
				1.5f
			);
		}
		else
		{
			FName ClosestDebugBone =
				NAME_None;

			if (OutContactResult.bHandContact)
			{
				ClosestDebugBone =
					OutContactResult.ClosestHandBone;
			}
			else if (OutContactResult.bBodyContact)
			{
				ClosestDebugBone =
					OutContactResult.ClosestBodyBone;
			}

			if (
				!ClosestDebugBone.IsNone() &&
				CharacterMesh->GetBoneIndex(
					ClosestDebugBone
				) != INDEX_NONE
				)
			{
				const FVector ClosestBoneLocation =
					CharacterMesh->GetBoneLocation(
						ClosestDebugBone,
						EBoneSpaces::WorldSpace
					);

				ASoccerDebugManager::DrawLine(
					this,
					ESoccerDebugCategory::GoalkeeperSave,
					CurrentBallLocation,
					ClosestBoneLocation,
					FColor::Green,
					0.75f,
					1.5f
				);
			}
		}
	}

	PendingGoalkeeperPreviousBallLocation =
		CurrentBallLocation;

	bHasPendingGoalkeeperPreviousBallLocation =
		true;

	return OutContactResult.HasAnyContact();
}

bool ASoccerAIController::TryExecutePendingGoalkeeperSaveImpact(
	ASoccerAICharacter* SoccerCharacter
)
{
	if (!HasPendingGoalkeeperSaveImpactFor(SoccerCharacter))
	{
		return false;
	}

	ASoccerBall* SoccerBall =
		PendingGoalkeeperSaveBall;

	const ESoccerGoalkeeperAction GoalkeeperAction =
		PendingGoalkeeperSaveAction;

	if (!IsValid(SoccerBall))
	{
		ClearPendingGoalkeeperSaveImpact();
		return false;
	}

	FGoalkeeperAnimatedContactResult ContactResult;

	const bool bContactDetected =
		DetectGoalkeeperAnimatedContact(
			SoccerCharacter,
			SoccerBall,
			ContactResult
		);

	if (!bContactDetected)
	{
		return false;
	}

	ApplyGoalkeeperSaveEffect(
		SoccerCharacter,
		SoccerBall,
		GoalkeeperAction,
		ContactResult
	);

	FName EffectiveContactBone = NAME_None;
	float EffectiveContactDistance = BIG_NUMBER;

	const bool bHandEffectExpected =
		ContactResult.bHandContact &&
		(
			IsGoalkeeperCatchAction(GoalkeeperAction) ||
			IsGoalkeeperDeflectAction(GoalkeeperAction)
			);

	if (bHandEffectExpected)
	{
		EffectiveContactBone =
			ContactResult.ClosestHandBone;

		EffectiveContactDistance =
			ContactResult.ClosestHandDistance;
	}
	else if (ContactResult.bBodyContact)
	{
		EffectiveContactBone =
			ContactResult.ClosestBodyBone;

		EffectiveContactDistance =
			ContactResult.ClosestBodyDistance;
	}
	else
	{
		EffectiveContactBone =
			ContactResult.ClosestHandBone;

		EffectiveContactDistance =
			ContactResult.ClosestHandDistance;
	}

	ClearPendingGoalkeeperSaveImpact();

	const bool bTwoHandCatchZoneContact =
		ContactResult.bHandContact &&
		IsGoalkeeperCatchAction(
			GoalkeeperAction
		);

	const FString ContactType =
		bTwoHandCatchZoneContact
		? TEXT("ZONA ENTRE MANOS")
		: (
			ContactResult.bHandContact
			? TEXT("MANO")
			: TEXT("CUERPO")
			);

	SetGoalkeeperPersistentLineIfEnabled(
		bDebugGoalkeeperSaveTiming ||
		bDebugGoalkeeperSaveCoordination ||
		bDebugGoalkeeperAnimatedContact ||
		bDebugGoalkeeperHandTrackSelector,

		this,
		ESoccerDebugCategory::GoalkeeperSave,
		50,
		FString::Printf(
			TEXT("Contacto: %s"),
			*ContactType
		),
		ContactResult.bHandContact
		? FColor::Green
		: FColor::Yellow
	);

	SetGoalkeeperPersistentLineIfEnabled(
		bDebugGoalkeeperSaveTiming ||
		bDebugGoalkeeperSaveCoordination ||
		bDebugGoalkeeperAnimatedContact ||
		bDebugGoalkeeperHandTrackSelector,

		this,
		ESoccerDebugCategory::GoalkeeperSave,
		60,
		bTwoHandCatchZoneContact
		? TEXT("Zona: ENTRE AMBAS MANOS")
		: FString::Printf(
			TEXT("Hueso: %s"),
			*EffectiveContactBone.ToString()
		),
		FColor::White
	);

	SetGoalkeeperPersistentLineIfEnabled(
		bDebugGoalkeeperSaveTiming ||
		bDebugGoalkeeperSaveCoordination ||
		bDebugGoalkeeperAnimatedContact ||
		bDebugGoalkeeperHandTrackSelector,

		this,
		ESoccerDebugCategory::GoalkeeperSave,
		70,
		FString::Printf(
			TEXT("Distancia: %.1f cm"),
			EffectiveContactDistance
		),
		FColor::White
	);

	return true;
}

void ASoccerAIController::ClearPendingGoalkeeperSaveImpact()
{
	PendingGoalkeeperSaveCharacter = nullptr;
	PendingGoalkeeperSaveBall = nullptr;

	PendingGoalkeeperSaveAction =
		ESoccerGoalkeeperAction::None;

	ResetGoalkeeperBallContactTracking();
}

void ASoccerAIController::ResetGoalkeeperBallContactTracking()
{
	PendingGoalkeeperPreviousBallLocation =
		FVector::ZeroVector;

	bHasPendingGoalkeeperPreviousBallLocation =
		false;
}

bool ASoccerAIController::HasPendingGoalkeeperSaveImpactFor(
	const ASoccerAICharacter* SoccerCharacter
) const
{
	return
		IsValid(SoccerCharacter) &&
		PendingGoalkeeperSaveCharacter == SoccerCharacter &&
		IsValid(PendingGoalkeeperSaveBall) &&
		PendingGoalkeeperSaveAction != ESoccerGoalkeeperAction::None;
}

bool ASoccerAIController::IsGoalkeeperCatchAction(
	ESoccerGoalkeeperAction GoalkeeperAction
) const
{
	switch (GoalkeeperAction)
	{
		// Acciones nuevas de captura.

	case ESoccerGoalkeeperAction::BodyBlockCatchToLeft:
	case ESoccerGoalkeeperAction::BodyBlockCatchToRight:

	case ESoccerGoalkeeperAction::CatchAbdomen:

	case ESoccerGoalkeeperAction::CatchFaceToLeft:
	case ESoccerGoalkeeperAction::CatchFaceToRight:

	case ESoccerGoalkeeperAction::CatchOverHeadJumpToLeft:
	case ESoccerGoalkeeperAction::CatchOverHeadJumpToRight:
	case ESoccerGoalkeeperAction::CatchOverHeadRunJump:

	case ESoccerGoalkeeperAction::DivingSaveFloorToLeft:
	case ESoccerGoalkeeperAction::DivingSaveFloorToRight:

	case ESoccerGoalkeeperAction::DivingSaveOneMeterToLeft:
	case ESoccerGoalkeeperAction::DivingSaveOneMeterToRight:

	case ESoccerGoalkeeperAction::ScoopToLeft:
	case ESoccerGoalkeeperAction::ScoopToRight:
		return true;

		/*
		 * Compatibilidad temporal.
		 *
		 * El selector autom�tico y el sweeper todav�a pueden producir
		 * estas acciones. Se eliminar�n al migrar esos sistemas.
		 */
	case ESoccerGoalkeeperAction::CatchLow:
	case ESoccerGoalkeeperAction::CatchChest:
	case ESoccerGoalkeeperAction::CatchHighForward:
	case ESoccerGoalkeeperAction::CatchHighRight:
		return true;

	default:
		return false;
	}
}

bool ASoccerAIController::IsGoalkeeperDeflectAction(
	ESoccerGoalkeeperAction GoalkeeperAction
) const
{
	switch (GoalkeeperAction)
	{
		// Acciones nuevas de rechazo intencional.

	case ESoccerGoalkeeperAction::BodyBlockDeflectToLeft:
	case ESoccerGoalkeeperAction::BodyBlockDeflectToRight:
		return true;

		/*
		 * Compatibilidad temporal con las acciones anteriores.
		 */
	case ESoccerGoalkeeperAction::BodyBlockLeft:
	case ESoccerGoalkeeperAction::BodyBlockRight:
	case ESoccerGoalkeeperAction::BodyBlockLeftAlt:
	case ESoccerGoalkeeperAction::DivingSaveLeft:
	case ESoccerGoalkeeperAction::DivingSaveRight:
		return true;

	default:
		return false;
	}
}

void ASoccerAIController::ApplyGoalkeeperSaveEffect(
	ASoccerAICharacter* SoccerCharacter,
	ASoccerBall* SoccerBall,
	ESoccerGoalkeeperAction GoalkeeperAction,
	const FGoalkeeperAnimatedContactResult& ContactResult
)
{
	ClearGoalkeeperLooseBallClaimMode();

	if (
		!IsValid(SoccerCharacter) ||
		!IsValid(SoccerBall) ||
		!ContactResult.HasAnyContact()
		)
	{
		return;
	}

	const bool bHandsLegallyAllowed =
		IsGoalkeeperHandlingAllowedAtLocation(
			SoccerCharacter,
			SoccerBall->GetActorLocation()
		);

	/*
	 * La mano tiene prioridad cuando existe un contacto v�lido.
	 *
	 * Si al mismo tiempo la pelota est� cerca de la mano y del
	 * antebrazo, una acci�n Catch sigue consider�ndose captura.
	 */
	if (
		ContactResult.bHandContact &&
		bHandsLegallyAllowed
		)
	{
		if (IsGoalkeeperCatchAction(GoalkeeperAction))
		{
			ApplyGoalkeeperCatchEffect(
				SoccerCharacter,
				SoccerBall
			);

			return;
		}

		if (IsGoalkeeperDeflectAction(GoalkeeperAction))
		{
			ApplyGoalkeeperDeflectEffect(
				SoccerCharacter,
				SoccerBall
			);

			return;
		}
	}

	/*
	 * Casos que llegan ac�:
	 *
	 * - Catch toc� torso, brazo, pierna, cabeza, etc.
	 * - Deflect toc� una parte que no es la mano.
	 * - Miss tuvo cualquier contacto accidental.
	 * - La mano toc� la pelota fuera del �rea permitida.
	 */
	ApplyGoalkeeperBodyReboundEffect(
		SoccerCharacter,
		SoccerBall
	);
}

void ASoccerAIController::ApplyGoalkeeperCatchEffect(
	ASoccerAICharacter* SoccerCharacter,
	ASoccerBall* SoccerBall
)
{
	if (
		!IsValid(SoccerCharacter) ||
		!IsValid(SoccerBall)
		)
	{
		return;
	}

	ReleaseGoalkeeperOpponentBallPossession(
		SoccerCharacter,
		SoccerBall
	);

	const bool bBallAttachedToHands =
		SoccerCharacter->HoldGoalkeeperBallInHands(
			SoccerBall
		);

	if (!bBallAttachedToHands)
	{
		/*
		 * Fallback temporal.
		 *
		 * Si el socket no existe, mantenemos la posesi�n l�gica
		 * anterior en lugar de perder por completo la pelota.
		 */
		SoccerBall->SetPossessed(false);
		SoccerBall->StopBallKeepingPhysics();

		const FVector Forward =
			SoccerCharacter->GetActorForwardVector();

		const UCapsuleComponent* CapsuleComponent =
			SoccerCharacter->GetCapsuleComponent();

		const float GroundZ =
			CapsuleComponent != nullptr
			? SoccerCharacter->GetActorLocation().Z -
			CapsuleComponent->GetScaledCapsuleHalfHeight()
			: SoccerCharacter->GetActorLocation().Z;

		FVector CatchBallLocation =
			SoccerCharacter->GetActorLocation() +
			Forward * GoalkeeperCatchBallForwardOffset;

		CatchBallLocation.Z =
			GroundZ +
			GoalkeeperCatchBallHeight;

		SoccerBall->SetActorLocation(
			CatchBallLocation,
			false,
			nullptr,
			ETeleportType::TeleportPhysics
		);

		SoccerCharacter->PossessAIBall(
			SoccerBall
		);
	}

	// La captura ya est� consolidada en el personaje. Ahora se
	// actualiza la posesi�n del partido de forma inmediata, para que
	// el equipo rival deje de tratar la pelota como disputable.
	if (IsValid(MatchManager))
	{
		MatchManager->RegisterControlledBallPossession(
			SoccerCharacter
		);
	}

	SetGoalkeeperBehaviorMode(
		ESoccerGoalkeeperBehaviorMode::HoldingBall
	);

	SetGoalkeeperPersistentLineIfEnabled(
		bDebugGoalkeeperSaveTiming ||
		bDebugGoalkeeperSaveCoordination ||
		bDebugGoalkeeperAnimatedContact ||
		bDebugGoalkeeperHandTrackSelector,

		this,
		ESoccerDebugCategory::GoalkeeperSave,
		90,
		bBallAttachedToHands
		? TEXT("Resultado: CAPTURA")
		: TEXT(
			"Resultado: CAPTURA CON FALLBACK"
		),
		bBallAttachedToHands
		? FColor::Green
		: FColor::Yellow,
		1.1f
	);
}

void ASoccerAIController::ApplyGoalkeeperDeflectEffect(
	ASoccerAICharacter* SoccerCharacter,
	ASoccerBall* SoccerBall
)
{
	if (
		!IsValid(SoccerCharacter) ||
		!IsValid(SoccerBall)
		)
	{
		return;
	}

	ReleaseGoalkeeperOpponentBallPossession(
		SoccerCharacter,
		SoccerBall
	);

	FVector GoalCenterLocation = GetGoalkeeperHomeLocation(SoccerCharacter);

	if (IsValid(MatchManager))
	{
		const ASoccerField* SoccerField = MatchManager->GetSoccerField();
		if (IsValid(SoccerField))
		{
			GoalCenterLocation = SoccerField->GetGoalCenterWorldLocation(
				MatchManager->GetOwnGoalLineSign(SoccerCharacter->GetTeam()),
				0.0f
			);
		}
	}

	FVector DeflectDirection =
		SoccerBall->GetActorLocation() -
		GoalCenterLocation;

	DeflectDirection.Z = 0.0f;

	if (DeflectDirection.IsNearlyZero())
	{
		DeflectDirection =
			SoccerCharacter->GetActorForwardVector();

		DeflectDirection.Z = 0.0f;
	}

	DeflectDirection =
		DeflectDirection.GetSafeNormal();

	ApplyGoalkeeperPhysicalRebound(
		SoccerBall,
		DeflectDirection,
		GoalkeeperDeflectForwardStrength,
		GoalkeeperDeflectUpwardStrength,
		GoalkeeperDeflectIncomingNormalVelocityRetention,
		GoalkeeperDeflectTangentialVelocityRetention,
		GoalkeeperDeflectVerticalVelocityRetention,
		GoalkeeperDeflectAngularVelocityRetention
	);

	if (IsValid(MatchManager))
	{
		MatchManager->RegisterGoalkeeperReboundTouch(SoccerCharacter);
	}

	if (
		bUseGoalkeeperPostReboundEarlyRecovery &&
		SoccerCharacter->IsGoalkeeperActionActive()
		)
	{
		SoccerCharacter->StopGoalkeeperActionMontage(
			GoalkeeperPostReboundRecoveryBlendOutTime
		);
	}

	SetGoalkeeperBehaviorMode(
		ESoccerGoalkeeperBehaviorMode::Retreating
	);

	MarkGoalkeeperSweeperResolved();

	SetGoalkeeperPersistentLineIfEnabled(
		bDebugGoalkeeperSaveTiming ||
		bDebugGoalkeeperSaveCoordination ||
		bDebugGoalkeeperAnimatedContact ||
		bDebugGoalkeeperHandTrackSelector,

		this,
		ESoccerDebugCategory::GoalkeeperSave,
		90,
		TEXT(
			"Resultado: RECHAZO CON MANOS"
		),
		FColor::Orange,
		1.1f
	);
}

void ASoccerAIController::ApplyGoalkeeperBodyReboundEffect(
	ASoccerAICharacter* SoccerCharacter,
	ASoccerBall* SoccerBall
)
{
	if (
		!IsValid(SoccerCharacter) ||
		!IsValid(SoccerBall)
		)
	{
		return;
	}

	ReleaseGoalkeeperOpponentBallPossession(
		SoccerCharacter,
		SoccerBall
	);

	FVector GoalCenterLocation = GetGoalkeeperHomeLocation(SoccerCharacter);

	if (IsValid(MatchManager))
	{
		const ASoccerField* SoccerField = MatchManager->GetSoccerField();
		if (IsValid(SoccerField))
		{
			GoalCenterLocation = SoccerField->GetGoalCenterWorldLocation(
				MatchManager->GetOwnGoalLineSign(SoccerCharacter->GetTeam()),
				0.0f
			);
		}
	}

	/*
	 * El rebote sale aproximadamente alej�ndose del centro del
	 * arco. Es menos fuerte y menos elevado que un rechazo
	 * intencional con las manos.
	 */
	FVector ReboundDirection =
		SoccerBall->GetActorLocation() -
		GoalCenterLocation;

	ReboundDirection.Z = 0.0f;

	if (ReboundDirection.IsNearlyZero())
	{
		ReboundDirection =
			SoccerCharacter->GetActorForwardVector();

		ReboundDirection.Z = 0.0f;
	}

	ReboundDirection =
		ReboundDirection.GetSafeNormal();

	ApplyGoalkeeperPhysicalRebound(
		SoccerBall,
		ReboundDirection,
		GoalkeeperBodyReboundForwardStrength,
		GoalkeeperBodyReboundUpwardStrength,
		GoalkeeperBodyIncomingNormalVelocityRetention,
		GoalkeeperBodyTangentialVelocityRetention,
		GoalkeeperBodyVerticalVelocityRetention,
		GoalkeeperBodyAngularVelocityRetention
	);

	if (IsValid(MatchManager))
	{
		MatchManager->RegisterGoalkeeperReboundTouch(SoccerCharacter);
	}

	if (
		bUseGoalkeeperPostReboundEarlyRecovery &&
		SoccerCharacter->IsGoalkeeperActionActive()
		)
	{
		SoccerCharacter->StopGoalkeeperActionMontage(
			GoalkeeperPostReboundRecoveryBlendOutTime
		);
	}

	SetGoalkeeperBehaviorMode(
		ESoccerGoalkeeperBehaviorMode::Retreating
	);

	MarkGoalkeeperSweeperResolved();

	SetGoalkeeperPersistentLineIfEnabled(
		bDebugGoalkeeperSaveTiming ||
		bDebugGoalkeeperSaveCoordination ||
		bDebugGoalkeeperAnimatedContact ||
		bDebugGoalkeeperHandTrackSelector,

		this,
		ESoccerDebugCategory::GoalkeeperSave,
		90,
		TEXT("Resultado: REBOTE CORPORAL"),
		FColor::Yellow,
		1.1f
	);
}

void ASoccerAIController::ApplyGoalkeeperPhysicalRebound(
	ASoccerBall* SoccerBall,
	const FVector& OutwardDirection,
	float ForwardStrength,
	float UpwardStrength,
	float NormalVelocityRetention,
	float TangentialVelocityRetention,
	float VerticalVelocityRetention,
	float AngularVelocityRetention
) const
{
	if (!IsValid(SoccerBall))
	{
		return;
	}

	FVector SafeOutwardDirection = OutwardDirection;
	SafeOutwardDirection.Z = 0.0f;
	SafeOutwardDirection = SafeOutwardDirection.GetSafeNormal();

	if (SafeOutwardDirection.IsNearlyZero())
	{
		return;
	}

	const FVector IncomingVelocity = SoccerBall->GetBallPhysicsVelocity();
	FVector IncomingHorizontalVelocity = IncomingVelocity;
	IncomingHorizontalVelocity.Z = 0.0f;

	const float IncomingNormalSpeed = FVector::DotProduct(
		IncomingHorizontalVelocity,
		SafeOutwardDirection
	);
	const FVector IncomingTangentialVelocity =
		IncomingHorizontalVelocity -
		SafeOutwardDirection * IncomingNormalSpeed;

	const float SafeNormalRetention = FMath::Clamp(
		NormalVelocityRetention,
		0.0f,
		1.0f
	);
	const float SafeTangentialRetention = FMath::Clamp(
		TangentialVelocityRetention,
		0.0f,
		1.0f
	);
	const float SafeVerticalRetention = FMath::Clamp(
		VerticalVelocityRetention,
		0.0f,
		1.0f
	);

	FVector ReboundVelocity =
		SafeOutwardDirection *
		FMath::Abs(IncomingNormalSpeed) *
		SafeNormalRetention +
		IncomingTangentialVelocity *
		SafeTangentialRetention;

	ReboundVelocity.Z =
		FMath::Abs(IncomingVelocity.Z) * SafeVerticalRetention;

	SoccerBall->SetPossessed(false);
	SoccerBall->ApplyAerialContactVelocity(
		ReboundVelocity,
		FMath::Clamp(AngularVelocityRetention, 0.0f, 1.0f)
	);
	SoccerBall->Kick(
		SafeOutwardDirection,
		FMath::Max(0.0f, ForwardStrength),
		FMath::Max(0.0f, UpwardStrength)
	);
}
bool ASoccerAIController::TryBuildGoalkeeperDistributionTarget(
	const ASoccerAICharacter* SoccerCharacter,
	bool bUseSweeperClearance,
	FVector& OutTargetLocation,
	float& OutHorizontalSpeed,
	float& OutMinTravelTime,
	float& OutMaxTravelTime,
	FString& OutDebugLabel
) const
{
	OutTargetLocation = FVector::ZeroVector;
	OutHorizontalSpeed = 0.0f;
	OutMinTravelTime = 0.0f;
	OutMaxTravelTime = 0.0f;
	OutDebugLabel = TEXT("None");

	if (!IsValid(SoccerCharacter))
	{
		return false;
	}

	if (bUseGoalkeeperSmartDistribution)
	{
		FVector TeammateTargetLocation = FVector::ZeroVector;
		float TeammateScore = 0.0f;

		ASoccerCharacterBase* BestTeammate =
			FindBestGoalkeeperDistributionTeammate(
				SoccerCharacter,
				TeammateTargetLocation,
				TeammateScore
			);

		if (
			IsValid(BestTeammate) &&
			TeammateScore >= GoalkeeperDistributionMinPassScore
			)
		{
			OutTargetLocation = TeammateTargetLocation;
			OutHorizontalSpeed = GoalkeeperDistributionGroundPassHorizontalSpeed;
			OutMinTravelTime = GoalkeeperDistributionGroundPassMinTravelTime;
			OutMaxTravelTime = GoalkeeperDistributionGroundPassMaxTravelTime;
			OutDebugLabel = FString::Printf(
				TEXT("pase seguro %.2f"),
				TeammateScore
			);

			return true;
		}
	}

	OutTargetLocation =
		BuildGoalkeeperLongClearanceTargetLocation(
			SoccerCharacter,
			bUseSweeperClearance
		);

	if (OutTargetLocation.IsNearlyZero())
	{
		return false;
	}

	OutHorizontalSpeed =
		bUseSweeperClearance
		? GoalkeeperSweeperClearanceHorizontalSpeed
		: GoalkeeperClearanceHorizontalSpeed;

	OutMinTravelTime =
		bUseSweeperClearance
		? GoalkeeperSweeperClearanceMinTravelTime
		: GoalkeeperClearanceMinTravelTime;

	OutMaxTravelTime =
		bUseSweeperClearance
		? GoalkeeperSweeperClearanceMaxTravelTime
		: GoalkeeperClearanceMaxTravelTime;

	OutDebugLabel = TEXT("despeje largo");

	return true;
}

ASoccerCharacterBase* ASoccerAIController::FindBestGoalkeeperDistributionTeammate(
	const ASoccerAICharacter* SoccerCharacter,
	FVector& OutTargetLocation,
	float& OutScore
) const
{
	OutTargetLocation = FVector::ZeroVector;
	OutScore = -1000.0f;

	if (!IsValid(SoccerCharacter))
	{
		return nullptr;
	}

	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return nullptr;
	}

	ASoccerCharacterBase* BestTeammate = nullptr;

	for (TActorIterator<ASoccerCharacterBase> It(World); It; ++It)
	{
		ASoccerCharacterBase* Candidate = *It;

		if (!IsValid(Candidate))
		{
			continue;
		}

		if (Candidate == SoccerCharacter)
		{
			continue;
		}

		if (Candidate->GetTeam() != SoccerCharacter->GetTeam())
		{
			continue;
		}

		const ASoccerAICharacter* CandidateAI =
			Cast<ASoccerAICharacter>(Candidate);

		if (
			IsValid(CandidateAI) &&
			CandidateAI->GetPlayerRole() == ESoccerPlayerRole::Goalkeeper
			)
		{
			continue;
		}

		const float DistanceToTeammate =
			FVector::Dist2D(
				SoccerCharacter->GetActorLocation(),
				Candidate->GetActorLocation()
			);

		if (
			DistanceToTeammate < GoalkeeperDistributionMinPassDistance ||
			DistanceToTeammate > GoalkeeperDistributionMaxPassDistance
			)
		{
			continue;
		}

		FVector CandidateVelocity = Candidate->GetVelocity();
		CandidateVelocity.Z = 0.0f;

		FVector CandidateTargetLocation = Candidate->GetActorLocation();

		if (!CandidateVelocity.IsNearlyZero())
		{
			CandidateTargetLocation +=
				CandidateVelocity.GetSafeNormal() *
				GoalkeeperDistributionTeammateLeadDistance;
		}

		CandidateTargetLocation.Z =
			SoccerCharacter->GetActorLocation().Z;

		const float CandidateScore =
			ScoreGoalkeeperDistributionTeammate(
				SoccerCharacter,
				Candidate,
				CandidateTargetLocation
			);

		if (CandidateScore > OutScore)
		{
			OutScore = CandidateScore;
			OutTargetLocation = CandidateTargetLocation;
			BestTeammate = Candidate;
		}
	}

	return BestTeammate;
}

float ASoccerAIController::ScoreGoalkeeperDistributionTeammate(
	const ASoccerAICharacter* SoccerCharacter,
	const ASoccerCharacterBase* Teammate,
	const FVector& TargetLocation
) const
{
	if (!IsValid(SoccerCharacter) || !IsValid(Teammate))
	{
		return -1000.0f;
	}

	const FVector FromLocation =
		SoccerCharacter->GetActorLocation();

	const float Distance =
		FVector::Dist2D(
			FromLocation,
			TargetLocation
		);

	if (Distance <= KINDA_SMALL_NUMBER)
	{
		return -1000.0f;
	}

	FVector ToTarget = TargetLocation - FromLocation;
	ToTarget.Z = 0.0f;
	ToTarget = ToTarget.GetSafeNormal();

	FVector OutfieldDirection =
		GetGoalkeeperOutfieldDirection(SoccerCharacter);

	OutfieldDirection.Z = 0.0f;
	OutfieldDirection = OutfieldDirection.GetSafeNormal();

	const float ForwardDot =
		!OutfieldDirection.IsNearlyZero()
		? FVector::DotProduct(ToTarget, OutfieldDirection)
		: 0.0f;

	const float ForwardScore =
		FMath::Clamp(
			(ForwardDot + 1.0f) * 0.5f,
			0.0f,
			1.0f
		);

	const float DistanceScore =
		1.0f -
		FMath::Clamp(
			FMath::Abs(Distance - GoalkeeperDistributionIdealPassDistance) /
			GoalkeeperDistributionIdealPassDistance,
			0.0f,
			1.0f
		);

	const float TargetDepth =
		GetGoalkeeperDepthFromGoal(
			SoccerCharacter,
			TargetLocation
		);

	const float DepthScore =
		FMath::Clamp(
			TargetDepth / GoalkeeperDistributionMaxPassDistance,
			0.0f,
			1.0f
		);

	const int32 NearbyOpponents =
		CountGoalkeeperDistributionOpponentsAroundLocation(
			SoccerCharacter,
			TargetLocation,
			GoalkeeperDistributionOpponentPressureRadius
		);

	const bool bLaneBlocked =
		IsGoalkeeperDistributionLaneBlockedByOpponent(
			SoccerCharacter,
			FromLocation,
			TargetLocation
		);

	float Score = 0.0f;

	Score += ForwardScore * 0.42f;
	Score += DistanceScore * 0.26f;
	Score += DepthScore * 0.24f;

	if (NearbyOpponents > 0)
	{
		Score -= FMath::Min(0.45f, NearbyOpponents * 0.18f);
	}

	if (bLaneBlocked)
	{
		Score -= 0.55f;
	}

	if (TargetDepth < 260.0f)
	{
		Score -= 0.30f;
	}

	// Build-up style also reaches a goalkeeper who has secured the ball. The
	// safety checks above remain authoritative; tactics only rank viable outlets.
	if (
		IsValid(MatchManager) &&
		MatchManager->ShouldUseCollectiveTacticsForOpenPlay(SoccerCharacter->GetTeam())
		)
	{
		const FSoccerTeamTacticalPlan TacticalPlan =
			MatchManager->GetTacticalPlanForTeam(SoccerCharacter->GetTeam());

		const float DistributionDistanceAlpha = FMath::Clamp(
			Distance / FMath::Max(GoalkeeperDistributionMaxPassDistance, 1.0f),
			0.0f,
			1.0f
		);

		if (TacticalPlan.BuildUpStyle == ESoccerBuildUpStyle::ShortPossession)
		{
			Score += (1.0f - DistributionDistanceAlpha) * 0.18f;

			if (!bLaneBlocked && NearbyOpponents == 0)
			{
				Score += 0.08f;
			}
		}
		else if (TacticalPlan.BuildUpStyle == ESoccerBuildUpStyle::Direct)
		{
			Score += DistributionDistanceAlpha * 0.12f;
			Score += DepthScore * 0.12f;
		}

		if (TacticalPlan.AttackingTempo == ESoccerAttackingTempo::Patient)
		{
			if (!bLaneBlocked && NearbyOpponents == 0)
			{
				Score += 0.06f;
			}
		}
		else if (TacticalPlan.AttackingTempo == ESoccerAttackingTempo::Fast)
		{
			Score += ForwardScore * 0.06f;
			Score += DepthScore * 0.05f;
		}
	}

	return Score;
}

bool ASoccerAIController::IsGoalkeeperDistributionLaneBlockedByOpponent(
	const ASoccerAICharacter* SoccerCharacter,
	const FVector& FromLocation,
	const FVector& TargetLocation
) const
{
	if (!IsValid(SoccerCharacter))
	{
		return false;
	}

	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return false;
	}

	FVector Start = FromLocation;
	FVector End = TargetLocation;

	Start.Z = 0.0f;
	End.Z = 0.0f;

	const FVector Segment = End - Start;
	const float SegmentLength = Segment.Size();

	if (SegmentLength < 150.0f)
	{
		return false;
	}

	const FVector Direction = Segment / SegmentLength;

	for (TActorIterator<ASoccerCharacterBase> It(World); It; ++It)
	{
		ASoccerCharacterBase* Candidate = *It;

		if (!IsValid(Candidate))
		{
			continue;
		}

		if (Candidate == SoccerCharacter)
		{
			continue;
		}

		if (Candidate->GetTeam() == SoccerCharacter->GetTeam())
		{
			continue;
		}

		FVector CandidateLocation = Candidate->GetActorLocation();
		CandidateLocation.Z = 0.0f;

		const float ProjectionDistance =
			FVector::DotProduct(
				CandidateLocation - Start,
				Direction
			);

		if (
			ProjectionDistance < 160.0f ||
			ProjectionDistance > SegmentLength - 80.0f
			)
		{
			continue;
		}

		const FVector ClosestPointOnLine =
			Start + Direction * ProjectionDistance;

		const float DistanceToLine =
			FVector::Dist2D(
				CandidateLocation,
				ClosestPointOnLine
			);

		if (DistanceToLine <= GoalkeeperDistributionLaneBlockRadius)
		{
			return true;
		}
	}

	return false;
}

int32 ASoccerAIController::CountGoalkeeperDistributionOpponentsAroundLocation(
	const ASoccerAICharacter* SoccerCharacter,
	const FVector& Location,
	float Radius
) const
{
	if (!IsValid(SoccerCharacter) || Radius <= 0.0f)
	{
		return 0;
	}

	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return 0;
	}

	int32 Count = 0;

	for (TActorIterator<ASoccerCharacterBase> It(World); It; ++It)
	{
		ASoccerCharacterBase* Candidate = *It;

		if (!IsValid(Candidate))
		{
			continue;
		}

		if (Candidate == SoccerCharacter)
		{
			continue;
		}

		if (Candidate->GetTeam() == SoccerCharacter->GetTeam())
		{
			continue;
		}

		if (
			FVector::Dist2D(
				Candidate->GetActorLocation(),
				Location
			) <= Radius
			)
		{
			Count++;
		}
	}

	return Count;
}

bool ASoccerAIController::ShouldGoalkeeperUseUrgentDistribution(
	const ASoccerAICharacter* SoccerCharacter
) const
{
	if (!IsValid(SoccerCharacter))
	{
		return false;
	}

	const int32 OpponentsNearGoalkeeper =
		CountGoalkeeperDistributionOpponentsAroundLocation(
			SoccerCharacter,
			SoccerCharacter->GetActorLocation(),
			GoalkeeperDistributionUrgentPressureRadius
		);

	if (OpponentsNearGoalkeeper > 0)
	{
		return true;
	}

	ASoccerCharacterBase* SweeperThreat =
		GoalkeeperSweeperThreatCharacter.Get();

	if (
		IsValid(SweeperThreat) &&
		SweeperThreat->GetTeam() != SoccerCharacter->GetTeam() &&
		FVector::Dist2D(
			SweeperThreat->GetActorLocation(),
			SoccerCharacter->GetActorLocation()
		) <= GoalkeeperDistributionUrgentPressureRadius
		)
	{
		return true;
	}

	return false;
}

FVector ASoccerAIController::BuildGoalkeeperLongClearanceTargetLocation(
	const ASoccerAICharacter* SoccerCharacter,
	bool bUseSweeperClearance
) const
{
	if (!IsValid(SoccerCharacter))
	{
		return FVector::ZeroVector;
	}

	FVector ForwardDirection =
		GetGoalkeeperOutfieldDirection(SoccerCharacter);

	ForwardDirection.Z = 0.0f;
	ForwardDirection = ForwardDirection.GetSafeNormal();

	if (ForwardDirection.IsNearlyZero())
	{
		if (IsValid(MatchManager))
		{
			FVector TargetReferenceLocation =
				MatchManager->GetShotTargetLocation(SoccerCharacter);

			ForwardDirection =
				TargetReferenceLocation - SoccerCharacter->GetActorLocation();

			ForwardDirection.Z = 0.0f;
			ForwardDirection = ForwardDirection.GetSafeNormal();
		}
	}

	if (ForwardDirection.IsNearlyZero())
	{
		ForwardDirection = SoccerCharacter->GetActorForwardVector();
		ForwardDirection.Z = 0.0f;
		ForwardDirection = ForwardDirection.GetSafeNormal();
	}

	if (ForwardDirection.IsNearlyZero())
	{
		return FVector::ZeroVector;
	}

	FVector RightDirection =
		FVector::CrossProduct(
			FVector::UpVector,
			ForwardDirection
		);

	RightDirection.Z = 0.0f;
	RightDirection = RightDirection.GetSafeNormal();

	const float ForwardDistance =
		bUseSweeperClearance
		? GoalkeeperSweeperClearanceForwardDistance
		: GoalkeeperClearanceForwardDistance;

	const float LateralRandomRange =
		bUseSweeperClearance
		? GoalkeeperSweeperClearanceLateralRandomRange
		: GoalkeeperClearanceLateralRandomRange;

	const float RandomLateralOffset =
		FMath::RandRange(
			-LateralRandomRange,
			LateralRandomRange
		);

	FVector ClearanceTargetLocation =
		SoccerCharacter->GetActorLocation()
		+ ForwardDirection * ForwardDistance
		+ RightDirection * RandomLateralOffset;

	ClearanceTargetLocation.Z =
		SoccerCharacter->GetActorLocation().Z;

	return ClearanceTargetLocation;
}

FString ASoccerAIController::
GetGoalkeeperDistributionTypeText(
	ESoccerGoalkeeperDistributionType DistributionType
) const
{
	switch (DistributionType)
	{
	case ESoccerGoalkeeperDistributionType::OverhandThrow:
		return TEXT("Overhand Throw");

	case ESoccerGoalkeeperDistributionType::DropKick:
		return TEXT("Drop Kick");

	case ESoccerGoalkeeperDistributionType::PlacingBallShort:
		return TEXT("Placing Ball Short");

	case ESoccerGoalkeeperDistributionType::PlacingBallLong:
		return TEXT("Placing Ball Long");

	case ESoccerGoalkeeperDistributionType::None:
	default:
		return TEXT("None");
	}
}

ESoccerGoalkeeperDistributionType
ASoccerAIController::SelectGoalkeeperDistributionType(
	bool bUrgentDistribution,
	bool bHasSafeTeammatePass,
	bool& bOutForcedByDebug
) const
{
	bOutForcedByDebug = true;

	switch (GoalkeeperDistributionDebugMode)
	{
		case ESoccerGoalkeeperDistributionDebugMode::
		ForceOverhandThrow:
			return
				ESoccerGoalkeeperDistributionType::
				OverhandThrow;

			case ESoccerGoalkeeperDistributionDebugMode::
			ForceDropKick:
				return
					ESoccerGoalkeeperDistributionType::
					DropKick;

				case ESoccerGoalkeeperDistributionDebugMode::
				ForcePlacingBallShort:
					return
						ESoccerGoalkeeperDistributionType::
						PlacingBallShort;

					case ESoccerGoalkeeperDistributionDebugMode::
					ForcePlacingBallLong:
						return
							ESoccerGoalkeeperDistributionType::
							PlacingBallLong;

					case ESoccerGoalkeeperDistributionDebugMode::Auto:
					default:
						break;
	}

	bOutForcedByDebug = false;

	if (bUrgentDistribution)
	{
		return
			bHasSafeTeammatePass
			? ESoccerGoalkeeperDistributionType::
			OverhandThrow
			: ESoccerGoalkeeperDistributionType::
			DropKick;
	}

	return
		bHasSafeTeammatePass
		? ESoccerGoalkeeperDistributionType::
		PlacingBallShort
		: ESoccerGoalkeeperDistributionType::
		PlacingBallLong;
}

FVector ASoccerAIController::
BuildGoalkeeperShortFallbackTargetLocation(
	const ASoccerAICharacter* SoccerCharacter
) const
{
	if (!IsValid(SoccerCharacter))
	{
		return FVector::ZeroVector;
	}

	FVector ForwardDirection =
		GetGoalkeeperOutfieldDirection(
			SoccerCharacter
		);

	ForwardDirection.Z = 0.0f;
	ForwardDirection =
		ForwardDirection.GetSafeNormal();

	if (ForwardDirection.IsNearlyZero())
	{
		ForwardDirection =
			SoccerCharacter->GetActorForwardVector();

		ForwardDirection.Z = 0.0f;
		ForwardDirection =
			ForwardDirection.GetSafeNormal();
	}

	if (ForwardDirection.IsNearlyZero())
	{
		return FVector::ZeroVector;
	}

	FVector RightDirection =
		FVector::CrossProduct(
			FVector::UpVector,
			ForwardDirection
		);

	RightDirection.Z = 0.0f;
	RightDirection =
		RightDirection.GetSafeNormal();

	const float LateralOffset =
		FMath::RandRange(
			-GoalkeeperDistributionFallbackShortLateralRange,
			GoalkeeperDistributionFallbackShortLateralRange
		);

	FVector TargetLocation =
		SoccerCharacter->GetActorLocation()
		+ ForwardDirection *
		GoalkeeperDistributionFallbackShortForwardDistance
		+ RightDirection * LateralOffset;

	TargetLocation.Z =
		SoccerCharacter->GetActorLocation().Z;

	return TargetLocation;
}

bool ASoccerAIController::
TryBuildGoalkeeperDistributionPlan(
	const ASoccerAICharacter* SoccerCharacter,
	FGoalkeeperDistributionPlan& OutPlan
) const
{
	OutPlan = FGoalkeeperDistributionPlan();

	if (!IsValid(SoccerCharacter))
	{
		return false;
	}

	OutPlan.bUrgent =
		ShouldGoalkeeperUseUrgentDistribution(
			SoccerCharacter
		);

	FVector SafePassTargetLocation =
		FVector::ZeroVector;

	float SafePassScore = 0.0f;

	ASoccerCharacterBase* BestTeammate = nullptr;

	if (bUseGoalkeeperSmartDistribution)
	{
		BestTeammate =
			FindBestGoalkeeperDistributionTeammate(
				SoccerCharacter,
				SafePassTargetLocation,
				SafePassScore
			);
	}

	FSoccerTeamTacticalPlan TacticalPlan;
	const bool bHasCollectiveTacticalPlan =
		IsValid(MatchManager) &&
		MatchManager->ShouldUseCollectiveTacticsForOpenPlay(SoccerCharacter->GetTeam());

	if (bHasCollectiveTacticalPlan)
	{
		TacticalPlan =
			MatchManager->GetTacticalPlanForTeam(SoccerCharacter->GetTeam());
	}

	float RequiredSafePassScore = GoalkeeperDistributionMinPassScore;

	if (
		bHasCollectiveTacticalPlan &&
		TacticalPlan.BuildUpStyle == ESoccerBuildUpStyle::ShortPossession
		)
	{
		// Short build-up may accept a slightly less ideal outlet, but the lane and
		// nearby-opponent penalties inside the score still protect the ball.
		RequiredSafePassScore -= 0.08f;
	}

	OutPlan.bHasSafeTeammatePass =
		IsValid(BestTeammate) &&
		SafePassScore >= RequiredSafePassScore &&
		!SafePassTargetLocation.IsNearlyZero();

	OutPlan.Type =
		SelectGoalkeeperDistributionType(
			OutPlan.bUrgent,
			OutPlan.bHasSafeTeammatePass,
			OutPlan.bForcedByDebug
		);

	if (
		!OutPlan.bForcedByDebug &&
		!OutPlan.bUrgent &&
		bHasCollectiveTacticalPlan &&
		TacticalPlan.BuildUpStyle == ESoccerBuildUpStyle::Direct
		)
	{
		// Direct build-up deliberately chooses the existing long distribution
		// path instead of inventing a separate goalkeeper kick behavior.
		OutPlan.Type = ESoccerGoalkeeperDistributionType::PlacingBallLong;
	}

	const bool bSelectedShortDistribution =
		OutPlan.Type ==
		ESoccerGoalkeeperDistributionType::
		OverhandThrow ||
		OutPlan.Type ==
		ESoccerGoalkeeperDistributionType::
		PlacingBallShort;

	if (bSelectedShortDistribution)
	{
		if (OutPlan.bHasSafeTeammatePass)
		{
			OutPlan.TargetLocation =
				SafePassTargetLocation;

			OutPlan.DebugLabel =
				FString::Printf(
					TEXT("pase seguro %.2f"),
					SafePassScore
				);
		}
		else
		{
			OutPlan.TargetLocation =
				BuildGoalkeeperShortFallbackTargetLocation(
					SoccerCharacter
				);

			OutPlan.DebugLabel =
				TEXT("destino corto de prueba");
		}

		OutPlan.HorizontalSpeed =
			GoalkeeperDistributionGroundPassHorizontalSpeed;

		OutPlan.MinTravelTime =
			GoalkeeperDistributionGroundPassMinTravelTime;

		OutPlan.MaxTravelTime =
			GoalkeeperDistributionGroundPassMaxTravelTime;
	}
	else
	{
		OutPlan.TargetLocation =
			BuildGoalkeeperLongClearanceTargetLocation(
				SoccerCharacter,
				false
			);

		OutPlan.HorizontalSpeed =
			GoalkeeperClearanceHorizontalSpeed;

		OutPlan.MinTravelTime =
			GoalkeeperClearanceMinTravelTime;

		OutPlan.MaxTravelTime =
			GoalkeeperClearanceMaxTravelTime;

		OutPlan.DebugLabel =
			TEXT("despeje largo");
	}

	if (!OutPlan.HasValidData())
	{
		return false;
	}

	return true;
}

bool ASoccerAIController::TryGoalkeeperClearCaughtBall(
	ASoccerAICharacter* SoccerCharacter
)
{
	if (!IsValid(SoccerCharacter))
	{
		return false;
	}

	if (!SoccerCharacter->IsGoalkeeperHoldingBall())
	{
		return false;
	}

	if (bGoalkeeperDistributionActive)
	{
		return true;
	}

	const bool bUrgentDistribution =
		ShouldGoalkeeperUseUrgentDistribution(
			SoccerCharacter
		);

	const float RequiredHoldTime =
		bUrgentDistribution
		? GoalkeeperUrgentHoldBallBeforeDistributionTime
		: GoalkeeperHoldBallBeforeClearTime;

	if (
		SoccerCharacter->
		GetTimeSinceAIPossessionStarted() <
		RequiredHoldTime
		)
	{
		return false;
	}

	FGoalkeeperDistributionPlan Plan;

	if (
		!TryBuildGoalkeeperDistributionPlan(
			SoccerCharacter,
			Plan
		)
		)
	{
		return false;
	}

	ASoccerBall* ControlledBall =
		SoccerCharacter->GetControlledAIBall();

	if (!IsValid(ControlledBall))
	{
		return false;
	}

	ActiveGoalkeeperDistributionType =
		Plan.Type;

	ActiveGoalkeeperDistributionCharacter =
		SoccerCharacter;

	ActiveGoalkeeperDistributionBall =
		ControlledBall;

	ActiveGoalkeeperDistributionTargetLocation =
		ApplyGoalkeeperProfileDistributionTargetExecution(
			SoccerCharacter,
			Plan.TargetLocation
		);

	ActiveGoalkeeperDistributionHorizontalSpeed =
		Plan.HorizontalSpeed *
		GetGoalkeeperProfileDistributionSpeedMultiplier(
			SoccerCharacter
		);

	ActiveGoalkeeperDistributionMinTravelTime =
		Plan.MinTravelTime;

	ActiveGoalkeeperDistributionMaxTravelTime =
		Plan.MaxTravelTime;

	bGoalkeeperDistributionActive = true;
	bGoalkeeperDistributionMontageStarted = false;
	bGoalkeeperDistributionBallReleased = false;
	bGoalkeeperDistributionBallPlaced = false;
	bGoalkeeperDistributionBallKicked = false;
	bGoalkeeperDistributionRuleTouchRegistered = false;

	ActiveGoalkeeperDistributionFinishDeadline =
		-1000.0f;

	SetGoalkeeperBehaviorMode(
		ESoccerGoalkeeperBehaviorMode::Distributing
	);

	StopMovement();

	if (
		ASoccerDebugManager::IsEnabled(this, ESoccerDebugCategory::GoalkeeperDistribution) &&
		bDebugGoalkeeperDistribution &&
		GEngine
		)
	{
		const FString SelectionModeText =
			Plan.bForcedByDebug
			? TEXT("FORZADA")
			: TEXT("AUTO");

		GEngine->AddOnScreenDebugMessage(
			-1,
			2.0f,
			FColor::Cyan,
			FString::Printf(
				TEXT(
					"GK seleccion: %s [%s] | %s"
				),
				*GetGoalkeeperDistributionTypeText(
					Plan.Type
				),
				*SelectionModeText,
				*Plan.DebugLabel
			)
		);
	}

	return true;
}

bool ASoccerAIController::HasActiveGoalkeeperDistributionFor(
	const ASoccerAICharacter* SoccerCharacter
) const
{
	return
		bGoalkeeperDistributionActive &&
		IsValid(SoccerCharacter) &&
		ActiveGoalkeeperDistributionCharacter ==
		SoccerCharacter;
}

bool ASoccerAIController::UpdateGoalkeeperDistribution(
	ASoccerAICharacter* SoccerCharacter,
	float DeltaTime
)
{
	if (
		!HasActiveGoalkeeperDistributionFor(
			SoccerCharacter
		)
		)
	{
		ClearGoalkeeperDistribution();
		return false;
	}

	if (!IsValid(ActiveGoalkeeperDistributionBall))
	{
		ClearGoalkeeperDistribution();
		return false;
	}

	// Cancela cualquier movimiento de navegaci�n anterior.
	// El desplazamiento de distribuci�n se aplicar� despu�s
	// mediante la CurveTable.
	StopMovement();

	ClearFocus(
		EAIFocusPriority::Gameplay
	);

	SetGoalkeeperBehaviorMode(
		ESoccerGoalkeeperBehaviorMode::Distributing
	);

	// ========================================================
	// PREPARACI�N E INICIO DEL MONTAGE
	// ========================================================

	if (!bGoalkeeperDistributionMontageStarted)
	{
		FacePawnTowardLocation(
			ActiveGoalkeeperDistributionTargetLocation,
			DeltaTime
		);

		FVector ForwardDirection =
			SoccerCharacter->GetActorForwardVector();

		ForwardDirection.Z = 0.0f;
		ForwardDirection =
			ForwardDirection.GetSafeNormal();

		FVector TargetDirection =
			ActiveGoalkeeperDistributionTargetLocation -
			SoccerCharacter->GetActorLocation();

		TargetDirection.Z = 0.0f;
		TargetDirection =
			TargetDirection.GetSafeNormal();

		if (TargetDirection.IsNearlyZero())
		{
			ClearGoalkeeperDistribution();
			return false;
		}

		const float FacingDot =
			FMath::Clamp(
				FVector::DotProduct(
					ForwardDirection,
					TargetDirection
				),
				-1.0f,
				1.0f
			);

		const float FacingAngleDegrees =
			FMath::RadiansToDegrees(
				FMath::Acos(
					FacingDot
				)
			);

		if (
			FacingAngleDegrees >
			GoalkeeperDistributionFacingToleranceDegrees
			)
		{
			return true;
		}

		/*
		 * Validamos la CurveTable y guardamos:
		 *
		 * - ubicaci�n inicial;
		 * - direcci�n frontal inicial;
		 * - direcci�n lateral inicial.
		 *
		 * Lo hacemos antes de reproducir el montage para
		 * evitar iniciar una animaci�n in place sin movimiento.
		 */
		if (bUseGoalkeeperDistributionCurveMotion)
		{
			const bool bCurveMotionInitialized =
				InitializeGoalkeeperDistributionCurveMotion(
					SoccerCharacter
				);

			if (!bCurveMotionInitialized)
			{
				if (
					ASoccerDebugManager::IsEnabled(this, ESoccerDebugCategory::GoalkeeperDistribution) &&
		bDebugGoalkeeperDistributionCurveMotion &&
					GEngine
					)
				{
					GEngine->AddOnScreenDebugMessage(
						-1,
						3.0f,
						FColor::Red,
						FString::Printf(
							TEXT(
								"GK Curve Motion: no se pudo "
								"inicializar para %s"
							),
							*GetGoalkeeperDistributionTypeText(
								ActiveGoalkeeperDistributionType
							)
						)
					);
				}

				ClearGoalkeeperDistribution();
				return true;
			}
		}

		const float MontageDuration =
			SoccerCharacter->
			PlayGoalkeeperDistributionMontage(
				ActiveGoalkeeperDistributionType
			);

		if (MontageDuration <= 0.0f)
		{
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(
					-1,
					2.0f,
					FColor::Red,
					TEXT(
						"GK: no se pudo iniciar montage "
						"de distribucion"
					)
				);
			}

			ClearGoalkeeperDistribution();
			return true;
		}

		/*
		 * Solo lo marcamos como iniciado cuando:
		 *
		 * - la curva pudo inicializarse;
		 * - el montage realmente empez�.
		 */
		bGoalkeeperDistributionMontageStarted =
			true;

		/*
		 * En tiempo cero las curvas deber�an devolver:
		 *
		 * Forward = 0
		 * Lateral = 0
		 *
		 * Aplicarlo ahora no deber�a mover nada, pero deja
		 * el actor sincronizado desde el comienzo.
		 */
		if (
			bUseGoalkeeperDistributionCurveMotion &&
			bGoalkeeperDistributionCurveMotionInitialized
			)
		{
			ApplyGoalkeeperDistributionCurveMotionAtTime(
				SoccerCharacter,
				0.0f
			);
		}

		const float CurrentTime =
			GetWorld() != nullptr
			? GetWorld()->GetTimeSeconds()
			: 0.0f;

		ActiveGoalkeeperDistributionFinishDeadline =
			CurrentTime +
			MontageDuration +
			GoalkeeperDistributionMontageTimeoutExtraTime;

		return true;
	}

	// ========================================================
	// MONTAGE EN EJECUCI�N
	// ========================================================

	if (
		bUseGoalkeeperDistributionCurveMotion &&
		bGoalkeeperDistributionCurveMotionInitialized
		)
	{
		UpdateGoalkeeperDistributionCurveMotion(
			SoccerCharacter
		);
	}

	// ========================================================
	// TIMEOUT DE SEGURIDAD
	// ========================================================

	const float CurrentTime =
		GetWorld() != nullptr
		? GetWorld()->GetTimeSeconds()
		: 0.0f;

	if (
		ActiveGoalkeeperDistributionFinishDeadline >
		0.0f &&
		CurrentTime >=
		ActiveGoalkeeperDistributionFinishDeadline
		)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				1.2f,
				FColor::Orange,
				TEXT(
					"GK distribucion: finalizacion por timeout"
				)
			);
		}

		HandleGoalkeeperDistributionEvent(
			SoccerCharacter,
			ESoccerGoalkeeperDistributionEvent::Finished
		);
	}

	return true;
}

void ASoccerAIController::
HandleGoalkeeperDistributionEvent(
	ASoccerAICharacter* SoccerCharacter,
	ESoccerGoalkeeperDistributionEvent EventType
)
{
	if (
		!HasActiveGoalkeeperDistributionFor(
			SoccerCharacter
		)
		)
	{
		return;
	}

	if (!IsValid(ActiveGoalkeeperDistributionBall))
	{
		ClearGoalkeeperDistribution();
		return;
	}

	switch (EventType)
	{
	case ESoccerGoalkeeperDistributionEvent::ReleaseBall:
	{
		if (bGoalkeeperDistributionBallReleased)
		{
			return;
		}

		if (
			ActiveGoalkeeperDistributionType ==
			ESoccerGoalkeeperDistributionType::
			OverhandThrow
			)
		{
			if (
				!SoccerCharacter->
				IsGoalkeeperHoldingBall()
				)
			{
				ClearGoalkeeperDistribution();
				return;
			}

			if (
				!bGoalkeeperDistributionRuleTouchRegistered
				)
			{
				if (
					!TryRegisterAIKickTouchForRules(
						SoccerCharacter
					)
					)
				{
					ClearGoalkeeperDistribution();
					return;
				}

				bGoalkeeperDistributionRuleTouchRegistered =
					true;
			}

			const bool bReleased =
				SoccerCharacter->
				ReleaseHeldGoalkeeperBallToAirTarget(
					ActiveGoalkeeperDistributionTargetLocation,
					ActiveGoalkeeperDistributionHorizontalSpeed,
					ActiveGoalkeeperDistributionMinTravelTime,
					ActiveGoalkeeperDistributionMaxTravelTime
				);

			if (!bReleased)
			{
				ClearGoalkeeperDistribution();
				return;
			}

			bGoalkeeperDistributionBallReleased = true;
			bGoalkeeperDistributionBallKicked = true;
		}
		else if (
			ActiveGoalkeeperDistributionType ==
			ESoccerGoalkeeperDistributionType::
			DropKick
			)
		{
			const bool bReleased =
				SoccerCharacter->
				ReleaseHeldGoalkeeperBallForDrop(
					GoalkeeperDropKickReleaseForwardSpeed,
					GoalkeeperDropKickReleaseLateralSpeed,
					GoalkeeperDropKickReleaseUpwardSpeed
				);

			if (!bReleased)
			{
				ClearGoalkeeperDistribution();
				return;
			}

			bGoalkeeperDistributionBallReleased = true;
		}
		else
		{
			// Las distribuciones PlacingBall usan PlaceBall,
			// no ReleaseBall.
			return;
		}

		if (
			ASoccerDebugManager::IsEnabled(this, ESoccerDebugCategory::GoalkeeperDistribution) &&
		bDebugGoalkeeperDistribution &&
			GEngine
			)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				1.0f,
				FColor::Green,
				TEXT("GK notify: Release Ball")
			);
		}

		return;
	}

	case ESoccerGoalkeeperDistributionEvent::PlaceBall:
	{
		const bool bIsPlacingDistribution =
			ActiveGoalkeeperDistributionType ==
			ESoccerGoalkeeperDistributionType::
			PlacingBallShort ||
			ActiveGoalkeeperDistributionType ==
			ESoccerGoalkeeperDistributionType::
			PlacingBallLong;

		if (!bIsPlacingDistribution)
		{
			return;
		}

		if (bGoalkeeperDistributionBallPlaced)
		{
			return;
		}

		const bool bPlaced =
			SoccerCharacter->
			PlaceHeldGoalkeeperBallForDistribution();

		if (!bPlaced)
		{
			ClearGoalkeeperDistribution();
			return;
		}

		bGoalkeeperDistributionBallReleased = true;
		bGoalkeeperDistributionBallPlaced = true;

		if (
			ASoccerDebugManager::IsEnabled(this, ESoccerDebugCategory::GoalkeeperDistribution) &&
		bDebugGoalkeeperDistribution &&
			GEngine
			)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				1.0f,
				FColor::Yellow,
				TEXT("GK notify: Place Ball")
			);
		}

		return;
	}

	case ESoccerGoalkeeperDistributionEvent::KickBall:
	{
		if (bGoalkeeperDistributionBallKicked)
		{
			return;
		}

		const bool bIsDropKick =
			ActiveGoalkeeperDistributionType ==
			ESoccerGoalkeeperDistributionType::
			DropKick;

		const bool bIsPlacingShort =
			ActiveGoalkeeperDistributionType ==
			ESoccerGoalkeeperDistributionType::
			PlacingBallShort;

		const bool bIsPlacingLong =
			ActiveGoalkeeperDistributionType ==
			ESoccerGoalkeeperDistributionType::
			PlacingBallLong;

		if (
			!bIsDropKick &&
			!bIsPlacingShort &&
			!bIsPlacingLong
			)
		{
			return;
		}

		// Failsafe si falt� el notify anterior.
		if (
			bIsDropKick &&
			!bGoalkeeperDistributionBallReleased
			)
		{
			HandleGoalkeeperDistributionEvent(
				SoccerCharacter,
				ESoccerGoalkeeperDistributionEvent::
				ReleaseBall
			);
		}

		if (
			(bIsPlacingShort || bIsPlacingLong) &&
			!bGoalkeeperDistributionBallPlaced
			)
		{
			HandleGoalkeeperDistributionEvent(
				SoccerCharacter,
				ESoccerGoalkeeperDistributionEvent::
				PlaceBall
			);
		}

		if (
			!HasActiveGoalkeeperDistributionFor(
				SoccerCharacter
			)
			)
		{
			return;
		}

		if (
			!bGoalkeeperDistributionRuleTouchRegistered
			)
		{
			if (
				!TryRegisterAIKickTouchForRules(
					SoccerCharacter
				)
				)
			{
				ClearGoalkeeperDistribution();
				return;
			}

			bGoalkeeperDistributionRuleTouchRegistered =
				true;
		}

		const bool bUseGroundPass =
			bIsPlacingShort;

		const bool bKicked =
			SoccerCharacter->
			KickReleasedGoalkeeperDistributionBall(
				ActiveGoalkeeperDistributionBall,
				ActiveGoalkeeperDistributionTargetLocation,
				ActiveGoalkeeperDistributionHorizontalSpeed,
				ActiveGoalkeeperDistributionMinTravelTime,
				ActiveGoalkeeperDistributionMaxTravelTime,
				bUseGroundPass
			);

		if (!bKicked)
		{
			ClearGoalkeeperDistribution();
			return;
		}

		bGoalkeeperDistributionBallKicked = true;

		if (
			ASoccerDebugManager::IsEnabled(this, ESoccerDebugCategory::GoalkeeperDistribution) &&
		bDebugGoalkeeperDistribution &&
			GEngine
			)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				1.0f,
				FColor::Green,
				bUseGroundPass
				? TEXT(
					"GK notify: Kick Ball rasante"
				)
				: TEXT(
					"GK notify: Kick Ball aereo"
				)
			);
		}

		return;
	}

	case ESoccerGoalkeeperDistributionEvent::Finished:
	{
		// Failsafe espec�fico para cada distribuci�n.
		switch (ActiveGoalkeeperDistributionType)
		{
			case ESoccerGoalkeeperDistributionType::
			OverhandThrow:
				if (!bGoalkeeperDistributionBallKicked)
				{
					HandleGoalkeeperDistributionEvent(
						SoccerCharacter,
						ESoccerGoalkeeperDistributionEvent::
						ReleaseBall
					);
				}
				break;

				case ESoccerGoalkeeperDistributionType::
				DropKick:
					if (!bGoalkeeperDistributionBallReleased)
					{
						HandleGoalkeeperDistributionEvent(
							SoccerCharacter,
							ESoccerGoalkeeperDistributionEvent::
							ReleaseBall
						);
					}

					if (
						HasActiveGoalkeeperDistributionFor(
							SoccerCharacter
						) &&
						!bGoalkeeperDistributionBallKicked
						)
					{
						HandleGoalkeeperDistributionEvent(
							SoccerCharacter,
							ESoccerGoalkeeperDistributionEvent::
							KickBall
						);
					}
					break;

					case ESoccerGoalkeeperDistributionType::
					PlacingBallShort:
						case ESoccerGoalkeeperDistributionType::
						PlacingBallLong:
							if (!bGoalkeeperDistributionBallPlaced)
							{
								HandleGoalkeeperDistributionEvent(
									SoccerCharacter,
									ESoccerGoalkeeperDistributionEvent::
									PlaceBall
								);
							}

							if (
								HasActiveGoalkeeperDistributionFor(
									SoccerCharacter
								) &&
								!bGoalkeeperDistributionBallKicked
								)
							{
								HandleGoalkeeperDistributionEvent(
									SoccerCharacter,
									ESoccerGoalkeeperDistributionEvent::
									KickBall
								);
							}
							break;

						case ESoccerGoalkeeperDistributionType::None:
						default:
							break;
		}

		if (
			!HasActiveGoalkeeperDistributionFor(
				SoccerCharacter
			)
			)
		{
			return;
		}

		CommitGoalkeeperDistributionCurveMotionToEnd(
			SoccerCharacter
		);

		SetGoalkeeperBehaviorMode(
			ESoccerGoalkeeperBehaviorMode::Retreating
		);

		MarkGoalkeeperSweeperResolved();

		if (
			ASoccerDebugManager::IsEnabled(this, ESoccerDebugCategory::GoalkeeperDistribution) &&
		bDebugGoalkeeperDistribution &&
			GEngine
			)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				1.2f,
				FColor::Cyan,
				FString::Printf(
					TEXT(
						"GK distribucion terminada: %s"
					),
					*GetGoalkeeperDistributionTypeText(
						ActiveGoalkeeperDistributionType
					)
				)
			);
		}

		ClearGoalkeeperDistribution();

		return;
	}

	default:
		return;
	}
}

void ASoccerAIController::
ClearGoalkeeperDistribution()
{
	// Si la pelota hab�a quedado congelada en el c�sped,
	// pero la distribuci�n fue cancelada antes de patear,
	// devolvemos la f�sica para que no quede inm�vil para siempre.
	if (
		IsValid(ActiveGoalkeeperDistributionBall) &&
		bGoalkeeperDistributionBallPlaced &&
		!bGoalkeeperDistributionBallKicked
		)
	{
		ActiveGoalkeeperDistributionBall->
			SetPossessed(false);

		ActiveGoalkeeperDistributionBall->
			StopBallKeepingPhysics();
	}

	// Si la distribuci�n se cancel� mientras todav�a
	// sosten�a la pelota, tambi�n la liberamos.
	if (
		bGoalkeeperDistributionActive &&
		IsValid(ActiveGoalkeeperDistributionCharacter) &&
		ActiveGoalkeeperDistributionCharacter->
		IsGoalkeeperHoldingBall()
		)
	{
		ActiveGoalkeeperDistributionCharacter->
			ReleaseAIBall();
	}

	bGoalkeeperDistributionActive = false;
	bGoalkeeperDistributionMontageStarted = false;
	bGoalkeeperDistributionBallReleased = false;
	bGoalkeeperDistributionBallPlaced = false;
	bGoalkeeperDistributionBallKicked = false;
	bGoalkeeperDistributionRuleTouchRegistered = false;

	ActiveGoalkeeperDistributionType =
		ESoccerGoalkeeperDistributionType::None;

	ActiveGoalkeeperDistributionCharacter =
		nullptr;

	ActiveGoalkeeperDistributionBall =
		nullptr;

	ActiveGoalkeeperDistributionTargetLocation =
		FVector::ZeroVector;

	ActiveGoalkeeperDistributionHorizontalSpeed =
		0.0f;

	ActiveGoalkeeperDistributionMinTravelTime =
		0.0f;

	ActiveGoalkeeperDistributionMaxTravelTime =
		0.0f;

	ActiveGoalkeeperDistributionFinishDeadline =
		-1000.0f;

	bGoalkeeperDistributionCurveMotionInitialized =
		false;

	ActiveGoalkeeperDistributionMotionStartLocation =
		FVector::ZeroVector;

	ActiveGoalkeeperDistributionMotionForwardDirection =
		FVector::ForwardVector;

	ActiveGoalkeeperDistributionMotionRightDirection =
		FVector::RightVector;

}

UCurveTable*
ASoccerAIController::
GetGoalkeeperDistributionMotionCurveTable(
	ESoccerGoalkeeperDistributionType DistributionType
) const
{
	switch (DistributionType)
	{
	case ESoccerGoalkeeperDistributionType::OverhandThrow:
		return GoalkeeperOverhandThrowMotionCurveTable;

	case ESoccerGoalkeeperDistributionType::DropKick:
		return GoalkeeperDropKickMotionCurveTable;

	case ESoccerGoalkeeperDistributionType::PlacingBallShort:
		return GoalkeeperPlacingBallShortMotionCurveTable;

	case ESoccerGoalkeeperDistributionType::PlacingBallLong:
		return GoalkeeperPlacingBallLongMotionCurveTable;

	case ESoccerGoalkeeperDistributionType::None:
	default:
		return nullptr;
	}
}

void ASoccerAIController::
GetGoalkeeperDistributionMotionScales(
	ESoccerGoalkeeperDistributionType DistributionType,
	float& OutForwardScale,
	float& OutLateralScale
) const
{
	OutForwardScale = 1.0f;
	OutLateralScale = 1.0f;

	switch (DistributionType)
	{
	case ESoccerGoalkeeperDistributionType::OverhandThrow:
		OutForwardScale =
			GoalkeeperOverhandThrowForwardMotionScale;

		OutLateralScale =
			GoalkeeperOverhandThrowLateralMotionScale;
		break;

	case ESoccerGoalkeeperDistributionType::DropKick:
		OutForwardScale =
			GoalkeeperDropKickForwardMotionScale;

		OutLateralScale =
			GoalkeeperDropKickLateralMotionScale;
		break;

	case ESoccerGoalkeeperDistributionType::PlacingBallShort:
		OutForwardScale =
			GoalkeeperPlacingBallShortForwardMotionScale;

		OutLateralScale =
			GoalkeeperPlacingBallShortLateralMotionScale;
		break;

	case ESoccerGoalkeeperDistributionType::PlacingBallLong:
		OutForwardScale =
			GoalkeeperPlacingBallLongForwardMotionScale;

		OutLateralScale =
			GoalkeeperPlacingBallLongLateralMotionScale;
		break;

	case ESoccerGoalkeeperDistributionType::None:
	default:
		break;
	}
}

bool ASoccerAIController::
EvaluateGoalkeeperDistributionLocalDisplacement(
	ESoccerGoalkeeperDistributionType DistributionType,
	float MontageTime,
	FVector2D& OutLocalDisplacement
) const
{
	OutLocalDisplacement =
		FVector2D::ZeroVector;

	UCurveTable* CurveTable =
		GetGoalkeeperDistributionMotionCurveTable(
			DistributionType
		);

	if (CurveTable == nullptr)
	{
		return false;
	}

	static const FName ForwardRowName(
		TEXT("Forward")
	);

	static const FName LateralRowName(
		TEXT("Lateral")
	);

	const FString ContextString =
		TEXT("GoalkeeperDistributionCurveMotion");

	const FRealCurve* ForwardCurve =
		CurveTable->FindCurve(
			ForwardRowName,
			ContextString,
			false
		);

	const FRealCurve* LateralCurve =
		CurveTable->FindCurve(
			LateralRowName,
			ContextString,
			false
		);

	if (
		ForwardCurve == nullptr ||
		LateralCurve == nullptr
		)
	{
		return false;
	}

	const float SafeMontageTime =
		FMath::Max(
			0.0f,
			MontageTime
		);

	OutLocalDisplacement.X =
		ForwardCurve->Eval(
			SafeMontageTime
		);

	OutLocalDisplacement.Y =
		LateralCurve->Eval(
			SafeMontageTime
		);

	return true;
}

bool ASoccerAIController::
InitializeGoalkeeperDistributionCurveMotion(
	ASoccerAICharacter* SoccerCharacter
)
{
	bGoalkeeperDistributionCurveMotionInitialized =
		false;

	if (
		!bUseGoalkeeperDistributionCurveMotion ||
		!IsValid(SoccerCharacter)
		)
	{
		return false;
	}

	FVector2D InitialDisplacement;

	if (
		!EvaluateGoalkeeperDistributionLocalDisplacement(
			ActiveGoalkeeperDistributionType,
			0.0f,
			InitialDisplacement
		)
		)
	{
		if (
			ASoccerDebugManager::IsEnabled(this, ESoccerDebugCategory::GoalkeeperDistribution) &&
		bDebugGoalkeeperDistributionCurveMotion &&
			GEngine
			)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				2.0f,
				FColor::Red,
				TEXT(
					"GK Curve Motion: tabla o filas no asignadas"
				)
			);
		}

		return false;
	}

	ActiveGoalkeeperDistributionMotionStartLocation =
		SoccerCharacter->GetActorLocation();

	ActiveGoalkeeperDistributionMotionForwardDirection =
		SoccerCharacter->GetActorForwardVector();

	ActiveGoalkeeperDistributionMotionForwardDirection.Z =
		0.0f;

	ActiveGoalkeeperDistributionMotionForwardDirection =
		ActiveGoalkeeperDistributionMotionForwardDirection.
		GetSafeNormal();

	ActiveGoalkeeperDistributionMotionRightDirection =
		SoccerCharacter->GetActorRightVector();

	ActiveGoalkeeperDistributionMotionRightDirection.Z =
		0.0f;

	ActiveGoalkeeperDistributionMotionRightDirection =
		ActiveGoalkeeperDistributionMotionRightDirection.
		GetSafeNormal();

	if (
		ActiveGoalkeeperDistributionMotionForwardDirection.
		IsNearlyZero() ||
		ActiveGoalkeeperDistributionMotionRightDirection.
		IsNearlyZero()
		)
	{
		return false;
	}

	bGoalkeeperDistributionCurveMotionInitialized =
		true;

	return true;
}

bool ASoccerAIController::
ApplyGoalkeeperDistributionCurveMotionAtTime(
	ASoccerAICharacter* SoccerCharacter,
	float MontageTime
)
{
	if (
		!bUseGoalkeeperDistributionCurveMotion ||
		!bGoalkeeperDistributionCurveMotionInitialized ||
		!IsValid(SoccerCharacter)
		)
	{
		return false;
	}

	FVector2D LocalDisplacement;

	if (
		!EvaluateGoalkeeperDistributionLocalDisplacement(
			ActiveGoalkeeperDistributionType,
			MontageTime,
			LocalDisplacement
		)
		)
	{
		return false;
	}

	float ForwardScale = 1.0f;
	float LateralScale = 1.0f;

	GetGoalkeeperDistributionMotionScales(
		ActiveGoalkeeperDistributionType,
		ForwardScale,
		LateralScale
	);

	const FVector DesiredHorizontalOffset =
		ActiveGoalkeeperDistributionMotionForwardDirection *
		(LocalDisplacement.X * ForwardScale)
		+
		ActiveGoalkeeperDistributionMotionRightDirection *
		(LocalDisplacement.Y * LateralScale);

	FVector DesiredActorLocation =
		ActiveGoalkeeperDistributionMotionStartLocation +
		DesiredHorizontalOffset;

	// Nunca copiamos la altura de Hips a la c�psula.
	DesiredActorLocation.Z =
		SoccerCharacter->GetActorLocation().Z;

	FVector MovementDelta =
		DesiredActorLocation -
		SoccerCharacter->GetActorLocation();

	MovementDelta.Z = 0.0f;

	SoccerCharacter->
		MoveGoalkeeperDistributionByWorldDelta(
			MovementDelta
		);

	if (
		ASoccerDebugManager::IsEnabled(this, ESoccerDebugCategory::GoalkeeperDistribution) &&
		bDebugGoalkeeperDistributionCurveMotion &&
		GetWorld() != nullptr
		)
	{
		DrawDebugLine(
			GetWorld(),
			SoccerCharacter->GetActorLocation(),
			DesiredActorLocation,
			FColor::Magenta,
			false,
			0.05f,
			0,
			2.0f
		);
	}

	return true;
}

bool ASoccerAIController::
UpdateGoalkeeperDistributionCurveMotion(
	ASoccerAICharacter* SoccerCharacter
)
{
	if (
		!bUseGoalkeeperDistributionCurveMotion ||
		!bGoalkeeperDistributionCurveMotionInitialized ||
		!IsValid(SoccerCharacter)
		)
	{
		return false;
	}

	float MontagePosition = 0.0f;
	float MontageLength = 0.0f;

	if (
		!SoccerCharacter->
		GetGoalkeeperDistributionMontagePlaybackState(
			ActiveGoalkeeperDistributionType,
			MontagePosition,
			MontageLength
		)
		)
	{
		return false;
	}

	const float SafeMontagePosition =
		FMath::Clamp(
			MontagePosition,
			0.0f,
			MontageLength
		);

	return ApplyGoalkeeperDistributionCurveMotionAtTime(
		SoccerCharacter,
		SafeMontagePosition
	);
}

bool ASoccerAIController::
CommitGoalkeeperDistributionCurveMotionToEnd(
	ASoccerAICharacter* SoccerCharacter
)
{
	if (
		!bUseGoalkeeperDistributionCurveMotion ||
		!bGoalkeeperDistributionCurveMotionInitialized ||
		!IsValid(SoccerCharacter)
		)
	{
		return false;
	}

	UCurveTable* CurveTable =
		GetGoalkeeperDistributionMotionCurveTable(
			ActiveGoalkeeperDistributionType
		);

	if (CurveTable == nullptr)
	{
		return false;
	}

	const FRealCurve* ForwardCurve =
		CurveTable->FindCurve(
			FName(TEXT("Forward")),
			TEXT("GK Distribution Motion End"),
			false
		);

	if (ForwardCurve == nullptr)
	{
		return false;
	}

	float MinCurveTime = 0.0f;
	float MaxCurveTime = 0.0f;

	ForwardCurve->GetTimeRange(
		MinCurveTime,
		MaxCurveTime
	);

	return ApplyGoalkeeperDistributionCurveMotionAtTime(
		SoccerCharacter,
		MaxCurveTime
	);
}

//////

bool ASoccerAIController::TryPossessBallIfClose(
	ASoccerAICharacter* SoccerCharacter,
	ASoccerBall* SoccerBall
)
{
	if (!IsValid(SoccerCharacter) || !IsValid(SoccerBall))
	{
		return false;
	}

	if (!IsValid(MatchManager))
	{
		return false;
	}

	if (SoccerCharacter->GetTimeSinceAIBallReleased() < AIPostReleaseRepossessCooldown)
	{
		return false;
	}

	if (MatchManager->GetPossessionTeam() != ESoccerPossessionTeam::None)
	{
		return false;
	}

	if (!MatchManager->CanCharacterClaimLooseBallNow(SoccerCharacter))
	{
		return false;
	}

	const float DistanceToBall = FVector::Dist2D(
		SoccerCharacter->GetActorLocation(),
		SoccerBall->GetActorLocation()
	);

	if (DistanceToBall > AIBallPossessionDistance)
	{
		return false;
	}

	if (!IsBallAtAIPossessionHeight(SoccerCharacter, SoccerBall))
	{
		return false;
	}

	if (!MatchManager->CanCharacterTouchBallNow(SoccerCharacter))
	{
		return false;
	}

	if (!MatchManager->TryRegisterIntentionalBallTouch(SoccerCharacter))
	{
		return false;
	}

	SoccerCharacter->PossessAIBall(SoccerBall);

	return true;
}

bool ASoccerAIController::ReleasePhysicalPossessionIfBallEscaped(
	ASoccerAICharacter* SoccerCharacter
)
{
	if (
		!IsValid(SoccerCharacter) ||
		!SoccerCharacter->IsAIPossessingBall() ||
		SoccerCharacter->IsGoalkeeperHoldingBall() ||
		!IsValid(MatchManager)
		)
	{
		return false;
	}

	ASoccerBall* SoccerBall = MatchManager->GetSoccerBall();
	if (!IsValid(SoccerBall))
	{
		return false;
	}

	const float DistanceToBall = FVector::Dist2D(
		SoccerCharacter->GetActorLocation(),
		SoccerBall->GetActorLocation()
	);

	if (
		DistanceToBall <= FMath::Max(1.0f, AIPhysicalPossessionMaxDistance) &&
		IsBallAtAIPossessionHeight(SoccerCharacter, SoccerBall)
		)
	{
		return false;
	}

	// The ball did not teleport or stop: it genuinely left the player's control.
	// Clear only the logical ownership and let the normal free-ball race resume.
	SoccerCharacter->ReleaseAIBall(false);
	MatchManager->ReleaseControlledBallPossession(SoccerCharacter);
	SoccerCharacter->SetAIChasingBall(true);

	return true;
}

bool ASoccerAIController::TryHandleGoalAreaAttackerHoldingRespect(
	ASoccerAICharacter* SoccerCharacter,
	ESoccerAIOrder CurrentOrder
)
{
	if (!IsValid(SoccerCharacter) || !IsValid(MatchManager))
	{
		return false;
	}

	FVector WaitLocation = FVector::ZeroVector;

	if (
		!MatchManager->TryGetGoalAreaAttackerHoldingWaitLocation(
			SoccerCharacter,
			WaitLocation
		)
		)
	{
		return false;
	}

	ClearCurrentRecoveryIntent();
	ClearFilteredMoveRequest();
	ClearFilteredDefenseMoveRequest();
	SoccerCharacter->SetAIChasingBall(false);

	MoveToLocationWithAIMovement(
		CurrentOrder,
		WaitLocation,
		FMath::Max(
			1.0f,
			GoalAreaAttackerHoldingMoveAcceptanceRadius
		),
		false
	);

	const float DistanceToWaitLocation = FVector::Dist2D(
		SoccerCharacter->GetActorLocation(),
		WaitLocation
	);

	if (
		DistanceToWaitLocation >=
		FMath::Max(0.0f, GoalAreaAttackerHoldingRunDistance)
		)
	{
		SoccerCharacter->RequestAIMovementMode(
			ESoccerAIMovementMode::Run,
			ESoccerAIMovementReason::NearbyReposition
		);
	}

	ASoccerCharacterBase* PossessingCharacter =
		MatchManager->GetPossessingCharacter();

	if (IsValid(PossessingCharacter))
	{
		SetFocus(PossessingCharacter);
	}
	else
	{
		ASoccerBall* SoccerBall = MatchManager->GetSoccerBall();

		if (IsValid(SoccerBall))
		{
			SetFocus(SoccerBall);
		}
	}

	return true;
}

bool ASoccerAIController::TryStealBallIfClose(
	ASoccerAICharacter* SoccerCharacter,
	ASoccerBall* SoccerBall
)
{
	if (!IsValid(SoccerCharacter) || !IsValid(SoccerBall))
	{
		return false;
	}

	if (!IsValid(MatchManager))
	{
		return false;
	}

	if (SoccerCharacter->IsAIStealRecoveryActive(AILostBallStealRecoveryTime))
	{
		return false;
	}

	ASoccerCharacterBase* PossessingCharacter =
		MatchManager->GetPossessingCharacter();

	if (!IsValid(PossessingCharacter))
	{
		return false;
	}

	if (PossessingCharacter == SoccerCharacter)
	{
		return false;
	}

	if (PossessingCharacter->GetTeam() == SoccerCharacter->GetTeam())
	{
		return false;
	}

	if (!MatchManager->CanCharacterTouchBallNow(SoccerCharacter))
	{
		return false;
	}

	const float DistanceToBall = FVector::Dist2D(
		SoccerCharacter->GetActorLocation(),
		SoccerBall->GetActorLocation()
	);

	if (
		!MatchManager->ShouldDefensivePressureAttemptSteal(
			SoccerCharacter,
			PossessingCharacter,
			SoccerBall
		)
		)
	{
		return false;
	}

	// Caso 1: bot roba a otro bot.
	ASoccerAICharacter* PossessingAICharacter =
		Cast<ASoccerAICharacter>(PossessingCharacter);

	if (IsValid(PossessingAICharacter))
	{
		// La pelota en las manos del arquero no es una posesi�n
		// ordinaria: no puede ser robada bajo ninguna probabilidad.
		if (PossessingAICharacter->IsGoalkeeperHoldingBall())
		{
			return false;
		}

		if (PossessingAICharacter->IsAIPossessionProtected(AIPossessionProtectionTime))
		{
			return false;
		}

		if (DistanceToBall > AIBallStealDistance)
		{
			return false;
		}

		if (
			!CanStealPossessedBall(
				SoccerCharacter,
				PossessingAICharacter,
				SoccerBall
			)
			)
		{
			return false;
		}
		/*
		if (
			IsBallShieldedByPossessor(
				SoccerCharacter,
				PossessingAICharacter,
				SoccerBall
			)
			)
		{
			return false;
		}
		*/
		return TryExecuteCurrentRecoveryIntentAtDefensiveContact(
			SoccerCharacter,
			PossessingAICharacter,
			SoccerBall
		);
	}

	// Caso 2: bot roba al jugador humano.
	AThirdPersonCppCharacter* PossessingHumanCharacter =
		Cast<AThirdPersonCppCharacter>(PossessingCharacter);

	if (IsValid(PossessingHumanCharacter))
	{
		if (!PossessingHumanCharacter->IsPossessingBall())
		{
			return false;
		}

		if (DistanceToBall > AIBallStealFromHumanDistance)
		{
			return false;
		}

		if (
			!CanStealPossessedBall(
				SoccerCharacter,
				PossessingHumanCharacter,
				SoccerBall
			)
			)
		{
			return false;
		}
		/*
		if (
			IsBallShieldedByPossessor(
				SoccerCharacter,
				PossessingHumanCharacter,
				SoccerBall
			)
			)
		{
			return false;
		}
		*/
		const float CurrentTime =
			GetWorld() != nullptr
			? GetWorld()->GetTimeSeconds()
			: 0.0f;

		if (
			CurrentTime - LastHumanStealAttemptTime <
			AIBallStealFromHumanCooldown
			)
		{
			return false;
		}

		LastHumanStealAttemptTime = CurrentTime;

		const bool bStealSucceeded =
			FMath::FRand() <= AIBallStealFromHumanChance;

		if (!bStealSucceeded)
		{
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(
					-1,
					0.8f,
					FColor::Silver,
					TEXT("El rival intento robar y fallo")
				);
			}

			return false;
		}

		if (!TryExecuteCurrentRecoveryIntentAtDefensiveContact(
			SoccerCharacter,
			PossessingHumanCharacter,
			SoccerBall
		))
		{
			return false;
		}

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				1.0f,
				FColor::Red,
				TEXT("El rival te robo la pelota")
			);
		}

		return true;
	}

	return false;
}

bool ASoccerAIController::TryShootBallIfClose(
	ASoccerAICharacter* SoccerCharacter
)
{
	if (!IsValid(SoccerCharacter))
	{
		return false;
	}

	if (!SoccerCharacter->IsAIPossessingBall())
	{
		return false;
	}

	if (!IsValid(MatchManager))
	{
		return false;
	}

	const FVector ShotCenterLocation =
		MatchManager->GetShotTargetLocation(SoccerCharacter);

	const float DistanceToShotTarget = FVector::Dist2D(
		SoccerCharacter->GetActorLocation(),
		ShotCenterLocation
	);

	if (DistanceToShotTarget > AIShotDistanceToTarget)
	{
		return false;
	}

	const FVector IntendedShotTargetLocation =
		BuildAIShotTargetLocation(SoccerCharacter);

	if (IntendedShotTargetLocation.IsNearlyZero())
	{
		return false;
	}

	if (
		TryPrepareMainActionIfBlocked(
			SoccerCharacter,
			ESoccerAIPendingMainAction::Shoot,
			IntendedShotTargetLocation,
			nullptr
		)
		)
	{
		return true;
	}

	const FVector ShotTargetLocation =
		ApplyAIShotExecutionError(
			SoccerCharacter,
			IntendedShotTargetLocation
		);

	if (!TryRegisterAIKickTouchForRules(SoccerCharacter))
	{
		return true;
	}

	SoccerCharacter->KickAIBallToAirTarget(
		ShotTargetLocation,
		AIShotHorizontalSpeed,
		AIShotMinTravelTime,
		AIShotMaxTravelTime
	);

	if (SoccerCharacter->IsAIKickMontageActive())
	{
		StopMovement();
	}

	return true;
}

bool ASoccerAIController::UpdateInitialPossessionEscape(
	ASoccerAICharacter* SoccerCharacter,
	float DeltaTime
)
{
	(void)DeltaTime;

	if (!bUseInitialPossessionEscape)
	{
		ClearInitialPossessionEscape();
		return false;
	}

	if (!IsValid(SoccerCharacter))
	{
		ClearInitialPossessionEscape();
		return false;
	}

	if (!SoccerCharacter->IsAIPossessingBall())
	{
		ClearInitialPossessionEscape();
		return false;
	}

	if (!bInitialPossessionEscapeStartedForCurrentPossession)
	{
		TryStartInitialPossessionEscape(SoccerCharacter);
	}

	if (!bInitialPossessionEscapeActive)
	{
		return false;
	}

	const float CurrentTime =
		GetWorld() != nullptr
		? GetWorld()->GetTimeSeconds()
		: 0.0f;

	const float EscapeElapsedTime =
		CurrentTime - InitialPossessionEscapeStartTime;

	if (EscapeElapsedTime >= InitialPossessionEscapeDuration)
	{
		bInitialPossessionEscapeActive = false;
		SoccerCharacter->SetAIPossessionCarryActive(false);
		return false;
	}

	const float DistanceToEscapeLocation = FVector::Dist2D(
		SoccerCharacter->GetActorLocation(),
		InitialPossessionEscapeLocation
	);

	if (DistanceToEscapeLocation <= InitialPossessionEscapeAcceptanceRadius)
	{
		bInitialPossessionEscapeActive = false;
		SoccerCharacter->SetAIPossessionCarryActive(false);
		return false;
	}

	// Escape used to drag a kinematic ball in front of the runner. It is now a
	// real short touch followed by the existing physical auto-pass pursuit.
	if (!TryRegisterAIKickTouchForRules(SoccerCharacter))
	{
		return true;
	}

	SoccerCharacter->StartAIAutoPassToLocation(
		InitialPossessionEscapeLocation,
		AIAutoPassHorizontalSpeed,
		AIAutoPassMinTravelTime,
		AIAutoPassMaxTravelTime
	);

	MoveToLocationWithAIMovement(
		ESoccerAIOrder::AttackRunIntoSpace,
		InitialPossessionEscapeLocation,
		AIAutoPassFollowAcceptanceRadius,
		false
	);

	bInitialPossessionEscapeActive = false;
	return true;
}

bool ASoccerAIController::TryStartInitialPossessionEscape(
	ASoccerAICharacter* SoccerCharacter
)
{
	bInitialPossessionEscapeStartedForCurrentPossession = true;

	if (!IsValid(SoccerCharacter))
	{
		return false;
	}

	ASoccerCharacterBase* ClosestOpponent =
		FindClosestOpponentPressure(
			SoccerCharacter,
			InitialPossessionEscapePressureRadius
		);

	if (!IsValid(ClosestOpponent))
	{
		return false;
	}

	InitialPossessionEscapeLocation =
		BuildInitialEscapeLocation(
			SoccerCharacter,
			ClosestOpponent
		);

	if (InitialPossessionEscapeLocation.IsNearlyZero())
	{
		return false;
	}

	const float EscapeDistance = FVector::Dist2D(
		SoccerCharacter->GetActorLocation(),
		InitialPossessionEscapeLocation
	);

	if (EscapeDistance <= InitialPossessionEscapeAcceptanceRadius)
	{
		return false;
	}

	InitialPossessionEscapeStartTime =
		GetWorld() != nullptr
		? GetWorld()->GetTimeSeconds()
		: 0.0f;

	bInitialPossessionEscapeActive = true;

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			0.8f,
			FColor::Cyan,
			TEXT("AI: salida inicial para escapar de presion")
		);
	}

	return true;
}

ASoccerCharacterBase* ASoccerAIController::FindClosestOpponentPressure(
	const ASoccerAICharacter* SoccerCharacter,
	float MaxDistance
) const
{
	if (!IsValid(SoccerCharacter))
	{
		return nullptr;
	}

	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return nullptr;
	}

	ASoccerCharacterBase* ClosestOpponent = nullptr;
	float ClosestDistance = MaxDistance;

	for (TActorIterator<ASoccerCharacterBase> It(World); It; ++It)
	{
		ASoccerCharacterBase* Candidate = *It;

		if (!IsValid(Candidate))
		{
			continue;
		}

		if (Candidate == SoccerCharacter)
		{
			continue;
		}

		if (Candidate->GetTeam() == SoccerCharacter->GetTeam())
		{
			continue;
		}

		const float DistanceToCandidate = FVector::Dist2D(
			SoccerCharacter->GetActorLocation(),
			Candidate->GetActorLocation()
		);

		if (DistanceToCandidate < ClosestDistance)
		{
			ClosestDistance = DistanceToCandidate;
			ClosestOpponent = Candidate;
		}
	}

	return ClosestOpponent;
}

FVector ASoccerAIController::BuildInitialEscapeLocation(
	const ASoccerAICharacter* SoccerCharacter,
	const ASoccerCharacterBase* ClosestOpponent
) const
{
	if (!IsValid(SoccerCharacter) || !IsValid(ClosestOpponent))
	{
		return FVector::ZeroVector;
	}

	FVector AwayFromOpponent =
		SoccerCharacter->GetActorLocation() - ClosestOpponent->GetActorLocation();

	AwayFromOpponent.Z = 0.0f;
	AwayFromOpponent = AwayFromOpponent.GetSafeNormal();

	if (AwayFromOpponent.IsNearlyZero())
	{
		AwayFromOpponent = SoccerCharacter->GetActorForwardVector();
		AwayFromOpponent.Z = 0.0f;
		AwayFromOpponent = AwayFromOpponent.GetSafeNormal();
	}

	FVector SideDirection = SoccerCharacter->GetActorRightVector();
	SideDirection.Z = 0.0f;
	SideDirection = SideDirection.GetSafeNormal();

	if (FMath::RandBool())
	{
		SideDirection *= -1.0f;
	}

	FVector EscapeDirection =
		AwayFromOpponent + SideDirection * InitialPossessionEscapeSideWeight;

	EscapeDirection.Z = 0.0f;
	EscapeDirection = EscapeDirection.GetSafeNormal();

	if (EscapeDirection.IsNearlyZero())
	{
		EscapeDirection = AwayFromOpponent;
	}

	FVector DesiredEscapeLocation =
		SoccerCharacter->GetActorLocation()
		+ EscapeDirection * InitialPossessionEscapeDistance;

	DesiredEscapeLocation.Z = SoccerCharacter->GetActorLocation().Z;

	UWorld* World = GetWorld();

	if (World != nullptr)
	{
		UNavigationSystemV1* NavigationSystem =
			FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);

		if (NavigationSystem != nullptr)
		{
			FNavLocation ProjectedLocation;

			const bool bProjected =
				NavigationSystem->ProjectPointToNavigation(
					DesiredEscapeLocation,
					ProjectedLocation,
					FVector(350.0f, 350.0f, 350.0f)
				);

			if (bProjected)
			{
				DesiredEscapeLocation = ProjectedLocation.Location;
			}
		}
	}

	return DesiredEscapeLocation;
}

void ASoccerAIController::ClearInitialPossessionEscape()
{
	ASoccerAICharacter* SoccerCharacter =
		Cast<ASoccerAICharacter>(GetPawn());

	if (IsValid(SoccerCharacter))
	{
		SoccerCharacter->SetAIPossessionCarryActive(false);
	}

	bInitialPossessionEscapeActive = false;
	bInitialPossessionEscapeStartedForCurrentPossession = false;
	InitialPossessionEscapeLocation = FVector::ZeroVector;
	InitialPossessionEscapeStartTime = 0.0f;
}

bool ASoccerAIController::UpdateAIAutoPassFollow(
	ASoccerAICharacter* SoccerCharacter
)
{
	if (!IsValid(SoccerCharacter))
	{
		return false;
	}

	if (!SoccerCharacter->IsAIAutoPassActive())
	{
		return false;
	}

	ASoccerBall* SoccerBall = SoccerCharacter->GetAIAutoPassBall();

	if (!IsValid(SoccerBall))
	{
		SoccerCharacter->ClearAIAutoPassState();
		return false;
	}

	if (IsValid(MatchManager))
	{
		ASoccerCharacterBase* PossessingCharacter =
			MatchManager->GetPossessingCharacter();

		if (
			IsValid(PossessingCharacter) &&
			PossessingCharacter != SoccerCharacter
			)
		{
			SoccerCharacter->ClearAIAutoPassState();
			ClearPendingMainActionAfterPreparation();
			ClearCurrentRecoveryIntent();
			return false;
		}
	}

	if (!IsAIAutoPassStillValid(SoccerCharacter, SoccerBall))
	{
		SoccerCharacter->ClearAIAutoPassState();
		ClearPendingMainActionAfterPreparation();
		ClearCurrentRecoveryIntent();
		return false;
	}

	SoccerCharacter->SetAIChasingBall(true);

	SoccerCharacter->RequestAIMovementMode(
		ESoccerAIMovementMode::FastRun,
		ESoccerAIMovementReason::ChaseOwnAutoPass,
		true
	);

	if (SoccerCharacter->TryCollectAIAutoPassIfClose(AIAutoPassCollectDistance))
	{
		BeginOffensiveDecisionEpisode(SoccerCharacter, true);

		if (IsValid(SoccerBall))
		{
			SetFocus(SoccerBall);
		}

		// No cortamos el Tick.
		// Ya recuper� la pelota; devolvemos false para que el Tick contin�e
		// y pueda ejecutar intenci�n, preparaci�n pendiente o decisi�n por zona.
		return false;
	}

	FVector MoveLocation =
		BuildAIAutoPassFollowMoveLocation(
			SoccerCharacter,
			SoccerBall
		);

	if (MoveLocation.IsNearlyZero())
	{
		MoveLocation = SoccerBall->GetActorLocation();
	}

	MoveLocation.Z = SoccerCharacter->GetActorLocation().Z;

	MoveToLocationWithAIMovement(
		ESoccerAIOrder::AttackRunIntoSpace,
		MoveLocation,
		AIAutoPassFollowAcceptanceRadius,
		false
	);

	SetFocalPoint(
		MoveLocation,
		EAIFocusPriority::Gameplay
	);

	return true;
}

bool ASoccerAIController::TryStartAIAutoPass(
	ASoccerAICharacter* SoccerCharacter,
	bool* bOutRejectedByLocalSafety
)
{
	if (bOutRejectedByLocalSafety != nullptr)
	{
		*bOutRejectedByLocalSafety = false;
	}

	if (!bUseAIAutoPass)
	{
		return false;
	}

	if (!IsValid(SoccerCharacter))
	{
		return false;
	}

	if (!SoccerCharacter->IsAIPossessingBall())
	{
		return false;
	}

	if (!IsValid(MatchManager))
	{
		return false;
	}

	const bool bConservativeAction =
		MatchManager->ShouldBallCarrierPreferConservativeAction(
			SoccerCharacter
		);

	// Goalkeeper distribution has its own decision system. Preserve the old
	// conservative veto so the local dribble layer can never steal authority.
	if (
		bConservativeAction &&
		SoccerCharacter->GetPlayerRole() == ESoccerPlayerRole::Goalkeeper
		)
	{
		return false;
	}

	bool bUsedLocalDribbleDirection = false;

	const FVector AutoPassTargetLocation =
		BuildAIAutoPassTargetLocation(
			SoccerCharacter,
			&bUsedLocalDribbleDirection,
			bOutRejectedByLocalSafety
		);

	if (AutoPassTargetLocation.IsNearlyZero())
	{
		return false;
	}

	// If local direction evaluation is disabled/unavailable, keep the previous
	// team-discipline rule exactly as before. A conservative carrier only gains
	// a dribble outlet when the grid explicitly found a safe lateral/back route.
	if (bConservativeAction && !bUsedLocalDribbleDirection)
	{
		return false;
	}

	// The old preparation system also tries to dodge blockers laterally. Once
	// the grid already selected a safe local direction, running both systems
	// would make them compete. Passes and shots still use preparation normally.
	if (
		!bUsedLocalDribbleDirection &&
		TryPrepareMainActionIfBlocked(
			SoccerCharacter,
			ESoccerAIPendingMainAction::AutoPass,
			AutoPassTargetLocation,
			nullptr
		)
		)
	{
		return true;
	}

	if (!TryRegisterAIKickTouchForRules(SoccerCharacter))
	{
		return true;
	}

	SoccerCharacter->StartAIAutoPassToLocation(
		AutoPassTargetLocation,
		AIAutoPassHorizontalSpeed,
		AIAutoPassMinTravelTime,
		AIAutoPassMaxTravelTime
	);

	MoveToLocationWithAIMovement(
		ESoccerAIOrder::AttackRunIntoSpace,
		AutoPassTargetLocation,
		AIAutoPassFollowAcceptanceRadius,
		false
	);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			0.8f,
			FColor::Cyan,
			bUsedLocalDribbleDirection
				? TEXT("AI autopase: direccion local")
				: TEXT("AI autopase")
		);
	}

	return true;
}

FVector ASoccerAIController::BuildAIAutoPassTargetLocation(
	const ASoccerAICharacter* SoccerCharacter,
	bool* bOutUsedLocalDribbleDirection,
	bool* bOutRejectedByLocalSafety
) const
{
	if (bOutUsedLocalDribbleDirection != nullptr)
	{
		*bOutUsedLocalDribbleDirection = false;
	}

	if (bOutRejectedByLocalSafety != nullptr)
	{
		*bOutRejectedByLocalSafety = false;
	}

	if (!IsValid(SoccerCharacter) || !IsValid(MatchManager))
	{
		return FVector::ZeroVector;
	}

	const FVector AttackMoveLocation =
		MatchManager->GetOpenPlayCarryIntentTargetLocation(SoccerCharacter);

	FVector PreferredDirection =
		AttackMoveLocation - SoccerCharacter->GetActorLocation();

	PreferredDirection.Z = 0.0f;

	const float DistanceToAttack = PreferredDirection.Size();

	if (DistanceToAttack < 50.0f)
	{
		return FVector::ZeroVector;
	}

	PreferredDirection = PreferredDirection.GetSafeNormal();

	if (PreferredDirection.IsNearlyZero())
	{
		return FVector::ZeroVector;
	}

	FVector DirectionToUse = PreferredDirection;

	if (
		bUseAILocalDribbleDirection &&
		SoccerCharacter->IsAIPossessingBall() &&
		SoccerCharacter->GetPlayerRole() != ESoccerPlayerRole::Goalkeeper
		)
	{
		FVector LocalDirection = FVector::ZeroVector;
		bool bLocalDirectionIsSafe = false;

		const bool bConservativeMode =
			MatchManager->ShouldBallCarrierPreferConservativeAction(
				SoccerCharacter
			);

		const bool bEvaluatedLocalGrid =
			FindBestLocalAIDribbleDirection(
				SoccerCharacter,
				PreferredDirection,
				bConservativeMode,
				LocalDirection,
				bLocalDirectionIsSafe
			);

		if (bEvaluatedLocalGrid)
		{
			// An evaluated-but-unsafe 3 x 3 neighborhood means the dribble
			// should not be forced. The caller may pass, protect or use its
			// existing possession fallback instead.
			if (!bLocalDirectionIsSafe || LocalDirection.IsNearlyZero())
			{
				if (bOutRejectedByLocalSafety != nullptr)
				{
					*bOutRejectedByLocalSafety = true;
				}

				return FVector::ZeroVector;
			}

			DirectionToUse = LocalDirection;

			if (bOutUsedLocalDribbleDirection != nullptr)
			{
				*bOutUsedLocalDribbleDirection = true;
			}
		}
	}

	const float PassDistance =
		FMath::Min(AIAutoPassDistance, DistanceToAttack);

	FVector TargetLocation =
		SoccerCharacter->GetActorLocation()
		+ DirectionToUse * PassDistance;

	TargetLocation.Z = SoccerCharacter->GetActorLocation().Z;

	return TargetLocation;
}

bool ASoccerAIController::FindBestLocalAIDribbleDirection(
	const ASoccerAICharacter* SoccerCharacter,
	const FVector& PreferredDirection,
	bool bConservativeMode,
	FVector& OutBestDirection,
	bool& bOutDirectionIsSafe
) const
{
	OutBestDirection = FVector::ZeroVector;
	bOutDirectionIsSafe = false;

	if (!IsValid(SoccerCharacter))
	{
		return false;
	}

	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return false;
	}

	FVector SafePreferredDirection = PreferredDirection;
	SafePreferredDirection.Z = 0.0f;
	SafePreferredDirection = SafePreferredDirection.GetSafeNormal();

	if (SafePreferredDirection.IsNearlyZero())
	{
		return false;
	}

	FVector BallLocation = SoccerCharacter->GetActorLocation();

	if (IsValid(MatchManager))
	{
		ASoccerBall* SoccerBall = MatchManager->GetSoccerBall();

		if (IsValid(SoccerBall))
		{
			BallLocation = SoccerBall->GetActorLocation();
		}
	}

	BallLocation.Z = SoccerCharacter->GetActorLocation().Z;

	constexpr int32 GridColumns = 15;
	constexpr int32 GridRows = 10;

	const float CellSizeX =
		SoccerFieldDimensions::PitchLengthCm /
		static_cast<float>(GridColumns);

	const float CellSizeY =
		SoccerFieldDimensions::PitchWidthCm /
		static_cast<float>(GridRows);

	const float GridMinX = SoccerFieldDimensions::LeftGoalLineX;
	const float GridMinY = SoccerFieldDimensions::SouthTouchLineY;

	const ASoccerField* SoccerField = IsValid(MatchManager)
		? MatchManager->GetSoccerField()
		: nullptr;
	const FVector BallPitchLocation = IsValid(SoccerField)
		? SoccerField->WorldToPitchLocal(BallLocation)
		: BallLocation;
	const FVector PitchLengthDirection = IsValid(SoccerField)
		? SoccerField->GetPitchLengthWorldDirection()
		: FVector::ForwardVector;
	const FVector PitchWidthDirection = IsValid(SoccerField)
		? SoccerField->GetPitchWidthWorldDirection()
		: FVector::RightVector;

	const float LocalX = (BallPitchLocation.X - GridMinX) / CellSizeX;
	const float LocalY = (BallPitchLocation.Y - GridMinY) / CellSizeY;

	// The selector is for open-play ball control. If the ball is genuinely
	// outside the pitch, leave restart/out-of-play systems in charge.
	if (
		LocalX < -0.05f ||
		LocalX > static_cast<float>(GridColumns) + 0.05f ||
		LocalY < -0.05f ||
		LocalY > static_cast<float>(GridRows) + 0.05f
		)
	{
		return false;
	}

	const int32 CurrentCellX =
		FMath::Clamp(
			FMath::FloorToInt(LocalX),
			0,
			GridColumns - 1
		);

	const int32 CurrentCellY =
		FMath::Clamp(
			FMath::FloorToInt(LocalY),
			0,
			GridRows - 1
		);

	TArray<ASoccerCharacterBase*> NearbyPlayers;
	NearbyPlayers.Reserve(10);

	const float NearbyRadius =
		FMath::Max(400.0f, AILocalDribbleNearbyPlayerRadius);

	for (TActorIterator<ASoccerCharacterBase> It(World); It; ++It)
	{
		ASoccerCharacterBase* CandidatePlayer = *It;

		if (!IsValid(CandidatePlayer) || CandidatePlayer == SoccerCharacter)
		{
			continue;
		}

		if (
			FVector::Dist2D(
				CandidatePlayer->GetActorLocation(),
				BallLocation
			) > NearbyRadius
			)
		{
			continue;
		}

		NearbyPlayers.Add(CandidatePlayer);
	}

	struct FLocalDribbleCandidate
	{
		FVector Direction = FVector::ZeroVector;
		FVector ProbeLocation = FVector::ZeroVector;
		float Alignment = -1.0f;
		float SafetyMargin = -BIG_NUMBER;
		float SafeSelectionScore = -BIG_NUMBER;
		float FallbackScore = -BIG_NUMBER;
		bool bSafe = false;
		bool bOpponentLaneBlocked = false;
		bool bTeammateCongested = false;
	};

	TArray<FLocalDribbleCandidate> EvaluatedCandidates;
	EvaluatedCandidates.Reserve(8);

	float BestSafeScore = -BIG_NUMBER;
	float BestFallbackScore = -BIG_NUMBER;
	FVector BestSafeDirection = FVector::ZeroVector;
	FVector BestFallbackDirection = FVector::ZeroVector;

	for (int32 DeltaX = -1; DeltaX <= 1; ++DeltaX)
	{
		for (int32 DeltaY = -1; DeltaY <= 1; ++DeltaY)
		{
			if (DeltaX == 0 && DeltaY == 0)
			{
				continue;
			}

			const int32 CandidateCellX = CurrentCellX + DeltaX;
			const int32 CandidateCellY = CurrentCellY + DeltaY;

			if (
				CandidateCellX < 0 ||
				CandidateCellX >= GridColumns ||
				CandidateCellY < 0 ||
				CandidateCellY >= GridRows
				)
			{
				continue;
			}

			FLocalDribbleCandidate Candidate;

			const FVector ProbePitchLocation(
				GridMinX + (static_cast<float>(CandidateCellX) + 0.5f) * CellSizeX,
				GridMinY + (static_cast<float>(CandidateCellY) + 0.5f) * CellSizeY,
				0.0f
			);
			Candidate.ProbeLocation = IsValid(SoccerField)
				? SoccerField->PitchLocalToWorld(ProbePitchLocation)
				: ProbePitchLocation;
			Candidate.ProbeLocation.Z = SoccerCharacter->GetActorLocation().Z;

			// The cell center is only an ETA probe. The real dribble direction
			// comes from the neighbor offset, so the auto-pass never tries to
			// "snap" the ball toward grid centers.
			Candidate.Direction = (
				PitchLengthDirection * (static_cast<float>(DeltaX) * CellSizeX) +
				PitchWidthDirection * (static_cast<float>(DeltaY) * CellSizeY)
			).GetSafeNormal();

			if (Candidate.Direction.IsNearlyZero())
			{
				continue;
			}

			Candidate.Alignment =
				FVector::DotProduct(
					Candidate.Direction,
					SafePreferredDirection
				);

			if (
				bConservativeMode &&
				Candidate.Alignment > AILocalDribbleConservativeMaxAlignment
				)
			{
				continue;
			}

			const float BallArrivalTime =
				EstimateLocalAIDribbleBallArrivalTime(
					BallLocation,
					Candidate.ProbeLocation
				);

			const float CarrierArrivalTime =
				SoccerCharacter->EstimateArrivalTimeToLocation(
					Candidate.ProbeLocation,
					AILocalDribbleArrivalReachRadius
				);

			const float CarrierControlTime =
				FMath::Max(BallArrivalTime, CarrierArrivalTime);

			float FastestOpponentControlTime = BIG_NUMBER;
			float FastestTeammateControlTime = BIG_NUMBER;
			bool bTeammateLaneBlocked = false;

			for (ASoccerCharacterBase* NearbyPlayer : NearbyPlayers)
			{
				if (!IsValid(NearbyPlayer))
				{
					continue;
				}

				const float PlayerArrivalTime =
					NearbyPlayer->EstimateArrivalTimeToLocation(
						Candidate.ProbeLocation,
						AILocalDribbleArrivalReachRadius
					);

				const float PlayerControlTime =
					FMath::Max(BallArrivalTime, PlayerArrivalTime);

				FVector PlayerLocation = NearbyPlayer->GetActorLocation();
				PlayerLocation.Z = BallLocation.Z;

				const FVector ClosestPointOnProbeLane =
					FMath::ClosestPointOnSegment(
						PlayerLocation,
						BallLocation,
						Candidate.ProbeLocation
					);

				FVector BallToPlayer = PlayerLocation - BallLocation;
				BallToPlayer.Z = 0.0f;

				const float PlayerForwardProjection =
					FVector::DotProduct(BallToPlayer, Candidate.Direction);

				const bool bPlayerBlocksProbeLane =
					PlayerForwardProjection >= 45.0f &&
					FVector::Dist2D(
						PlayerLocation,
						ClosestPointOnProbeLane
					) <= FMath::Max(0.0f, AILocalDribbleLaneHalfWidth);

				if (NearbyPlayer->GetTeam() == SoccerCharacter->GetTeam())
				{
					bTeammateLaneBlocked =
						bTeammateLaneBlocked || bPlayerBlocksProbeLane;
					FastestTeammateControlTime =
						FMath::Min(
							FastestTeammateControlTime,
							PlayerControlTime
						);
				}
				else
				{
					Candidate.bOpponentLaneBlocked =
						Candidate.bOpponentLaneBlocked || bPlayerBlocksProbeLane;

					FastestOpponentControlTime =
						FMath::Min(
							FastestOpponentControlTime,
							PlayerControlTime
						);
				}
			}

			Candidate.SafetyMargin =
				FastestOpponentControlTime < BIG_NUMBER * 0.5f
				? FastestOpponentControlTime - CarrierControlTime
				: 2.0f;

			Candidate.bTeammateCongested =
				bTeammateLaneBlocked ||
				(
					FastestTeammateControlTime < BIG_NUMBER * 0.5f &&
					FastestTeammateControlTime <=
						CarrierControlTime + AILocalDribbleTeammateCongestionTime
				);

			Candidate.bSafe =
				Candidate.SafetyMargin >= AILocalDribbleMinimumSafetyMargin &&
				!Candidate.bOpponentLaneBlocked &&
				!Candidate.bTeammateCongested;

			int32 DirectionTier = 0;

			if (Candidate.Alignment >= 0.80f)
			{
				DirectionTier = 4;
			}
			else if (Candidate.Alignment >= 0.25f)
			{
				DirectionTier = 3;
			}
			else if (Candidate.Alignment >= -0.25f)
			{
				DirectionTier = 2;
			}
			else if (Candidate.Alignment >= -0.80f)
			{
				DirectionTier = 1;
			}

			const float ClampedSafetyMargin =
				FMath::Clamp(Candidate.SafetyMargin, -2.0f, 2.0f);

			Candidate.SafeSelectionScore =
				static_cast<float>(DirectionTier) * 100.0f +
				Candidate.Alignment * 10.0f +
				ClampedSafetyMargin * 4.0f;

			Candidate.FallbackScore =
				ClampedSafetyMargin * 100.0f +
				Candidate.Alignment * 8.0f -
				(Candidate.bOpponentLaneBlocked ? 80.0f : 0.0f) -
				(Candidate.bTeammateCongested ? 60.0f : 0.0f);

			if (Candidate.bSafe && Candidate.SafeSelectionScore > BestSafeScore)
			{
				BestSafeScore = Candidate.SafeSelectionScore;
				BestSafeDirection = Candidate.Direction;
			}

			if (Candidate.FallbackScore > BestFallbackScore)
			{
				BestFallbackScore = Candidate.FallbackScore;
				BestFallbackDirection = Candidate.Direction;
			}

			EvaluatedCandidates.Add(Candidate);
		}
	}

	if (EvaluatedCandidates.Num() == 0)
	{
		return false;
	}

	if (!BestSafeDirection.IsNearlyZero())
	{
		OutBestDirection = BestSafeDirection;
		bOutDirectionIsSafe = true;
	}
	else
	{
		// This direction is useful to the existing stuck failsafe, which may
		// choose the least dangerous escape when every immediate option is bad.
		OutBestDirection = BestFallbackDirection;
		bOutDirectionIsSafe = false;
	}

	if (
		ASoccerDebugManager::IsWorldDrawingEnabled(
			this,
			ESoccerDebugCategory::AI
		)
		)
	{
		const int32 DebugMinCellX = FMath::Max(0, CurrentCellX - 1);
		const int32 DebugMaxCellX = FMath::Min(GridColumns - 1, CurrentCellX + 1);
		const int32 DebugMinCellY = FMath::Max(0, CurrentCellY - 1);
		const int32 DebugMaxCellY = FMath::Min(GridRows - 1, CurrentCellY + 1);

		const float DebugMinX = GridMinX + DebugMinCellX * CellSizeX;
		const float DebugMaxX = GridMinX + (DebugMaxCellX + 1) * CellSizeX;
		const float DebugMinY = GridMinY + DebugMinCellY * CellSizeY;
		const float DebugMaxY = GridMinY + (DebugMaxCellY + 1) * CellSizeY;

		const UCapsuleComponent* DebugCapsule = SoccerCharacter->GetCapsuleComponent();
		const float DebugGroundOffset =
			DebugCapsule != nullptr
			? DebugCapsule->GetScaledCapsuleHalfHeight()
			: 0.0f;

		const float DebugZ =
			SoccerCharacter->GetActorLocation().Z - DebugGroundOffset + 12.0f;

		auto PitchDebugPointToWorld = [&](float LocalX, float LocalY)
		{
			FVector Point = IsValid(SoccerField)
				? SoccerField->PitchLocalToWorld(FVector(LocalX, LocalY, 0.0f))
				: FVector(LocalX, LocalY, 0.0f);
			Point.Z = DebugZ;
			return Point;
		};

		for (int32 GridX = DebugMinCellX; GridX <= DebugMaxCellX + 1; ++GridX)
		{
			const float LineX = GridMinX + GridX * CellSizeX;

			ASoccerDebugManager::DrawLine(
				this,
				ESoccerDebugCategory::AI,
				PitchDebugPointToWorld(LineX, DebugMinY),
				PitchDebugPointToWorld(LineX, DebugMaxY),
				FColor(80, 80, 80),
				0.65f,
				1.0f
			);
		}

		for (int32 GridY = DebugMinCellY; GridY <= DebugMaxCellY + 1; ++GridY)
		{
			const float LineY = GridMinY + GridY * CellSizeY;

			ASoccerDebugManager::DrawLine(
				this,
				ESoccerDebugCategory::AI,
				PitchDebugPointToWorld(DebugMinX, LineY),
				PitchDebugPointToWorld(DebugMaxX, LineY),
				FColor(80, 80, 80),
				0.65f,
				1.0f
			);
		}

		for (const FLocalDribbleCandidate& Candidate : EvaluatedCandidates)
		{
			FVector DebugProbeLocation = Candidate.ProbeLocation;
			DebugProbeLocation.Z = DebugZ;

			const FColor CandidateColor =
				Candidate.bSafe
				? FColor::Green
				: Candidate.bTeammateCongested
				? FColor::Yellow
				: FColor::Red;

			ASoccerDebugManager::DrawSphere(
				this,
				ESoccerDebugCategory::AI,
				DebugProbeLocation,
				28.0f,
				CandidateColor,
				0.65f,
				10,
				1.5f
			);
		}

		if (!OutBestDirection.IsNearlyZero())
		{
			FVector DebugDirectionStart = BallLocation;
			DebugDirectionStart.Z = DebugZ + 8.0f;

			ASoccerDebugManager::DrawLine(
				this,
				ESoccerDebugCategory::AI,
				DebugDirectionStart,
				DebugDirectionStart + OutBestDirection * 520.0f,
				bOutDirectionIsSafe ? FColor::Cyan : FColor::Orange,
				0.65f,
				4.0f
			);
		}
	}

	return !OutBestDirection.IsNearlyZero();
}

float ASoccerAIController::EstimateLocalAIDribbleBallArrivalTime(
	const FVector& BallLocation,
	const FVector& EvaluationLocation
) const
{
	const float Distance2D =
		FVector::Dist2D(BallLocation, EvaluationLocation);

	if (Distance2D <= KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	const float SafeRequestedSpeed =
		FMath::Max(100.0f, AIAutoPassHorizontalSpeed);

	return FMath::Clamp(
		Distance2D / SafeRequestedSpeed,
		AIAutoPassMinTravelTime,
		AIAutoPassMaxTravelTime
	);
}

void ASoccerAIController::UpdateAIPossessionStuckTracking(
	ASoccerAICharacter* SoccerCharacter
)
{
	if (!IsValid(SoccerCharacter))
	{
		ClearAIPossessionStuckTracking();
		return;
	}

	if (!SoccerCharacter->IsAIPossessingBall())
	{
		ClearAIPossessionStuckTracking();
		return;
	}

	const float CurrentTime =
		GetWorld() != nullptr
		? GetWorld()->GetTimeSeconds()
		: 0.0f;

	if (!bAIPossessionStuckTrackingActive)
	{
		bAIPossessionStuckTrackingActive = true;
		LastAIPossessionMovementTime = CurrentTime;
		return;
	}

	const float Speed2D = SoccerCharacter->GetVelocity().Size2D();

	if (Speed2D >= AIPossessionStuckMinSpeed)
	{
		LastAIPossessionMovementTime = CurrentTime;
	}
}

bool ASoccerAIController::TryForceAIPossessionActionIfStuck(
	ASoccerAICharacter* SoccerCharacter
)
{
	if (!bUseAIPossessionFailsafe)
	{
		return false;
	}

	if (!IsValid(SoccerCharacter))
	{
		return false;
	}

	if (!SoccerCharacter->IsAIPossessingBall())
	{
		return false;
	}

	const float CurrentTime =
		GetWorld() != nullptr
		? GetWorld()->GetTimeSeconds()
		: 0.0f;

	if (
		CurrentTime - LastAIPossessionMovementTime <
		AIPossessionStuckSeconds
		)
	{
		return false;
	}

	if (
		CurrentTime - LastAIPossessionForcedActionTime <
		AIPossessionStuckForceCooldown
		)
	{
		return false;
	}

	const FVector ForcedAutoPassTarget =
		BuildAIStuckAutoPassTargetLocation(SoccerCharacter);

	if (ForcedAutoPassTarget.IsNearlyZero())
	{
		return false;
	}

	LastAIPossessionForcedActionTime = CurrentTime;

	ClearInitialPossessionEscape();

	if (!TryRegisterAIKickTouchForRules(SoccerCharacter))
	{
		return true;
	}

	SoccerCharacter->StartAIAutoPassToLocation(
		ForcedAutoPassTarget,
		AIPossessionStuckAutoPassHorizontalSpeed,
		AIAutoPassMinTravelTime,
		AIAutoPassMaxTravelTime
	);

	ClearAIPossessionStuckTracking();

	// This is another short carry touch. Keep following it instead of inserting
	// a stop between the kick and the next run.
	MoveToLocationWithAIMovement(
		ESoccerAIOrder::AttackRunIntoSpace,
		ForcedAutoPassTarget,
		AIAutoPassFollowAcceptanceRadius,
		false
	);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			0.9f,
			FColor::Orange,
			TEXT("AI failsafe: autopase forzado")
		);
	}

	return true;
}

void ASoccerAIController::ClearAIPossessionStuckTracking()
{
	bAIPossessionStuckTrackingActive = false;
	LastAIPossessionMovementTime = -1000.0f;
}

FVector ASoccerAIController::BuildAIStuckAutoPassTargetLocation(
	const ASoccerAICharacter* SoccerCharacter
) const
{
	if (!IsValid(SoccerCharacter))
	{
		return FVector::ZeroVector;
	}

	FVector PreferredDirection = FVector::ZeroVector;

	if (IsValid(MatchManager))
	{
		const FVector AttackMoveLocation =
			MatchManager->GetOpenPlayCarryIntentTargetLocation(SoccerCharacter);

		PreferredDirection =
			AttackMoveLocation - SoccerCharacter->GetActorLocation();

		PreferredDirection.Z = 0.0f;
		PreferredDirection = PreferredDirection.GetSafeNormal();
	}

	if (PreferredDirection.IsNearlyZero())
	{
		PreferredDirection = SoccerCharacter->GetActorForwardVector();
		PreferredDirection.Z = 0.0f;
		PreferredDirection = PreferredDirection.GetSafeNormal();
	}

	if (PreferredDirection.IsNearlyZero())
	{
		return FVector::ZeroVector;
	}

	FVector DirectionToUse = PreferredDirection;

	if (
		bUseAILocalDribbleDirection &&
		SoccerCharacter->GetPlayerRole() != ESoccerPlayerRole::Goalkeeper
		)
	{
		FVector LocalDirection = FVector::ZeroVector;
		bool bLocalDirectionIsSafe = false;

		const bool bConservativeMode =
			IsValid(MatchManager) &&
			MatchManager->ShouldBallCarrierPreferConservativeAction(
				SoccerCharacter
			);

		if (
			FindBestLocalAIDribbleDirection(
				SoccerCharacter,
				PreferredDirection,
				bConservativeMode,
				LocalDirection,
				bLocalDirectionIsSafe
			) &&
			!LocalDirection.IsNearlyZero()
			)
		{
			(void)bLocalDirectionIsSafe;

			// The failsafe is deliberately allowed to use the least dangerous
			// evaluated direction even when every candidate is formally unsafe.
			// Its purpose is to avoid the old stationary deadlock.
			DirectionToUse = LocalDirection;
		}
	}

	FVector TargetLocation =
		SoccerCharacter->GetActorLocation()
		+ DirectionToUse * AIPossessionStuckAutoPassDistance;

	TargetLocation.Z = SoccerCharacter->GetActorLocation().Z;

	UWorld* World = GetWorld();

	if (World != nullptr)
	{
		UNavigationSystemV1* NavigationSystem =
			FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);

		if (NavigationSystem != nullptr)
		{
			FNavLocation ProjectedLocation;

			const bool bProjected =
				NavigationSystem->ProjectPointToNavigation(
					TargetLocation,
					ProjectedLocation,
					FVector(350.0f, 350.0f, 350.0f)
				);

			if (bProjected)
			{
				TargetLocation = ProjectedLocation.Location;
			}
		}
	}

	return TargetLocation;
}

bool ASoccerAIController::TryPassToTeammateIfReady(
	ASoccerAICharacter* SoccerCharacter
)
{
	if (!bUseAISimpleTeammatePass)
	{
		return false;
	}

	if (!IsValid(SoccerCharacter))
	{
		return false;
	}

	if (!SoccerCharacter->IsAIPossessingBall())
	{
		return false;
	}

	if (!IsValid(MatchManager))
	{
		return false;
	}

	ASoccerCharacterBase* Teammate =
		FindSimplePassTeammate(SoccerCharacter);

	if (!IsValid(Teammate))
	{
		return false;
	}

	const ESoccerFieldZone SelfZone =
		MatchManager->GetCharacterFieldZone(SoccerCharacter);

	const ESoccerFieldZone TeammateZone =
		MatchManager->GetCharacterFieldZone(Teammate);

	const int32 SelfZoneIndex =
		GetFieldZoneIndex(SelfZone);

	const int32 TeammateZoneIndex =
		GetFieldZoneIndex(TeammateZone);

	const bool bTeammateIsBetterPositioned =
		TeammateZoneIndex - SelfZoneIndex >= AIBetterPassMinZoneAdvantage;

	if (!bTeammateIsBetterPositioned)
	{
		return false;
	}

	const FVector PassTargetLocation =
		BuildSimplePassTargetLocation(
			SoccerCharacter,
			Teammate
		);

	if (PassTargetLocation.IsNearlyZero())
	{
		return false;
	}

	if (
		TryPrepareMainActionIfBlocked(
			SoccerCharacter,
			ESoccerAIPendingMainAction::PassToTeammate,
			PassTargetLocation,
			Teammate
		)
		)
	{
		return true;
	}

	if (!TryRegisterAIKickTouchForRules(SoccerCharacter))
	{
		return true;
	}

	SoccerCharacter->KickAIBallToTarget(
		PassTargetLocation,
		AISimplePassHorizontalSpeed,
		AISimplePassMinTravelTime,
		AISimplePassMaxTravelTime
	);

	MatchManager->RegisterOpenPlayPassIntent(
		SoccerCharacter,
		Teammate,
		PassTargetLocation
	);

	if (SoccerCharacter->IsAIKickMontageActive())
	{
		StopMovement();
	}

	if (GEngine)
	{
		const bool bPassTargetIsHuman =
			Cast<AThirdPersonCppCharacter>(Teammate) != nullptr;

		const FString Message =
			bPassTargetIsHuman
			? TEXT("AI pase simple al humano")
			: TEXT("AI pase simple a companero bot");

		GEngine->AddOnScreenDebugMessage(
			-1,
			2.0f,
			FColor::Magenta,
			Message
		);
	}

	return true;
}

ASoccerCharacterBase* ASoccerAIController::FindSimplePassTeammate(
	const ASoccerAICharacter* SoccerCharacter
) const
{
	if (!IsValid(SoccerCharacter))
	{
		return nullptr;
	}

	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return nullptr;
	}

	FVector Forward = SoccerCharacter->GetActorForwardVector();
	Forward.Z = 0.0f;
	Forward = Forward.GetSafeNormal();

	const int32 SelfZoneIndex =
		IsValid(MatchManager)
		? GetFieldZoneIndex(MatchManager->GetCharacterFieldZone(SoccerCharacter))
		: 0;

	ASoccerCharacterBase* BestTeammate = nullptr;
	float BestScore = -TNumericLimits<float>::Max();

	for (TActorIterator<ASoccerCharacterBase> It(World); It; ++It)
	{
		ASoccerCharacterBase* Candidate = *It;

		if (!IsValid(Candidate))
		{
			continue;
		}

		if (Candidate == SoccerCharacter)
		{
			continue;
		}

		if (Candidate->GetTeam() != SoccerCharacter->GetTeam())
		{
			continue;
		}

		if (Candidate->GetPlayerRole() == ESoccerPlayerRole::Goalkeeper)
		{
			continue;
		}

		const float DistanceToCandidate = FVector::Dist2D(
			SoccerCharacter->GetActorLocation(),
			Candidate->GetActorLocation()
		);

		if (DistanceToCandidate < AISimplePassMinDistance)
		{
			continue;
		}

		if (DistanceToCandidate > AISimplePassMaxDistance)
		{
			continue;
		}

		FVector ToCandidate =
			Candidate->GetActorLocation() - SoccerCharacter->GetActorLocation();

		ToCandidate.Z = 0.0f;
		ToCandidate = ToCandidate.GetSafeNormal();

		float ForwardDot = 0.0f;

		if (!Forward.IsNearlyZero() && !ToCandidate.IsNearlyZero())
		{
			ForwardDot = FVector::DotProduct(Forward, ToCandidate);
		}

		const float DistanceScore =
			1.0f - FMath::Clamp(
				DistanceToCandidate / AISimplePassMaxDistance,
				0.0f,
				1.0f
			);

		const float ForwardScore =
			(ForwardDot + 1.0f) * 0.5f;

		const int32 CandidateZoneIndex =
			IsValid(MatchManager)
			? GetFieldZoneIndex(MatchManager->GetCharacterFieldZone(Candidate))
			: SelfZoneIndex;

		const int32 ZoneAdvantage =
			CandidateZoneIndex - SelfZoneIndex;

		const float ZoneScore =
			static_cast<float>(ZoneAdvantage) * AISimplePassBetterZoneWeight;

		const bool bCandidateIsHuman =
			Cast<AThirdPersonCppCharacter>(Candidate) != nullptr;

		const float HumanBonus =
			bCandidateIsHuman
			? AISimplePassHumanTestBonus
			: 0.0f;

		const float TotalScore =
			DistanceScore +
			ForwardScore * AISimplePassForwardPreferenceWeight +
			ZoneScore +
			HumanBonus;

		if (TotalScore > BestScore)
		{
			BestScore = TotalScore;
			BestTeammate = Candidate;
		}
	}

	return BestTeammate;
}

FVector ASoccerAIController::BuildSimplePassTargetLocation(
	const ASoccerAICharacter* SoccerCharacter,
	const ASoccerCharacterBase* Teammate
) const
{
	if (!IsValid(SoccerCharacter) || !IsValid(Teammate))
	{
		return FVector::ZeroVector;
	}

	FVector TeammateForward = Teammate->GetActorForwardVector();
	TeammateForward.Z = 0.0f;
	TeammateForward = TeammateForward.GetSafeNormal();

	FVector TargetLocation = Teammate->GetActorLocation();

	if (!TeammateForward.IsNearlyZero())
	{
		TargetLocation += TeammateForward * AISimplePassTargetForwardLead;
	}

	TargetLocation.Z = SoccerCharacter->GetActorLocation().Z;

	UWorld* World = GetWorld();

	if (World != nullptr)
	{
		UNavigationSystemV1* NavigationSystem =
			FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);

		if (NavigationSystem != nullptr)
		{
			FNavLocation ProjectedLocation;

			const bool bProjected =
				NavigationSystem->ProjectPointToNavigation(
					TargetLocation,
					ProjectedLocation,
					FVector(350.0f, 350.0f, 350.0f)
				);

			if (bProjected)
			{
				TargetLocation = ProjectedLocation.Location;
			}
		}
	}

	return TargetLocation;
}

FVector ASoccerAIController::BuildAIShotTargetLocation(
	const ASoccerAICharacter* SoccerCharacter
) const
{
	if (!IsValid(SoccerCharacter) || !IsValid(MatchManager))
	{
		return FVector::ZeroVector;
	}

	const ESoccerTeam AttackingTeam = SoccerCharacter->GetTeam();

	const FVector GoalCenterLocation =
		MatchManager->GetShotTargetLocation(SoccerCharacter);

	const FVector AttackDirection =
		MatchManager->GetFieldAttackDirectionForTeam(AttackingTeam);

	FVector GoalRight = FVector::CrossProduct(
		FVector::UpVector,
		AttackDirection
	);
	GoalRight.Z = 0.0f;
	GoalRight = GoalRight.GetSafeNormal();

	if (GoalRight.IsNearlyZero())
	{
		return FVector::ZeroVector;
	}

	FVector ShotOrigin = SoccerCharacter->GetActorLocation();
	if (ASoccerBall* SoccerBall = MatchManager->GetSoccerBall())
	{
		if (IsValid(SoccerBall))
		{
			ShotOrigin = SoccerBall->GetActorLocation();
		}
	}

	const float GoalHalfWidth = SoccerFieldDimensions::GoalHalfWidthCm;
	const float GoalHeight = SoccerFieldDimensions::GoalHeightCm;

	const float SectorLateralOffset =
		GoalHalfWidth *
		FMath::Clamp(AIShotSectorLateralFraction, 0.0f, 0.95f);

	const float BottomHeight =
		GoalHeight *
		FMath::Clamp(AIShotBottomHeightFraction, 0.05f, 0.90f);

	const float CenterHeight =
		GoalHeight *
		FMath::Clamp(AIShotCenterHeightFraction, 0.05f, 0.90f);

	const float TopHeight =
		GoalHeight *
		FMath::Clamp(AIShotTopHeightFraction, 0.10f, 0.95f);

	struct FShotSectorCandidate
	{
		const TCHAR* Label = TEXT("CENTER");
		float LateralOffset = 0.0f;
		float Height = 0.0f;
		FVector Location = FVector::ZeroVector;
		float Score = -TNumericLimits<float>::Max();
	};

	TArray<FShotSectorCandidate> Candidates;
	Candidates.Reserve(5);

	auto AddCandidate =
		[&](const TCHAR* Label, float LateralOffset, float Height)
	{
		FShotSectorCandidate Candidate;
		Candidate.Label = Label;
		Candidate.LateralOffset = LateralOffset;
		Candidate.Height = Height;
		Candidate.Location =
			GoalCenterLocation + GoalRight * LateralOffset;
		Candidate.Location.Z = GoalCenterLocation.Z + Height;
		Candidates.Add(Candidate);
	};

	// Left/right are team-relative through the field attack direction, so
	// they remain correct after the halftime side swap.
	AddCandidate(TEXT("BOTTOM_LEFT"), -SectorLateralOffset, BottomHeight);
	AddCandidate(TEXT("TOP_LEFT"), -SectorLateralOffset, TopHeight);
	AddCandidate(TEXT("CENTER"), 0.0f, CenterHeight);
	AddCandidate(TEXT("BOTTOM_RIGHT"), SectorLateralOffset, BottomHeight);
	AddCandidate(TEXT("TOP_RIGHT"), SectorLateralOffset, TopHeight);

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return Candidates[2].Location;
	}

	const ASoccerCharacterBase* OpponentGoalkeeper = nullptr;
	float BestGoalkeeperGoalDistanceSq = TNumericLimits<float>::Max();

	for (TActorIterator<ASoccerCharacterBase> It(World); It; ++It)
	{
		const ASoccerCharacterBase* CandidateCharacter = *It;

		if (
			!IsValid(CandidateCharacter) ||
			CandidateCharacter == SoccerCharacter ||
			CandidateCharacter->GetTeam() == AttackingTeam ||
			CandidateCharacter->GetPlayerRole() != ESoccerPlayerRole::Goalkeeper
			)
		{
			continue;
		}

		const float GoalDistanceSq = FVector::DistSquared2D(
			CandidateCharacter->GetActorLocation(),
			GoalCenterLocation
		);

		if (GoalDistanceSq < BestGoalkeeperGoalDistanceSq)
		{
			BestGoalkeeperGoalDistanceSq = GoalDistanceSq;
			OpponentGoalkeeper = CandidateCharacter;
		}
	}

	auto PointToSegmentAlpha =
		[](const FVector& Point, const FVector& SegmentStart, const FVector& SegmentEnd)
	{
		const FVector Segment = SegmentEnd - SegmentStart;
		const float SegmentSizeSq = Segment.SizeSquared();

		if (SegmentSizeSq <= KINDA_SMALL_NUMBER)
		{
			return 0.0f;
		}

		return FMath::Clamp(
			FVector::DotProduct(Point - SegmentStart, Segment) / SegmentSizeSq,
			0.0f,
			1.0f
		);
	};

	auto PointToSegmentDistance =
		[](const FVector& Point, const FVector& SegmentStart, const FVector& SegmentEnd)
	{
		const FVector Segment = SegmentEnd - SegmentStart;
		const float SegmentSizeSq = Segment.SizeSquared();

		if (SegmentSizeSq <= KINDA_SMALL_NUMBER)
		{
			return FVector::Dist(Point, SegmentStart);
		}

		const float Alpha = FMath::Clamp(
			FVector::DotProduct(Point - SegmentStart, Segment) / SegmentSizeSq,
			0.0f,
			1.0f
		);

		const FVector ClosestPoint = SegmentStart + Segment * Alpha;
		return FVector::Dist(Point, ClosestPoint);
	};

	const float ShotDistance = FVector::Dist2D(
		SoccerCharacter->GetActorLocation(),
		GoalCenterLocation
	);

	const float ShotDistanceAlpha = FMath::Clamp(
		ShotDistance / FMath::Max(AIShotDistanceToTarget, 1.0f),
		0.0f,
		1.0f
	);

	const float ShooterLateralOffset = FVector::DotProduct(
		SoccerCharacter->GetActorLocation() - GoalCenterLocation,
		GoalRight
	);

	FVector DirectionToGoal =
		GoalCenterLocation - SoccerCharacter->GetActorLocation();
	DirectionToGoal.Z = 0.0f;
	DirectionToGoal = DirectionToGoal.GetSafeNormal();

	const float ApproachDot = FMath::Clamp(
		FVector::DotProduct(DirectionToGoal, AttackDirection),
		0.0f,
		1.0f
	);

	const float ShotApproachAngleRadians = FMath::Acos(ApproachDot);
	const float WideAngleAlpha = FMath::Clamp(
		ShotApproachAngleRadians /
		FMath::DegreesToRadians(45.0f),
		0.0f,
		1.0f
	);

	const float ShooterSideSign =
		FMath::Abs(ShooterLateralOffset) > 1.0f
		? FMath::Sign(ShooterLateralOffset)
		: 0.0f;

	int32 BestCandidateIndex = 0;
	float BestCandidateScore = -TNumericLimits<float>::Max();

	for (int32 CandidateIndex = 0; CandidateIndex < Candidates.Num(); ++CandidateIndex)
	{
		FShotSectorCandidate& Candidate = Candidates[CandidateIndex];
		float Score = 0.50f;

		// 1) Goalkeeper: evaluate both the distance from the keeper to the
		// complete shot ray and how far the selected mouth sector is from the
		// keeper's current lateral position. This naturally accounts for a
		// goalkeeper who is advanced or displaced toward one post.
		if (IsValid(OpponentGoalkeeper))
		{
			const float GoalkeeperPathDistance = PointToSegmentDistance(
				OpponentGoalkeeper->GetActorLocation(),
				ShotOrigin,
				Candidate.Location
			);

			const float GoalkeeperClearanceScore = FMath::Clamp(
				GoalkeeperPathDistance /
				FMath::Max(AIShotGoalkeeperClearanceDistance, 1.0f),
				0.0f,
				1.0f
			);

			Score +=
				GoalkeeperClearanceScore *
				AIShotGoalkeeperClearanceWeight;

			const float GoalkeeperLateralOffset = FVector::DotProduct(
				OpponentGoalkeeper->GetActorLocation() - GoalCenterLocation,
				GoalRight
			);

			const float GoalkeeperGoalSeparationScore = FMath::Clamp(
				FMath::Abs(Candidate.LateralOffset - GoalkeeperLateralOffset) /
				FMath::Max(GoalHalfWidth * 1.25f, 1.0f),
				0.0f,
				1.0f
			);

			Score +=
				GoalkeeperGoalSeparationScore *
				AIShotGoalkeeperGoalSeparationWeight;
		}

		// 2) Defenders: use the nearest opposing field player's capsule center
		// to the 3D shot segment. Top sectors therefore become naturally less
		// obstructed by a player who only blocks a low trajectory.
		float NearestDefenderPathDistance = TNumericLimits<float>::Max();

		for (TActorIterator<ASoccerCharacterBase> It(World); It; ++It)
		{
			const ASoccerCharacterBase* Defender = *It;

			if (
				!IsValid(Defender) ||
				Defender == SoccerCharacter ||
				Defender->GetTeam() == AttackingTeam ||
				Defender->GetPlayerRole() == ESoccerPlayerRole::Goalkeeper
				)
			{
				continue;
			}

			const float DefenderSegmentAlpha = PointToSegmentAlpha(
				Defender->GetActorLocation(),
				ShotOrigin,
				Candidate.Location
			);

			// A rival behind or effectively level with the shooter can pressure
			// the kick but does not physically close the outgoing shot lane.
			if (DefenderSegmentAlpha <= 0.04f)
			{
				continue;
			}

			NearestDefenderPathDistance = FMath::Min(
				NearestDefenderPathDistance,
				PointToSegmentDistance(
					Defender->GetActorLocation(),
					ShotOrigin,
					Candidate.Location
				)
			);
		}

		if (NearestDefenderPathDistance < TNumericLimits<float>::Max())
		{
			const float DefenderClearanceScore = FMath::Clamp(
				NearestDefenderPathDistance /
				FMath::Max(AIShotDefenderClearanceDistance, 1.0f),
				0.0f,
				1.0f
			);

			Score +=
				DefenderClearanceScore *
				AIShotDefenderClearanceWeight;
		}

		// 3) Wide shooting angles make the far post attractive, but this is a
		// preference only. A goalkeeper or defender can still make another
		// sector score higher.
		if (ShooterSideSign != 0.0f && SectorLateralOffset > 1.0f)
		{
			const float FarPostScore = FMath::Clamp(
				(-ShooterSideSign * Candidate.LateralOffset) / SectorLateralOffset,
				0.0f,
				1.0f
			);

			Score +=
				FarPostScore *
				WideAngleAlpha *
				AIShotFarPostPreferenceWeight;
		}

		// 4) From farther away, extreme lateral/high sectors receive a small
		// precision cost. Close to goal the AI is much more willing to attack
		// the corners.
		const float LateralExtremity =
			GoalHalfWidth > 1.0f
			? FMath::Abs(Candidate.LateralOffset) / GoalHalfWidth
			: 0.0f;

		const float HeightExtremity = FMath::Clamp(
			FMath::Abs(Candidate.Height - CenterHeight) /
			FMath::Max(GoalHeight * 0.5f, 1.0f),
			0.0f,
			1.0f
		);

		const float PrecisionDifficulty =
			LateralExtremity * 0.70f +
			HeightExtremity * 0.30f;

		Score -=
			ShotDistanceAlpha *
			PrecisionDifficulty *
			AIShotDistancePrecisionPenalty;

		// 5) Human-like uncertainty: this is deliberately applied after the
		// tactical scores so good sectors remain more likely without becoming
		// guaranteed.
		Score += FMath::FRandRange(
			-FMath::Max(AIShotSectorScoreRandomness, 0.0f),
			FMath::Max(AIShotSectorScoreRandomness, 0.0f)
		);

		Candidate.Score = Score;

		if (Score > BestCandidateScore)
		{
			BestCandidateScore = Score;
			BestCandidateIndex = CandidateIndex;
		}
	}

	FShotSectorCandidate& BestCandidate = Candidates[BestCandidateIndex];

	// A little placement variation keeps repeated shots from converging on
	// the exact same pixel. Error grows with distance but remains inside the
	// real goal mouth in this stage.
	const float ErrorScale = FMath::Lerp(0.35f, 1.0f, ShotDistanceAlpha);

	float FinalLateralOffset =
		BestCandidate.LateralOffset +
		FMath::FRandRange(
			-AIShotAimErrorLateralFraction * SoccerFieldDimensions::GoalWidthCm,
			AIShotAimErrorLateralFraction * SoccerFieldDimensions::GoalWidthCm
		) * ErrorScale;

	float FinalHeight =
		BestCandidate.Height +
		FMath::FRandRange(
			-AIShotAimErrorVerticalFraction * GoalHeight,
			AIShotAimErrorVerticalFraction * GoalHeight
		) * ErrorScale;

	FinalLateralOffset = FMath::Clamp(
		FinalLateralOffset,
		-GoalHalfWidth * 0.94f,
		GoalHalfWidth * 0.94f
	);

	FinalHeight = FMath::Clamp(
		FinalHeight,
		GoalHeight * 0.08f,
		GoalHeight * 0.90f
	);

	FVector ShotTargetLocation =
		GoalCenterLocation + GoalRight * FinalLateralOffset;
	ShotTargetLocation.Z = GoalCenterLocation.Z + FinalHeight;

	if (bDebugAIShotSectorSelection)
	{
		for (int32 CandidateIndex = 0; CandidateIndex < Candidates.Num(); ++CandidateIndex)
		{
			const FShotSectorCandidate& Candidate = Candidates[CandidateIndex];
			const bool bSelected = CandidateIndex == BestCandidateIndex;
			const FColor CandidateColor = bSelected ? FColor::Green : FColor::Silver;

			ASoccerDebugManager::DrawSphere(
				this,
				ESoccerDebugCategory::AI,
				Candidate.Location,
				18.0f,
				CandidateColor,
				1.25f,
				10,
				2.0f
			);

			ASoccerDebugManager::DrawString(
				this,
				ESoccerDebugCategory::AI,
				Candidate.Location + FVector(0.0f, 0.0f, 25.0f),
				FString::Printf(
					TEXT("%s %.2f"),
					Candidate.Label,
					Candidate.Score
				),
				CandidateColor,
				1.25f
			);
		}

		// Cyan is the intended point selected by the Stage 13 tactical model.
		// Stage 14 can later displace the actually executed kick away from it.
		ASoccerDebugManager::DrawLine(
			this,
			ESoccerDebugCategory::AI,
			ShotOrigin,
			ShotTargetLocation,
			FColor::Cyan,
			1.25f,
			2.5f
		);
	}

	return ShotTargetLocation;
}

FVector ASoccerAIController::ApplyAIShotExecutionError(
	const ASoccerAICharacter* SoccerCharacter,
	const FVector& IntendedShotTargetLocation
) const
{
	if (
		!bUseAIShotExecutionError ||
		!IsValid(SoccerCharacter) ||
		!IsValid(MatchManager) ||
		IntendedShotTargetLocation.IsNearlyZero()
		)
	{
		return IntendedShotTargetLocation;
	}

	const ESoccerTeam AttackingTeam = SoccerCharacter->GetTeam();

	const FVector GoalCenterLocation =
		MatchManager->GetShotTargetLocation(SoccerCharacter);

	FVector AttackDirection =
		MatchManager->GetFieldAttackDirectionForTeam(AttackingTeam);
	AttackDirection.Z = 0.0f;
	AttackDirection = AttackDirection.GetSafeNormal();

	FVector GoalRight = FVector::CrossProduct(
		FVector::UpVector,
		AttackDirection
	);
	GoalRight.Z = 0.0f;
	GoalRight = GoalRight.GetSafeNormal();

	if (AttackDirection.IsNearlyZero() || GoalRight.IsNearlyZero())
	{
		return IntendedShotTargetLocation;
	}

	const float GoalHalfWidth = SoccerFieldDimensions::GoalHalfWidthCm;
	const float GoalHeight = SoccerFieldDimensions::GoalHeightCm;

	const float ShotDistance = FVector::Dist2D(
		SoccerCharacter->GetActorLocation(),
		GoalCenterLocation
	);

	const float DistanceAlpha = FMath::Clamp(
		ShotDistance / FMath::Max(AIShotDistanceToTarget, 1.0f),
		0.0f,
		1.0f
	);

	// Pressure is evaluated at the moment the kick is actually committed.
	// Any opponent can disturb the strike; the goalkeeper only contributes if
	// it has genuinely come close enough to challenge the shooter.
	float NearestOpponentDistance = TNumericLimits<float>::Max();
	UWorld* World = GetWorld();

	if (World != nullptr)
	{
		for (TActorIterator<ASoccerCharacterBase> It(World); It; ++It)
		{
			const ASoccerCharacterBase* Opponent = *It;

			if (
				!IsValid(Opponent) ||
				Opponent == SoccerCharacter ||
				Opponent->GetTeam() == AttackingTeam
				)
			{
				continue;
			}

			NearestOpponentDistance = FMath::Min(
				NearestOpponentDistance,
				FVector::Dist2D(
					Opponent->GetActorLocation(),
					SoccerCharacter->GetActorLocation()
				)
			);
		}
	}

	const float PressureFullDistance =
		FMath::Max(AIShotExecutionPressureFullDistanceCm, 0.0f);

	const float PressureMaxDistance = FMath::Max(
		AIShotExecutionPressureMaxDistanceCm,
		PressureFullDistance + 1.0f
	);

	float PressureAlpha = 0.0f;

	if (NearestOpponentDistance < TNumericLimits<float>::Max())
	{
		PressureAlpha = 1.0f - FMath::Clamp(
			(NearestOpponentDistance - PressureFullDistance) /
			(PressureMaxDistance - PressureFullDistance),
			0.0f,
			1.0f
		);
	}

	FVector DirectionToGoal =
		GoalCenterLocation - SoccerCharacter->GetActorLocation();
	DirectionToGoal.Z = 0.0f;
	DirectionToGoal = DirectionToGoal.GetSafeNormal();

	const float ApproachDot = FMath::Clamp(
		FVector::DotProduct(DirectionToGoal, AttackDirection),
		0.0f,
		1.0f
	);

	const float WideAngleAlpha = FMath::Clamp(
		FMath::Acos(ApproachDot) /
		FMath::DegreesToRadians(45.0f),
		0.0f,
		1.0f
	);

	const float IntendedLateralOffset = FVector::DotProduct(
		IntendedShotTargetLocation - GoalCenterLocation,
		GoalRight
	);

	const float IntendedHeight =
		IntendedShotTargetLocation.Z - GoalCenterLocation.Z;

	const float LateralDifficulty =
		GoalHalfWidth > 1.0f
		? FMath::Clamp(
			FMath::Abs(IntendedLateralOffset) / GoalHalfWidth,
			0.0f,
			1.0f
		)
		: 0.0f;

	const float HeightDifficulty = FMath::Clamp(
		FMath::Abs(IntendedHeight - GoalHeight * 0.45f) /
		FMath::Max(GoalHeight * 0.50f, 1.0f),
		0.0f,
		1.0f
	);

	const float DifficultTargetAlpha = FMath::Clamp(
		LateralDifficulty * 0.70f + HeightDifficulty * 0.30f,
		0.0f,
		1.0f
	);

	const float ErrorMagnitudeCm =
		FMath::Max(AIShotExecutionErrorScale, 0.0f) *
		(
			FMath::Max(AIShotExecutionBaseErrorCm, 0.0f) +
			DistanceAlpha *
				FMath::Max(AIShotExecutionDistanceAdditionalErrorCm, 0.0f) +
			PressureAlpha *
				FMath::Max(AIShotExecutionPressureAdditionalErrorCm, 0.0f) +
			WideAngleAlpha *
				FMath::Max(AIShotExecutionWideAngleAdditionalErrorCm, 0.0f) +
			DifficultTargetAlpha *
				FMath::Max(AIShotExecutionDifficultTargetAdditionalErrorCm, 0.0f)
		);

	// Two uniform samples averaged together create a triangular distribution:
	// small errors are common, while large misses remain possible but rarer.
	auto SampleExecutionError = []()
	{
		return
			(FMath::FRandRange(-1.0f, 1.0f) +
			 FMath::FRandRange(-1.0f, 1.0f)) * 0.5f;
	};

	const float LateralExecutionError =
		SampleExecutionError() * ErrorMagnitudeCm;

	const float VerticalExecutionError =
		SampleExecutionError() *
		ErrorMagnitudeCm *
		FMath::Max(AIShotExecutionVerticalErrorScale, 0.0f);

	const float FinalLateralOffset =
		IntendedLateralOffset + LateralExecutionError;

	const float MinimumHeight =
		GoalHeight *
		FMath::Clamp(AIShotExecutionMinimumHeightFraction, 0.0f, 0.25f);

	const float FinalHeight = FMath::Max(
		IntendedHeight + VerticalExecutionError,
		MinimumHeight
	);

	FVector ExecutedShotTargetLocation =
		GoalCenterLocation + GoalRight * FinalLateralOffset;
	ExecutedShotTargetLocation.Z = GoalCenterLocation.Z + FinalHeight;

	if (bDebugAIShotSectorSelection)
	{
		FVector ShotOrigin = SoccerCharacter->GetActorLocation();
		if (ASoccerBall* SoccerBall = MatchManager->GetSoccerBall())
		{
			if (IsValid(SoccerBall))
			{
				ShotOrigin = SoccerBall->GetActorLocation();
			}
		}

		const bool bExecutedInsideGoal =
			FMath::Abs(FinalLateralOffset) <= GoalHalfWidth &&
			FinalHeight >= 0.0f &&
			FinalHeight <= GoalHeight;

		const FColor ExecutionColor =
			bExecutedInsideGoal ? FColor::Green : FColor::Red;

		ASoccerDebugManager::DrawSphere(
			this,
			ESoccerDebugCategory::AI,
			IntendedShotTargetLocation,
			12.0f,
			FColor::Cyan,
			1.25f,
			8,
			1.5f
		);

		ASoccerDebugManager::DrawSphere(
			this,
			ESoccerDebugCategory::AI,
			ExecutedShotTargetLocation,
			22.0f,
			ExecutionColor,
			1.25f,
			10,
			2.0f
		);

		ASoccerDebugManager::DrawLine(
			this,
			ESoccerDebugCategory::AI,
			IntendedShotTargetLocation,
			ExecutedShotTargetLocation,
			FColor::Yellow,
			1.25f,
			2.0f
		);

		ASoccerDebugManager::DrawLine(
			this,
			ESoccerDebugCategory::AI,
			ShotOrigin,
			ExecutedShotTargetLocation,
			ExecutionColor,
			1.25f,
			3.0f
		);

		ASoccerDebugManager::DrawString(
			this,
			ESoccerDebugCategory::AI,
			ExecutedShotTargetLocation + FVector(0.0f, 0.0f, 32.0f),
			FString::Printf(
				TEXT("EXEC %.0fcm | D %.2f P %.2f A %.2f T %.2f | %s"),
				ErrorMagnitudeCm,
				DistanceAlpha,
				PressureAlpha,
				WideAngleAlpha,
				DifficultTargetAlpha,
				bExecutedInsideGoal ? TEXT("ON TARGET") : TEXT("MISS")
			),
			ExecutionColor,
			1.25f
		);
	}

	return ExecutedShotTargetLocation;
}

bool ASoccerAIController::IsBallAtAIPossessionHeight(
	const ASoccerAICharacter* SoccerCharacter,
	const ASoccerBall* SoccerBall
) const
{
	if (!IsValid(SoccerCharacter) || !IsValid(SoccerBall))
	{
		return false;
	}

	const UCapsuleComponent* CapsuleComponent =
		SoccerCharacter->GetCapsuleComponent();

	if (CapsuleComponent == nullptr)
	{
		return false;
	}

	const float CharacterGroundZ =
		SoccerCharacter->GetActorLocation().Z -
		CapsuleComponent->GetScaledCapsuleHalfHeight();

	const float BallHeightFromGround =
		SoccerBall->GetActorLocation().Z - CharacterGroundZ;

	return
		BallHeightFromGround >= -20.0f &&
		BallHeightFromGround <= FMath::Min(
			FMath::Max(10.0f, AIFootControlMaxBallHeight),
			FMath::Max(10.0f, AIBallPossessionMaxHeight)
		);
}

void ASoccerAIController::DebugPrintAISituation(
	ASoccerAICharacter* SoccerCharacter,
	ESoccerAIOrder CurrentOrder
)
{
	if (!ASoccerDebugManager::IsEnabled(this, ESoccerDebugCategory::AI))
	{
		return;
	}

	if (!IsValid(SoccerCharacter) || !IsValid(MatchManager))
	{
		return;
	}

	const bool bIsRelevant =
		SoccerCharacter->IsAIPossessingBall() ||
		CurrentOrder == ESoccerAIOrder::ChaseBall ||
		CurrentOrder == ESoccerAIOrder::PressBall ||
		CurrentOrder == ESoccerAIOrder::SupportBall ||
		CurrentOrder == ESoccerAIOrder::MaintainTeamShape ||
		CurrentOrder == ESoccerAIOrder::AttackRecoverBall ||
		CurrentOrder == ESoccerAIOrder::AttackSupportShort ||
		CurrentOrder == ESoccerAIOrder::AttackSupportForward ||
		CurrentOrder == ESoccerAIOrder::AttackRunIntoSpace ||
		CurrentOrder == ESoccerAIOrder::AttackWideSupport ||
		CurrentOrder == ESoccerAIOrder::AttackRestDefense ||
		CurrentOrder == ESoccerAIOrder::AttackCompensateCover ||
		CurrentOrder == ESoccerAIOrder::DefendProtectGoalLane ||
		CurrentOrder == ESoccerAIOrder::DefendCoverCenter ||
		CurrentOrder == ESoccerAIOrder::DefendCompactShape ||
		CurrentOrder == ESoccerAIOrder::DefendMarkDangerousReceiver;

	if (bDebugOnlyPrintRelevantAISituation && !bIsRelevant)
	{
		return;
	}

	const float CurrentTime =
		GetWorld() != nullptr
		? GetWorld()->GetTimeSeconds()
		: 0.0f;

	if (CurrentTime - LastAISituationDebugPrintTime < DebugAISituationInterval)
	{
		return;
	}

	LastAISituationDebugPrintTime = CurrentTime;

	const ESoccerFieldZone CharacterZone =
		MatchManager->GetCharacterFieldZone(SoccerCharacter);

	const ESoccerFieldZone BallZone =
		MatchManager->GetBallFieldZoneForTeam(SoccerCharacter->GetTeam());

	const ESoccerBallSituation BallSituation =
		MatchManager->GetBallSituationForCharacter(SoccerCharacter);

	const FString DebugText = FString::Printf(
		TEXT("AI %s | %s | Zone %s | BallZone %s | Ball %s | Order %s"),
		*GetDebugTeamText(SoccerCharacter->GetTeam()),
		*GetDebugRoleText(SoccerCharacter->GetPlayerRole()),
		*GetDebugFieldZoneText(CharacterZone),
		*GetDebugFieldZoneText(BallZone),
		*GetDebugBallSituationText(BallSituation),
		*GetDebugAIOrderText(CurrentOrder)
	);

	if (GEngine)
	{
		const int32 DebugKey =
			10000 + static_cast<int32>(SoccerCharacter->GetUniqueID());

		GEngine->AddOnScreenDebugMessage(
			DebugKey,
			DebugAISituationDuration,
			FColor::White,
			DebugText
		);
	}
}

FString ASoccerAIController::GetDebugFieldZoneText(
	ESoccerFieldZone Zone
) const
{
	switch (Zone)
	{
	case ESoccerFieldZone::ZoneA:
		return TEXT("A");

	case ESoccerFieldZone::ZoneB:
		return TEXT("B");

	case ESoccerFieldZone::ZoneC:
		return TEXT("C");

	case ESoccerFieldZone::ZoneD:
		return TEXT("D");

	case ESoccerFieldZone::ZoneE:
		return TEXT("E");

	case ESoccerFieldZone::ZoneF:
		return TEXT("F");

	default:
		return TEXT("?");
	}
}

FString ASoccerAIController::GetDebugBallSituationText(
	ESoccerBallSituation Situation
) const
{
	switch (Situation)
	{
	case ESoccerBallSituation::FreeBall:
		return TEXT("Free");

	case ESoccerBallSituation::SelfPossession:
		return TEXT("Self");

	case ESoccerBallSituation::OwnTeamPossession:
		return TEXT("OwnTeam");

	case ESoccerBallSituation::OpponentTeamPossession:
		return TEXT("OpponentTeam");

	default:
		return TEXT("?");
	}
}

FString ASoccerAIController::GetDebugAIOrderText(
	ESoccerAIOrder Order
) const
{
	switch (Order)
	{
	case ESoccerAIOrder::ReturnHome:
		return TEXT("ReturnHome");

	case ESoccerAIOrder::ChaseBall:
		return TEXT("ChaseBall");

	case ESoccerAIOrder::SupportBall:
		return TEXT("SupportBall");

	case ESoccerAIOrder::PressBall:
		return TEXT("PressBall");

	case ESoccerAIOrder::MaintainTeamShape:
		return TEXT("MaintainShape");

	case ESoccerAIOrder::AttackRecoverBall:
		return TEXT("AttackRecover");

	case ESoccerAIOrder::AttackSupportShort:
		return TEXT("AttackShort");

	case ESoccerAIOrder::AttackSupportForward:
		return TEXT("AttackForward");

	case ESoccerAIOrder::AttackRunIntoSpace:
		return TEXT("RunSpace");

	case ESoccerAIOrder::AttackWideSupport:
		return TEXT("WideSupport");

	case ESoccerAIOrder::AttackRestDefense:
		return TEXT("RestDefense");

	case ESoccerAIOrder::AttackCompensateCover:
		return TEXT("CompCover");

	case ESoccerAIOrder::DefendProtectGoalLane:
		return TEXT("DefGoalLane");

	case ESoccerAIOrder::DefendCoverCenter:
		return TEXT("DefCenter");

	case ESoccerAIOrder::DefendCompactShape:
		return TEXT("DefCompact");

	case ESoccerAIOrder::DefendMarkDangerousReceiver:
		return TEXT("DefMark");

	default:
		return TEXT("?");
	}
}

FString ASoccerAIController::GetDebugTeamText(
	ESoccerTeam Team
) const
{
	switch (Team)
	{
	case ESoccerTeam::PlayerTeam:
		return TEXT("PlayerTeam");

	case ESoccerTeam::OpponentTeam:
		return TEXT("OpponentTeam");

	default:
		return TEXT("?");
	}
}

FString ASoccerAIController::GetDebugRoleText(
	ESoccerPlayerRole PlayerRole
) const
{
	switch (PlayerRole)
	{
	case ESoccerPlayerRole::Goalkeeper:
		return TEXT("Goalkeeper");

	case ESoccerPlayerRole::Defender:
		return TEXT("Defender");

	case ESoccerPlayerRole::Midfielder:
		return TEXT("Midfielder");

	case ESoccerPlayerRole::Forward:
		return TEXT("Forward");

	default:
		return TEXT("?");
	}
}

bool ASoccerAIController::TryExecuteZoneBasedPossessionDecision(
	ASoccerAICharacter* SoccerCharacter,
	ESoccerFieldZone CharacterZone,
	float DeltaTime
)
{
	(void)DeltaTime;

	if (!IsValid(SoccerCharacter))
	{
		return false;
	}

	if (!SoccerCharacter->IsAIPossessingBall())
	{
		return false;
	}

	// A persistent human request is evaluated before the carrier's normal
	// shot/pass/autopass hierarchy, including near the opponent goal.
	if (
		IsValid(MatchManager) &&
		MatchManager->TryExecuteActiveHumanPassRequest(SoccerCharacter)
	)
	{
		ClearInitialPossessionEscape();
		ClearAIPossessionStuckTracking();
		return true;
	}

	// Stage 8D: the tactical system still decides which actions are legal, but
	// DecisionMaking/Anticipation control how quickly the carrier reaches that
	// decision. Composure only enlarges the hesitation while actually pressed.
	if (!IsOffensiveProfileDecisionReady(SoccerCharacter))
	{
		if (
			ActiveOffensiveDecisionEpisode ==
			ESoccerAIDecisionEpisode::StrongHesitation
			)
		{
			StopMovement();
			SoccerCharacter->SetAIPossessionCarryActive(false);
		}
		else
		{
			// A mild hesitation is cognitive, not a full-body stop. Preserve the
			// current run and adjust to the real moving ball without dragging it.
			SoccerCharacter->SetAIPossessionCarryActive(true);
			UpdatePhysicalPossessionApproach(SoccerCharacter, true);
		}

		ASoccerBall* DecisionWaitBall = MatchManager->GetSoccerBall();
		if (IsValid(DecisionWaitBall))
		{
			SetFocus(DecisionWaitBall);
		}

		return true;
	}

	// A mild episode may have used carry tracking while preserving locomotion.
	// Once the decision is ready, return ball authority to the chosen action.
	SoccerCharacter->SetAIPossessionCarryActive(false);

	// Physics may have carried the ball away during the decision episode. Move
	// into a genuine contact position before allowing pass, shot or auto-pass.
	if (UpdatePhysicalPossessionApproach(SoccerCharacter, false))
	{
		return true;
	}

	auto CompleteIfActionStarted =
		[this](bool bActionStarted) -> bool
	{
		if (!bActionStarted)
		{
			return false;
		}

		ClearInitialPossessionEscape();
		ClearAIPossessionStuckTracking();
		return true;
	};

	auto TryPreferredShot =
		[this, SoccerCharacter]() -> bool
	{
		if (!IsValid(MatchManager))
		{
			return false;
		}

		if (!MatchManager->ShouldBallCarrierPreferShot(SoccerCharacter))
		{
			return false;
		}

		if (!ShouldOffensiveProfileAcceptPreferredShot(SoccerCharacter))
		{
			return false;
		}

		return TryShootBallIfClose(SoccerCharacter);
	};

	auto TryBestPass =
		[this, SoccerCharacter](bool bAllowRetention) -> bool
	{
		if (TrySmartAttackPass(SoccerCharacter, bAllowRetention))
		{
			return true;
		}

		if (bAllowRetention)
		{
			// The carry grid already found danger. Do not bypass the evaluated
			// forward/feet/retention hierarchy with the old blind simple pass.
			return false;
		}

		// Fallback temporal.
		// M�s adelante, cuando el pase inteligente est� bien ajustado,
		// probablemente podamos eliminar este fallback.
		return TryPassToTeammateIfReady(SoccerCharacter);
	};

	auto TryAdvanceWithBall =
		[this, SoccerCharacter](bool& bOutRejectedByLocalSafety) -> bool
	{
		bOutRejectedByLocalSafety = false;

		if (
			TryStartAIAutoPass(
				SoccerCharacter,
				&bOutRejectedByLocalSafety
			)
			)
		{
			return true;
		}

		return false;
	};

	auto TryCarryThenPass =
		[&CompleteIfActionStarted, &TryAdvanceWithBall, &TryBestPass]() -> bool
	{
		bool bCarryRejectedBySafety = false;
		if (CompleteIfActionStarted(TryAdvanceWithBall(bCarryRejectedBySafety)))
		{
			return true;
		}

		// Only a locally unsafe carry unlocks the backward-space alternative.
		// The same evaluation still prefers a viable forward-space or feet pass.
		return CompleteIfActionStarted(TryBestPass(bCarryRejectedBySafety));
	};

	switch (CharacterZone)
	{
	case ESoccerFieldZone::ZoneA:
	case ESoccerFieldZone::ZoneB:
		// Continue carrying when the local reading finds a useful safe route;
		// otherwise prefer forward space, then feet, then retention space.
		if (TryCarryThenPass())
		{
			return true;
		}

		return false;

	case ESoccerFieldZone::ZoneC:
	case ESoccerFieldZone::ZoneD:
		if (TryCarryThenPass())
		{
			return true;
		}

		return false;

	case ESoccerFieldZone::ZoneE:
		// Zona ofensiva:
		// 1. Remate si hay ocasi�n clara.
		// A clear shot remains exceptional; otherwise use carry/pass hierarchy.
		if (CompleteIfActionStarted(TryPreferredShot()))
		{
			return true;
		}

		if (TryCarryThenPass())
		{
			return true;
		}

		return false;

	case ESoccerFieldZone::ZoneF:
	default:
		// Zona de definici�n:
		// 1. Remate claro.
		// Then carry or pass; keep the emergency shot as final attacking outlet.
		if (CompleteIfActionStarted(TryPreferredShot()))
		{
			return true;
		}

		if (TryCarryThenPass())
		{
			return true;
		}

		// Fallback agresivo solo en zona de definici�n.
		// Sirve para que no dude demasiado cerca del arco.
		if (CompleteIfActionStarted(TryShootBallIfClose(SoccerCharacter)))
		{
			return true;
		}

		return false;
	}
}

int32 ASoccerAIController::GetFieldZoneIndex(
	ESoccerFieldZone Zone
) const
{
	switch (Zone)
	{
	case ESoccerFieldZone::ZoneA:
		return 0;

	case ESoccerFieldZone::ZoneB:
		return 1;

	case ESoccerFieldZone::ZoneC:
		return 2;

	case ESoccerFieldZone::ZoneD:
		return 3;

	case ESoccerFieldZone::ZoneE:
		return 4;

	case ESoccerFieldZone::ZoneF:
		return 5;

	default:
		return 2;
	}
}

bool ASoccerAIController::IsBallShieldedByPossessor(
	const ASoccerAICharacter* StealingCharacter,
	const ASoccerCharacterBase* PossessingCharacter,
	const ASoccerBall* SoccerBall
) const
{
	if (
		!IsValid(StealingCharacter) ||
		!IsValid(PossessingCharacter) ||
		!IsValid(SoccerBall)
		)
	{
		return false;
	}

	FVector StealerLocation =
		StealingCharacter->GetActorLocation();

	FVector PossessorLocation =
		PossessingCharacter->GetActorLocation();

	FVector BallLocation =
		SoccerBall->GetActorLocation();

	StealerLocation.Z = 0.0f;
	PossessorLocation.Z = 0.0f;
	BallLocation.Z = 0.0f;

	FVector PossessorToBall =
		BallLocation - PossessorLocation;

	FVector PossessorToStealer =
		StealerLocation - PossessorLocation;

	PossessorToBall.Z = 0.0f;
	PossessorToStealer.Z = 0.0f;

	const FVector PossessorToBallDirection =
		PossessorToBall.GetSafeNormal();

	const FVector PossessorToStealerDirection =
		PossessorToStealer.GetSafeNormal();

	if (
		!PossessorToBallDirection.IsNearlyZero() &&
		!PossessorToStealerDirection.IsNearlyZero()
		)
	{
		const float StealerBehindDot =
			FVector::DotProduct(
				PossessorToBallDirection,
				PossessorToStealerDirection
			);

		// Si la pelota est� de un lado del poseedor y el ladr�n est� del lado contrario,
		// el cuerpo del poseedor est� protegiendo la pelota.
		if (StealerBehindDot <= -AIBallShieldBehindDotThreshold)
		{
			return true;
		}
	}

	const FVector SegmentStart = StealerLocation;
	const FVector SegmentEnd = BallLocation;
	const FVector Point = PossessorLocation;

	const FVector Segment = SegmentEnd - SegmentStart;

	const float SegmentLengthSquared = Segment.SizeSquared();

	if (SegmentLengthSquared <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const float ProjectionAlpha =
		FVector::DotProduct(Point - SegmentStart, Segment) /
		SegmentLengthSquared;

	if (ProjectionAlpha <= 0.0f || ProjectionAlpha >= 1.0f)
	{
		return false;
	}

	const FVector ClosestPointOnSegment =
		SegmentStart + Segment * ProjectionAlpha;

	const float DistanceFromPossessorToStealLine =
		FVector::Dist2D(
			Point,
			ClosestPointOnSegment
		);

	return DistanceFromPossessorToStealLine <= AIBallShieldRadius;
}

bool ASoccerAIController::CanStealPossessedBall(
	const ASoccerAICharacter* StealingCharacter,
	const ASoccerCharacterBase* PossessingCharacter,
	const ASoccerBall* SoccerBall
) const
{
	if (
		!IsValid(StealingCharacter) ||
		!IsValid(PossessingCharacter) ||
		!IsValid(SoccerBall)
		)
	{
		return false;
	}

	FVector StealerLocation = StealingCharacter->GetActorLocation();
	FVector PossessorLocation = PossessingCharacter->GetActorLocation();
	FVector BallLocation = SoccerBall->GetActorLocation();

	StealerLocation.Z = 0.0f;
	PossessorLocation.Z = 0.0f;
	BallLocation.Z = 0.0f;

	const float StealerToBallDistance =
		FVector::Dist2D(StealerLocation, BallLocation);

	const float PossessorToBallDistance =
		FVector::Dist2D(PossessorLocation, BallLocation);

	// Regla fuerte:
	// Si el poseedor est� m�s cerca de la pelota que el ladr�n,
	// el ladr�n no puede robar desde atr�s solo por estar dentro del radio.
	if (
		StealerToBallDistance >
		PossessorToBallDistance + AIBallStealReachAdvantage
		)
	{
		return false;
	}

	FVector PossessorToBall = BallLocation - PossessorLocation;
	FVector PossessorToStealer = StealerLocation - PossessorLocation;

	PossessorToBall.Z = 0.0f;
	PossessorToStealer.Z = 0.0f;

	const FVector PossessorToBallDirection =
		PossessorToBall.GetSafeNormal();

	const FVector PossessorToStealerDirection =
		PossessorToStealer.GetSafeNormal();

	if (
		!PossessorToBallDirection.IsNearlyZero() &&
		!PossessorToStealerDirection.IsNearlyZero()
		)
	{
		const float BallSideVsStealerSide =
			FVector::DotProduct(
				PossessorToBallDirection,
				PossessorToStealerDirection
			);

		// Si la pelota est� delante del poseedor y el ladr�n est� del lado contrario,
		// es robo desde atr�s o desde una diagonal trasera.
		if (BallSideVsStealerSide <= AIBallStealBehindBlockDot)
		{
			return false;
		}
	}

	if (
		IsBallShieldedByPossessor(
			StealingCharacter,
			PossessingCharacter,
			SoccerBall
		)
		)
	{
		return false;
	}

	return true;
}

bool ASoccerAIController::TryPrepareMainActionIfBlocked(
	ASoccerAICharacter* SoccerCharacter,
	ESoccerAIPendingMainAction MainAction,
	const FVector& MainActionTargetLocation,
	ASoccerCharacterBase* OptionalTargetCharacter
)
{
	if (!bUseAIMainActionPreparation)
	{
		return false;
	}

	if (!IsValid(SoccerCharacter))
	{
		return false;
	}

	if (!SoccerCharacter->IsAIPossessingBall())
	{
		return false;
	}

	if (MainAction == ESoccerAIPendingMainAction::None)
	{
		return false;
	}

	if (MainActionTargetLocation.IsNearlyZero())
	{
		return false;
	}

	if (PendingMainActionAfterPreparation != ESoccerAIPendingMainAction::None)
	{
		return false;
	}

	const bool bIsBlocked =
		IsMainActionLineBlockedByOpponent(
			SoccerCharacter,
			SoccerCharacter->GetActorLocation(),
			MainActionTargetLocation,
			AIMainActionPreparationBlockRadius
		);

	if (!bIsBlocked)
	{
		return false;
	}

	const FVector PreparationTargetLocation =
		BuildLateralPreparationTouchTargetLocation(
			SoccerCharacter,
			MainActionTargetLocation
		);

	if (PreparationTargetLocation.IsNearlyZero())
	{
		return false;
	}

	PendingMainActionAfterPreparation = MainAction;
	PendingMainActionTargetLocation = MainActionTargetLocation;
	PendingMainActionTargetCharacter = OptionalTargetCharacter;

	PendingMainActionStartTime =
		GetWorld() != nullptr
		? GetWorld()->GetTimeSeconds()
		: 0.0f;

	if (!TryRegisterAIKickTouchForRules(SoccerCharacter))
	{
		return true;
	}

	SoccerCharacter->StartAIAutoPassToLocation(
		PreparationTargetLocation,
		AIMainActionPreparationHorizontalSpeed,
		AIMainActionPreparationMinTravelTime,
		AIMainActionPreparationMaxTravelTime
	);

	MoveToLocationWithAIMovement(
		ESoccerAIOrder::AttackRunIntoSpace,
		PreparationTargetLocation,
		AIAutoPassFollowAcceptanceRadius,
		false
	);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			1.5f,
			FColor::Yellow,
			TEXT("AI preparacion lateral antes de accion principal")
		);
	}

	return true;
}

bool ASoccerAIController::TryExecutePendingMainActionAfterPreparation(
	ASoccerAICharacter* SoccerCharacter
)
{
	if (PendingMainActionAfterPreparation == ESoccerAIPendingMainAction::None)
	{
		return false;
	}

	if (!IsValid(SoccerCharacter))
	{
		ClearPendingMainActionAfterPreparation();
		return false;
	}

	if (!SoccerCharacter->IsAIPossessingBall())
	{
		return false;
	}

	const float CurrentTime =
		GetWorld() != nullptr
		? GetWorld()->GetTimeSeconds()
		: 0.0f;

	if (
		CurrentTime - PendingMainActionStartTime >
		AIMainActionPreparationTimeout
		)
	{
		ClearPendingMainActionAfterPreparation();
		return false;
	}

	FVector TargetLocation =
		PendingMainActionTargetLocation;

	if (
		PendingMainActionAfterPreparation ==
		ESoccerAIPendingMainAction::PassToTeammate
		)
	{
		ASoccerCharacterBase* TargetCharacter =
			PendingMainActionTargetCharacter.Get();

		if (IsValid(TargetCharacter))
		{
			TargetLocation =
				BuildSimplePassTargetLocation(
					SoccerCharacter,
					TargetCharacter
				);
		}
	}

	if (TargetLocation.IsNearlyZero())
	{
		ClearPendingMainActionAfterPreparation();
		return false;
	}

	const ESoccerAIPendingMainAction ActionToExecute =
		PendingMainActionAfterPreparation;

	ASoccerCharacterBase* PassReceiverToUse =
		PendingMainActionTargetCharacter.Get();

	ClearPendingMainActionAfterPreparation();

	// Todo pase, autopase o remate intencional pasa primero
	// por el sistema de reglas: offside, doble toque, etc.
	if (!TryRegisterAIKickTouchForRules(SoccerCharacter))
	{
		StopMovement();
		return true;
	}

	switch (ActionToExecute)
	{
	case ESoccerAIPendingMainAction::Clearance:
		SoccerCharacter->KickAIBallToTarget(
			TargetLocation,
			AIRecoveryClearanceHorizontalSpeed,
			AIRecoveryClearanceMinTravelTime,
			AIRecoveryClearanceMaxTravelTime
		);
		if (SoccerCharacter->IsAIKickMontageActive())
		{
			StopMovement();
		}
		return true;

	case ESoccerAIPendingMainAction::AutoPass:
		SoccerCharacter->StartAIAutoPassToLocation(
			TargetLocation,
			AIAutoPassHorizontalSpeed,
			AIAutoPassMinTravelTime,
			AIAutoPassMaxTravelTime
		);

		MoveToLocationWithAIMovement(
			ESoccerAIOrder::AttackRunIntoSpace,
			TargetLocation,
			AIAutoPassFollowAcceptanceRadius,
			false
		);

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				1.5f,
				FColor::Cyan,
				TEXT("AI ejecuta autopase despues de preparacion")
			);
		}

		return true;

	case ESoccerAIPendingMainAction::PassToTeammate:
		SoccerCharacter->KickAIBallToTarget(
			TargetLocation,
			AISimplePassHorizontalSpeed,
			AISimplePassMinTravelTime,
			AISimplePassMaxTravelTime
		);

		if (IsValid(PassReceiverToUse))
		{
			MatchManager->RegisterOpenPlayPassIntent(
				SoccerCharacter,
				PassReceiverToUse,
				TargetLocation
			);
		}

		if (SoccerCharacter->IsAIKickMontageActive())
		{
			StopMovement();
		}

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				1.5f,
				FColor::Magenta,
				TEXT("AI ejecuta pase despues de preparacion")
			);
		}

		return true;

	case ESoccerAIPendingMainAction::Shoot:
		TargetLocation = ApplyAIShotExecutionError(
			SoccerCharacter,
			TargetLocation
		);

		SoccerCharacter->KickAIBallToAirTarget(
			TargetLocation,
			AIShotHorizontalSpeed,
			AIShotMinTravelTime,
			AIShotMaxTravelTime
		);

		if (SoccerCharacter->IsAIKickMontageActive())
		{
			StopMovement();
		}

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				1.5f,
				FColor::Red,
				TEXT("AI remata despues de preparacion")
			);
		}

		return true;

	case ESoccerAIPendingMainAction::None:
	default:
		return false;
	}
}

void ASoccerAIController::ClearPendingMainActionAfterPreparation()
{
	PendingMainActionAfterPreparation =
		ESoccerAIPendingMainAction::None;

	PendingMainActionTargetLocation =
		FVector::ZeroVector;

	PendingMainActionTargetCharacter =
		nullptr;

	PendingMainActionStartTime =
		-1000.0f;
}

bool ASoccerAIController::IsMainActionLineBlockedByOpponent(
	const ASoccerAICharacter* SoccerCharacter,
	const FVector& FromLocation,
	const FVector& TargetLocation,
	float BlockRadius
) const
{
	if (!IsValid(SoccerCharacter))
	{
		return false;
	}

	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return false;
	}

	FVector Start = FromLocation;
	FVector End = TargetLocation;

	Start.Z = 0.0f;
	End.Z = 0.0f;

	FVector Segment = End - Start;
	const float SegmentLength = Segment.Size();

	if (SegmentLength < 150.0f)
	{
		return false;
	}

	const FVector Direction = Segment / SegmentLength;

	const float MaxBlockDistance =
		FMath::Min(
			SegmentLength,
			AIMainActionPreparationMaxBlockDistance
		);

	for (TActorIterator<ASoccerCharacterBase> It(World); It; ++It)
	{
		ASoccerCharacterBase* Candidate = *It;

		if (!IsValid(Candidate))
		{
			continue;
		}

		if (Candidate == SoccerCharacter)
		{
			continue;
		}

		if (Candidate->GetTeam() == SoccerCharacter->GetTeam())
		{
			continue;
		}

		FVector CandidateLocation = Candidate->GetActorLocation();
		CandidateLocation.Z = 0.0f;

		const float ProjectionDistance =
			FVector::DotProduct(
				CandidateLocation - Start,
				Direction
			);

		if (ProjectionDistance < AIMainActionPreparationMinBlockDistance)
		{
			continue;
		}

		if (ProjectionDistance > MaxBlockDistance)
		{
			continue;
		}

		const FVector ClosestPointOnLine =
			Start + Direction * ProjectionDistance;

		const float DistanceToLine =
			FVector::Dist2D(
				CandidateLocation,
				ClosestPointOnLine
			);

		if (DistanceToLine <= BlockRadius)
		{
			return true;
		}
	}

	return false;
}

FVector ASoccerAIController::BuildLateralPreparationTouchTargetLocation(
	const ASoccerAICharacter* SoccerCharacter,
	const FVector& MainActionTargetLocation
) const
{
	if (!IsValid(SoccerCharacter))
	{
		return FVector::ZeroVector;
	}

	FVector MainDirection =
		MainActionTargetLocation - SoccerCharacter->GetActorLocation();

	MainDirection.Z = 0.0f;
	MainDirection = MainDirection.GetSafeNormal();

	if (MainDirection.IsNearlyZero())
	{
		return FVector::ZeroVector;
	}

	FVector SideDirection =
		FVector::CrossProduct(
			FVector::UpVector,
			MainDirection
		);

	SideDirection.Z = 0.0f;
	SideDirection = SideDirection.GetSafeNormal();

	if (SideDirection.IsNearlyZero())
	{
		return FVector::ZeroVector;
	}

	FVector CandidateA =
		SoccerCharacter->GetActorLocation()
		+ SideDirection * AIMainActionPreparationTouchDistance;

	FVector CandidateB =
		SoccerCharacter->GetActorLocation()
		- SideDirection * AIMainActionPreparationTouchDistance;

	CandidateA.Z = SoccerCharacter->GetActorLocation().Z;
	CandidateB.Z = SoccerCharacter->GetActorLocation().Z;

	UWorld* World = GetWorld();

	if (World != nullptr)
	{
		UNavigationSystemV1* NavigationSystem =
			FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);

		if (NavigationSystem != nullptr)
		{
			FNavLocation ProjectedA;

			if (
				NavigationSystem->ProjectPointToNavigation(
					CandidateA,
					ProjectedA,
					FVector(350.0f, 350.0f, 350.0f)
				)
				)
			{
				CandidateA = ProjectedA.Location;
			}

			FNavLocation ProjectedB;

			if (
				NavigationSystem->ProjectPointToNavigation(
					CandidateB,
					ProjectedB,
					FVector(350.0f, 350.0f, 350.0f)
				)
				)
			{
				CandidateB = ProjectedB.Location;
			}
		}
	}

	const bool bCandidateABlocked =
		IsMainActionLineBlockedByOpponent(
			SoccerCharacter,
			CandidateA,
			MainActionTargetLocation,
			AIMainActionPreparationBlockRadius * 0.75f
		);

	const bool bCandidateBBlocked =
		IsMainActionLineBlockedByOpponent(
			SoccerCharacter,
			CandidateB,
			MainActionTargetLocation,
			AIMainActionPreparationBlockRadius * 0.75f
		);

	if (!bCandidateABlocked && bCandidateBBlocked)
	{
		return CandidateA;
	}

	if (bCandidateABlocked && !bCandidateBBlocked)
	{
		return CandidateB;
	}

	if (!bCandidateABlocked && !bCandidateBBlocked)
	{
		// Si ambos lados sirven, elegimos uno al azar para que no sea tan rob�tico.
		return FMath::RandBool()
			? CandidateA
			: CandidateB;
	}

	// Si ambos siguen bloqueados, igual elegimos un costado.
	// Puede abrir algo de �ngulo aunque no sea perfecto.
	return FMath::RandBool()
		? CandidateA
		: CandidateB;
}

bool ASoccerAIController::ConfigureAIAerialHeaderDecisionForIntent(
	ASoccerAICharacter* SoccerCharacter,
	ASoccerBall* SoccerBall,
	ESoccerAerialActionIntent Intent
)
{
	if (!IsValid(SoccerCharacter) || !IsValid(SoccerBall))
	{
		return false;
	}

	SoccerCharacter->ClearAIAerialHeaderDecision();

	switch (Intent)
	{
	case ESoccerAerialActionIntent::ActiveHeader:
		return BuildAIOffensiveHeaderDecision(
			SoccerCharacter,
			SoccerBall
		);

	case ESoccerAerialActionIntent::DefensiveBlock:
		return BuildAIDefensiveHeaderDecision(
			SoccerCharacter,
			SoccerBall
		);

	default:
		return true;
	}
}

bool ASoccerAIController::BuildAIOffensiveHeaderDecision(
	ASoccerAICharacter* SoccerCharacter,
	ASoccerBall* SoccerBall
)
{
	if (
		!IsValid(SoccerCharacter) ||
		!IsValid(SoccerBall) ||
		!IsValid(MatchManager) ||
		!bUseAIOffensiveHeaders
	)
	{
		return false;
	}

	const ESoccerPlayerRole PlayerRole =
		SoccerCharacter->GetPlayerRole();
	const bool bRoleCanAttackHeader =
		PlayerRole == ESoccerPlayerRole::Forward ||
		PlayerRole == ESoccerPlayerRole::Midfielder ||
		(
			bAllowDefenderOffensiveHeaders &&
			PlayerRole == ESoccerPlayerRole::Defender
		);

	if (!bRoleCanAttackHeader)
	{
		return false;
	}

	const FVector GoalCenter =
		MatchManager->GetShotTargetLocation(SoccerCharacter);
	const float GoalDistance = FVector::Dist2D(
		SoccerCharacter->GetActorLocation(),
		GoalCenter
	);

	if (GoalDistance <= AIAerialActiveHeaderMaximumGoalDistance)
	{
		const FVector IntendedShotTarget =
			BuildAIShotTargetLocation(SoccerCharacter);

		if (!IntendedShotTarget.IsNearlyZero())
		{
			FVector ExecutedShotTarget = IntendedShotTarget;
			float ExecutedShotHorizontalSpeed =
				FMath::Max(0.0f, AIAerialHeaderShotHorizontalSpeed);
			float DirectionErrorDegrees = 0.0f;
			float PowerErrorFraction = 0.0f;

			if (bUseAIAerialHeaderShotExecutionError)
			{
				/*
				 * Freeze one execution sample when the header decision is made.
				 * Two uniform samples averaged together produce a triangular
				 * distribution: small errors are common, extremes are rare.
				 */
				const auto SampleTriangularSignedUnit = []()
					{
						return 0.5f * (
							FMath::FRandRange(-1.0f, 1.0f) +
							FMath::FRandRange(-1.0f, 1.0f)
						);
					};

				const float AerialExecutionErrorMultiplier =
					GetAerialAbilityHeaderExecutionErrorMultiplier(
						SoccerCharacter
					);

				DirectionErrorDegrees =
					SampleTriangularSignedUnit() *
					FMath::Max(0.0f, AIAerialHeaderShotDirectionErrorDegrees) *
					AerialExecutionErrorMultiplier;

				PowerErrorFraction =
					SampleTriangularSignedUnit() *
					FMath::Clamp(
						AIAerialHeaderShotPowerErrorFraction *
							AerialExecutionErrorMultiplier,
						0.0f,
						0.50f
					);

				FVector HorizontalShotDirection =
					IntendedShotTarget - SoccerCharacter->GetActorLocation();
				HorizontalShotDirection.Z = 0.0f;

				const float HorizontalTargetDistance =
					HorizontalShotDirection.Size();

				if (HorizontalTargetDistance > KINDA_SMALL_NUMBER)
				{
					HorizontalShotDirection /= HorizontalTargetDistance;
					HorizontalShotDirection =
						HorizontalShotDirection.RotateAngleAxis(
							DirectionErrorDegrees,
							FVector::UpVector
						).GetSafeNormal();

					ExecutedShotTarget =
						SoccerCharacter->GetActorLocation() +
						HorizontalShotDirection * HorizontalTargetDistance;
					ExecutedShotTarget.Z = IntendedShotTarget.Z;
				}

				ExecutedShotHorizontalSpeed *=
					FMath::Max(0.0f, 1.0f + PowerErrorFraction);
			}

			SoccerCharacter->ConfigureAIAerialHeaderDecision(
				ESoccerAIAerialHeaderTactic::ShotAtGoal,
				ExecutedShotTarget,
				ExecutedShotHorizontalSpeed
			);

			if (ASoccerDebugManager::IsEnabled(this, ESoccerDebugCategory::Aerial))
			{
				UE_LOG(
					LogTemp,
					Log,
					TEXT(
						"%s aerial tactic: header shot | dir error %.2f deg | power error %.1f%% | speed %.1f."
					),
					*SoccerCharacter->GetName(),
					DirectionErrorDegrees,
					PowerErrorFraction * 100.0f,
					ExecutedShotHorizontalSpeed
				);
			}

			return true;
		}
	}

	if (bUseAIOffensiveHeaderPasses)
	{
		FVector PassTarget = FVector::ZeroVector;
		float PassScore = -TNumericLimits<float>::Max();

		ASoccerCharacterBase* Teammate =
			FindBestAerialHeaderTeammate(
				SoccerCharacter,
				SoccerBall,
				false,
				PassTarget,
				PassScore
			);

		if (
			IsValid(Teammate) &&
			PassScore >= AIAerialHeaderMinimumTeammateScore
		)
		{
			FVector ExecutedPassTarget = PassTarget;
			float ExecutedPassHorizontalSpeed =
				FMath::Max(0.0f, AIAerialHeaderPassHorizontalSpeed);
			float DirectionErrorDegrees = 0.0f;
			float PowerErrorFraction = 0.0f;

			ApplyAIAerialHeaderPassExecutionError(
				SoccerCharacter,
				PassTarget,
				AIAerialHeaderPassHorizontalSpeed,
				ExecutedPassTarget,
				ExecutedPassHorizontalSpeed,
				DirectionErrorDegrees,
				PowerErrorFraction
			);

			SoccerCharacter->ConfigureAIAerialHeaderDecision(
				ESoccerAIAerialHeaderTactic::PassToTeammate,
				ExecutedPassTarget,
				ExecutedPassHorizontalSpeed
			);

			if (ASoccerDebugManager::IsEnabled(this, ESoccerDebugCategory::Aerial))
			{
				UE_LOG(
					LogTemp,
					Log,
					TEXT(
						"%s aerial tactic: header pass to %s (score %.2f) | dir error %.2f deg | power error %.1f%% | speed %.1f."
					),
					*SoccerCharacter->GetName(),
					*Teammate->GetName(),
					PassScore,
					DirectionErrorDegrees,
					PowerErrorFraction * 100.0f,
					ExecutedPassHorizontalSpeed
				);
			}

			return true;
		}
	}

	if (bUseAIOffensiveHeaderProlongations)
	{
		const FVector ProlongTarget =
			BuildAIOffensiveHeaderProlongTarget(
				SoccerCharacter,
				SoccerBall
			);

		if (!ProlongTarget.IsNearlyZero())
		{
			SoccerCharacter->ConfigureAIAerialHeaderDecision(
				ESoccerAIAerialHeaderTactic::ProlongAttack,
				ProlongTarget,
				AIAerialHeaderProlongHorizontalSpeed
			);

			if (ASoccerDebugManager::IsEnabled(this, ESoccerDebugCategory::Aerial))
			{
				UE_LOG(
					LogTemp,
					Log,
					TEXT("%s aerial tactic: prolong attack."),
					*SoccerCharacter->GetName()
				);
			}

			return true;
		}
	}

	return false;
}

bool ASoccerAIController::BuildAIDefensiveHeaderDecision(
	ASoccerAICharacter* SoccerCharacter,
	ASoccerBall* SoccerBall
)
{
	if (
		!IsValid(SoccerCharacter) ||
		!IsValid(SoccerBall) ||
		!IsValid(MatchManager)
	)
	{
		return false;
	}

	const FVector BallLocation = SoccerBall->GetActorLocation();
	const FVector OwnGoal =
		MatchManager->GetOwnGoalCenterLocation(
			SoccerCharacter->GetTeam()
		);
	const float OwnGoalDistance =
		FVector::Dist2D(BallLocation, OwnGoal);
	const float NearestOpponentDistance =
		GetNearestOpponentDistanceToAerialLocation(
			SoccerCharacter,
			BallLocation
		);
	const bool bUnderPressure =
		NearestOpponentDistance <=
		FMath::Max(0.0f, AIAerialDefensivePressureDistance);
	const bool bGoalDanger =
		OwnGoalDistance <=
		FMath::Max(
			0.0f,
			AIAerialDefensiveClearanceOwnGoalDistance
		);

	if (
		bUseAIDefensiveHeaderPasses &&
		!bUnderPressure &&
		!bGoalDanger
	)
	{
		FVector PassTarget = FVector::ZeroVector;
		float PassScore = -TNumericLimits<float>::Max();

		ASoccerCharacterBase* Teammate =
			FindBestAerialHeaderTeammate(
				SoccerCharacter,
				SoccerBall,
				true,
				PassTarget,
				PassScore
			);

		if (
			IsValid(Teammate) &&
			PassScore >= AIAerialHeaderMinimumTeammateScore
		)
		{
			FVector ExecutedPassTarget = PassTarget;
			float ExecutedPassHorizontalSpeed =
				FMath::Max(0.0f, AIAerialDefensivePassHorizontalSpeed);
			float DirectionErrorDegrees = 0.0f;
			float PowerErrorFraction = 0.0f;

			ApplyAIAerialHeaderPassExecutionError(
				SoccerCharacter,
				PassTarget,
				AIAerialDefensivePassHorizontalSpeed,
				ExecutedPassTarget,
				ExecutedPassHorizontalSpeed,
				DirectionErrorDegrees,
				PowerErrorFraction
			);

			SoccerCharacter->ConfigureAIAerialHeaderDecision(
				ESoccerAIAerialHeaderTactic::DefensivePass,
				ExecutedPassTarget,
				ExecutedPassHorizontalSpeed
			);

			if (ASoccerDebugManager::IsEnabled(this, ESoccerDebugCategory::Aerial))
			{
				UE_LOG(
					LogTemp,
					Log,
					TEXT(
						"%s aerial tactic: defensive header pass to %s (score %.2f) | dir error %.2f deg | power error %.1f%% | speed %.1f."
					),
					*SoccerCharacter->GetName(),
					*Teammate->GetName(),
					PassScore,
					DirectionErrorDegrees,
					PowerErrorFraction * 100.0f,
					ExecutedPassHorizontalSpeed
				);
			}

			return true;
		}
	}

	const FVector ClearanceTarget =
		BuildAIDefensiveHeaderClearanceTarget(
			SoccerCharacter,
			SoccerBall
		);

	if (ClearanceTarget.IsNearlyZero())
	{
		return false;
	}

	SoccerCharacter->ConfigureAIAerialHeaderDecision(
		ESoccerAIAerialHeaderTactic::DefensiveClearance,
		ClearanceTarget,
		AIAerialDefensiveClearanceHorizontalSpeed
	);

	if (ASoccerDebugManager::IsEnabled(this, ESoccerDebugCategory::Aerial))
	{
		UE_LOG(
			LogTemp,
			Log,
			TEXT("%s aerial tactic: defensive clearance (pressure %s, goal danger %s)."),
			*SoccerCharacter->GetName(),
			bUnderPressure ? TEXT("yes") : TEXT("no"),
			bGoalDanger ? TEXT("yes") : TEXT("no")
		);
	}

	return true;
}

float ASoccerAIController::GetAerialAbilityHeaderExecutionErrorMultiplier(
	const ASoccerCharacterBase* SoccerCharacter
) const
{
	if (!IsValid(SoccerCharacter) || !SoccerCharacter->HasPlayerProfile())
	{
		return 1.0f;
	}

	return FMath::Lerp(
		AerialAbilityHeaderExecutionErrorMultiplierAtZero,
		AerialAbilityHeaderExecutionErrorMultiplierAtHundred,
		SoccerCharacter->GetPlayerProfileAerialAbilityAlpha()
	);
}

void ASoccerAIController::ApplyAIAerialHeaderPassExecutionError(
	const ASoccerAICharacter* SoccerCharacter,
	const FVector& IntendedTarget,
	float IntendedHorizontalSpeed,
	FVector& OutExecutedTarget,
	float& OutExecutedHorizontalSpeed,
	float& OutDirectionErrorDegrees,
	float& OutPowerErrorFraction
) const
{
	OutExecutedTarget = IntendedTarget;
	OutExecutedHorizontalSpeed = FMath::Max(0.0f, IntendedHorizontalSpeed);
	OutDirectionErrorDegrees = 0.0f;
	OutPowerErrorFraction = 0.0f;

	if (
		!bUseAIAerialHeaderPassExecutionError ||
		!IsValid(SoccerCharacter)
	)
	{
		return;
	}

	/*
	 * Freeze one execution sample when the header-pass decision is made.
	 * Averaging two uniform signed samples yields a triangular distribution:
	 * small mistakes are common and maximum mistakes are comparatively rare.
	 */
	const auto SampleTriangularSignedUnit = []()
		{
			return 0.5f * (
				FMath::FRandRange(-1.0f, 1.0f) +
				FMath::FRandRange(-1.0f, 1.0f)
			);
		};

	const float AerialExecutionErrorMultiplier =
		GetAerialAbilityHeaderExecutionErrorMultiplier(SoccerCharacter);

	OutDirectionErrorDegrees =
		SampleTriangularSignedUnit() *
		FMath::Max(0.0f, AIAerialHeaderPassDirectionErrorDegrees) *
		AerialExecutionErrorMultiplier;

	OutPowerErrorFraction =
		SampleTriangularSignedUnit() *
		FMath::Clamp(
			AIAerialHeaderPassPowerErrorFraction *
				AerialExecutionErrorMultiplier,
			0.0f,
			0.50f
		);

	FVector HorizontalPassDirection =
		IntendedTarget - SoccerCharacter->GetActorLocation();
	HorizontalPassDirection.Z = 0.0f;

	const float HorizontalTargetDistance =
		HorizontalPassDirection.Size();

	if (HorizontalTargetDistance > KINDA_SMALL_NUMBER)
	{
		HorizontalPassDirection /= HorizontalTargetDistance;
		HorizontalPassDirection =
			HorizontalPassDirection.RotateAngleAxis(
				OutDirectionErrorDegrees,
				FVector::UpVector
			).GetSafeNormal();

		FVector ErrorTarget =
			SoccerCharacter->GetActorLocation() +
			HorizontalPassDirection * HorizontalTargetDistance;
		ErrorTarget.Z = IntendedTarget.Z;

		OutExecutedTarget =
			ClampAIAerialHeaderTargetInsideField(ErrorTarget);
	}

	OutExecutedHorizontalSpeed *=
		FMath::Max(0.0f, 1.0f + OutPowerErrorFraction);
}

ASoccerCharacterBase*
ASoccerAIController::FindBestAerialHeaderTeammate(
	const ASoccerAICharacter* SoccerCharacter,
	const ASoccerBall* SoccerBall,
	bool bDefensiveDecision,
	FVector& OutTargetLocation,
	float& OutScore
) const
{
	OutTargetLocation = FVector::ZeroVector;
	OutScore = -TNumericLimits<float>::Max();

	if (
		!IsValid(SoccerCharacter) ||
		!IsValid(SoccerBall) ||
		!IsValid(MatchManager)
	)
	{
		return nullptr;
	}

	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return nullptr;
	}

	const FVector StartLocation = SoccerBall->GetActorLocation();
	const FVector OpponentGoal =
		MatchManager->GetShotTargetLocation(SoccerCharacter);
	const float SelfGoalDistance =
		FVector::Dist2D(StartLocation, OpponentGoal);
	const float MinimumDistance =
		FMath::Max(0.0f, AIAerialHeaderPassMinimumDistance);
	const float MaximumDistance =
		FMath::Max(
			MinimumDistance + 1.0f,
			AIAerialHeaderPassMaximumDistance
		);

	ASoccerCharacterBase* BestTeammate = nullptr;

	for (TActorIterator<ASoccerCharacterBase> It(World); It; ++It)
	{
		ASoccerCharacterBase* Candidate = *It;

		if (
			!IsValid(Candidate) ||
			Candidate == SoccerCharacter ||
			Candidate->GetTeam() != SoccerCharacter->GetTeam() ||
			Candidate->GetPlayerRole() == ESoccerPlayerRole::Goalkeeper ||
			Candidate->IsAerialActionLocked()
		)
		{
			continue;
		}

		const float CandidateDistance =
			FVector::Dist2D(
				StartLocation,
				Candidate->GetActorLocation()
			);

		if (
			CandidateDistance < MinimumDistance ||
			CandidateDistance > MaximumDistance
		)
		{
			continue;
		}

		FVector Lead = Candidate->GetVelocity();
		Lead.Z = 0.0f;
		Lead *= FMath::Max(0.0f, AIAerialHeaderTeammateLeadTime);

		const float MaximumLead =
			FMath::Max(0.0f, AIAerialHeaderMaximumLeadDistance);

		if (Lead.Size2D() > MaximumLead && MaximumLead > 0.0f)
		{
			Lead = Lead.GetSafeNormal2D() * MaximumLead;
		}

		FVector CandidateTarget =
			Candidate->GetActorLocation() + Lead;
		CandidateTarget.Z = SoccerCharacter->GetActorLocation().Z;
		CandidateTarget =
			ClampAIAerialHeaderTargetInsideField(CandidateTarget);

		const float NearestOpponentDistance =
			GetNearestOpponentDistanceToAerialLocation(
				SoccerCharacter,
				CandidateTarget
			);

		if (
			NearestOpponentDistance <
			FMath::Max(
				0.0f,
				AIAerialHeaderReceiverCriticalPressureDistance
			)
		)
		{
			continue;
		}

		if (
			IsAerialHeaderPassLaneBlocked(
				SoccerCharacter,
				StartLocation,
				CandidateTarget
			)
		)
		{
			continue;
		}

		const float DistanceAlpha = FMath::Clamp(
			(CandidateDistance - MinimumDistance) /
				(MaximumDistance - MinimumDistance),
			0.0f,
			1.0f
		);
		const float DistanceScore =
			1.0f - FMath::Abs(DistanceAlpha - 0.50f) * 2.0f;
		const float SpaceScore = FMath::Clamp(
			NearestOpponentDistance /
				FMath::Max(
					1.0f,
					AIAerialHeaderReceiverSpaceReferenceDistance
				),
			0.0f,
			1.0f
		);
		const float CandidateGoalDistance =
			FVector::Dist2D(CandidateTarget, OpponentGoal);
		const float ProgressScore = FMath::Clamp(
			(SelfGoalDistance - CandidateGoalDistance) / 1600.0f,
			-1.0f,
			1.0f
		);

		float Score =
			SpaceScore * 0.50f +
			DistanceScore * 0.25f +
			ProgressScore *
				(bDefensiveDecision ? 0.25f : 0.35f);

		if (Cast<AThirdPersonCppCharacter>(Candidate) != nullptr)
		{
			Score += 0.05f;
		}

		if (Score > OutScore)
		{
			OutScore = Score;
			OutTargetLocation = CandidateTarget;
			BestTeammate = Candidate;
		}
	}

	return BestTeammate;
}

float ASoccerAIController::GetNearestOpponentDistanceToAerialLocation(
	const ASoccerAICharacter* SoccerCharacter,
	const FVector& Location
) const
{
	if (!IsValid(SoccerCharacter))
	{
		return TNumericLimits<float>::Max();
	}

	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return TNumericLimits<float>::Max();
	}

	float BestDistance = TNumericLimits<float>::Max();

	for (TActorIterator<ASoccerCharacterBase> It(World); It; ++It)
	{
		const ASoccerCharacterBase* Candidate = *It;

		if (
			!IsValid(Candidate) ||
			Candidate->GetTeam() == SoccerCharacter->GetTeam()
		)
		{
			continue;
		}

		BestDistance = FMath::Min(
			BestDistance,
			FVector::Dist2D(
				Candidate->GetActorLocation(),
				Location
			)
		);
	}

	return BestDistance;
}

bool ASoccerAIController::IsAerialHeaderPassLaneBlocked(
	const ASoccerAICharacter* SoccerCharacter,
	const FVector& StartLocation,
	const FVector& TargetLocation
) const
{
	if (!IsValid(SoccerCharacter))
	{
		return true;
	}

	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return true;
	}

	FVector Segment = TargetLocation - StartLocation;
	Segment.Z = 0.0f;
	const float SegmentLengthSquared = Segment.SizeSquared();

	if (SegmentLengthSquared <= KINDA_SMALL_NUMBER)
	{
		return true;
	}

	const float LaneHalfWidth =
		FMath::Max(0.0f, AIAerialHeaderPassLaneHalfWidth);

	for (TActorIterator<ASoccerCharacterBase> It(World); It; ++It)
	{
		const ASoccerCharacterBase* Candidate = *It;

		if (
			!IsValid(Candidate) ||
			Candidate->GetTeam() == SoccerCharacter->GetTeam()
		)
		{
			continue;
		}

		FVector ToOpponent =
			Candidate->GetActorLocation() - StartLocation;
		ToOpponent.Z = 0.0f;

		const float Alpha = FVector::DotProduct(
			ToOpponent,
			Segment
		) / SegmentLengthSquared;

		if (Alpha <= 0.08f || Alpha >= 0.92f)
		{
			continue;
		}

		const FVector ClosestPoint =
			StartLocation + Segment * Alpha;
		const float DistanceToLane =
			FVector::Dist2D(
				Candidate->GetActorLocation(),
				ClosestPoint
			);

		if (DistanceToLane <= LaneHalfWidth)
		{
			return true;
		}
	}

	return false;
}

FVector ASoccerAIController::BuildAIOffensiveHeaderProlongTarget(
	const ASoccerAICharacter* SoccerCharacter,
	const ASoccerBall* SoccerBall
) const
{
	if (
		!IsValid(SoccerCharacter) ||
		!IsValid(SoccerBall) ||
		!IsValid(MatchManager)
	)
	{
		return FVector::ZeroVector;
	}

	const FVector StartLocation = SoccerBall->GetActorLocation();
	FVector AttackDirection =
		MatchManager->GetShotTargetLocation(SoccerCharacter) -
		StartLocation;
	AttackDirection.Z = 0.0f;
	AttackDirection = AttackDirection.GetSafeNormal();

	if (AttackDirection.IsNearlyZero())
	{
		AttackDirection =
			SoccerCharacter->GetActorForwardVector().GetSafeNormal2D();
	}

	if (AttackDirection.IsNearlyZero())
	{
		return FVector::ZeroVector;
	}

	FVector TargetLocation =
		StartLocation +
		AttackDirection *
			FMath::Max(100.0f, AIAerialOffensiveProlongDistance);
	TargetLocation.Z = SoccerCharacter->GetActorLocation().Z;

	return ClampAIAerialHeaderTargetInsideField(TargetLocation);
}

FVector ASoccerAIController::BuildAIDefensiveHeaderClearanceTarget(
	const ASoccerAICharacter* SoccerCharacter,
	const ASoccerBall* SoccerBall
) const
{
	if (
		!IsValid(SoccerCharacter) ||
		!IsValid(SoccerBall) ||
		!IsValid(MatchManager)
	)
	{
		return FVector::ZeroVector;
	}

	const FVector StartLocation = SoccerBall->GetActorLocation();
	const FVector OwnGoal =
		MatchManager->GetOwnGoalCenterLocation(
			SoccerCharacter->GetTeam()
		);
	const FVector OpponentGoal =
		MatchManager->GetShotTargetLocation(SoccerCharacter);

	FVector AttackDirection = OpponentGoal - OwnGoal;
	AttackDirection.Z = 0.0f;
	AttackDirection = AttackDirection.GetSafeNormal();

	if (AttackDirection.IsNearlyZero())
	{
		AttackDirection =
			SoccerCharacter->GetActorForwardVector().GetSafeNormal2D();
	}

	if (AttackDirection.IsNearlyZero())
	{
		return FVector::ZeroVector;
	}

	FVector SideDirection = FVector::CrossProduct(
		FVector::UpVector,
		AttackDirection
	);
	SideDirection.Z = 0.0f;
	SideDirection = SideDirection.GetSafeNormal();

	if (SideDirection.IsNearlyZero())
	{
		SideDirection = FVector::RightVector;
	}

	const float ForwardDistance =
		FMath::Max(
			100.0f,
			AIAerialDefensiveClearanceForwardDistance
		);
	const float LateralDistance =
		FMath::Max(
			100.0f,
			AIAerialDefensiveClearanceLateralDistance
		);

	FVector CandidatePositive =
		StartLocation +
		AttackDirection * ForwardDistance +
		SideDirection * LateralDistance;
	FVector CandidateNegative =
		StartLocation +
		AttackDirection * ForwardDistance -
		SideDirection * LateralDistance;

	CandidatePositive.Z = SoccerCharacter->GetActorLocation().Z;
	CandidateNegative.Z = SoccerCharacter->GetActorLocation().Z;
	CandidatePositive =
		ClampAIAerialHeaderTargetInsideField(CandidatePositive);
	CandidateNegative =
		ClampAIAerialHeaderTargetInsideField(CandidateNegative);

	const float PositiveSpace =
		GetNearestOpponentDistanceToAerialLocation(
			SoccerCharacter,
			CandidatePositive
		);
	const float NegativeSpace =
		GetNearestOpponentDistanceToAerialLocation(
			SoccerCharacter,
			CandidateNegative
		);

	if (!FMath::IsNearlyEqual(PositiveSpace, NegativeSpace, 25.0f))
	{
		return PositiveSpace > NegativeSpace
			? CandidatePositive
			: CandidateNegative;
	}

	const ASoccerField* SoccerField = IsValid(MatchManager)
		? MatchManager->GetSoccerField()
		: nullptr;
	const FVector FieldCenter = IsValid(SoccerField)
		? SoccerField->GetPitchCenterWorldLocation()
		: FVector::ZeroVector;
	const float CurrentSide = FVector::DotProduct(
		StartLocation - FieldCenter,
		SideDirection
	);

	return CurrentSide >= 0.0f
		? CandidatePositive
		: CandidateNegative;
}

FVector ASoccerAIController::ClampAIAerialHeaderTargetInsideField(
	const FVector& TargetLocation
) const
{
	const float RequestedInset = FMath::Max(
		0.0f,
		AIAerialHeaderTargetFieldInset
	);

	if (IsValid(MatchManager))
	{
		const ASoccerField* SoccerField = MatchManager->GetSoccerField();
		if (IsValid(SoccerField))
		{
			return SoccerField->ClampWorldLocationInsidePitch(
				TargetLocation,
				RequestedInset
			);
		}
	}

	return SoccerFieldDimensions::ClampLocationInsidePitch(
		TargetLocation,
		RequestedInset
	);
}

bool ASoccerAIController::IsAerialContestCandidateEligible(
	const ASoccerAICharacter* Candidate,
	ESoccerAerialActionIntent Intent
) const
{
	if (!IsValid(Candidate))
	{
		return false;
	}

	if (
		Candidate->GetPlayerRole() == ESoccerPlayerRole::Goalkeeper &&
		!bAllowGoalkeeperAerialActions
	)
	{
		return false;
	}

	if (Intent == ESoccerAerialActionIntent::ActiveHeader)
	{
		const ESoccerPlayerRole PlayerRole = Candidate->GetPlayerRole();
		const bool bRoleCanAttackHeader =
			PlayerRole == ESoccerPlayerRole::Forward ||
			PlayerRole == ESoccerPlayerRole::Midfielder ||
			(
				bAllowDefenderOffensiveHeaders &&
				PlayerRole == ESoccerPlayerRole::Defender
			);

		if (!bRoleCanAttackHeader)
		{
			return false;
		}
	}

	if (Candidate->IsAerialActionQueuedOrPlaying())
	{
		return true;
	}

	if (!IsValid(MatchManager))
	{
		return true;
	}

	const ESoccerAIOrder Order =
		MatchManager->GetAIOrderForCharacter(Candidate);

	return
		Order == ESoccerAIOrder::ChaseBall ||
		Order == ESoccerAIOrder::PressBall ||
		Order == ESoccerAIOrder::AttackRecoverBall;
}

float ASoccerAIController::ScoreAerialContestPlan(
	const ASoccerCharacterBase* Candidate,
	const FSoccerAerialInterceptionPlan& Plan
) const
{
	if (!IsValid(Candidate) || !Plan.bValid)
	{
		return TNumericLimits<float>::Max();
	}

	const float LateArrival = FMath::Max(
		0.0f,
		-Plan.PreparationArrivalMargin
	);
	const float UsefulPositiveMargin = FMath::Clamp(
		Plan.PreparationArrivalMargin,
		0.0f,
		0.40f
	);

	float Score =
		Plan.BallArrivalTime *
			FMath::Max(0.0f, AIAerialContestBallArrivalWeight) +
		Plan.PlayerArrivalTime *
			FMath::Max(0.0f, AIAerialContestPlayerArrivalWeight) +
		LateArrival *
			FMath::Max(0.0f, AIAerialContestLateArrivalPenaltyWeight) +
		FMath::Abs(Plan.ContactTimingOffsetFromIdeal) *
			FMath::Max(0.0f, AIAerialContestTimingErrorWeight) -
		UsefulPositiveMargin *
			FMath::Max(0.0f, AIAerialContestPositiveMarginBonusWeight);

	if (Candidate->HasPlayerProfile())
	{
		Score += FMath::Lerp(
			AerialAbilityContestPlanScoreAdjustmentAtZero,
			AerialAbilityContestPlanScoreAdjustmentAtHundred,
			Candidate->GetPlayerProfileAerialAbilityAlpha()
		);
		Score += FMath::Lerp(
			StrengthContestPlanScoreAdjustmentAtZero,
			StrengthContestPlanScoreAdjustmentAtHundred,
			Candidate->GetPlayerProfileStrengthAlpha()
		);
	}

	switch (Candidate->GetAerialActionPhase())
	{
	case ESoccerAerialActionPhase::Approaching:
		Score -= FMath::Max(
			0.0f,
			AIAerialContestApproachingCommitmentBonus
		);
		break;

	case ESoccerAerialActionPhase::WaitingToStart:
		Score -= FMath::Max(
			0.0f,
			AIAerialContestWaitingCommitmentBonus
		);
		break;

	case ESoccerAerialActionPhase::Playing:
		Score -= FMath::Max(
			0.0f,
			AIAerialContestPlayingCommitmentBonus
		);
		break;

	default:
		break;
	}

	const AThirdPersonCppCharacter* HumanCandidate =
		Cast<AThirdPersonCppCharacter>(Candidate);

	if (
		IsValid(HumanCandidate) &&
		HumanCandidate->IsHumanJumpHeaderRequestActive()
	)
	{
		Score -= FMath::Max(
			0.0f,
			AIAerialContestHumanRequestBonus
		);
	}

	return Score;
}

bool ASoccerAIController::FindPrimaryAerialContestCharacter(
	ASoccerAICharacter* SoccerCharacter,
	ASoccerBall* SoccerBall,
	ESoccerAerialActionIntent Intent,
	const FSoccerAerialInterceptionPlan& SelfPlan,
	ASoccerCharacterBase*& OutPrimaryCharacter,
	FSoccerAerialInterceptionPlan& OutPrimaryPlan,
	float& OutSelfScore,
	float& OutPrimaryScore,
	int32& OutCandidateCount
) const
{
	OutPrimaryCharacter = nullptr;
	OutPrimaryPlan = FSoccerAerialInterceptionPlan();
	OutSelfScore = TNumericLimits<float>::Max();
	OutPrimaryScore = TNumericLimits<float>::Max();
	OutCandidateCount = 0;

	if (
		!IsValid(SoccerCharacter) ||
		!IsValid(SoccerBall) ||
		!SelfPlan.bValid ||
		SelfPlan.Ball != SoccerBall
	)
	{
		return false;
	}

	const ESoccerAerialActionType ExpectedAction =
		Intent == ESoccerAerialActionIntent::ActiveHeader
		? ESoccerAerialActionType::JumpHeaderKick
		: Intent == ESoccerAerialActionIntent::DefensiveBlock
			? ESoccerAerialActionType::JumpHeaderBlock
			: ESoccerAerialActionType::None;

	if (
		ExpectedAction == ESoccerAerialActionType::None ||
		SelfPlan.ActionType != ExpectedAction
	)
	{
		return false;
	}

	OutPrimaryCharacter = SoccerCharacter;
	OutPrimaryPlan = SelfPlan;
	OutSelfScore = ScoreAerialContestPlan(SoccerCharacter, SelfPlan);
	OutPrimaryScore = OutSelfScore;
	OutCandidateCount = 1;

	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return true;
	}

	for (TActorIterator<ASoccerCharacterBase> It(World); It; ++It)
	{
		ASoccerCharacterBase* Candidate = *It;

		if (
			!IsValid(Candidate) ||
			Candidate == SoccerCharacter ||
			Candidate->GetTeam() != SoccerCharacter->GetTeam()
		)
		{
			continue;
		}

		if (
			Candidate->IsAerialActionQueuedOrPlaying() &&
			Candidate->GetAerialActionBall() != SoccerBall
		)
		{
			continue;
		}

		FSoccerAerialInterceptionPlan CandidatePlan;
		bool bHasCandidatePlan = false;

		if (
			Candidate->IsAerialActionQueuedOrPlaying() &&
			Candidate->GetAerialActionBall() == SoccerBall &&
			Candidate->GetQueuedAerialPlan().bValid &&
			Candidate->GetQueuedAerialPlan().ActionType == ExpectedAction
		)
		{
			CandidatePlan = Candidate->GetQueuedAerialPlan();
			bHasCandidatePlan = true;
		}
		else if (ASoccerAICharacter* CandidateAI = Cast<ASoccerAICharacter>(Candidate))
		{
			if (!IsAerialContestCandidateEligible(CandidateAI, Intent))
			{
				continue;
			}

			bHasCandidatePlan =
				CandidateAI->FindBestAerialInterceptionPlan(
					SoccerBall,
					Intent,
					CandidatePlan
				);
		}

		if (
			!bHasCandidatePlan ||
			!CandidatePlan.bValid ||
			CandidatePlan.Ball != SoccerBall ||
			CandidatePlan.ActionType != ExpectedAction
		)
		{
			continue;
		}

		++OutCandidateCount;

		const float CandidateScore =
			ScoreAerialContestPlan(Candidate, CandidatePlan);
		const bool bClearlyBetter =
			CandidateScore < OutPrimaryScore - 0.0005f;
		const bool bTieWithStablePriority =
			FMath::Abs(CandidateScore - OutPrimaryScore) <= 0.0005f &&
			Candidate->GetUniqueID() < OutPrimaryCharacter->GetUniqueID();

		if (bClearlyBetter || bTieWithStablePriority)
		{
			OutPrimaryCharacter = Candidate;
			OutPrimaryPlan = CandidatePlan;
			OutPrimaryScore = CandidateScore;
		}
	}

	return IsValid(OutPrimaryCharacter);
}

FVector ASoccerAIController::BuildAerialContestSecondBallSupportLocation(
	const ASoccerAICharacter* SoccerCharacter,
	const ASoccerCharacterBase* PrimaryCharacter,
	const FSoccerAerialInterceptionPlan& PrimaryPlan
) const
{
	if (!IsValid(SoccerCharacter) || !IsValid(PrimaryCharacter))
	{
		return FVector::ZeroVector;
	}

	const FVector ContactLocation = PrimaryPlan.ContactLocation;

	/*
	 * Stage 6: after the primary player actually touches the ball, the
	 * original tactical target is no longer the best information available.
	 * The real outgoing velocity and the new predicted trajectory are now the
	 * authoritative source for the second-ball player.
	 *
	 * This is deliberately a short-lived reaction. It does not replace the
	 * normal interception system; it only gives the secondary player a better
	 * first target immediately after the header.
	 */
	if (bUseAIAerialPostContactRecovery && IsValid(PrimaryPlan.Ball))
	{
		const UWorld* World = GetWorld();
		const float CurrentTime =
			World != nullptr ? World->GetTimeSeconds() : 0.0f;
		const float ContactAge =
			CurrentTime - PrimaryCharacter->GetLastAerialContactWorldTime();
		const FSoccerAerialContactResult ContactResult =
			PrimaryCharacter->GetLastAerialContactResult();

		const bool bRecentAcceptedContact =
			ContactResult.bContactResolved &&
			ContactResult.bTouchAcceptedByRules &&
			ContactAge >= 0.0f &&
			ContactAge <=
				FMath::Max(0.0f, AIAerialPostContactReactionWindow) &&
			ContactResult.OutgoingBallVelocity.Size2D() >= 100.0f;

		if (bRecentAcceptedContact)
		{
			const float PredictionTime =
			ContactResult.ContactQuality <
				FMath::Clamp(
					AIAerialPostContactPoorQualityThreshold,
					0.0f,
					1.0f
				)
			? FMath::Max(
					0.05f,
					AIAerialPostContactPoorQualityPredictionTime
				)
			: FMath::Max(
					0.05f,
					AIAerialPostContactPredictionTime
				);

			TArray<FSoccerBallTrajectorySample> PostContactTrajectory;
			const float PredictionHorizon =
				FMath::Max(1.25f, PredictionTime + 0.45f);

			if (
				PrimaryPlan.Ball->BuildPredictedTrajectory(
					PredictionHorizon,
					0.04f,
					PostContactTrajectory
				) &&
				PostContactTrajectory.Num() > 0
			)
			{
				const FSoccerBallTrajectorySample* SelectedSample = nullptr;
				const FSoccerBallTrajectorySample* FallbackSample = nullptr;

				for (const FSoccerBallTrajectorySample& Sample : PostContactTrajectory)
				{
					if (Sample.TimeFromNow + KINDA_SMALL_NUMBER < PredictionTime)
					{
						continue;
					}

					if (FallbackSample == nullptr)
					{
						FallbackSample = &Sample;
					}

					if (Sample.bNearGround)
					{
						SelectedSample = &Sample;
						break;
					}
				}

				if (SelectedSample == nullptr)
				{
					SelectedSample = FallbackSample;
				}

				if (SelectedSample != nullptr)
				{
					FVector OutgoingDirection =
						ContactResult.OutgoingBallVelocity.GetSafeNormal2D();

					if (OutgoingDirection.IsNearlyZero())
					{
						OutgoingDirection =
							SelectedSample->Velocity.GetSafeNormal2D();
					}

					if (OutgoingDirection.IsNearlyZero())
					{
						OutgoingDirection =
							PrimaryCharacter->GetActorForwardVector().GetSafeNormal2D();
					}

					if (OutgoingDirection.IsNearlyZero())
					{
						OutgoingDirection = FVector::ForwardVector;
					}

					FVector RightDirection = FVector::CrossProduct(
						FVector::UpVector,
						OutgoingDirection
					).GetSafeNormal2D();

					if (RightDirection.IsNearlyZero())
					{
						RightDirection = FVector::RightVector;
					}

					const FVector RelativeToBall =
						SoccerCharacter->GetActorLocation() -
						SelectedSample->Location;
					const float SideDot = FVector::DotProduct(
						RelativeToBall,
						RightDirection
					);
					const float SideSign =
						FMath::Abs(SideDot) > 20.0f
						? FMath::Sign(SideDot)
						: (SoccerCharacter->GetUniqueID() % 2 == 0 ? 1.0f : -1.0f);

					FVector SupportLocation =
						SelectedSample->Location +
						OutgoingDirection *
							(
								FMath::Max(0.0f, AIAerialPostContactSupportLateralOffset) *
								FMath::Max(0.0f, AIAerialPostContactSupportForwardBias)
							) +
						RightDirection * SideSign *
							FMath::Max(0.0f, AIAerialPostContactSupportLateralOffset);

					SupportLocation.Z = SoccerCharacter->GetActorLocation().Z;

					if (ASoccerDebugManager::IsEnabled(this, ESoccerDebugCategory::Aerial) && World != nullptr)
					{
						DrawDebugSphere(
							World,
							SelectedSample->Location + FVector(0.0f, 0.0f, 18.0f),
							20.0f,
							12,
							FColor::Orange,
							false,
							0.20f,
							0,
							2.0f
						);
					}

					return ClampAIAerialHeaderTargetInsideField(SupportLocation);
				}
			}
		}
	}

	FVector ProgressDirection = FVector::ZeroVector;

	if (const ASoccerAICharacter* PrimaryAI =
		Cast<ASoccerAICharacter>(PrimaryCharacter))
	{
		if (PrimaryAI->HasAIAerialHeaderTarget())
		{
			ProgressDirection =
				PrimaryAI->GetAIAerialHeaderTargetLocation() -
				ContactLocation;
			ProgressDirection.Z = 0.0f;
		}
	}

	if (ProgressDirection.IsNearlyZero() && IsValid(MatchManager))
	{
		const ESoccerTeamPhase TeamPhase =
			MatchManager->GetTeamPhase(SoccerCharacter->GetTeam());

		if (TeamPhase == ESoccerTeamPhase::Defending)
		{
			ProgressDirection =
				ContactLocation -
				MatchManager->GetOwnGoalCenterLocation(
					SoccerCharacter->GetTeam()
				);
		}
		else
		{
			ProgressDirection =
				MatchManager->GetShotTargetLocation(SoccerCharacter) -
				ContactLocation;
		}
		ProgressDirection.Z = 0.0f;
	}

	if (ProgressDirection.IsNearlyZero())
	{
		ProgressDirection = PrimaryPlan.PredictedIncomingBallVelocity;
		ProgressDirection.Z = 0.0f;
	}

	ProgressDirection = ProgressDirection.GetSafeNormal2D();

	if (ProgressDirection.IsNearlyZero())
	{
		ProgressDirection = SoccerCharacter->GetActorForwardVector().GetSafeNormal2D();
	}

	if (ProgressDirection.IsNearlyZero())
	{
		ProgressDirection = FVector::ForwardVector;
	}

	FVector RightDirection = FVector::CrossProduct(
		FVector::UpVector,
		ProgressDirection
	).GetSafeNormal2D();

	if (RightDirection.IsNearlyZero())
	{
		RightDirection = FVector::RightVector;
	}

	const FVector RelativeToContact =
		SoccerCharacter->GetActorLocation() - ContactLocation;
	const float SideDot = FVector::DotProduct(
		RelativeToContact,
		RightDirection
	);
	const float SideSign =
		FMath::Abs(SideDot) > 20.0f
		? FMath::Sign(SideDot)
		: (SoccerCharacter->GetUniqueID() % 2 == 0 ? 1.0f : -1.0f);

	FVector SupportLocation =
		ContactLocation +
		ProgressDirection *
			FMath::Max(0.0f, AIAerialContestSecondBallForwardDistance) +
		RightDirection * SideSign *
			FMath::Max(0.0f, AIAerialContestSecondBallLateralDistance);

	SupportLocation.Z = SoccerCharacter->GetActorLocation().Z;
	return ClampAIAerialHeaderTargetInsideField(SupportLocation);
}

bool ASoccerAIController::UpdateAerialContestSecondBallSupport(
	ASoccerAICharacter* SoccerCharacter,
	ASoccerBall* SoccerBall,
	ASoccerCharacterBase* PrimaryCharacter,
	const FSoccerAerialInterceptionPlan& PrimaryPlan,
	ESoccerAIOrder CurrentOrder
)
{
	if (
		!IsValid(SoccerCharacter) ||
		!IsValid(SoccerBall) ||
		!IsValid(PrimaryCharacter) ||
		PrimaryCharacter == SoccerCharacter ||
		!PrimaryPlan.bValid
	)
	{
		ClearAerialContestCoordinationState();
		return false;
	}

	if (
		SoccerCharacter->IsAerialActionApproaching() ||
		SoccerCharacter->IsAerialActionWaitingToStart()
	)
	{
		SoccerCharacter->CancelAerialAction();
	}

	SoccerCharacter->ClearAIAerialHeaderDecision();

	const FVector SupportLocation =
		BuildAerialContestSecondBallSupportLocation(
			SoccerCharacter,
			PrimaryCharacter,
			PrimaryPlan
		);

	if (SupportLocation.IsNearlyZero())
	{
		ClearAerialContestCoordinationState();
		return false;
	}

	UWorld* World = GetWorld();
	const float CurrentTime =
		World != nullptr ? World->GetTimeSeconds() : 0.0f;
	const float TimeSinceLastRequest =
		CurrentTime - LastAerialContestSecondBallMoveRequestTime;
	const float TargetDistance =
		bAerialContestSecondarySupportActive
		? FVector::Dist2D(
			SupportLocation,
			LastAerialContestSecondBallTarget
		)
		: TNumericLimits<float>::Max();

	const bool bPrimaryChanged =
		AerialContestPrimaryCharacter.Get() != PrimaryCharacter ||
		AerialContestCoordinationBall.Get() != SoccerBall;
	const bool bTargetMovedEnough =
		TargetDistance >=
		FMath::Max(0.0f, AIAerialContestSecondBallRepathDistance);
	const bool bMinIntervalPassed =
		TimeSinceLastRequest >=
		FMath::Max(0.0f, AIAerialContestSecondBallRepathMinInterval);
	const bool bForcedRefresh =
		TimeSinceLastRequest >=
		FMath::Max(0.02f, AIAerialContestSecondBallForcedRefreshInterval);
	const EPathFollowingStatus::Type MoveStatus = GetMoveStatus();
	const bool bNotMoving =
		MoveStatus == EPathFollowingStatus::Idle ||
		MoveStatus == EPathFollowingStatus::Paused;

	if (
		bPrimaryChanged ||
		!bAerialContestSecondarySupportActive ||
		bNotMoving ||
		bForcedRefresh ||
		(bTargetMovedEnough && bMinIntervalPassed)
	)
	{
		MoveToLocationWithAIMovement(
			CurrentOrder,
			SupportLocation,
			FMath::Max(
				10.0f,
				AIAerialContestSecondBallAcceptanceRadius
			),
			false
		);

		LastAerialContestSecondBallTarget = SupportLocation;
		LastAerialContestSecondBallMoveRequestTime = CurrentTime;
	}

	AerialContestCoordinationBall = SoccerBall;
	AerialContestPrimaryCharacter = PrimaryCharacter;
	bAerialContestSecondarySupportActive = true;
	SetFocus(SoccerBall);

	if (ASoccerDebugManager::IsEnabled(this, ESoccerDebugCategory::Aerial) && World != nullptr)
	{
		DrawDebugSphere(
			World,
			SupportLocation + FVector(0.0f, 0.0f, 20.0f),
			24.0f,
			12,
			FColor::Cyan,
			false,
			0.12f,
			0,
			2.0f
		);
		DrawDebugLine(
			World,
			SoccerCharacter->GetActorLocation() + FVector(0.0f, 0.0f, 50.0f),
			SupportLocation + FVector(0.0f, 0.0f, 20.0f),
			FColor::Cyan,
			false,
			0.12f,
			0,
			1.5f
		);
	}

	return true;
}

void ASoccerAIController::ClearAerialContestCoordinationState()
{
	AerialContestCoordinationBall.Reset();
	AerialContestPrimaryCharacter.Reset();
	bAerialContestSecondarySupportActive = false;
	LastAerialContestSecondBallTarget = FVector::ZeroVector;
	LastAerialContestSecondBallMoveRequestTime = -1000.0f;
}

bool ASoccerAIController::UpdateAerialBallInterceptionMovement(
	ASoccerAICharacter* SoccerCharacter,
	ASoccerBall* SoccerBall,
	ESoccerAIOrder CurrentOrder
)
{
	if (!IsValid(SoccerCharacter) || !IsValid(SoccerBall))
	{
		ClearAerialBallInterceptionMovement();
		return false;
	}

	if (!bUseAIAerialActions)
	{
		if (
			SoccerCharacter->IsAerialActionApproaching() ||
			SoccerCharacter->IsAerialActionWaitingToStart()
		)
		{
			SoccerCharacter->CancelAerialAction();
		}

		ClearAerialBallInterceptionMovement();
		return false;
	}

	if (
		SoccerCharacter->GetPlayerRole() ==
			ESoccerPlayerRole::Goalkeeper &&
		!bAllowGoalkeeperAerialActions
	)
	{
		ClearAerialBallInterceptionMovement();
		return false;
	}

	if (!SoccerCharacter->IsAerialActionQueuedOrPlaying())
	{
		SoccerCharacter->ClearAIAerialHeaderDecision();

		/*
		 * Tactical intent defines the desired result. The trajectory planner
		 * still decides whether the chosen action is physically possible.
		 */
		TArray<ESoccerAerialActionIntent> IntentPriority;

		const auto AddIntentIfMissing =
			[&IntentPriority](ESoccerAerialActionIntent Intent)
			{
				if (!IntentPriority.Contains(Intent))
				{
					IntentPriority.Add(Intent);
				}
			};

		ESoccerTeamPhase TeamPhase = ESoccerTeamPhase::Neutral;

		if (IsValid(MatchManager))
		{
			TeamPhase = MatchManager->GetTeamPhase(
				SoccerCharacter->GetTeam()
			);
		}

		if (TeamPhase == ESoccerTeamPhase::Defending)
		{
			AddIntentIfMissing(
				ESoccerAerialActionIntent::DefensiveBlock
			);
			AddIntentIfMissing(
				ESoccerAerialActionIntent::Control
			);
		}
		else if (
			TeamPhase == ESoccerTeamPhase::Attacking &&
			bUseAIOffensiveHeaders
		)
		{
			const ESoccerPlayerRole PlayerRole =
				SoccerCharacter->GetPlayerRole();
			const bool bRoleCanAttackHeader =
				PlayerRole == ESoccerPlayerRole::Forward ||
				PlayerRole == ESoccerPlayerRole::Midfielder ||
				(
					bAllowDefenderOffensiveHeaders &&
					PlayerRole == ESoccerPlayerRole::Defender
				);

			if (bRoleCanAttackHeader)
			{
				AddIntentIfMissing(
					ESoccerAerialActionIntent::ActiveHeader
				);
			}

			AddIntentIfMissing(
				ESoccerAerialActionIntent::Control
			);
		}
		else
		{
			AddIntentIfMissing(
				ESoccerAerialActionIntent::Control
			);
		}

		/* Last-resort physical interception if the preferred tactic is absent. */
		AddIntentIfMissing(
			ESoccerAerialActionIntent::Automatic
		);

		bool bStartedAerialApproach = false;

		for (ESoccerAerialActionIntent CandidateIntent : IntentPriority)
		{
			if (
				!ConfigureAIAerialHeaderDecisionForIntent(
					SoccerCharacter,
					SoccerBall,
					CandidateIntent
				)
			)
			{
				continue;
			}

			FSoccerAerialInterceptionPlan CandidatePlan;

			if (!SoccerCharacter->FindBestAerialInterceptionPlan(
				SoccerBall,
				CandidateIntent,
				CandidatePlan
			))
			{
				SoccerCharacter->ClearAIAerialHeaderDecision();
				continue;
			}

			const bool bJumpContestIntent =
				CandidateIntent == ESoccerAerialActionIntent::ActiveHeader ||
				CandidateIntent == ESoccerAerialActionIntent::DefensiveBlock;

			if (bUseAIAerialContestCoordination && bJumpContestIntent)
			{
				ASoccerCharacterBase* PrimaryCharacter = nullptr;
				FSoccerAerialInterceptionPlan PrimaryPlan;
				float SelfScore = TNumericLimits<float>::Max();
				float PrimaryScore = TNumericLimits<float>::Max();
				int32 CandidateCount = 0;

				if (FindPrimaryAerialContestCharacter(
					SoccerCharacter,
					SoccerBall,
					CandidateIntent,
					CandidatePlan,
					PrimaryCharacter,
					PrimaryPlan,
					SelfScore,
					PrimaryScore,
					CandidateCount
				))
				{
					if (PrimaryCharacter != SoccerCharacter)
					{
						if (ASoccerDebugManager::IsEnabled(this, ESoccerDebugCategory::Aerial))
						{
							UE_LOG(
								LogTemp,
								Log,
								TEXT("%s aerial contest: SECONDARY for %s | self %.3f primary %.3f candidates %d."),
								*SoccerCharacter->GetName(),
								*PrimaryCharacter->GetName(),
								SelfScore,
								PrimaryScore,
								CandidateCount
							);
						}

						SoccerCharacter->ClearAIAerialHeaderDecision();
						bHasAerialBallMoveTarget = false;
						LastAerialBallMoveTarget = FVector::ZeroVector;
						LastAerialBallMoveRequestTime = -1000.0f;

						return UpdateAerialContestSecondBallSupport(
							SoccerCharacter,
							SoccerBall,
							PrimaryCharacter,
							PrimaryPlan,
							CurrentOrder
						);
					}

					ClearAerialContestCoordinationState();

					if (ASoccerDebugManager::IsEnabled(this, ESoccerDebugCategory::Aerial) && CandidateCount > 1)
					{
						UE_LOG(
							LogTemp,
							Log,
							TEXT("%s aerial contest: PRIMARY | score %.3f candidates %d."),
							*SoccerCharacter->GetName(),
							PrimaryScore,
							CandidateCount
						);
					}
				}
			}

			if (SoccerCharacter->TryStartAerialActionApproachForPlan(
				CandidatePlan,
				CandidateIntent
			))
			{
				bStartedAerialApproach = true;
				break;
			}

			SoccerCharacter->ClearAIAerialHeaderDecision();
		}

		if (!bStartedAerialApproach)
		{
			SoccerCharacter->ClearAIAerialHeaderDecision();
			ClearAerialBallInterceptionMovement();
			return false;
		}
	}

	/*
	 * While a jump is still only being approached/waited for, keep comparing
	 * the team candidates. Once the montage is playing the player is committed
	 * and the existing contact-contest resolver decides the duel with rivals.
	 */
	if (
		bUseAIAerialContestCoordination &&
		!SoccerCharacter->IsAerialActionLocked() &&
		(
			SoccerCharacter->IsAerialActionApproaching() ||
			SoccerCharacter->IsAerialActionWaitingToStart()
		) &&
		(
			SoccerCharacter->GetActiveAerialActionType() ==
				ESoccerAerialActionType::JumpHeaderKick ||
			SoccerCharacter->GetActiveAerialActionType() ==
				ESoccerAerialActionType::JumpHeaderBlock
		)
	)
	{
		const ESoccerAerialActionIntent ContestIntent =
			SoccerCharacter->GetActiveAerialActionType() ==
				ESoccerAerialActionType::JumpHeaderKick
			? ESoccerAerialActionIntent::ActiveHeader
			: ESoccerAerialActionIntent::DefensiveBlock;

		ASoccerCharacterBase* PrimaryCharacter = nullptr;
		FSoccerAerialInterceptionPlan PrimaryPlan;
		float SelfScore = TNumericLimits<float>::Max();
		float PrimaryScore = TNumericLimits<float>::Max();
		int32 CandidateCount = 0;

		if (
			FindPrimaryAerialContestCharacter(
				SoccerCharacter,
				SoccerBall,
				ContestIntent,
				SoccerCharacter->GetQueuedAerialPlan(),
				PrimaryCharacter,
				PrimaryPlan,
				SelfScore,
				PrimaryScore,
				CandidateCount
			) &&
			PrimaryCharacter != SoccerCharacter
		)
		{
			if (ASoccerDebugManager::IsEnabled(this, ESoccerDebugCategory::Aerial))
			{
				UE_LOG(
					LogTemp,
					Log,
					TEXT("%s aerial contest: yields approach to %s | self %.3f primary %.3f."),
					*SoccerCharacter->GetName(),
					*PrimaryCharacter->GetName(),
					SelfScore,
					PrimaryScore
				);
			}

			return UpdateAerialContestSecondBallSupport(
				SoccerCharacter,
				SoccerBall,
				PrimaryCharacter,
				PrimaryPlan,
				CurrentOrder
			);
		}

		ClearAerialContestCoordinationState();
	}

	if (SoccerCharacter->IsAerialActionLocked())
	{
		StopMovement();
		ClearAerialBallInterceptionMovement();
		SetFocus(SoccerBall);
		return true;
	}

	if (SoccerCharacter->IsAerialActionWaitingToStart())
	{
		StopMovement();
		ClearAerialBallInterceptionMovement();
		SetFocus(SoccerBall);
		return true;
	}

	if (!SoccerCharacter->IsAerialActionApproaching())
	{
		ClearAerialBallInterceptionMovement();
		return false;
	}

	FVector MoveLocation;
	float AcceptanceRadius = 45.0f;

	if (!SoccerCharacter->GetAerialPreparationTarget(
		MoveLocation,
		AcceptanceRadius
	))
	{
		SoccerCharacter->CancelAerialAction();
		ClearAerialBallInterceptionMovement();
		return false;
	}

	MoveLocation.Z = SoccerCharacter->GetActorLocation().Z;

	UWorld* World = GetWorld();
	const float CurrentTime =
		World != nullptr ? World->GetTimeSeconds() : 0.0f;
	const float TimeSinceLastRequest =
		CurrentTime - LastAerialBallMoveRequestTime;

	const bool bOrderChanged =
		!bHasAerialBallMoveTarget ||
		LastAerialBallMoveOrder != CurrentOrder;

	const float TargetMoveDistance =
		bHasAerialBallMoveTarget
		? FVector::Dist2D(
			MoveLocation,
			LastAerialBallMoveTarget
		)
		: TNumericLimits<float>::Max();

	const bool bTargetMovedEnough =
		TargetMoveDistance >= AIAerialMoveRepathDistance;
	const bool bMinimumIntervalPassed =
		TimeSinceLastRequest >= AIAerialMoveRepathMinInterval;
	const bool bForcedRefresh =
		TimeSinceLastRequest >= AIAerialMoveForcedRefreshInterval;

	const EPathFollowingStatus::Type MoveStatus = GetMoveStatus();
	const bool bNotMoving =
		MoveStatus == EPathFollowingStatus::Idle ||
		MoveStatus == EPathFollowingStatus::Paused;

	if (
		bOrderChanged ||
		bNotMoving ||
		bForcedRefresh ||
		(bTargetMovedEnough && bMinimumIntervalPassed)
	)
	{
		MoveToLocationWithAIMovement(
			CurrentOrder,
			MoveLocation,
			FMath::Max(10.0f, AcceptanceRadius),
			false
		);

		LastAerialBallMoveTarget = MoveLocation;
		LastAerialBallMoveRequestTime = CurrentTime;
		LastAerialBallMoveOrder = CurrentOrder;
		bHasAerialBallMoveTarget = true;
	}

	SetFocus(SoccerBall);
	return true;
}

void ASoccerAIController::ClearAerialBallInterceptionMovement()
{
	bHasAerialBallMoveTarget = false;
	LastAerialBallMoveTarget = FVector::ZeroVector;
	LastAerialBallMoveRequestTime = -1000.0f;
	LastAerialBallMoveOrder = ESoccerAIOrder::ReturnHome;
	ClearAerialContestCoordinationState();
}

bool ASoccerAIController::ResolvePredictiveBallChaseLocation(
	ASoccerAICharacter* SoccerCharacter,
	ASoccerBall* SoccerBall,
	FVector& OutMoveLocation
)
{
	OutMoveLocation = FVector::ZeroVector;

	if (!IsValid(SoccerCharacter) || !IsValid(SoccerBall))
	{
		return false;
	}

	if (bUsePredictiveBallChase)
	{
		const bool bProfileDefensivePrediction =
			IsValid(MatchManager) &&
			MatchManager->GetTeamPhase(SoccerCharacter->GetTeam()) == ESoccerTeamPhase::Defending &&
			SoccerCharacter->HasPlayerProfile();

		const float EffectivePredictiveTargetRefreshInterval =
			bProfileDefensivePrediction
			? AIPredictiveTargetRefreshInterval *
				GetDefensiveProfileReactionMultiplier(SoccerCharacter)
			: AIPredictiveTargetRefreshInterval;

		SoccerCharacter->ResolveStableBallPursuitTarget(
			SoccerBall,
			OutMoveLocation,
			nullptr,
			FMath::Max(0.02f, EffectivePredictiveTargetRefreshInterval)
		);

		if (bProfileDefensivePrediction)
		{
			FVector ObservedBallLocation =
				GetDelayedObservedBallLocation(SoccerBall);
			ObservedBallLocation.Z = OutMoveLocation.Z;

			const float PredictionTrust =
				GetDefensiveProfileAnticipationPredictionTrust(SoccerCharacter);

			OutMoveLocation = FMath::Lerp(
				ObservedBallLocation,
				OutMoveLocation,
				PredictionTrust
			);
		}
	}
	else
	{
		OutMoveLocation = GetDelayedObservedBallLocation(SoccerBall);
	}

	if (OutMoveLocation.ContainsNaN())
	{
		OutMoveLocation = SoccerBall->GetActorLocation();
	}

	OutMoveLocation.Z = SoccerCharacter->GetActorLocation().Z;

	FVector GoalAreaRespectMoveLocation;

	if (
		IsValid(MatchManager) &&
		MatchManager->TryAdjustGoalAreaAttackerBallChaseLocation(
			SoccerCharacter,
			OutMoveLocation,
			GoalAreaRespectMoveLocation
		)
		)
	{
		OutMoveLocation = GoalAreaRespectMoveLocation;
	}

	return true;
}

void ASoccerAIController::UpdatePredictiveBallChaseMovement(
	ASoccerAICharacter* SoccerCharacter,
	ASoccerBall* SoccerBall,
	ESoccerAIOrder CurrentOrder
)
{
	FVector MoveLocation;

	if (!ResolvePredictiveBallChaseLocation(
		SoccerCharacter,
		SoccerBall,
		MoveLocation
	))
	{
		return;
	}

	UWorld* World = GetWorld();
	const float CurrentTime =
		World != nullptr
		? World->GetTimeSeconds()
		: 0.0f;

	const float TimeSinceLastRequest =
		CurrentTime - LastPredictiveBallMoveRequestTime;

	const bool bOrderChanged =
		!bHasPredictiveBallMoveTarget ||
		LastPredictiveBallMoveOrder != CurrentOrder;

	const float TargetMoveDistance =
		bHasPredictiveBallMoveTarget
		? FVector::Dist2D(
			MoveLocation,
			LastPredictiveBallMoveTarget
		)
		: TNumericLimits<float>::Max();

	const bool bTargetMovedEnough =
		TargetMoveDistance >= AIPredictiveMoveRepathDistance;

	const bool bMinimumIntervalPassed =
		TimeSinceLastRequest >= AIPredictiveMoveRepathMinInterval;

	const bool bForcedRefresh =
		TimeSinceLastRequest >= AIPredictiveMoveForcedRefreshInterval;

	const EPathFollowingStatus::Type MoveStatus = GetMoveStatus();
	const bool bNotMoving =
		MoveStatus == EPathFollowingStatus::Idle ||
		MoveStatus == EPathFollowingStatus::Paused;

	const bool bShouldRequestMove =
		bOrderChanged ||
		bNotMoving ||
		bForcedRefresh ||
		(bTargetMovedEnough && bMinimumIntervalPassed);

	if (bShouldRequestMove)
	{
		MoveToLocationWithAIMovement(
			CurrentOrder,
			MoveLocation,
			BallChaseAcceptanceRadius,
			false
		);

		LastPredictiveBallMoveTarget = MoveLocation;
		LastPredictiveBallMoveRequestTime = CurrentTime;
		LastPredictiveBallMoveOrder = CurrentOrder;
		bHasPredictiveBallMoveTarget = true;
	}

	SetFocalPoint(
		MoveLocation,
		EAIFocusPriority::Gameplay
	);
}

bool ASoccerAIController::UpdateDefensivePressureOvertakeMovement(
	ASoccerAICharacter* SoccerCharacter,
	ASoccerBall* SoccerBall
)
{
	if (
		!bUseAIDefensivePressureOvertake ||
		!IsValid(SoccerCharacter) ||
		!IsValid(SoccerBall) ||
		!IsValid(MatchManager)
		)
	{
		ClearDefensivePressureOvertakeState();
		return false;
	}

	bool bCarrierIsAutoPassing = false;
	ASoccerCharacterBase* Carrier =
		FindDefensivePressureCarrier(
			SoccerCharacter,
			SoccerBall,
			bCarrierIsAutoPassing
		);

	if (!IsValid(Carrier))
	{
		ClearDefensivePressureOvertakeState();
		return false;
	}

	if (bCarrierIsAutoPassing)
	{
		const ASoccerAICharacter* AutoPassCarrier =
			Cast<ASoccerAICharacter>(Carrier);

		// La pelota del autopase esta realmente libre. Si el defensor puede
		// ganar una intercepcion limpia, se conserva la persecucion predictiva
		// normal; la maniobra lateral solo aparece cuando seguir esa linea lo
		// dejaria corriendo detras del atacante.
		if (
			IsValid(AutoPassCarrier) &&
			ShouldDirectlyInterceptAutoPass(
				SoccerCharacter,
				AutoPassCarrier,
				SoccerBall
			)
			)
		{
			ClearDefensivePressureOvertakeState();
			return false;
		}
	}

	FVector MoveLocation;

	if (
		!BuildDefensivePressureOvertakeLocation(
			SoccerCharacter,
			Carrier,
			SoccerBall,
			bCarrierIsAutoPassing,
			MoveLocation
		)
		)
	{
		ClearDefensivePressureOvertakeState();
		return false;
	}

	const bool bWasAlreadyUsingOvertake =
		bHasDefensivePressureOvertakeMoveTarget;

	if (!bWasAlreadyUsingOvertake)
	{
		// No se conserva el compromiso de intercepcion de pelota libre porque
		// ahora el objetivo tactico es rodear al corredor.
		ClearPredictiveBallChaseMovement(SoccerCharacter);
	}

	UWorld* World = GetWorld();
	const float CurrentTime =
		World != nullptr
		? World->GetTimeSeconds()
		: 0.0f;

	const float TimeSinceLastRequest =
		CurrentTime - LastDefensivePressureOvertakeMoveRequestTime;

	const float TargetMoveDistance =
		bHasDefensivePressureOvertakeMoveTarget
		? FVector::Dist2D(
			MoveLocation,
			LastDefensivePressureOvertakeMoveTarget
		)
		: TNumericLimits<float>::Max();

	const bool bTargetMovedEnough =
		TargetMoveDistance >= AIPressOvertakeMoveRepathDistance;

	const bool bMinimumIntervalPassed =
		TimeSinceLastRequest >= AIPressOvertakeMoveRepathMinInterval;

	const bool bForcedRefresh =
		TimeSinceLastRequest >= AIPressOvertakeMoveForcedRefreshInterval;

	const EPathFollowingStatus::Type MoveStatus = GetMoveStatus();
	const bool bNotMoving =
		MoveStatus == EPathFollowingStatus::Idle ||
		MoveStatus == EPathFollowingStatus::Paused;

	if (
		!bHasDefensivePressureOvertakeMoveTarget ||
		bNotMoving ||
		bForcedRefresh ||
		(bTargetMovedEnough && bMinimumIntervalPassed)
		)
	{
		MoveToLocationWithAIMovement(
			ESoccerAIOrder::PressBall,
			MoveLocation,
			AIPressOvertakeAcceptanceRadius,
			false
		);

		LastDefensivePressureOvertakeMoveTarget = MoveLocation;
		LastDefensivePressureOvertakeMoveRequestTime = CurrentTime;
		bHasDefensivePressureOvertakeMoveTarget = true;
	}

	SetFocus(Carrier);
	return true;
}

ASoccerCharacterBase* ASoccerAIController::FindDefensivePressureCarrier(
	const ASoccerAICharacter* SoccerCharacter,
	const ASoccerBall* SoccerBall,
	bool& bOutCarrierIsAutoPassing
) const
{
	bOutCarrierIsAutoPassing = false;

	if (
		!IsValid(SoccerCharacter) ||
		!IsValid(SoccerBall) ||
		!IsValid(MatchManager)
		)
	{
		return nullptr;
	}

	ASoccerCharacterBase* PossessingCharacter =
		MatchManager->GetPossessingCharacter();

	if (
		IsValid(PossessingCharacter) &&
		PossessingCharacter != SoccerCharacter &&
		PossessingCharacter->GetTeam() != SoccerCharacter->GetTeam()
		)
	{
		const ASoccerAICharacter* PossessingAI =
			Cast<ASoccerAICharacter>(PossessingCharacter);

		if (
			IsValid(PossessingAI) &&
			PossessingAI->IsGoalkeeperHoldingBall()
			)
		{
			return nullptr;
		}

		return PossessingCharacter;
	}

	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return nullptr;
	}

	ASoccerAICharacter* BestAutoPassCarrier = nullptr;
	float BestDistanceToBall = TNumericLimits<float>::Max();

	for (TActorIterator<ASoccerAICharacter> It(World); It; ++It)
	{
		ASoccerAICharacter* Candidate = *It;

		if (!IsValid(Candidate))
		{
			continue;
		}

		if (Candidate->GetTeam() == SoccerCharacter->GetTeam())
		{
			continue;
		}

		if (!Candidate->IsAIAutoPassActive())
		{
			continue;
		}

		if (Candidate->GetAIAutoPassBall() != SoccerBall)
		{
			continue;
		}

		const float DistanceToBall = FVector::Dist2D(
			Candidate->GetActorLocation(),
			SoccerBall->GetActorLocation()
		);

		if (DistanceToBall < BestDistanceToBall)
		{
			BestDistanceToBall = DistanceToBall;
			BestAutoPassCarrier = Candidate;
		}
	}

	if (IsValid(BestAutoPassCarrier))
	{
		bOutCarrierIsAutoPassing = true;
		return BestAutoPassCarrier;
	}

	return nullptr;
}

bool ASoccerAIController::ShouldDirectlyInterceptAutoPass(
	const ASoccerAICharacter* PressingCharacter,
	const ASoccerAICharacter* AutoPassCarrier,
	const ASoccerBall* SoccerBall
) const
{
	if (
		!IsValid(PressingCharacter) ||
		!IsValid(AutoPassCarrier) ||
		!IsValid(SoccerBall)
		)
	{
		return false;
	}

	FSoccerBallInterceptionResult DefenderInterception;

	if (
		!PressingCharacter->FindBestBallInterception(
			SoccerBall,
			DefenderInterception
		) ||
		!DefenderInterception.bHasSolution ||
		!DefenderInterception.bCanArriveInTime
		)
	{
		return false;
	}

	const float CarrierArrivalTime =
		AutoPassCarrier->EstimateArrivalTimeToLocation(
			DefenderInterception.InterceptionLocation
		);

	if (!FMath::IsFinite(CarrierArrivalTime))
	{
		return true;
	}

	return
		DefenderInterception.PlayerArrivalTime +
		AIPressAutoPassDirectInterceptionAdvantage <=
		CarrierArrivalTime;
}

bool ASoccerAIController::BuildDefensivePressureOvertakeLocation(
	ASoccerAICharacter* PressingCharacter,
	ASoccerCharacterBase* Carrier,
	ASoccerBall* SoccerBall,
	bool bCarrierIsAutoPassing,
	FVector& OutMoveLocation
)
{
	OutMoveLocation = FVector::ZeroVector;

	if (
		!IsValid(PressingCharacter) ||
		!IsValid(Carrier) ||
		!IsValid(SoccerBall) ||
		!IsValid(MatchManager)
		)
	{
		return false;
	}

	FVector PressingLocation = PressingCharacter->GetActorLocation();
	FVector CarrierLocation = Carrier->GetActorLocation();
	FVector CarrierVelocity = Carrier->GetVelocity();
	FVector BallLocation = SoccerBall->GetActorLocation();

	PressingLocation.Z = 0.0f;
	CarrierLocation.Z = 0.0f;
	CarrierVelocity.Z = 0.0f;
	BallLocation.Z = 0.0f;

	const float CarrierSpeed = CarrierVelocity.Size();
	FVector CarrierDirection = FVector::ZeroVector;

	if (bCarrierIsAutoPassing)
	{
		const ASoccerAICharacter* AutoPassCarrier =
			Cast<ASoccerAICharacter>(Carrier);

		if (IsValid(AutoPassCarrier))
		{
			FVector DirectionToAutoPassTarget =
				AutoPassCarrier->GetAIAutoPassTargetLocation() -
				CarrierLocation;

			DirectionToAutoPassTarget.Z = 0.0f;
			CarrierDirection =
				DirectionToAutoPassTarget.GetSafeNormal();
		}
	}

	if (CarrierDirection.IsNearlyZero() && CarrierSpeed > KINDA_SMALL_NUMBER)
	{
		CarrierDirection = CarrierVelocity / CarrierSpeed;
	}

	if (CarrierDirection.IsNearlyZero())
	{
		FVector CarrierToBall = BallLocation - CarrierLocation;
		CarrierToBall.Z = 0.0f;
		CarrierDirection = CarrierToBall.GetSafeNormal();
	}

	if (CarrierDirection.IsNearlyZero())
	{
		CarrierDirection = Carrier->GetActorForwardVector();
		CarrierDirection.Z = 0.0f;
		CarrierDirection = CarrierDirection.GetSafeNormal();
	}

	if (CarrierDirection.IsNearlyZero())
	{
		return false;
	}

	if (
		!bCarrierIsAutoPassing &&
		CarrierSpeed < AIPressOvertakeMinimumCarrierSpeed
		)
	{
		return false;
	}

	FVector CarrierRightDirection(
		-CarrierDirection.Y,
		CarrierDirection.X,
		0.0f
	);

	CarrierRightDirection = CarrierRightDirection.GetSafeNormal();

	if (CarrierRightDirection.IsNearlyZero())
	{
		return false;
	}

	const FVector CarrierToPressing =
		PressingLocation - CarrierLocation;

	const float DistanceToCarrier = CarrierToPressing.Size();
	const float LongitudinalOffset =
		FVector::DotProduct(CarrierToPressing, CarrierDirection);
	const float LateralOffset = FMath::Abs(
		FVector::DotProduct(CarrierToPressing, CarrierRightDirection)
	);

	const bool bPressingCharacterIsBehind =
		LongitudinalOffset <= -AIPressOvertakeMinimumBehindDistance;

	if (
		!bPressingCharacterIsBehind ||
		DistanceToCarrier > AIPressOvertakeActivationDistance ||
		LateralOffset > AIPressOvertakeMaximumLateralSeparation
		)
	{
		return false;
	}

	const float EffectiveCarrierSpeed =
		FMath::Max(CarrierSpeed, AIPressOvertakeMinimumCarrierSpeed);

	const float LeadDistance = FMath::Clamp(
		EffectiveCarrierSpeed * AIPressOvertakeLeadTime,
		AIPressOvertakeMinimumLeadDistance,
		AIPressOvertakeMaximumLeadDistance
	);

	const FVector PredictedCarrierLocation =
		CarrierLocation + CarrierDirection * LeadDistance;

	FVector OwnGoalLocation =
		MatchManager->GetOwnGoalCenterLocation(
			PressingCharacter->GetTeam()
		);

	OwnGoalLocation.Z = 0.0f;

	FVector PredictedCarrierToOwnGoal =
		OwnGoalLocation - PredictedCarrierLocation;

	PredictedCarrierToOwnGoal.Z = 0.0f;
	PredictedCarrierToOwnGoal =
		PredictedCarrierToOwnGoal.GetSafeNormal();

	int32 PreferredInnerSideSign = 0;

	if (!PredictedCarrierToOwnGoal.IsNearlyZero())
	{
		PreferredInnerSideSign =
			FVector::DotProduct(
				CarrierRightDirection,
				PredictedCarrierToOwnGoal
			) >= 0.0f
			? 1
			: -1;
	}

	struct FPressureSideCandidate
	{
		bool bValid = false;
		int32 SideSign = 0;
		FVector Location = FVector::ZeroVector;
		float Score = TNumericLimits<float>::Max();
	};

	auto BuildCandidate =
		[&](int32 SideSign) -> FPressureSideCandidate
		{
			FPressureSideCandidate Candidate;
			Candidate.SideSign = SideSign;

			FVector DesiredLocation =
				PredictedCarrierLocation +
				CarrierDirection * AIPressOvertakeForwardDistance +
				CarrierRightDirection *
				(static_cast<float>(SideSign) * AIPressOvertakeLateralDistance);

			DesiredLocation.Z = PressingCharacter->GetActorLocation().Z;

			FVector ProjectedLocation;

			if (
				!ProjectDefensivePressureLocationToNavigation(
					DesiredLocation,
					ProjectedLocation
				)
				)
			{
				return Candidate;
			}

			Candidate.bValid = true;
			Candidate.Location = ProjectedLocation;
			Candidate.Score = FVector::Dist2D(
				PressingCharacter->GetActorLocation(),
				ProjectedLocation
			);

			if (
				PreferredInnerSideSign != 0 &&
				SideSign == PreferredInnerSideSign
				)
			{
				Candidate.Score -= AIPressOvertakeInnerSideScoreBonus;
			}

			return Candidate;
		};

	const FPressureSideCandidate PositiveCandidate =
		BuildCandidate(1);
	const FPressureSideCandidate NegativeCandidate =
		BuildCandidate(-1);

	if (!PositiveCandidate.bValid && !NegativeCandidate.bValid)
	{
		return false;
	}

	UWorld* World = GetWorld();
	const float CurrentTime =
		World != nullptr
		? World->GetTimeSeconds()
		: 0.0f;

	const bool bSameCarrier =
		DefensivePressureOvertakeCarrier.Get() == Carrier &&
		bDefensivePressureOvertakeAgainstAutoPass ==
		bCarrierIsAutoPassing;

	const bool bDirectionChangedStrongly =
		bSameCarrier &&
		!DefensivePressureOvertakeCarrierDirection.IsNearlyZero() &&
		FVector::DotProduct(
			DefensivePressureOvertakeCarrierDirection,
			CarrierDirection
		) < AIPressOvertakeDirectionResetDot;

	const bool bMustChooseNewSide =
		!bSameCarrier ||
		DefensivePressureOvertakeSideSign == 0 ||
		bDirectionChangedStrongly;

	auto ChooseBestValidCandidate = [&]() -> FPressureSideCandidate
		{
			if (!PositiveCandidate.bValid)
			{
				return NegativeCandidate;
			}

			if (!NegativeCandidate.bValid)
			{
				return PositiveCandidate;
			}

			return
				PositiveCandidate.Score <= NegativeCandidate.Score
				? PositiveCandidate
				: NegativeCandidate;
		};

	FPressureSideCandidate SelectedCandidate;

	if (bMustChooseNewSide)
	{
		SelectedCandidate = ChooseBestValidCandidate();
		DefensivePressureOvertakeSideSign =
			SelectedCandidate.SideSign;
		DefensivePressureOvertakeSideCommitEndTime =
			CurrentTime + AIPressOvertakeSideCommitDuration;
	}
	else
	{
		const FPressureSideCandidate CurrentSideCandidate =
			DefensivePressureOvertakeSideSign > 0
			? PositiveCandidate
			: NegativeCandidate;

		const FPressureSideCandidate OtherSideCandidate =
			DefensivePressureOvertakeSideSign > 0
			? NegativeCandidate
			: PositiveCandidate;

		if (!CurrentSideCandidate.bValid)
		{
			SelectedCandidate = OtherSideCandidate;

			if (!SelectedCandidate.bValid)
			{
				return false;
			}

			DefensivePressureOvertakeSideSign =
				SelectedCandidate.SideSign;
			DefensivePressureOvertakeSideCommitEndTime =
				CurrentTime + AIPressOvertakeSideCommitDuration;
		}
		else
		{
			SelectedCandidate = CurrentSideCandidate;

			const bool bCommitFinished =
				CurrentTime >=
				DefensivePressureOvertakeSideCommitEndTime;

			const bool bOtherSideClearlyBetter =
				OtherSideCandidate.bValid &&
				OtherSideCandidate.Score +
				AIPressOvertakeSideSwitchRequiredAdvantage <
				CurrentSideCandidate.Score;

			if (bCommitFinished && bOtherSideClearlyBetter)
			{
				SelectedCandidate = OtherSideCandidate;
				DefensivePressureOvertakeSideSign =
					SelectedCandidate.SideSign;
				DefensivePressureOvertakeSideCommitEndTime =
					CurrentTime + AIPressOvertakeSideCommitDuration;
			}
		}
	}

	if (!SelectedCandidate.bValid)
	{
		return false;
	}

	DefensivePressureOvertakeCarrier = Carrier;
	bDefensivePressureOvertakeAgainstAutoPass =
		bCarrierIsAutoPassing;
	DefensivePressureOvertakeCarrierDirection = CarrierDirection;

	OutMoveLocation = SelectedCandidate.Location;
	return true;
}

bool ASoccerAIController::ProjectDefensivePressureLocationToNavigation(
	const FVector& DesiredLocation,
	FVector& OutProjectedLocation
) const
{
	OutProjectedLocation = DesiredLocation;

	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return false;
	}

	UNavigationSystemV1* NavigationSystem =
		FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);

	if (NavigationSystem == nullptr)
	{
		return true;
	}

	FNavLocation ProjectedLocation;

	const bool bProjected =
		NavigationSystem->ProjectPointToNavigation(
			DesiredLocation,
			ProjectedLocation,
			FVector(280.0f, 280.0f, 350.0f)
		);

	if (!bProjected)
	{
		return false;
	}

	OutProjectedLocation = ProjectedLocation.Location;
	return true;
}

void ASoccerAIController::ClearDefensivePressureOvertakeState()
{
	DefensivePressureOvertakeCarrier.Reset();
	bDefensivePressureOvertakeAgainstAutoPass = false;
	DefensivePressureOvertakeSideSign = 0;
	DefensivePressureOvertakeCarrierDirection = FVector::ZeroVector;
	DefensivePressureOvertakeSideCommitEndTime = -1000.0f;

	bHasDefensivePressureOvertakeMoveTarget = false;
	LastDefensivePressureOvertakeMoveTarget = FVector::ZeroVector;
	LastDefensivePressureOvertakeMoveRequestTime = -1000.0f;
}

bool ASoccerAIController::TryStartBasicAITackleInterception(
	ASoccerAICharacter* SoccerCharacter,
	ASoccerBall* SoccerBall,
	ESoccerAIOrder CurrentOrder
)
{
	if (
		!bUseBasicAITackleInterception ||
		!IsValid(MatchManager) ||
		!IsValid(SoccerCharacter) ||
		!IsValid(SoccerBall) ||
		MatchManager->GetMatchPlayState() != ESoccerMatchPlayState::Playing ||
		MatchManager->GetPossessingCharacter() != nullptr ||
		MatchManager->GetPossessionTeam() != ESoccerPossessionTeam::None ||
		SoccerCharacter->IsTackleActive() ||
		SoccerCharacter->IsTackleFallReactionActive() ||
		SoccerCharacter->IsAerialActionQueuedOrPlaying()
	)
	{
		return false;
	}

	if (
		CurrentOrder != ESoccerAIOrder::ChaseBall &&
		CurrentOrder != ESoccerAIOrder::PressBall &&
		CurrentOrder != ESoccerAIOrder::AttackRecoverBall
	)
	{
		return false;
	}

	if (
		SoccerCharacter->GetPlayerRole() == ESoccerPlayerRole::Goalkeeper &&
		!bAllowGoalkeeperBasicAITackle
	)
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return false;
	}

	const float CurrentTime = World->GetTimeSeconds();
	const float EffectiveTackleDecisionCooldown =
		FMath::Max(0.0f, AITackleDecisionCooldown) *
		GetDefensiveProfileTackleCooldownMultiplier(SoccerCharacter);

	if (
		CurrentTime - LastBasicAITackleAttemptTime <
		EffectiveTackleDecisionCooldown
	)
	{
		return false;
	}

	const FVector BallVelocity = SoccerBall->GetBallPhysicsVelocity();
	const float BallHorizontalSpeed = BallVelocity.Size2D();
	if (
		BallHorizontalSpeed < FMath::Max(0.0f, AITackleMinimumBallSpeed) ||
		!SoccerBall->IsNearGroundForPrediction() ||
		!SoccerBall->IsAvailableForTrajectoryPrediction()
	)
	{
		return false;
	}

	const float RunnerSpeed = SoccerCharacter->GetVelocity().Size2D();
	if (RunnerSpeed < FMath::Max(0.0f, AITackleMinimumRunnerSpeed))
	{
		return false;
	}

	const float CurrentBallDistance = FVector::Dist2D(
		SoccerCharacter->GetActorLocation(),
		SoccerBall->GetActorLocation()
	);

	if (
		CurrentBallDistance < FMath::Max(0.0f, AITackleMinimumBallDistance) ||
		CurrentBallDistance > FMath::Max(
			AITackleMinimumBallDistance,
			AITackleMaximumBallDistance
		)
	)
	{
		return false;
	}

	const float AnticipationHorizonMultiplier =
		GetDefensiveProfileAnticipationHorizonMultiplier(SoccerCharacter);

	TArray<FSoccerBallTrajectorySample> Trajectory;
	if (!SoccerBall->BuildPredictedGroundTrajectory(
		FMath::Max(
			0.10f,
			AITacklePredictionHorizon * AnticipationHorizonMultiplier
		),
		FMath::Max(0.02f, AITacklePredictionSampleInterval),
		Trajectory
	))
	{
		return false;
	}

	const FVector CharacterLocation = SoccerCharacter->GetActorLocation();
	FVector CharacterForward = SoccerCharacter->GetActorForwardVector();
	CharacterForward.Z = 0.0f;
	CharacterForward.Normalize();

	const float MinSampleTime = FMath::Max(0.0f, AITackleMinimumSampleTime);
	const float MaxSampleTime = FMath::Max(
		MinSampleTime,
		AITackleMaximumSampleTime * AnticipationHorizonMultiplier
	);
	const float EstimatedSlideSpeed = FMath::Max(
		1.0f,
		AITackleEstimatedAverageSlideSpeed
	);
	const float LaunchDelay = FMath::Max(0.0f, AITackleEstimatedLaunchDelay);
	const float RunLateMargin = FMath::Max(0.0f, AITackleRequiredRunLateMargin);
	const float TackleSafetyMargin = FMath::Max(0.0f, AITackleArrivalSafetyMargin);
	const float MaximumTurnCos = FMath::Cos(
		FMath::DegreesToRadians(
			FMath::Clamp(AITackleMaximumStartAngleDegrees, 0.0f, 180.0f)
		)
	);

	FVector SelectedTarget = FVector::ZeroVector;
	float SelectedSampleTime = 0.0f;
	float SelectedRunArrivalTime = 0.0f;
	float SelectedTackleArrivalTime = 0.0f;

	for (const FSoccerBallTrajectorySample& Sample : Trajectory)
	{
		if (
			Sample.TimeFromNow < MinSampleTime ||
			Sample.TimeFromNow > MaxSampleTime
		)
		{
			continue;
		}

		if (!Sample.bNearGround || Sample.bTrajectoryTerminated)
		{
			continue;
		}

		FVector ToTarget = Sample.Location - CharacterLocation;
		ToTarget.Z = 0.0f;
		const float TargetDistance = ToTarget.Size();

		if (
			TargetDistance < FMath::Max(0.0f, AITackleMinimumBallDistance) ||
			TargetDistance > FMath::Max(
				AITackleMinimumBallDistance,
				AITackleMaximumBallDistance
			)
		)
		{
			continue;
		}

		const FVector TargetDirection = ToTarget.GetSafeNormal();
		if (
			!CharacterForward.IsNearlyZero() &&
			FVector::DotProduct(CharacterForward, TargetDirection) < MaximumTurnCos
		)
		{
			continue;
		}

		const float RunArrivalTime =
			SoccerCharacter->EstimateArrivalTimeToLocation(Sample.Location);

		// Stage 1 only uses tackle when ordinary running is genuinely late.
		if (RunArrivalTime <= Sample.TimeFromNow + RunLateMargin)
		{
			continue;
		}

		const float TackleArrivalTime =
			LaunchDelay + TargetDistance / EstimatedSlideSpeed;

		if (
			TackleArrivalTime + TackleSafetyMargin >
			Sample.TimeFromNow
		)
		{
			continue;
		}

		if (!IsBasicAITackleLaneSafe(SoccerCharacter, Sample.Location))
		{
			continue;
		}

		SelectedTarget = Sample.Location;
		SelectedSampleTime = Sample.TimeFromNow;
		SelectedRunArrivalTime = RunArrivalTime;
		SelectedTackleArrivalTime = TackleArrivalTime;
		break;
	}

	if (SelectedTarget.IsNearlyZero())
	{
		return false;
	}

	LastBasicAITackleAttemptTime = CurrentTime;

	ClearDefensivePressureOvertakeState();
	ClearAerialBallInterceptionMovement();
	ClearPredictiveBallChaseMovement(SoccerCharacter);
	ClearFilteredMoveRequest();
	ClearFilteredDefenseMoveRequest();

	if (!SoccerCharacter->TryStartTackleTowardLocation(SelectedTarget))
	{
		return false;
	}

	ASoccerDebugManager::Message(
		this,
		ESoccerDebugCategory::Tackle,
		FString::Printf(
			TEXT("AI TACKLE INTERCEPT %s | ball=%.2fs run=%.2fs slide=%.2fs dist=%.0f"),
			*SoccerCharacter->GetName(),
			SelectedSampleTime,
			SelectedRunArrivalTime,
			SelectedTackleArrivalTime,
			FVector::Dist2D(CharacterLocation, SelectedTarget)
		),
		FColor::Cyan
	);

	return true;
}

bool ASoccerAIController::TryStartAITackleEvasion(
	ASoccerAICharacter* SoccerCharacter
)
{
	if (
		!bUseAITackleEvasion ||
		!IsValid(SoccerCharacter) ||
		SoccerCharacter->IsTackleActive() ||
		SoccerCharacter->IsTackleFallReactionActive() ||
		SoccerCharacter->IsTackleEvasionActive() ||
		SoccerCharacter->IsAerialActionQueuedOrPlaying()
	)
	{
		return false;
	}

	const float RunnerSpeed = SoccerCharacter->GetVelocity().Size2D();
	if (
		RunnerSpeed <
		FMath::Max(0.0f, AITackleEvasionMinimumRunnerSpeed)
	)
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return false;
	}

	ASoccerCharacterBase* BestThreat = nullptr;
	float BestTimeToThreat = TNumericLimits<float>::Max();
	float BestLateralDistance = TNumericLimits<float>::Max();

	const FVector VictimLocation = SoccerCharacter->GetActorLocation();
	const float MinReaction = FMath::Max(
		0.0f,
		AITackleEvasionMinimumReactionTime
	);
	const float MaxReaction = FMath::Max(
		MinReaction,
		AITackleEvasionMaximumReactionTime
	);
	const float ThreatHalfWidth = FMath::Max(
		1.0f,
		AITackleEvasionThreatHalfWidth
	);

	for (TActorIterator<ASoccerCharacterBase> It(World); It; ++It)
	{
		ASoccerCharacterBase* Tackler = *It;

		if (
			!IsValid(Tackler) ||
			Tackler == SoccerCharacter ||
			Tackler->GetTeam() == SoccerCharacter->GetTeam() ||
			!Tackler->IsTackleActive() ||
			Tackler->GetTacklePhase() != ESoccerTacklePhase::Sliding ||
			Tackler->GetTackleNormalizedTime() >
				FMath::Clamp(
					AITackleEvasionMaximumTacklerNormalizedTime,
					0.0f,
					1.0f
				)
		)
		{
			continue;
		}

		const FVector SlideDirection =
			Tackler->GetActiveTackleDirection();
		const float SlideSpeed =
			Tackler->GetCurrentTackleHorizontalSpeed();

		if (
			SlideDirection.IsNearlyZero() ||
			SlideSpeed <
				FMath::Max(
					1.0f,
					AITackleEvasionMinimumIncomingSlideSpeed
				)
		)
		{
			continue;
		}

		FVector ToVictim =
			VictimLocation - Tackler->GetActorLocation();
		ToVictim.Z = 0.0f;

		const float AlongDistance =
			FVector::DotProduct(ToVictim, SlideDirection);

		// Challenge has to be travelling toward a point ahead of the tackler.
		if (AlongDistance <= 0.0f)
		{
			continue;
		}

		const float TimeToThreat = AlongDistance / SlideSpeed;
		if (
			TimeToThreat < MinReaction ||
			TimeToThreat > MaxReaction
		)
		{
			continue;
		}

		const FVector ClosestPoint =
			Tackler->GetActorLocation() +
			SlideDirection * AlongDistance;

		const float LateralDistance =
			FVector::Dist2D(VictimLocation, ClosestPoint);

		if (LateralDistance > ThreatHalfWidth)
		{
			continue;
		}

		if (TimeToThreat < BestTimeToThreat)
		{
			BestThreat = Tackler;
			BestTimeToThreat = TimeToThreat;
			BestLateralDistance = LateralDistance;
		}
	}

	if (!IsValid(BestThreat))
	{
		return false;
	}

	if (!SoccerCharacter->TryStartTackleEvasion(BestThreat))
	{
		return false;
	}

	ASoccerDebugManager::Message(
		this,
		ESoccerDebugCategory::Tackle,
		FString::Printf(
			TEXT("AI TACKLE EVADE %s <- %s | t=%.2f lateral=%.0f"),
			*SoccerCharacter->GetName(),
			*BestThreat->GetName(),
			BestTimeToThreat,
			BestLateralDistance
		),
		FColor::Green
	);

	return true;
}

bool ASoccerAIController::TryStartContestedAITackle(
	ASoccerAICharacter* SoccerCharacter,
	ASoccerBall* SoccerBall,
	ESoccerAIOrder CurrentOrder
)
{
	if (
		!bUseContestedAITackle ||
		!IsValid(MatchManager) ||
		!IsValid(SoccerCharacter) ||
		!IsValid(SoccerBall) ||
		MatchManager->GetMatchPlayState() != ESoccerMatchPlayState::Playing ||
		SoccerCharacter->IsTackleActive() ||
		SoccerCharacter->IsTackleFallReactionActive() ||
		SoccerCharacter->IsAerialActionQueuedOrPlaying()
	)
	{
		return false;
	}

	if (
		CurrentOrder != ESoccerAIOrder::ChaseBall &&
		CurrentOrder != ESoccerAIOrder::PressBall &&
		CurrentOrder != ESoccerAIOrder::AttackRecoverBall
	)
	{
		return false;
	}

	ASoccerCharacterBase* Carrier =
		MatchManager->GetPossessingCharacter();

	if (
		!IsValid(Carrier) ||
		Carrier == SoccerCharacter ||
		Carrier->GetTeam() == SoccerCharacter->GetTeam()
	)
	{
		return false;
	}

	if (
		SoccerCharacter->GetPlayerRole() == ESoccerPlayerRole::Goalkeeper &&
		!bAllowGoalkeeperContestedAITackle
	)
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return false;
	}

	const float CurrentTime = World->GetTimeSeconds();
	const float EffectiveContestedTackleCooldown =
		FMath::Max(0.0f, AIContestedTackleDecisionCooldown) *
		GetDefensiveProfileTackleCooldownMultiplier(SoccerCharacter);

	if (
		CurrentTime - LastContestedAITackleAttemptTime <
		EffectiveContestedTackleCooldown
	)
	{
		return false;
	}

	const FVector CharacterLocation = SoccerCharacter->GetActorLocation();
	const FVector CarrierLocation = Carrier->GetActorLocation();
	const FVector BallLocation = SoccerBall->GetActorLocation();

	const float CurrentDistance =
		FVector::Dist2D(CharacterLocation, BallLocation);

	const float MinDistance =
		FMath::Max(0.0f, AIContestedTackleMinimumDistance);
	const float MaxDistance =
		FMath::Max(MinDistance + 1.0f, AIContestedTackleMaximumDistance);

	if (
		CurrentDistance < MinDistance ||
		CurrentDistance > MaxDistance
	)
	{
		return false;
	}

	FVector CharacterForward = SoccerCharacter->GetActorForwardVector();
	CharacterForward.Z = 0.0f;
	CharacterForward.Normalize();

	const float LeadTime =
		FMath::Max(0.0f, AIContestedTackleLeadTime) *
		GetDefensiveProfileAnticipationHorizonMultiplier(SoccerCharacter);

	FVector CarrierVelocity = Carrier->GetVelocity();
	CarrierVelocity.Z = 0.0f;

	FVector TargetLocation =
		BallLocation + CarrierVelocity * LeadTime;
	TargetLocation.Z = BallLocation.Z;

	FVector ToTarget = TargetLocation - CharacterLocation;
	ToTarget.Z = 0.0f;

	const float TargetDistance = ToTarget.Size();
	if (
		TargetDistance < MinDistance ||
		TargetDistance > MaxDistance
	)
	{
		return false;
	}

	const FVector TargetDirection = ToTarget.GetSafeNormal();

	const float MaximumTurnCos = FMath::Cos(
		FMath::DegreesToRadians(
			FMath::Clamp(
				AIContestedTackleMaximumStartAngleDegrees,
				0.0f,
				180.0f
			)
		)
	);

	if (
		!CharacterForward.IsNearlyZero() &&
		FVector::DotProduct(CharacterForward, TargetDirection) <
			MaximumTurnCos
	)
	{
		return false;
	}

	FVector CarrierForward = Carrier->GetActorForwardVector();
	CarrierForward.Z = 0.0f;
	CarrierForward.Normalize();

	FVector CarrierToTackler = CharacterLocation - CarrierLocation;
	CarrierToTackler.Z = 0.0f;
	CarrierToTackler.Normalize();

	const float CarrierSideDot =
		CarrierForward.IsNearlyZero() ||
		CarrierToTackler.IsNearlyZero()
		? 0.0f
		: FVector::DotProduct(CarrierForward, CarrierToTackler);

	if (
		CarrierSideDot <
		FMath::Clamp(
			AIContestedTackleMinimumCarrierSideDot,
			-1.0f,
			1.0f
		)
	)
	{
		return false;
	}

	FVector CarrierToBall = BallLocation - CarrierLocation;
	CarrierToBall.Z = 0.0f;
	const float BallExposureDistance = CarrierToBall.Size();

	const FVector CarrierToBallDirection =
		CarrierToBall.GetSafeNormal();

	const float BallFrontDot =
		CarrierForward.IsNearlyZero() ||
		CarrierToBallDirection.IsNearlyZero()
		? 0.0f
		: FVector::DotProduct(
			CarrierForward,
			CarrierToBallDirection
		);

	if (
		BallFrontDot <
		FMath::Clamp(
			AIContestedTackleMinimumBallFrontDot,
			-1.0f,
			1.0f
		)
	)
	{
		return false;
	}

	if (
		!bAllowContestedAITackleInOwnPenaltyArea &&
		MatchManager->IsLocationInsidePenaltyAreaForTeam(
			TargetLocation,
			SoccerCharacter->GetTeam()
		)
	)
	{
		return false;
	}

	// The intended carrier is allowed in the slide corridor. Any other
	// opponent still vetoes the challenge in this stage.
	if (
		!IsBasicAITackleLaneSafe(
			SoccerCharacter,
			TargetLocation,
			Carrier
		)
	)
	{
		return false;
	}

	const float LowExposure =
		FMath::Max(0.0f, AIContestedTackleLowExposureDistance);
	const float HighExposure =
		FMath::Max(
			LowExposure + 1.0f,
			AIContestedTackleHighExposureDistance
		);

	const float ExposureScore = FMath::Clamp(
		(BallExposureDistance - LowExposure) /
			(HighExposure - LowExposure),
		0.0f,
		1.0f
	);

	// +1 means the tackler is in front of the carrier, -1 behind.
	const float ApproachScore = FMath::Clamp(
		(CarrierSideDot + 0.30f) / 1.30f,
		0.0f,
		1.0f
	);

	const float DistanceScore = 1.0f - FMath::Clamp(
		(TargetDistance - MinDistance) /
			FMath::Max(1.0f, MaxDistance - MinDistance),
		0.0f,
		1.0f
	);

	// Ball exposure is the strongest factor, followed by approach geometry.
	const float DecisionScore =
		ExposureScore * 0.45f +
		ApproachScore * 0.35f +
		DistanceScore * 0.20f;

	const float EffectiveMinimumDecisionScore = FMath::Clamp(
		AIContestedTackleMinimumDecisionScore +
			GetDefensiveProfileContestedTackleScoreAdjustment(SoccerCharacter),
		0.0f,
		1.0f
	);

	if (DecisionScore < EffectiveMinimumDecisionScore)
	{
		return false;
	}

	const float EstimatedSlideSpeed = FMath::Max(
		1.0f,
		AIContestedTackleEstimatedAverageSlideSpeed
	);
	const float EstimatedArrivalTime =
		FMath::Max(0.0f, AIContestedTackleEstimatedLaunchDelay) +
		TargetDistance / EstimatedSlideSpeed;

	LastContestedAITackleAttemptTime = CurrentTime;

	ClearDefensivePressureOvertakeState();
	ClearAerialBallInterceptionMovement();
	ClearPredictiveBallChaseMovement(SoccerCharacter);
	ClearFilteredMoveRequest();
	ClearFilteredDefenseMoveRequest();

	if (!SoccerCharacter->TryStartTackleTowardLocation(TargetLocation))
	{
		return false;
	}

	ASoccerDebugManager::Message(
		this,
		ESoccerDebugCategory::Tackle,
		FString::Printf(
			TEXT("AI TACKLE CONTEST %s -> %s | score=%.2f exposure=%.0f side=%.2f front=%.2f dist=%.0f eta=%.2f"),
			*SoccerCharacter->GetName(),
			*Carrier->GetName(),
			DecisionScore,
			BallExposureDistance,
			CarrierSideDot,
			BallFrontDot,
			TargetDistance,
			EstimatedArrivalTime
		),
		FColor::Orange
	);

	return true;
}

bool ASoccerAIController::IsBasicAITackleLaneSafe(
	const ASoccerAICharacter* SoccerCharacter,
	const FVector& TargetLocation,
	const ASoccerCharacterBase* IgnoredOpponent
) const
{
	if (!IsValid(SoccerCharacter))
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return false;
	}

	const FVector StartLocation = SoccerCharacter->GetActorLocation();
	const float LaneHalfWidth = FMath::Max(0.0f, AITackleOpponentLaneHalfWidth);
	const float TargetSafetyRadius = FMath::Max(
		0.0f,
		AITackleOpponentTargetSafetyRadius
	);

	for (TActorIterator<ASoccerCharacterBase> It(World); It; ++It)
	{
		ASoccerCharacterBase* Candidate = *It;
		if (
			!IsValid(Candidate) ||
			Candidate == SoccerCharacter ||
			Candidate == IgnoredOpponent ||
			Candidate->GetTeam() == SoccerCharacter->GetTeam()
		)
		{
			continue;
		}

		const FVector CandidateLocation = Candidate->GetActorLocation();
		if (
			FVector::Dist2D(CandidateLocation, TargetLocation) <=
			TargetSafetyRadius
		)
		{
			return false;
		}

		FVector ClosestPoint = FMath::ClosestPointOnSegment(
			CandidateLocation,
			StartLocation,
			TargetLocation
		);
		ClosestPoint.Z = CandidateLocation.Z;

		if (
			FVector::Dist2D(CandidateLocation, ClosestPoint) <=
			LaneHalfWidth
		)
		{
			return false;
		}
	}

	return true;
}

void ASoccerAIController::ClearPredictiveBallChaseMovement(
	ASoccerAICharacter* SoccerCharacter
)
{
	bHasPredictiveBallMoveTarget = false;
	LastPredictiveBallMoveTarget = FVector::ZeroVector;
	LastPredictiveBallMoveRequestTime = -1000.0f;
	LastPredictiveBallMoveOrder = ESoccerAIOrder::ReturnHome;

	if (IsValid(SoccerCharacter))
	{
		SoccerCharacter->ClearBallPursuitTarget();
	}
}

float ASoccerAIController::GetGoalkeeperProfileDecisionHorizon(
	const ASoccerAICharacter* SoccerCharacter
) const
{
	if (!IsValid(SoccerCharacter) || !SoccerCharacter->HasPlayerProfile())
	{
		return GoalkeeperSaveDecisionTimeHorizon;
	}

	const float ReflexAlpha =
		SoccerCharacter->GetPlayerProfileGoalkeeperReflexesAlpha();

	const float HorizonMultiplier = FMath::Lerp(
		GoalkeeperReflexDecisionHorizonMultiplierAtZero,
		GoalkeeperReflexDecisionHorizonMultiplierAtHundred,
		ReflexAlpha
	);

	return FMath::Max(0.05f, GoalkeeperSaveDecisionTimeHorizon * HorizonMultiplier);
}

float ASoccerAIController::GetGoalkeeperProfileAdditionalReactionDelay(
	const ASoccerAICharacter* SoccerCharacter
) const
{
	if (!IsValid(SoccerCharacter) || !SoccerCharacter->HasPlayerProfile())
	{
		return 0.0f;
	}

	return FMath::Max(
		0.0f,
		FMath::Lerp(
			GoalkeeperReflexAdditionalStartDelayAtZero,
			GoalkeeperReflexAdditionalStartDelayAtHundred,
			SoccerCharacter->GetPlayerProfileGoalkeeperReflexesAlpha()
		)
	);
}

FVector ASoccerAIController::ApplyGoalkeeperProfilePositioningExecution(
	const ASoccerAICharacter* SoccerCharacter,
	const FVector& IdealTargetLocation
) const
{
	if (
		!IsValid(SoccerCharacter) ||
		!SoccerCharacter->HasPlayerProfile() ||
		IdealTargetLocation.IsNearlyZero()
	)
	{
		return IdealTargetLocation;
	}

	const float PositioningAlpha =
		SoccerCharacter->GetPlayerProfileGoalkeeperPositioningAlpha();

	const float MaximumLateralError = FMath::Lerp(
		GoalkeeperPositioningLateralErrorCmAtZero,
		GoalkeeperPositioningLateralErrorCmAtHundred,
		PositioningAlpha
	);

	const float MaximumDepthError = FMath::Lerp(
		GoalkeeperPositioningDepthErrorCmAtZero,
		GoalkeeperPositioningDepthErrorCmAtHundred,
		PositioningAlpha
	);

	const uint32 ProfileHash = GetTypeHash(SoccerCharacter->GetPlayerProfileId());
	const float LateralSign = (ProfileHash & 1u) != 0u ? 1.0f : -1.0f;
	const float DepthSign = (ProfileHash & 2u) != 0u ? 1.0f : -1.0f;
	const float LateralMagnitude = 0.65f + 0.35f * static_cast<float>((ProfileHash >> 2) & 255u) / 255.0f;
	const float DepthMagnitude = 0.55f + 0.45f * static_cast<float>((ProfileHash >> 10) & 255u) / 255.0f;

	FVector RightDirection = GetGoalkeeperRightDirection(SoccerCharacter);
	RightDirection.Z = 0.0f;
	RightDirection = RightDirection.GetSafeNormal();

	FVector OutfieldDirection = GetGoalkeeperOutfieldDirection(SoccerCharacter);
	OutfieldDirection.Z = 0.0f;
	OutfieldDirection = OutfieldDirection.GetSafeNormal();

	if (RightDirection.IsNearlyZero() || OutfieldDirection.IsNearlyZero())
	{
		return IdealTargetLocation;
	}

	FVector AdjustedTargetLocation = IdealTargetLocation;
	AdjustedTargetLocation += RightDirection * (MaximumLateralError * LateralMagnitude * LateralSign);
	AdjustedTargetLocation += OutfieldDirection * (MaximumDepthError * DepthMagnitude * DepthSign);
	AdjustedTargetLocation.Z = IdealTargetLocation.Z;

	return AdjustedTargetLocation;
}

float ASoccerAIController::GetGoalkeeperProfileCatchRadius(
	const ASoccerAICharacter* SoccerCharacter
) const
{
	if (!IsValid(SoccerCharacter) || !SoccerCharacter->HasPlayerProfile())
	{
		return GoalkeeperHandContactRadius;
	}

	const float HandlingAlpha =
		SoccerCharacter->GetPlayerProfileGoalkeeperHandlingAlpha();

	const float RadiusMultiplier = FMath::Lerp(
		GoalkeeperHandlingCatchRadiusMultiplierAtZero,
		GoalkeeperHandlingCatchRadiusMultiplierAtHundred,
		HandlingAlpha
	);

	return FMath::Max(1.0f, GoalkeeperHandContactRadius * RadiusMultiplier);
}

float ASoccerAIController::GetGoalkeeperProfileAdaptiveLateralMaximumExtraDistance(
	const ASoccerAICharacter* SoccerCharacter
) const
{
	if (!IsValid(SoccerCharacter) || !SoccerCharacter->HasPlayerProfile())
	{
		return GoalkeeperAdaptiveLateralMaximumExtraDistance;
	}

	const float DivingAlpha =
		SoccerCharacter->GetPlayerProfileGoalkeeperDivingAlpha();

	const float ReachMultiplier = FMath::Lerp(
		GoalkeeperDivingAdaptiveReachMultiplierAtZero,
		GoalkeeperDivingAdaptiveReachMultiplierAtHundred,
		DivingAlpha
	);

	return FMath::Max(0.0f, GoalkeeperAdaptiveLateralMaximumExtraDistance * ReachMultiplier);
}

FVector ASoccerAIController::ApplyGoalkeeperProfileDistributionTargetExecution(
	const ASoccerAICharacter* SoccerCharacter,
	const FVector& IntendedTargetLocation
) const
{
	if (
		!IsValid(SoccerCharacter) ||
		!SoccerCharacter->HasPlayerProfile() ||
		IntendedTargetLocation.IsNearlyZero()
	)
	{
		return IntendedTargetLocation;
	}

	FVector ToTarget = IntendedTargetLocation - SoccerCharacter->GetActorLocation();
	const float TargetHeight = ToTarget.Z;
	ToTarget.Z = 0.0f;

	const float TargetDistance = ToTarget.Size();
	if (TargetDistance <= KINDA_SMALL_NUMBER)
	{
		return IntendedTargetLocation;
	}

	const float DistributionAlpha =
		SoccerCharacter->GetPlayerProfileGoalkeeperDistributionAlpha();

	const float MaximumAngleDegrees = FMath::Lerp(
		GoalkeeperDistributionMaxAngularErrorDegreesAtZero,
		GoalkeeperDistributionMaxAngularErrorDegreesAtHundred,
		DistributionAlpha
	);

	const uint32 ProfileHash = GetTypeHash(SoccerCharacter->GetPlayerProfileId());
	const float ErrorSign = (ProfileHash & 4u) != 0u ? 1.0f : -1.0f;
	const float ErrorMagnitude = 0.55f + 0.45f * static_cast<float>((ProfileHash >> 18) & 255u) / 255.0f;
	const float ErrorAngleRadians = FMath::DegreesToRadians(MaximumAngleDegrees * ErrorMagnitude * ErrorSign);

	const FVector HorizontalDirection = ToTarget.GetSafeNormal();
	const FVector RotatedDirection = HorizontalDirection.RotateAngleAxis(
		FMath::RadiansToDegrees(ErrorAngleRadians),
		FVector::UpVector
	);

	FVector AdjustedTargetLocation = SoccerCharacter->GetActorLocation() + RotatedDirection * TargetDistance;
	AdjustedTargetLocation.Z += TargetHeight;
	return AdjustedTargetLocation;
}

float ASoccerAIController::GetGoalkeeperProfileDistributionSpeedMultiplier(
	const ASoccerAICharacter* SoccerCharacter
) const
{
	if (!IsValid(SoccerCharacter) || !SoccerCharacter->HasPlayerProfile())
	{
		return 1.0f;
	}

	return FMath::Lerp(
		GoalkeeperDistributionSpeedMultiplierAtZero,
		GoalkeeperDistributionSpeedMultiplierAtHundred,
		SoccerCharacter->GetPlayerProfileGoalkeeperDistributionAlpha()
	);
}

float ASoccerAIController::BuildAIReactionDelayForCharacter(
	const ASoccerAICharacter* SoccerCharacter
) const
{
	const float SafeMin = FMath::Max(0.0f, AIReactionDelayMin);
	const float SafeMax = FMath::Max(SafeMin, AIReactionDelayMax);
	const float BaseDelay = FMath::FRandRange(SafeMin, SafeMax);

	if (
		!IsValid(SoccerCharacter) ||
		!SoccerCharacter->HasPlayerProfile() ||
		!IsValid(MatchManager) ||
		MatchManager->GetTeamPhase(SoccerCharacter->GetTeam()) != ESoccerTeamPhase::Defending
	)
	{
		return BaseDelay;
	}

	return BaseDelay * GetDefensiveProfileReactionMultiplier(SoccerCharacter);
}

float ASoccerAIController::GetOffensiveProfilePressureAlpha(
	const ASoccerAICharacter* SoccerCharacter
) const
{
	if (!IsValid(SoccerCharacter))
	{
		return 0.0f;
	}

	const float SafePressureRadius = FMath::Max(
		50.0f,
		OffensiveDecisionPressureRadius
	);

	ASoccerCharacterBase* ClosestPressureOpponent =
		FindClosestOpponentPressure(
			SoccerCharacter,
			SafePressureRadius
		);

	if (!IsValid(ClosestPressureOpponent))
	{
		return 0.0f;
	}

	const float PressureDistance = FVector::Dist2D(
		SoccerCharacter->GetActorLocation(),
		ClosestPressureOpponent->GetActorLocation()
	);

	return 1.0f - FMath::Clamp(
		PressureDistance / SafePressureRadius,
		0.0f,
		1.0f
	);
}

float ASoccerAIController::GetOffensiveProfileDecisionDelay(
	const ASoccerAICharacter* SoccerCharacter
) const
{
	if (!IsValid(SoccerCharacter) || !SoccerCharacter->HasPlayerProfile())
	{
		return 0.0f;
	}

	const float DecisionAlpha =
		SoccerCharacter->GetPlayerProfileDecisionMakingAlpha();

	const float AnticipationAlpha =
		SoccerCharacter->GetPlayerProfileAnticipationAlpha();

	const float ComposureAlpha =
		SoccerCharacter->GetPlayerProfileComposureAlpha();

	float DecisionDelay = FMath::Lerp(
		OffensiveDecisionInitialDelayAtZero,
		OffensiveDecisionInitialDelayAtHundred,
		DecisionAlpha
	);

	DecisionDelay *= FMath::Lerp(
		OffensiveAnticipationDecisionDelayMultiplierAtZero,
		OffensiveAnticipationDecisionDelayMultiplierAtHundred,
		AnticipationAlpha
	);

	const float PressureAlpha =
		GetOffensiveProfilePressureAlpha(SoccerCharacter);

	const float FullPressureComposureMultiplier = FMath::Lerp(
		OffensiveComposurePressureDelayMultiplierAtZero,
		OffensiveComposurePressureDelayMultiplierAtHundred,
		ComposureAlpha
	);

	DecisionDelay *= FMath::Lerp(
		1.0f,
		FullPressureComposureMultiplier,
		PressureAlpha
	);

	return FMath::Max(0.0f, DecisionDelay);
}

void ASoccerAIController::BeginOffensiveDecisionEpisode(
	const ASoccerAICharacter* SoccerCharacter,
	bool bOwnAutoPassContinuation
)
{
	if (!IsValid(SoccerCharacter))
	{
		return;
	}

	EvaluatedOffensivePossessionSequence =
		SoccerCharacter->GetAIPossessionSequence();

	if (!SoccerCharacter->HasPlayerProfile())
	{
		ActiveOffensiveDecisionEpisode = ESoccerAIDecisionEpisode::Fluid;
		ActiveOffensiveDecisionReadyTime = -1000.0f;
		return;
	}

	if (!bOffensiveDecisionRandomInitialized)
	{
		const uint32 CharacterHash = GetTypeHash(SoccerCharacter->GetFName());
		OffensiveDecisionRandomStream.Initialize(
			OffensiveDecisionRandomSeed ^ static_cast<int32>(CharacterHash)
		);
		bOffensiveDecisionRandomInitialized = true;
	}

	if (bOwnAutoPassContinuation && FluentOwnTouchEpisodesRemaining > 0)
	{
		--FluentOwnTouchEpisodesRemaining;
		ActiveOffensiveDecisionEpisode = ESoccerAIDecisionEpisode::Fluid;
		ActiveOffensiveDecisionReadyTime = -1000.0f;
		return;
	}

	const float DecisionAlpha =
		SoccerCharacter->GetPlayerProfileDecisionMakingAlpha();
	const float AnticipationAlpha =
		SoccerCharacter->GetPlayerProfileAnticipationAlpha();
	const float ComposureAlpha =
		SoccerCharacter->GetPlayerProfileComposureAlpha();
	const float PressureAlpha =
		GetOffensiveProfilePressureAlpha(SoccerCharacter);

	float HesitationChance = FMath::Lerp(
		OffensiveHesitationChanceAtZero,
		OffensiveHesitationChanceAtHundred,
		DecisionAlpha
	);

	// Anticipation reduces how often a player is caught without a prepared idea.
	HesitationChance *= FMath::Lerp(1.18f, 0.82f, AnticipationAlpha);
	HesitationChance += PressureAlpha * FMath::Lerp(
		OffensivePressureHesitationChanceAtZeroComposure,
		OffensivePressureHesitationChanceAtHundredComposure,
		ComposureAlpha
	);
	HesitationChance = FMath::Clamp(HesitationChance, 0.02f, 0.75f);

	if (OffensiveDecisionRandomStream.FRand() >= HesitationChance)
	{
		ActiveOffensiveDecisionEpisode = ESoccerAIDecisionEpisode::Fluid;
		ActiveOffensiveDecisionReadyTime = -1000.0f;

		if (bOwnAutoPassContinuation)
		{
			const int32 MinStreak = FMath::RoundToInt(FMath::Lerp(1.0f, 3.0f, DecisionAlpha));
			const int32 MaxStreak = FMath::RoundToInt(FMath::Lerp(3.0f, 7.0f, DecisionAlpha));
			FluentOwnTouchEpisodesRemaining =
				OffensiveDecisionRandomStream.RandRange(MinStreak, MaxStreak);
		}
		return;
	}

	const float StrongChance = FMath::Clamp(
		FMath::Lerp(0.48f, 0.16f, DecisionAlpha) + PressureAlpha * 0.12f,
		0.10f,
		0.65f
	);

	const bool bStrong =
		OffensiveDecisionRandomStream.FRand() < StrongChance;

	ActiveOffensiveDecisionEpisode = bStrong
		? ESoccerAIDecisionEpisode::StrongHesitation
		: ESoccerAIDecisionEpisode::MildHesitation;

	const float Duration = bStrong
		? OffensiveDecisionRandomStream.FRandRange(
			OffensiveStrongHesitationMinDuration,
			OffensiveStrongHesitationMaxDuration
		)
		: OffensiveDecisionRandomStream.FRandRange(
			OffensiveMildHesitationMinDuration,
			OffensiveMildHesitationMaxDuration
		);

	const float SkillDurationMultiplier = FMath::Lerp(1.25f, 0.75f, DecisionAlpha);
	const float AnticipationDurationMultiplier = FMath::Lerp(1.12f, 0.88f, AnticipationAlpha);
	const float CurrentTime = GetWorld() != nullptr
		? GetWorld()->GetTimeSeconds()
		: 0.0f;

	ActiveOffensiveDecisionReadyTime = CurrentTime +
		Duration * SkillDurationMultiplier * AnticipationDurationMultiplier;
}

void ASoccerAIController::EnsureOffensiveDecisionEpisode(
	const ASoccerAICharacter* SoccerCharacter
)
{
	if (
		IsValid(SoccerCharacter) &&
		EvaluatedOffensivePossessionSequence !=
			SoccerCharacter->GetAIPossessionSequence()
		)
	{
		BeginOffensiveDecisionEpisode(SoccerCharacter, false);
	}
}

bool ASoccerAIController::UpdatePhysicalPossessionApproach(
	ASoccerAICharacter* SoccerCharacter,
	bool bTrackMovingBallWhileHesitating
)
{
	if (
		!bUseAIPhysicalPossessionApproach ||
		!IsValid(SoccerCharacter) ||
		!SoccerCharacter->IsAIPossessingBall() ||
		SoccerCharacter->IsGoalkeeperHoldingBall() ||
		!IsValid(MatchManager)
		)
	{
		return false;
	}

	ASoccerBall* SoccerBall = MatchManager->GetSoccerBall();
	if (!IsValid(SoccerBall))
	{
		return false;
	}

	FVector BallVelocity = SoccerBall->GetBallPhysicsVelocity();
	BallVelocity.Z = 0.0f;
	const float BallSpeed = BallVelocity.Size();
	const float DistanceToBall = FVector::Dist2D(
		SoccerCharacter->GetActorLocation(),
		SoccerBall->GetActorLocation()
	);

	const bool bNeedsContactApproach =
		DistanceToBall > FMath::Max(1.0f, AIPhysicalKickReadyDistance);
	const bool bNeedsHesitationTracking =
		bTrackMovingBallWhileHesitating &&
		BallSpeed >= FMath::Max(0.0f, AIPhysicalHesitationTrackMinBallSpeed);

	if (!bNeedsContactApproach && !bNeedsHesitationTracking)
	{
		return false;
	}

	FVector ApproachDirection = BallVelocity.GetSafeNormal();
	if (ApproachDirection.IsNearlyZero())
	{
		ApproachDirection =
			SoccerBall->GetActorLocation() - SoccerCharacter->GetActorLocation();
		ApproachDirection.Z = 0.0f;
		ApproachDirection = ApproachDirection.GetSafeNormal();
	}
	if (ApproachDirection.IsNearlyZero())
	{
		ApproachDirection = SoccerCharacter->GetActorForwardVector();
		ApproachDirection.Z = 0.0f;
		ApproachDirection = ApproachDirection.GetSafeNormal();
	}

	FVector PredictedBallLocation = SoccerBall->GetActorLocation() +
		BallVelocity * FMath::Clamp(AIPhysicalBallPredictionTime, 0.0f, 0.5f);
	FVector ApproachLocation = PredictedBallLocation -
		ApproachDirection * FMath::Max(0.0f, AIPhysicalApproachBehindDistance);
	ApproachLocation.Z = SoccerCharacter->GetActorLocation().Z;

	if (
		FVector::Dist2D(SoccerCharacter->GetActorLocation(), ApproachLocation) <=
		FMath::Max(1.0f, AIPhysicalApproachAcceptanceRadius)
		)
	{
		return false;
	}

	SoccerCharacter->RequestAIMovementMode(
		BallSpeed >= FMath::Max(0.0f, AIPhysicalApproachFastRunBallSpeed)
			? ESoccerAIMovementMode::FastRun
			: ESoccerAIMovementMode::Run,
		ESoccerAIMovementReason::ChaseOwnAutoPass,
		true
	);

	MoveToLocationWithAIMovement(
		ESoccerAIOrder::AttackRunIntoSpace,
		ApproachLocation,
		AIPhysicalApproachAcceptanceRadius,
		false
	);
	SetFocalPoint(PredictedBallLocation, EAIFocusPriority::Gameplay);

	return true;
}

bool ASoccerAIController::IsOffensiveProfileDecisionReady(
	const ASoccerAICharacter* SoccerCharacter
)
{
	if (!IsValid(SoccerCharacter) || !SoccerCharacter->HasPlayerProfile())
	{
		return true;
	}

	if (!SoccerCharacter->IsAIPossessingBall())
	{
		return true;
	}

	EnsureOffensiveDecisionEpisode(SoccerCharacter);

	if (ActiveOffensiveDecisionEpisode == ESoccerAIDecisionEpisode::Fluid)
	{
		return true;
	}

	const float CurrentTime = GetWorld() != nullptr
		? GetWorld()->GetTimeSeconds()
		: 0.0f;

	return CurrentTime >= ActiveOffensiveDecisionReadyTime;
}

bool ASoccerAIController::ShouldOffensiveProfileAcceptPreferredShot(
	const ASoccerAICharacter* SoccerCharacter
) const
{
	if (!IsValid(SoccerCharacter) || !SoccerCharacter->HasPlayerProfile())
	{
		return true;
	}

	if (!IsValid(MatchManager))
	{
		return true;
	}

	const FVector PreferredShotCenter =
		MatchManager->GetShotTargetLocation(SoccerCharacter);

	const float PreferredShotDistance = FVector::Dist2D(
		SoccerCharacter->GetActorLocation(),
		PreferredShotCenter
	);

	const float DecisionAlpha =
		SoccerCharacter->GetPlayerProfileDecisionMakingAlpha();

	const float ComposureAlpha =
		SoccerCharacter->GetPlayerProfileComposureAlpha();

	float AllowedDistanceFraction = FMath::Lerp(
		PreferredShotDistanceFractionAtZeroDecision,
		PreferredShotDistanceFractionAtHundredDecision,
		DecisionAlpha
	);

	const float PressureAlpha =
		GetOffensiveProfilePressureAlpha(SoccerCharacter);

	AllowedDistanceFraction -=
		PressureAlpha *
		(1.0f - ComposureAlpha) *
		FMath::Max(0.0f, LowComposurePreferredShotDistancePenalty);

	AllowedDistanceFraction = FMath::Clamp(
		AllowedDistanceFraction,
		0.10f,
		1.0f
	);

	return PreferredShotDistance <=
		FMath::Max(1.0f, AIShotDistanceToTarget) *
		AllowedDistanceFraction;
}

float ASoccerAIController::GetOffensiveProfileRequiredPassScore(
	const ASoccerAICharacter* SoccerCharacter,
	bool bUsePossessionRetentionThreshold
) const
{
	if (!IsValid(SoccerCharacter) || !SoccerCharacter->HasPlayerProfile())
	{
		return 0.0f;
	}

	const float DecisionAlpha =
		SoccerCharacter->GetPlayerProfileDecisionMakingAlpha();

	const float ComposureAlpha =
		SoccerCharacter->GetPlayerProfileComposureAlpha();

	float RequiredPassScore = bUsePossessionRetentionThreshold
		? FMath::Lerp(
			DecisionRetentionPassMinimumScoreAtZero,
			DecisionRetentionPassMinimumScoreAtHundred,
			DecisionAlpha
		)
		: FMath::Lerp(
			DecisionSmartPassMinimumScoreAtZero,
			DecisionSmartPassMinimumScoreAtHundred,
			DecisionAlpha
		);

	const float PressureAlpha =
		GetOffensiveProfilePressureAlpha(SoccerCharacter);

	const float PressurePenalty = FMath::Lerp(
		ComposurePressurePassScorePenaltyAtZero,
		ComposurePressurePassScorePenaltyAtHundred,
		ComposureAlpha
	);

	RequiredPassScore += PressureAlpha * PressurePenalty;

	return FMath::Max(0.0f, RequiredPassScore);
}

float ASoccerAIController::GetAttackingProfileMoveRefreshMultiplier(
	const ASoccerAICharacter* SoccerCharacter
) const
{
	if (!IsValid(SoccerCharacter) || !SoccerCharacter->HasPlayerProfile())
	{
		return 1.0f;
	}

	const float PositioningAlpha =
		SoccerCharacter->GetPlayerProfileOffBallPositioningAlpha();

	const float AnticipationAlpha =
		SoccerCharacter->GetPlayerProfileAnticipationAlpha();

	const float MovementReadingAlpha = FMath::Clamp(
		PositioningAlpha * 0.65f + AnticipationAlpha * 0.35f,
		0.0f,
		1.0f
	);

	return FMath::Max(
		0.25f,
		FMath::Lerp(
			AttackMoveRefreshMultiplierAtZero,
			AttackMoveRefreshMultiplierAtHundred,
			MovementReadingAlpha
		)
	);
}

FVector ASoccerAIController::ApplyAttackingProfileTargetExecution(
	ASoccerAICharacter* SoccerCharacter,
	const FVector& IdealTargetLocation,
	ESoccerAIOrder CurrentOrder
) const
{
	if (
		!IsValid(SoccerCharacter) ||
		!SoccerCharacter->HasPlayerProfile() ||
		IdealTargetLocation.IsNearlyZero() ||
		IdealTargetLocation.ContainsNaN()
	)
	{
		return IdealTargetLocation;
	}

	const float PositioningAlpha =
		SoccerCharacter->GetPlayerProfileOffBallPositioningAlpha();

	float MaximumErrorCm = FMath::Lerp(
		OffBallPositioningTargetErrorCmAtZero,
		OffBallPositioningTargetErrorCmAtHundred,
		PositioningAlpha
	);

	// Rest-defense/compensation are structurally important even during attack.
	// Keep profile expression visible there, but do not let a low rating break
	// the collective safety shape.
	if (
		CurrentOrder == ESoccerAIOrder::AttackRestDefense ||
		CurrentOrder == ESoccerAIOrder::AttackCompensateCover
	)
	{
		MaximumErrorCm *= 0.55f;
	}

	MaximumErrorCm = FMath::Max(0.0f, MaximumErrorCm);
	if (MaximumErrorCm <= KINDA_SMALL_NUMBER)
	{
		return IdealTargetLocation;
	}

	FVector AttackDirection = FVector::ForwardVector;
	if (IsValid(MatchManager))
	{
		AttackDirection =
			MatchManager->GetFieldAttackDirectionForTeam(
				SoccerCharacter->GetTeam()
			);
	}

	AttackDirection.Z = 0.0f;
	AttackDirection = AttackDirection.GetSafeNormal();
	if (AttackDirection.IsNearlyZero())
	{
		AttackDirection = FVector::ForwardVector;
	}

	const FVector RightDirection(
		-AttackDirection.Y,
		AttackDirection.X,
		0.0f
	);

	const uint32 ProfileHash =
		GetTypeHash(SoccerCharacter->GetPlayerProfileId());
	const uint32 OrderHash =
		static_cast<uint32>(CurrentOrder) * 2654435761u;
	const uint32 CombinedHash = ProfileHash ^ OrderHash;

	const float ErrorAngleRadians = FMath::DegreesToRadians(
		static_cast<float>(CombinedHash % 360u)
	);

	const float ErrorMagnitudeAlpha =
		0.55f + static_cast<float>((CombinedHash >> 9) % 46u) / 100.0f;

	const FVector StableErrorDirection =
		AttackDirection * FMath::Cos(ErrorAngleRadians) +
		RightDirection * FMath::Sin(ErrorAngleRadians);

	FVector AdjustedTargetLocation =
		IdealTargetLocation +
		StableErrorDirection * (MaximumErrorCm * ErrorMagnitudeAlpha);

	AdjustedTargetLocation.Z = IdealTargetLocation.Z;

	UWorld* World = GetWorld();
	if (World != nullptr)
	{
		UNavigationSystemV1* NavigationSystem =
			FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);

		if (NavigationSystem != nullptr)
		{
			FNavLocation ProjectedAttackLocation;
			const bool bProjectedAttackTarget =
				NavigationSystem->ProjectPointToNavigation(
					AdjustedTargetLocation,
					ProjectedAttackLocation,
					FVector(250.0f, 250.0f, 250.0f)
				);

			if (bProjectedAttackTarget)
			{
				AdjustedTargetLocation =
					ProjectedAttackLocation.Location;
				AdjustedTargetLocation.Z = IdealTargetLocation.Z;
			}
			else
			{
				return IdealTargetLocation;
			}
		}
	}

	return AdjustedTargetLocation;
}

float ASoccerAIController::GetDefensiveProfileReactionMultiplier(
	const ASoccerAICharacter* SoccerCharacter
) const
{
	if (!IsValid(SoccerCharacter) || !SoccerCharacter->HasPlayerProfile())
	{
		return 1.0f;
	}

	return FMath::Max(
		0.10f,
		FMath::Lerp(
			DefensiveReactionDelayMultiplierAtZero,
			DefensiveReactionDelayMultiplierAtHundred,
			SoccerCharacter->GetPlayerProfileDefensiveReactionAlpha()
		)
	);
}

float ASoccerAIController::GetDefensiveProfileAnticipationPredictionTrust(
	const ASoccerAICharacter* SoccerCharacter
) const
{
	if (!IsValid(SoccerCharacter) || !SoccerCharacter->HasPlayerProfile())
	{
		return 1.0f;
	}

	return FMath::Clamp(
		FMath::Lerp(
			AnticipationPredictionTrustAtZero,
			AnticipationPredictionTrustAtHundred,
			SoccerCharacter->GetPlayerProfileAnticipationAlpha()
		),
		0.0f,
		1.0f
	);
}

float ASoccerAIController::GetDefensiveProfileAnticipationHorizonMultiplier(
	const ASoccerAICharacter* SoccerCharacter
) const
{
	if (!IsValid(SoccerCharacter) || !SoccerCharacter->HasPlayerProfile())
	{
		return 1.0f;
	}

	return FMath::Max(
		0.25f,
		FMath::Lerp(
			AnticipationHorizonMultiplierAtZero,
			AnticipationHorizonMultiplierAtHundred,
			SoccerCharacter->GetPlayerProfileAnticipationAlpha()
		)
	);
}

float ASoccerAIController::GetDefensiveProfileMovementSkillAlpha(
	const ASoccerAICharacter* SoccerCharacter,
	ESoccerAIOrder CurrentOrder
) const
{
	if (!IsValid(SoccerCharacter) || !SoccerCharacter->HasPlayerProfile())
	{
		return 1.0f;
	}

	return CurrentOrder == ESoccerAIOrder::DefendMarkDangerousReceiver
		? SoccerCharacter->GetPlayerProfileMarkingAlpha()
		: SoccerCharacter->GetPlayerProfileDefensivePositioningAlpha();
}

float ASoccerAIController::GetDefensiveProfileMoveRefreshMultiplier(
	const ASoccerAICharacter* SoccerCharacter,
	ESoccerAIOrder CurrentOrder
) const
{
	if (!IsValid(SoccerCharacter) || !SoccerCharacter->HasPlayerProfile())
	{
		return 1.0f;
	}

	return FMath::Max(
		0.25f,
		FMath::Lerp(
			DefenseMoveRefreshMultiplierAtZero,
			DefenseMoveRefreshMultiplierAtHundred,
			GetDefensiveProfileMovementSkillAlpha(
				SoccerCharacter,
				CurrentOrder
			)
		)
	);
}

float ASoccerAIController::GetDefensiveProfileTackleCooldownMultiplier(
	const ASoccerAICharacter* SoccerCharacter
) const
{
	if (!IsValid(SoccerCharacter) || !SoccerCharacter->HasPlayerProfile())
	{
		return 1.0f;
	}

	return FMath::Max(
		0.25f,
		FMath::Lerp(
			TacklingDecisionCooldownMultiplierAtZero,
			TacklingDecisionCooldownMultiplierAtHundred,
			SoccerCharacter->GetPlayerProfileTacklingAlpha()
		)
	);
}

float ASoccerAIController::GetDefensiveProfileContestedTackleScoreAdjustment(
	const ASoccerAICharacter* SoccerCharacter
) const
{
	if (!IsValid(SoccerCharacter) || !SoccerCharacter->HasPlayerProfile())
	{
		return 0.0f;
	}

	return FMath::Lerp(
		ContestedTackleDecisionScoreAdjustmentAtZero,
		ContestedTackleDecisionScoreAdjustmentAtHundred,
		SoccerCharacter->GetPlayerProfileTacklingAlpha()
	);
}

FVector ASoccerAIController::ApplyDefensiveProfileTargetExecution(
	ASoccerAICharacter* SoccerCharacter,
	const FVector& IdealTargetLocation,
	ESoccerAIOrder CurrentOrder,
	bool bGoalAreaEmergency
)
{
	if (
		!IsValid(SoccerCharacter) ||
		!SoccerCharacter->HasPlayerProfile() ||
		IdealTargetLocation.IsNearlyZero() ||
		IdealTargetLocation.ContainsNaN()
	)
	{
		ClearDefensiveProfileTargetExecutionState();
		return IdealTargetLocation;
	}

	const bool bMarkingOrder =
		CurrentOrder == ESoccerAIOrder::DefendMarkDangerousReceiver;

	const float SkillAlpha = GetDefensiveProfileMovementSkillAlpha(
		SoccerCharacter,
		CurrentOrder
	);

	float MaximumErrorCm = bMarkingOrder
		? FMath::Lerp(
			MarkingTargetErrorCmAtZero,
			MarkingTargetErrorCmAtHundred,
			SkillAlpha
		)
		: FMath::Lerp(
			DefensivePositioningTargetErrorCmAtZero,
			DefensivePositioningTargetErrorCmAtHundred,
			SkillAlpha
		);

	if (bGoalAreaEmergency)
	{
		MaximumErrorCm *= FMath::Clamp(
			GoalAreaEmergencyDefensiveErrorScale,
			0.0f,
			1.0f
		);
	}

	MaximumErrorCm = FMath::Max(0.0f, MaximumErrorCm);

	if (MaximumErrorCm <= KINDA_SMALL_NUMBER)
	{
		ClearDefensiveProfileTargetExecutionState();
		return IdealTargetLocation;
	}

	UWorld* World = GetWorld();
	const float CurrentTime = World != nullptr
		? World->GetTimeSeconds()
		: 0.0f;

	const float IdealTargetMovement = bHasDefensiveProfileExecutionOffset
		? FVector::Dist2D(
			IdealTargetLocation,
			LastDefensiveProfileIdealTarget
		)
		: TNumericLimits<float>::Max();

	const float TargetMovementRefreshThreshold = FMath::Max(
		80.0f,
		MaximumErrorCm * 0.75f
	);

	const bool bNeedsNewExecutionOffset =
		!bHasDefensiveProfileExecutionOffset ||
		LastDefensiveProfileExecutionOrder != CurrentOrder ||
		CurrentTime - LastDefensiveProfileExecutionOffsetTime >=
			FMath::Max(0.20f, DefensiveExecutionOffsetRefreshInterval) ||
		IdealTargetMovement >= TargetMovementRefreshThreshold;

	if (bNeedsNewExecutionOffset)
	{
		const float ErrorAngle = FMath::FRandRange(-PI, PI);
		const float ErrorRadius =
			FMath::FRandRange(0.25f, 1.0f) * MaximumErrorCm;

		DefensiveProfileExecutionOffset = FVector(
			FMath::Cos(ErrorAngle) * ErrorRadius,
			FMath::Sin(ErrorAngle) * ErrorRadius,
			0.0f
		);

		LastDefensiveProfileExecutionOffsetTime = CurrentTime;
		LastDefensiveProfileExecutionOrder = CurrentOrder;
		bHasDefensiveProfileExecutionOffset = true;
	}

	LastDefensiveProfileIdealTarget = IdealTargetLocation;

	FVector AdjustedTargetLocation =
		IdealTargetLocation + DefensiveProfileExecutionOffset;
	AdjustedTargetLocation.Z = IdealTargetLocation.Z;

	FVector ProjectedTargetLocation;
	if (
		ProjectDefensivePressureLocationToNavigation(
			AdjustedTargetLocation,
			ProjectedTargetLocation
		)
	)
	{
		return ProjectedTargetLocation;
	}

	return IdealTargetLocation;
}

void ASoccerAIController::ClearDefensiveProfileTargetExecutionState()
{
	bHasDefensiveProfileExecutionOffset = false;
	DefensiveProfileExecutionOffset = FVector::ZeroVector;
	LastDefensiveProfileIdealTarget = FVector::ZeroVector;
	LastDefensiveProfileExecutionOffsetTime = -1000.0f;
	LastDefensiveProfileExecutionOrder = ESoccerAIOrder::ReturnHome;
}

FVector ASoccerAIController::GetDelayedObservedBallLocation(
	ASoccerBall* SoccerBall
)
{
	if (!IsValid(SoccerBall))
	{
		return FVector::ZeroVector;
	}

	const FVector CurrentBallLocation =
		SoccerBall->GetActorLocation();

	if (!bUseAIReactionDelay)
	{
		return CurrentBallLocation;
	}

	const float CurrentTime =
		GetWorld() != nullptr
		? GetWorld()->GetTimeSeconds()
		: 0.0f;

	if (!bHasDelayedBallObservation)
	{
		bHasDelayedBallObservation = true;
		DelayedObservedBallLocation = CurrentBallLocation;
		LastDelayedBallObservationTime = CurrentTime;

		CurrentAIReactionDelay =
			BuildAIReactionDelayForCharacter(
				Cast<ASoccerAICharacter>(GetPawn())
			);

		return DelayedObservedBallLocation;
	}

	const float DistanceFromObservedToReal =
		FVector::Dist2D(
			DelayedObservedBallLocation,
			CurrentBallLocation
		);

	if (DistanceFromObservedToReal > AIReactionDelayTeleportDistance)
	{
		DelayedObservedBallLocation = CurrentBallLocation;
		LastDelayedBallObservationTime = CurrentTime;

		CurrentAIReactionDelay =
			BuildAIReactionDelayForCharacter(
				Cast<ASoccerAICharacter>(GetPawn())
			);

		return DelayedObservedBallLocation;
	}

	if (
		CurrentTime - LastDelayedBallObservationTime >=
		CurrentAIReactionDelay
		)
	{
		DelayedObservedBallLocation = CurrentBallLocation;
		LastDelayedBallObservationTime = CurrentTime;

		CurrentAIReactionDelay =
			BuildAIReactionDelayForCharacter(
				Cast<ASoccerAICharacter>(GetPawn())
			);
	}

	return DelayedObservedBallLocation;
}

void ASoccerAIController::ClearDelayedBallObservation()
{
	bHasDelayedBallObservation = false;
	DelayedObservedBallLocation = FVector::ZeroVector;
	LastDelayedBallObservationTime = -1000.0f;
	CurrentAIReactionDelay = 0.0f;
}

void ASoccerAIController::CreateOrUpdateRecoveryIntent(
	ASoccerAICharacter* SoccerCharacter,
	ASoccerBall* SoccerBall
)
{
	if (!bUseAIRecoveryIntent)
	{
		return;
	}

	if (!IsValid(SoccerCharacter) || !IsValid(SoccerBall))
	{
		ClearCurrentRecoveryIntent();
		return;
	}

	if (!IsValid(MatchManager))
	{
		ClearCurrentRecoveryIntent();
		return;
	}

	if (
		CurrentRecoveryIntent != ESoccerAIIntent::None &&
		!IsCurrentRecoveryIntentExpired()
		)
	{
		return;
	}

	ClearCurrentRecoveryIntent();

	const float CurrentTime =
		GetWorld() != nullptr
		? GetWorld()->GetTimeSeconds()
		: 0.0f;

	const ESoccerFieldZone CharacterZone =
		MatchManager->GetCharacterFieldZone(SoccerCharacter);

	const bool bCanThinkAboutShot =
		CharacterZone == ESoccerFieldZone::ZoneE ||
		CharacterZone == ESoccerFieldZone::ZoneF;

	if (bCanThinkAboutShot)
	{
		const FVector ShotCenterLocation =
			MatchManager->GetShotTargetLocation(SoccerCharacter);

		const float DistanceToShotTarget =
			FVector::Dist2D(
				SoccerCharacter->GetActorLocation(),
				ShotCenterLocation
			);

		if (DistanceToShotTarget <= AIShotDistanceToTarget)
		{
			const FVector ShotTargetLocation =
				BuildAIShotTargetLocation(SoccerCharacter);

			if (!ShotTargetLocation.IsNearlyZero())
			{
				CurrentRecoveryIntent =
					ESoccerAIIntent::RecoverAndShoot;

				RecoveryIntentPlannedAction =
					ESoccerAIPendingMainAction::Shoot;

				RecoveryIntentTargetLocation =
					ShotTargetLocation;

				RecoveryIntentCreatedTime =
					CurrentTime;

				return;
			}
		}
	}

	if (bEnableAIRecoveryClearance)
	{
		const FVector OwnGoalLocation =
			MatchManager->GetOwnGoalCenterLocation(SoccerCharacter->GetTeam());
		const float DistanceToOwnGoal = FVector::Dist2D(
			SoccerBall->GetActorLocation(),
			OwnGoalLocation
		);

		if (DistanceToOwnGoal <= AIRecoveryClearanceOwnGoalDistance)
		{
			const FVector ClearanceTarget =
				BuildRecoveryClearanceTargetLocation(SoccerCharacter, SoccerBall);
			if (!ClearanceTarget.IsNearlyZero())
			{
				CurrentRecoveryIntent = ESoccerAIIntent::RecoverAndClear;
				RecoveryIntentPlannedAction = ESoccerAIPendingMainAction::Clearance;
				RecoveryIntentTargetLocation = ClearanceTarget;
				RecoveryIntentCreatedTime = CurrentTime;
				return;
			}
		}
	}

	ASoccerCharacterBase* SmartPassReceiver = nullptr;
	FVector SmartPassTarget = FVector::ZeroVector;
	ESoccerAttackPassType SmartPassType = ESoccerAttackPassType::ToFeet;
	float SmartPassScore = 0.0f;
	float SmartPassHorizontalSpeed = 0.0f;

	if (TryBuildSmartAttackPassPlan(
		SoccerCharacter,
		false,
		false,
		SmartPassReceiver,
		SmartPassTarget,
		SmartPassType,
		SmartPassScore,
		SmartPassHorizontalSpeed
	))
	{
		CurrentRecoveryIntent = ESoccerAIIntent::RecoverAndPass;
		RecoveryIntentPlannedAction = ESoccerAIPendingMainAction::PassToTeammate;
		RecoveryIntentTargetCharacter = SmartPassReceiver;
		RecoveryIntentTargetLocation = SmartPassTarget;
		RecoveryIntentPassType = SmartPassType;
		RecoveryIntentPassScore = SmartPassScore;
		RecoveryIntentPassHorizontalSpeed = SmartPassHorizontalSpeed;
		RecoveryIntentCreatedTime = CurrentTime;
		return;
	}

	const FVector AutoPassTargetLocation =
		BuildAIAutoPassTargetLocation(SoccerCharacter);

	if (!AutoPassTargetLocation.IsNearlyZero())
	{
		CurrentRecoveryIntent =
			ESoccerAIIntent::RecoverAndCarry;

		RecoveryIntentPlannedAction =
			ESoccerAIPendingMainAction::AutoPass;

		RecoveryIntentTargetLocation =
			AutoPassTargetLocation;

		RecoveryIntentCreatedTime =
			CurrentTime;
	}
}

bool ASoccerAIController::TryExecuteCurrentRecoveryIntent(
	ASoccerAICharacter* SoccerCharacter
)
{
	if (!bUseAIRecoveryIntent)
	{
		return false;
	}

	if (!IsValid(SoccerCharacter))
	{
		ClearCurrentRecoveryIntent();
		return false;
	}

	if (!SoccerCharacter->IsAIPossessingBall())
	{
		return false;
	}

	if (CurrentRecoveryIntent == ESoccerAIIntent::None)
	{
		return false;
	}

	if (
		RecoveryIntentPlannedAction ==
		ESoccerAIPendingMainAction::None
		)
	{
		ClearCurrentRecoveryIntent();
		return false;
	}

	if (IsCurrentRecoveryIntentExpired())
	{
		ClearCurrentRecoveryIntent();
		return false;
	}

	FVector TargetLocation =
		RecoveryIntentTargetLocation;

	ASoccerCharacterBase* TargetCharacter =
		RecoveryIntentTargetCharacter.Get();

	if (
		RecoveryIntentPlannedAction ==
		ESoccerAIPendingMainAction::PassToTeammate
		)
	{
		if (
			!IsValid(TargetCharacter) ||
			TargetCharacter->GetTeam() != SoccerCharacter->GetTeam()
			)
		{
			ClearCurrentRecoveryIntent();
			return false;
		}
	}

	if (TargetLocation.IsNearlyZero())
	{
		ClearCurrentRecoveryIntent();
		return false;
	}

	const ESoccerAIPendingMainAction ActionToExecute =
		RecoveryIntentPlannedAction;

	ASoccerCharacterBase* CharacterToUse =
		TargetCharacter;
	const float RecoveryPassSpeedToUse =
		RecoveryIntentPassHorizontalSpeed > 0.0f
		? RecoveryIntentPassHorizontalSpeed
		: SmartAttackPassToFeetHorizontalSpeed;

	ClearCurrentRecoveryIntent();

	switch (ActionToExecute)
	{
	case ESoccerAIPendingMainAction::Clearance:
		if (!TryRegisterAIKickTouchForRules(SoccerCharacter))
		{
			StopMovement();
			return true;
		}

		SoccerCharacter->KickAIBallToTarget(
			TargetLocation,
			AIRecoveryClearanceHorizontalSpeed,
			AIRecoveryClearanceMinTravelTime,
			AIRecoveryClearanceMaxTravelTime
		);
		if (SoccerCharacter->IsAIKickMontageActive())
		{
			StopMovement();
		}
		return true;

	case ESoccerAIPendingMainAction::Shoot:
		if (
			TryPrepareMainActionIfBlocked(
				SoccerCharacter,
				ESoccerAIPendingMainAction::Shoot,
				TargetLocation,
				nullptr
			)
			)
		{
			return true;
		}

		TargetLocation = ApplyAIShotExecutionError(
			SoccerCharacter,
			TargetLocation
		);

		if (!TryRegisterAIKickTouchForRules(SoccerCharacter))
		{
			StopMovement();
			return true;
		}

		SoccerCharacter->KickAIBallToAirTarget(
			TargetLocation,
			AIShotHorizontalSpeed,
			AIShotMinTravelTime,
			AIShotMaxTravelTime
		);

		if (SoccerCharacter->IsAIKickMontageActive())
		{
			StopMovement();
		}

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				1.2f,
				FColor::Red,
				TEXT("AI intencion: recupero y remato")
			);
		}

		return true;

	case ESoccerAIPendingMainAction::PassToTeammate:
		if (
			TryPrepareMainActionIfBlocked(
				SoccerCharacter,
				ESoccerAIPendingMainAction::PassToTeammate,
				TargetLocation,
				CharacterToUse
			)
			)
		{
			return true;
		}

		if (!TryRegisterAIKickTouchForRules(SoccerCharacter))
		{
			StopMovement();
			return true;
		}

		SoccerCharacter->KickAIBallToTarget(
			TargetLocation,
			RecoveryPassSpeedToUse,
			SmartAttackPassMinTravelTime,
			SmartAttackPassMaxTravelTime
		);

		if (IsValid(CharacterToUse))
		{
			MatchManager->RegisterOpenPlayPassIntent(
				SoccerCharacter,
				CharacterToUse,
				TargetLocation
			);
		}

		if (SoccerCharacter->IsAIKickMontageActive())
		{
			StopMovement();
		}

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				1.2f,
				FColor::Magenta,
				TEXT("AI intencion: recupero y pasa")
			);
		}

		return true;

	case ESoccerAIPendingMainAction::AutoPass:
	{
		bool bUsedLocalDribbleDirection = false;

		const FVector FreshAutoPassTarget =
			BuildAIAutoPassTargetLocation(
				SoccerCharacter,
				&bUsedLocalDribbleDirection
			);

		if (!FreshAutoPassTarget.IsNearlyZero())
		{
			TargetLocation = FreshAutoPassTarget;
		}
		else if (bUseAILocalDribbleDirection)
		{
			// Local space changed while recovering the ball. Do not execute a
			// stale carry intent into a now-unsafe sector; let normal possession
			// decision-making choose pass/protection instead.
			return false;
		}

		if (
			!bUsedLocalDribbleDirection &&
			TryPrepareMainActionIfBlocked(
				SoccerCharacter,
				ESoccerAIPendingMainAction::AutoPass,
				TargetLocation,
				nullptr
			)
			)
		{
			return true;
		}

		if (!TryRegisterAIKickTouchForRules(SoccerCharacter))
		{
			StopMovement();
			return true;
		}

		SoccerCharacter->StartAIAutoPassToLocation(
			TargetLocation,
			AIAutoPassHorizontalSpeed,
			AIAutoPassMinTravelTime,
			AIAutoPassMaxTravelTime
		);

		MoveToLocationWithAIMovement(
			ESoccerAIOrder::AttackRunIntoSpace,
			TargetLocation,
			AIAutoPassFollowAcceptanceRadius,
			false
		);

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				1.2f,
				FColor::Cyan,
				TEXT("AI intencion: recupero y autopase")
			);
		}

		return true;
	}

	case ESoccerAIPendingMainAction::None:
	default:
		return false;
	}
}

FVector ASoccerAIController::BuildRecoveryClearanceTargetLocation(
	const ASoccerAICharacter* SoccerCharacter,
	const ASoccerBall* SoccerBall
) const
{
	if (!IsValid(SoccerCharacter) || !IsValid(SoccerBall) || !IsValid(MatchManager))
	{
		return FVector::ZeroVector;
	}

	FVector AttackDirection = MatchManager->GetFieldAttackDirectionForTeam(
		SoccerCharacter->GetTeam()
	);
	AttackDirection.Z = 0.0f;
	AttackDirection = AttackDirection.GetSafeNormal();
	if (AttackDirection.IsNearlyZero())
	{
		return FVector::ZeroVector;
	}

	FVector SideDirection = FVector::CrossProduct(FVector::UpVector, AttackDirection);
	SideDirection.Z = 0.0f;
	SideDirection = SideDirection.GetSafeNormal();

	const FVector BallLocation = SoccerBall->GetActorLocation();
	float SideSign = 1.0f;
	const ASoccerField* SoccerField = MatchManager->GetSoccerField();
	if (IsValid(SoccerField))
	{
		const FVector LocalBallLocation = SoccerField->WorldToPitchLocal(BallLocation);
		SideSign = LocalBallLocation.Y >= 0.0f ? 1.0f : -1.0f;
	}

	FVector TargetLocation =
		BallLocation +
		AttackDirection * AIRecoveryClearanceForwardDistance +
		SideDirection * SideSign * AIRecoveryClearanceLateralDistance;
	TargetLocation.Z = BallLocation.Z;

	if (IsValid(SoccerField))
	{
		return SoccerField->ClampWorldLocationInsidePitch(TargetLocation, 140.0f);
	}

	return SoccerFieldDimensions::ClampLocationInsidePitch(TargetLocation, 140.0f);
}

bool ASoccerAIController::TryExecuteCurrentRecoveryIntentAtDefensiveContact(
	ASoccerAICharacter* SoccerCharacter,
	ASoccerCharacterBase* PreviousPossessor,
	ASoccerBall* SoccerBall
)
{
	if (!IsValid(SoccerCharacter) || !IsValid(PreviousPossessor) ||
		!IsValid(SoccerBall) || !IsValid(MatchManager))
	{
		return false;
	}

	if (CurrentRecoveryIntent == ESoccerAIIntent::None || IsCurrentRecoveryIntentExpired())
	{
		CreateOrUpdateRecoveryIntent(SoccerCharacter, SoccerBall);
	}

	ESoccerAIPendingMainAction ActionToExecute = RecoveryIntentPlannedAction;
	FVector TargetLocation = RecoveryIntentTargetLocation;
	ASoccerCharacterBase* TargetCharacter = RecoveryIntentTargetCharacter.Get();
	const ESoccerAttackPassType RecoveryPassTypeToUse = RecoveryIntentPassType;
	const float RecoveryPassScoreToUse = RecoveryIntentPassScore;
	const float RecoveryPassSpeedToUse =
		RecoveryIntentPassHorizontalSpeed > 0.0f
		? RecoveryIntentPassHorizontalSpeed
		: SmartAttackPassToFeetHorizontalSpeed;
	if (ActionToExecute == ESoccerAIPendingMainAction::None)
	{
		// RecoveryIntent can be disabled for experiments. A successful defensive
		// contact must still become a physical poke instead of restoring the old
		// instantaneous possession transfer.
		ActionToExecute = ESoccerAIPendingMainAction::AutoPass;
	}

	if (ActionToExecute == ESoccerAIPendingMainAction::PassToTeammate)
	{
		if (!IsValid(TargetCharacter) || TargetCharacter->GetTeam() != SoccerCharacter->GetTeam())
		{
			ActionToExecute = ESoccerAIPendingMainAction::AutoPass;
		}
		else
		{
			// Keep the intelligent destination selected before contact. Rebuilding
			// it as a simple pass here would erase ForwardSpace/RetentionSpace.
		}
	}

	if (ActionToExecute == ESoccerAIPendingMainAction::AutoPass)
	{
		TargetLocation = BuildAIAutoPassTargetLocation(SoccerCharacter);
		if (TargetLocation.IsNearlyZero())
		{
			FVector EscapeDirection = SoccerCharacter->GetActorLocation() - PreviousPossessor->GetActorLocation();
			EscapeDirection.Z = 0.0f;
			EscapeDirection = EscapeDirection.GetSafeNormal();
			if (EscapeDirection.IsNearlyZero())
			{
				EscapeDirection = MatchManager->GetFieldAttackDirectionForTeam(SoccerCharacter->GetTeam());
			}
			TargetLocation = SoccerBall->GetActorLocation() + EscapeDirection * 220.0f;
		}
	}
	else if (ActionToExecute == ESoccerAIPendingMainAction::Clearance)
	{
		TargetLocation = BuildRecoveryClearanceTargetLocation(SoccerCharacter, SoccerBall);
	}
	else if (ActionToExecute == ESoccerAIPendingMainAction::Shoot)
	{
		TargetLocation = ApplyAIShotExecutionError(SoccerCharacter, TargetLocation);
	}

	if (ActionToExecute == ESoccerAIPendingMainAction::None || TargetLocation.IsNearlyZero())
	{
		return false;
	}

	if (!MatchManager->BeginIntentionalLooseBallTouch(SoccerCharacter))
	{
		return false;
	}

	if (ASoccerAICharacter* PreviousAI = Cast<ASoccerAICharacter>(PreviousPossessor))
	{
		PreviousAI->ReleaseAIBall(false);
	}
	else if (AThirdPersonCppCharacter* PreviousHuman = Cast<AThirdPersonCppCharacter>(PreviousPossessor))
	{
		PreviousHuman->ReleaseBallForAISteal();
	}

	ClearCurrentRecoveryIntent();

	const TCHAR* ContactType = TEXT("POKE");
	switch (ActionToExecute)
	{
	case ESoccerAIPendingMainAction::Shoot:
		ContactType = TEXT("SHOT");
		SoccerCharacter->ExecuteImmediateAIContactKick(
			SoccerBall, TargetLocation, AIShotHorizontalSpeed,
			AIShotMinTravelTime, AIShotMaxTravelTime, true, true, false
		);
		SoccerCharacter->SetAIChasingBall(false);
		StopMovement();
		break;

	case ESoccerAIPendingMainAction::PassToTeammate:
		ContactType = TEXT("PASS");
		SoccerCharacter->ExecuteImmediateAIContactKick(
			SoccerBall, TargetLocation, RecoveryPassSpeedToUse,
			SmartAttackPassMinTravelTime, SmartAttackPassMaxTravelTime,
			false, false, false
		);
		if (IsValid(TargetCharacter))
		{
			MatchManager->RegisterOpenPlayPassIntent(SoccerCharacter, TargetCharacter, TargetLocation);
		}
		SoccerCharacter->SetAIChasingBall(false);
		StopMovement();
		break;

	case ESoccerAIPendingMainAction::Clearance:
		ContactType = TEXT("CLEARANCE");
		SoccerCharacter->ExecuteImmediateAIContactKick(
			SoccerBall, TargetLocation, AIRecoveryClearanceHorizontalSpeed,
			AIRecoveryClearanceMinTravelTime, AIRecoveryClearanceMaxTravelTime,
			false, false, false
		);
		SoccerCharacter->SetAIChasingBall(false);
		StopMovement();
		break;

	case ESoccerAIPendingMainAction::AutoPass:
		SoccerCharacter->ExecuteImmediateAIContactKick(
			SoccerBall, TargetLocation, AIAutoPassHorizontalSpeed,
			AIAutoPassMinTravelTime, AIAutoPassMaxTravelTime, false, false, true
		);
		SoccerCharacter->SetAIChasingBall(true);
		break;

	case ESoccerAIPendingMainAction::None:
	default:
		return false;
	}

	UE_LOG(
		LogTemp,
		Display,
		TEXT("[DefensiveTouch] type=%s defender=%s previous=%s target=%s"),
		ContactType,
		*GetNameSafe(SoccerCharacter),
		*GetNameSafe(PreviousPossessor),
		*TargetLocation.ToCompactString()
	);

	if (ActionToExecute == ESoccerAIPendingMainAction::PassToTeammate)
	{
		const TCHAR* PassTypeText =
			RecoveryPassTypeToUse == ESoccerAttackPassType::ForwardSpace
			? TEXT("FORWARD_SPACE")
			: RecoveryPassTypeToUse == ESoccerAttackPassType::RetentionSpace
			? TEXT("RETENTION_SPACE")
			: TEXT("TO_FEET");
		UE_LOG(
			LogTemp,
			Display,
			TEXT("[DefensiveTouchPass] type=%s receiver=%s score=%.1f speed=%.0f"),
			PassTypeText,
			*GetNameSafe(TargetCharacter),
			RecoveryPassScoreToUse,
			RecoveryPassSpeedToUse
		);
	}
	return true;
}

void ASoccerAIController::ClearCurrentRecoveryIntent()
{
	CurrentRecoveryIntent =
		ESoccerAIIntent::None;

	RecoveryIntentPlannedAction =
		ESoccerAIPendingMainAction::None;

	RecoveryIntentTargetCharacter =
		nullptr;

	RecoveryIntentTargetLocation =
		FVector::ZeroVector;

	RecoveryIntentPassType =
		ESoccerAttackPassType::ToFeet;

	RecoveryIntentPassScore = 0.0f;
	RecoveryIntentPassHorizontalSpeed = 0.0f;

	RecoveryIntentCreatedTime =
		-1000.0f;
}

bool ASoccerAIController::IsCurrentRecoveryIntentExpired() const
{
	if (CurrentRecoveryIntent == ESoccerAIIntent::None)
	{
		return true;
	}

	const float CurrentTime =
		GetWorld() != nullptr
		? GetWorld()->GetTimeSeconds()
		: 0.0f;

	return
		CurrentTime - RecoveryIntentCreatedTime >
		AIRecoveryIntentLifetime;
}

bool ASoccerAIController::IsAIAutoPassStillValid(
	const ASoccerAICharacter* SoccerCharacter,
	const ASoccerBall* SoccerBall
) const
{
	if (!IsValid(SoccerCharacter) || !IsValid(SoccerBall))
	{
		return false;
	}

	if (!SoccerCharacter->IsAIAutoPassActive())
	{
		return false;
	}

	// El autopase pertenece a su autor solo mientras ningun otro jugador
	// haya registrado un toque posterior. Esto corta la intencion en el
	// mismo momento en que un rival o companero interviene.
	if (IsValid(MatchManager))
	{
		ASoccerCharacterBase* LastTouchCharacter =
			MatchManager->GetLastTouchCharacter();

		if (
			IsValid(LastTouchCharacter) &&
			LastTouchCharacter != SoccerCharacter
			)
		{
			return false;
		}
	}

	if (
		SoccerCharacter->GetTimeSinceAIAutoPassStarted() >
		AIAutoPassMaxFollowTime
		)
	{
		return false;
	}

	const FVector StartLocation =
		SoccerCharacter->GetAIAutoPassStartBallLocation();

	const FVector TargetLocation =
		SoccerCharacter->GetAIAutoPassTargetLocation();

	if (
		StartLocation.IsNearlyZero() ||
		TargetLocation.IsNearlyZero()
		)
	{
		return true;
	}

	FVector Start2D = StartLocation;
	FVector Target2D = TargetLocation;
	FVector Ball2D = SoccerBall->GetActorLocation();
	FVector Character2D = SoccerCharacter->GetActorLocation();

	Start2D.Z = 0.0f;
	Target2D.Z = 0.0f;
	Ball2D.Z = 0.0f;
	Character2D.Z = 0.0f;

	const FVector Segment = Target2D - Start2D;
	const float SegmentLength = Segment.Size();

	if (SegmentLength < 50.0f)
	{
		return true;
	}

	const FVector Direction = Segment / SegmentLength;

	const float BallProjection =
		FVector::DotProduct(
			Ball2D - Start2D,
			Direction
		);

	if (BallProjection < -80.0f)
	{
		return false;
	}

	if (BallProjection > SegmentLength + 350.0f)
	{
		return false;
	}

	const FVector ClosestPointOnPath =
		Start2D + Direction * BallProjection;

	const float BallDistanceFromPath =
		FVector::Dist2D(
			Ball2D,
			ClosestPointOnPath
		);

	if (BallDistanceFromPath > AIAutoPassPathTolerance)
	{
		return false;
	}

	const float CharacterProjection =
		FVector::DotProduct(
			Character2D - Start2D,
			Direction
		);

	if (
		BallProjection <
		CharacterProjection - AIAutoPassCancelIfBallBehindDistance
		)
	{
		return false;
	}

	return true;
}

FVector ASoccerAIController::BuildAIAutoPassFollowMoveLocation(
	const ASoccerAICharacter* SoccerCharacter,
	const ASoccerBall* SoccerBall
) const
{
	if (!IsValid(SoccerCharacter) || !IsValid(SoccerBall))
	{
		return FVector::ZeroVector;
	}

	if (!bUseAIAutoPassIntentFollow)
	{
		return SoccerBall->GetActorLocation();
	}

	const FVector TargetLocation =
		SoccerCharacter->GetAIAutoPassTargetLocation();

	const FVector StartLocation =
		SoccerCharacter->GetAIAutoPassStartBallLocation();

	if (
		TargetLocation.IsNearlyZero() ||
		StartLocation.IsNearlyZero()
		)
	{
		return SoccerBall->GetActorLocation();
	}

	FVector Start2D = StartLocation;
	FVector Target2D = TargetLocation;

	Start2D.Z = 0.0f;
	Target2D.Z = 0.0f;

	FVector DirectionToTarget =
		Target2D - Start2D;

	DirectionToTarget.Z = 0.0f;
	DirectionToTarget = DirectionToTarget.GetSafeNormal();

	if (DirectionToTarget.IsNearlyZero())
	{
		return SoccerBall->GetActorLocation();
	}

	FVector MoveLocation =
		TargetLocation + DirectionToTarget * AIAutoPassFollowTargetLeadDistance;

	MoveLocation.Z = SoccerCharacter->GetActorLocation().Z;

	UWorld* World = GetWorld();

	if (World != nullptr)
	{
		UNavigationSystemV1* NavigationSystem =
			FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);

		if (NavigationSystem != nullptr)
		{
			FNavLocation ProjectedLocation;

			const bool bProjected =
				NavigationSystem->ProjectPointToNavigation(
					MoveLocation,
					ProjectedLocation,
					FVector(350.0f, 350.0f, 350.0f)
				);

			if (bProjected)
			{
				MoveLocation = ProjectedLocation.Location;
			}
		}
	}

	return MoveLocation;
}

void ASoccerAIController::ClearFilteredMoveRequest()
{
	bHasLastFilteredMoveTarget = false;
	LastFilteredMoveTarget = FVector::ZeroVector;
	LastFilteredMoveRequestTime = -1000.0f;
	LastFilteredMoveOrder = ESoccerAIOrder::ReturnHome;
	CurrentAttackMoveForcedRefreshInterval = 0.0f;
}

bool ASoccerAIController::MoveToLocationFilteredForAttack(
	const FVector& TargetLocation,
	float AcceptanceRadius,
	ESoccerAIOrder CurrentOrder
)
{
	if (TargetLocation.ContainsNaN())
	{
		return false;
	}

	UWorld* World = GetWorld();

	const float CurrentTime =
		World != nullptr
		? World->GetTimeSeconds()
		: 0.0f;

	const ASoccerAICharacter* AttackingProfileCharacter =
		Cast<ASoccerAICharacter>(GetPawn());

	const float AttackProfileRefreshMultiplier =
		GetAttackingProfileMoveRefreshMultiplier(
			AttackingProfileCharacter
		);

	if (CurrentAttackMoveForcedRefreshInterval <= 0.0f)
	{
		CurrentAttackMoveForcedRefreshInterval =
			BuildRandomAttackMoveRefreshInterval();
	}

	const float EffectiveAttackRepathDistanceThreshold =
		AttackMoveRepathDistanceThreshold *
		AttackProfileRefreshMultiplier;

	const float EffectiveAttackRepathMinInterval =
		AttackMoveRepathMinInterval *
		AttackProfileRefreshMultiplier;

	const float EffectiveAttackForcedRefreshInterval =
		CurrentAttackMoveForcedRefreshInterval *
		AttackProfileRefreshMultiplier;

	const float TimeSinceLastRequest =
		CurrentTime - LastFilteredMoveRequestTime;

	const bool bOrderChanged =
		!bHasLastFilteredMoveTarget ||
		LastFilteredMoveOrder != CurrentOrder;

	const float DistanceFromLastTarget =
		bHasLastFilteredMoveTarget
		? FVector::Dist2D(TargetLocation, LastFilteredMoveTarget)
		: TNumericLimits<float>::Max();

	const bool bTargetMovedEnough =
		DistanceFromLastTarget >=
		EffectiveAttackRepathDistanceThreshold;

	const bool bMinIntervalPassed =
		TimeSinceLastRequest >=
		EffectiveAttackRepathMinInterval;

	const bool bForcedRefreshNeeded =
		TimeSinceLastRequest >=
		EffectiveAttackForcedRefreshInterval;

	const EPathFollowingStatus::Type MoveStatus =
		GetMoveStatus();

	const bool bNotCurrentlyMoving =
		MoveStatus == EPathFollowingStatus::Idle ||
		MoveStatus == EPathFollowingStatus::Paused;

	const bool bShouldRequestMove =
		bOrderChanged ||
		bNotCurrentlyMoving ||
		bForcedRefreshNeeded ||
		(
			bTargetMovedEnough &&
			bMinIntervalPassed
			);

	if (!bShouldRequestMove)
	{
		return false;
	}

	MoveToLocationWithAIMovement(
		CurrentOrder,
		TargetLocation,
		AcceptanceRadius,
		false
	);

	LastFilteredMoveTarget = TargetLocation;
	LastFilteredMoveRequestTime = CurrentTime;
	LastFilteredMoveOrder = CurrentOrder;
	bHasLastFilteredMoveTarget = true;

	CurrentAttackMoveForcedRefreshInterval =
		BuildRandomAttackMoveRefreshInterval();

	return true;
}

float ASoccerAIController::BuildRandomAttackMoveRefreshInterval() const
{
	const float MinInterval =
		FMath::Max(
			AttackMoveRepathMinInterval,
			AttackMoveForcedRefreshInterval - AttackMoveForcedRefreshJitter
		);

	const float MaxInterval =
		FMath::Max(
			MinInterval,
			AttackMoveForcedRefreshInterval + AttackMoveForcedRefreshJitter
		);

	return FMath::FRandRange(
		MinInterval,
		MaxInterval
	);
}

float ASoccerAIController::BuildRandomDefenseMoveRefreshInterval() const
{
	return FMath::FRandRange(
		DefenseMoveForcedRefreshIntervalMin,
		DefenseMoveForcedRefreshIntervalMax
	);
}

void ASoccerAIController::ClearFilteredDefenseMoveRequest()
{
	bHasLastFilteredDefenseMoveTarget = false;
	LastFilteredDefenseMoveTarget = FVector::ZeroVector;
	LastFilteredDefenseMoveRequestTime = -1000.0f;
	LastFilteredDefenseMoveOrder = ESoccerAIOrder::ReturnHome;
	CurrentDefenseMoveForcedRefreshInterval = 0.0f;
}

bool ASoccerAIController::MoveToLocationFilteredForDefense(
	const FVector& TargetLocation,
	float AcceptanceRadius,
	ESoccerAIOrder CurrentOrder,
	bool bGoalAreaEmergency
)
{
	if (TargetLocation.ContainsNaN())
	{
		return false;
	}

	UWorld* World = GetWorld();

	const float CurrentTime =
		World != nullptr
		? World->GetTimeSeconds()
		: 0.0f;

	const ASoccerAICharacter* DefensiveCharacter =
		Cast<ASoccerAICharacter>(GetPawn());

	const float ProfileRefreshMultiplier =
		bGoalAreaEmergency
		? 1.0f
		: GetDefensiveProfileMoveRefreshMultiplier(
			DefensiveCharacter,
			CurrentOrder
		);

	if (CurrentDefenseMoveForcedRefreshInterval <= 0.0f)
	{
		CurrentDefenseMoveForcedRefreshInterval =
			bGoalAreaEmergency
			? FMath::Max(
				0.01f,
				GoalAreaDefenseMoveForcedRefreshInterval
			)
			: BuildRandomDefenseMoveRefreshInterval();
	}

	const float EffectiveRepathDistanceThreshold =
		bGoalAreaEmergency
		? FMath::Max(
			1.0f,
			GoalAreaDefenseMoveRepathDistanceThreshold
		)
		: DefenseMoveRepathDistanceThreshold * ProfileRefreshMultiplier;

	const float EffectiveRepathMinInterval =
		bGoalAreaEmergency
		? FMath::Max(
			0.0f,
			GoalAreaDefenseMoveRepathMinInterval
		)
		: DefenseMoveRepathMinInterval * ProfileRefreshMultiplier;

	const float EffectiveForcedRefreshInterval =
		bGoalAreaEmergency
		? FMath::Max(
			0.01f,
			GoalAreaDefenseMoveForcedRefreshInterval
		)
		: CurrentDefenseMoveForcedRefreshInterval * ProfileRefreshMultiplier;

	const float TimeSinceLastRequest =
		CurrentTime - LastFilteredDefenseMoveRequestTime;

	const bool bOrderChanged =
		!bHasLastFilteredDefenseMoveTarget ||
		LastFilteredDefenseMoveOrder != CurrentOrder;

	const float DistanceFromLastTarget =
		bHasLastFilteredDefenseMoveTarget
		? FVector::Dist2D(TargetLocation, LastFilteredDefenseMoveTarget)
		: TNumericLimits<float>::Max();

	const bool bTargetMovedEnough =
		DistanceFromLastTarget >=
		EffectiveRepathDistanceThreshold;

	const bool bMinIntervalPassed =
		TimeSinceLastRequest >=
		EffectiveRepathMinInterval;

	const bool bForcedRefreshNeeded =
		TimeSinceLastRequest >=
		EffectiveForcedRefreshInterval;

	const EPathFollowingStatus::Type MoveStatus =
		GetMoveStatus();

	const bool bNotCurrentlyMoving =
		MoveStatus == EPathFollowingStatus::Idle ||
		MoveStatus == EPathFollowingStatus::Paused;

	const bool bShouldRequestMove =
		bOrderChanged ||
		bNotCurrentlyMoving ||
		bForcedRefreshNeeded ||
		(bTargetMovedEnough && bMinIntervalPassed);

	if (!bShouldRequestMove)
	{
		return false;
	}

	MoveToLocationWithAIMovement(
		CurrentOrder,
		TargetLocation,
		AcceptanceRadius,
		false
	);

	LastFilteredDefenseMoveTarget = TargetLocation;
	LastFilteredDefenseMoveRequestTime = CurrentTime;
	LastFilteredDefenseMoveOrder = CurrentOrder;
	bHasLastFilteredDefenseMoveTarget = true;

	CurrentDefenseMoveForcedRefreshInterval =
		bGoalAreaEmergency
		? FMath::Max(
			0.01f,
			GoalAreaDefenseMoveForcedRefreshInterval
		)
		: BuildRandomDefenseMoveRefreshInterval();

	return true;
}

bool ASoccerAIController::TryBuildSmartAttackPassPlan(
	ASoccerAICharacter* SoccerCharacter,
	bool bUsePossessionRetentionThreshold,
	bool bRequireCurrentPossession,
	ASoccerCharacterBase*& OutReceiver,
	FVector& OutTargetLocation,
	ESoccerAttackPassType& OutPassType,
	float& OutPassScore,
	float& OutHorizontalSpeed
) const
{
	OutReceiver = nullptr;
	OutTargetLocation = FVector::ZeroVector;
	OutPassType = ESoccerAttackPassType::ToFeet;
	OutPassScore = 0.0f;
	OutHorizontalSpeed = 0.0f;

	if (!IsValid(SoccerCharacter))
	{
		return false;
	}

	if (!IsValid(MatchManager))
	{
		return false;
	}

	const bool bFoundPass =
		MatchManager->FindBestAttackPassOption(
			SoccerCharacter,
			OutReceiver,
			OutTargetLocation,
			OutPassType,
			OutPassScore,
			bUsePossessionRetentionThreshold,
			bRequireCurrentPossession
		);

	if (!bFoundPass || !IsValid(OutReceiver) || OutTargetLocation.IsNearlyZero())
	{
		return false;
	}

	const float ProfileRequiredPassScore =
		GetOffensiveProfileRequiredPassScore(
			SoccerCharacter,
			bUsePossessionRetentionThreshold
		);

	if (
		SoccerCharacter->HasPlayerProfile() &&
		OutPassScore < ProfileRequiredPassScore
	)
	{
		return false;
	}

	OutHorizontalSpeed = SmartAttackPassToFeetHorizontalSpeed;
	if (OutPassType == ESoccerAttackPassType::ForwardSpace)
	{
		OutHorizontalSpeed = SmartAttackPassToSpaceHorizontalSpeed;
	}
	else if (OutPassType == ESoccerAttackPassType::RetentionSpace)
	{
		OutHorizontalSpeed = SmartAttackRetentionPassHorizontalSpeed;
	}

	return true;
}

bool ASoccerAIController::TrySmartAttackPass(
	ASoccerAICharacter* SoccerCharacter,
	bool bUsePossessionRetentionThreshold
)
{
	if (!IsValid(SoccerCharacter) || !SoccerCharacter->IsAIPossessingBall())
	{
		return false;
	}

	ASoccerCharacterBase* Receiver = nullptr;
	FVector PassTargetLocation = FVector::ZeroVector;
	ESoccerAttackPassType PassType = ESoccerAttackPassType::ToFeet;
	float PassScore = 0.0f;
	float PassHorizontalSpeed = 0.0f;

	if (!TryBuildSmartAttackPassPlan(
		SoccerCharacter,
		bUsePossessionRetentionThreshold,
		true,
		Receiver,
		PassTargetLocation,
		PassType,
		PassScore,
		PassHorizontalSpeed
	))
	{
		return false;
	}

	if (!TryRegisterAIKickTouchForRules(SoccerCharacter))
	{
		return true;
	}

	SoccerCharacter->KickAIBallToTarget(
		PassTargetLocation,
		PassHorizontalSpeed,
		SmartAttackPassMinTravelTime,
		SmartAttackPassMaxTravelTime
	);

	MatchManager->RegisterOpenPlayPassIntent(
		SoccerCharacter,
		Receiver,
		PassTargetLocation
	);

	const TCHAR* PassTypeLogText =
		PassType == ESoccerAttackPassType::ForwardSpace
		? TEXT("FORWARD_SPACE")
		: PassType == ESoccerAttackPassType::RetentionSpace
		? TEXT("RETENTION_SPACE")
		: TEXT("TO_FEET");

	UE_LOG(
		LogTemp,
		Display,
		TEXT("[AttackPass] type=%s receiver=%s score=%.1f target=(%.0f, %.0f) retentionUnlocked=%s"),
		PassTypeLogText,
		*Receiver->GetName(),
		PassScore,
		PassTargetLocation.X,
		PassTargetLocation.Y,
		bUsePossessionRetentionThreshold ? TEXT("YES") : TEXT("NO")
	);

	if (GEngine)
	{
		const FString PassTypeText =
			PassType == ESoccerAttackPassType::ForwardSpace
			? TEXT("espacio adelante")
			: PassType == ESoccerAttackPassType::RetentionSpace
			? TEXT("espacio atras")
			: TEXT("pie");

		const TCHAR* DecisionText =
			bUsePossessionRetentionThreshold
			? TEXT("AI pase conservacion")
			: TEXT("AI pase");

		GEngine->AddOnScreenDebugMessage(
			-1,
			0.75f,
			FColor::Green,
			FString::Printf(
				TEXT("%s %s | Score %.0f"),
				DecisionText,
				*PassTypeText,
				PassScore
			)
		);
	}

	return true;
}
