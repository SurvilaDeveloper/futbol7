//SoccerMatchManager.cpp

#include "SoccerMatchManager.h"
#include "SoccerPenaltyKickRestart.h"
#include "SoccerFreeKickRestart.h"
#include "SoccerGoalLineRestart.h"
#include "SoccerPlayingState.h"
#include "SoccerBallOutOfPlayDelayState.h"
#include "SoccerPenaltyFoulDelayState.h"
#include "SoccerOffsideReviewFreezeState.h"
#include "SoccerPenaltyConfigurationState.h"
#include "SoccerPenaltyPreparationState.h"
#include "SoccerPenaltyExecutionState.h"
#include "SoccerOffsideConfigurationState.h"
#include "SoccerOffsidePreparationState.h"
#include "SoccerOffsideExecutionState.h"
#include "SoccerFaultConfigurationState.h"
#include "SoccerFaultPreparationState.h"
#include "SoccerFaultExecutionState.h"
#include "SoccerCornerConfigurationState.h"
#include "SoccerCornerPreparationState.h"
#include "SoccerCornerExecutionState.h"
#include "SoccerGoalKickConfigurationState.h"
#include "SoccerGoalKickPreparationState.h"
#include "SoccerGoalKickExecutionState.h"
#include "SoccerThrowInConfigurationState.h"
#include "SoccerThrowInPreparationState.h"
#include "SoccerThrowInExecutionState.h"
#include "SoccerKickoffConfigurationState.h"
#include "SoccerKickoffPreparationState.h"
#include "SoccerKickoffExecutionState.h"

#include "SoccerField.h"
#include "SoccerFieldDimensions.h"
#include "SoccerFormationLibrary.h"
#include "SoccerAICharacter.h"
#include "SoccerAIController.h"
#include "SoccerTeamTypes.h"
#include "SoccerBall.h"
#include "SoccerCharacterBase.h"
#include "SoccerDebugManager.h"
#include "SoccerOffsideLineActor.h"
#include "SoccerRestartRadiusActor.h"
#include "SoccerInstantReplayManager.h"
#include "ThirdPersonCppCharacter.h"
#include "SoccerGameInstance.h"
#include "SoccerPlayerProfile.h"
#include "SoccerPlayerAppearanceCatalog.h"
#include "SoccerClubProfile.h"
#include "SoccerSquadCatalog.h"
#include "SoccerLineupEvaluationLibrary.h"
#include "SoccerCoachProfile.h"
#include "SoccerCoachPlanningLibrary.h"

#include "Engine/CurveTable.h"
#include "Engine/Engine.h"
#include "Curves/RealCurve.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"

#include "EngineUtils.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"

#include "TimerManager.h"

#include "NavigationSystem.h"

#include <algorithm>

bool ASoccerMatchManager::ActivateMatchState(TUniquePtr<ISoccerMatchState> NewState)
{
	if (!NewState)
	{
		return false;
	}

	ClearActiveMatchState();
	PendingMatchStateTransition = ESoccerMatchStateTransition::None;
	ActiveMatchState = MoveTemp(NewState);

	if (!ActiveMatchState->Enter(*this))
	{
		ActiveMatchState.Reset();
		return false;
	}

	return true;
}

void ASoccerMatchManager::UpdateActiveMatchState(float DeltaTime)
{
	if (!ActiveMatchState)
	{
		return;
	}

	ActiveMatchState->Tick(*this, DeltaTime);
	ApplyPendingMatchStateTransition();
}

void ASoccerMatchManager::RequestMatchStateTransition(ESoccerMatchStateTransition Transition)
{
	if (Transition != ESoccerMatchStateTransition::None)
	{
		PendingMatchStateTransition = Transition;
	}
}

void ASoccerMatchManager::ApplyPendingMatchStateTransition()
{
	const ESoccerMatchStateTransition Transition = PendingMatchStateTransition;
	PendingMatchStateTransition = ESoccerMatchStateTransition::None;

	switch (Transition)
	{
	case ESoccerMatchStateTransition::PenaltyConfigurationAfterFoulDelay:
	{
		const ESoccerTeam RestartTeam = PendingPenaltyFoulRestartTeam;
		const FVector IncidentLocation = PendingPenaltyFoulIncidentLocation;
		PendingPenaltyFoulRestartTeam = ESoccerTeam::PlayerTeam;
		PendingPenaltyFoulIncidentLocation = FVector::ZeroVector;
		StartPenaltyKickRestart(RestartTeam, IncidentLocation);
		break;
	}

	case ESoccerMatchStateTransition::PenaltyPreparation:
		ActivateMatchState(MakeUnique<FSoccerPenaltyPreparationState>());
		break;

	case ESoccerMatchStateTransition::PenaltyExecution:
		ActivateMatchState(MakeUnique<FSoccerPenaltyExecutionState>());
		break;

	case ESoccerMatchStateTransition::OffsidePreparation:
		ActivateMatchState(MakeUnique<FSoccerOffsidePreparationState>());
		break;

	case ESoccerMatchStateTransition::OffsideExecution:
		ActivateMatchState(MakeUnique<FSoccerOffsideExecutionState>());
		break;

	case ESoccerMatchStateTransition::FaultPreparation:
		ActivateMatchState(MakeUnique<FSoccerFaultPreparationState>());
		break;

	case ESoccerMatchStateTransition::FaultExecution:
		ActivateMatchState(MakeUnique<FSoccerFaultExecutionState>());
		break;

	case ESoccerMatchStateTransition::CornerPreparation:
		ActivateMatchState(MakeUnique<FSoccerCornerPreparationState>());
		break;

	case ESoccerMatchStateTransition::CornerExecution:
		ActivateMatchState(MakeUnique<FSoccerCornerExecutionState>());
		break;

	case ESoccerMatchStateTransition::GoalKickPreparation:
		ActivateMatchState(MakeUnique<FSoccerGoalKickPreparationState>());
		break;

	case ESoccerMatchStateTransition::GoalKickExecution:
		ActivateMatchState(MakeUnique<FSoccerGoalKickExecutionState>());
		break;

	case ESoccerMatchStateTransition::ThrowInPreparation:
		ActivateMatchState(MakeUnique<FSoccerThrowInPreparationState>());
		break;

	case ESoccerMatchStateTransition::ThrowInExecution:
		ActivateMatchState(MakeUnique<FSoccerThrowInExecutionState>());
		break;

	case ESoccerMatchStateTransition::KickoffPreparation:
		ActivateMatchState(MakeUnique<FSoccerKickoffPreparationState>());
		break;

	case ESoccerMatchStateTransition::KickoffExecution:
		ActivateMatchState(MakeUnique<FSoccerKickoffExecutionState>());
		break;

	case ESoccerMatchStateTransition::BallOutOfPlayDelay:
		ActivateMatchState(MakeUnique<FSoccerBallOutOfPlayDelayState>(
			PendingBallOutOfPlayRestartType,
			PendingBallOutOfPlayRestartTeam,
			PendingBallOutOfPlayRestartReferenceLocation,
			PendingBallOutOfPlayThrowInInwardDirection,
			PendingBallOutOfPlayGoalLineSign
		));
		break;

	case ESoccerMatchStateTransition::CompleteBallOutOfPlayDelay:
		CompleteBallOutOfPlayDelay();
		break;

	case ESoccerMatchStateTransition::OffsideReviewFreeze:
		ActivateMatchState(MakeUnique<FSoccerOffsideReviewFreezeState>(
			PendingOffsideFreezeRestartTeam,
			PendingOffsideFreezeRestartLocation
		));
		break;

	case ESoccerMatchStateTransition::OffsideConfigurationAfterFreeze:
	{
		const ESoccerTeam RestartTeam = PendingOffsideFreezeRestartTeam;
		const FVector RestartLocation = PendingOffsideFreezeRestartLocation;
		ActivateMatchState(MakeUnique<FSoccerOffsideConfigurationState>(
			RestartTeam,
			RestartLocation
		));
		break;
	}

	case ESoccerMatchStateTransition::Playing:
	{
		const bool bWasPenaltyState = IsPenaltyMatchStateActive();
		const bool bWasOffsideState = IsOffsideMatchStateActive();
		const bool bWasFaultState = IsFaultMatchStateActive();
		const bool bWasCornerState = IsCornerMatchStateActive();
		const bool bWasGoalKickState = IsGoalKickMatchStateActive();
		const bool bWasThrowInState = IsThrowInMatchStateActive();
		const bool bWasKickoffState = IsKickoffMatchStateActive();
		const bool bThrowInWasSuccessfullyReleased =
			bWasThrowInState && bThrowInBallReleased;
		ClearActiveMatchState();
		if (bWasPenaltyState)
		{
			PenaltyKickRestart.Cancel(*this);
		}
		if (bWasOffsideState || bWasFaultState)
		{
			const bool bOwnedFreeKickContext =
				(bWasOffsideState && ActiveRestartType == ESoccerRestartType::OffsideFreeKick) ||
				(bWasFaultState && ActiveRestartType == ESoccerRestartType::DirectFreeKick);
			FreeKickRestart.ResetRuntime(*this);
			if (bOwnedFreeKickContext)
			{
				EndRestartContext();
			}
		}
		if (
			(bWasCornerState || bWasGoalKickState) &&
			(ActiveRestartType == ESoccerRestartType::CornerKick || ActiveRestartType == ESoccerRestartType::GoalKick) &&
			!IsGoalLineRestartActive()
		)
		{
			EndRestartContext();
		}
		if (bWasThrowInState && !bThrowInWasSuccessfullyReleased)
		{
			CancelThrowInRestart();
		}
		if (bWasKickoffState && ActiveRestartType == ESoccerRestartType::Kickoff)
		{
			EndRestartContext();
		}
		MatchPlayState = ESoccerMatchPlayState::Playing;
		ActivateMatchState(MakeUnique<FSoccerPlayingState>());

		if (bWasKickoffState)
		{
			StartCurrentHalfClockIfNeeded();
		}

		break;
	}


	case ESoccerMatchStateTransition::None:
	default:
		break;
	}
}

void ASoccerMatchManager::ClearActiveMatchState()
{
	if (ActiveMatchState)
	{
		ActiveMatchState->Exit(*this);
		ActiveMatchState.Reset();
	}

	PendingMatchStateTransition = ESoccerMatchStateTransition::None;
}

bool ASoccerMatchManager::IsPenaltyMatchStateActive() const
{
	if (!ActiveMatchState)
	{
		return false;
	}

	const ESoccerMatchStateId StateId = ActiveMatchState->GetStateId();
	return
		StateId == ESoccerMatchStateId::PenaltyFoulDelay ||
		StateId == ESoccerMatchStateId::PenaltyConfiguration ||
		StateId == ESoccerMatchStateId::PenaltyPreparation ||
		StateId == ESoccerMatchStateId::PenaltyExecution;
}

bool ASoccerMatchManager::IsOffsideMatchStateActive() const
{
	if (!ActiveMatchState)
	{
		return false;
	}

	const ESoccerMatchStateId StateId = ActiveMatchState->GetStateId();
	return
		StateId == ESoccerMatchStateId::OffsideConfiguration ||
		StateId == ESoccerMatchStateId::OffsidePreparation ||
		StateId == ESoccerMatchStateId::OffsideExecution;
}

bool ASoccerMatchManager::IsFaultMatchStateActive() const
{
	if (!ActiveMatchState)
	{
		return false;
	}

	const ESoccerMatchStateId StateId = ActiveMatchState->GetStateId();
	return
		StateId == ESoccerMatchStateId::FaultConfiguration ||
		StateId == ESoccerMatchStateId::FaultPreparation ||
		StateId == ESoccerMatchStateId::FaultExecution;
}

bool ASoccerMatchManager::IsCornerMatchStateActive() const
{
	if (!ActiveMatchState)
	{
		return false;
	}

	const ESoccerMatchStateId StateId = ActiveMatchState->GetStateId();
	return
		StateId == ESoccerMatchStateId::CornerConfiguration ||
		StateId == ESoccerMatchStateId::CornerPreparation ||
		StateId == ESoccerMatchStateId::CornerExecution;
}

bool ASoccerMatchManager::IsGoalKickMatchStateActive() const
{
	if (!ActiveMatchState)
	{
		return false;
	}

	const ESoccerMatchStateId StateId = ActiveMatchState->GetStateId();
	return
		StateId == ESoccerMatchStateId::GoalKickConfiguration ||
		StateId == ESoccerMatchStateId::GoalKickPreparation ||
		StateId == ESoccerMatchStateId::GoalKickExecution;
}

bool ASoccerMatchManager::IsThrowInMatchStateActive() const
{
	if (!ActiveMatchState)
	{
		return false;
	}

	const ESoccerMatchStateId StateId = ActiveMatchState->GetStateId();
	return
		StateId == ESoccerMatchStateId::ThrowInConfiguration ||
		StateId == ESoccerMatchStateId::ThrowInPreparation ||
		StateId == ESoccerMatchStateId::ThrowInExecution;
}

bool ASoccerMatchManager::IsKickoffMatchStateActive() const
{
	if (!ActiveMatchState)
	{
		return false;
	}

	const ESoccerMatchStateId StateId = ActiveMatchState->GetStateId();
	return
		StateId == ESoccerMatchStateId::KickoffConfiguration ||
		StateId == ESoccerMatchStateId::KickoffPreparation ||
		StateId == ESoccerMatchStateId::KickoffExecution;
}

ASoccerMatchManager::ASoccerMatchManager()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;

	OffsideLineActorClass = ASoccerOffsideLineActor::StaticClass();
	OffsideRestartHumanRadiusActorClass =
		ASoccerRestartRadiusActor::StaticClass();
	RestartHumanRestrictionIndicatorActorClass =
		ASoccerRestartRadiusActor::StaticClass();
}

void ASoccerMatchManager::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(
		LogTemp,
		Display,
		TEXT("[ClubMatch] PlayerTeam=%s | OpponentTeam=%s."),
		*GetClubIdForTeam(ESoccerTeam::PlayerTeam).ToString(),
		*GetClubIdForTeam(ESoccerTeam::OpponentTeam).ToString()
	);

	FindSoccerBall();
	InitializeInstantReplayRecorder();
	FindSoccerField();
	InitializeTeamFieldSides();
	CaptureInitialHumanFieldReferences();

	// Stage 7 (Director Technical): the persistent user setup is loaded before
	// kickoff. If there is no valid persistent setup, legacy PlayerTeam defaults
	// remain untouched and the old formation assignment path is used.
	const bool bPersistentPlayerTeamSetupApplied =
		ApplyPersistentDirectorTechnicalSetupToPlayerTeam(true);

	if (!bPersistentPlayerTeamSetupApplied)
	{
		RebuildFormationAssignmentsForTeam(
			ESoccerTeam::PlayerTeam,
			PlayerTeamFormationSystem,
			false
		);
	}

	RebuildFormationAssignmentsForTeam(
		ESoccerTeam::OpponentTeam,
		OpponentTeamFormationSystem,
		false
	);

	EnsureSlotTacticalInstructionsForTeam(ESoccerTeam::PlayerTeam);
	EnsureSlotTacticalInstructionsForTeam(ESoccerTeam::OpponentTeam);

	InitializeOpponentCoachAI();
	InitializeMatchClock();
	StartKickoff(FirstHalfKickoffTeam);
}

void ASoccerMatchManager::EndPlay(
	const EEndPlayReason::Type EndPlayReason
)
{
	GetWorldTimerManager().ClearTimer(GoalReplayStartTimerHandle);
	ShutdownInstantReplayRecorder();

	DestroyOffsideFreezeLine();
	DestroyActiveRestartHumanRestrictionIndicator();
	CancelBallOutOfPlayDelay();
	CancelThrowInRestart();
	CancelGoalLineRestart();
	CancelPenaltyKickRestart();
	ResetVisualSubstitution(true);

	UWorld* World = GetWorld();

	if (
		bOffsideFreezeAppliedGamePause &&
		World != nullptr &&
		UGameplayStatics::IsGamePaused(World)
		)
	{
		UGameplayStatics::SetGamePaused(World, false);
	}

	bOffsideFreezeAppliedGamePause = false;
	SetMatchPeriodHumanMoveLock(false);

	Super::EndPlay(EndPlayReason);
}

void ASoccerMatchManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Human pass requests are independent from the tactical match-state cadence.
	UpdateHumanPassRequestState();

	// Formation and individual-marking debug are independent diagnostic layers.
	DrawFormationDebug();
	DrawIndividualMarkingDebug();

	UpdateMatchClock(DeltaTime);
	UpdatePendingMatchSubstitutions();
	UpdateVisualSubstitution(DeltaTime);
	if (VisualSubstitutionPhase != ESoccerVisualSubstitutionPhase::None)
	{
		DestroyActiveRestartHumanRestrictionIndicator();
		return;
	}
	UpdateOpponentCoachAI(DeltaTime);
	UpdateOpponentCoachSubstitutionAI(DeltaTime);
	UpdateLiveTacticalShapeTransitions();

	// Half-time and full-time deliberately have no active football state.
	// The AI controller also checks the period and stays still during these breaks.
	if (!IsMatchPeriodGameplayActive())
	{
		DestroyActiveRestartHumanRestrictionIndicator();
		return;
	}

	// The explicit state machine is now the sole owner of match flow.
	// MatchPlayState remains only as a compatibility mirror for systems that
	// have not yet been migrated away from the legacy enum.
	if (!ActiveMatchState)
	{
		DestroyActiveRestartHumanRestrictionIndicator();
		return;
	}

	UpdateGroundBodyContests(DeltaTime);

	if (IsRestartContextActive())
	{
		UpdateActiveRestartRestrictionSystem(DeltaTime);
		UpdateActiveRestartLivePositioning(DeltaTime);
	}
	else
	{
		DestroyActiveRestartHumanRestrictionIndicator();
	}

	UpdateActiveMatchState(DeltaTime);
}

void ASoccerMatchManager::UpdateGroundBodyContests(float DeltaTime)
{
	UWorld* World = GetWorld();
	const bool bOpenPlayGroundContacts =
		MatchPlayState == ESoccerMatchPlayState::Playing &&
		!IsRestartContextActive();
	const bool bRestartWaitingGroundContacts =
		IsRestartContextActive() &&
		ActiveMatchState &&
		(
			ActiveMatchState->GetStateId() ==
				ESoccerMatchStateId::BallOutOfPlayDelay ||
			ActiveMatchState->GetPhase() == ESoccerStatePhase::Preparation ||
			ActiveMatchState->GetPhase() == ESoccerStatePhase::Execution
		);
	const bool bCanResolveGroundContacts =
		bEnableGroundBodyContests &&
		World != nullptr &&
		GetNetMode() != NM_Client &&
		IsMatchPeriodGameplayActive() &&
		(bOpenPlayGroundContacts || bRestartWaitingGroundContacts) &&
		!UGameplayStatics::IsGamePaused(World);

	if (!bCanResolveGroundContacts)
	{
		GroundBodyContestUpdateAccumulator = 0.0f;
		return;
	}

	// Restart waiting still allows ordinary opponents to compete for position.
	// The active taker is excluded so contact cannot delay or invalidate the
	// restart itself. A penalty goalkeeper is likewise an official participant
	// whose exact legal position must remain authoritative.
	const auto IsProtectedRestartParticipant =
		[this, bRestartWaitingGroundContacts](ASoccerCharacterBase* Character)
		{
			if (
				!bRestartWaitingGroundContacts ||
				!IsValid(Character)
			)
			{
				return false;
			}

			ASoccerAICharacter* AICharacter =
				Cast<ASoccerAICharacter>(Character);
			AThirdPersonCppCharacter* HumanCharacter =
				Cast<AThirdPersonCppCharacter>(Character);

			switch (ActiveRestartType)
			{
			case ESoccerRestartType::Kickoff:
				return
					IsKickoffTaker(AICharacter) ||
					IsHumanFootRestartTaker(HumanCharacter);

			case ESoccerRestartType::OffsideFreeKick:
			case ESoccerRestartType::DirectFreeKick:
				return
					IsOffsideRestartTaker(AICharacter) ||
					IsHumanFreeKickTaker(HumanCharacter);

			case ESoccerRestartType::ThrowIn:
				return
					IsThrowInTaker(AICharacter) ||
					IsHumanThrowInTaker(HumanCharacter);

			case ESoccerRestartType::CornerKick:
			case ESoccerRestartType::GoalKick:
				return
					IsGoalLineRestartTaker(AICharacter) ||
					IsHumanFootRestartTaker(HumanCharacter);

			case ESoccerRestartType::PenaltyKick:
				return
					IsPenaltyKickTaker(Character) ||
					IsPenaltyKickDefendingGoalkeeper(AICharacter);

			case ESoccerRestartType::None:
			default:
				return false;
			}
		};

	GroundBodyContestUpdateAccumulator += FMath::Max(0.0f, DeltaTime);
	const float SafeUpdateInterval = FMath::Max(
		0.01f,
		GroundBodyContestUpdateInterval
	);

	if (GroundBodyContestUpdateAccumulator < SafeUpdateInterval)
	{
		return;
	}

	// A hitch must not turn one contact update into an explosive shove.
	const float ContactStepSeconds = FMath::Clamp(
		GroundBodyContestUpdateAccumulator,
		SafeUpdateInterval,
		0.10f
	);
	GroundBodyContestUpdateAccumulator = 0.0f;

	TArray<ASoccerCharacterBase*> Contestants;
	for (TActorIterator<ASoccerCharacterBase> It(World); It; ++It)
	{
		ASoccerCharacterBase* Candidate = *It;
		if (!IsValid(Candidate) || !Candidate->CanParticipateInGroundBodyContest())
		{
			continue;
		}

		if (IsProtectedRestartParticipant(Candidate))
		{
			// Remove any short response tail left from the frame in which this
			// character became the designated restart participant.
			Candidate->ResetGroundBodyContactResponse();
			continue;
		}

		// A goalkeeper holding the ball is protected by its dedicated possession
		// rules and must not be displaced by the ordinary shoulder system.
		const ASoccerAICharacter* CandidateAI =
			Cast<ASoccerAICharacter>(Candidate);
		if (IsValid(CandidateAI) && CandidateAI->IsGoalkeeperHoldingBall())
		{
			continue;
		}

		Contestants.Add(Candidate);
	}

	const float SafeContactExtraRadius = FMath::Max(
		0.0f,
		GroundBodyContestContactExtraRadius
	);
	const float SafeMaximumVerticalSeparation = FMath::Max(
		0.0f,
		GroundBodyContestMaximumVerticalSeparation
	);
	const float SafeMinimumDrive = FMath::Clamp(
		GroundBodyContestMinimumDrive,
		0.0f,
		1.0f
	);
	const float SafeMinimumWeightKg = FMath::Max(
		1.0f,
		GroundBodyContestMinimumWeightKg
	);
	const float SafeMaximumWeightKg = FMath::Max(
		SafeMinimumWeightKg,
		GroundBodyContestMaximumWeightKg
	);
	const float SafeReferenceWeightKg = FMath::Clamp(
		GroundBodyContestReferenceWeightKg,
		SafeMinimumWeightKg,
		SafeMaximumWeightKg
	);
	const float SafeWeightMomentumInfluence = FMath::Clamp(
		GroundBodyContestWeightMomentumInfluence,
		0.0f,
		1.0f
	);
	const float SafeWeightInertiaInfluence = FMath::Clamp(
		GroundBodyContestWeightInertiaInfluence,
		0.0f,
		1.0f
	);
	const float SafeNetDeadZone = FMath::Clamp(
		GroundBodyContestNetDriveDeadZone,
		0.0f,
		0.95f
	);
	const float SafeBraceRearDot = FMath::Clamp(
		GroundBodyBraceRearDotThreshold,
		-1.0f,
		0.0f
	);
	const float SafeBraceMaximumOwnMovement = FMath::Clamp(
		GroundBodyBraceMaximumOwnMovementAlpha,
		0.0f,
		1.0f
	);
	const float SafeBraceReactionScale = FMath::Clamp(
		GroundBodyBraceIncomingReactionScale,
		0.10f,
		1.0f
	);
	const float SafeRearSurpriseReactionDelay = FMath::Max(
		0.0f,
		GroundBodyRearSurpriseReactionDelay
	);
	const float SafeBraceHoldTime = FMath::Max(
		0.0f,
		GroundBodyBraceHoldTime
	);
	const float SafeRearAwarenessHoldTime = FMath::Max(
		SafeBraceHoldTime,
		GroundBodyRearAwarenessHoldTime
	);
	const float SafeRearSurpriseResponseMultiplier = FMath::Max(
		1.0f,
		GroundBodyRearSurpriseResponseMultiplier
	);
	const float SafePersistentPushHoldTime = FMath::Max(
		SafeUpdateInterval,
		GroundBodyContestPersistentPushHoldTime
	);
	const float SafeOpposingVelocitySuppression = FMath::Clamp(
		GroundBodyContestOpposingVelocitySuppression,
		0.0f,
		1.0f
	);
	const float SafeMaximumTranslationSpeed = FMath::Max(
		0.0f,
		GroundBodyContestMaximumTranslationSpeed
	);

	for (int32 FirstIndex = 0; FirstIndex < Contestants.Num(); ++FirstIndex)
	{
		ASoccerCharacterBase* FirstCharacter = Contestants[FirstIndex];
		const UCapsuleComponent* FirstCapsule =
			FirstCharacter->GetCapsuleComponent();
		if (FirstCapsule == nullptr)
		{
			continue;
		}

		for (
			int32 SecondIndex = FirstIndex + 1;
			SecondIndex < Contestants.Num();
			++SecondIndex
		)
		{
			ASoccerCharacterBase* SecondCharacter = Contestants[SecondIndex];
			if (
				!IsValid(SecondCharacter) ||
				FirstCharacter->GetTeam() == SecondCharacter->GetTeam()
			)
			{
				continue;
			}

			const UCapsuleComponent* SecondCapsule =
				SecondCharacter->GetCapsuleComponent();
			if (SecondCapsule == nullptr)
			{
				continue;
			}

			const FVector FirstLocation = FirstCharacter->GetActorLocation();
			const FVector SecondLocation = SecondCharacter->GetActorLocation();
			if (
				FMath::Abs(SecondLocation.Z - FirstLocation.Z) >
				SafeMaximumVerticalSeparation
			)
			{
				continue;
			}

			FVector FirstToSecond = SecondLocation - FirstLocation;
			FirstToSecond.Z = 0.0f;
			const float ContactDistance =
				FirstCapsule->GetScaledCapsuleRadius() +
				SecondCapsule->GetScaledCapsuleRadius() +
				SafeContactExtraRadius;

			if (FirstToSecond.SizeSquared() > FMath::Square(ContactDistance))
			{
				continue;
			}

			if (FirstToSecond.IsNearlyZero())
			{
				FirstToSecond =
					FirstCharacter->GetUniqueID() < SecondCharacter->GetUniqueID()
					? FirstCharacter->GetActorRightVector()
					: -FirstCharacter->GetActorRightVector();
				FirstToSecond.Z = 0.0f;
			}

			const FVector FirstToSecondDirection =
				FirstToSecond.GetSafeNormal();
			if (FirstToSecondDirection.IsNearlyZero())
			{
				continue;
			}

			float FirstDrive = FirstCharacter->GetGroundBodyContestDriveToward(
				FirstToSecondDirection,
				GroundBodyContestMomentumDriveWeight
			);
			float SecondDrive = SecondCharacter->GetGroundBodyContestDriveToward(
				-FirstToSecondDirection,
				GroundBodyContestMomentumDriveWeight
			);

			FirstDrive = FirstDrive >= SafeMinimumDrive ? FirstDrive : 0.0f;
			SecondDrive = SecondDrive >= SafeMinimumDrive ? SecondDrive : 0.0f;
			if (FirstDrive <= 0.0f && SecondDrive <= 0.0f)
			{
				continue;
			}

			const float FirstWeightKg = FMath::Clamp(
				FirstCharacter->GetPlayerProfileWeightKg(
					SafeReferenceWeightKg
				),
				SafeMinimumWeightKg,
				SafeMaximumWeightKg
			);
			const float SecondWeightKg = FMath::Clamp(
				SecondCharacter->GetPlayerProfileWeightKg(
					SafeReferenceWeightKg
				),
				SafeMinimumWeightKg,
				SafeMaximumWeightKg
			);
			const float FirstWeightMomentumMultiplier = FMath::Clamp(
				FMath::Lerp(
					1.0f,
					FirstWeightKg / SafeReferenceWeightKg,
					SafeWeightMomentumInfluence
				),
				0.75f,
				1.25f
			);
			const float SecondWeightMomentumMultiplier = FMath::Clamp(
				FMath::Lerp(
					1.0f,
					SecondWeightKg / SafeReferenceWeightKg,
					SafeWeightMomentumInfluence
				),
				0.75f,
				1.25f
			);

			FVector FirstForward = FirstCharacter->GetActorForwardVector();
			FirstForward.Z = 0.0f;
			FirstForward = FirstForward.GetSafeNormal();
			FVector SecondForward = SecondCharacter->GetActorForwardVector();
			SecondForward.Z = 0.0f;
			SecondForward = SecondForward.GetSafeNormal();

			const bool bFirstReceivesRearPressure =
				SecondDrive > 0.0f &&
				!FirstForward.IsNearlyZero() &&
				FVector::DotProduct(
					FirstForward,
					FirstToSecondDirection
				) <= SafeBraceRearDot;

			const bool bSecondReceivesRearPressure =
				FirstDrive > 0.0f &&
				!SecondForward.IsNearlyZero() &&
				FVector::DotProduct(
					SecondForward,
					-FirstToSecondDirection
				) <= SafeBraceRearDot;

			bool bFirstWasSurprisedByRearPressure = false;
			bool bSecondWasSurprisedByRearPressure = false;
			if (bFirstReceivesRearPressure)
			{
				bFirstWasSurprisedByRearPressure =
					FirstCharacter->RegisterGroundBodyRearPressure(
						SafeRearSurpriseReactionDelay,
						SafeBraceHoldTime,
						SafeRearAwarenessHoldTime,
						SafeBraceMaximumOwnMovement
					);
			}
			if (bSecondReceivesRearPressure)
			{
				bSecondWasSurprisedByRearPressure =
					SecondCharacter->RegisterGroundBodyRearPressure(
						SafeRearSurpriseReactionDelay,
						SafeBraceHoldTime,
						SafeRearAwarenessHoldTime,
						SafeBraceMaximumOwnMovement
					);
			}

			// Both players contribute opposing drive along the contact normal.
			// Strength remains primary; WeightKg adds only a moderate momentum term.
			const float EffectOnSecond =
				FirstDrive *
				FirstCharacter->GetPlayerProfileStrengthBodyForceMultiplier() *
				FirstWeightMomentumMultiplier *
				SecondCharacter->GetPlayerProfileStrengthBodyResistanceMultiplier();
			const float EffectOnFirst =
				SecondDrive *
				SecondCharacter->GetPlayerProfileStrengthBodyForceMultiplier() *
				SecondWeightMomentumMultiplier *
				FirstCharacter->GetPlayerProfileStrengthBodyResistanceMultiplier();
			const float NetDriveTowardSecond = EffectOnSecond - EffectOnFirst;

			if (FMath::Abs(NetDriveTowardSecond) <= SafeNetDeadZone)
			{
				continue;
			}

			float PushAlpha = FMath::Clamp(
				(FMath::Abs(NetDriveTowardSecond) - SafeNetDeadZone) /
					FMath::Max(0.05f, 1.0f - SafeNetDeadZone),
				0.0f,
				1.0f
			);

			ASoccerCharacterBase* DisplacedCharacter =
				NetDriveTowardSecond > 0.0f
				? SecondCharacter
				: FirstCharacter;
			const FVector DisplacementDirection =
				NetDriveTowardSecond > 0.0f
				? FirstToSecondDirection
				: -FirstToSecondDirection;
			const bool bDisplacedReceivesRearPressure =
				DisplacedCharacter == FirstCharacter
					? bFirstReceivesRearPressure
					: bSecondReceivesRearPressure;
			const bool bDisplacedWasSurprisedByRearPressure =
				DisplacedCharacter == FirstCharacter
					? bFirstWasSurprisedByRearPressure
					: bSecondWasSurprisedByRearPressure;
			const bool bDisplacedIsBracingRearPressure =
				bDisplacedReceivesRearPressure &&
				DisplacedCharacter->GetSoccerIsBracingPhysicalContact();

			const float DisplacedStrengthReaction = FMath::Lerp(
				GroundBodyStrengthReactionMultiplierAtZero,
				GroundBodyStrengthReactionMultiplierAtHundred,
				DisplacedCharacter->GetPlayerProfileStrengthAlpha()
			);
			const float DisplacedBalanceReaction =
				DisplacedCharacter->GetPlayerProfileBalanceBodyReactionMultiplier(
					GroundBodyBalanceReactionMultiplierAtZero,
					GroundBodyBalanceReactionMultiplierAtHundred
				);
			const float DisplacedWeightKg =
				DisplacedCharacter == FirstCharacter
					? FirstWeightKg
					: SecondWeightKg;
			const float DisplacedWeightReaction = FMath::Clamp(
				FMath::Lerp(
					1.0f,
					SafeReferenceWeightKg / DisplacedWeightKg,
					SafeWeightInertiaInfluence
				),
				0.65f,
				1.45f
			);

			// Balance, Strength resistance and body mass determine acceleration of
			// the loser, but never reverse the already resolved winner direction.
			PushAlpha *=
				FMath::Max(0.0f, DisplacedStrengthReaction) *
				FMath::Max(0.0f, DisplacedBalanceReaction) *
				FMath::Max(0.0f, DisplacedWeightReaction);
			if (bDisplacedIsBracingRearPressure)
			{
				PushAlpha *= SafeBraceReactionScale;
			}
			PushAlpha = FMath::Clamp(PushAlpha, 0.0f, 1.0f);
			const float RearSurpriseResponseMultiplier =
				bDisplacedWasSurprisedByRearPressure
					? SafeRearSurpriseResponseMultiplier
					: 1.0f;

			const float SpeedChange =
				FMath::Max(0.0f, GroundBodyContestPushAcceleration) *
				ContactStepSeconds *
				PushAlpha *
				RearSurpriseResponseMultiplier;
			const float PersistentTranslationSpeed =
				SafeMaximumTranslationSpeed *
				PushAlpha *
				RearSurpriseResponseMultiplier;
			const float MaximumPersistentTranslationSpeed =
				SafeMaximumTranslationSpeed *
				RearSurpriseResponseMultiplier;

			DisplacedCharacter->ApplyGroundBodyContestPush(
				DisplacementDirection,
				SpeedChange,
				FMath::Max(0.0f, GroundBodyContestMaximumPushSpeed),
				PersistentTranslationSpeed,
				MaximumPersistentTranslationSpeed,
				SafePersistentPushHoldTime,
				SafeOpposingVelocitySuppression
			);

			if (bDrawGroundBodyContestDebug)
			{
				const FVector DebugStart =
					DisplacedCharacter->GetActorLocation() +
					FVector(0.0f, 0.0f, 105.0f);
				DrawDebugDirectionalArrow(
					World,
					DebugStart,
					DebugStart + DisplacementDirection * 120.0f,
					22.0f,
					bDisplacedWasSurprisedByRearPressure
						? FColor::Yellow
						: bDisplacedIsBracingRearPressure
							? FColor::Cyan
							: FColor::Orange,
					false,
					SafeUpdateInterval * 1.5f,
					0,
					3.0f
				);
			}
		}
	}
}

void ASoccerMatchManager::InitializeMatchClock()
{
	CurrentMatchPeriod = ESoccerMatchPeriod::FirstHalf;
	CurrentHalfElapsedSeconds = 0.0f;
	TotalMatchElapsedSeconds = 0.0f;
	HalfTimeElapsedSeconds = 0.0f;
	bCurrentHalfClockStarted = false;
	SetMatchPeriodHumanMoveLock(false);
}

void ASoccerMatchManager::UpdateMatchClock(float DeltaTime)
{
	if (!bEnableMatchClock)
	{
		return;
	}

	const float SafeDeltaTime = FMath::Max(0.0f, DeltaTime);

	if (CurrentMatchPeriod == ESoccerMatchPeriod::HalfTime)
	{
		// If another system pauses the world (for example the tactics menu),
		// the half-time countdown pauses as well. This avoids competing pause owners.
		if (GetWorld() != nullptr && UGameplayStatics::IsGamePaused(GetWorld()))
		{
			return;
		}

		HalfTimeElapsedSeconds += SafeDeltaTime;
		UpdateHalfTimeFieldTransition();

		const float MinimumHalfTimeDuration =
			FMath::Max(0.0f, HalfTimeDurationSeconds);

		const bool bMinimumHalfTimeElapsed =
			HalfTimeElapsedSeconds >= MinimumHalfTimeDuration;

		const bool bTransitionComplete =
			!bEnableHalfTimeFieldTransition ||
			AreAllHalfTimeFieldTransitionBotsComplete();

		const float MaximumTransitionDuration = FMath::Max(
			MinimumHalfTimeDuration,
			FMath::Max(1.0f, HalfTimeFieldTransitionMaximumDurationSeconds)
		);

		const bool bTransitionTimedOut =
			HalfTimeElapsedSeconds >= MaximumTransitionDuration;

		if (
			bMinimumHalfTimeElapsed &&
			(bTransitionComplete || bTransitionTimedOut)
		)
		{
			CompleteHalfTime();
		}

		return;
	}

	if (CurrentMatchPeriod == ESoccerMatchPeriod::FullTime)
	{
		return;
	}

	if (!bCurrentHalfClockStarted)
	{
		return;
	}

	if (GetWorld() != nullptr && UGameplayStatics::IsGamePaused(GetWorld()))
	{
		return;
	}

	const float SafeHalfDuration = FMath::Max(1.0f, HalfDurationSeconds);
	CurrentHalfElapsedSeconds = FMath::Min(
		SafeHalfDuration,
		CurrentHalfElapsedSeconds + SafeDeltaTime
	);

	const float CompletedHalfTime =
		CurrentMatchPeriod == ESoccerMatchPeriod::SecondHalf
		? SafeHalfDuration
		: 0.0f;

	TotalMatchElapsedSeconds = FMath::Min(
		SafeHalfDuration * 2.0f,
		CompletedHalfTime + CurrentHalfElapsedSeconds
	);

	if (CurrentHalfElapsedSeconds < SafeHalfDuration)
	{
		return;
	}

	if (CurrentMatchPeriod == ESoccerMatchPeriod::FirstHalf)
	{
		BeginHalfTime();
	}
	else if (CurrentMatchPeriod == ESoccerMatchPeriod::SecondHalf)
	{
		BeginFullTime();
	}
}

void ASoccerMatchManager::StartCurrentHalfClockIfNeeded()
{
	if (!bEnableMatchClock || bCurrentHalfClockStarted)
	{
		return;
	}

	if (
		CurrentMatchPeriod != ESoccerMatchPeriod::FirstHalf &&
		CurrentMatchPeriod != ESoccerMatchPeriod::SecondHalf
	)
	{
		return;
	}

	bCurrentHalfClockStarted = true;
}

void ASoccerMatchManager::BeginHalfTime()
{
	if (CurrentMatchPeriod != ESoccerMatchPeriod::FirstHalf)
	{
		return;
	}

	const float SafeHalfDuration = FMath::Max(1.0f, HalfDurationSeconds);
	CurrentHalfElapsedSeconds = SafeHalfDuration;
	TotalMatchElapsedSeconds = SafeHalfDuration;
	HalfTimeElapsedSeconds = 0.0f;
	bCurrentHalfClockStarted = false;
	CurrentMatchPeriod = ESoccerMatchPeriod::HalfTime;

	PrepareForMatchPeriodBreak();
	InitializeHalfTimeFieldTransition();
	SetMatchPeriodHumanMoveLock(true);

	if (GEngine != nullptr)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			FMath::Max(1.0f, HalfTimeDurationSeconds),
			FColor::Yellow,
			TEXT("ENTRETIEMPO")
		);
	}
}

void ASoccerMatchManager::CompleteHalfTime()
{
	if (CurrentMatchPeriod != ESoccerMatchPeriod::HalfTime)
	{
		return;
	}

	CurrentMatchPeriod = ESoccerMatchPeriod::SecondHalf;
	CurrentHalfElapsedSeconds = 0.0f;
	HalfTimeElapsedSeconds = 0.0f;
	bCurrentHalfClockStarted = false;

	// Stage 10D: change ends before the second-half kickoff is configured. The
	// staged halftime transition has already moved AI players into the opposite
	// half through separated corridors; now geometry-driven consumers may adopt
	// the new team-side signs and normal kickoff positioning can finish the job.
	ApplySecondHalfFieldSideSwap();
	ResetHalfTimeFieldTransition();

	// Keep the human locked while the side swap/reposition is applied. Once the
	// new references are coherent, normal kickoff preparation may take over.
	SetMatchPeriodHumanMoveLock(false);
	StartKickoff(GetOppositeTeam(FirstHalfKickoffTeam));
}

void ASoccerMatchManager::BeginFullTime()
{
	if (CurrentMatchPeriod != ESoccerMatchPeriod::SecondHalf)
	{
		return;
	}

	const float SafeHalfDuration = FMath::Max(1.0f, HalfDurationSeconds);
	CurrentHalfElapsedSeconds = SafeHalfDuration;
	TotalMatchElapsedSeconds = SafeHalfDuration * 2.0f;
	bCurrentHalfClockStarted = false;
	CurrentMatchPeriod = ESoccerMatchPeriod::FullTime;

	PrepareForMatchPeriodBreak();
	SetMatchPeriodHumanMoveLock(true);

	if (GEngine != nullptr)
	{
		const FString FinalMessage = FString::Printf(
			TEXT("FINAL - PlayerTeam %d - %d OpponentTeam"),
			PlayerTeamScore,
			OpponentTeamScore
		);

		GEngine->AddOnScreenDebugMessage(
			-1,
			5.0f,
			FColor::Yellow,
			FinalMessage
		);
	}
}

void ASoccerMatchManager::PrepareForMatchPeriodBreak()
{
	if (GetWorld() != nullptr)
	{
		GetWorldTimerManager().ClearTimer(GoalResetTimerHandle);
		GetWorldTimerManager().ClearTimer(GoalReplayStartTimerHandle);
	}

	ClearActiveMatchState();
	CancelBallOutOfPlayDelay();
	CancelThrowInRestart();
	CancelGoalLineRestart();
	CancelPenaltyKickRestart();

	if (IsRestartContextActive())
	{
		EndRestartContext();
	}

	DestroyOffsideFreezeLine();
	DestroyActiveRestartHumanRestrictionIndicator();

	PossessionTeam = ESoccerPossessionTeam::None;
	PossessingCharacter = nullptr;
	ClearAttackState();
	ClearAssignedAI();
	ClearNoRetouchRestriction();
	ClearAttackRunRelease();
	ReleaseAllAIBallPossessions();

	UWorld* World = GetWorld();
	if (World != nullptr)
	{
		for (TActorIterator<ASoccerAICharacter> It(World); It; ++It)
		{
			ASoccerAICharacter* SoccerAICharacter = *It;
			if (!IsValid(SoccerAICharacter))
			{
				continue;
			}

			if (ASoccerAIController* SoccerAIController =
				Cast<ASoccerAIController>(SoccerAICharacter->GetController()))
			{
				SoccerAIController->StopMovement();
			}

			SoccerAICharacter->ClearScriptedLocomotionVelocity();

			if (UCharacterMovementComponent* Movement =
				SoccerAICharacter->GetCharacterMovement())
			{
				Movement->StopMovementImmediately();
			}
		}

		for (TActorIterator<AThirdPersonCppCharacter> It(World); It; ++It)
		{
			AThirdPersonCppCharacter* HumanCharacter = *It;
			if (!IsValid(HumanCharacter))
			{
				continue;
			}

			// Also clears an assisted chase/autopass that could otherwise resume
			// automatically when the second-half kickoff is prepared.
			HumanCharacter->ReleaseBallForMatchRestart(false);

			if (UCharacterMovementComponent* Movement =
				HumanCharacter->GetCharacterMovement())
			{
				Movement->StopMovementImmediately();
			}
		}
	}

	if (IsValid(SoccerBall))
	{
		SoccerBall->SetPossessed(false);
		SoccerBall->StopBallKeepingPhysics();
	}

	MatchPlayState = ESoccerMatchPlayState::Resetting;
}

void ASoccerMatchManager::SetMatchPeriodHumanMoveLock(bool bLocked)
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		if (!bLocked)
		{
			bMatchPeriodAppliedMoveInputLock = false;
		}
		return;
	}

	APlayerController* PlayerController =
		UGameplayStatics::GetPlayerController(World, 0);

	if (PlayerController == nullptr)
	{
		return;
	}

	if (bLocked)
	{
		if (!bMatchPeriodAppliedMoveInputLock)
		{
			PlayerController->SetIgnoreMoveInput(true);
			bMatchPeriodAppliedMoveInputLock = true;
		}

		return;
	}

	if (bMatchPeriodAppliedMoveInputLock)
	{
		PlayerController->SetIgnoreMoveInput(false);
		bMatchPeriodAppliedMoveInputLock = false;
	}
}

ESoccerMatchPeriod ASoccerMatchManager::GetCurrentMatchPeriod() const
{
	return CurrentMatchPeriod;
}

float ASoccerMatchManager::GetCurrentHalfElapsedSeconds() const
{
	return CurrentHalfElapsedSeconds;
}

float ASoccerMatchManager::GetTotalMatchElapsedSeconds() const
{
	return TotalMatchElapsedSeconds;
}

float ASoccerMatchManager::GetCurrentHalfProgress() const
{
	const float SafeHalfDuration = FMath::Max(1.0f, HalfDurationSeconds);

	if (CurrentMatchPeriod == ESoccerMatchPeriod::FullTime)
	{
		return 1.0f;
	}

	if (CurrentMatchPeriod == ESoccerMatchPeriod::HalfTime)
	{
		return 1.0f;
	}

	return FMath::Clamp(
		CurrentHalfElapsedSeconds / SafeHalfDuration,
		0.0f,
		1.0f
	);
}

float ASoccerMatchManager::GetMatchProgress() const
{
	const float SafeMatchDuration = FMath::Max(2.0f, HalfDurationSeconds * 2.0f);
	return FMath::Clamp(
		TotalMatchElapsedSeconds / SafeMatchDuration,
		0.0f,
		1.0f
	);
}

float ASoccerMatchManager::GetHalfTimeRemainingSeconds() const
{
	if (CurrentMatchPeriod != ESoccerMatchPeriod::HalfTime)
	{
		return 0.0f;
	}

	return FMath::Max(
		0.0f,
		FMath::Max(0.0f, HalfTimeDurationSeconds) - HalfTimeElapsedSeconds
	);
}

bool ASoccerMatchManager::IsMatchPeriodGameplayActive() const
{
	return
		CurrentMatchPeriod == ESoccerMatchPeriod::FirstHalf ||
		CurrentMatchPeriod == ESoccerMatchPeriod::SecondHalf;
}

bool ASoccerMatchManager::IsHalfTimeFieldTransitionActive() const
{
	return
		CurrentMatchPeriod == ESoccerMatchPeriod::HalfTime &&
		bEnableHalfTimeFieldTransition &&
		bHalfTimeFieldTransitionRuntimeActive;
}

bool ASoccerMatchManager::GetHalfTimeFieldTransitionMoveTarget(
	const ASoccerAICharacter* SoccerAICharacter,
	FVector& OutMoveLocation,
	float& OutAcceptanceRadius
) const
{
	OutMoveLocation = FVector::ZeroVector;
	OutAcceptanceRadius = FMath::Max(
		20.0f,
		HalfTimeFieldTransitionAcceptanceRadiusCm
	);

	if (
		!IsHalfTimeFieldTransitionActive() ||
		!IsValid(SoccerAICharacter) ||
		HalfTimeFieldTransitionExitCompleted.Contains(
			const_cast<ASoccerAICharacter*>(SoccerAICharacter)
		)
	)
	{
		return false;
	}

	ASoccerAICharacter* MutableCharacter =
		const_cast<ASoccerAICharacter*>(SoccerAICharacter);

	if (!HalfTimeFieldTransitionEntryCompleted.Contains(MutableCharacter))
	{
		if (const FVector* EntryTarget =
			HalfTimeFieldTransitionEntryTargets.Find(MutableCharacter))
		{
			OutMoveLocation = *EntryTarget;
			return !OutMoveLocation.IsNearlyZero();
		}
	}

	if (const FVector* ExitTarget =
		HalfTimeFieldTransitionExitTargets.Find(MutableCharacter))
	{
		OutMoveLocation = *ExitTarget;
		return !OutMoveLocation.IsNearlyZero();
	}

	return false;
}

void ASoccerMatchManager::InitializeHalfTimeFieldTransition()
{
	ResetHalfTimeFieldTransition();

	if (!bEnableHalfTimeFieldTransition || !IsValid(SoccerField))
	{
		return;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	TArray<ASoccerAICharacter*> PlayerTeamCharacters;
	TArray<ASoccerAICharacter*> OpponentTeamCharacters;

	for (TActorIterator<ASoccerAICharacter> It(World); It; ++It)
	{
		ASoccerAICharacter* SoccerAICharacter = *It;
		if (!IsValid(SoccerAICharacter))
		{
			continue;
		}

		if (SoccerAICharacter->GetTeam() == ESoccerTeam::PlayerTeam)
		{
			PlayerTeamCharacters.Add(SoccerAICharacter);
		}
		else
		{
			OpponentTeamCharacters.Add(SoccerAICharacter);
		}
	}

	auto BuildTeamTargets = [this](
		ESoccerTeam Team,
		TArray<ASoccerAICharacter*>& TeamCharacters
	)
	{
		if (TeamCharacters.Num() <= 0 || !IsValid(SoccerField))
		{
			return;
		}

		// Preserve the players' lateral order at the whistle. This makes the fan-out
		// look like a group exchanging ends instead of randomly crossing lanes.
		TeamCharacters.Sort([this](
			const ASoccerAICharacter& A,
			const ASoccerAICharacter& B
		)
		{
			const float LocalYA = SoccerField->WorldToPitchLocal(
				A.GetActorLocation()
			).Y;
			const float LocalYB = SoccerField->WorldToPitchLocal(
				B.GetActorLocation()
			).Y;
			return LocalYA < LocalYB;
		});

		const float CorridorSideSign =
			Team == ESoccerTeam::PlayerTeam ? 1.0f : -1.0f;

		const float SafeCorridorAlpha = FMath::Clamp(
			HalfTimeFieldTransitionCorridorLateralAlpha,
			0.15f,
			0.90f
		);

		const float CorridorCenterY =
			CorridorSideSign *
			SoccerFieldDimensions::HalfPitchWidthCm *
			SafeCorridorAlpha;

		const float MaximumAllowedHalfSpread = FMath::Max(
			0.0f,
			SoccerFieldDimensions::HalfPitchWidthCm -
			FMath::Abs(CorridorCenterY) - 120.0f
		);

		const float SafeLaneHalfSpread = FMath::Clamp(
			HalfTimeFieldTransitionLaneHalfSpreadCm,
			0.0f,
			MaximumAllowedHalfSpread
		);

		const float DestinationHalfSign =
			-SoccerFieldDimensions::NormalizeGoalLineSign(
				GetOwnGoalLineSign(Team)
			);

		const float SafeEntryDepth = FMath::Clamp(
			HalfTimeFieldTransitionEntryDepthFromHalfwayCm,
			50.0f,
			SoccerFieldDimensions::HalfPitchLengthCm - 150.0f
		);

		const float SafeExitDepth = FMath::Clamp(
			FMath::Max(
				HalfTimeFieldTransitionExitDepthFromHalfwayCm,
				SafeEntryDepth + 100.0f
			),
			100.0f,
			SoccerFieldDimensions::HalfPitchLengthCm - 150.0f
		);

		for (int32 PlayerIndex = 0; PlayerIndex < TeamCharacters.Num(); ++PlayerIndex)
		{
			ASoccerAICharacter* SoccerAICharacter = TeamCharacters[PlayerIndex];
			if (!IsValid(SoccerAICharacter))
			{
				continue;
			}

			const float LaneAlpha = TeamCharacters.Num() > 1
				? static_cast<float>(PlayerIndex) /
					static_cast<float>(TeamCharacters.Num() - 1)
				: 0.5f;

			const float LaneY = FMath::Lerp(
				CorridorCenterY - SafeLaneHalfSpread,
				CorridorCenterY + SafeLaneHalfSpread,
				LaneAlpha
			);

			FVector EntryLocalLocation(
				-DestinationHalfSign * SafeEntryDepth,
				LaneY,
				0.0f
			);
			FVector ExitLocalLocation(
				DestinationHalfSign * SafeExitDepth,
				LaneY,
				0.0f
			);

			EntryLocalLocation = SoccerFieldDimensions::ClampLocationInsidePitch(
				EntryLocalLocation,
				100.0f
			);
			ExitLocalLocation = SoccerFieldDimensions::ClampLocationInsidePitch(
				ExitLocalLocation,
				100.0f
			);

			FVector EntryWorldLocation = SoccerField->PitchLocalToWorld(
				EntryLocalLocation
			);
			FVector ExitWorldLocation = SoccerField->PitchLocalToWorld(
				ExitLocalLocation
			);

			EntryWorldLocation.Z = SoccerAICharacter->GetActorLocation().Z;
			ExitWorldLocation.Z = SoccerAICharacter->GetActorLocation().Z;

			EntryWorldLocation = ProjectLocationToNavigation(
				EntryWorldLocation,
				SoccerAICharacter
			);
			ExitWorldLocation = ProjectLocationToNavigation(
				ExitWorldLocation,
				SoccerAICharacter
			);

			HalfTimeFieldTransitionEntryTargets.Add(
				SoccerAICharacter,
				EntryWorldLocation
			);
			HalfTimeFieldTransitionExitTargets.Add(
				SoccerAICharacter,
				ExitWorldLocation
			);

			const FVector CurrentLocalLocation =
				SoccerField->WorldToPitchLocal(
					SoccerAICharacter->GetActorLocation()
				);

			const float DestinationProgress =
				CurrentLocalLocation.X * DestinationHalfSign;

			// A player who happened to finish the half beyond midfield must never be
			// sent backwards merely to touch the entry waypoint.
			if (DestinationProgress >= 0.0f)
			{
				HalfTimeFieldTransitionEntryCompleted.Add(
					SoccerAICharacter
				);
			}

			if (DestinationProgress >= SafeExitDepth)
			{
				HalfTimeFieldTransitionEntryCompleted.Add(
					SoccerAICharacter
				);
				HalfTimeFieldTransitionExitCompleted.Add(
					SoccerAICharacter
				);
			}
		}
	};

	BuildTeamTargets(ESoccerTeam::PlayerTeam, PlayerTeamCharacters);
	BuildTeamTargets(ESoccerTeam::OpponentTeam, OpponentTeamCharacters);

	bHalfTimeFieldTransitionRuntimeActive =
		HalfTimeFieldTransitionExitTargets.Num() > 0;
}

void ASoccerMatchManager::UpdateHalfTimeFieldTransition()
{
	if (!IsHalfTimeFieldTransitionActive())
	{
		return;
	}

	const float AcceptanceRadius = FMath::Max(
		20.0f,
		HalfTimeFieldTransitionAcceptanceRadiusCm
	);

	for (const TPair<ASoccerAICharacter*, FVector>& Pair :
		HalfTimeFieldTransitionExitTargets)
	{
		ASoccerAICharacter* SoccerAICharacter = Pair.Key;
		if (!IsValid(SoccerAICharacter))
		{
			continue;
		}

		if (HalfTimeFieldTransitionExitCompleted.Contains(SoccerAICharacter))
		{
			continue;
		}

		if (!HalfTimeFieldTransitionEntryCompleted.Contains(SoccerAICharacter))
		{
			const FVector* EntryTarget =
				HalfTimeFieldTransitionEntryTargets.Find(SoccerAICharacter);

			if (
				EntryTarget != nullptr &&
				FVector::Dist2D(
					SoccerAICharacter->GetActorLocation(),
					*EntryTarget
				) <= AcceptanceRadius
			)
			{
				HalfTimeFieldTransitionEntryCompleted.Add(SoccerAICharacter);
			}
		}

		if (HalfTimeFieldTransitionEntryCompleted.Contains(SoccerAICharacter))
		{
			if (
				FVector::Dist2D(
					SoccerAICharacter->GetActorLocation(),
					Pair.Value
				) <= AcceptanceRadius
			)
			{
				HalfTimeFieldTransitionExitCompleted.Add(SoccerAICharacter);

				if (ASoccerAIController* SoccerAIController =
					Cast<ASoccerAIController>(SoccerAICharacter->GetController()))
				{
					SoccerAIController->StopMovement();
				}
			}
		}
	}
}

bool ASoccerMatchManager::AreAllHalfTimeFieldTransitionBotsComplete() const
{
	if (!bHalfTimeFieldTransitionRuntimeActive)
	{
		return true;
	}

	for (const TPair<ASoccerAICharacter*, FVector>& Pair :
		HalfTimeFieldTransitionExitTargets)
	{
		if (
			IsValid(Pair.Key) &&
			!HalfTimeFieldTransitionExitCompleted.Contains(Pair.Key)
		)
		{
			return false;
		}
	}

	return true;
}

void ASoccerMatchManager::ResetHalfTimeFieldTransition()
{
	bHalfTimeFieldTransitionRuntimeActive = false;
	HalfTimeFieldTransitionEntryTargets.Empty();
	HalfTimeFieldTransitionExitTargets.Empty();
	HalfTimeFieldTransitionEntryCompleted.Empty();
	HalfTimeFieldTransitionExitCompleted.Empty();
}

bool ASoccerMatchManager::ShouldShowMatchClockHUD() const
{
	return bEnableMatchClock && bShowMatchClockHUD;
}

bool ASoccerMatchManager::IsOpponentCoachAIEnabled() const
{
	return bEnableOpponentCoachAI;
}

ESoccerOpponentCoachMode ASoccerMatchManager::GetOpponentCoachMode() const
{
	return CurrentOpponentCoachMode;
}

FString ASoccerMatchManager::GetOpponentCoachModeDisplayName() const
{
	return GetOpponentCoachModeDisplayNameForMode(CurrentOpponentCoachMode);
}

FString ASoccerMatchManager::GetOpponentCoachModeDisplayNameForMode(
	ESoccerOpponentCoachMode Mode
) const
{
	switch (Mode)
	{
	case ESoccerOpponentCoachMode::ProtectLead:
		return TEXT("Proteger ventaja");
	case ESoccerOpponentCoachMode::LockDown:
		return TEXT("Cerrar partido");
	case ESoccerOpponentCoachMode::ChaseGame:
		return TEXT("Buscar resultado");
	case ESoccerOpponentCoachMode::AllOutAttack:
		return TEXT("Ataque total");
	case ESoccerOpponentCoachMode::Baseline:
	default:
		return TEXT("Plan base");
	}
}

void ASoccerMatchManager::InitializeOpponentCoachAI()
{
	OpponentCoachBaselineFormation = OpponentTeamFormationSystem;
	OpponentCoachBaselineTacticalPlan = OpponentTeamTacticalPlan;
	OpponentCoachBaselineSlotInstructions =
		GetSlotTacticalInstructionsForTeam(ESoccerTeam::OpponentTeam);

	CurrentOpponentCoachMode = ESoccerOpponentCoachMode::Baseline;
	OpponentCoachDecisionAccumulator = 0.0f;
	OpponentCoachLastModeChangeProgress = -1.0f;
	OpponentCoachLastObservedPlayerScore = PlayerTeamScore;
	OpponentCoachLastObservedOpponentScore = OpponentTeamScore;
	OpponentCoachSubstitutionEvaluationAccumulator = 0.0f;
	OpponentCoachLastSubstitutionDecisionProgress = -1.0f;
	OpponentCoachLastSubstitutionDiagnosticProgress = -1.0f;
	LastOpponentCoachRuntimeDecision = FSoccerCoachRuntimeDecision();
	bOpponentCoachInitialized = true;

	const USoccerCoachProfile* CoachProfile = ResolveOpponentCoachProfile();
	if (IsValid(CoachProfile))
	{
		UE_LOG(
			LogTemp,
			Display,
			TEXT("[CoachRuntime] initialized coach=%s baselineFormation=%d reading=%d flexibility=%d risk=%d attackingIntent=%d composure=%d interval=%.2fs minGap=%.3f."),
			*CoachProfile->Identity.CoachId.ToString(),
			static_cast<int32>(OpponentCoachBaselineFormation),
			CoachProfile->Abilities.MatchReading,
			CoachProfile->Abilities.TacticalFlexibility,
			CoachProfile->Philosophy.RiskTolerance,
			CoachProfile->Philosophy.AttackingIntent,
			CoachProfile->Abilities.Composure,
			GetOpponentCoachEffectiveDecisionInterval(),
			GetOpponentCoachEffectiveMinimumChangeGap()
		);
	}
}

bool ASoccerMatchManager::CanOpponentCoachChangePlanNow() const
{
	if (!bOpponentCoachInitialized || !bEnableOpponentCoachAI)
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (World != nullptr && UGameplayStatics::IsGamePaused(World))
	{
		return false;
	}

	if (CurrentMatchPeriod == ESoccerMatchPeriod::FullTime)
	{
		return false;
	}

	if (CurrentMatchPeriod == ESoccerMatchPeriod::HalfTime)
	{
		return true;
	}

	return
		IsMatchPeriodGameplayActive() &&
		MatchPlayState == ESoccerMatchPlayState::Playing &&
		!IsRestartContextActive();
}

USoccerCoachProfile* ASoccerMatchManager::ResolveOpponentCoachProfile() const
{
	if (IsValid(OpponentCoachProfile))
	{
		return OpponentCoachProfile;
	}

	UWorld* World = GetWorld();
	USoccerGameInstance* SoccerGameInstance = World != nullptr
		? Cast<USoccerGameInstance>(World->GetGameInstance())
		: nullptr;
	return IsValid(SoccerGameInstance)
		? SoccerGameInstance->GetCoachProfileForClubId(
			GetClubIdForTeam(ESoccerTeam::OpponentTeam)
		)
		: nullptr;
}

float ASoccerMatchManager::GetOpponentCoachEffectiveDecisionInterval() const
{
	const USoccerCoachProfile* CoachProfile = ResolveOpponentCoachProfile();
	const float MatchReadingAlpha = IsValid(CoachProfile)
		? FMath::Clamp(
			static_cast<float>(CoachProfile->Abilities.MatchReading) / 100.0f,
			0.0f,
			1.0f
		)
		: 0.5f;
	return FMath::Clamp(
		OpponentCoachDecisionIntervalSeconds *
			FMath::Lerp(1.60f, 0.55f, MatchReadingAlpha),
		0.10f,
		5.0f
	);
}

float ASoccerMatchManager::GetOpponentCoachEffectiveMinimumChangeGap() const
{
	const USoccerCoachProfile* CoachProfile = ResolveOpponentCoachProfile();
	const float FlexibilityAlpha = IsValid(CoachProfile)
		? FMath::Clamp(
			static_cast<float>(CoachProfile->Abilities.TacticalFlexibility) / 100.0f,
			0.0f,
			1.0f
		)
		: 0.5f;
	const float ComposureAlpha = IsValid(CoachProfile)
		? FMath::Clamp(
			static_cast<float>(CoachProfile->Abilities.Composure) / 100.0f,
			0.0f,
			1.0f
		)
		: 0.5f;
	return FMath::Clamp(
		OpponentCoachMinimumProgressBetweenChanges *
			FMath::Lerp(1.25f, 0.65f, FlexibilityAlpha) *
			FMath::Lerp(0.75f, 1.20f, ComposureAlpha),
		0.0f,
		0.5f
	);
}

float ASoccerMatchManager::GetOpponentCoachAdjustedModeThreshold(
	ESoccerOpponentCoachMode Mode,
	bool bLargeScoreDifference
) const
{
	float BaseThreshold = 1.0f;
	switch (Mode)
	{
	case ESoccerOpponentCoachMode::ProtectLead:
		BaseThreshold = bLargeScoreDifference
			? OpponentCoachProtectTwoGoalLeadProgress
			: OpponentCoachProtectLeadProgress;
		break;
	case ESoccerOpponentCoachMode::LockDown:
		BaseThreshold = bLargeScoreDifference
			? OpponentCoachLockDownTwoGoalLeadProgress
			: OpponentCoachLockDownProgress;
		break;
	case ESoccerOpponentCoachMode::ChaseGame:
		BaseThreshold = bLargeScoreDifference
			? OpponentCoachChaseTwoGoalDeficitProgress
			: OpponentCoachChaseGameProgress;
		break;
	case ESoccerOpponentCoachMode::AllOutAttack:
		BaseThreshold = bLargeScoreDifference
			? OpponentCoachAllOutAttackTwoGoalDeficitProgress
			: OpponentCoachAllOutAttackProgress;
		break;
	case ESoccerOpponentCoachMode::Baseline:
	default:
		return 1.0f;
	}

	const USoccerCoachProfile* CoachProfile = ResolveOpponentCoachProfile();
	if (!IsValid(CoachProfile))
	{
		return FMath::Clamp(BaseThreshold, 0.0f, 1.0f);
	}

	const float RiskAlpha = FMath::Clamp(
		static_cast<float>(CoachProfile->Philosophy.RiskTolerance) / 100.0f,
		0.0f,
		1.0f
	);
	const float AttackAlpha = FMath::Clamp(
		static_cast<float>(CoachProfile->Philosophy.AttackingIntent) / 100.0f,
		0.0f,
		1.0f
	);
	const float FlexibilityAlpha = FMath::Clamp(
		static_cast<float>(CoachProfile->Abilities.TacticalFlexibility) / 100.0f,
		0.0f,
		1.0f
	);
	const float ComposureAlpha = FMath::Clamp(
		static_cast<float>(CoachProfile->Abilities.Composure) / 100.0f,
		0.0f,
		1.0f
	);

	float Adjustment = 0.0f;
	if (
		Mode == ESoccerOpponentCoachMode::ProtectLead ||
		Mode == ESoccerOpponentCoachMode::LockDown
	)
	{
		const float ConservativeAlpha =
			((1.0f - RiskAlpha) + (1.0f - AttackAlpha)) * 0.5f;
		const float DefensiveSkillAlpha = FMath::Clamp(
			static_cast<float>(CoachProfile->Abilities.DefensiveCoaching) / 100.0f,
			0.0f,
			1.0f
		);
		Adjustment =
			-0.14f * ConservativeAlpha -
			0.05f * FlexibilityAlpha -
			0.03f * DefensiveSkillAlpha +
			0.06f * RiskAlpha;
	}
	else
	{
		const float AggressionAlpha = (RiskAlpha + AttackAlpha) * 0.5f;
		const float OffensiveSkillAlpha = FMath::Clamp(
			static_cast<float>(CoachProfile->Abilities.OffensiveCoaching) / 100.0f,
			0.0f,
			1.0f
		);
		Adjustment =
			-0.14f * AggressionAlpha -
			0.05f * FlexibilityAlpha -
			0.03f * OffensiveSkillAlpha -
			0.04f * (1.0f - ComposureAlpha) +
			0.04f * (1.0f - AggressionAlpha);
	}

	return FMath::Clamp(BaseThreshold + Adjustment, 0.05f, 0.95f);
}

ESoccerOpponentCoachMode ASoccerMatchManager::DetermineDesiredOpponentCoachMode(
	float& OutEffectiveThreshold,
	FString& OutReason
) const
{
	OutEffectiveThreshold = 1.0f;
	OutReason = TEXT("Marcador equilibrado: mantener plan base");
	const int32 ScoreDifference = OpponentTeamScore - PlayerTeamScore;
	const int32 AbsoluteDifference = FMath::Abs(ScoreDifference);
	const float MatchProgress = GetMatchProgress();

	if (ScoreDifference > 0)
	{
		const bool bLargeLead = AbsoluteDifference >= 2;
		const float LockDownThreshold = GetOpponentCoachAdjustedModeThreshold(
			ESoccerOpponentCoachMode::LockDown,
			bLargeLead
		);
		const float ProtectThreshold = GetOpponentCoachAdjustedModeThreshold(
			ESoccerOpponentCoachMode::ProtectLead,
			bLargeLead
		);

		if (MatchProgress >= LockDownThreshold)
		{
			OutEffectiveThreshold = LockDownThreshold;
			OutReason = FString::Printf(
				TEXT("Ventaja de %d: cerrar el partido"),
				AbsoluteDifference
			);
			return ESoccerOpponentCoachMode::LockDown;
		}

		if (MatchProgress >= ProtectThreshold)
		{
			OutEffectiveThreshold = ProtectThreshold;
			OutReason = FString::Printf(
				TEXT("Ventaja de %d: proteger el resultado"),
				AbsoluteDifference
			);
			return ESoccerOpponentCoachMode::ProtectLead;
		}
		OutEffectiveThreshold = ProtectThreshold;
		OutReason = TEXT("Ventaja todavia temprana: sostener plan base");
	}
	else if (ScoreDifference < 0)
	{
		const bool bLargeDeficit = AbsoluteDifference >= 2;
		const float AllOutThreshold = GetOpponentCoachAdjustedModeThreshold(
			ESoccerOpponentCoachMode::AllOutAttack,
			bLargeDeficit
		);
		const float ChaseThreshold = GetOpponentCoachAdjustedModeThreshold(
			ESoccerOpponentCoachMode::ChaseGame,
			bLargeDeficit
		);

		if (MatchProgress >= AllOutThreshold)
		{
			OutEffectiveThreshold = AllOutThreshold;
			OutReason = FString::Printf(
				TEXT("Desventaja de %d: asumir riesgo maximo"),
				AbsoluteDifference
			);
			return ESoccerOpponentCoachMode::AllOutAttack;
		}

		if (MatchProgress >= ChaseThreshold)
		{
			OutEffectiveThreshold = ChaseThreshold;
			OutReason = FString::Printf(
				TEXT("Desventaja de %d: buscar el resultado"),
				AbsoluteDifference
			);
			return ESoccerOpponentCoachMode::ChaseGame;
		}
		OutEffectiveThreshold = ChaseThreshold;
		OutReason = TEXT("Desventaja todavia temprana: sostener plan base");
	}

	return ESoccerOpponentCoachMode::Baseline;
}

void ASoccerMatchManager::UpdateOpponentCoachAI(float DeltaTime)
{
	if (!bOpponentCoachInitialized || !bEnableOpponentCoachAI)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (World != nullptr && UGameplayStatics::IsGamePaused(World))
	{
		return;
	}

	OpponentCoachDecisionAccumulator += FMath::Max(0.0f, DeltaTime);
	const float SafeInterval = GetOpponentCoachEffectiveDecisionInterval();

	if (OpponentCoachDecisionAccumulator < SafeInterval)
	{
		return;
	}

	OpponentCoachDecisionAccumulator = 0.0f;

	if (!CanOpponentCoachChangePlanNow())
	{
		return;
	}

	const bool bScoreChanged =
		PlayerTeamScore != OpponentCoachLastObservedPlayerScore ||
		OpponentTeamScore != OpponentCoachLastObservedOpponentScore;

	OpponentCoachLastObservedPlayerScore = PlayerTeamScore;
	OpponentCoachLastObservedOpponentScore = OpponentTeamScore;

	float EffectiveThreshold = 1.0f;
	FString DecisionReason;
	const ESoccerOpponentCoachMode DesiredMode =
		DetermineDesiredOpponentCoachMode(
			EffectiveThreshold,
			DecisionReason
		);

	if (DesiredMode == CurrentOpponentCoachMode)
	{
		return;
	}

	const float MatchProgress = GetMatchProgress();
	const float SafeMinimumGap =
		GetOpponentCoachEffectiveMinimumChangeGap();

	if (
		!bScoreChanged &&
		OpponentCoachLastModeChangeProgress >= 0.0f &&
		MatchProgress - OpponentCoachLastModeChangeProgress < SafeMinimumGap
		)
	{
		return;
	}

	const ESoccerOpponentCoachMode PreviousMode = CurrentOpponentCoachMode;
	ApplyOpponentCoachMode(DesiredMode);
	OpponentCoachLastModeChangeProgress = MatchProgress;

	const USoccerCoachProfile* CoachProfile = ResolveOpponentCoachProfile();
	LastOpponentCoachRuntimeDecision = FSoccerCoachRuntimeDecision();
	LastOpponentCoachRuntimeDecision.CoachId = IsValid(CoachProfile)
		? CoachProfile->Identity.CoachId
		: NAME_None;
	LastOpponentCoachRuntimeDecision.PreviousMode = PreviousMode;
	LastOpponentCoachRuntimeDecision.NewMode = DesiredMode;
	LastOpponentCoachRuntimeDecision.ScoreDifference =
		OpponentTeamScore - PlayerTeamScore;
	LastOpponentCoachRuntimeDecision.MatchProgress = MatchProgress;
	LastOpponentCoachRuntimeDecision.EffectiveDecisionIntervalSeconds =
		SafeInterval;
	LastOpponentCoachRuntimeDecision.EffectiveChangeThreshold =
		EffectiveThreshold;
	LastOpponentCoachRuntimeDecision.Reason = DecisionReason;
	LastOpponentCoachRuntimeDecision.bValid = true;

	UE_LOG(
		LogTemp,
		Display,
		TEXT("[CoachRuntime] coach=%s mode=%s -> %s scoreDiff=%d progress=%.3f threshold=%.3f interval=%.2fs minGap=%.3f reason='%s'."),
		*LastOpponentCoachRuntimeDecision.CoachId.ToString(),
		*GetOpponentCoachModeDisplayNameForMode(PreviousMode),
		*GetOpponentCoachModeDisplayNameForMode(DesiredMode),
		LastOpponentCoachRuntimeDecision.ScoreDifference,
		MatchProgress,
		EffectiveThreshold,
		SafeInterval,
		SafeMinimumGap,
		*DecisionReason
	);
}

void ASoccerMatchManager::SynchronizeMatchSquadActiveSlotsFromActors(
	ESoccerTeam Team
)
{
	FSoccerMatchSquadState& State = GetMutableMatchSquadState(Team);
	if (!State.bInitialized)
	{
		return;
	}

	const FSoccerFormationDefinition& Formation =
		SoccerFormationLibrary::GetDefinition(GetFormationSystemForTeam(Team));
	TMap<FName, FName> SynchronizedLineup;
	for (const FSoccerFormationSlot& Slot : Formation.Slots)
	{
		ASoccerCharacterBase* Character =
			GetFormationSlotAssignedCharacter(Team, Slot.SlotId);
		if (IsValid(Character) && !Character->GetPlayerProfileId().IsNone())
		{
			SynchronizedLineup.Add(
				Slot.SlotId,
				Character->GetPlayerProfileId()
			);
		}
	}
	if (SynchronizedLineup.Num() == Formation.Slots.Num())
	{
		State.ActivePlayerByFormationSlot = MoveTemp(SynchronizedLineup);
	}
}

void ASoccerMatchManager::DebugForceOpponentCoachSubstitutionDecision()
{
	bForceOpponentCoachSubstitutionEvaluation = true;
	UE_LOG(
		LogTemp,
		Display,
		TEXT("[CoachSubstitutionEvaluation] NumPad 0 requested an immediate real coach evaluation.")
	);
	UpdateOpponentCoachSubstitutionAI(0.0f);
}

void ASoccerMatchManager::UpdateOpponentCoachSubstitutionAI(float DeltaTime)
{
	const bool bForcedDebugEvaluation =
		bForceOpponentCoachSubstitutionEvaluation;
	bForceOpponentCoachSubstitutionEvaluation = false;

	if (
		(
			!bForcedDebugEvaluation &&
			(
				!bEnableOpponentCoachAI ||
				!bEnableOpponentCoachAutomaticSubstitutions
			)
		) ||
		!bOpponentCoachInitialized ||
		!OpponentTeamMatchSquadState.HasPendingEligibleSubstitutes() ||
		HasPendingMatchSubstitution(ESoccerTeam::OpponentTeam) ||
		CurrentMatchPeriod == ESoccerMatchPeriod::FullTime
	)
	{
		if (bForcedDebugEvaluation)
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("[CoachSubstitutionEvaluation] result=WAIT reason='coach/squad unavailable, no bench, limit reached, pending change or full time'.")
			);
		}
		return;
	}

	UWorld* World = GetWorld();
	if (
		!bForcedDebugEvaluation &&
		World != nullptr &&
		UGameplayStatics::IsGamePaused(World)
	)
	{
		return;
	}
	const bool bCanEvaluateNow =
		bForcedDebugEvaluation ||
		CurrentMatchPeriod == ESoccerMatchPeriod::HalfTime ||
		(
			IsMatchPeriodGameplayActive() &&
			MatchPlayState == ESoccerMatchPlayState::Playing &&
			!IsRestartContextActive()
		);
	if (!bCanEvaluateNow)
	{
		return;
	}

	const USoccerCoachProfile* CoachProfile = ResolveOpponentCoachProfile();
	if (!IsValid(CoachProfile))
	{
		if (bForcedDebugEvaluation)
		{
			UE_LOG(LogTemp, Warning, TEXT("[CoachSubstitutionEvaluation] result=WAIT reason='opponent coach profile unavailable'."));
		}
		return;
	}
	const float ReadingAlpha = FMath::Clamp(
		static_cast<float>(CoachProfile->Abilities.MatchReading) / 100.0f,
		0.0f,
		1.0f
	);
	const float EffectiveInterval = FMath::Clamp(
		OpponentCoachSubstitutionEvaluationIntervalSeconds *
			FMath::Lerp(1.40f, 0.70f, ReadingAlpha),
		0.25f,
		10.0f
	);
	OpponentCoachSubstitutionEvaluationAccumulator += FMath::Max(0.0f, DeltaTime);
	if (
		!bForcedDebugEvaluation &&
		OpponentCoachSubstitutionEvaluationAccumulator < EffectiveInterval
	)
	{
		return;
	}
	OpponentCoachSubstitutionEvaluationAccumulator = 0.0f;

	SynchronizeMatchSquadActiveSlotsFromActors(ESoccerTeam::OpponentTeam);
	const FSoccerMatchSquadState& State = OpponentTeamMatchSquadState;
	const float MatchProgress = GetMatchProgress();
	const float TimingAlpha = FMath::Clamp(
		static_cast<float>(CoachProfile->Abilities.SubstitutionTiming) / 100.0f,
		0.0f,
		1.0f
	);
	const float FatigueManagementAlpha = FMath::Clamp(
		static_cast<float>(CoachProfile->Abilities.FatigueManagement) / 100.0f,
		0.0f,
		1.0f
	);
	const float PlayerEvaluationAlpha = FMath::Clamp(
		static_cast<float>(CoachProfile->Abilities.PlayerEvaluation) / 100.0f,
		0.0f,
		1.0f
	);
	const float RiskAlpha = FMath::Clamp(
		static_cast<float>(CoachProfile->Philosophy.RiskTolerance) / 100.0f,
		0.0f,
		1.0f
	);
	const int32 ScoreDifference = OpponentTeamScore - PlayerTeamScore;
	const auto LogWait = [
		this,
		bForcedDebugEvaluation,
		MatchProgress,
		ScoreDifference
	](
		const TCHAR* Reason,
		FName OutgoingId,
		FName IncomingId,
		float EnergyPercent,
		int32 IncomingSuitability,
		float DecisionScore,
		float RequiredScore
	)
	{
		if (
			!bForcedDebugEvaluation &&
			(
				!bLogOpponentCoachSubstitutionEvaluations ||
				(
					OpponentCoachLastSubstitutionDiagnosticProgress >= 0.0f &&
					MatchProgress - OpponentCoachLastSubstitutionDiagnosticProgress < 0.10f
				)
			)
		)
		{
			return;
		}
		OpponentCoachLastSubstitutionDiagnosticProgress = MatchProgress;
		UE_LOG(
			LogTemp,
			Display,
			TEXT("[CoachSubstitutionEvaluation] result=WAIT forced=%s reason='%s' bestOUT=%s bestIN=%s energy=%.2f fit=%d decision=%.1f reference=%.1f progress=%.3f scoreDiff=%d."),
			bForcedDebugEvaluation ? TEXT("YES") : TEXT("NO"),
			Reason,
			*OutgoingId.ToString(),
			*IncomingId.ToString(),
			EnergyPercent,
			IncomingSuitability,
			DecisionScore,
			RequiredScore,
			MatchProgress,
			ScoreDifference
		);
	};

	float EarliestNormalProgress = FMath::Lerp(0.68f, 0.38f, TimingAlpha);
	if (ScoreDifference <= -2)
	{
		EarliestNormalProgress -= FMath::Lerp(0.04f, 0.12f, RiskAlpha);
	}
	EarliestNormalProgress = FMath::Clamp(EarliestNormalProgress, 0.20f, 0.75f);
	const float EffectiveMinimumGap =
		OpponentCoachMinimumProgressBetweenSubstitutions *
		FMath::Lerp(1.25f, 0.70f, TimingAlpha);
	if (
		!bForcedDebugEvaluation &&
		OpponentCoachLastSubstitutionDecisionProgress >= 0.0f &&
		MatchProgress - OpponentCoachLastSubstitutionDecisionProgress <
			EffectiveMinimumGap
	)
	{
		return;
	}

	const ESoccerFormationSystem FormationSystem =
		GetFormationSystemForTeam(ESoccerTeam::OpponentTeam);
	const FSoccerFormationDefinition& Formation =
		SoccerFormationLibrary::GetDefinition(FormationSystem);
	USoccerGameInstance* SoccerGameInstance = World != nullptr
		? Cast<USoccerGameInstance>(World->GetGameInstance())
		: nullptr;
	if (!IsValid(SoccerGameInstance))
	{
		if (bForcedDebugEvaluation)
		{
			UE_LOG(LogTemp, Warning, TEXT("[CoachSubstitutionEvaluation] result=WAIT reason='SoccerGameInstance unavailable'."));
		}
		return;
	}

	float BestDecisionScore = -BIG_NUMBER;
	float BestEnergyPercent = 1.0f;
	int32 BestIncomingSuitability = 0;
	FName BestOutgoingId = NAME_None;
	FName BestIncomingId = NAME_None;
	FName BestSlotId = NAME_None;

	for (const FSoccerFormationSlot& Slot : Formation.Slots)
	{
		if (Slot.PlayerRole == ESoccerPlayerRole::Goalkeeper)
		{
			continue;
		}
		const FName* OutgoingId = State.ActivePlayerByFormationSlot.Find(Slot.SlotId);
		ASoccerAICharacter* ActiveCharacter = Cast<ASoccerAICharacter>(
			GetFormationSlotAssignedCharacter(
				ESoccerTeam::OpponentTeam,
				Slot.SlotId
			)
		);
		if (OutgoingId == nullptr || OutgoingId->IsNone() || !IsValid(ActiveCharacter))
		{
			continue;
		}
		USoccerPlayerProfile* OutgoingProfile =
			SoccerGameInstance->FindPlayerProfileById(*OutgoingId);
		if (!IsValid(OutgoingProfile))
		{
			continue;
		}

		const float EnergyPercent = ActiveCharacter->GetAIPlayerEnergyPercent();
		const FSoccerPlayerSlotSuitability OutgoingSuitability =
			USoccerLineupEvaluationLibrary::EvaluatePlayerForFormationSlot(
				OutgoingProfile,
				FormationSystem,
				Slot.SlotId
			);

		for (const FName IncomingId : State.AvailableBenchPlayerIds)
		{
			USoccerPlayerProfile* IncomingProfile =
				SoccerGameInstance->FindPlayerProfileById(IncomingId);
			if (!IsValid(IncomingProfile))
			{
				continue;
			}
			const FSoccerPlayerSlotSuitability IncomingSuitability =
				USoccerLineupEvaluationLibrary::EvaluatePlayerForFormationSlot(
					IncomingProfile,
					FormationSystem,
					Slot.SlotId
				);
			if (!IncomingSuitability.bValid)
			{
				continue;
			}

			const float PerceivedIncomingFit = FMath::Lerp(
				50.0f,
				static_cast<float>(IncomingSuitability.OverallScore),
				PlayerEvaluationAlpha
			);
			const float PerceivedOutgoingFit = FMath::Lerp(
				50.0f,
				static_cast<float>(OutgoingSuitability.OverallScore),
				PlayerEvaluationAlpha
			);
			const float FatigueNeed = (1.0f - EnergyPercent) * 100.0f;
			const float FatigueWeight =
				FMath::Lerp(0.45f, 0.70f, FatigueManagementAlpha);
			float TacticalUrgency = 0.0f;
			if (ScoreDifference < 0)
			{
				if (Slot.FormationLine == ESoccerFormationLine::Attack)
				{
					TacticalUrgency = 14.0f;
				}
				else if (
					Slot.FormationLine == ESoccerFormationLine::Midfield ||
					Slot.FormationLine == ESoccerFormationLine::AttackingMidfield
				)
				{
					TacticalUrgency = 8.0f;
				}
			}
			else if (
				ScoreDifference > 0 &&
				Slot.FormationLine == ESoccerFormationLine::Defense
			)
			{
				TacticalUrgency = 9.0f;
			}

			const float DecisionScore =
				FatigueNeed * FatigueWeight +
				PerceivedIncomingFit * 0.30f +
				FMath::Max(0.0f, PerceivedIncomingFit - PerceivedOutgoingFit) * 0.15f +
				TacticalUrgency;
			if (DecisionScore > BestDecisionScore)
			{
				BestDecisionScore = DecisionScore;
				BestEnergyPercent = EnergyPercent;
				BestIncomingSuitability = IncomingSuitability.OverallScore;
				BestOutgoingId = *OutgoingId;
				BestIncomingId = IncomingId;
				BestSlotId = Slot.SlotId;
			}
		}
	}

	if (BestOutgoingId.IsNone() || BestIncomingId.IsNone())
	{
		LogWait(
			TEXT("no valid starter/bench pairing"),
			NAME_None,
			NAME_None,
			1.0f,
			0,
			0.0f,
			0.0f
		);
		return;
	}
	const bool bEmergencyFatigue = BestEnergyPercent <= 0.15f;
	if (
		!bForcedDebugEvaluation &&
		!bEmergencyFatigue &&
		MatchProgress < EarliestNormalProgress
	)
	{
		LogWait(
			TEXT("too early for this coach"),
			BestOutgoingId,
			BestIncomingId,
			BestEnergyPercent,
			BestIncomingSuitability,
			BestDecisionScore,
			EarliestNormalProgress
		);
		return;
	}
	const float DecisionThreshold = FMath::Lerp(62.0f, 50.0f, TimingAlpha);
	const bool bDesperateMatchSituation =
		MatchProgress >= 0.75f && ScoreDifference <= -2;
	const int32 MinimumIncomingSuitability =
		(bEmergencyFatigue || bDesperateMatchSituation) ? 20 : 35;
	const float EffectiveDecisionThreshold = FMath::Max(
		35.0f,
		DecisionThreshold - (bDesperateMatchSituation ? 8.0f : 0.0f)
	);
	if (
		BestIncomingSuitability < MinimumIncomingSuitability ||
		(
			!bForcedDebugEvaluation &&
			!bEmergencyFatigue &&
			BestDecisionScore < EffectiveDecisionThreshold
		)
	)
	{
		LogWait(
			BestIncomingSuitability < MinimumIncomingSuitability
				? TEXT("best substitute is not suitable enough for the slot")
				: TEXT("decision score below coach threshold"),
			BestOutgoingId,
			BestIncomingId,
			BestEnergyPercent,
			BestIncomingSuitability,
			BestDecisionScore,
			EffectiveDecisionThreshold
		);
		return;
	}

	if (RequestMatchSubstitution(
		ESoccerTeam::OpponentTeam,
		BestOutgoingId,
		BestIncomingId
	))
	{
		OpponentCoachLastSubstitutionDecisionProgress = MatchProgress;
		UE_LOG(
			LogTemp,
			Display,
			TEXT("[CoachSubstitution] coach=%s slot=%s OUT=%s IN=%s energy=%.2f fit=%d minFit=%d decision=%.1f threshold=%.1f progress=%.3f scoreDiff=%d emergency=%s desperation=%s forced=%s."),
			*CoachProfile->Identity.CoachId.ToString(),
			*BestSlotId.ToString(),
			*BestOutgoingId.ToString(),
			*BestIncomingId.ToString(),
			BestEnergyPercent,
			BestIncomingSuitability,
			MinimumIncomingSuitability,
			BestDecisionScore,
			EffectiveDecisionThreshold,
			MatchProgress,
			ScoreDifference,
			bEmergencyFatigue ? TEXT("YES") : TEXT("NO"),
			bDesperateMatchSituation ? TEXT("YES") : TEXT("NO"),
			bForcedDebugEvaluation ? TEXT("YES") : TEXT("NO")
		);
	}
}

ESoccerFormationSystem ASoccerMatchManager::GetOpponentCoachFormationForMode(
	ESoccerOpponentCoachMode Mode
) const
{
	if (Mode == ESoccerOpponentCoachMode::Baseline)
	{
		return OpponentCoachBaselineFormation;
	}

	float TargetAttackBias = 50.0f;
	switch (Mode)
	{
	case ESoccerOpponentCoachMode::LockDown:
		TargetAttackBias = 5.0f;
		break;
	case ESoccerOpponentCoachMode::ProtectLead:
		TargetAttackBias = 22.0f;
		break;
	case ESoccerOpponentCoachMode::ChaseGame:
		TargetAttackBias = 72.0f;
		break;
	case ESoccerOpponentCoachMode::AllOutAttack:
		TargetAttackBias = 95.0f;
		break;
	case ESoccerOpponentCoachMode::Baseline:
	default:
		break;
	}

	const USoccerCoachProfile* CoachProfile = ResolveOpponentCoachProfile();
	const float FlexibilityAlpha = IsValid(CoachProfile)
		? FMath::Clamp(
			static_cast<float>(CoachProfile->Abilities.TacticalFlexibility) / 100.0f,
			0.0f,
			1.0f
		)
		: 0.5f;
	ESoccerFormationSystem BestFormation = OpponentCoachBaselineFormation;
	float BestScore = -BIG_NUMBER;

	for (const ESoccerFormationSystem CandidateFormation :
		SoccerFormationLibrary::GetAllSystems())
	{
		const float ShapeFit = 100.0f - FMath::Abs(
			CalculateFormationAttackBias(CandidateFormation) - TargetAttackBias
		);
		const float Preference = IsValid(CoachProfile)
			? static_cast<float>(CoachProfile->GetFormationPreference(CandidateFormation))
			: 50.0f;
		const float DeparturePenalty =
			CandidateFormation == OpponentCoachBaselineFormation
				? 0.0f
				: (1.0f - FlexibilityAlpha) * 35.0f;
		const float CandidateScore =
			ShapeFit * (0.70f + 0.20f * FlexibilityAlpha) +
			Preference * (0.30f - 0.20f * FlexibilityAlpha) -
			DeparturePenalty;
		if (
			CandidateScore > BestScore ||
			(
				FMath::IsNearlyEqual(CandidateScore, BestScore) &&
				static_cast<uint8>(CandidateFormation) <
					static_cast<uint8>(BestFormation)
			)
		)
		{
			BestScore = CandidateScore;
			BestFormation = CandidateFormation;
		}
	}

	return BestFormation;
}

float ASoccerMatchManager::CalculateFormationAttackBias(
	ESoccerFormationSystem FormationSystem
) const
{
	const FSoccerFormationDefinition& Formation =
		SoccerFormationLibrary::GetDefinition(FormationSystem);
	float TotalBias = 0.0f;
	int32 OutfieldSlotCount = 0;
	for (const FSoccerFormationSlot& Slot : Formation.Slots)
	{
		float SlotBias = 0.0f;
		switch (Slot.FormationLine)
		{
		case ESoccerFormationLine::Defense:
			SlotBias = 15.0f;
			break;
		case ESoccerFormationLine::DefensiveMidfield:
			SlotBias = 32.0f;
			break;
		case ESoccerFormationLine::Midfield:
			SlotBias = 50.0f;
			break;
		case ESoccerFormationLine::AttackingMidfield:
			SlotBias = 70.0f;
			break;
		case ESoccerFormationLine::Attack:
			SlotBias = 90.0f;
			break;
		case ESoccerFormationLine::Goalkeeper:
		default:
			continue;
		}
		TotalBias += SlotBias;
		++OutfieldSlotCount;
	}
	return OutfieldSlotCount > 0
		? TotalBias / static_cast<float>(OutfieldSlotCount)
		: 50.0f;
}

FSoccerTeamTacticalPlan ASoccerMatchManager::BuildOpponentCoachTacticalPlanForMode(
	ESoccerOpponentCoachMode Mode
) const
{
	if (Mode == ESoccerOpponentCoachMode::Baseline)
	{
		return OpponentCoachBaselineTacticalPlan;
	}

	FSoccerTeamTacticalPlan Plan = OpponentCoachBaselineTacticalPlan;
	const USoccerCoachProfile* CoachProfile = ResolveOpponentCoachProfile();
	const FSoccerCoachPhilosophy* Philosophy = IsValid(CoachProfile)
		? &CoachProfile->Philosophy
		: nullptr;
	const bool bStrongOffensiveAdjustment =
		!IsValid(CoachProfile) ||
		(
			CoachProfile->Abilities.OffensiveCoaching +
			CoachProfile->Abilities.TacticalFlexibility
		) >= 100;
	const bool bStrongDefensiveAdjustment =
		!IsValid(CoachProfile) ||
		(
			CoachProfile->Abilities.DefensiveCoaching +
			CoachProfile->Abilities.TacticalFlexibility
		) >= 100;

	switch (Mode)
	{
	case ESoccerOpponentCoachMode::ProtectLead:
		Plan.BuildUpStyle = Philosophy != nullptr && Philosophy->PossessionPreference >= 55
			? ESoccerBuildUpStyle::ShortPossession
			: Plan.BuildUpStyle;
		Plan.AttackingWidth = Philosophy != nullptr && Philosophy->Compactness >= 55
			? ESoccerAttackingWidth::Narrow
			: Plan.AttackingWidth;
		Plan.AttackingTempo = ESoccerAttackingTempo::Patient;
		Plan.AttackingTransition = Philosophy != nullptr && Philosophy->CounterAttackPreference >= 65
			? ESoccerAttackingTransition::CounterAttack
			: ESoccerAttackingTransition::RetainPossession;
		Plan.DefensiveBlock = bStrongDefensiveAdjustment
			? ESoccerDefensiveBlock::Low
			: ESoccerDefensiveBlock::Medium;
		Plan.PressingIntensity = ESoccerPressingIntensity::Low;
		Plan.MarkingStyle = ESoccerMarkingStyle::Zonal;
		Plan.DefensiveTransition = ESoccerDefensiveTransition::Regroup;
		break;

	case ESoccerOpponentCoachMode::LockDown:
		Plan.BuildUpStyle = Philosophy != nullptr && Philosophy->Directness < 50
			? ESoccerBuildUpStyle::ShortPossession
			: ESoccerBuildUpStyle::Direct;
		Plan.AttackingWidth = ESoccerAttackingWidth::Narrow;
		Plan.AttackingTempo = ESoccerAttackingTempo::Balanced;
		Plan.AttackingTransition = ESoccerAttackingTransition::CounterAttack;
		Plan.DefensiveBlock = ESoccerDefensiveBlock::Low;
		Plan.PressingIntensity = ESoccerPressingIntensity::Low;
		Plan.MarkingStyle = ESoccerMarkingStyle::Zonal;
		Plan.DefensiveTransition = ESoccerDefensiveTransition::Regroup;
		break;

	case ESoccerOpponentCoachMode::ChaseGame:
		if (bStrongOffensiveAdjustment)
		{
			Plan.BuildUpStyle = Philosophy != nullptr && Philosophy->Directness < 45
				? Plan.BuildUpStyle
				: ESoccerBuildUpStyle::Direct;
		}
		Plan.AttackingWidth = Philosophy != nullptr && Philosophy->TeamWidth < 35
			? ESoccerAttackingWidth::Balanced
			: ESoccerAttackingWidth::Wide;
		Plan.AttackingTempo = ESoccerAttackingTempo::Fast;
		Plan.AttackingTransition = Philosophy != nullptr && Philosophy->CounterAttackPreference < 45
			? ESoccerAttackingTransition::Balanced
			: ESoccerAttackingTransition::CounterAttack;
		Plan.DefensiveBlock = Philosophy != nullptr && Philosophy->DefensiveLineHeight < 35
			? ESoccerDefensiveBlock::Medium
			: ESoccerDefensiveBlock::High;
		Plan.PressingIntensity = Philosophy != nullptr && Philosophy->PressingIntensity < 35
			? ESoccerPressingIntensity::Medium
			: ESoccerPressingIntensity::High;
		Plan.MarkingStyle = ESoccerMarkingStyle::Mixed;
		Plan.DefensiveTransition = Plan.PressingIntensity == ESoccerPressingIntensity::High
			? ESoccerDefensiveTransition::CounterPress
			: ESoccerDefensiveTransition::Balanced;
		break;

	case ESoccerOpponentCoachMode::AllOutAttack:
		Plan.BuildUpStyle = Philosophy != nullptr && Philosophy->Directness < 35
			? Plan.BuildUpStyle
			: ESoccerBuildUpStyle::Direct;
		Plan.AttackingWidth = ESoccerAttackingWidth::Wide;
		Plan.AttackingTempo = ESoccerAttackingTempo::Fast;
		Plan.AttackingTransition = ESoccerAttackingTransition::CounterAttack;
		Plan.DefensiveBlock = ESoccerDefensiveBlock::High;
		Plan.PressingIntensity = ESoccerPressingIntensity::High;
		Plan.MarkingStyle = ESoccerMarkingStyle::ManToMan;
		Plan.DefensiveTransition = ESoccerDefensiveTransition::CounterPress;
		break;

	case ESoccerOpponentCoachMode::Baseline:
	default:
		break;
	}

	return Plan;
}

TArray<FSoccerSlotTacticalInstruction>
ASoccerMatchManager::BuildOpponentCoachIndividualInstructionsForMode(
	ESoccerOpponentCoachMode Mode,
	ESoccerFormationSystem FormationSystem
) const
{
	if (Mode == ESoccerOpponentCoachMode::Baseline)
	{
		return OpponentCoachBaselineSlotInstructions;
	}

	TArray<FSoccerSlotTacticalInstruction> Instructions;
	const FSoccerFormationDefinition& Definition =
		SoccerFormationLibrary::GetDefinition(FormationSystem);
	Instructions.Reserve(Definition.Slots.Num());

	for (const FSoccerFormationSlot& FormationSlot : Definition.Slots)
	{
		FSoccerSlotTacticalInstruction Instruction;
		Instruction.SlotId = FormationSlot.SlotId;
		Instruction.MarkingTargetSlotId = NAME_None;

		if (FormationSlot.FormationLine == ESoccerFormationLine::Goalkeeper)
		{
			Instructions.Add(Instruction);
			continue;
		}

		const bool bDefensiveLine =
			FormationSlot.FormationLine == ESoccerFormationLine::Defense ||
			FormationSlot.FormationLine == ESoccerFormationLine::DefensiveMidfield;
		const bool bMidfieldLine =
			FormationSlot.FormationLine == ESoccerFormationLine::Midfield ||
			FormationSlot.FormationLine == ESoccerFormationLine::AttackingMidfield;
		const bool bAttackLine =
			FormationSlot.FormationLine == ESoccerFormationLine::Attack;
		const bool bCentralLane =
			FormationSlot.FormationLane == ESoccerFormationLane::Center ||
			FormationSlot.FormationLane == ESoccerFormationLane::LeftCenter ||
			FormationSlot.FormationLane == ESoccerFormationLane::RightCenter;

		switch (Mode)
		{
		case ESoccerOpponentCoachMode::ProtectLead:
			if (bDefensiveLine)
			{
				Instruction.AttackInstruction =
					ESoccerIndividualAttackInstruction::HoldPosition;
				Instruction.DefensiveInstruction =
					bCentralLane
					? ESoccerIndividualDefensiveInstruction::ProtectCenter
					: ESoccerIndividualDefensiveInstruction::Cover;
			}
			else if (bMidfieldLine)
			{
				Instruction.AttackInstruction =
					ESoccerIndividualAttackInstruction::LinkPlay;
				Instruction.DefensiveInstruction =
					ESoccerIndividualDefensiveInstruction::Cover;
			}
			else if (bAttackLine)
			{
				Instruction.AttackInstruction =
					ESoccerIndividualAttackInstruction::TargetPlayer;
			}
			break;

		case ESoccerOpponentCoachMode::LockDown:
			if (bDefensiveLine || bMidfieldLine)
			{
				Instruction.AttackInstruction =
					ESoccerIndividualAttackInstruction::HoldPosition;
				Instruction.DefensiveInstruction =
					ESoccerIndividualDefensiveInstruction::ProtectCenter;
			}
			else if (bAttackLine)
			{
				Instruction.AttackInstruction =
					ESoccerIndividualAttackInstruction::TargetPlayer;
			}
			break;

		case ESoccerOpponentCoachMode::ChaseGame:
			if (bDefensiveLine)
			{
				Instruction.DefensiveInstruction =
					ESoccerIndividualDefensiveInstruction::Cover;
			}
			else if (bMidfieldLine)
			{
				Instruction.AttackInstruction =
					ESoccerIndividualAttackInstruction::LinkPlay;
				Instruction.DefensiveInstruction =
					ESoccerIndividualDefensiveInstruction::PressBall;
			}
			else if (bAttackLine)
			{
				Instruction.AttackInstruction =
					ESoccerIndividualAttackInstruction::MakeForwardRuns;
			}
			break;

		case ESoccerOpponentCoachMode::AllOutAttack:
			if (bDefensiveLine)
			{
				Instruction.DefensiveInstruction =
					ESoccerIndividualDefensiveInstruction::Cover;
			}
			else if (bMidfieldLine)
			{
				Instruction.AttackInstruction =
					ESoccerIndividualAttackInstruction::LinkPlay;
				Instruction.DefensiveInstruction =
					ESoccerIndividualDefensiveInstruction::PressBall;
			}
			else if (bAttackLine)
			{
				Instruction.AttackInstruction =
					FormationSlot.FormationLane == ESoccerFormationLane::Center
					? ESoccerIndividualAttackInstruction::TargetPlayer
					: ESoccerIndividualAttackInstruction::StayWide;
				Instruction.DefensiveInstruction =
					ESoccerIndividualDefensiveInstruction::PressBall;
			}
			break;

		case ESoccerOpponentCoachMode::Baseline:
		default:
			break;
		}

		Instructions.Add(Instruction);
	}

	return Instructions;
}

void ASoccerMatchManager::ApplyOpponentCoachMode(
	ESoccerOpponentCoachMode NewMode
)
{
	if (!bOpponentCoachInitialized || NewMode == CurrentOpponentCoachMode)
	{
		return;
	}

	const ESoccerFormationSystem DesiredFormation =
		GetOpponentCoachFormationForMode(NewMode);

	if (
		bOpponentCoachControlsFormation &&
		GetFormationSystemForTeam(ESoccerTeam::OpponentTeam) != DesiredFormation
		)
	{
		SetFormationSystemForTeam(
			ESoccerTeam::OpponentTeam,
			DesiredFormation
		);
	}

	if (bOpponentCoachControlsCollectiveTactics)
	{
		SetTacticalPlanForTeam(
			ESoccerTeam::OpponentTeam,
			BuildOpponentCoachTacticalPlanForMode(NewMode)
		);
	}

	if (bOpponentCoachControlsIndividualInstructions)
	{
		const ESoccerFormationSystem ActiveFormation =
			GetFormationSystemForTeam(ESoccerTeam::OpponentTeam);
		const TArray<FSoccerSlotTacticalInstruction> Instructions =
			BuildOpponentCoachIndividualInstructionsForMode(
				NewMode,
				ActiveFormation
			);

		for (const FSoccerSlotTacticalInstruction& Instruction : Instructions)
		{
			SetSlotTacticalInstructionForTeam(
				ESoccerTeam::OpponentTeam,
				Instruction
			);
		}
	}

	CurrentOpponentCoachMode = NewMode;

	UE_LOG(
		LogTemp,
		Log,
		TEXT("Opponent coach -> %s | score %d-%d | progress %.2f"),
		*GetOpponentCoachModeDisplayNameForMode(NewMode),
		OpponentTeamScore,
		PlayerTeamScore,
		GetMatchProgress()
	);
}

ASoccerBall* ASoccerMatchManager::GetSoccerBall() const
{
	return SoccerBall;
}

USkeletalMesh* ASoccerMatchManager::ResolvePlayerBodyVariantMesh(
	FName BodyVariantId
) const
{
	if (BodyVariantId.IsNone())
	{
		return nullptr;
	}

	if (!IsValid(PlayerAppearanceCatalog))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[PlayerAppearance] MatchManager has no PlayerAppearanceCatalog; BodyVariantId '%s' cannot be resolved."),
			*BodyVariantId.ToString()
		);
		return nullptr;
	}

	return PlayerAppearanceCatalog->LoadBodyVariantMesh(BodyVariantId);
}

USoccerClubProfile* ASoccerMatchManager::GetClubProfileForTeam(
	ESoccerTeam Team
) const
{
	return
		Team == ESoccerTeam::PlayerTeam
			? PlayerTeamClubProfile
			: OpponentTeamClubProfile;
}

FName ASoccerMatchManager::GetClubIdForTeam(ESoccerTeam Team) const
{
	const USoccerClubProfile* ClubProfile = GetClubProfileForTeam(Team);
	return IsValid(ClubProfile) && ClubProfile->HasValidClubId()
		? ClubProfile->ClubId
		: NAME_None;
}

void ASoccerMatchManager::SetClubProfileForTeam(
	ESoccerTeam Team,
	USoccerClubProfile* ClubProfile
)
{
	if (Team == ESoccerTeam::PlayerTeam)
	{
		PlayerTeamClubProfile = ClubProfile;
	}
	else
	{
		OpponentTeamClubProfile = ClubProfile;
	}

	UE_LOG(
		LogTemp,
		Display,
		TEXT("[ClubMatch] %s now represents club %s."),
		Team == ESoccerTeam::PlayerTeam ? TEXT("PlayerTeam") : TEXT("OpponentTeam"),
		*GetClubIdForTeam(Team).ToString()
	);
}

bool ASoccerMatchManager::ShouldShowClubSelectionAtMatchStart() const
{
	return bShowClubSelectionAtMatchStart;
}

FSoccerCoachMatchPlan ASoccerMatchManager::GetOpponentTeamCoachPlan() const
{
	return OpponentTeamCoachPlan;
}

FSoccerCoachRuntimeDecision
ASoccerMatchManager::GetLastOpponentCoachRuntimeDecision() const
{
	return LastOpponentCoachRuntimeDecision;
}

FSoccerMatchSquadState ASoccerMatchManager::GetMatchSquadState(
	ESoccerTeam Team
) const
{
	return GetMatchSquadStateRef(Team);
}

FSoccerMatchSquadState& ASoccerMatchManager::GetMutableMatchSquadState(
	ESoccerTeam Team
)
{
	return Team == ESoccerTeam::OpponentTeam
		? OpponentTeamMatchSquadState
		: PlayerTeamMatchSquadState;
}

const FSoccerMatchSquadState& ASoccerMatchManager::GetMatchSquadStateRef(
	ESoccerTeam Team
) const
{
	return Team == ESoccerTeam::OpponentTeam
		? OpponentTeamMatchSquadState
		: PlayerTeamMatchSquadState;
}

void ASoccerMatchManager::InitializeMatchSquadState(
	ESoccerTeam Team,
	FName ClubId,
	const TMap<FName, FName>& StartingLineupBySlot,
	const TArray<FName>& BenchPlayerIds
)
{
	FSoccerMatchSquadState& State = GetMutableMatchSquadState(Team);
	State = FSoccerMatchSquadState();
	State.ClubId = ClubId;
	State.ActivePlayerByFormationSlot = StartingLineupBySlot;
	State.AvailableBenchPlayerIds = BenchPlayerIds;
	State.MaximumSubstitutions = FMath::Max(0, MaximumSubstitutionsPerTeam);
	State.bInitialized = StartingLineupBySlot.Num() == 7;
	if (Team == ESoccerTeam::PlayerTeam)
	{
		DebugSelectedPlayerTeamOutgoingId = NAME_None;
		DebugSelectedPlayerTeamIncomingId = NAME_None;
	}

	PendingMatchSubstitutions.RemoveAll(
		[Team](const FSoccerMatchSubstitutionRequest& Request)
		{
			return Request.Team == Team;
		}
	);

	UE_LOG(
		LogTemp,
		Display,
		TEXT("[MatchSquad] team=%d club=%s initialized=%s starters=%d bench=%d maxSubs=%d."),
		static_cast<int32>(Team),
		*ClubId.ToString(),
		State.bInitialized ? TEXT("YES") : TEXT("NO"),
		State.ActivePlayerByFormationSlot.Num(),
		State.AvailableBenchPlayerIds.Num(),
		State.MaximumSubstitutions
	);
}

bool ASoccerMatchManager::HasPendingMatchSubstitution(ESoccerTeam Team) const
{
	return PendingMatchSubstitutions.ContainsByPredicate(
		[Team](const FSoccerMatchSubstitutionRequest& Request)
		{
			return Request.Team == Team;
		}
	);
}

bool ASoccerMatchManager::GetPendingMatchSubstitution(
	ESoccerTeam Team,
	FSoccerMatchSubstitutionRequest& OutRequest
) const
{
	OutRequest = FSoccerMatchSubstitutionRequest();
	for (const FSoccerMatchSubstitutionRequest& Request : PendingMatchSubstitutions)
	{
		if (Request.Team == Team)
		{
			OutRequest = Request;
			return true;
		}
	}
	return false;
}

bool ASoccerMatchManager::RequestMatchSubstitution(
	ESoccerTeam Team,
	FName OutgoingPlayerId,
	FName IncomingPlayerId
)
{
	SynchronizeMatchSquadActiveSlotsFromActors(Team);
	FSoccerMatchSquadState& State = GetMutableMatchSquadState(Team);
	if (
		!State.bInitialized ||
		OutgoingPlayerId.IsNone() ||
		IncomingPlayerId.IsNone() ||
		OutgoingPlayerId == IncomingPlayerId ||
		State.SubstitutionHistory.Num() >= State.MaximumSubstitutions ||
		!State.AvailableBenchPlayerIds.Contains(IncomingPlayerId) ||
		State.WithdrawnPlayerIds.Contains(IncomingPlayerId) ||
		HasPendingMatchSubstitution(Team)
	)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[Substitution] Rejected team=%d outgoing=%s incoming=%s."),
			static_cast<int32>(Team),
			*OutgoingPlayerId.ToString(),
			*IncomingPlayerId.ToString()
		);
		return false;
	}

	FName FormationSlotId = NAME_None;
	for (const TPair<FName, FName>& Pair : State.ActivePlayerByFormationSlot)
	{
		if (Pair.Value == OutgoingPlayerId)
		{
			FormationSlotId = Pair.Key;
			break;
		}
	}
	if (FormationSlotId.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Substitution] Player %s is not active."), *OutgoingPlayerId.ToString());
		return false;
	}

	FSoccerMatchSubstitutionRequest Request;
	Request.Team = Team;
	Request.OutgoingPlayerId = OutgoingPlayerId;
	Request.IncomingPlayerId = IncomingPlayerId;
	Request.FormationSlotId = FormationSlotId;
	Request.RequestedAtMatchSeconds = TotalMatchElapsedSeconds;
	PendingMatchSubstitutions.Add(Request);

	UE_LOG(
		LogTemp,
		Display,
		TEXT("[Substitution] Queued team=%d slot=%s outgoing=%s incoming=%s."),
		static_cast<int32>(Team),
		*FormationSlotId.ToString(),
		*OutgoingPlayerId.ToString(),
		*IncomingPlayerId.ToString()
	);
	UpdatePendingMatchSubstitutions();
	return true;
}

bool ASoccerMatchManager::CancelPendingMatchSubstitution(ESoccerTeam Team)
{
	const int32 Removed = PendingMatchSubstitutions.RemoveAll(
		[Team](const FSoccerMatchSubstitutionRequest& Request)
		{
			return Request.Team == Team;
		}
	);
	return Removed > 0;
}

void ASoccerMatchManager::ShowDebugPlayerTeamSubstitutionSelection(
	const FString& Prefix
) const
{
	const UWorld* World = GetWorld();
	const USoccerGameInstance* SoccerGameInstance = World != nullptr
		? Cast<USoccerGameInstance>(World->GetGameInstance())
		: nullptr;
	const auto DescribePlayer = [SoccerGameInstance](FName PlayerId)
	{
		if (PlayerId.IsNone())
		{
			return FString(TEXT("sin seleccionar"));
		}
		const USoccerPlayerProfile* Profile = IsValid(SoccerGameInstance)
			? SoccerGameInstance->FindPlayerProfileById(PlayerId)
			: nullptr;
		if (IsValid(Profile) && !Profile->Identity.DisplayName.IsEmpty())
		{
			return FString::Printf(
				TEXT("%s [%s]"),
				*Profile->Identity.DisplayName.ToString(),
				*PlayerId.ToString()
			);
		}
		return PlayerId.ToString();
	};
	const FString OutgoingText =
		DescribePlayer(DebugSelectedPlayerTeamOutgoingId);
	const FString IncomingText =
		DescribePlayer(DebugSelectedPlayerTeamIncomingId);
	const FString Message = FString::Printf(
		TEXT("%s\nCAMBIO HUMANO  |  Sale: %s  |  Entra: %s\nNumPad 1: sale  2: entra  3: confirmar  4: cancelar"),
		*Prefix,
		*OutgoingText,
		*IncomingText
	);
	UE_LOG(LogTemp, Display, TEXT("[SubstitutionDebug] %s OUT=%s IN=%s."), *Prefix, *OutgoingText, *IncomingText);
	if (GEngine != nullptr)
	{
		GEngine->AddOnScreenDebugMessage(-9410, 8.0f, FColor::Cyan, Message);
	}
}

void ASoccerMatchManager::DebugCyclePlayerTeamOutgoingSubstitute()
{
	const FSoccerMatchSquadState& State = PlayerTeamMatchSquadState;
	if (!State.bInitialized)
	{
		ShowDebugPlayerTeamSubstitutionSelection(TEXT("Plantel de partido no inicializado"));
		return;
	}

	const FSoccerFormationDefinition& Formation =
		SoccerFormationLibrary::GetDefinition(PlayerTeamFormationSystem);
	TArray<FName> Candidates;
	for (const FSoccerFormationSlot& Slot : Formation.Slots)
	{
		const FName* PlayerId = State.ActivePlayerByFormationSlot.Find(Slot.SlotId);
		if (PlayerId != nullptr && !PlayerId->IsNone())
		{
			Candidates.Add(*PlayerId);
		}
	}
	if (Candidates.Num() == 0)
	{
		ShowDebugPlayerTeamSubstitutionSelection(TEXT("No hay titulares disponibles"));
		return;
	}

	const int32 CurrentIndex = Candidates.IndexOfByKey(DebugSelectedPlayerTeamOutgoingId);
	DebugSelectedPlayerTeamOutgoingId =
		Candidates[(CurrentIndex + 1) % Candidates.Num()];
	ShowDebugPlayerTeamSubstitutionSelection(TEXT("Titular seleccionado"));
}

void ASoccerMatchManager::DebugCyclePlayerTeamIncomingSubstitute()
{
	const TArray<FName>& Candidates =
		PlayerTeamMatchSquadState.AvailableBenchPlayerIds;
	if (Candidates.Num() == 0)
	{
		ShowDebugPlayerTeamSubstitutionSelection(TEXT("No hay suplentes disponibles"));
		return;
	}

	const int32 CurrentIndex = Candidates.IndexOfByKey(DebugSelectedPlayerTeamIncomingId);
	DebugSelectedPlayerTeamIncomingId =
		Candidates[(CurrentIndex + 1) % Candidates.Num()];
	ShowDebugPlayerTeamSubstitutionSelection(TEXT("Suplente seleccionado"));
}

void ASoccerMatchManager::DebugConfirmPlayerTeamSubstitution()
{
	if (
		DebugSelectedPlayerTeamOutgoingId.IsNone() ||
		DebugSelectedPlayerTeamIncomingId.IsNone()
	)
	{
		ShowDebugPlayerTeamSubstitutionSelection(TEXT("Falta elegir quien sale o quien entra"));
		return;
	}

	if (RequestMatchSubstitution(
		ESoccerTeam::PlayerTeam,
		DebugSelectedPlayerTeamOutgoingId,
		DebugSelectedPlayerTeamIncomingId
	))
	{
		ShowDebugPlayerTeamSubstitutionSelection(
			HasPendingMatchSubstitution(ESoccerTeam::PlayerTeam)
				? TEXT("Cambio confirmado; se ejecutara en una pausa segura")
				: TEXT("Cambio ejecutado")
		);
		DebugSelectedPlayerTeamOutgoingId = NAME_None;
		DebugSelectedPlayerTeamIncomingId = NAME_None;
	}
	else
	{
		ShowDebugPlayerTeamSubstitutionSelection(TEXT("No se pudo solicitar el cambio"));
	}
}

void ASoccerMatchManager::DebugCancelPlayerTeamSubstitution()
{
	const bool bCancelled =
		CancelPendingMatchSubstitution(ESoccerTeam::PlayerTeam);
	DebugSelectedPlayerTeamOutgoingId = NAME_None;
	DebugSelectedPlayerTeamIncomingId = NAME_None;
	ShowDebugPlayerTeamSubstitutionSelection(
		bCancelled
			? TEXT("Solicitud pendiente cancelada")
			: TEXT("Seleccion cancelada")
	);
}

void ASoccerMatchManager::DebugRequestAutomaticSubstitution(ESoccerTeam Team)
{
	const FSoccerMatchSquadState& State = GetMatchSquadStateRef(Team);
	if (!State.bInitialized)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SubstitutionDebug] team=%d has no initialized match squad."), static_cast<int32>(Team));
		return;
	}
	if (State.AvailableBenchPlayerIds.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SubstitutionDebug] team=%d has no available bench players."), static_cast<int32>(Team));
		return;
	}
	if (HasPendingMatchSubstitution(Team))
	{
		UE_LOG(LogTemp, Warning, TEXT("[SubstitutionDebug] team=%d already has a pending substitution."), static_cast<int32>(Team));
		return;
	}

	const ESoccerFormationSystem FormationSystem =
		Team == ESoccerTeam::OpponentTeam
			? OpponentTeamFormationSystem
			: PlayerTeamFormationSystem;
	const FSoccerFormationDefinition& Formation =
		SoccerFormationLibrary::GetDefinition(FormationSystem);

	FName OutgoingPlayerId = NAME_None;
	for (const FSoccerFormationSlot& Slot : Formation.Slots)
	{
		if (Slot.PlayerRole == ESoccerPlayerRole::Goalkeeper)
		{
			continue;
		}
		const FName* ActivePlayerId =
			State.ActivePlayerByFormationSlot.Find(Slot.SlotId);
		if (ActivePlayerId != nullptr && !ActivePlayerId->IsNone())
		{
			OutgoingPlayerId = *ActivePlayerId;
			break;
		}
	}

	if (OutgoingPlayerId.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("[SubstitutionDebug] team=%d has no active field player to replace."), static_cast<int32>(Team));
		return;
	}

	const FName IncomingPlayerId = State.AvailableBenchPlayerIds[0];
	UE_LOG(
		LogTemp,
		Display,
		TEXT("[SubstitutionDebug] key test team=%d selected OUT=%s IN=%s."),
		static_cast<int32>(Team),
		*OutgoingPlayerId.ToString(),
		*IncomingPlayerId.ToString()
	);
	RequestMatchSubstitution(Team, OutgoingPlayerId, IncomingPlayerId);
}

bool ASoccerMatchManager::IsSafeMomentForSubstitution() const
{
	if (CurrentMatchPeriod == ESoccerMatchPeriod::HalfTime)
	{
		return true;
	}

	if (!IsMatchPeriodGameplayActive() || !ActiveMatchState)
	{
		return false;
	}

	switch (ActiveMatchState->GetStateId())
	{
	case ESoccerMatchStateId::BallOutOfPlayDelay:
	case ESoccerMatchStateId::PenaltyFoulDelay:
	case ESoccerMatchStateId::PenaltyConfiguration:
	case ESoccerMatchStateId::PenaltyPreparation:
	case ESoccerMatchStateId::OffsideConfiguration:
	case ESoccerMatchStateId::OffsidePreparation:
	case ESoccerMatchStateId::FaultConfiguration:
	case ESoccerMatchStateId::FaultPreparation:
	case ESoccerMatchStateId::CornerConfiguration:
	case ESoccerMatchStateId::CornerPreparation:
	case ESoccerMatchStateId::GoalKickConfiguration:
	case ESoccerMatchStateId::GoalKickPreparation:
	case ESoccerMatchStateId::ThrowInConfiguration:
	case ESoccerMatchStateId::ThrowInPreparation:
	case ESoccerMatchStateId::KickoffConfiguration:
	case ESoccerMatchStateId::KickoffPreparation:
		return true;

	default:
		return false;
	}
}

void ASoccerMatchManager::UpdatePendingMatchSubstitutions()
{
	if (
		VisualSubstitutionPhase != ESoccerVisualSubstitutionPhase::None ||
		!IsSafeMomentForSubstitution()
	)
	{
		return;
	}

	bool bRebuildKickoff = false;
	for (int32 Index = PendingMatchSubstitutions.Num() - 1; Index >= 0; --Index)
	{
		const bool bExecuted =
			ExecuteMatchSubstitution(PendingMatchSubstitutions[Index]);
		PendingMatchSubstitutions.RemoveAt(Index);
		bRebuildKickoff =
			bRebuildKickoff || (bExecuted && IsKickoffMatchStateActive());

		if (VisualSubstitutionPhase != ESoccerVisualSubstitutionPhase::None)
		{
			break;
		}
	}

	if (
		bRebuildKickoff &&
		VisualSubstitutionPhase == ESoccerVisualSubstitutionPhase::None
	)
	{
		StartKickoff(PendingKickoffTeam);
	}
}

bool ASoccerMatchManager::ExecuteMatchSubstitution(
	const FSoccerMatchSubstitutionRequest& Request
)
{
	if (
		bEnableVisualBotSubstitutions &&
		CurrentMatchPeriod != ESoccerMatchPeriod::HalfTime &&
		TryStartVisualSubstitution(Request)
	)
	{
		return true;
	}

	return ExecuteMatchSubstitutionImmediate(Request);
}

bool ASoccerMatchManager::ExecuteMatchSubstitutionImmediate(
	const FSoccerMatchSubstitutionRequest& Request
)
{
	FSoccerMatchSquadState& State = GetMutableMatchSquadState(Request.Team);
	FName ExecutionSlotId = Request.FormationSlotId;
	const FName* RequestedSlotPlayerId =
		ExecutionSlotId.IsNone()
			? nullptr
			: State.ActivePlayerByFormationSlot.Find(ExecutionSlotId);
	if (
		RequestedSlotPlayerId == nullptr ||
		*RequestedSlotPlayerId != Request.OutgoingPlayerId
	)
	{
		SynchronizeMatchSquadActiveSlotsFromActors(Request.Team);
		ExecutionSlotId = NAME_None;
		for (const TPair<FName, FName>& ActivePair : State.ActivePlayerByFormationSlot)
		{
			if (ActivePair.Value == Request.OutgoingPlayerId)
			{
				ExecutionSlotId = ActivePair.Key;
				break;
			}
		}
	}
	if (
		ExecutionSlotId.IsNone() ||
		!State.AvailableBenchPlayerIds.Contains(Request.IncomingPlayerId)
	)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Substitution] Queued request became invalid and was cancelled."));
		return false;
	}

	ASoccerCharacterBase* MatchCharacter = GetFormationSlotAssignedCharacter(
		Request.Team,
		ExecutionSlotId
	);
	UWorld* World = GetWorld();
	USoccerGameInstance* SoccerGameInstance = World != nullptr
		? Cast<USoccerGameInstance>(World->GetGameInstance())
		: nullptr;
	USoccerPlayerProfile* IncomingProfile = IsValid(SoccerGameInstance)
		? SoccerGameInstance->FindPlayerProfileById(Request.IncomingPlayerId)
		: nullptr;
	if (!IsValid(MatchCharacter) || !IsValid(IncomingProfile))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[Substitution] Cannot materialize incoming player=%s slot=%s."),
			*Request.IncomingPlayerId.ToString(),
			*ExecutionSlotId.ToString()
		);
		return false;
	}

	MatchCharacter->SetPlayerProfileForMatch(IncomingProfile);
	MatchCharacter->ResetRuntimeStateForIncomingSubstitute();
	ApplySelectedClubKitToCharacter(MatchCharacter);

	State.ActivePlayerByFormationSlot.Add(
		ExecutionSlotId,
		Request.IncomingPlayerId
	);
	State.AvailableBenchPlayerIds.RemoveSingle(Request.IncomingPlayerId);
	State.WithdrawnPlayerIds.AddUnique(Request.OutgoingPlayerId);

	FSoccerMatchSubstitutionRecord Record;
	Record.Team = Request.Team;
	Record.OutgoingPlayerId = Request.OutgoingPlayerId;
	Record.IncomingPlayerId = Request.IncomingPlayerId;
	Record.FormationSlotId = ExecutionSlotId;
	Record.ExecutedAtMatchSeconds = TotalMatchElapsedSeconds;
	State.SubstitutionHistory.Add(Record);

	UE_LOG(
		LogTemp,
		Display,
		TEXT("[Substitution] Executed team=%d slot=%s OUT=%s IN=%s used=%d/%d actor=%s."),
		static_cast<int32>(Request.Team),
		*ExecutionSlotId.ToString(),
		*Request.OutgoingPlayerId.ToString(),
		*Request.IncomingPlayerId.ToString(),
		State.SubstitutionHistory.Num(),
		State.MaximumSubstitutions,
		*MatchCharacter->GetName()
	);
	return true;
}

bool ASoccerMatchManager::IsCharacterInVisualSubstitution(
	const ASoccerAICharacter* SoccerAICharacter
) const
{
	return
		VisualSubstitutionPhase != ESoccerVisualSubstitutionPhase::None &&
		IsValid(SoccerAICharacter) &&
		(
			SoccerAICharacter == VisualSubstitutionOutgoingCharacter ||
			SoccerAICharacter == VisualSubstitutionIncomingProxy
		);
}

FVector ASoccerMatchManager::BuildSubstitutionFieldLocation(
	float LocalLongitudinalOffset,
	float OutsideTouchlineDistance,
	float CharacterWorldZ
) const
{
	const float TouchlineSign = SubstitutionBenchTouchlineSign < 0.0f
		? -1.0f
		: 1.0f;
	const FVector LocalLocation(
		SoccerFieldDimensions::HalfwayLineX + LocalLongitudinalOffset,
		TouchlineSign *
			(SoccerFieldDimensions::HalfPitchWidthCm + OutsideTouchlineDistance),
		0.0f
	);

	FVector WorldLocation = IsValid(SoccerField)
		? SoccerField->PitchLocalToWorld(LocalLocation)
		: GetActorLocation() + LocalLocation;
	WorldLocation.Z = CharacterWorldZ;
	return WorldLocation;
}

bool ASoccerMatchManager::TryStartVisualSubstitution(
	const FSoccerMatchSubstitutionRequest& Request
)
{
	if (
		VisualSubstitutionPhase != ESoccerVisualSubstitutionPhase::None ||
		!IsValid(SoccerField)
	)
	{
		return false;
	}

	SynchronizeMatchSquadActiveSlotsFromActors(Request.Team);
	FSoccerMatchSquadState& State = GetMutableMatchSquadState(Request.Team);
	FName ExecutionSlotId = NAME_None;
	for (const TPair<FName, FName>& ActivePair : State.ActivePlayerByFormationSlot)
	{
		if (ActivePair.Value == Request.OutgoingPlayerId)
		{
			ExecutionSlotId = ActivePair.Key;
			break;
		}
	}

	ASoccerCharacterBase* OutgoingCharacter =
		GetFormationSlotAssignedCharacter(Request.Team, ExecutionSlotId);
	UWorld* World = GetWorld();
	USoccerGameInstance* SoccerGameInstance = World != nullptr
		? Cast<USoccerGameInstance>(World->GetGameInstance())
		: nullptr;
	USoccerPlayerProfile* IncomingProfile = IsValid(SoccerGameInstance)
		? SoccerGameInstance->FindPlayerProfileById(Request.IncomingPlayerId)
		: nullptr;

	const FSoccerFormationDefinition& Formation =
		SoccerFormationLibrary::GetDefinition(GetFormationSystemForTeam(Request.Team));
	const FSoccerFormationSlot* FormationSlot =
		FindFormationSlotById(Formation, ExecutionSlotId);

	if (
		ExecutionSlotId.IsNone() ||
		!State.AvailableBenchPlayerIds.Contains(Request.IncomingPlayerId) ||
		!IsValid(OutgoingCharacter) ||
		!IsValid(IncomingProfile) ||
		FormationSlot == nullptr ||
		World == nullptr
	)
	{
		return false;
	}

	ASoccerAICharacter* PresentationTemplate =
		Cast<ASoccerAICharacter>(OutgoingCharacter);
	if (!IsValid(PresentationTemplate))
	{
		for (const FSoccerFormationSlot& CandidateFormationSlot : Formation.Slots)
		{
			if (CandidateFormationSlot.SlotId == ExecutionSlotId)
			{
				continue;
			}

			PresentationTemplate = Cast<ASoccerAICharacter>(
				GetFormationSlotAssignedCharacter(
					Request.Team,
					CandidateFormationSlot.SlotId
				)
			);
			if (IsValid(PresentationTemplate))
			{
				break;
			}
		}
	}
	if (!IsValid(PresentationTemplate))
	{
		return false;
	}

	const float CharacterWorldZ = OutgoingCharacter->GetActorLocation().Z;
	const float TeamLongitudinalSign =
		Request.Team == ESoccerTeam::PlayerTeam ? -1.0f : 1.0f;
	const FVector IncomingWaitingLocation = BuildSubstitutionFieldLocation(
		TeamLongitudinalSign *
			FMath::Max(0.0f, SubstitutionIncomingWaitingLongitudinalOffsetCm),
		FMath::Max(50.0f, SubstitutionOutsideTouchlineDistanceCm),
		CharacterWorldZ
	);
	const FVector OutgoingTargetLocation = BuildSubstitutionFieldLocation(
		0.0f,
		FMath::Max(50.0f, SubstitutionOutsideTouchlineDistanceCm),
		CharacterWorldZ
	);

	FVector IncomingFacingDirection =
		GetFormationSlotWorldLocation(Request.Team, *FormationSlot) -
		IncomingWaitingLocation;
	IncomingFacingDirection.Z = 0.0f;
	const FRotator IncomingRotation = IncomingFacingDirection.IsNearlyZero()
		? OutgoingCharacter->GetActorRotation()
		: IncomingFacingDirection.Rotation();

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ASoccerAICharacter* IncomingProxy = World->SpawnActor<ASoccerAICharacter>(
		PresentationTemplate->GetClass(),
		IncomingWaitingLocation,
		IncomingRotation,
		SpawnParameters
	);
	if (!IsValid(IncomingProxy))
	{
		return false;
	}

	if (AController* PresentationController = IncomingProxy->GetController())
	{
		PresentationController->UnPossess();
		PresentationController->Destroy();
	}
	IncomingProxy->Tags.AddUnique(FName(TEXT("SubstitutionPresentation")));
	IncomingProxy->SetActorEnableCollision(false);
	IncomingProxy->SetPlayerProfileForMatch(IncomingProfile);
	IncomingProxy->ResetRuntimeStateForIncomingSubstitute();
	ApplySelectedClubKitToCharacter(
		IncomingProxy,
		Request.Team,
		FormationSlot->PlayerRole
	);
	IncomingProxy->SetAIChasingBall(false);

	ActiveVisualSubstitutionRequest = Request;
	ActiveVisualSubstitutionRequest.FormationSlotId = ExecutionSlotId;
	VisualSubstitutionOutgoingCharacter = OutgoingCharacter;
	VisualSubstitutionIncomingProxy = IncomingProxy;
	VisualSubstitutionOutgoingTarget = OutgoingTargetLocation;
	VisualSubstitutionIncomingTarget =
		GetFormationSlotWorldLocation(Request.Team, *FormationSlot);
	VisualSubstitutionIncomingTarget.Z = CharacterWorldZ;
	VisualSubstitutionElapsedSeconds = 0.0f;
	VisualSubstitutionPhase = ESoccerVisualSubstitutionPhase::OutgoingLeaving;

	if (ASoccerAICharacter* OutgoingAI =
		Cast<ASoccerAICharacter>(OutgoingCharacter))
	{
		OutgoingAI->ReleaseAIBall(false);
		OutgoingAI->SetAIChasingBall(false);
	}
	else if (AThirdPersonCppCharacter* OutgoingHuman =
		Cast<AThirdPersonCppCharacter>(OutgoingCharacter))
	{
		OutgoingHuman->ReleaseBallForMatchRestart(false);
		OutgoingHuman->ClearBallActionsForMatchRestriction();
		if (APlayerController* PlayerController =
			Cast<APlayerController>(OutgoingHuman->GetController()))
		{
			PlayerController->SetIgnoreMoveInput(true);
			bVisualSubstitutionAppliedHumanMoveInputLock = true;
		}
	}
	ReleaseControlledBallPossession(OutgoingCharacter);

	UE_LOG(
		LogTemp,
		Display,
		TEXT("[SubstitutionVisual] Started team=%d slot=%s OUT=%s IN=%s humanControlTransfer=%s."),
		static_cast<int32>(Request.Team),
		*ExecutionSlotId.ToString(),
		*Request.OutgoingPlayerId.ToString(),
		*Request.IncomingPlayerId.ToString(),
		Cast<AThirdPersonCppCharacter>(OutgoingCharacter) != nullptr
			? TEXT("YES")
			: TEXT("NO")
	);
	return true;
}

bool ASoccerMatchManager::MoveVisualSubstitutionCharacterTowards(
	ASoccerCharacterBase* SoccerCharacter,
	const FVector& TargetLocation,
	float MovementSpeed,
	float AcceptanceRadius,
	float DeltaTime
)
{
	if (!IsValid(SoccerCharacter))
	{
		return true;
	}

	const FVector CurrentLocation = SoccerCharacter->GetActorLocation();
	FVector ToTarget = TargetLocation - CurrentLocation;
	ToTarget.Z = 0.0f;
	const float DistanceToTarget = ToTarget.Size();
	const float SafeAcceptanceRadius = FMath::Max(5.0f, AcceptanceRadius);
	if (DistanceToTarget <= SafeAcceptanceRadius)
	{
		if (ASoccerAICharacter* SoccerAICharacter =
			Cast<ASoccerAICharacter>(SoccerCharacter))
		{
			SoccerAICharacter->ClearScriptedLocomotionVelocity();
		}
		else if (AThirdPersonCppCharacter* HumanCharacter =
			Cast<AThirdPersonCppCharacter>(SoccerCharacter))
		{
			HumanCharacter->ClearThrowInScriptedMovementVelocity();
		}
		return true;
	}

	const FVector MovementDirection = ToTarget.GetSafeNormal();
	const float SafeDeltaTime = FMath::Clamp(DeltaTime, 0.0f, 0.10f);
	const float SafeMovementSpeed = FMath::Max(50.0f, MovementSpeed);
	const float MovementStep = FMath::Min(
		DistanceToTarget,
		SafeMovementSpeed * SafeDeltaTime
	);
	const FVector PreviousLocation = CurrentLocation;
	SoccerCharacter->SetActorLocation(
		CurrentLocation + MovementDirection * MovementStep,
		true,
		nullptr,
		ETeleportType::None
	);

	FVector ActualVelocity = SafeDeltaTime > KINDA_SMALL_NUMBER
		? (SoccerCharacter->GetActorLocation() - PreviousLocation) / SafeDeltaTime
		: FVector::ZeroVector;
	ActualVelocity.Z = 0.0f;
	if (ASoccerAICharacter* SoccerAICharacter =
		Cast<ASoccerAICharacter>(SoccerCharacter))
	{
		SoccerAICharacter->SetScriptedLocomotionVelocity(
			ActualVelocity,
			ESoccerAIMovementMode::Jog,
			ESoccerAIMovementReason::NearbyReposition
		);
	}
	else if (AThirdPersonCppCharacter* HumanCharacter =
		Cast<AThirdPersonCppCharacter>(SoccerCharacter))
	{
		HumanCharacter->SetThrowInScriptedMovementVelocity(ActualVelocity);
	}
	if (!MovementDirection.IsNearlyZero())
	{
		SoccerCharacter->SetActorRotation(MovementDirection.Rotation());
	}

	return FVector::Dist2D(
		SoccerCharacter->GetActorLocation(),
		TargetLocation
	) <= SafeAcceptanceRadius;
}

void ASoccerMatchManager::BeginVisualSubstitutionIncomingEntry()
{
	if (!IsValid(VisualSubstitutionOutgoingCharacter))
	{
		CompleteVisualSubstitution();
		return;
	}

	if (ASoccerAICharacter* OutgoingAI =
		Cast<ASoccerAICharacter>(VisualSubstitutionOutgoingCharacter))
	{
		OutgoingAI->ClearScriptedLocomotionVelocity();
	}
	else if (AThirdPersonCppCharacter* OutgoingHuman =
		Cast<AThirdPersonCppCharacter>(VisualSubstitutionOutgoingCharacter))
	{
		OutgoingHuman->ClearThrowInScriptedMovementVelocity();

		if (IsValid(VisualSubstitutionIncomingProxy))
		{
			USoccerPlayerProfile* IncomingProfile =
				VisualSubstitutionIncomingProxy->GetPlayerProfile();
			const FVector IncomingStartLocation =
				VisualSubstitutionIncomingProxy->GetActorLocation();
			const FRotator IncomingStartRotation =
				VisualSubstitutionIncomingProxy->GetActorRotation();

			VisualSubstitutionIncomingProxy->Destroy();
			VisualSubstitutionIncomingProxy = nullptr;

			OutgoingHuman->SetActorLocationAndRotation(
				IncomingStartLocation,
				IncomingStartRotation,
				false,
				nullptr,
				ETeleportType::TeleportPhysics
			);
			OutgoingHuman->SetPlayerProfileForMatch(IncomingProfile);
			FSoccerFormationSlot HumanFormationSlot;
			const ESoccerPlayerRole IncomingPlayerRole =
				GetAssignedFormationSlotForCharacter(
					OutgoingHuman,
					HumanFormationSlot
				)
					? HumanFormationSlot.PlayerRole
					: OutgoingHuman->GetPlayerRole();
			ApplySelectedClubKitToCharacter(
				OutgoingHuman,
				ActiveVisualSubstitutionRequest.Team,
				IncomingPlayerRole
			);
			bVisualSubstitutionUsesHumanActorForEntry = true;
		}
	}

	VisualSubstitutionOutgoingCharacter->SetActorHiddenInGame(
		!bVisualSubstitutionUsesHumanActorForEntry
	);
	VisualSubstitutionOutgoingCharacter->SetActorEnableCollision(false);
	VisualSubstitutionPhase = ESoccerVisualSubstitutionPhase::IncomingEntering;
	VisualSubstitutionElapsedSeconds = 0.0f;

	UE_LOG(LogTemp, Display, TEXT("[SubstitutionVisual] Outgoing player crossed the touchline; incoming player entering."));
}

void ASoccerMatchManager::UpdateVisualSubstitution(float DeltaTime)
{
	if (VisualSubstitutionPhase == ESoccerVisualSubstitutionPhase::None)
	{
		return;
	}

	VisualSubstitutionElapsedSeconds += FMath::Max(0.0f, DeltaTime);
	const bool bTimedOut =
		VisualSubstitutionElapsedSeconds >=
		FMath::Max(1.0f, SubstitutionVisualSequenceTimeoutSeconds);

	if (VisualSubstitutionPhase == ESoccerVisualSubstitutionPhase::OutgoingLeaving)
	{
		const bool bOutgoingArrived = MoveVisualSubstitutionCharacterTowards(
			VisualSubstitutionOutgoingCharacter,
			VisualSubstitutionOutgoingTarget,
			SubstitutionOutgoingMovementSpeedCmPerSecond,
			SubstitutionMovementAcceptanceRadiusCm,
			DeltaTime
		);
		if (bOutgoingArrived || bTimedOut)
		{
			BeginVisualSubstitutionIncomingEntry();
		}
		return;
	}

	ASoccerCharacterBase* EnteringCharacter =
		bVisualSubstitutionUsesHumanActorForEntry
			? VisualSubstitutionOutgoingCharacter
			: VisualSubstitutionIncomingProxy;
	const bool bIncomingArrived = MoveVisualSubstitutionCharacterTowards(
		EnteringCharacter,
		VisualSubstitutionIncomingTarget,
		SubstitutionIncomingMovementSpeedCmPerSecond,
		SubstitutionMovementAcceptanceRadiusCm,
		DeltaTime
	);
	if (bIncomingArrived || bTimedOut)
	{
		CompleteVisualSubstitution();
	}
}

void ASoccerMatchManager::CompleteVisualSubstitution()
{
	ASoccerCharacterBase* OutgoingCharacter =
		VisualSubstitutionOutgoingCharacter;
	ASoccerAICharacter* IncomingProxy =
		VisualSubstitutionIncomingProxy;
	const FSoccerMatchSubstitutionRequest CompletedRequest =
		ActiveVisualSubstitutionRequest;

	if (IsValid(OutgoingCharacter))
	{
		const FRotator FinalRotation = IsValid(IncomingProxy)
			? IncomingProxy->GetActorRotation()
			: OutgoingCharacter->GetActorRotation();
		OutgoingCharacter->SetActorLocationAndRotation(
			VisualSubstitutionIncomingTarget,
			FinalRotation,
			false,
			nullptr,
			ETeleportType::TeleportPhysics
		);
		OutgoingCharacter->SetActorHiddenInGame(false);
		OutgoingCharacter->SetActorEnableCollision(true);
	}

	if (IsValid(IncomingProxy))
	{
		IncomingProxy->Destroy();
	}
	VisualSubstitutionIncomingProxy = nullptr;

	const bool bExecuted = ExecuteMatchSubstitutionImmediate(CompletedRequest);
	if (bExecuted)
	{
		UE_LOG(
			LogTemp,
			Display,
			TEXT("[SubstitutionVisual] Completed OUT=%s IN=%s."),
			*CompletedRequest.OutgoingPlayerId.ToString(),
			*CompletedRequest.IncomingPlayerId.ToString()
		);
	}
	else
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[SubstitutionVisual] Failed during final materialization OUT=%s IN=%s."),
			*CompletedRequest.OutgoingPlayerId.ToString(),
			*CompletedRequest.IncomingPlayerId.ToString()
		);
	}

	ResetVisualSubstitution(false);
	if (bExecuted && IsKickoffMatchStateActive())
	{
		StartKickoff(PendingKickoffTeam);
	}
}

void ASoccerMatchManager::ResetVisualSubstitution(
	bool bRestoreOutgoingCharacter
)
{
	if (IsValid(VisualSubstitutionIncomingProxy))
	{
		VisualSubstitutionIncomingProxy->Destroy();
	}
	if (IsValid(VisualSubstitutionOutgoingCharacter))
	{
		if (ASoccerAICharacter* OutgoingAI =
			Cast<ASoccerAICharacter>(VisualSubstitutionOutgoingCharacter))
		{
			OutgoingAI->ClearScriptedLocomotionVelocity();
		}
		else if (AThirdPersonCppCharacter* OutgoingHuman =
			Cast<AThirdPersonCppCharacter>(VisualSubstitutionOutgoingCharacter))
		{
			OutgoingHuman->ClearThrowInScriptedMovementVelocity();
		}
		if (bRestoreOutgoingCharacter)
		{
			VisualSubstitutionOutgoingCharacter->SetActorHiddenInGame(false);
			VisualSubstitutionOutgoingCharacter->SetActorEnableCollision(true);
		}
	}
	if (bVisualSubstitutionAppliedHumanMoveInputLock)
	{
		if (UWorld* World = GetWorld())
		{
			if (APlayerController* PlayerController =
				UGameplayStatics::GetPlayerController(World, 0))
			{
				PlayerController->SetIgnoreMoveInput(false);
			}
		}
		bVisualSubstitutionAppliedHumanMoveInputLock = false;
	}

	ActiveVisualSubstitutionRequest = FSoccerMatchSubstitutionRequest();
	VisualSubstitutionPhase = ESoccerVisualSubstitutionPhase::None;
	VisualSubstitutionOutgoingCharacter = nullptr;
	VisualSubstitutionIncomingProxy = nullptr;
	VisualSubstitutionOutgoingTarget = FVector::ZeroVector;
	VisualSubstitutionIncomingTarget = FVector::ZeroVector;
	VisualSubstitutionElapsedSeconds = 0.0f;
	bVisualSubstitutionUsesHumanActorForEntry = false;
}

bool ASoccerMatchManager::MaterializeConfiguredMatchTeams()
{
	UWorld* World = GetWorld();
	USoccerGameInstance* SoccerGameInstance =
		World != nullptr
			? Cast<USoccerGameInstance>(World->GetGameInstance())
			: nullptr;
	if (!IsValid(SoccerGameInstance))
	{
		return false;
	}

	const FSoccerMatchSetup MatchSetup =
		SoccerGameInstance->GetCurrentMatchSetup();
	if (!MatchSetup.HasTwoDifferentClubs())
	{
		UE_LOG(LogTemp, Warning, TEXT("[MatchMaterialization] Match setup has no two valid clubs."));
		return false;
	}

	USoccerSquadCatalog* PlayerSquad =
		SoccerGameInstance->FindSquadCatalogByClubId(
			MatchSetup.PlayerTeamClubId
		);
	USoccerSquadCatalog* OpponentSquad =
		SoccerGameInstance->FindSquadCatalogByClubId(
			MatchSetup.OpponentTeamClubId
		);
	if (!IsValid(PlayerSquad) || !IsValid(OpponentSquad))
	{
		UE_LOG(LogTemp, Warning, TEXT("[MatchMaterialization] Could not resolve both selected squad catalogs."));
		return false;
	}

	// The human team keeps the lineup chosen in Director Technical. The rival's
	// independent coach now chooses formation, starters, slots and bench.
	ApplyPersistentDirectorTechnicalSetupToPlayerTeam(true);
	OpponentTeamCoachPlan = FSoccerCoachMatchPlan();
	OpponentCoachProfile =
		SoccerGameInstance->GetCoachProfileForClubId(
			MatchSetup.OpponentTeamClubId
		);
	const bool bCoachPlanBuilt =
		USoccerCoachPlanningLibrary::BuildPreMatchPlan(
			OpponentCoachProfile,
			OpponentSquad,
			OpponentTeamCoachPlan
		);
	const int32 OpponentProfilesApplied = bCoachPlanBuilt
		? ApplyCoachMatchPlanToTeam(
			ESoccerTeam::OpponentTeam,
			OpponentTeamCoachPlan
		)
		: ApplySquadCatalogProfilesToTeam(
			ESoccerTeam::OpponentTeam,
			OpponentSquad
		);
	if (!bCoachPlanBuilt)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[CoachPlan] Rival club '%s' has no valid complete AI plan; legacy automatic selection used."),
			*MatchSetup.OpponentTeamClubId.ToString()
		);
	}

	const int32 PlayerKitsApplied =
		ApplySelectedClubKitsToTeam(ESoccerTeam::PlayerTeam);
	const int32 OpponentKitsApplied =
		ApplySelectedClubKitsToTeam(ESoccerTeam::OpponentTeam);

	if (bCoachPlanBuilt)
	{
		InitializeMatchSquadState(
			ESoccerTeam::OpponentTeam,
			MatchSetup.OpponentTeamClubId,
			OpponentTeamCoachPlan.StartingLineupBySlot,
			OpponentTeamCoachPlan.BenchPlayerIds
		);
	}

	UE_LOG(
		LogTemp,
		Display,
		TEXT("[MatchMaterialization] %s vs %s ready: opponentProfiles=%d, playerKits=%d, opponentKits=%d."),
		*MatchSetup.PlayerTeamClubId.ToString(),
		*MatchSetup.OpponentTeamClubId.ToString(),
		OpponentProfilesApplied,
		PlayerKitsApplied,
		OpponentKitsApplied
	);
	return OpponentProfilesApplied > 0;
}

int32 ASoccerMatchManager::ApplyCoachMatchPlanToTeam(
	ESoccerTeam Team,
	const FSoccerCoachMatchPlan& CoachPlan
)
{
	if (!CoachPlan.bComplete || Team != ESoccerTeam::OpponentTeam)
	{
		return 0;
	}

	UWorld* World = GetWorld();
	USoccerGameInstance* SoccerGameInstance = World != nullptr
		? Cast<USoccerGameInstance>(World->GetGameInstance())
		: nullptr;
	if (!IsValid(SoccerGameInstance))
	{
		return 0;
	}

	const ESoccerFormationSystem PreviousFormation =
		OpponentTeamFormationSystem;
	OpponentTeamFormationSystem = CoachPlan.FormationSystem;
	OpponentTeamTacticalPlan = CoachPlan.TacticalPlan;
	RebuildFormationAssignmentsForTeam(
		Team,
		PreviousFormation,
		false
	);
	EnsureSlotTacticalInstructionsForTeam(Team);
	InitializeOpponentCoachAI();

	const FSoccerFormationDefinition& Formation =
		SoccerFormationLibrary::GetDefinition(CoachPlan.FormationSystem);
	int32 AppliedCount = 0;
	for (const FSoccerFormationSlot& FormationSlot : Formation.Slots)
	{
		const FName* PlayerId =
			CoachPlan.StartingLineupBySlot.Find(FormationSlot.SlotId);
		ASoccerCharacterBase* MatchCharacter =
			GetFormationSlotAssignedCharacter(Team, FormationSlot.SlotId);
		USoccerPlayerProfile* PlayerProfile = PlayerId != nullptr
			? SoccerGameInstance->FindPlayerProfileById(*PlayerId)
			: nullptr;
		if (!IsValid(MatchCharacter) || !IsValid(PlayerProfile))
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("[CoachPlan] Could not materialize team=%d slot=%s player=%s."),
				static_cast<int32>(Team),
				*FormationSlot.SlotId.ToString(),
				PlayerId != nullptr ? *PlayerId->ToString() : TEXT("None")
			);
			continue;
		}

		MatchCharacter->SetPlayerProfileForMatch(PlayerProfile);
		ApplySelectedClubKitToCharacter(MatchCharacter);
		++AppliedCount;
	}

	if (
		TotalMatchElapsedSeconds <= 0.01f &&
		IsKickoffMatchStateActive()
	)
	{
		StartKickoff(PendingKickoffTeam);
	}

	UE_LOG(
		LogTemp,
		Display,
		TEXT("[CoachPlan] Materialized coach=%s club=%s formation=%d profiles=%d/7 bench=%d."),
		*CoachPlan.CoachId.ToString(),
		*CoachPlan.ClubId.ToString(),
		static_cast<int32>(CoachPlan.FormationSystem),
		AppliedCount,
		CoachPlan.BenchPlayerIds.Num()
	);
	return AppliedCount;
}

int32 ASoccerMatchManager::ApplySquadCatalogProfilesToTeam(
	ESoccerTeam Team,
	USoccerSquadCatalog* SquadCatalog
)
{
	if (!IsValid(SquadCatalog))
	{
		return 0;
	}

	const ESoccerFormationSystem FormationSystem =
		GetFormationSystemForTeam(Team);
	const FSoccerFormationDefinition& Formation =
		SoccerFormationLibrary::GetDefinition(FormationSystem);
	TSet<USoccerPlayerProfile*> UsedProfiles;
	int32 AppliedCount = 0;

	for (const FSoccerFormationSlot& FormationSlot : Formation.Slots)
	{
		USoccerPlayerProfile* BestProfile = nullptr;
		int32 BestScore = MIN_int32;

		for (USoccerPlayerProfile* CandidateProfile : SquadCatalog->PlayerProfiles)
		{
			if (
				!IsValid(CandidateProfile) ||
				UsedProfiles.Contains(CandidateProfile)
			)
			{
				continue;
			}

			const FSoccerPlayerSlotSuitability Suitability =
				USoccerLineupEvaluationLibrary::EvaluatePlayerForFormationSlot(
					CandidateProfile,
					FormationSystem,
					FormationSlot.SlotId
				);
			if (BestProfile == nullptr || Suitability.OverallScore > BestScore)
			{
				BestProfile = CandidateProfile;
				BestScore = Suitability.OverallScore;
			}
		}

		ASoccerCharacterBase* MatchCharacter =
			GetFormationSlotAssignedCharacter(Team, FormationSlot.SlotId);
		if (!IsValid(BestProfile) || !IsValid(MatchCharacter))
		{
			continue;
		}

		MatchCharacter->SetPlayerProfileForMatch(BestProfile);
		UsedProfiles.Add(BestProfile);
		ApplySelectedClubKitToCharacter(MatchCharacter);
		++AppliedCount;

		UE_LOG(
			LogTemp,
			Display,
			TEXT("[MatchRoster] team=%d slot=%s player=%s actor=%s suitability=%d."),
			static_cast<int32>(Team),
			*FormationSlot.SlotId.ToString(),
			*BestProfile->Identity.PlayerId.ToString(),
			*MatchCharacter->GetName(),
			BestScore
		);
	}

	return AppliedCount;
}

int32 ASoccerMatchManager::ApplySelectedClubKitsToTeam(ESoccerTeam Team)
{
	TArray<ASoccerCharacterBase*> TeamPlayers;
	CollectFormationPlayersForTeam(Team, TeamPlayers);
	int32 AppliedCount = 0;
	for (ASoccerCharacterBase* Character : TeamPlayers)
	{
		if (IsValid(Character))
		{
			ApplySelectedClubKitToCharacter(Character);
			++AppliedCount;
		}
	}
	return AppliedCount;
}

void ASoccerMatchManager::ApplySelectedClubKitToCharacter(
	ASoccerCharacterBase* Character
)
{
	if (!IsValid(Character))
	{
		return;
	}

	ApplySelectedClubKitToCharacter(
		Character,
		Character->GetTeam(),
		Character->GetPlayerRole()
	);
}

void ASoccerMatchManager::ApplySelectedClubKitToCharacter(
	ASoccerCharacterBase* Character,
	ESoccerTeam UniformTeam,
	ESoccerPlayerRole UniformPlayerRole
)
{
	if (!IsValid(Character))
	{
		return;
	}

	USoccerClubProfile* ClubProfile =
		GetClubProfileForTeam(UniformTeam);
	if (!IsValid(ClubProfile))
	{
		return;
	}

	ESoccerClubKitType KitType = ESoccerClubKitType::Home;
	if (UniformPlayerRole == ESoccerPlayerRole::Goalkeeper)
	{
		KitType = ESoccerClubKitType::Goalkeeper;
	}
	else
	{
		UWorld* World = GetWorld();
		USoccerGameInstance* SoccerGameInstance =
			World != nullptr
				? Cast<USoccerGameInstance>(World->GetGameInstance())
				: nullptr;
		const bool bPlayerTeamIsHome = IsValid(SoccerGameInstance)
			? SoccerGameInstance->GetCurrentMatchSetup().bPlayerTeamIsHome
			: true;
		const bool bCharacterTeamIsHome =
			UniformTeam == ESoccerTeam::PlayerTeam
				? bPlayerTeamIsHome
				: !bPlayerTeamIsHome;
		KitType = bCharacterTeamIsHome
			? ESoccerClubKitType::Home
			: ESoccerClubKitType::Away;
	}

	const FSoccerClubKitDefinition& Kit = ClubProfile->GetKit(KitType);
	Character->ApplyClubKitMaterials(
		Kit.SocksMaterial.IsNull() ? nullptr : Kit.SocksMaterial.LoadSynchronous(),
		Kit.ShirtMaterial.IsNull() ? nullptr : Kit.ShirtMaterial.LoadSynchronous(),
		Kit.ShortsMaterial.IsNull() ? nullptr : Kit.ShortsMaterial.LoadSynchronous()
	);
}

const ASoccerField* ASoccerMatchManager::GetSoccerField() const
{
	return SoccerField;
}

float ASoccerMatchManager::GetOwnGoalLineSign(ESoccerTeam Team) const
{
	return Team == ESoccerTeam::PlayerTeam
		? PlayerTeamOwnGoalLineSign
		: OpponentTeamOwnGoalLineSign;
}

float ASoccerMatchManager::GetOpponentGoalLineSign(ESoccerTeam Team) const
{
	return -GetOwnGoalLineSign(Team);
}


FVector ASoccerMatchManager::GetTeamRebasedFieldReferenceLocation(
	ESoccerTeam Team,
	const FVector& ReferenceWorldLocation
) const
{
	const float CurrentOwnGoalSign = GetOwnGoalLineSign(Team);
	const float InitialOwnGoalSign =
		Team == ESoccerTeam::PlayerTeam
		? PlayerTeamInitialOwnGoalLineSign
		: OpponentTeamInitialOwnGoalLineSign;

	if (IsValid(SoccerField))
	{
		FVector LocalReference =
			SoccerField->WorldToPitchLocal(ReferenceWorldLocation);

		// HomePositionActor and the other authored tactical references were
		// positioned when the reference pitch was 60x40. Re-express them in
		// current-pitch local coordinates before using them. This is essential
		// for goalkeepers: otherwise moving a goal line outward leaves the old
		// HomePositionActor deep inside the enlarged pitch.
		LocalReference.X *= SoccerFieldDimensions::GetAuthoredLengthScale();
		LocalReference.Y *= SoccerFieldDimensions::GetAuthoredWidthScale();

		if (!FMath::IsNearlyEqual(CurrentOwnGoalSign, InitialOwnGoalSign))
		{
			// A change of ends is a 180-degree rotation around the pitch center.
			// X changes end and Y changes side relative to the team's attack.
			LocalReference.X = -LocalReference.X;
			LocalReference.Y = -LocalReference.Y;
		}

		return SoccerField->PitchLocalToWorld(LocalReference);
	}

	// Without ASoccerField there is no authoritative center/rotation/scale
	// from which an authored field reference can be rebuilt safely.
	return ReferenceWorldLocation;
}

ESoccerPossessionTeam ASoccerMatchManager::GetPossessionTeam() const
{
	return PossessionTeam;
}

ASoccerCharacterBase* ASoccerMatchManager::GetPossessingCharacter() const
{
	return PossessingCharacter;
}

bool ASoccerMatchManager::IsBallSecuredByGoalkeeperHands() const
{
	const ASoccerAICharacter* Goalkeeper =
		Cast<ASoccerAICharacter>(PossessingCharacter);

	return
		IsValid(Goalkeeper) &&
		Goalkeeper->GetPlayerRole() ==
		ESoccerPlayerRole::Goalkeeper &&
		Goalkeeper->IsGoalkeeperHoldingBall();
}

bool ASoccerMatchManager::IsBallProtectedFromCharacter(
	const ASoccerCharacterBase* Character
) const
{
	if (!IsValid(Character))
	{
		return false;
	}

	const ASoccerAICharacter* Goalkeeper =
		Cast<ASoccerAICharacter>(PossessingCharacter);

	if (
		IsValid(Goalkeeper) &&
		IsBallSecuredByGoalkeeperHands()
		)
	{
		// Nobody except the goalkeeper that is physically holding the ball may
		// interact with it. The previous team-only check accidentally allowed a
		// human teammate to take the ball out of his own goalkeeper's hands.
		return Goalkeeper != Character;
	}

	// Durante el armado y la animación del lateral la pelota está
	// fuera de juego y, una vez adjunta, no puede ser disputada.
	if (
		bThrowInExecutionActive &&
		!bThrowInBallReleased &&
		IsValid(ThrowInTakerAI) &&
		PossessingCharacter == ThrowInTakerAI
		)
	{
		return ThrowInTakerAI->GetTeam() != Character->GetTeam();
	}

	return false;
}

bool ASoccerMatchManager::RegisterControlledBallPossession(
	ASoccerCharacterBase* NewPossessingCharacter
)
{
	if (!IsValid(NewPossessingCharacter))
	{
		return false;
	}

	if (!TryRegisterIntentionalBallTouch(NewPossessingCharacter))
	{
		return false;
	}

	PossessingCharacter = NewPossessingCharacter;
	PossessionTeam = ConvertTeamToPossessionTeam(
		NewPossessingCharacter->GetTeam()
	);

	ClearFreeBallChaserMemory();

	// Las órdenes anteriores pueden incluir a un rival presionando
	// la pelota. Se limpian ahora para que deje de perseguir al arquero
	// en el mismo frame en que se confirma la captura.
	ClearAssignedAI();

	return true;
}


void ASoccerMatchManager::RegisterOpenPlayPassIntent(
	ASoccerCharacterBase* Passer,
	ASoccerCharacterBase* IntendedReceiver,
	const FVector& PassTargetLocation
)
{
	if (
		MatchPlayState != ESoccerMatchPlayState::Playing ||
		!IsValid(SoccerBall) ||
		!IsValid(Passer) ||
		!IsValid(IntendedReceiver) ||
		Passer == IntendedReceiver ||
		Passer->GetTeam() != IntendedReceiver->GetTeam() ||
		PassTargetLocation.IsNearlyZero()
		)
	{
		ClearOpenPlayPassIntent();
		return;
	}

	bOpenPlayPassIntentActive = true;
	OpenPlayPasser = Passer;
	OpenPlayIntendedReceiver = IntendedReceiver;
OpenPlayPassIntentStartTime =
		GetWorld() != nullptr
		? GetWorld()->GetTimeSeconds()
		: 0.0f;

	// KickAIBallToTarget ya libero la posesion fisica. El MatchManager puede
	// conservarla hasta el siguiente ciclo tactico de Playing, asi que la soltamos
	// tambien aqui para que la nueva prioridad tactica rija en este frame.
	if (PossessingCharacter == Passer)
	{
		PossessingCharacter = nullptr;
		PossessionTeam = ESoccerPossessionTeam::None;
	}

	ClearAssignedAI();

	if (HasActiveAttack())
	{
		AssignAttackDefenseRoles();
	}
	else
	{
		AssignFreeBallRoles();
	}
}

void ASoccerMatchManager::UpdateOpenPlayPassIntent()
{
	if (!bOpenPlayPassIntentActive)
	{
		return;
	}

	if (!IsOpenPlayPassIntentValid())
	{
		ClearOpenPlayPassIntent();
	}
}

void ASoccerMatchManager::ClearOpenPlayPassIntent()
{
	bOpenPlayPassIntentActive = false;
	OpenPlayPasser = nullptr;
	OpenPlayIntendedReceiver = nullptr;
OpenPlayPassIntentStartTime = -1000.0f;
}

bool ASoccerMatchManager::IsOpenPlayPassIntentValid() const
{
	if (
		!bOpenPlayPassIntentActive ||
		MatchPlayState != ESoccerMatchPlayState::Playing ||
		IsRestartContextActive() ||
		!IsValid(SoccerBall) ||
		!IsValid(OpenPlayPasser) ||
		!IsValid(OpenPlayIntendedReceiver) ||
		OpenPlayPasser == OpenPlayIntendedReceiver ||
		OpenPlayPasser->GetTeam() != OpenPlayIntendedReceiver->GetTeam()
		)
	{
		return false;
	}

	// Una posesion confirmada, de cualquier equipo, termina la fase de pase.
	if (IsValid(PossessingCharacter))
	{
		return false;
	}

	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return false;
	}

	const float ElapsedTime =
		World->GetTimeSeconds() - OpenPlayPassIntentStartTime;

	if (ElapsedTime > FMath::Max(0.1f, OpenPlayPassIntentMaxLifetime))
	{
		return false;
	}

	// Si el pase murio lejos del receptor, deja de tener sentido seguir
	// reservandole la pelota. Desde ese momento vuelve la carrera normal.
	if (
		ElapsedTime >= FMath::Max(0.0f, OpenPlayPassIntentSlowBallGraceTime) &&
		SoccerBall->GetVelocity().Size2D() <=
			FMath::Max(0.0f, OpenPlayPassIntentMinimumBallSpeed) &&
		FVector::Dist2D(
			SoccerBall->GetActorLocation(),
			OpenPlayIntendedReceiver->GetActorLocation()
		) > FMath::Max(0.0f, OpenPlayPassIntentAbandonedDistance)
		)
	{
		return false;
	}

	return true;
}

bool ASoccerMatchManager::HasOpenPlayPassIntentForTeam(
	ESoccerTeam Team
) const
{
	return
		bOpenPlayPassIntentActive &&
		IsValid(OpenPlayPasser) &&
		IsValid(OpenPlayIntendedReceiver) &&
		OpenPlayPasser->GetTeam() == Team &&
		OpenPlayIntendedReceiver->GetTeam() == Team;
}

FSoccerFoulDecision ASoccerMatchManager::SubmitFoulIncident(
    const FSoccerFoulIncident& Incident
)
{
    FSoccerFoulDecision Decision;
    Decision.BenefitedTeam = Incident.VictimTeam;

    if (
        !bEnableTackleFoulEvaluation ||
        Incident.PhysicalActionType != ESoccerPhysicalActionType::SlidingTackle ||
        Incident.InstigatorCharacter == nullptr ||
        Incident.VictimCharacter == nullptr ||
        Incident.InstigatorTeam == Incident.VictimTeam
    )
    {
        return Decision;
    }

    const float RelativeSpeed = FMath::Max(0.0f, Incident.RelativeSpeedCmPerSec);
    const float ContactHeight = FMath::Max(0.0f, Incident.ContactHeightCm);

    const bool bHighContact =
        ContactHeight >= FMath::Max(0.0f, TackleFoulHighContactHeight);
    const bool bVeryHighContact =
        ContactHeight >= FMath::Max(
            TackleFoulHighContactHeight,
            TackleFoulVeryHighContactHeight
        );

    const bool bRecklessSpeed =
        RelativeSpeed >= FMath::Max(0.0f, TackleFoulRecklessRelativeSpeed);
    const bool bExcessiveSpeed =
        RelativeSpeed >= FMath::Max(
            TackleFoulRecklessRelativeSpeed,
            TackleFoulExcessiveRelativeSpeed
        );
    const bool bCarelessSpeed =
        RelativeSpeed >= FMath::Max(0.0f, TackleFoulCarelessRelativeSpeed);

    const bool bValidBallTouch =
        Incident.bBallContactOccurred && Incident.bBallContactApplied;

    const float BallFirstLead =
        Incident.OpponentContactNormalizedTime -
        Incident.BallContactNormalizedTime;

    const bool bCleanBallFirstTiming =
        Incident.ContactOrder == ESoccerTackleContactOrder::BallFirst &&
        bValidBallTouch &&
        BallFirstLead >= FMath::Max(
            0.0f,
            TackleFoulCleanBallFirstLeadNormalized
        );

    bool bFoul = false;
    ESoccerFoulSeverity Severity = ESoccerFoulSeverity::None;
    ESoccerFoulType FoulType = ESoccerFoulType::None;
    FString Reason;

    switch (Incident.ContactOrder)
    {
    case ESoccerTackleContactOrder::OpponentOnly:
    case ESoccerTackleContactOrder::OpponentFirst:
        bFoul = true;
        Severity = ESoccerFoulSeverity::Careless;
        FoulType = ESoccerFoulType::SlidingChallenge;
        Reason = TEXT("rival primero / sin toque valido de pelota");
        break;

    case ESoccerTackleContactOrder::NearlySimultaneous:
        if (
            !bValidBallTouch ||
            Incident.bContactFromBehind ||
            bCarelessSpeed ||
            bHighContact
        )
        {
            bFoul = true;
            Severity = ESoccerFoulSeverity::Careless;
            FoulType = ESoccerFoulType::SlidingChallenge;
            Reason = TEXT("contacto casi simultaneo con riesgo");
        }
        break;

    case ESoccerTackleContactOrder::BallFirst:
        if (
            !bCleanBallFirstTiming ||
            Incident.bContactFromBehind ||
            bHighContact ||
            RelativeSpeed > FMath::Max(
                0.0f,
                TackleFoulBallFirstLegalMaxRelativeSpeed
            )
        )
        {
            bFoul = true;
            Severity = ESoccerFoulSeverity::Careless;
            FoulType = ESoccerFoulType::LateChallenge;
            Reason = TEXT("pelota primero pero contacto posterior peligroso/tardio");
        }
        break;

    default:
        break;
    }

    if (bFoul)
    {
        if (
            bVeryHighContact &&
            (bRecklessSpeed || Incident.bContactFromBehind)
        )
        {
            Severity = ESoccerFoulSeverity::ExcessiveForce;
            FoulType = ESoccerFoulType::DangerousChallenge;
            Reason = TEXT("contacto muy alto con fuerza/direccion peligrosa");
        }
        else if (
            bExcessiveSpeed &&
            (bHighContact || Incident.bContactFromBehind || !bValidBallTouch)
        )
        {
            Severity = ESoccerFoulSeverity::ExcessiveForce;
            FoulType = ESoccerFoulType::DangerousChallenge;
            Reason = TEXT("fuerza excesiva");
        }
        else if (
            bHighContact ||
            bRecklessSpeed ||
            (Incident.bContactFromBehind && bCarelessSpeed)
        )
        {
            Severity = ESoccerFoulSeverity::Reckless;
            FoulType = bHighContact
                ? ESoccerFoulType::DangerousChallenge
                : FoulType;
            Reason = bHighContact
                ? TEXT("pierna/pie alto")
                : Incident.bContactFromBehind
                    ? TEXT("entrada por detras con velocidad")
                    : TEXT("velocidad temeraria");
        }

        Decision.bIsFoul = true;
        Decision.FoulType = FoulType;
        Decision.Severity = Severity;
        Decision.RestartType =
            IsLocationInsidePenaltyAreaForTeam(
                Incident.IncidentLocation,
                Incident.InstigatorTeam
            )
            ? ESoccerFoulRestartType::PenaltyKick
            : ESoccerFoulRestartType::DirectFreeKick;
        Decision.Reason = Reason;
    }
    else
    {
        Decision.Reason = bCleanBallFirstTiming
            ? TEXT("pelota primero, contacto bajo/controlado")
            : TEXT("sin infraccion sancionable");
    }

    const TCHAR* SeverityText = TEXT("NONE");
    switch (Decision.Severity)
    {
    case ESoccerFoulSeverity::Careless:
        SeverityText = TEXT("CARELESS");
        break;
    case ESoccerFoulSeverity::Reckless:
        SeverityText = TEXT("RECKLESS");
        break;
    case ESoccerFoulSeverity::ExcessiveForce:
        SeverityText = TEXT("EXCESSIVE");
        break;
    default:
        break;
    }

    const TCHAR* RestartText = TEXT("NONE");
    if (Decision.RestartType == ESoccerFoulRestartType::DirectFreeKick)
    {
        RestartText = TEXT("DIRECT FK");
    }
    else if (Decision.RestartType == ESoccerFoulRestartType::PenaltyKick)
    {
        RestartText = TEXT("PENALTY");
    }

    ASoccerDebugManager::Message(
        this,
        ESoccerDebugCategory::Tackle,
        FString::Printf(
            TEXT("ARBITRO TACKLE: %s | %s | %s | rel=%.0f h=%.1f behind=%s | %s"),
            Decision.bIsFoul ? TEXT("FALTA") : TEXT("LEGAL"),
            SeverityText,
            RestartText,
            RelativeSpeed,
            ContactHeight,
            Incident.bContactFromBehind ? TEXT("SI") : TEXT("NO"),
            *Decision.Reason
        ),
        Decision.bIsFoul
            ? (Decision.Severity == ESoccerFoulSeverity::ExcessiveForce
                ? FColor::Red
                : FColor::Orange)
            : FColor::Green);

    // A foul that occurred in live play has priority over a boundary restart
    // that may have been detected in the same frame (or a few milliseconds
    // later). Character and MatchManager tick order is not guaranteed, so
    // requiring MatchPlayState == Playing here can incorrectly discard a foul
    // if BallOutOfPlayDelay / throw-in / goal-line restart wins that race.
    const bool bCanFoulRestartTakePriority =
        MatchPlayState == ESoccerMatchPlayState::Playing ||
        MatchPlayState == ESoccerMatchPlayState::BallOutOfPlayDelay ||
        IsThrowInRestartActive() ||
        IsGoalLineRestartActive();

    if (
        Decision.bIsFoul &&
        bEnableFoulMatchStoppage &&
        bCanFoulRestartTakePriority
    )
    {
        if (
            Decision.RestartType == ESoccerFoulRestartType::DirectFreeKick &&
            bEnableDirectFreeKickRestartFromFouls
        )
        {
            StartDirectFreeKickRestart(
                Decision.BenefitedTeam,
                Incident.IncidentLocation
            );
        }
        else if (
            Decision.RestartType == ESoccerFoulRestartType::PenaltyKick
        )
        {
            // Once the referee has classified a live-play foul as a penalty,
            // there is no second per-restart switch that may silently discard it.
            // bEnableFoulMatchStoppage is the single master gameplay gate.
            BeginPenaltyFoulDelay(
                Decision.BenefitedTeam,
                Incident.IncidentLocation
            );
        }
    }
    else if (Decision.bIsFoul && bEnableFoulMatchStoppage)
    {
        // A classified foul must never fail silently. This is especially useful
        // for spotting a same-frame out-of-play/restart race during tackle tests.
        ASoccerDebugManager::Message(
            this,
            ESoccerDebugCategory::Restarts,
            FString::Printf(
                TEXT("FALTA CLASIFICADA SIN REINICIO: state=%d restartActive=%s preemptible=%s"),
                static_cast<int32>(MatchPlayState),
                IsRestartContextActive() ? TEXT("SI") : TEXT("NO"),
                bCanFoulRestartTakePriority ? TEXT("SI") : TEXT("NO")
            ),
            FColor::Red
        );
    }

    return Decision;
}

ESoccerTeamPhase ASoccerMatchManager::GetTeamPhase(ESoccerTeam Team) const
{
	if (!HasActiveAttack())
	{
		return ESoccerTeamPhase::Neutral;
	}

	if (IsTeamCurrentlyAttacking(Team))
	{
		return ESoccerTeamPhase::Attacking;
	}

	return ESoccerTeamPhase::Defending;
}

ESoccerAIOrder ASoccerMatchManager::GetAIOrderForCharacter(
	const ASoccerAICharacter* SoccerAICharacter
) const
{
	if (SoccerAICharacter == nullptr)
	{
		return ESoccerAIOrder::ReturnHome;
	}

	const ESoccerTeam Team =
		SoccerAICharacter->GetTeam();

	const ASoccerAICharacter* TeamPressureAI =
		Team == ESoccerTeam::PlayerTeam
		? PlayerTeamPressureAI
		: OpponentTeamPressureAI;

	const ASoccerAICharacter* TeamSupportAI =
		Team == ESoccerTeam::PlayerTeam
		? PlayerTeamSupportAI
		: OpponentTeamSupportAI;

	const ASoccerAICharacter* TeamCoverAI =
		Team == ESoccerTeam::PlayerTeam
		? PlayerTeamCoverAI
		: OpponentTeamCoverAI;

	const ASoccerAICharacter* TeamDefensiveMarkerAI =
		Team == ESoccerTeam::PlayerTeam
		? PlayerTeamDefensiveMarkerAI
		: OpponentTeamDefensiveMarkerAI;

	if (!HasActiveAttack())
	{
		if (SoccerAICharacter == TeamPressureAI)
		{
			return ESoccerAIOrder::ChaseBall;
		}

		return ESoccerAIOrder::MaintainTeamShape;
	}

	if (IsTeamCurrentlyAttacking(Team))
	{
		const bool bTeamCurrentlyHasPossession =
			PossessionTeam != ESoccerPossessionTeam::None &&
			DoesTeamHavePossession(Team);

		if (!bTeamCurrentlyHasPossession && SoccerAICharacter == TeamPressureAI)
		{
			return ESoccerAIOrder::AttackRecoverBall;
		}

		const ASoccerAICharacter* CompensationAI =
			FindBestAttackCompensationAI(Team);

		if (SoccerAICharacter == CompensationAI)
		{
			return ESoccerAIOrder::AttackCompensateCover;
		}

		const ESoccerPlayerRole PlayerRole =
			SoccerAICharacter->GetPlayerRole();

		ESoccerAIOrder BaseAttackOrder =
			ESoccerAIOrder::MaintainTeamShape;

		if (PlayerRole == ESoccerPlayerRole::Defender)
		{
			BaseAttackOrder = ESoccerAIOrder::AttackRestDefense;
		}
		else if (PlayerRole == ESoccerPlayerRole::Midfielder)
		{
			BaseAttackOrder =
				SoccerAICharacter == TeamSupportAI
				? ESoccerAIOrder::AttackSupportShort
				: ESoccerAIOrder::AttackSupportForward;
		}
		else if (PlayerRole == ESoccerPlayerRole::Forward)
		{
			BaseAttackOrder =
				SoccerAICharacter == TeamSupportAI
				? ESoccerAIOrder::AttackWideSupport
				: ESoccerAIOrder::AttackRunIntoSpace;
		}

		// Recovery and compensation were handled above and remain authoritative.
		// Only the normal attacking role is specialized by the slot instruction.
		return ApplyIndividualAttackInstructionToOrder(
			SoccerAICharacter,
			BaseAttackOrder
		);
	}

	if (IsTeamCurrentlyDefending(Team))
	{
		// Un rival no puede presionar ni disputar una pelota que el
		// arquero ya tiene asegurada con las manos. El presionador
		// vuelve a una posición defensiva y evita correr contra él.
		if (
			SoccerAICharacter == TeamPressureAI &&
			IsBallProtectedFromCharacter(SoccerAICharacter)
			)
		{
			return ESoccerAIOrder::DefendCompactShape;
		}

		if (
			SoccerAICharacter == TeamPressureAI &&
			!ShouldCollectivePressureEngageBall(Team, SoccerAICharacter)
			)
		{
			// Low pressing / regroup keeps the designated pressure player inside
			// the team block until the threat enters the configured engagement range.
			return ESoccerAIOrder::DefendCompactShape;
		}

		if (SoccerAICharacter == TeamPressureAI)
		{
			ASoccerAICharacter* Goalkeeper = nullptr;
			FVector ThreatLocation = FVector::ZeroVector;
			FVector OwnGoalLocation = FVector::ZeroVector;
			FVector AttackDirection = FVector::ZeroVector;
			FVector RightDirection = FVector::ZeroVector;

			// Inside the goal area the goalkeeper receives priority. The
			// normal pressure player stops attacking the ball through the
			// goalkeeper path and becomes an additional covering defender.
			if (
				TryBuildGoalAreaDefenderCoordinationContext(
					SoccerAICharacter,
					Goalkeeper,
					ThreatLocation,
					OwnGoalLocation,
					AttackDirection,
					RightDirection
				)
				)
			{
				return ESoccerAIOrder::DefendCompactShape;
			}

			return ESoccerAIOrder::PressBall;
		}

		if (SoccerAICharacter == TeamSupportAI)
		{
			return ESoccerAIOrder::DefendProtectGoalLane;
		}

		if (SoccerAICharacter == TeamCoverAI)
		{
			return ESoccerAIOrder::DefendCoverCenter;
		}

		// Stage 8: explicit slot-to-slot marking is coordinated only after the
		// pressure, goal-lane and central-cover duties have been reserved.
		if (IsValid(GetExplicitIndividualMarkingTarget(SoccerAICharacter)))
		{
			return ESoccerAIOrder::DefendMarkDangerousReceiver;
		}

		if (IsValid(GetCollectiveManMarkingTarget(SoccerAICharacter)))
		{
			return ESoccerAIOrder::DefendMarkDangerousReceiver;
		}

		if (SoccerAICharacter == TeamDefensiveMarkerAI)
		{
			return ESoccerAIOrder::DefendMarkDangerousReceiver;
		}

		FSoccerSlotTacticalInstruction IndividualInstruction;

		if (
			TryGetOpenPlayIndividualInstructionForCharacter(
				SoccerAICharacter,
				IndividualInstruction
			) &&
			IndividualInstruction.DefensiveInstruction ==
				ESoccerIndividualDefensiveInstruction::HoldPosition
			)
		{
			return ESoccerAIOrder::DefendCompactShape;
		}

		const ESoccerPlayerRole PlayerRole =
			SoccerAICharacter->GetPlayerRole();

		if (
			PlayerRole == ESoccerPlayerRole::Defender ||
			PlayerRole == ESoccerPlayerRole::Midfielder
			)
		{
			return ESoccerAIOrder::DefendCompactShape;
		}

		return ESoccerAIOrder::MaintainTeamShape;
	}

	return ESoccerAIOrder::MaintainTeamShape;
}

FVector ASoccerMatchManager::GetAttackShapeMoveLocation(
	const ASoccerAICharacter* SoccerAICharacter
) const
{
	if (!IsValid(SoccerAICharacter))
	{
		return FVector::ZeroVector;
	}

	const ESoccerAIOrder AttackOrder =
		GetAIOrderForCharacter(SoccerAICharacter);

	const FVector DesiredLocation =
		BuildAttackShapeLocation(
			SoccerAICharacter,
			AttackOrder
		);

	const FVector OffsideSafeLocation =
		ApplyOffsideSafetyToAttackMoveLocation(
			SoccerAICharacter,
			DesiredLocation,
			AttackOrder
		);

	const FVector GoalAreaRespectLocation =
		AdjustAttackMoveLocationForGoalAreaRespect(
			SoccerAICharacter,
			OffsideSafeLocation
		);

	// El ajuste lateral para respetar al arquero no puede crear una
	// posicion adelantada. Aplicamos nuevamente la seguridad de offside.
	return ApplyOffsideSafetyToAttackMoveLocation(
		SoccerAICharacter,
		GoalAreaRespectLocation,
		AttackOrder
	);
}

FVector ASoccerMatchManager::GetAttackReferenceLocation(
	ESoccerTeam AttackingTeam
) const
{
	if (
		IsRestartContextActive() &&
		ActiveRestartTeam == AttackingTeam
		)
	{
		return ActiveRestartLocation;
	}

	if (
		IsValid(PossessingCharacter) &&
		PossessingCharacter->GetTeam() == AttackingTeam
		)
	{
		return PossessingCharacter->GetActorLocation();
	}

	if (
		IsValid(LastTouchCharacter) &&
		LastTouchCharacter->GetTeam() == AttackingTeam
		)
	{
		return LastTouchCharacter->GetActorLocation();
	}

	if (!LastTouchLocation.IsNearlyZero())
	{
		return LastTouchLocation;
	}

	if (IsValid(SoccerBall))
	{
		return SoccerBall->GetActorLocation();
	}

	return FVector::ZeroVector;
}

FVector ASoccerMatchManager::BuildAttackShapeLocation(
	const ASoccerAICharacter* SoccerAICharacter,
	ESoccerAIOrder AttackOrder
) const
{
	if (SoccerAICharacter == nullptr)
	{
		return FVector::ZeroVector;
	}

	const ESoccerTeam Team =
		SoccerAICharacter->GetTeam();

	if (!IsTeamCurrentlyAttacking(Team))
	{
		return BuildDynamicTeamShapeLocation(
			Team,
			SoccerAICharacter
		);
	}

	const FVector OwnGoalLocation =
		GetOwnGoalReferenceLocation(Team);

	const FVector OpponentGoalLocation =
		GetOpponentGoalReferenceLocation(Team);

	FVector AttackDirection =
		OpponentGoalLocation - OwnGoalLocation;

	AttackDirection.Z = 0.0f;

	const float FieldLength =
		AttackDirection.Size();

	if (FieldLength <= 1.0f)
	{
		return SoccerAICharacter->GetActorLocation();
	}

	AttackDirection =
		AttackDirection / FieldLength;

	const FVector RightDirection =
		GetFieldRightDirectionForTeam(Team);

	if (RightDirection.IsNearlyZero())
	{
		return SoccerAICharacter->GetActorLocation();
	}

	const float PitchLengthScale =
		SoccerFieldDimensions::GetAuthoredLengthScale();
	const float PitchWidthScale =
		SoccerFieldDimensions::GetAuthoredWidthScale();

	const FVector AttackReferenceLocation =
		GetAttackReferenceLocation(Team);

	if (AttackReferenceLocation.IsNearlyZero())
	{
		return SoccerAICharacter->GetActorLocation();
	}

	AActor* HomePositionActor =
		SoccerAICharacter->GetHomePositionActor();

	const FVector LegacyHomeLocation =
		HomePositionActor != nullptr
		? GetTeamRebasedFieldReferenceLocation(
			Team,
			HomePositionActor->GetActorLocation()
		)
		: SoccerAICharacter->GetActorLocation();

	FSoccerFormationSlot FormationSlot;
	FVector FormationSlotLocation = FVector::ZeroVector;

	const bool bHasFormationStructure =
		TryGetFormationStructuralReference(
			SoccerAICharacter,
			FormationSlot,
			FormationSlotLocation
		);

	const FVector HomeLocation =
		bHasFormationStructure
		? FormationSlotLocation
		: LegacyHomeLocation;

	const float HomeLateralOffset =
		GetFieldLateralOffsetForTeam(
			HomeLocation,
			Team
		);

	const float CurrentLateralOffset =
		GetFieldLateralOffsetForTeam(
			SoccerAICharacter->GetActorLocation(),
			Team
		);

	const float ReferenceLateralOffset =
		GetFieldLateralOffsetForTeam(
			AttackReferenceLocation,
			Team
		);

	const float ReferenceDepth =
		FVector::DotProduct(
			AttackReferenceLocation - OwnGoalLocation,
			AttackDirection
		);

	const float ReferenceDepthAlpha =
		FMath::Clamp(
			ReferenceDepth / FieldLength,
			0.0f,
			1.0f
		);

	float NaturalSideSign = 0.0f;

	if (HomeLateralOffset > 80.0f)
	{
		NaturalSideSign = 1.0f;
	}
	else if (HomeLateralOffset < -80.0f)
	{
		NaturalSideSign = -1.0f;
	}
	else if (CurrentLateralOffset > 80.0f)
	{
		NaturalSideSign = 1.0f;
	}
	else if (CurrentLateralOffset < -80.0f)
	{
		NaturalSideSign = -1.0f;
	}

	float ReferenceSideSign = 0.0f;

	if (ReferenceLateralOffset > 140.0f)
	{
		ReferenceSideSign = 1.0f;
	}
	else if (ReferenceLateralOffset < -140.0f)
	{
		ReferenceSideSign = -1.0f;
	}

	float LaneSign = NaturalSideSign;

	// Si el ataque viene por un costado, el apoyo corto suele servir más hacia adentro.
	if (AttackOrder == ESoccerAIOrder::AttackSupportShort)
	{
		if (ReferenceSideSign != 0.0f)
		{
			LaneSign = -ReferenceSideSign;
		}
		else if (LaneSign == 0.0f)
		{
			LaneSign = -1.0f;
		}
	}

	// El apoyo adelantado busca un carril distinto del apoyo corto.
	else if (AttackOrder == ESoccerAIOrder::AttackSupportForward)
	{
		if (ReferenceSideSign != 0.0f)
		{
			LaneSign = ReferenceSideSign;
		}
		else if (LaneSign == 0.0f)
		{
			LaneSign = 1.0f;
		}
	}

	// El delantero profundo intenta ocupar medio espacio, no tan abierto.
	else if (AttackOrder == ESoccerAIOrder::AttackRunIntoSpace)
	{
		if (LaneSign == 0.0f)
		{
			LaneSign =
				ReferenceSideSign != 0.0f
				? ReferenceSideSign
				: -1.0f;
		}
	}

	// El apoyo ancho busca estirar la defensa.
	// Si la pelota está en un lado, intenta abrirse al lado contrario.
	else if (AttackOrder == ESoccerAIOrder::AttackWideSupport)
	{
		if (ReferenceSideSign != 0.0f)
		{
			LaneSign = -ReferenceSideSign;
		}
		else if (LaneSign == 0.0f)
		{
			LaneSign = 1.0f;
		}
	}

	if (LaneSign == 0.0f)
	{
		LaneSign = 1.0f;
	}

	float DesiredDepth = ReferenceDepth;
	float DesiredLateral = ReferenceLateralOffset;

	if (AttackOrder == ESoccerAIOrder::AttackSupportShort)
	{
		DesiredDepth =
			ReferenceDepth - AttackShortSupportBackDistance * PitchLengthScale;

		DesiredLateral =
			ReferenceLateralOffset * 0.35f
			+ LaneSign * AttackShortSupportSideOffset * PitchWidthScale;
	}
	else if (AttackOrder == ESoccerAIOrder::AttackSupportForward)
	{
		DesiredDepth =
			ReferenceDepth + AttackForwardSupportDistance * PitchLengthScale;

		DesiredLateral =
			ReferenceLateralOffset * 0.20f
			+ LaneSign * AttackForwardSupportSideOffset * PitchWidthScale;
	}
	else if (AttackOrder == ESoccerAIOrder::AttackRunIntoSpace)
	{
		DesiredDepth =
			ReferenceDepth + AttackRunIntoSpaceDistance * PitchLengthScale;

		DesiredLateral =
			LaneSign * AttackRunIntoSpaceSideOffset * PitchWidthScale;
	}
	else if (AttackOrder == ESoccerAIOrder::AttackWideSupport)
	{
		DesiredDepth =
			ReferenceDepth + AttackWideSupportDistance * PitchLengthScale;

		DesiredLateral =
			LaneSign * AttackWideSupportSideOffset * PitchWidthScale;
	}
	else if (AttackOrder == ESoccerAIOrder::AttackRestDefense)
	{
		const float DesiredDepthAlpha =
			FMath::Clamp(
				ReferenceDepthAlpha - 0.22f,
				0.16f,
				AttackRestDefenseMaxDepthAlpha
			);

		DesiredDepth =
			DesiredDepthAlpha * FieldLength;

		DesiredLateral =
			HomeLateralOffset * AttackShapeHomeLateralKeepAlpha;
	}
	else
	{
		const float HomeDepth =
			FVector::DotProduct(
				HomeLocation - OwnGoalLocation,
				AttackDirection
			);

		DesiredDepth =
			FMath::Lerp(
				HomeDepth,
				ReferenceDepth,
				0.35f
			);

		DesiredLateral =
			HomeLateralOffset;
	}

	// Anti-coreografía ofensiva.
	// Evita que jugadores de distinto rol terminen recorriendo el mismo camino
	// porque todos calculan desde la misma referencia móvil.
	if (
		AttackOrder == ESoccerAIOrder::AttackSupportShort ||
		AttackOrder == ESoccerAIOrder::AttackSupportForward ||
		AttackOrder == ESoccerAIOrder::AttackRunIntoSpace ||
		AttackOrder == ESoccerAIOrder::AttackWideSupport ||
		AttackOrder == ESoccerAIOrder::AttackRestDefense
		)
	{
		float RoleLateralOffset = 0.0f;
		float RoleDepthOffset = 0.0f;

		const float SafeLaneSign =
			LaneSign != 0.0f
			? LaneSign
			: 1.0f;

		float HomeSideSign = 0.0f;

		if (HomeLateralOffset > 80.0f)
		{
			HomeSideSign = 1.0f;
		}
		else if (HomeLateralOffset < -80.0f)
		{
			HomeSideSign = -1.0f;
		}

		const float SafeHomeSideSign =
			HomeSideSign != 0.0f
			? HomeSideSign
			: SafeLaneSign;

		if (AttackOrder == ESoccerAIOrder::AttackRestDefense)
		{
			// Defensor: no debe ir en la misma diagonal que el mediocampista.
			// Queda un poco más atrás y conserva más su carril natural.
			RoleDepthOffset =
				-AttackShapeRoleDepthSeparation * PitchLengthScale;

			RoleLateralOffset =
				SafeHomeSideSign * AttackShapeRoleLateralSeparation * PitchWidthScale;
		}
		else if (AttackOrder == ESoccerAIOrder::AttackSupportShort)
		{
			// Apoyo corto: queda un poco más contenido,
			// no calcado al apoyo adelantado.
			RoleDepthOffset =
				-AttackShapeRoleDepthSeparation * PitchLengthScale * 0.35f;

			RoleLateralOffset =
				-SafeLaneSign * AttackShapeRoleLateralSeparation * PitchWidthScale * 0.35f;
		}
		else if (AttackOrder == ESoccerAIOrder::AttackSupportForward)
		{
			// Apoyo adelantado: se despega del defensor y del apoyo corto.
			RoleDepthOffset =
				AttackShapeRoleDepthSeparation * PitchLengthScale * 0.55f;

			RoleLateralOffset =
				SafeLaneSign * AttackShapeRoleLateralSeparation * PitchWidthScale * 0.45f;
		}
		else if (AttackOrder == ESoccerAIOrder::AttackRunIntoSpace)
		{
			// Delantero profundo: más agresivo en profundidad.
			RoleDepthOffset =
				AttackShapeRoleDepthSeparation * PitchLengthScale;

			RoleLateralOffset =
				SafeLaneSign * AttackShapeRoleLateralSeparation * PitchWidthScale * 0.25f;
		}
		else if (AttackOrder == ESoccerAIOrder::AttackWideSupport)
		{
			// Apoyo ancho: se abre más que el resto.
			RoleDepthOffset =
				AttackShapeRoleDepthSeparation * PitchLengthScale * 0.25f;

			RoleLateralOffset =
				SafeLaneSign * AttackShapeRoleLateralSeparation * PitchWidthScale * 0.75f;
		}

		// Desfase personal estable.
		// No es Random: depende del actor, así no cambia cada Tick.
		const uint32 StableId =
			SoccerAICharacter->GetUniqueID();

		const int32 LateralBucket =
			static_cast<int32>(StableId % 3) - 1;

		const int32 DepthBucket =
			static_cast<int32>((StableId / 3) % 3) - 1;

		float PersonalLateralOffset =
			static_cast<float>(LateralBucket) *
			AttackShapePersonalLateralSpacing * PitchWidthScale;

		float PersonalDepthOffset =
			static_cast<float>(DepthBucket) *
			AttackShapePersonalDepthSpacing * PitchLengthScale;

		// Si justo le tocó bucket 0, igual le damos un pequeño empujón
		// para evitar que quede calcado con otro jugador.
		if (FMath::IsNearlyZero(PersonalLateralOffset))
		{
			PersonalLateralOffset =
				SafeLaneSign *
				AttackShapePersonalLateralSpacing * PitchWidthScale *
				0.5f;
		}

		DesiredDepth +=
			RoleDepthOffset +
			PersonalDepthOffset;

		DesiredLateral +=
			RoleLateralOffset +
			PersonalLateralOffset;
	}
	// Disciplina ofensiva sin pelota.
	// El rol marca una profundidad recomendada.
	// Esto evita que defensores y mediocampistas sin pelota terminen todos
	// atacando como delanteros.
	{
		int32 AdvancedTeammatesCount = 0;

		UWorld* World = GetWorld();

		if (World != nullptr)
		{
			for (TActorIterator<ASoccerCharacterBase> It(World); It; ++It)
			{
				const ASoccerCharacterBase* Teammate = *It;

				if (!IsValid(Teammate))
				{
					continue;
				}

				if (Teammate == SoccerAICharacter)
				{
					continue;
				}

				if (Teammate->GetTeam() != Team)
				{
					continue;
				}

				const float TeammateDepth =
					FVector::DotProduct(
						Teammate->GetActorLocation() - OwnGoalLocation,
						AttackDirection
					);

				const float TeammateDepthAlpha =
					FMath::Clamp(
						TeammateDepth / FieldLength,
						0.0f,
						1.0f
					);

				if (TeammateDepthAlpha >= AttackNoBallCrowdedFinalThirdAlpha)
				{
					AdvancedTeammatesCount++;
				}
			}
		}

		const bool bAttackIsAlreadyCrowdedHigh =
			AdvancedTeammatesCount >= AttackNoBallCrowdedFinalThirdCount;

		const bool bAttackNeedsMorePeopleHigh =
			AdvancedTeammatesCount <= 1;

		const ESoccerPlayerRole PlayerRole =
			SoccerAICharacter->GetPlayerRole();

		float RecommendedMaxDepthAlpha = 1.0f;

		if (PlayerRole == ESoccerPlayerRole::Defender)
		{
			RecommendedMaxDepthAlpha =
				AttackNoBallDefenderMaxDepthAlpha;

			if (bAttackNeedsMorePeopleHigh)
			{
				RecommendedMaxDepthAlpha += 0.05f;
			}

			if (bAttackIsAlreadyCrowdedHigh)
			{
				RecommendedMaxDepthAlpha -= 0.06f;
			}
		}
		else if (PlayerRole == ESoccerPlayerRole::Midfielder)
		{
			RecommendedMaxDepthAlpha =
				AttackNoBallMidfielderMaxDepthAlpha;

			if (bAttackNeedsMorePeopleHigh)
			{
				RecommendedMaxDepthAlpha += 0.05f;
			}

			if (bAttackIsAlreadyCrowdedHigh)
			{
				RecommendedMaxDepthAlpha -= 0.08f;
			}
		}
		else if (PlayerRole == ESoccerPlayerRole::Forward)
		{
			RecommendedMaxDepthAlpha =
				AttackNoBallForwardMaxDepthAlpha;
		}
		else if (PlayerRole == ESoccerPlayerRole::Goalkeeper)
		{
			RecommendedMaxDepthAlpha = 0.22f;
		}

		RecommendedMaxDepthAlpha =
			FMath::Clamp(
				RecommendedMaxDepthAlpha,
				0.10f,
				0.98f
			);

		const float RecommendedMaxDepth =
			RecommendedMaxDepthAlpha * FieldLength;

		if (DesiredDepth > RecommendedMaxDepth)
		{
			DesiredDepth =
				FMath::Lerp(
					DesiredDepth,
					RecommendedMaxDepth,
					AttackNoBallRoleDisciplineStrength
				);
		}
	}

	if (bHasFormationStructure)
	{
		// Tactical attacking orders remain authoritative, but their destination is
		// pulled toward the formation-derived open-play shape. This is what lets a
		// 1-5-1 remain visibly deeper or a 1-3-3 remain visibly higher without
		// changing legacy PlayerRole or replacing support/run behaviours.
		const FVector FormationStructureLocation =
			BuildDynamicTeamShapeLocation(Team, SoccerAICharacter);

		if (!FormationStructureLocation.IsNearlyZero())
		{
			const float FormationStructureDepth =
				FVector::DotProduct(
					FormationStructureLocation - OwnGoalLocation,
					AttackDirection
				);

			const float FormationStructureLateral =
				FVector::DotProduct(
					FormationStructureLocation - OwnGoalLocation,
					RightDirection
				);

			const float FormationInfluence =
				AttackOrder == ESoccerAIOrder::AttackRestDefense
				? FormationAttackRestDefenseInfluence
				: FormationAttackShapeInfluence;

			DesiredDepth =
				FMath::Lerp(
					DesiredDepth,
					FormationStructureDepth,
					FMath::Clamp(FormationInfluence, 0.0f, 1.0f)
				);

			DesiredLateral =
				FMath::Lerp(
					DesiredLateral,
					FormationStructureLateral,
					FMath::Clamp(FormationInfluence, 0.0f, 1.0f)
				);
		}
	}

	if (
		ShouldApplyCollectiveTacticsToOpenPlay(Team) &&
		AttackOrder != ESoccerAIOrder::AttackRestDefense
		)
	{
		const FSoccerTeamTacticalPlan& TacticalPlan =
			GetTacticalPlanForTeamInternal(Team);

		// Width changes separation around the existing tactical destination; it
		// never replaces the formation slot or the support/run order itself.
		DesiredLateral *= GetCollectiveAttackWidthScale(Team);

		if (TacticalPlan.AttackChannel == ESoccerAttackChannel::Center)
		{
			DesiredLateral *= 0.70f;
		}
		else
		{
			DesiredLateral += GetCollectiveAttackChannelLateralShift(Team);
		}

		const bool bForwardProgressOrder =
			AttackOrder == ESoccerAIOrder::AttackSupportForward ||
			AttackOrder == ESoccerAIOrder::AttackRunIntoSpace ||
			AttackOrder == ESoccerAIOrder::AttackWideSupport;

		if (bForwardProgressOrder && DesiredDepth > ReferenceDepth)
		{
			float ProgressScale = 1.0f;

			if (TacticalPlan.BuildUpStyle == ESoccerBuildUpStyle::ShortPossession)
			{
				ProgressScale *= 0.94f;
			}
			else if (TacticalPlan.BuildUpStyle == ESoccerBuildUpStyle::Direct)
			{
				ProgressScale *= 1.07f;
			}

			if (TacticalPlan.AttackingTempo == ESoccerAttackingTempo::Patient)
			{
				ProgressScale *= 0.90f;
			}
			else if (TacticalPlan.AttackingTempo == ESoccerAttackingTempo::Fast)
			{
				ProgressScale *= 1.10f;
			}

			if (IsCollectiveAttackingTransitionActiveForTeam(Team))
			{
				if (TacticalPlan.AttackingTransition == ESoccerAttackingTransition::RetainPossession)
				{
					ProgressScale *= 0.88f;
				}
				else if (TacticalPlan.AttackingTransition == ESoccerAttackingTransition::CounterAttack)
				{
					ProgressScale *= 1.18f;
				}
			}

			DesiredDepth =
				ReferenceDepth +
				(DesiredDepth - ReferenceDepth) * ProgressScale;
		}
	}

	DesiredDepth =
		FMath::Clamp(
			DesiredDepth,
			120.0f,
			FieldLength - 320.0f
		);

	const float ScaledAttackShapeMaxLateralOffset =
		FMath::Min(
			AttackShapeMaxLateralOffset * PitchWidthScale,
			FMath::Max(
				0.0f,
				SoccerFieldDimensions::HalfPitchWidthCm - 120.0f
			)
		);

	DesiredLateral =
		FMath::Clamp(
			DesiredLateral,
			-ScaledAttackShapeMaxLateralOffset,
			ScaledAttackShapeMaxLateralOffset
		);

	FVector DesiredLocation =
		OwnGoalLocation
		+ AttackDirection * DesiredDepth
		+ RightDirection * DesiredLateral;

	DesiredLocation.Z =
		SoccerAICharacter->GetActorLocation().Z;

	DesiredLocation =
		ProjectLocationToNavigation(
			DesiredLocation,
			SoccerAICharacter
		);

	return AdjustAttackMoveLocationUsingSpace(
		SoccerAICharacter,
		DesiredLocation,
		AttackOrder
	);
}

FVector ASoccerMatchManager::GetMaintainTeamShapeMoveLocation(
	const ASoccerAICharacter* SoccerAICharacter
) const
{
	if (SoccerAICharacter == nullptr)
	{
		return FVector::ZeroVector;
	}

	return BuildDynamicTeamShapeLocation(
		SoccerAICharacter->GetTeam(),
		SoccerAICharacter
	);
}

float ASoccerMatchManager::ScoreAttackingSupportCandidate(
	ESoccerTeam AttackingTeam,
	const ASoccerAICharacter* Candidate
) const
{
	if (!IsValid(Candidate))
	{
		return -TNumericLimits<float>::Max();
	}

	if (Candidate->GetTeam() != AttackingTeam)
	{
		return -TNumericLimits<float>::Max();
	}

	if (Candidate->GetPlayerRole() == ESoccerPlayerRole::Goalkeeper)
	{
		return -TNumericLimits<float>::Max();
	}

	if (Candidate == PossessingCharacter)
	{
		return -TNumericLimits<float>::Max();
	}

	ESoccerAIOrder DesiredSupportOrder =
		ESoccerAIOrder::AttackSupportShort;

	const ESoccerPlayerRole PlayerRole =
		Candidate->GetPlayerRole();

	if (PlayerRole == ESoccerPlayerRole::Forward)
	{
		DesiredSupportOrder = ESoccerAIOrder::AttackWideSupport;
	}
	else if (PlayerRole == ESoccerPlayerRole::Defender)
	{
		DesiredSupportOrder = ESoccerAIOrder::AttackRestDefense;
	}

	const FVector DesiredLocation =
		BuildAttackShapeLocation(
			Candidate,
			DesiredSupportOrder
		);

	const float DistanceToDesiredLocation =
		FVector::Dist2D(
			Candidate->GetActorLocation(),
			DesiredLocation
		);

	float Score =
		5000.0f -
		DistanceToDesiredLocation * AttackSupportDistanceWeight;

	const ASoccerAICharacter* CurrentSupportAI =
		AttackingTeam == ESoccerTeam::PlayerTeam
		? PlayerTeamSupportAI
		: OpponentTeamSupportAI;

	if (Candidate == CurrentSupportAI)
	{
		Score += AttackSupportCurrentRoleBonus;
	}

	if (PlayerRole == ESoccerPlayerRole::Midfielder)
	{
		Score += AttackSupportMidfielderBonus;
	}
	else if (PlayerRole == ESoccerPlayerRole::Forward)
	{
		Score += AttackSupportForwardBonus;
	}
	else if (PlayerRole == ESoccerPlayerRole::Defender)
	{
		Score -= AttackSupportDefenderPenalty;
	}

	const ASoccerCharacterBase* CurrentCarrier =
		GetCurrentAttackingBallCarrier(AttackingTeam);

	if (!IsValid(CurrentCarrier))
	{
		CurrentCarrier = PossessingCharacter;
	}

	if (IsValid(CurrentCarrier))
	{
		const float DistanceToCarrier =
			FVector::Dist2D(
				Candidate->GetActorLocation(),
				CurrentCarrier->GetActorLocation()
			);

		if (DistanceToCarrier < AttackSupportMinDistanceToCarrier)
		{
			Score -= AttackSupportTooClosePenalty;
		}
		else if (DistanceToCarrier > AttackSupportMaxDistanceToCarrier)
		{
			Score -= AttackSupportTooFarPenalty;
		}
	}

	FSoccerSlotTacticalInstruction IndividualInstruction;

	if (
		TryGetOpenPlayIndividualInstructionForCharacter(
			Candidate,
			IndividualInstruction
		)
		)
	{
		switch (IndividualInstruction.AttackInstruction)
		{
		case ESoccerIndividualAttackInstruction::LinkPlay:
		case ESoccerIndividualAttackInstruction::ComeShort:
			Score += IndividualPreferredRoleScoreBonus;
			break;

		case ESoccerIndividualAttackInstruction::StayWide:
		case ESoccerIndividualAttackInstruction::TargetPlayer:
			Score += IndividualSecondaryRoleScoreBonus;
			break;

		case ESoccerIndividualAttackInstruction::HoldPosition:
			Score -= IndividualAvoidRoleScorePenalty;
			break;

		default:
			break;
		}
	}

	return Score;
}

ASoccerAICharacter* ASoccerMatchManager::FindBestAttackingSupportAI(
	ESoccerTeam AttackingTeam,
	const TArray<const ASoccerAICharacter*>& ExcludedCharacters
) const
{
	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return nullptr;
	}

	ASoccerAICharacter* BestCharacter = nullptr;
	float BestScore = -TNumericLimits<float>::Max();

	for (TActorIterator<ASoccerAICharacter> It(World); It; ++It)
	{
		ASoccerAICharacter* Candidate = *It;

		if (!IsValid(Candidate))
		{
			continue;
		}

		if (Candidate->GetTeam() != AttackingTeam)
		{
			continue;
		}

		if (ExcludedCharacters.Contains(Candidate))
		{
			continue;
		}

		const float CandidateScore =
			ScoreAttackingSupportCandidate(
				AttackingTeam,
				Candidate
			);

		if (CandidateScore > BestScore)
		{
			BestScore = CandidateScore;
			BestCharacter = Candidate;
		}
	}

	return BestCharacter;
}

float ASoccerMatchManager::ScoreAttackingSecondaryCandidate(
	ESoccerTeam AttackingTeam,
	const ASoccerAICharacter* Candidate,
	const ASoccerAICharacter* SelectedSupportAI
) const
{
	if (!IsValid(Candidate))
	{
		return -TNumericLimits<float>::Max();
	}

	if (Candidate->GetTeam() != AttackingTeam)
	{
		return -TNumericLimits<float>::Max();
	}

	if (Candidate->GetPlayerRole() == ESoccerPlayerRole::Goalkeeper)
	{
		return -TNumericLimits<float>::Max();
	}

	if (Candidate == PossessingCharacter)
	{
		return -TNumericLimits<float>::Max();
	}

	ESoccerAIOrder DesiredSecondaryOrder =
		ESoccerAIOrder::AttackSupportForward;

	const ESoccerPlayerRole PlayerRole =
		Candidate->GetPlayerRole();

	if (PlayerRole == ESoccerPlayerRole::Forward)
	{
		DesiredSecondaryOrder = ESoccerAIOrder::AttackRunIntoSpace;
	}
	else if (PlayerRole == ESoccerPlayerRole::Defender)
	{
		DesiredSecondaryOrder = ESoccerAIOrder::AttackRestDefense;
	}

	const FVector DesiredLocation =
		BuildAttackShapeLocation(
			Candidate,
			DesiredSecondaryOrder
		);

	const float DistanceToDesiredLocation =
		FVector::Dist2D(
			Candidate->GetActorLocation(),
			DesiredLocation
		);

	float Score =
		4500.0f -
		DistanceToDesiredLocation * AttackSecondaryDistanceWeight;

	const ASoccerAICharacter* CurrentSecondaryAI =
		AttackingTeam == ESoccerTeam::PlayerTeam
		? PlayerTeamCoverAI
		: OpponentTeamCoverAI;

	if (Candidate == CurrentSecondaryAI)
	{
		Score += AttackSecondaryCurrentRoleBonus;
	}

	if (PlayerRole == ESoccerPlayerRole::Forward)
	{
		Score += AttackSecondaryForwardBonus;
	}
	else if (PlayerRole == ESoccerPlayerRole::Midfielder)
	{
		Score += AttackSecondaryMidfielderBonus;
	}
	else if (PlayerRole == ESoccerPlayerRole::Defender)
	{
		Score -= AttackSecondaryDefenderPenalty;
	}

	if (IsValid(SelectedSupportAI))
	{
		const float DistanceToSelectedSupport =
			FVector::Dist2D(
				Candidate->GetActorLocation(),
				SelectedSupportAI->GetActorLocation()
			);

		if (DistanceToSelectedSupport < AttackSecondaryMinDistanceFromSupport)
		{
			Score -= AttackSecondaryTooCloseToSupportPenalty;
		}
	}

	FSoccerSlotTacticalInstruction IndividualInstruction;

	if (
		TryGetOpenPlayIndividualInstructionForCharacter(
			Candidate,
			IndividualInstruction
		)
		)
	{
		switch (IndividualInstruction.AttackInstruction)
		{
		case ESoccerIndividualAttackInstruction::MakeForwardRuns:
		case ESoccerIndividualAttackInstruction::StayWide:
		case ESoccerIndividualAttackInstruction::TargetPlayer:
			Score += IndividualPreferredRoleScoreBonus;
			break;

		case ESoccerIndividualAttackInstruction::LinkPlay:
		case ESoccerIndividualAttackInstruction::ComeShort:
			Score += IndividualSecondaryRoleScoreBonus;
			break;

		case ESoccerIndividualAttackInstruction::HoldPosition:
			Score -= IndividualAvoidRoleScorePenalty;
			break;

		default:
			break;
		}
	}

	return Score;
}

ASoccerAICharacter* ASoccerMatchManager::FindBestAttackingSecondaryAI(
	ESoccerTeam AttackingTeam,
	const TArray<const ASoccerAICharacter*>& ExcludedCharacters,
	const ASoccerAICharacter* SelectedSupportAI
) const
{
	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return nullptr;
	}

	ASoccerAICharacter* BestCharacter = nullptr;
	float BestScore = -TNumericLimits<float>::Max();

	for (TActorIterator<ASoccerAICharacter> It(World); It; ++It)
	{
		ASoccerAICharacter* Candidate = *It;

		if (!IsValid(Candidate))
		{
			continue;
		}

		if (Candidate->GetTeam() != AttackingTeam)
		{
			continue;
		}

		if (ExcludedCharacters.Contains(Candidate))
		{
			continue;
		}

		const float CandidateScore =
			ScoreAttackingSecondaryCandidate(
				AttackingTeam,
				Candidate,
				SelectedSupportAI
			);

		if (CandidateScore > BestScore)
		{
			BestScore = CandidateScore;
			BestCharacter = Candidate;
		}
	}

	return BestCharacter;
}

bool ASoccerMatchManager::CanCharacterBePassReceiverNow(
	const ASoccerCharacterBase* Passer,
	const ASoccerCharacterBase* Receiver
) const
{
	if (!IsValid(Receiver))
	{
		return false;
	}

	if (!bNoRetouchRestrictionActive)
	{
		return true;
	}

	if (!IsValid(NoRetouchRestrictedCharacter))
	{
		return true;
	}

	// Si el receptor no es el jugador restringido,
	// no hay problema.
	if (Receiver != NoRetouchRestrictedCharacter)
	{
		return true;
	}

	// Si el receptor es el jugador restringido,
	// solo puede recibir si el pase viene de otro jugador.
	// Ese toque del otro jugador libera la restricción.
	return IsValid(Passer) && Passer != NoRetouchRestrictedCharacter;
}

void ASoccerMatchManager::ClearPendingOffsideSnapshot()
{
	bHasPendingOffsideSnapshot = false;
PendingOffsideRestrictedPlayers.Empty();
}

bool ASoccerMatchManager::IsCharacterInOffsidePositionForCurrentTouch(
	const ASoccerCharacterBase* Candidate,
	ESoccerTeam AttackingTeam,
	float BallProgress,
	float LastFieldDefenderProgress,
	float MidfieldProgress,
	const FVector& OwnGoalLocation,
	const FVector& AttackDirection
) const
{
	if (!IsValid(Candidate))
	{
		return false;
	}

	if (Candidate->GetTeam() != AttackingTeam)
	{
		return false;
	}

	if (Candidate->GetPlayerRole() == ESoccerPlayerRole::Goalkeeper)
	{
		return false;
	}

	const float CandidateProgress =
		FVector::DotProduct(
			Candidate->GetActorLocation() - OwnGoalLocation,
			AttackDirection
		);

	// En mitad propia no hay offside.
	if (CandidateProgress <= MidfieldProgress + OffsidePositionTolerance)
	{
		return false;
	}

	// No hay offside si está detrás o en línea con la pelota.
	if (CandidateProgress <= BallProgress + OffsidePositionTolerance)
	{
		return false;
	}

	// Usamos último jugador de campo rival como referencia práctica.
	if (CandidateProgress <= LastFieldDefenderProgress + OffsidePositionTolerance)
	{
		return false;
	}

	return true;
}

bool ASoccerMatchManager::IsCharacterStillInOffsidePositionAtReception(
	const ASoccerCharacterBase* Candidate,
	ESoccerTeam AttackingTeam
) const
{
	if (!IsValid(Candidate))
	{
		return false;
	}

	if (Candidate->GetTeam() != AttackingTeam)
	{
		return false;
	}

	if (Candidate->GetPlayerRole() == ESoccerPlayerRole::Goalkeeper)
	{
		return false;
	}

	FVector OwnGoalLocation = FVector::ZeroVector;
	FVector AttackDirection = FVector::ZeroVector;
	float FieldLength = 0.0f;

	if (
		!TryGetAttackFieldFrame(
			AttackingTeam,
			OwnGoalLocation,
			AttackDirection,
			FieldLength
		)
		)
	{
		return false;
	}

	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return false;
	}

	const ESoccerTeam DefendingTeam =
		GetOppositeTeam(AttackingTeam);

	bool bFoundFieldDefender = false;

	float LastFieldDefenderProgress =
		-TNumericLimits<float>::Max();

	for (TActorIterator<ASoccerCharacterBase> It(World); It; ++It)
	{
		const ASoccerCharacterBase* Defender = *It;

		if (!IsValid(Defender))
		{
			continue;
		}

		if (Defender->GetTeam() != DefendingTeam)
		{
			continue;
		}

		if (Defender->GetPlayerRole() == ESoccerPlayerRole::Goalkeeper)
		{
			continue;
		}

		const float DefenderProgress =
			FVector::DotProduct(
				Defender->GetActorLocation() - OwnGoalLocation,
				AttackDirection
			);

		if (
			!bFoundFieldDefender ||
			DefenderProgress > LastFieldDefenderProgress
			)
		{
			bFoundFieldDefender = true;
			LastFieldDefenderProgress = DefenderProgress;
		}
	}

	if (!bFoundFieldDefender)
	{
		return false;
	}

	const float CandidateProgress =
		FVector::DotProduct(
			Candidate->GetActorLocation() - OwnGoalLocation,
			AttackDirection
		);

	const float MidfieldProgress = FieldLength * 0.5f;

	// Segunda validación solicitada para esta regla de juego:
	// además de haber estado adelantado al salir el pase, el receptor
	// debe seguir por delante de la línea defensiva cuando interviene.
	// No se compara contra la pelota en este instante porque, al recibirla,
	// ambos ocupan prácticamente el mismo avance longitudinal.
	if (CandidateProgress <= MidfieldProgress + OffsidePositionTolerance)
	{
		return false;
	}

	if (
		CandidateProgress <=
		LastFieldDefenderProgress + OffsidePositionTolerance
		)
	{
		return false;
	}

	return true;
}

void ASoccerMatchManager::RefreshPendingOffsideSnapshotForTouch(
	ASoccerCharacterBase* TouchingCharacter
)
{
	ClearPendingOffsideSnapshot();

	if (!bEnableOffsideRule)
	{
		return;
	}

	if (!IsValid(TouchingCharacter))
	{
		return;
	}

	if (!IsValid(SoccerBall))
	{
		return;
	}

	if (MatchPlayState != ESoccerMatchPlayState::Playing)
	{
		return;
	}

	const ESoccerTeam AttackingTeam =
		TouchingCharacter->GetTeam();

	FVector OwnGoalLocation = FVector::ZeroVector;
	FVector AttackDirection = FVector::ZeroVector;
	float FieldLength = 0.0f;

	if (
		!TryGetAttackFieldFrame(
			AttackingTeam,
			OwnGoalLocation,
			AttackDirection,
			FieldLength
		)
		)
	{
		return;
	}

	const ESoccerTeam DefendingTeam =
		GetOppositeTeam(AttackingTeam);

	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return;
	}

	bool bFoundFieldDefender = false;

	float LastFieldDefenderProgress =
		-TNumericLimits<float>::Max();

	for (TActorIterator<ASoccerCharacterBase> It(World); It; ++It)
	{
		const ASoccerCharacterBase* Candidate = *It;

		if (!IsValid(Candidate))
		{
			continue;
		}

		if (Candidate->GetTeam() != DefendingTeam)
		{
			continue;
		}

		if (Candidate->GetPlayerRole() == ESoccerPlayerRole::Goalkeeper)
		{
			continue;
		}

		const float CandidateProgress =
			FVector::DotProduct(
				Candidate->GetActorLocation() - OwnGoalLocation,
				AttackDirection
			);

		if (
			!bFoundFieldDefender ||
			CandidateProgress > LastFieldDefenderProgress
			)
		{
			bFoundFieldDefender = true;
			LastFieldDefenderProgress = CandidateProgress;
		}
	}

	if (!bFoundFieldDefender)
	{
		return;
	}

	const float BallProgress =
		FVector::DotProduct(
			SoccerBall->GetActorLocation() - OwnGoalLocation,
			AttackDirection
		);

	const float MidfieldProgress =
		FieldLength * 0.5f;

	for (TActorIterator<ASoccerCharacterBase> It(World); It; ++It)
	{
		ASoccerCharacterBase* Candidate = *It;

		if (!IsValid(Candidate))
		{
			continue;
		}

		if (Candidate == TouchingCharacter)
		{
			continue;
		}

		if (
			IsCharacterInOffsidePositionForCurrentTouch(
				Candidate,
				AttackingTeam,
				BallProgress,
				LastFieldDefenderProgress,
				MidfieldProgress,
				OwnGoalLocation,
				AttackDirection
			)
			)
		{
			PendingOffsideRestrictedPlayers.Add(Candidate);
		}
	}

	if (PendingOffsideRestrictedPlayers.Num() <= 0)
	{
		return;
	}

	bHasPendingOffsideSnapshot = true;
	PendingOffsideAttackingTeam = AttackingTeam;
}

bool ASoccerMatchManager::TryHandlePendingOffsideTouch(
	ASoccerCharacterBase* TouchingCharacter
)
{
	if (!bEnableOffsideRule)
	{
		return false;
	}

	if (!bHasPendingOffsideSnapshot)
	{
		return false;
	}

	if (!IsValid(TouchingCharacter))
	{
		return false;
	}

	// Si toca un rival, se considera que la jugada cambió.
	// Para la primera versión tomamos todo toque intencional rival
	// como liberador del offside pendiente.
	if (TouchingCharacter->GetTeam() != PendingOffsideAttackingTeam)
	{
		ClearPendingOffsideSnapshot();
		return false;
	}

	if (PendingOffsideRestrictedPlayers.Contains(TouchingCharacter))
	{
		// Estar adelantado al salir el pase lo convierte solamente en
		// candidato. La falta se confirma si también sigue adelantado
		// respecto de la línea defensiva cuando recibe o interviene.
		if (
			!IsCharacterStillInOffsidePositionAtReception(
				TouchingCharacter,
				PendingOffsideAttackingTeam
			)
			)
		{
			ClearPendingOffsideSnapshot();
			return false;
		}

		HandleOffsideOffense(TouchingCharacter);
		return true;
	}

	// Tocó un compañero que no estaba adelantado.
	// Ese toque reinicia la jugada y después se creará un nuevo snapshot.
	ClearPendingOffsideSnapshot();

	return false;
}

void ASoccerMatchManager::HandleOffsideOffense(
	ASoccerCharacterBase* OffendingCharacter
)
{
	if (!IsValid(OffendingCharacter))
	{
		return;
	}

	const ESoccerTeam RestartTeam =
		GetOppositeTeam(OffendingCharacter->GetTeam());

	FVector RestartLocation =
		OffendingCharacter->GetActorLocation();

	if (IsValid(SoccerBall))
	{
		RestartLocation.Z =
			SoccerBall->GetActorLocation().Z;
	}

	StartOffsideFreezePresentation(
		RestartTeam,
		RestartLocation
	);

	if (GEngine)
	{
		const FString TeamText =
			RestartTeam == ESoccerTeam::PlayerTeam
			? TEXT("PlayerTeam")
			: TEXT("OpponentTeam");

		GEngine->AddOnScreenDebugMessage(
			-1,
			1.8f,
			FColor::Red,
			FString::Printf(
				TEXT("OFFSIDE - Saca %s"),
				*TeamText
			)
		);
	}
}

void ASoccerMatchManager::StartOffsideFreezePresentation(
	ESoccerTeam RestartTeam,
	const FVector& RestartLocation
)
{
	if (GetWorld() == nullptr)
	{
		return;
	}

	PendingOffsideFreezeRestartTeam = RestartTeam;
	PendingOffsideFreezeRestartLocation = RestartLocation;

	if (!bEnableOffsideFreezePresentation || OffsideFreezeDuration <= 0.0f)
	{
		RequestMatchStateTransition(
			ESoccerMatchStateTransition::OffsideConfigurationAfterFreeze
		);
		return;
	}

	RequestMatchStateTransition(
		ESoccerMatchStateTransition::OffsideReviewFreeze
	);
}

void ASoccerMatchManager::SpawnOffsideFreezeLine(
	const FVector& RestartLocation
)
{
	DestroyOffsideFreezeLine();

	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return;
	}

	TSubclassOf<ASoccerOffsideLineActor> LineClass =
		OffsideLineActorClass;

	if (!LineClass)
	{
		LineClass = ASoccerOffsideLineActor::StaticClass();
	}

	const float LineWidth = SoccerFieldDimensions::PitchWidthCm;

	FVector LineCenter = RestartLocation;
	FRotator LineRotation = FRotator::ZeroRotator;

	if (IsValid(SoccerField))
	{
		FVector LineLocal = SoccerField->WorldToPitchLocal(RestartLocation);
		LineLocal.Y = SoccerFieldDimensions::CenterY;
		LineLocal.Z = OffsideFreezeLineCenterZ;
		LineCenter = SoccerField->PitchLocalToWorld(LineLocal);
		LineRotation = SoccerField->GetActorRotation();
	}
	else
	{
		LineCenter.Y = SoccerFieldDimensions::CenterY;
		LineCenter.Z = OffsideFreezeLineCenterZ;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ActiveOffsideLineActor =
		World->SpawnActor<ASoccerOffsideLineActor>(
			LineClass,
			LineCenter,
			LineRotation,
			SpawnParameters
		);

	if (!IsValid(ActiveOffsideLineActor))
	{
		return;
	}

	ActiveOffsideLineActor->ConfigureLine(
		LineCenter,
		FVector(
			FMath::Max(1.0f, OffsideFreezeLineThickness),
			LineWidth,
			FMath::Max(1.0f, OffsideFreezeLineHeight)
		),
		OffsideFreezeLineColor
	);
}

void ASoccerMatchManager::DestroyOffsideFreezeLine()
{
	if (IsValid(ActiveOffsideLineActor))
	{
		ActiveOffsideLineActor->Destroy();
	}

	ActiveOffsideLineActor = nullptr;
}

void ASoccerMatchManager::StartDirectFreeKickRestart(
	ESoccerTeam RestartTeam,
	const FVector& RestartLocation
)
{
	ActivateMatchState(
		MakeUnique<FSoccerFaultConfigurationState>(RestartTeam, RestartLocation)
	);
}

bool ASoccerMatchManager::IsOffsideRestartActive() const
{
	return FreeKickRestart.IsActive(*this);
}

bool ASoccerMatchManager::IsOffsideRestartTaker(
	const ASoccerAICharacter* SoccerAICharacter
) const
{
	return FreeKickRestart.IsTaker(*this, SoccerAICharacter);
}

bool ASoccerMatchManager::ShouldDefendingFreeKickCharacterFaceBall(
	const ASoccerAICharacter* SoccerAICharacter
) const
{
	return FreeKickRestart.ShouldDefendingCharacterFaceBall(SoccerAICharacter);
}

bool ASoccerMatchManager::IsFreeKickDefensiveWallMember(
	const ASoccerAICharacter* SoccerAICharacter
) const
{
	return FreeKickRestart.IsDefensiveWallMember(SoccerAICharacter);
}

float ASoccerMatchManager::GetFreeKickWallMoveAcceptanceRadius() const
{
	return FMath::Clamp(FreeKickWallMoveAcceptanceRadius, 1.0f, 50.0f);
}

bool ASoccerMatchManager::IsHumanFreeKickTaker(
	const AThirdPersonCppCharacter* HumanCharacter
) const
{
	return FreeKickRestart.IsHumanTaker(*this, HumanCharacter);
}

bool ASoccerMatchManager::CanHumanFreeKickTakerExecuteNow(
	const AThirdPersonCppCharacter* HumanCharacter
) const
{
	return FreeKickRestart.CanHumanTakerExecute(*this, HumanCharacter);
}

bool ASoccerMatchManager::IsHumanFootRestartTaker(
	const AThirdPersonCppCharacter* HumanCharacter
) const
{
	if (!IsValid(HumanCharacter))
	{
		return false;
	}

	if (FreeKickRestart.IsHumanTaker(*this, HumanCharacter))
	{
		return true;
	}

	return
		bActiveNonFreeKickHumanTakerClaimed &&
		IsValid(ActiveNonFreeKickHumanTaker) &&
		HumanCharacter == ActiveNonFreeKickHumanTaker &&
		(
			ActiveNonFreeKickHumanTakerType == ESoccerRestartType::Kickoff ||
			ActiveNonFreeKickHumanTakerType == ESoccerRestartType::GoalKick ||
			ActiveNonFreeKickHumanTakerType == ESoccerRestartType::CornerKick
		);
}

bool ASoccerMatchManager::CanHumanFootRestartTakerExecuteNow(
	const AThirdPersonCppCharacter* HumanCharacter
) const
{
	if (FreeKickRestart.CanHumanTakerExecute(*this, HumanCharacter))
	{
		return true;
	}

	if (
		!bActiveNonFreeKickHumanExecutionAuthorized ||
		!IsHumanFootRestartTaker(HumanCharacter) ||
		!IsRestartContextActive() ||
		ActiveRestartType != ActiveNonFreeKickHumanTakerType
	)
	{
		return false;
	}

	switch (ActiveNonFreeKickHumanTakerType)
	{
	case ESoccerRestartType::Kickoff:
		return MatchPlayState == ESoccerMatchPlayState::KickoffTaking;

	case ESoccerRestartType::GoalKick:
	case ESoccerRestartType::CornerKick:
		return MatchPlayState == ESoccerMatchPlayState::GoalLineRestartTaking;

	default:
		return false;
	}
}

bool ASoccerMatchManager::IsOffsideRestartFinalRunActiveForCharacter(
	const ASoccerAICharacter* SoccerAICharacter
) const
{
	return FreeKickRestart.IsFinalRunActiveForCharacter(*this, SoccerAICharacter);
}

float ASoccerMatchManager::GetOffsideRestartRunUpMoveAcceptanceRadius() const
{
	return FMath::Max(1.0f, OffsideRestartRunUpMoveAcceptanceRadius);
}

ESoccerAIOrder ASoccerMatchManager::GetDefaultRestartAttackOrderForCharacter(
	const ASoccerAICharacter* SoccerAICharacter
) const
{
	if (!IsValid(SoccerAICharacter))
	{
		return ESoccerAIOrder::MaintainTeamShape;
	}

	const ESoccerPlayerRole PlayerRole =
		SoccerAICharacter->GetPlayerRole();

	if (PlayerRole == ESoccerPlayerRole::Defender)
	{
		return ESoccerAIOrder::AttackRestDefense;
	}

	if (PlayerRole == ESoccerPlayerRole::Forward)
	{
		return ESoccerAIOrder::AttackSupportForward;
	}

	return ESoccerAIOrder::AttackSupportShort;
}

FVector ASoccerMatchManager::GetOffsideRestartMoveLocation(
	const ASoccerAICharacter* SoccerAICharacter
) const
{
	return FreeKickRestart.GetMoveLocation(*this, SoccerAICharacter);
}

FVector ASoccerMatchManager::BuildOffsideRestartOpponentMoveLocation(
	const ASoccerAICharacter* SoccerAICharacter
) const
{
	return FreeKickRestart.BuildOpponentMoveLocation(*this, SoccerAICharacter);
}

void ASoccerMatchManager::ReleaseAllHumanBallPossessions()
{
	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return;
	}

	for (TActorIterator<AThirdPersonCppCharacter> It(World); It; ++It)
	{
		AThirdPersonCppCharacter* HumanCharacter = *It;

		if (!IsValid(HumanCharacter))
		{
			continue;
		}

		const bool bWasPossessingBall =
			HumanCharacter->HasHumanLogicalBallControl();

		if (bWasPossessingBall)
		{
			HumanCharacter->ReleaseBallForMatchRestart();
		}
		else
		{
			// A restart must also erase assisted actions that do not own the ball:
			// chase, steal, aerial interception, queued kick and charged release.
			HumanCharacter->ClearBallActionsForMatchRestriction();
		}
	}
}



bool ASoccerMatchManager::IsPenaltyKickRestartActive() const
{
	return PenaltyKickRestart.IsActive(*this);
}

bool ASoccerMatchManager::IsPenaltyKickTaker(
	const ASoccerCharacterBase* Character
) const
{
	return PenaltyKickRestart.IsTaker(Character);
}

bool ASoccerMatchManager::IsHumanPenaltyTaker(
	const AThirdPersonCppCharacter* Character
) const
{
	return PenaltyKickRestart.IsHumanTaker(Character);
}

bool ASoccerMatchManager::IsPenaltyKickDefendingGoalkeeper(
	const ASoccerAICharacter* Character
) const
{
	return PenaltyKickRestart.IsDefendingGoalkeeper(Character);
}

float ASoccerMatchManager::GetPenaltyKickRunUpMoveAcceptanceRadius() const
{
	return FMath::Max(10.0f, PenaltyKickRunUpMoveAcceptanceRadius);
}

float ASoccerMatchManager::GetPenaltyKickGoalkeeperMoveAcceptanceRadius() const
{
	return FMath::Max(5.0f, PenaltyKickGoalkeeperCenterAcceptanceRadius);
}

AThirdPersonCppCharacter* ASoccerMatchManager::FindHumanCharacterForTeam(
	ESoccerTeam Team
) const
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return nullptr;
	}

	for (TActorIterator<AThirdPersonCppCharacter> It(World); It; ++It)
	{
		AThirdPersonCppCharacter* Candidate = *It;
		if (IsValid(Candidate) && Candidate->GetTeam() == Team)
		{
			return Candidate;
		}
	}

	return nullptr;
}

FVector ASoccerMatchManager::BuildPenaltyKickMoveLocation(
	const ASoccerAICharacter* SoccerAICharacter
) const
{
	return PenaltyKickRestart.BuildMoveLocation(*this, SoccerAICharacter);
}

bool ASoccerMatchManager::DebugStartPenaltyKickForTeam(ESoccerTeam RestartTeam)
{
	// The tester intentionally bypasses foul classification, but it uses the
	// exact same penalty restart path as a real foul from this point onward.
	const FVector IncidentLocation = PenaltyKickRestart.GetSpotLocation(*this, RestartTeam);

	ASoccerDebugManager::Message(
		this,
		ESoccerDebugCategory::Restarts,
		FString::Printf(
			TEXT("NUMPAD 6 PENAL TEST: iniciando penal para team=%d"),
			static_cast<int32>(RestartTeam)
		),
		FColor::Yellow
	);

	return StartPenaltyKickRestart(RestartTeam, IncidentLocation);
}

bool ASoccerMatchManager::BeginPenaltyFoulDelay(
	ESoccerTeam RestartTeam,
	const FVector& IncidentLocation
)
{
	return ActivateMatchState(
		MakeUnique<FSoccerPenaltyFoulDelayState>(
			RestartTeam,
			IncidentLocation
		)
	);
}

bool ASoccerMatchManager::StartPenaltyKickRestart(
	ESoccerTeam RestartTeam,
	const FVector& IncidentLocation
)
{
	PendingPenaltyFoulRestartTeam = ESoccerTeam::PlayerTeam;
	PendingPenaltyFoulIncidentLocation = FVector::ZeroVector;

	return ActivateMatchState(
		MakeUnique<FSoccerPenaltyConfigurationState>(
			RestartTeam,
			IncidentLocation
		)
	);
}

void ASoccerMatchManager::CancelPenaltyKickRestart()
{
	if (IsPenaltyMatchStateActive())
	{
		ClearActiveMatchState();
	}

	PendingPenaltyFoulRestartTeam = ESoccerTeam::PlayerTeam;
	PendingPenaltyFoulIncidentLocation = FVector::ZeroVector;
	PenaltyKickRestart.Cancel(*this);
}

//DEFENSE////////////////////////////////               ///////////////////////////////////////
//DEFENSE////////////////////////////////               ///////////////////////////////////////
//DEFENSE////////////////////////////////               ///////////////////////////////////////
//DEFENSE////////////////////////////////               ///////////////////////////////////////
//DEFENSE////////////////////////////////               ///////////////////////////////////////
//DEFENSE////////////////////////////////               ///////////////////////////////////////

FVector ASoccerMatchManager::GetDefensiveMoveLocation(
	const ASoccerAICharacter* SoccerAICharacter
) const
{
	if (SoccerAICharacter == nullptr)
	{
		return FVector::ZeroVector;
	}

	const ESoccerAIOrder Order =
		GetAIOrderForCharacter(SoccerAICharacter);

	FVector BaseDesiredLocation = FVector::ZeroVector;

	if (Order == ESoccerAIOrder::DefendProtectGoalLane)
	{
		BaseDesiredLocation =
			BuildDefendProtectGoalLaneLocation(SoccerAICharacter);
	}
	else if (Order == ESoccerAIOrder::DefendCoverCenter)
	{
		BaseDesiredLocation =
			BuildDefendCoverCenterLocation(SoccerAICharacter);
	}
	else if (Order == ESoccerAIOrder::DefendMarkDangerousReceiver)
	{
		BaseDesiredLocation =
			BuildDefendMarkReceiverLocation(SoccerAICharacter);
	}
	else if (Order == ESoccerAIOrder::DefendCompactShape)
	{
		BaseDesiredLocation =
			BuildDefendCompactShapeLocation(SoccerAICharacter);
	}
	else
	{
		BaseDesiredLocation = BuildDynamicTeamShapeLocation(
			SoccerAICharacter->GetTeam(),
			SoccerAICharacter
		);
	}

	return AdjustDefensiveMoveLocationForGoalAreaCoordination(
		SoccerAICharacter,
		Order,
		BaseDesiredLocation
	);
}

bool ASoccerMatchManager::IsGoalAreaDefenderCoordinationActiveForCharacter(
	const ASoccerAICharacter* SoccerAICharacter
) const
{
	if (!IsValid(SoccerAICharacter))
	{
		return false;
	}

	if (
		SoccerAICharacter->GetPlayerRole() ==
		ESoccerPlayerRole::Goalkeeper
		)
	{
		return false;
	}

	ASoccerAICharacter* Goalkeeper = nullptr;
	FVector ThreatLocation = FVector::ZeroVector;
	FVector OwnGoalLocation = FVector::ZeroVector;
	FVector AttackDirection = FVector::ZeroVector;
	FVector RightDirection = FVector::ZeroVector;

	if (
		!TryBuildGoalAreaDefenderCoordinationContext(
			SoccerAICharacter,
			Goalkeeper,
			ThreatLocation,
			OwnGoalLocation,
			AttackDirection,
			RightDirection
		)
		)
	{
		return false;
	}

	const ESoccerAIOrder Order =
		GetAIOrderForCharacter(SoccerAICharacter);

	if (
		Order == ESoccerAIOrder::DefendProtectGoalLane ||
		Order == ESoccerAIOrder::DefendCoverCenter ||
		Order == ESoccerAIOrder::DefendMarkDangerousReceiver
		)
	{
		return true;
	}

	return FVector::Dist2D(
		SoccerAICharacter->GetActorLocation(),
		OwnGoalLocation
	) <= FMath::Max(
		50.0f,
		GoalAreaDefenderCoordinationParticipationDistance
	);
}

bool ASoccerMatchManager::TryGetGoalAreaAttackerHoldingWaitLocation(
	const ASoccerAICharacter* SoccerAICharacter,
	FVector& OutMoveLocation
) const
{
	OutMoveLocation = FVector::ZeroVector;

	ASoccerAICharacter* Goalkeeper = nullptr;
	FVector BallLocation = FVector::ZeroVector;
	FVector OpponentGoalLocation = FVector::ZeroVector;
	FVector FieldOutwardDirection = FVector::ZeroVector;
	FVector RightDirection = FVector::ZeroVector;
	bool bGoalkeeperHoldingBall = false;
	bool bGoalkeeperActionActive = false;

	if (
		!TryBuildGoalAreaAttackerRespectContext(
			SoccerAICharacter,
			Goalkeeper,
			BallLocation,
			OpponentGoalLocation,
			FieldOutwardDirection,
			RightDirection,
			bGoalkeeperHoldingBall,
			bGoalkeeperActionActive
		) ||
		!bGoalkeeperHoldingBall
		)
	{
		return false;
	}

	OutMoveLocation = BuildGoalAreaAttackerHoldingWaitLocation(
		SoccerAICharacter,
		Goalkeeper,
		OpponentGoalLocation,
		FieldOutwardDirection,
		RightDirection
	);

	return !OutMoveLocation.IsNearlyZero();
}

bool ASoccerMatchManager::TryAdjustGoalAreaAttackerBallChaseLocation(
	const ASoccerAICharacter* SoccerAICharacter,
	const FVector& DesiredBallLocation,
	FVector& OutMoveLocation
) const
{
	OutMoveLocation = DesiredBallLocation;

	ASoccerAICharacter* Goalkeeper = nullptr;
	FVector BallLocation = FVector::ZeroVector;
	FVector OpponentGoalLocation = FVector::ZeroVector;
	FVector FieldOutwardDirection = FVector::ZeroVector;
	FVector RightDirection = FVector::ZeroVector;
	bool bGoalkeeperHoldingBall = false;
	bool bGoalkeeperActionActive = false;

	if (
		!TryBuildGoalAreaAttackerRespectContext(
			SoccerAICharacter,
			Goalkeeper,
			BallLocation,
			OpponentGoalLocation,
			FieldOutwardDirection,
			RightDirection,
			bGoalkeeperHoldingBall,
			bGoalkeeperActionActive
		)
		)
	{
		return false;
	}

	if (bGoalkeeperHoldingBall)
	{
		OutMoveLocation = BuildGoalAreaAttackerHoldingWaitLocation(
			SoccerAICharacter,
			Goalkeeper,
			OpponentGoalLocation,
			FieldOutwardDirection,
			RightDirection
		);
		return !OutMoveLocation.IsNearlyZero();
	}

	// Con otro jugador en posesion no modificamos el duelo normal. La
	// aproximacion lateral se reserva para una pelota libre que el arquero
	// esta intentando capturar, bloquear o desviar.
	if (
		!bGoalkeeperActionActive ||
		IsValid(PossessingCharacter)
		)
	{
		return false;
	}

	if (
		FVector::Dist2D(
			SoccerAICharacter->GetActorLocation(),
			BallLocation
		) <= FMath::Max(0.0f, GoalAreaAttackerDirectContestDistance)
		)
	{
		return false;
	}

	OutMoveLocation = BuildGoalAreaAttackerSideApproachLocation(
		SoccerAICharacter,
		Goalkeeper,
		BallLocation,
		OpponentGoalLocation,
		FieldOutwardDirection,
		RightDirection
	);

	return !OutMoveLocation.IsNearlyZero();
}

ASoccerAICharacter* ASoccerMatchManager::FindGoalkeeperForTeam(
	ESoccerTeam Team
) const
{
	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return nullptr;
	}

	for (TActorIterator<ASoccerAICharacter> It(World); It; ++It)
	{
		ASoccerAICharacter* Candidate = *It;

		if (!IsValid(Candidate))
		{
			continue;
		}

		if (Candidate->GetTeam() != Team)
		{
			continue;
		}

		if (
			Candidate->GetPlayerRole() ==
			ESoccerPlayerRole::Goalkeeper
			)
		{
			return Candidate;
		}
	}

	return nullptr;
}

bool ASoccerMatchManager::TryBuildGoalAreaDefenderCoordinationContext(
	const ASoccerAICharacter* SoccerAICharacter,
	ASoccerAICharacter*& OutGoalkeeper,
	FVector& OutThreatLocation,
	FVector& OutOwnGoalLocation,
	FVector& OutAttackDirection,
	FVector& OutRightDirection
) const
{
	OutGoalkeeper = nullptr;
	OutThreatLocation = FVector::ZeroVector;
	OutOwnGoalLocation = FVector::ZeroVector;
	OutAttackDirection = FVector::ZeroVector;
	OutRightDirection = FVector::ZeroVector;

	if (
		!bUseGoalAreaDefenderCoordination ||
		!IsValid(SoccerAICharacter)
		)
	{
		return false;
	}

	const ESoccerTeam Team = SoccerAICharacter->GetTeam();

	if (!IsTeamCurrentlyDefending(Team))
	{
		return false;
	}

	ASoccerAICharacter* Goalkeeper =
		FindGoalkeeperForTeam(Team);

	if (!IsValid(Goalkeeper))
	{
		return false;
	}

	const FVector OwnGoalLocation =
		GetOwnGoalReferenceLocation(Team);

	const FVector OpponentGoalLocation =
		GetOpponentGoalReferenceLocation(Team);

	FVector AttackDirection =
		OpponentGoalLocation - OwnGoalLocation;

	AttackDirection.Z = 0.0f;
	AttackDirection = AttackDirection.GetSafeNormal();

	const FVector RightDirection =
		GetFieldRightDirectionForTeam(Team);

	if (
		AttackDirection.IsNearlyZero() ||
		RightDirection.IsNearlyZero()
		)
	{
		return false;
	}

	const float ActivationMargin =
		FMath::Max(
			0.0f,
			GoalAreaDefenderCoordinationActivationMargin
		);

	FVector ThreatLocation =
		GetCurrentDefensiveThreatLocation(Team);

	bool bThreatInsideAuthorityArea =
		!ThreatLocation.IsNearlyZero() &&
		IsLocationInsideGoalAreaForTeam(
			ThreatLocation,
			Team,
			ActivationMargin
		);

	if (IsValid(SoccerBall))
	{
		const FVector BallLocation =
			SoccerBall->GetActorLocation();

		const bool bBallInsideAuthorityArea =
			IsLocationInsideGoalAreaForTeam(
				BallLocation,
				Team,
				ActivationMargin
			);

		const bool bGoalkeeperActionNearAuthorityArea =
			Goalkeeper->IsGoalkeeperActionActive() &&
			IsLocationInsideGoalAreaForTeam(
				BallLocation,
				Team,
				ActivationMargin +
				FMath::Max(
					0.0f,
					GoalAreaGoalkeeperCorridorFrontExtension
				)
			);

		if (
			bBallInsideAuthorityArea ||
			bGoalkeeperActionNearAuthorityArea
			)
		{
			ThreatLocation = BallLocation;
			bThreatInsideAuthorityArea = true;
		}
	}

	if (!bThreatInsideAuthorityArea)
	{
		return false;
	}

	OutGoalkeeper = Goalkeeper;
	OutThreatLocation = ThreatLocation;
	OutOwnGoalLocation = OwnGoalLocation;
	OutAttackDirection = AttackDirection;
	OutRightDirection = RightDirection;

	return true;
}

FVector ASoccerMatchManager::AdjustDefensiveMoveLocationForGoalAreaCoordination(
	const ASoccerAICharacter* SoccerAICharacter,
	ESoccerAIOrder CurrentOrder,
	const FVector& BaseDesiredLocation
) const
{
	if (!IsValid(SoccerAICharacter))
	{
		return BaseDesiredLocation;
	}

	ASoccerAICharacter* Goalkeeper = nullptr;
	FVector ThreatLocation = FVector::ZeroVector;
	FVector OwnGoalLocation = FVector::ZeroVector;
	FVector AttackDirection = FVector::ZeroVector;
	FVector RightDirection = FVector::ZeroVector;

	if (
		!TryBuildGoalAreaDefenderCoordinationContext(
			SoccerAICharacter,
			Goalkeeper,
			ThreatLocation,
			OwnGoalLocation,
			AttackDirection,
			RightDirection
		)
		)
	{
		return BaseDesiredLocation;
	}

	FVector CoordinatedLocation = BaseDesiredLocation;

	if (CurrentOrder == ESoccerAIOrder::DefendProtectGoalLane)
	{
		CoordinatedLocation = BuildGoalAreaFarPostCoverLocation(
			SoccerAICharacter,
			Goalkeeper,
			ThreatLocation,
			OwnGoalLocation,
			AttackDirection,
			RightDirection
		);
	}
	else if (CurrentOrder == ESoccerAIOrder::DefendCoverCenter)
	{
		CoordinatedLocation = BuildGoalAreaSecondaryCoverLocation(
			SoccerAICharacter,
			Goalkeeper,
			ThreatLocation,
			OwnGoalLocation,
			AttackDirection,
			RightDirection
		);
	}

	CoordinatedLocation =
		MoveDefensiveLocationOutsideGoalkeeperCorridor(
			SoccerAICharacter,
			Goalkeeper,
			ThreatLocation,
			OwnGoalLocation,
			AttackDirection,
			RightDirection,
			CoordinatedLocation
		);

	CoordinatedLocation.Z =
		SoccerAICharacter->GetActorLocation().Z;

	return ProjectLocationToNavigation(
		CoordinatedLocation,
		SoccerAICharacter
	);
}

FVector ASoccerMatchManager::BuildGoalAreaFarPostCoverLocation(
	const ASoccerAICharacter* SoccerAICharacter,
	const ASoccerAICharacter* Goalkeeper,
	const FVector& ThreatLocation,
	const FVector& OwnGoalLocation,
	const FVector& AttackDirection,
	const FVector& RightDirection
) const
{
	if (!IsValid(SoccerAICharacter) || !IsValid(Goalkeeper))
	{
		return SoccerAICharacter != nullptr
			? SoccerAICharacter->GetActorLocation()
			: FVector::ZeroVector;
	}

	const float ThreatLateral = FVector::DotProduct(
		ThreatLocation - OwnGoalLocation,
		RightDirection
	);

	const float GoalkeeperLateral = FVector::DotProduct(
		Goalkeeper->GetActorLocation() - OwnGoalLocation,
		RightDirection
	);

	float CoverSideSign = 0.0f;

	if (FMath::Abs(ThreatLateral) >= 55.0f)
	{
		CoverSideSign = ThreatLateral >= 0.0f ? -1.0f : 1.0f;
	}
	else if (FMath::Abs(GoalkeeperLateral) >= 40.0f)
	{
		CoverSideSign = GoalkeeperLateral >= 0.0f ? -1.0f : 1.0f;
	}
	else
	{
		const float CharacterLateral = FVector::DotProduct(
			SoccerAICharacter->GetActorLocation() - OwnGoalLocation,
			RightDirection
		);

		CoverSideSign = CharacterLateral >= 0.0f ? 1.0f : -1.0f;
	}

	const float LateralMagnitude = FMath::Clamp(
		SoccerFieldDimensions::GoalHalfWidthCm -
		FMath::Max(0.0f, GoalAreaFarPostInsetFromPost),
		80.0f,
		SoccerFieldDimensions::GoalAreaHalfWidthCm - 45.0f
	);

	FVector DesiredLocation =
		OwnGoalLocation +
		AttackDirection * FMath::Max(45.0f, GoalAreaFarPostCoverDepth) +
		RightDirection * CoverSideSign * LateralMagnitude;

	DesiredLocation.Z = SoccerAICharacter->GetActorLocation().Z;
	return DesiredLocation;
}

FVector ASoccerMatchManager::BuildGoalAreaSecondaryCoverLocation(
	const ASoccerAICharacter* SoccerAICharacter,
	const ASoccerAICharacter* Goalkeeper,
	const FVector& ThreatLocation,
	const FVector& OwnGoalLocation,
	const FVector& AttackDirection,
	const FVector& RightDirection
) const
{
	if (!IsValid(SoccerAICharacter) || !IsValid(Goalkeeper))
	{
		return SoccerAICharacter != nullptr
			? SoccerAICharacter->GetActorLocation()
			: FVector::ZeroVector;
	}

	const float ThreatLateral = FVector::DotProduct(
		ThreatLocation - OwnGoalLocation,
		RightDirection
	);

	const float GoalkeeperLateral = FVector::DotProduct(
		Goalkeeper->GetActorLocation() - OwnGoalLocation,
		RightDirection
	);

	float CoverSideSign = 0.0f;

	if (FMath::Abs(ThreatLateral) >= 55.0f)
	{
		CoverSideSign = ThreatLateral >= 0.0f ? -1.0f : 1.0f;
	}
	else if (FMath::Abs(GoalkeeperLateral) >= 40.0f)
	{
		CoverSideSign = GoalkeeperLateral >= 0.0f ? -1.0f : 1.0f;
	}
	else
	{
		const float CharacterLateral = FVector::DotProduct(
			SoccerAICharacter->GetActorLocation() - OwnGoalLocation,
			RightDirection
		);

		CoverSideSign = CharacterLateral >= 0.0f ? 1.0f : -1.0f;
	}

	const float DesiredLateralMagnitude = FMath::Clamp(
		FMath::Max(
			GoalAreaGoalkeeperMobilityCorridorHalfWidth +
			GoalAreaDefenderCorridorClearance +
			GoalAreaSecondaryCoverExtraLateralClearance,
			170.0f
		),
		120.0f,
		SoccerFieldDimensions::GoalAreaHalfWidthCm - 45.0f
	);

	FVector DesiredLocation =
		OwnGoalLocation +
		AttackDirection * FMath::Max(100.0f, GoalAreaSecondaryCoverDepth) +
		RightDirection * CoverSideSign * DesiredLateralMagnitude;

	DesiredLocation.Z = SoccerAICharacter->GetActorLocation().Z;
	return DesiredLocation;
}

bool ASoccerMatchManager::IsLocationInsideGoalkeeperMobilityCorridor2D(
	const FVector& Location,
	const FVector& GoalkeeperLocation,
	const FVector& ThreatLocation,
	float CorridorHalfWidth
) const
{
	FVector CorridorDirection =
		ThreatLocation - GoalkeeperLocation;

	CorridorDirection.Z = 0.0f;

	const float BaseCorridorLength =
		CorridorDirection.Size();

	if (BaseCorridorLength <= 1.0f)
	{
		return FVector::Dist2D(
			Location,
			GoalkeeperLocation
		) <= FMath::Max(1.0f, CorridorHalfWidth);
	}

	CorridorDirection /= BaseCorridorLength;

	FVector CorridorStart =
		GoalkeeperLocation -
		CorridorDirection *
		FMath::Max(0.0f, GoalAreaGoalkeeperCorridorBackExtension);

	FVector CorridorEnd =
		ThreatLocation +
		CorridorDirection *
		FMath::Max(0.0f, GoalAreaGoalkeeperCorridorFrontExtension);

	CorridorStart.Z = 0.0f;
	CorridorEnd.Z = 0.0f;

	FVector FlatLocation = Location;
	FlatLocation.Z = 0.0f;

	const FVector CorridorVector = CorridorEnd - CorridorStart;
	const float CorridorLengthSquared = CorridorVector.SizeSquared();

	if (CorridorLengthSquared <= 1.0f)
	{
		return FVector::Dist2D(
			FlatLocation,
			CorridorStart
		) <= FMath::Max(1.0f, CorridorHalfWidth);
	}

	const float SegmentAlpha = FMath::Clamp(
		FVector::DotProduct(
			FlatLocation - CorridorStart,
			CorridorVector
		) / CorridorLengthSquared,
		0.0f,
		1.0f
	);

	const FVector ClosestPoint =
		CorridorStart + CorridorVector * SegmentAlpha;

	return FVector::Dist2D(
		FlatLocation,
		ClosestPoint
	) <= FMath::Max(1.0f, CorridorHalfWidth);
}

FVector ASoccerMatchManager::MoveDefensiveLocationOutsideGoalkeeperCorridor(
	const ASoccerAICharacter* SoccerAICharacter,
	const ASoccerAICharacter* Goalkeeper,
	const FVector& ThreatLocation,
	const FVector& OwnGoalLocation,
	const FVector& AttackDirection,
	const FVector& RightDirection,
	const FVector& DesiredLocation
) const
{
	if (!IsValid(SoccerAICharacter) || !IsValid(Goalkeeper))
	{
		return DesiredLocation;
	}

	const FVector CharacterLocation =
		SoccerAICharacter->GetActorLocation();

	const FVector GoalkeeperLocation =
		Goalkeeper->GetActorLocation();

	const float CorridorHalfWidth = FMath::Max(
		30.0f,
		GoalAreaGoalkeeperMobilityCorridorHalfWidth
	);

	const bool bCharacterInsideCorridor =
		IsLocationInsideGoalkeeperMobilityCorridor2D(
			CharacterLocation,
			GoalkeeperLocation,
			ThreatLocation,
			CorridorHalfWidth
		);

	const bool bDesiredInsideCorridor =
		IsLocationInsideGoalkeeperMobilityCorridor2D(
			DesiredLocation,
			GoalkeeperLocation,
			ThreatLocation,
			CorridorHalfWidth
		);

	const bool bCharacterTooCloseToGoalkeeper =
		FVector::Dist2D(
			CharacterLocation,
			GoalkeeperLocation
		) < FMath::Max(
			30.0f,
			GoalAreaDefenderMinDistanceFromGoalkeeper
		);

	const bool bDesiredTooCloseToGoalkeeper =
		FVector::Dist2D(
			DesiredLocation,
			GoalkeeperLocation
		) < FMath::Max(
			30.0f,
			GoalAreaDefenderMinDistanceFromGoalkeeper
		);

	if (
		!bCharacterInsideCorridor &&
		!bDesiredInsideCorridor &&
		!bCharacterTooCloseToGoalkeeper &&
		!bDesiredTooCloseToGoalkeeper
		)
	{
		return DesiredLocation;
	}

	FVector CorridorDirection =
		ThreatLocation - GoalkeeperLocation;

	CorridorDirection.Z = 0.0f;

	if (CorridorDirection.SizeSquared() <= 1.0f)
	{
		CorridorDirection = AttackDirection;
	}

	CorridorDirection = CorridorDirection.GetSafeNormal();

	if (CorridorDirection.IsNearlyZero())
	{
		return DesiredLocation;
	}

	const FVector CorridorNormal(
		-CorridorDirection.Y,
		CorridorDirection.X,
		0.0f
	);

	FVector CorridorStart =
		GoalkeeperLocation -
		CorridorDirection *
		FMath::Max(0.0f, GoalAreaGoalkeeperCorridorBackExtension);

	FVector CorridorEnd =
		ThreatLocation +
		CorridorDirection *
		FMath::Max(0.0f, GoalAreaGoalkeeperCorridorFrontExtension);

	CorridorStart.Z = CharacterLocation.Z;
	CorridorEnd.Z = CharacterLocation.Z;

	const FVector ReferenceLocation =
		bCharacterInsideCorridor || bCharacterTooCloseToGoalkeeper
		? CharacterLocation
		: DesiredLocation;

	const FVector CorridorVector = CorridorEnd - CorridorStart;
	const float CorridorLengthSquared = CorridorVector.SizeSquared();

	const float SegmentAlpha =
		CorridorLengthSquared > 1.0f
		? FMath::Clamp(
			FVector::DotProduct(
				ReferenceLocation - CorridorStart,
				CorridorVector
			) / CorridorLengthSquared,
			0.0f,
			1.0f
		)
		: 0.0f;

	const FVector ClosestPoint =
		CorridorStart + CorridorVector * SegmentAlpha;

	const float RequiredOffset =
		CorridorHalfWidth +
		FMath::Max(0.0f, GoalAreaDefenderCorridorClearance);

	FVector CandidateA =
		ClosestPoint + CorridorNormal * RequiredOffset;

	FVector CandidateB =
		ClosestPoint - CorridorNormal * RequiredOffset;

	const float MaxLateral =
		SoccerFieldDimensions::GoalAreaHalfWidthCm +
		FMath::Max(0.0f, GoalAreaCoordinationMaxLateralBeyondArea);

	const float MaxDepth =
		SoccerFieldDimensions::GoalAreaDepthCm +
		FMath::Max(
			120.0f,
			GoalAreaDefenderCoordinationActivationMargin
		);

	const float CandidateADepth = FMath::Clamp(
		FVector::DotProduct(
			CandidateA - OwnGoalLocation,
			AttackDirection
		),
		45.0f,
		MaxDepth
	);

	const float CandidateALateral = FMath::Clamp(
		FVector::DotProduct(
			CandidateA - OwnGoalLocation,
			RightDirection
		),
		-MaxLateral,
		MaxLateral
	);

	CandidateA =
		OwnGoalLocation +
		AttackDirection * CandidateADepth +
		RightDirection * CandidateALateral;

	const float CandidateBDepth = FMath::Clamp(
		FVector::DotProduct(
			CandidateB - OwnGoalLocation,
			AttackDirection
		),
		45.0f,
		MaxDepth
	);

	const float CandidateBLateral = FMath::Clamp(
		FVector::DotProduct(
			CandidateB - OwnGoalLocation,
			RightDirection
		),
		-MaxLateral,
		MaxLateral
	);

	CandidateB =
		OwnGoalLocation +
		AttackDirection * CandidateBDepth +
		RightDirection * CandidateBLateral;

	CandidateA.Z = CharacterLocation.Z;
	CandidateB.Z = CharacterLocation.Z;

	const float MinimumGoalkeeperDistance =
		FMath::Max(
			30.0f,
			GoalAreaDefenderMinDistanceFromGoalkeeper
		);

	FVector FromGoalkeeperToA = CandidateA - GoalkeeperLocation;
	FromGoalkeeperToA.Z = 0.0f;

	if (
		FromGoalkeeperToA.Size() < MinimumGoalkeeperDistance &&
		!FromGoalkeeperToA.IsNearlyZero()
		)
	{
		CandidateA = GoalkeeperLocation +
			FromGoalkeeperToA.GetSafeNormal() *
			MinimumGoalkeeperDistance;
		CandidateA.Z = CharacterLocation.Z;
	}

	FVector FromGoalkeeperToB = CandidateB - GoalkeeperLocation;
	FromGoalkeeperToB.Z = 0.0f;

	if (
		FromGoalkeeperToB.Size() < MinimumGoalkeeperDistance &&
		!FromGoalkeeperToB.IsNearlyZero()
		)
	{
		CandidateB = GoalkeeperLocation +
			FromGoalkeeperToB.GetSafeNormal() *
			MinimumGoalkeeperDistance;
		CandidateB.Z = CharacterLocation.Z;
	}

	float ScoreA = FVector::Dist2D(CandidateA, DesiredLocation);
	float ScoreB = FVector::Dist2D(CandidateB, DesiredLocation);

	if (
		IsLocationInsideGoalkeeperMobilityCorridor2D(
			CandidateA,
			GoalkeeperLocation,
			ThreatLocation,
			CorridorHalfWidth
		)
		)
	{
		ScoreA += 100000.0f;
	}

	if (
		IsLocationInsideGoalkeeperMobilityCorridor2D(
			CandidateB,
			GoalkeeperLocation,
			ThreatLocation,
			CorridorHalfWidth
		)
		)
	{
		ScoreB += 100000.0f;
	}

	return ScoreA <= ScoreB ? CandidateA : CandidateB;
}


bool ASoccerMatchManager::TryBuildGoalAreaAttackerRespectContext(
	const ASoccerAICharacter* SoccerAICharacter,
	ASoccerAICharacter*& OutGoalkeeper,
	FVector& OutBallLocation,
	FVector& OutOpponentGoalLocation,
	FVector& OutFieldOutwardDirection,
	FVector& OutRightDirection,
	bool& bOutGoalkeeperHoldingBall,
	bool& bOutGoalkeeperActionActive
) const
{
	OutGoalkeeper = nullptr;
	OutBallLocation = FVector::ZeroVector;
	OutOpponentGoalLocation = FVector::ZeroVector;
	OutFieldOutwardDirection = FVector::ZeroVector;
	OutRightDirection = FVector::ZeroVector;
	bOutGoalkeeperHoldingBall = false;
	bOutGoalkeeperActionActive = false;

	if (
		!bUseGoalAreaAttackerRespect ||
		!IsValid(SoccerAICharacter) ||
		SoccerAICharacter->GetPlayerRole() ==
			ESoccerPlayerRole::Goalkeeper ||
		!IsValid(SoccerBall)
		)
	{
		return false;
	}

	const ESoccerTeam GoalkeeperTeam =
		GetOppositeTeam(SoccerAICharacter->GetTeam());

	ASoccerAICharacter* Goalkeeper =
		FindGoalkeeperForTeam(GoalkeeperTeam);

	if (!IsValid(Goalkeeper))
	{
		return false;
	}

	const FVector OpponentGoalLocation =
		GetOwnGoalReferenceLocation(GoalkeeperTeam);

	const FVector OppositeFieldGoalLocation =
		GetOpponentGoalReferenceLocation(GoalkeeperTeam);

	FVector FieldOutwardDirection =
		OppositeFieldGoalLocation - OpponentGoalLocation;
	FieldOutwardDirection.Z = 0.0f;
	FieldOutwardDirection = FieldOutwardDirection.GetSafeNormal();

	const FVector RightDirection =
		GetFieldRightDirectionForTeam(GoalkeeperTeam);

	if (
		FieldOutwardDirection.IsNearlyZero() ||
		RightDirection.IsNearlyZero()
		)
	{
		return false;
	}

	const bool bGoalkeeperHoldingBall =
		PossessingCharacter == Goalkeeper &&
		Goalkeeper->IsGoalkeeperHoldingBall();

	const ASoccerAIController* GoalkeeperController =
		Cast<ASoccerAIController>(Goalkeeper->GetController());

	const bool bGoalkeeperActionActive =
		Goalkeeper->IsGoalkeeperActionActive() ||
		(
			IsValid(GoalkeeperController) &&
			GoalkeeperController->IsGoalkeeperActivelyClaimingBall()
		);

	if (!bGoalkeeperHoldingBall && !bGoalkeeperActionActive)
	{
		return false;
	}

	const FVector BallLocation = SoccerBall->GetActorLocation();
	const float ActivationMargin = FMath::Max(
		0.0f,
		GoalAreaAttackerRespectActivationMargin
	);

	const bool bBallInsideRespectArea =
		IsLocationInsideGoalAreaForTeam(
			BallLocation,
			GoalkeeperTeam,
			ActivationMargin
		);

	const bool bGoalkeeperInsideRespectArea =
		IsLocationInsideGoalAreaForTeam(
			Goalkeeper->GetActorLocation(),
			GoalkeeperTeam,
			ActivationMargin
		);

	if (!bBallInsideRespectArea && !bGoalkeeperInsideRespectArea)
	{
		return false;
	}

	const bool bCharacterInsideRespectArea =
		IsLocationInsideGoalAreaForTeam(
			SoccerAICharacter->GetActorLocation(),
			GoalkeeperTeam,
			ActivationMargin
		);

	if (
		!bCharacterInsideRespectArea &&
		FVector::Dist2D(
			SoccerAICharacter->GetActorLocation(),
			OpponentGoalLocation
		) > FMath::Max(
			50.0f,
			GoalAreaAttackerRespectParticipationDistance
		)
		)
	{
		return false;
	}

	OutGoalkeeper = Goalkeeper;
	OutBallLocation = BallLocation;
	OutOpponentGoalLocation = OpponentGoalLocation;
	OutFieldOutwardDirection = FieldOutwardDirection;
	OutRightDirection = RightDirection;
	bOutGoalkeeperHoldingBall = bGoalkeeperHoldingBall;
	bOutGoalkeeperActionActive = bGoalkeeperActionActive;
	return true;
}

FVector ASoccerMatchManager::BuildGoalAreaAttackerHoldingWaitLocation(
	const ASoccerAICharacter* SoccerAICharacter,
	const ASoccerAICharacter* Goalkeeper,
	const FVector& OpponentGoalLocation,
	const FVector& FieldOutwardDirection,
	const FVector& RightDirection
) const
{
	if (!IsValid(SoccerAICharacter) || !IsValid(Goalkeeper))
	{
		return FVector::ZeroVector;
	}

	const FVector CharacterLocation =
		SoccerAICharacter->GetActorLocation();

	const float CurrentDepth = FVector::DotProduct(
		CharacterLocation - OpponentGoalLocation,
		FieldOutwardDirection
	);

	const float MinimumWaitDepth =
		SoccerFieldDimensions::GoalAreaDepthCm +
		FMath::Max(0.0f, GoalAreaAttackerHoldingWaitOutsideDepth);

	const float DesiredDepth = FMath::Max(
		MinimumWaitDepth,
		CurrentDepth
	);

	const float CurrentLateral = FVector::DotProduct(
		CharacterLocation - OpponentGoalLocation,
		RightDirection
	);

	const int32 SpreadBucket =
		static_cast<int32>(SoccerAICharacter->GetUniqueID() % 3) - 1;

	// Un atacante que ya esta abierto no debe cerrarse hacia el arquero
	// solo porque este aseguro la pelota. Conservamos casi todo el ancho
	// disponible del campo y solamente separamos a los jugadores cercanos.
	const float MaxLateral = FMath::Max(
		SoccerFieldDimensions::GoalAreaHalfWidthCm,
		SoccerFieldDimensions::HalfPitchWidthCm - 100.0f
	);

	const float DesiredLateral = FMath::Clamp(
		CurrentLateral +
		static_cast<float>(SpreadBucket) *
		FMath::Max(0.0f, GoalAreaAttackerHoldingLateralSpread),
		-MaxLateral,
		MaxLateral
	);

	FVector DesiredLocation =
		OpponentGoalLocation +
		FieldOutwardDirection * DesiredDepth +
		RightDirection * DesiredLateral;

	FVector AwayFromGoalkeeper =
		DesiredLocation - Goalkeeper->GetActorLocation();
	AwayFromGoalkeeper.Z = 0.0f;

	const float MinimumGoalkeeperDistance = FMath::Max(
		50.0f,
		GoalAreaAttackerHoldingMinGoalkeeperDistance
	);

	if (AwayFromGoalkeeper.Size() < MinimumGoalkeeperDistance)
	{
		if (!AwayFromGoalkeeper.Normalize())
		{
			AwayFromGoalkeeper = FieldOutwardDirection;
		}

		DesiredLocation =
			Goalkeeper->GetActorLocation() +
			AwayFromGoalkeeper * MinimumGoalkeeperDistance;
	}

	DesiredLocation.Z = CharacterLocation.Z;
	return ProjectLocationToNavigation(
		DesiredLocation,
		SoccerAICharacter
	);
}

FVector ASoccerMatchManager::BuildGoalAreaAttackerSideApproachLocation(
	const ASoccerAICharacter* SoccerAICharacter,
	const ASoccerAICharacter* Goalkeeper,
	const FVector& BallLocation,
	const FVector& OpponentGoalLocation,
	const FVector& FieldOutwardDirection,
	const FVector& RightDirection
) const
{
	if (!IsValid(SoccerAICharacter) || !IsValid(Goalkeeper))
	{
		return FVector::ZeroVector;
	}

	FVector CorridorDirection =
		BallLocation - Goalkeeper->GetActorLocation();
	CorridorDirection.Z = 0.0f;

	if (!CorridorDirection.Normalize())
	{
		CorridorDirection = FieldOutwardDirection;
	}

	FVector CorridorNormal(
		-CorridorDirection.Y,
		CorridorDirection.X,
		0.0f
	);
	CorridorNormal = CorridorNormal.GetSafeNormal();

	float SideDot = FVector::DotProduct(
		SoccerAICharacter->GetActorLocation() -
		Goalkeeper->GetActorLocation(),
		CorridorNormal
	);

	float SideSign = SideDot >= 0.0f ? 1.0f : -1.0f;

	if (FMath::Abs(SideDot) < 25.0f)
	{
		SideSign = SoccerAICharacter->GetUniqueID() % 2 == 0
			? 1.0f
			: -1.0f;
	}

	FVector DesiredLocation =
		BallLocation +
		CorridorNormal * SideSign *
		FMath::Max(30.0f, GoalAreaAttackerApproachLateralOffset) +
		FieldOutwardDirection *
		FMath::Max(0.0f, GoalAreaAttackerApproachFieldOffset);

	const float MaxDepth =
		SoccerFieldDimensions::GoalAreaDepthCm +
		FMath::Max(
			GoalAreaAttackerRespectActivationMargin + 180.0f,
			GoalAreaAttackerApproachFieldOffset + 120.0f
		);

	const float DesiredDepth = FMath::Clamp(
		FVector::DotProduct(
			DesiredLocation - OpponentGoalLocation,
			FieldOutwardDirection
		),
		35.0f,
		MaxDepth
	);

	const float MaxLateral =
		SoccerFieldDimensions::GoalAreaHalfWidthCm +
		FMath::Max(0.0f, GoalAreaAttackerMaxLateralBeyondArea);

	const float DesiredLateral = FMath::Clamp(
		FVector::DotProduct(
			DesiredLocation - OpponentGoalLocation,
			RightDirection
		),
		-MaxLateral,
		MaxLateral
	);

	DesiredLocation =
		OpponentGoalLocation +
		FieldOutwardDirection * DesiredDepth +
		RightDirection * DesiredLateral;

	FVector AwayFromGoalkeeper =
		DesiredLocation - Goalkeeper->GetActorLocation();
	AwayFromGoalkeeper.Z = 0.0f;

	const float MinimumGoalkeeperDistance = FMath::Max(
		40.0f,
		GoalAreaAttackerMinDistanceFromGoalkeeper
	);

	if (AwayFromGoalkeeper.Size() < MinimumGoalkeeperDistance)
	{
		if (!AwayFromGoalkeeper.Normalize())
		{
			AwayFromGoalkeeper = CorridorNormal * SideSign;
		}

		DesiredLocation =
			Goalkeeper->GetActorLocation() +
			AwayFromGoalkeeper * MinimumGoalkeeperDistance;
	}

	DesiredLocation.Z = SoccerAICharacter->GetActorLocation().Z;
	return ProjectLocationToNavigation(
		DesiredLocation,
		SoccerAICharacter
	);
}

FVector ASoccerMatchManager::MoveAttackLocationOutsideGoalkeeperCorridor(
	const ASoccerAICharacter* SoccerAICharacter,
	const ASoccerAICharacter* Goalkeeper,
	const FVector& BallLocation,
	const FVector& OpponentGoalLocation,
	const FVector& FieldOutwardDirection,
	const FVector& RightDirection,
	const FVector& DesiredLocation
) const
{
	if (!IsValid(SoccerAICharacter) || !IsValid(Goalkeeper))
	{
		return DesiredLocation;
	}

	const FVector CharacterLocation =
		SoccerAICharacter->GetActorLocation();
	const FVector GoalkeeperLocation =
		Goalkeeper->GetActorLocation();

	const float CorridorHalfWidth = FMath::Max(
		30.0f,
		GoalAreaAttackerCorridorHalfWidth
	);

	const bool bCharacterInsideCorridor =
		IsLocationInsideGoalkeeperMobilityCorridor2D(
			CharacterLocation,
			GoalkeeperLocation,
			BallLocation,
			CorridorHalfWidth
		);

	const bool bDesiredInsideCorridor =
		IsLocationInsideGoalkeeperMobilityCorridor2D(
			DesiredLocation,
			GoalkeeperLocation,
			BallLocation,
			CorridorHalfWidth
		);

	const float MinimumGoalkeeperDistance = FMath::Max(
		40.0f,
		GoalAreaAttackerMinDistanceFromGoalkeeper
	);

	const bool bCharacterTooClose =
		FVector::Dist2D(CharacterLocation, GoalkeeperLocation) <
		MinimumGoalkeeperDistance;

	const bool bDesiredTooClose =
		FVector::Dist2D(DesiredLocation, GoalkeeperLocation) <
		MinimumGoalkeeperDistance;

	if (
		!bCharacterInsideCorridor &&
		!bDesiredInsideCorridor &&
		!bCharacterTooClose &&
		!bDesiredTooClose
		)
	{
		return DesiredLocation;
	}

	FVector CorridorDirection = BallLocation - GoalkeeperLocation;
	CorridorDirection.Z = 0.0f;

	if (!CorridorDirection.Normalize())
	{
		CorridorDirection = FieldOutwardDirection;
	}

	FVector CorridorNormal(
		-CorridorDirection.Y,
		CorridorDirection.X,
		0.0f
	);
	CorridorNormal = CorridorNormal.GetSafeNormal();

	const FVector ReferenceLocation =
		bCharacterInsideCorridor || bCharacterTooClose
		? CharacterLocation
		: DesiredLocation;

	const float ReferenceSideDot = FVector::DotProduct(
		ReferenceLocation - GoalkeeperLocation,
		CorridorNormal
	);

	float PreferredSideSign =
		ReferenceSideDot >= 0.0f ? 1.0f : -1.0f;

	if (FMath::Abs(ReferenceSideDot) < 20.0f)
	{
		PreferredSideSign =
			SoccerAICharacter->GetUniqueID() % 2 == 0
			? 1.0f
			: -1.0f;
	}

	const float RequiredOffset =
		CorridorHalfWidth +
		FMath::Max(0.0f, GoalAreaAttackerCorridorClearance);

	FVector CandidateA =
		DesiredLocation + CorridorNormal * RequiredOffset;
	FVector CandidateB =
		DesiredLocation - CorridorNormal * RequiredOffset;

	const float MaxDepth =
		SoccerFieldDimensions::GoalAreaDepthCm +
		FMath::Max(
			GoalAreaAttackerRespectActivationMargin + 180.0f,
			220.0f
		);

	const float MaxLateral =
		SoccerFieldDimensions::GoalAreaHalfWidthCm +
		FMath::Max(0.0f, GoalAreaAttackerMaxLateralBeyondArea);

	auto ClampCandidate = [&](FVector Candidate)
	{
		const float Depth = FMath::Clamp(
			FVector::DotProduct(
				Candidate - OpponentGoalLocation,
				FieldOutwardDirection
			),
			35.0f,
			MaxDepth
		);

		const float Lateral = FMath::Clamp(
			FVector::DotProduct(
				Candidate - OpponentGoalLocation,
				RightDirection
			),
			-MaxLateral,
			MaxLateral
		);

		Candidate =
			OpponentGoalLocation +
			FieldOutwardDirection * Depth +
			RightDirection * Lateral;

		FVector AwayFromGoalkeeper = Candidate - GoalkeeperLocation;
		AwayFromGoalkeeper.Z = 0.0f;

		if (AwayFromGoalkeeper.Size() < MinimumGoalkeeperDistance)
		{
			if (!AwayFromGoalkeeper.Normalize())
			{
				AwayFromGoalkeeper = FieldOutwardDirection;
			}

			Candidate =
				GoalkeeperLocation +
				AwayFromGoalkeeper * MinimumGoalkeeperDistance;
		}

		Candidate.Z = CharacterLocation.Z;
		return Candidate;
	};

	CandidateA = ClampCandidate(CandidateA);
	CandidateB = ClampCandidate(CandidateB);

	float ScoreA = FVector::Dist2D(CandidateA, DesiredLocation);
	float ScoreB = FVector::Dist2D(CandidateB, DesiredLocation);

	const float CandidateASide = FVector::DotProduct(
		CandidateA - GoalkeeperLocation,
		CorridorNormal
	);
	const float CandidateBSide = FVector::DotProduct(
		CandidateB - GoalkeeperLocation,
		CorridorNormal
	);

	if (CandidateASide * PreferredSideSign < 0.0f)
	{
		ScoreA += 90.0f;
	}

	if (CandidateBSide * PreferredSideSign < 0.0f)
	{
		ScoreB += 90.0f;
	}

	if (
		IsLocationInsideGoalkeeperMobilityCorridor2D(
			CandidateA,
			GoalkeeperLocation,
			BallLocation,
			CorridorHalfWidth
		)
		)
	{
		ScoreA += 100000.0f;
	}

	if (
		IsLocationInsideGoalkeeperMobilityCorridor2D(
			CandidateB,
			GoalkeeperLocation,
			BallLocation,
			CorridorHalfWidth
		)
		)
	{
		ScoreB += 100000.0f;
	}

	return ProjectLocationToNavigation(
		ScoreA <= ScoreB ? CandidateA : CandidateB,
		SoccerAICharacter
	);
}

FVector ASoccerMatchManager::AdjustAttackMoveLocationForGoalAreaRespect(
	const ASoccerAICharacter* SoccerAICharacter,
	const FVector& DesiredLocation
) const
{
	ASoccerAICharacter* Goalkeeper = nullptr;
	FVector BallLocation = FVector::ZeroVector;
	FVector OpponentGoalLocation = FVector::ZeroVector;
	FVector FieldOutwardDirection = FVector::ZeroVector;
	FVector RightDirection = FVector::ZeroVector;
	bool bGoalkeeperHoldingBall = false;
	bool bGoalkeeperActionActive = false;

	if (
		!TryBuildGoalAreaAttackerRespectContext(
			SoccerAICharacter,
			Goalkeeper,
			BallLocation,
			OpponentGoalLocation,
			FieldOutwardDirection,
			RightDirection,
			bGoalkeeperHoldingBall,
			bGoalkeeperActionActive
		) ||
		bGoalkeeperHoldingBall ||
		!bGoalkeeperActionActive ||
		!IsTeamCurrentlyAttacking(SoccerAICharacter->GetTeam())
		)
	{
		return DesiredLocation;
	}

	return MoveAttackLocationOutsideGoalkeeperCorridor(
		SoccerAICharacter,
		Goalkeeper,
		BallLocation,
		OpponentGoalLocation,
		FieldOutwardDirection,
		RightDirection,
		DesiredLocation
	);
}

FVector ASoccerMatchManager::BuildDefendProtectGoalLaneLocation(
	const ASoccerAICharacter* SoccerAICharacter
) const
{
	if (SoccerAICharacter == nullptr)
	{
		return FVector::ZeroVector;
	}

	const ESoccerTeam Team =
		SoccerAICharacter->GetTeam();

	const FVector OwnGoalLocation =
		GetOwnGoalReferenceLocation(Team);

	const FVector OpponentGoalLocation =
		GetOpponentGoalReferenceLocation(Team);

	FVector TeamAttackDirection =
		OpponentGoalLocation - OwnGoalLocation;

	TeamAttackDirection.Z = 0.0f;

	const float FieldLength =
		TeamAttackDirection.Size();

	if (FieldLength <= 1.0f)
	{
		return BuildDynamicTeamShapeLocation(
			Team,
			SoccerAICharacter
		);
	}

	TeamAttackDirection =
		TeamAttackDirection / FieldLength;

	const FVector RightDirection =
		GetFieldRightDirectionForTeam(Team);

	if (RightDirection.IsNearlyZero())
	{
		return BuildDynamicTeamShapeLocation(
			Team,
			SoccerAICharacter
		);
	}

	const FVector ThreatLocation =
		GetCurrentDefensiveThreatLocation(Team);

	if (ThreatLocation.IsNearlyZero())
	{
		return BuildDynamicTeamShapeLocation(
			Team,
			SoccerAICharacter
		);
	}

	FVector DirectionFromGoalToThreat =
		ThreatLocation - OwnGoalLocation;

	DirectionFromGoalToThreat.Z = 0.0f;

	const float DistanceFromGoalToThreat =
		DirectionFromGoalToThreat.Size();

	if (DistanceFromGoalToThreat <= 1.0f)
	{
		return BuildDynamicTeamShapeLocation(
			Team,
			SoccerAICharacter
		);
	}

	DirectionFromGoalToThreat =
		DirectionFromGoalToThreat / DistanceFromGoalToThreat;

	const float ThreatDepthAlpha =
		GetAttackDepthAlphaForLocation(
			ThreatLocation,
			Team
		);

	const float DesiredBlockDistanceFromGoal =
		FMath::Lerp(
			DefensiveGoalLaneBlockDistanceNearGoal,
			DefensiveGoalLaneBlockDistanceMidfield,
			ThreatDepthAlpha
		);

	float ClampedBlockDistanceFromGoal =
		FMath::Clamp(
			DesiredBlockDistanceFromGoal,
			DefensiveProtectGoalLaneMinDistanceFromGoal,
			DefensiveProtectGoalLaneMaxDistanceFromGoal
		);

	// Si la amenaza está demasiado cerca, el bloqueador no puede quedar
	// detrás de la pelota ni pasarse del portador.
	ClampedBlockDistanceFromGoal =
		FMath::Min(
			ClampedBlockDistanceFromGoal,
			DistanceFromGoalToThreat * 0.72f
		);

	FVector DesiredLocation =
		OwnGoalLocation +
		DirectionFromGoalToThreat * ClampedBlockDistanceFromGoal;

	// Pequeño corrimiento lateral hacia el lado de la pelota.
	// No rompe la línea principal, pero evita que quede demasiado rígido.
	const float ThreatLateralOffset =
		GetFieldLateralOffsetForTeam(
			ThreatLocation,
			Team
		);

	DesiredLocation +=
		RightDirection *
		ThreatLateralOffset *
		DefensiveGoalLaneBallSideLateralInfluence;

	const float DesiredDepth =
		FVector::DotProduct(
			DesiredLocation - OwnGoalLocation,
			TeamAttackDirection
		);

	const float DesiredLateral =
		FVector::DotProduct(
			DesiredLocation - OwnGoalLocation,
			RightDirection
		);

	const float ClampedDepth =
		FMath::Clamp(
			DesiredDepth,
			220.0f,
			FieldLength * 0.62f
		);

	const float ScaledShapeMaxLateralOffset =
		FMath::Min(
			SoccerFieldDimensions::ScaleAuthoredLateralDistance(
				AttackShapeMaxLateralOffset
			),
			FMath::Max(0.0f, SoccerFieldDimensions::HalfPitchWidthCm - 120.0f)
		);

	const float ClampedLateral =
		FMath::Clamp(
			DesiredLateral,
			-ScaledShapeMaxLateralOffset,
			ScaledShapeMaxLateralOffset
		);

	DesiredLocation =
		OwnGoalLocation +
		TeamAttackDirection * ClampedDepth +
		RightDirection * ClampedLateral;

	DesiredLocation.Z =
		SoccerAICharacter->GetActorLocation().Z;

	return ProjectLocationToNavigation(
		DesiredLocation,
		SoccerAICharacter
	);
}

FVector ASoccerMatchManager::BuildDefendCoverCenterLocation(
	const ASoccerAICharacter* SoccerAICharacter
) const
{
	if (SoccerAICharacter == nullptr)
	{
		return FVector::ZeroVector;
	}

	const ESoccerTeam Team =
		SoccerAICharacter->GetTeam();

	const FVector OwnGoalLocation =
		GetOwnGoalReferenceLocation(Team);

	const FVector OpponentGoalLocation =
		GetOpponentGoalReferenceLocation(Team);

	FVector AttackDirection =
		OpponentGoalLocation - OwnGoalLocation;

	AttackDirection.Z = 0.0f;

	const float FieldLength =
		AttackDirection.Size();

	if (FieldLength <= 1.0f)
	{
		return BuildDynamicTeamShapeLocation(
			Team,
			SoccerAICharacter
		);
	}

	AttackDirection =
		AttackDirection / FieldLength;

	const FVector RightDirection =
		GetFieldRightDirectionForTeam(Team);

	if (RightDirection.IsNearlyZero())
	{
		return BuildDynamicTeamShapeLocation(
			Team,
			SoccerAICharacter
		);
	}

	const FVector BallLocation =
		IsValid(SoccerBall)
		? SoccerBall->GetActorLocation()
		: SoccerAICharacter->GetActorLocation();

	const float BallDepth =
		FVector::DotProduct(
			BallLocation - OwnGoalLocation,
			AttackDirection
		);

	const float BallDepthAlpha =
		FMath::Clamp(
			BallDepth / FieldLength,
			0.0f,
			1.0f
		);

	const float DesiredDepthAlpha =
		FMath::Lerp(
			DefensiveCoverCenterNearGoalDepthAlpha,
			DefensiveCoverCenterMidfieldDepthAlpha,
			BallDepthAlpha
		);

	const float BallLateralOffset =
		GetFieldLateralOffsetForTeam(
			BallLocation,
			Team
		);

	const float ScaledDefensiveCoverCenterMaxLateralOffset =
		SoccerFieldDimensions::ScaleAuthoredLateralDistance(
			DefensiveCoverCenterMaxLateralOffset
		);

	const float DesiredLateralOffset =
		FMath::Clamp(
			BallLateralOffset * DefensiveCoverCenterBallSideShiftAlpha,
			-ScaledDefensiveCoverCenterMaxLateralOffset,
			ScaledDefensiveCoverCenterMaxLateralOffset
		);

	FVector DesiredLocation =
		OwnGoalLocation +
		AttackDirection * DesiredDepthAlpha * FieldLength +
		RightDirection * DesiredLateralOffset;

	DesiredLocation.Z =
		SoccerAICharacter->GetActorLocation().Z;

	return ProjectLocationToNavigation(
		DesiredLocation,
		SoccerAICharacter
	);
}

FVector ASoccerMatchManager::BuildDefendCompactShapeLocation(
	const ASoccerAICharacter* SoccerAICharacter
) const
{
	if (SoccerAICharacter == nullptr)
	{
		return FVector::ZeroVector;
	}

	return BuildDynamicTeamShapeLocation(
		SoccerAICharacter->GetTeam(),
		SoccerAICharacter
	);
}

FVector ASoccerMatchManager::GetCurrentDefensiveThreatLocation(
	ESoccerTeam DefendingTeam
) const
{
	if (
		IsValid(PossessingCharacter) &&
		PossessingCharacter->GetTeam() != DefendingTeam
		)
	{
		return PossessingCharacter->GetActorLocation();
	}

	if (
		IsValid(LastTouchCharacter) &&
		LastTouchCharacter->GetTeam() != DefendingTeam
		)
	{
		return LastTouchCharacter->GetActorLocation();
	}

	if (IsValid(SoccerBall))
	{
		return SoccerBall->GetActorLocation();
	}

	return FVector::ZeroVector;
}

bool ASoccerMatchManager::HasDefensiveCoverBehindCandidate(
	const ASoccerAICharacter* Candidate,
	float MinBehindDepthAlphaGap
) const
{
	if (!IsValid(Candidate))
	{
		return false;
	}

	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return false;
	}

	const ESoccerTeam Team =
		Candidate->GetTeam();

	const float CandidateDepthAlpha =
		GetAttackDepthAlphaForLocation(
			Candidate->GetActorLocation(),
			Team
		);

	for (TActorIterator<ASoccerAICharacter> It(World); It; ++It)
	{
		const ASoccerAICharacter* Teammate = *It;

		if (!IsValid(Teammate))
		{
			continue;
		}

		if (Teammate == Candidate)
		{
			continue;
		}

		if (Teammate->GetTeam() != Team)
		{
			continue;
		}

		if (Teammate->GetPlayerRole() == ESoccerPlayerRole::Goalkeeper)
		{
			continue;
		}

		const float TeammateDepthAlpha =
			GetAttackDepthAlphaForLocation(
				Teammate->GetActorLocation(),
				Team
			);

		if (
			TeammateDepthAlpha <=
			CandidateDepthAlpha - MinBehindDepthAlphaGap
			)
		{
			return true;
		}
	}

	return false;
}

float ASoccerMatchManager::ScoreDefensivePressureCandidate(
	ESoccerTeam DefendingTeam,
	const ASoccerAICharacter* Candidate,
	const FVector& ThreatLocation
) const
{
	if (!IsValid(Candidate))
	{
		return -TNumericLimits<float>::Max();
	}

	if (Candidate->GetTeam() != DefendingTeam)
	{
		return -TNumericLimits<float>::Max();
	}

	const ESoccerPlayerRole PlayerRole =
		Candidate->GetPlayerRole();

	if (PlayerRole == ESoccerPlayerRole::Goalkeeper)
	{
		return -TNumericLimits<float>::Max();
	}

	if (!CanCharacterTouchBallNow(Candidate))
	{
		return -TNumericLimits<float>::Max();
	}

	const float DistanceToThreat =
		FVector::Dist2D(
			Candidate->GetActorLocation(),
			ThreatLocation
		);

	float Score =
		5000.0f -
		DistanceToThreat * DefensivePressureDistanceWeight;

	const ASoccerAICharacter* CurrentPressureAI =
		DefendingTeam == ESoccerTeam::PlayerTeam
		? PlayerTeamPressureAI
		: OpponentTeamPressureAI;

	if (Candidate == CurrentPressureAI)
	{
		Score += DefensivePressureCurrentRoleBonus;
	}

	const float CandidateDepthAlpha =
		GetAttackDepthAlphaForLocation(
			Candidate->GetActorLocation(),
			DefendingTeam
		);

	const float ThreatDepthAlpha =
		GetAttackDepthAlphaForLocation(
			ThreatLocation,
			DefendingTeam
		);

	const bool bThreatNearOwnGoal =
		ThreatDepthAlpha <= DefensivePressureNearOwnGoalDepthAlpha;

	const bool bThreatFarFromOwnGoal =
		ThreatDepthAlpha >= DefensivePressureFarFromOwnGoalDepthAlpha;

	const bool bCandidateIsGoalSide =
		CandidateDepthAlpha <= ThreatDepthAlpha + 0.03f;

	if (!bCandidateIsGoalSide)
	{
		Score -= DefensivePressureWrongSidePenalty;
	}

	if (PlayerRole == ESoccerPlayerRole::Forward)
	{
		if (bThreatFarFromOwnGoal)
		{
			Score += 850.0f;
		}
		else if (bThreatNearOwnGoal)
		{
			Score -= 650.0f;
		}
		else
		{
			Score += 250.0f;
		}
	}
	else if (PlayerRole == ESoccerPlayerRole::Midfielder)
	{
		Score += 700.0f;

		if (bThreatNearOwnGoal)
		{
			Score += 150.0f;
		}
	}
	else if (PlayerRole == ESoccerPlayerRole::Defender)
	{
		if (bThreatNearOwnGoal)
		{
			Score += 650.0f;
		}
		else
		{
			Score += 150.0f;
		}

		const bool bHasCoverBehind =
			HasDefensiveCoverBehindCandidate(
				Candidate,
				DefensivePressureCoverBehindDepthGapAlpha
			);

		if (!bHasCoverBehind)
		{
			Score -= DefensivePressureNoCoverPenalty;
		}

		if (bThreatFarFromOwnGoal)
		{
			Score -= DefensivePressureDeepDefenderPenalty;
		}
	}

	FSoccerSlotTacticalInstruction IndividualInstruction;

	if (
		TryGetOpenPlayIndividualInstructionForCharacter(
			Candidate,
			IndividualInstruction
		)
		)
	{
		switch (IndividualInstruction.DefensiveInstruction)
		{
		case ESoccerIndividualDefensiveInstruction::PressBall:
			Score += IndividualPreferredRoleScoreBonus;
			break;

		case ESoccerIndividualDefensiveInstruction::Cover:
		case ESoccerIndividualDefensiveInstruction::MarkTightly:
			Score -= IndividualSecondaryRoleScoreBonus;
			break;

		case ESoccerIndividualDefensiveInstruction::HoldPosition:
		case ESoccerIndividualDefensiveInstruction::ProtectCenter:
			Score -= IndividualAvoidRoleScorePenalty;
			break;

		default:
			break;
		}
	}

	return Score;
}

ASoccerAICharacter* ASoccerMatchManager::FindBestDefensivePressureAI(
	ESoccerTeam DefendingTeam
) const
{
	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return nullptr;
	}

	const FVector ThreatLocation =
		GetCurrentDefensiveThreatLocation(DefendingTeam);

	ASoccerAICharacter* CurrentPressureAI =
		DefendingTeam == ESoccerTeam::PlayerTeam
		? PlayerTeamPressureAI
		: OpponentTeamPressureAI;

	if (
		IsValid(CurrentPressureAI) &&
		CurrentPressureAI->GetTeam() == DefendingTeam &&
		CurrentPressureAI->GetPlayerRole() != ESoccerPlayerRole::Goalkeeper &&
		CanCharacterTouchBallNow(CurrentPressureAI) &&
		FVector::Dist2D(
			CurrentPressureAI->GetActorLocation(),
			ThreatLocation
		) <= FMath::Max(0.0f, DefensivePressureCommitDistance)
		)
	{
		return CurrentPressureAI;
	}

	ASoccerAICharacter* BestCharacter = nullptr;
	float BestScore = -TNumericLimits<float>::Max();

	for (TActorIterator<ASoccerAICharacter> It(World); It; ++It)
	{
		ASoccerAICharacter* Candidate = *It;

		if (!IsValid(Candidate))
		{
			continue;
		}

		if (Candidate->GetTeam() != DefendingTeam)
		{
			continue;
		}

		if (Candidate->GetPlayerRole() == ESoccerPlayerRole::Goalkeeper)
		{
			continue;
		}

		const float CandidateScore =
			ScoreDefensivePressureCandidate(
				DefendingTeam,
				Candidate,
				ThreatLocation
			);

		if (CandidateScore > BestScore)
		{
			BestScore = CandidateScore;
			BestCharacter = Candidate;
		}
	}

	return BestCharacter;
}

float ASoccerMatchManager::ScoreDefensiveGoalLaneCandidate(
	ESoccerTeam DefendingTeam,
	const ASoccerAICharacter* Candidate,
	const FVector& ThreatLocation,
	const ASoccerAICharacter* CurrentPressureAI
) const
{
	if (!IsValid(Candidate))
	{
		return -TNumericLimits<float>::Max();
	}

	if (Candidate->GetTeam() != DefendingTeam)
	{
		return -TNumericLimits<float>::Max();
	}

	const ESoccerPlayerRole PlayerRole =
		Candidate->GetPlayerRole();

	if (PlayerRole == ESoccerPlayerRole::Goalkeeper)
	{
		return -TNumericLimits<float>::Max();
	}

	const FVector OwnGoalLocation =
		GetOwnGoalReferenceLocation(DefendingTeam);

	const float CandidateDistanceToThreat =
		FVector::Dist2D(
			Candidate->GetActorLocation(),
			ThreatLocation
		);

	float Score =
		4000.0f -
		CandidateDistanceToThreat * DefensiveGoalLaneDistanceWeight;

	const ASoccerAICharacter* CurrentGoalLaneAI =
		DefendingTeam == ESoccerTeam::PlayerTeam
		? PlayerTeamSupportAI
		: OpponentTeamSupportAI;

	if (Candidate == CurrentGoalLaneAI)
	{
		Score += DefensiveGoalLaneCurrentRoleBonus;
	}

	const float CandidateDepthAlpha =
		GetAttackDepthAlphaForLocation(
			Candidate->GetActorLocation(),
			DefendingTeam
		);

	const float ThreatDepthAlpha =
		GetAttackDepthAlphaForLocation(
			ThreatLocation,
			DefendingTeam
		);

	const bool bCandidateIsGoalSide =
		CandidateDepthAlpha <= ThreatDepthAlpha + 0.03f;

	if (!bCandidateIsGoalSide)
	{
		Score -= DefensiveGoalLaneWrongSidePenalty;
	}

	if (PlayerRole == ESoccerPlayerRole::Defender)
	{
		Score += DefensiveGoalLaneDefenderBonus;
	}
	else if (PlayerRole == ESoccerPlayerRole::Midfielder)
	{
		Score += DefensiveGoalLaneMidfielderBonus;
	}
	else if (PlayerRole == ESoccerPlayerRole::Forward)
	{
		Score -= DefensiveGoalLaneForwardPenalty;
	}

	if (IsValid(CurrentPressureAI))
	{
		const float DistanceToPressureAI =
			FVector::Dist2D(
				Candidate->GetActorLocation(),
				CurrentPressureAI->GetActorLocation()
			);

		if (DistanceToPressureAI < DefensiveGoalLaneMinDistanceFromPressure)
		{
			Score -= DefensiveGoalLaneTooCloseToPressurePenalty;
		}
	}

	const float DistanceToOwnGoal =
		FVector::Dist2D(
			Candidate->GetActorLocation(),
			OwnGoalLocation
		);

	Score -= DistanceToOwnGoal * 0.10f;

	FSoccerSlotTacticalInstruction IndividualInstruction;

	if (
		TryGetOpenPlayIndividualInstructionForCharacter(
			Candidate,
			IndividualInstruction
		)
		)
	{
		switch (IndividualInstruction.DefensiveInstruction)
		{
		case ESoccerIndividualDefensiveInstruction::ProtectCenter:
			Score += IndividualPreferredRoleScoreBonus;
			break;

		case ESoccerIndividualDefensiveInstruction::Cover:
		case ESoccerIndividualDefensiveInstruction::HoldPosition:
			Score += IndividualSecondaryRoleScoreBonus;
			break;

		case ESoccerIndividualDefensiveInstruction::PressBall:
		case ESoccerIndividualDefensiveInstruction::MarkTightly:
			Score -= IndividualSecondaryRoleScoreBonus;
			break;

		default:
			break;
		}
	}

	return Score;
}


ASoccerAICharacter* ASoccerMatchManager::FindBestDefensiveGoalLaneAI(
	ESoccerTeam DefendingTeam,
	const TArray<const ASoccerAICharacter*>& ExcludedCharacters,
	const ASoccerAICharacter* CurrentPressureAI
) const
{
	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return nullptr;
	}

	const FVector ThreatLocation =
		GetCurrentDefensiveThreatLocation(DefendingTeam);

	ASoccerAICharacter* BestCharacter = nullptr;
	float BestScore = -TNumericLimits<float>::Max();

	for (TActorIterator<ASoccerAICharacter> It(World); It; ++It)
	{
		ASoccerAICharacter* Candidate = *It;

		if (!IsValid(Candidate))
		{
			continue;
		}

		if (Candidate->GetTeam() != DefendingTeam)
		{
			continue;
		}

		if (Candidate->GetPlayerRole() == ESoccerPlayerRole::Goalkeeper)
		{
			continue;
		}

		if (ExcludedCharacters.Contains(Candidate))
		{
			continue;
		}

		const float CandidateScore =
			ScoreDefensiveGoalLaneCandidate(
				DefendingTeam,
				Candidate,
				ThreatLocation,
				CurrentPressureAI
			);

		if (CandidateScore > BestScore)
		{
			BestScore = CandidateScore;
			BestCharacter = Candidate;
		}
	}

	return BestCharacter;
}

float ASoccerMatchManager::ScoreDangerousAttackingReceiver(
	ESoccerTeam DefendingTeam,
	const ASoccerCharacterBase* CandidateReceiver
) const
{
	if (!IsValid(CandidateReceiver))
	{
		return -TNumericLimits<float>::Max();
	}

	const ESoccerTeam AttackingTeam =
		GetOppositeTeam(DefendingTeam);

	if (CandidateReceiver->GetTeam() != AttackingTeam)
	{
		return -TNumericLimits<float>::Max();
	}

	if (CandidateReceiver == PossessingCharacter)
	{
		return -TNumericLimits<float>::Max();
	}

	const ESoccerPlayerRole ReceiverRole =
		CandidateReceiver->GetPlayerRole();

	if (ReceiverRole == ESoccerPlayerRole::Goalkeeper)
	{
		return -TNumericLimits<float>::Max();
	}

	const FVector ReceiverLocation =
		CandidateReceiver->GetActorLocation();

	const float ReceiverDepthAlpha =
		GetAttackDepthAlphaForLocation(
			ReceiverLocation,
			AttackingTeam
		);

	float Score =
		ReceiverDepthAlpha *
		DefensiveDangerousReceiverDepthWeight;

	if (ReceiverRole == ESoccerPlayerRole::Forward)
	{
		Score += DefensiveDangerousReceiverForwardBonus;
	}
	else if (ReceiverRole == ESoccerPlayerRole::Midfielder)
	{
		Score += DefensiveDangerousReceiverMidfielderBonus;
	}

	if (ReceiverDepthAlpha >= DefensiveDangerousReceiverNearGoalDepthAlpha)
	{
		Score += DefensiveDangerousReceiverNearGoalBonus;
	}

	const int32 DefendersNearReceiver =
		CountOpponentsAroundLocation(
			AttackingTeam,
			ReceiverLocation,
			DefensiveDangerousReceiverFreeRadius
		);

	Score -=
		DefendersNearReceiver *
		DefensiveDangerousReceiverDefenderNearPenalty;

	FVector PassSourceLocation =
		GetCurrentDefensiveThreatLocation(DefendingTeam);

	if (PassSourceLocation.IsNearlyZero())
	{
		PassSourceLocation =
			IsValid(SoccerBall)
			? SoccerBall->GetActorLocation()
			: ReceiverLocation;
	}

	const float DistanceFromPassSource =
		FVector::Dist2D(
			PassSourceLocation,
			ReceiverLocation
		);

	if (DistanceFromPassSource < 300.0f)
	{
		Score -= 320.0f;
	}
	else if (DistanceFromPassSource <= 2800.0f)
	{
		Score += 180.0f;
	}
	else
	{
		Score -= 420.0f;
	}

	const bool bPassLaneBlocked =
		IsOpponentBlockingLaneBetweenLocations(
			AttackingTeam,
			PassSourceLocation,
			ReceiverLocation,
			DefensiveDangerousReceiverPassLaneHalfWidth
		);

	if (bPassLaneBlocked)
	{
		Score -= DefensiveDangerousReceiverPassLaneBlockedPenalty;
	}

	return Score;
}

ASoccerCharacterBase* ASoccerMatchManager::FindMostDangerousAttackingReceiver(
	ESoccerTeam DefendingTeam,
	const TArray<const ASoccerCharacterBase*>& ExcludedReceivers
) const
{
	if (
		ShouldApplyCollectiveTacticsToOpenPlay(DefendingTeam) &&
		GetTacticalPlanForTeamInternal(DefendingTeam).MarkingStyle ==
			ESoccerMarkingStyle::Zonal
		)
	{
		// Explicit individual marks were already reserved before this selector.
		// Zonal marking disables only the extra automatic receiver follower.
		return nullptr;
	}

	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return nullptr;
	}

	ASoccerCharacterBase* BestReceiver = nullptr;
	float BestScore = -TNumericLimits<float>::Max();

	for (TActorIterator<ASoccerCharacterBase> It(World); It; ++It)
	{
		ASoccerCharacterBase* CandidateReceiver = *It;

		if (!IsValid(CandidateReceiver))
		{
			continue;
		}

		if (ExcludedReceivers.Contains(CandidateReceiver))
		{
			continue;
		}

		const float CandidateScore =
			ScoreDangerousAttackingReceiver(
				DefendingTeam,
				CandidateReceiver
			);

		if (CandidateScore > BestScore)
		{
			BestScore = CandidateScore;
			BestReceiver = CandidateReceiver;
		}
	}

	if (BestScore < DefensiveDangerousReceiverMinScore)
	{
		return nullptr;
	}

	return BestReceiver;
}

float ASoccerMatchManager::ScoreDefensiveMarkerCandidate(
	ESoccerTeam DefendingTeam,
	const ASoccerAICharacter* CandidateMarker,
	const ASoccerCharacterBase* ReceiverToMark
) const
{
	if (!IsValid(CandidateMarker) || !IsValid(ReceiverToMark))
	{
		return -TNumericLimits<float>::Max();
	}

	if (CandidateMarker->GetTeam() != DefendingTeam)
	{
		return -TNumericLimits<float>::Max();
	}

	if (CandidateMarker->GetPlayerRole() == ESoccerPlayerRole::Goalkeeper)
	{
		return -TNumericLimits<float>::Max();
	}

	const FVector ReceiverLocation =
		ReceiverToMark->GetActorLocation();

	const float DistanceToReceiver =
		FVector::Dist2D(
			CandidateMarker->GetActorLocation(),
			ReceiverLocation
		);

	float Score =
		4000.0f -
		DistanceToReceiver * DefensiveMarkerDistanceWeight;

	const ASoccerAICharacter* CurrentMarkerAI =
		DefendingTeam == ESoccerTeam::PlayerTeam
		? PlayerTeamDefensiveMarkerAI
		: OpponentTeamDefensiveMarkerAI;

	if (CandidateMarker == CurrentMarkerAI)
	{
		Score += DefensiveMarkerCurrentRoleBonus;
	}

	const ESoccerPlayerRole MarkerRole =
		CandidateMarker->GetPlayerRole();

	if (MarkerRole == ESoccerPlayerRole::Defender)
	{
		Score += DefensiveMarkerDefenderBonus;
	}
	else if (MarkerRole == ESoccerPlayerRole::Midfielder)
	{
		Score += DefensiveMarkerMidfielderBonus;
	}
	else if (MarkerRole == ESoccerPlayerRole::Forward)
	{
		Score -= DefensiveMarkerForwardPenalty;
	}

	const float MarkerDepthAlpha =
		GetAttackDepthAlphaForLocation(
			CandidateMarker->GetActorLocation(),
			DefendingTeam
		);

	const float ReceiverDepthAlphaForDefendingTeam =
		GetAttackDepthAlphaForLocation(
			ReceiverLocation,
			DefendingTeam
		);

	const bool bMarkerIsGoalSide =
		MarkerDepthAlpha <= ReceiverDepthAlphaForDefendingTeam + 0.04f;

	if (!bMarkerIsGoalSide)
	{
		Score -= DefensiveMarkerWrongSidePenalty;
	}

	FSoccerSlotTacticalInstruction IndividualInstruction;

	if (
		TryGetOpenPlayIndividualInstructionForCharacter(
			CandidateMarker,
			IndividualInstruction
		)
		)
	{
		switch (IndividualInstruction.DefensiveInstruction)
		{
		case ESoccerIndividualDefensiveInstruction::MarkTightly:
			// With target "Automatic", this instruction means: prefer this
			// player for the legacy dangerous-receiver marking role. Explicit
			// slot targets are coordinated separately before this selector.
			if (IndividualInstruction.MarkingTargetSlotId.IsNone())
			{
				Score += IndividualPreferredRoleScoreBonus;
			}
			break;

		case ESoccerIndividualDefensiveInstruction::Cover:
			Score -= IndividualSecondaryRoleScoreBonus;
			break;

		case ESoccerIndividualDefensiveInstruction::HoldPosition:
		case ESoccerIndividualDefensiveInstruction::ProtectCenter:
		case ESoccerIndividualDefensiveInstruction::PressBall:
			Score -= IndividualAvoidRoleScorePenalty;
			break;

		default:
			break;
		}
	}

	return Score;
}

ASoccerAICharacter* ASoccerMatchManager::FindBestDefensiveMarkerAI(
	ESoccerTeam DefendingTeam,
	const ASoccerCharacterBase* ReceiverToMark,
	const TArray<const ASoccerAICharacter*>& ExcludedCharacters
) const
{
	if (!IsValid(ReceiverToMark))
	{
		return nullptr;
	}

	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return nullptr;
	}

	ASoccerAICharacter* BestMarker = nullptr;
	float BestScore = -TNumericLimits<float>::Max();

	for (TActorIterator<ASoccerAICharacter> It(World); It; ++It)
	{
		ASoccerAICharacter* CandidateMarker = *It;

		if (!IsValid(CandidateMarker))
		{
			continue;
		}

		if (CandidateMarker->GetTeam() != DefendingTeam)
		{
			continue;
		}

		if (CandidateMarker->GetPlayerRole() == ESoccerPlayerRole::Goalkeeper)
		{
			continue;
		}

		if (ExcludedCharacters.Contains(CandidateMarker))
		{
			continue;
		}

		const float CandidateScore =
			ScoreDefensiveMarkerCandidate(
				DefendingTeam,
				CandidateMarker,
				ReceiverToMark
			);

		if (CandidateScore > BestScore)
		{
			BestScore = CandidateScore;
			BestMarker = CandidateMarker;
		}
	}

	return BestMarker;
}

FVector ASoccerMatchManager::BuildDefendMarkReceiverLocation(
	const ASoccerAICharacter* SoccerAICharacter
) const
{
	if (SoccerAICharacter == nullptr)
	{
		return FVector::ZeroVector;
	}

	const ESoccerTeam DefendingTeam =
		SoccerAICharacter->GetTeam();

	const ASoccerCharacterBase* ReceiverToMark =
		GetDefensiveMarkedReceiverForCharacter(SoccerAICharacter);

	if (!IsValid(ReceiverToMark))
	{
		return BuildDefendCompactShapeLocation(SoccerAICharacter);
	}

	const FVector OwnGoalLocation =
		GetOwnGoalReferenceLocation(DefendingTeam);

	const FVector OpponentGoalLocation =
		GetOpponentGoalReferenceLocation(DefendingTeam);

	FVector TeamAttackDirection =
		OpponentGoalLocation - OwnGoalLocation;

	TeamAttackDirection.Z = 0.0f;

	const float FieldLength =
		TeamAttackDirection.Size();

	if (FieldLength <= 1.0f)
	{
		return BuildDefendCompactShapeLocation(SoccerAICharacter);
	}

	TeamAttackDirection =
		TeamAttackDirection / FieldLength;

	const FVector RightDirection =
		GetFieldRightDirectionForTeam(DefendingTeam);

	if (RightDirection.IsNearlyZero())
	{
		return BuildDefendCompactShapeLocation(SoccerAICharacter);
	}

	FVector DirectionReceiverToGoal =
		OwnGoalLocation - ReceiverToMark->GetActorLocation();

	DirectionReceiverToGoal.Z = 0.0f;

	if (DirectionReceiverToGoal.IsNearlyZero())
	{
		return BuildDefendCompactShapeLocation(SoccerAICharacter);
	}

	DirectionReceiverToGoal =
		DirectionReceiverToGoal.GetSafeNormal();

	float GoalSideMarkingDistance = DefensiveMarkReceiverGoalSideDistance;

	if (
		ShouldApplyCollectiveTacticsToOpenPlay(DefendingTeam) &&
		GetTacticalPlanForTeamInternal(DefendingTeam).MarkingStyle ==
			ESoccerMarkingStyle::ManToMan
		)
	{
		GoalSideMarkingDistance *= 0.75f;
	}

	FVector DesiredLocation =
		ReceiverToMark->GetActorLocation() +
		DirectionReceiverToGoal * GoalSideMarkingDistance;

	const float DesiredDepth =
		FVector::DotProduct(
			DesiredLocation - OwnGoalLocation,
			TeamAttackDirection
		);

	const float DesiredLateral =
		FVector::DotProduct(
			DesiredLocation - OwnGoalLocation,
			RightDirection
		);

	const float ClampedDepth =
		FMath::Clamp(
			DesiredDepth,
			DefensiveMarkReceiverMinDistanceFromOwnGoal,
			FieldLength * 0.82f
		);

	const float ScaledShapeMaxLateralOffset =
		FMath::Min(
			SoccerFieldDimensions::ScaleAuthoredLateralDistance(
				AttackShapeMaxLateralOffset
			),
			FMath::Max(0.0f, SoccerFieldDimensions::HalfPitchWidthCm - 120.0f)
		);

	const float ClampedLateral =
		FMath::Clamp(
			DesiredLateral,
			-ScaledShapeMaxLateralOffset,
			ScaledShapeMaxLateralOffset
		);

	DesiredLocation =
		OwnGoalLocation +
		TeamAttackDirection * ClampedDepth +
		RightDirection * ClampedLateral;

	// Explicit man marking may pull a player away from his normal slot, but it
	// should not drag the whole defensive structure across the pitch. Automatic
	// dangerous-receiver marking keeps the legacy unrestricted behavior.
	if (
		(
			IsValid(GetExplicitIndividualMarkingTarget(SoccerAICharacter)) ||
			IsValid(GetCollectiveManMarkingTarget(SoccerAICharacter))
		) &&
		IndividualMarkingMaxFollowDistanceFromStructure > 0.0f
		)
	{
		FSoccerFormationSlot StructuralSlot;
		FVector StructuralLocation = FVector::ZeroVector;

		if (TryGetFormationStructuralReference(
			SoccerAICharacter,
			StructuralSlot,
			StructuralLocation
		))
		{
			FVector FromStructureToMark = DesiredLocation - StructuralLocation;
			FromStructureToMark.Z = 0.0f;

			const float FollowDistance = FromStructureToMark.Size();

			if (FollowDistance > IndividualMarkingMaxFollowDistanceFromStructure)
			{
				DesiredLocation =
					StructuralLocation +
					FromStructureToMark.GetSafeNormal() *
					IndividualMarkingMaxFollowDistanceFromStructure;
			}
		}
	}

	DesiredLocation.Z =
		SoccerAICharacter->GetActorLocation().Z;

	return ProjectLocationToNavigation(
		DesiredLocation,
		SoccerAICharacter
	);
}

bool ASoccerMatchManager::HasDefensiveCoverBehindPressureCharacter(
	const ASoccerAICharacter* PressureCharacter,
	float MinBehindDepthAlphaGap
) const
{
	if (!IsValid(PressureCharacter))
	{
		return false;
	}

	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return false;
	}

	const ESoccerTeam DefendingTeam =
		PressureCharacter->GetTeam();

	const float PressureDepthAlpha =
		GetAttackDepthAlphaForLocation(
			PressureCharacter->GetActorLocation(),
			DefendingTeam
		);

	for (TActorIterator<ASoccerAICharacter> It(World); It; ++It)
	{
		const ASoccerAICharacter* Teammate = *It;

		if (!IsValid(Teammate))
		{
			continue;
		}

		if (Teammate == PressureCharacter)
		{
			continue;
		}

		if (Teammate->GetTeam() != DefendingTeam)
		{
			continue;
		}

		if (Teammate->GetPlayerRole() == ESoccerPlayerRole::Goalkeeper)
		{
			continue;
		}

		const float TeammateDepthAlpha =
			GetAttackDepthAlphaForLocation(
				Teammate->GetActorLocation(),
				DefendingTeam
			);

		if (
			TeammateDepthAlpha <=
			PressureDepthAlpha - MinBehindDepthAlphaGap
			)
		{
			return true;
		}
	}

	return false;
}

float ASoccerMatchManager::ScoreDefensiveStealOpportunity(
	const ASoccerAICharacter* PressureCharacter,
	const ASoccerCharacterBase* InPossessingCharacter,
	const ASoccerBall* InSoccerBall
) const
{
	if (
		!IsValid(PressureCharacter) ||
		!IsValid(InPossessingCharacter) ||
		!IsValid(InSoccerBall)
		)
	{
		return -TNumericLimits<float>::Max();
	}

	const ESoccerTeam DefendingTeam =
		PressureCharacter->GetTeam();

	if (!IsTeamCurrentlyDefending(DefendingTeam))
	{
		return -TNumericLimits<float>::Max();
	}

	if (InPossessingCharacter->GetTeam() == DefendingTeam)
	{
		return -TNumericLimits<float>::Max();
	}

	if (PressureCharacter->GetPlayerRole() == ESoccerPlayerRole::Goalkeeper)
	{
		return -TNumericLimits<float>::Max();
	}

	const FVector OwnGoalLocation =
		GetOwnGoalReferenceLocation(DefendingTeam);

	const FVector PressureLocation =
		PressureCharacter->GetActorLocation();

	const FVector PossessorLocation =
		InPossessingCharacter->GetActorLocation();

	const float DistanceToBall =
		FVector::Dist2D(
			PressureLocation,
			InSoccerBall->GetActorLocation()
		);

	float Score = 0.0f;

	Score +=
		(DefensiveStealIdealDistance - DistanceToBall) *
		DefensiveStealDistanceWeight;

	FVector FromPossessorToOwnGoal =
		OwnGoalLocation - PossessorLocation;

	FromPossessorToOwnGoal.Z = 0.0f;

	FVector FromPossessorToPressure =
		PressureLocation - PossessorLocation;

	FromPossessorToPressure.Z = 0.0f;

	if (
		!FromPossessorToOwnGoal.IsNearlyZero() &&
		!FromPossessorToPressure.IsNearlyZero()
		)
	{
		FromPossessorToOwnGoal =
			FromPossessorToOwnGoal.GetSafeNormal();

		FromPossessorToPressure =
			FromPossessorToPressure.GetSafeNormal();

		const float GoalSideDot =
			FVector::DotProduct(
				FromPossessorToOwnGoal,
				FromPossessorToPressure
			);

		if (GoalSideDot >= DefensiveStealGoalSideDotThreshold)
		{
			Score +=
				DefensiveStealGoalSideBonus *
				FMath::Clamp(GoalSideDot, 0.0f, 1.0f);
		}
		else
		{
			Score -= DefensiveStealWrongSidePenalty;
		}
	}

	const bool bHasCoverBehind =
		HasDefensiveCoverBehindPressureCharacter(
			PressureCharacter,
			DefensiveStealCoverBehindDepthGapAlpha
		);

	if (bHasCoverBehind)
	{
		Score += DefensiveStealCoverBehindBonus;
	}
	else
	{
		Score -= DefensiveStealNoCoverPenalty;
	}

	const float ThreatDepthAlpha =
		GetAttackDepthAlphaForLocation(
			PossessorLocation,
			DefendingTeam
		);

	if (ThreatDepthAlpha <= DefensiveStealDangerNearGoalDepthAlpha)
	{
		const float NearGoalUrgency =
			1.0f - FMath::Clamp(
				ThreatDepthAlpha / DefensiveStealDangerNearGoalDepthAlpha,
				0.0f,
				1.0f
			);

		Score +=
			DefensiveStealDangerNearGoalBonus *
			FMath::Lerp(
				0.65f,
				1.35f,
				NearGoalUrgency
			);
	}

	const ESoccerPlayerRole PressureRole =
		PressureCharacter->GetPlayerRole();

	if (PressureRole == ESoccerPlayerRole::Forward)
	{
		Score += DefensiveStealForwardPressureBonus;
	}
	else if (PressureRole == ESoccerPlayerRole::Midfielder)
	{
		Score += DefensiveStealMidfielderPressureBonus;
	}
	else if (PressureRole == ESoccerPlayerRole::Defender)
	{
		Score += DefensiveStealDefenderPressureBonus;

		if (ThreatDepthAlpha >= DefensiveStealFarFromGoalDepthAlpha)
		{
			Score -= DefensiveStealDefenderFarFromGoalPenalty;
		}
	}

	return Score;
}

bool ASoccerMatchManager::ShouldDefensivePressureAttemptSteal(
	const ASoccerAICharacter* PressureCharacter,
	const ASoccerCharacterBase* InPossessingCharacter,
	const ASoccerBall* InSoccerBall
) const
{
	if (
		!IsValid(PressureCharacter) ||
		!IsValid(InPossessingCharacter) ||
		!IsValid(InSoccerBall)
		)
	{
		return false;
	}

	const ESoccerTeam DefendingTeam =
		PressureCharacter->GetTeam();

	if (!IsTeamCurrentlyDefending(DefendingTeam))
	{
		return false;
	}

	if (InPossessingCharacter->GetTeam() == DefendingTeam)
	{
		return false;
	}

	if (IsBallProtectedFromCharacter(PressureCharacter))
	{
		return false;
	}

	const float ThreatDepthAlpha =
		GetAttackDepthAlphaForLocation(
			InPossessingCharacter->GetActorLocation(),
			DefendingTeam
		);

	const float DistanceToBall =
		FVector::Dist2D(
			PressureCharacter->GetActorLocation(),
			InSoccerBall->GetActorLocation()
		);

	// Zona de emergencia:
	// Si el rival está cerca de nuestro arco y el presionador ya está
	// a distancia razonable, que intente robar aunque la puntuación
	// táctica no sea perfecta.
	const bool bEmergencyNearGoal =
		ThreatDepthAlpha <= DefensiveStealEmergencyDepthAlpha &&
		DistanceToBall <= DefensiveStealEmergencyBallDistance;

	if (bEmergencyNearGoal)
	{
		return true;
	}

	const float OpportunityScore =
		ScoreDefensiveStealOpportunity(
			PressureCharacter,
			InPossessingCharacter,
			InSoccerBall
		);

	// Cerca del arco somos más agresivos.
	// No exigimos la misma puntuación que en mitad de cancha.
	if (ThreatDepthAlpha <= DefensiveStealDangerNearGoalDepthAlpha)
	{
		return OpportunityScore >= DefensiveStealMinOpportunityScore * 0.35f;
	}

	return OpportunityScore >= DefensiveStealMinOpportunityScore;
}

float ASoccerMatchManager::ScoreDefensiveCoverCenterCandidate(
	ESoccerTeam DefendingTeam,
	const ASoccerAICharacter* Candidate
) const
{
	if (!IsValid(Candidate))
	{
		return -TNumericLimits<float>::Max();
	}

	if (Candidate->GetTeam() != DefendingTeam)
	{
		return -TNumericLimits<float>::Max();
	}

	const ESoccerPlayerRole PlayerRole =
		Candidate->GetPlayerRole();

	if (PlayerRole == ESoccerPlayerRole::Goalkeeper)
	{
		return -TNumericLimits<float>::Max();
	}

	const FVector DesiredCoverLocation =
		BuildDefendCoverCenterLocation(Candidate);

	const float DistanceToCoverLocation =
		FVector::Dist2D(
			Candidate->GetActorLocation(),
			DesiredCoverLocation
		);

	float Score =
		4000.0f -
		DistanceToCoverLocation * DefensiveCoverCenterDistanceWeight;

	const ASoccerAICharacter* CurrentCoverCenterAI =
		DefendingTeam == ESoccerTeam::PlayerTeam
		? PlayerTeamCoverAI
		: OpponentTeamCoverAI;

	if (Candidate == CurrentCoverCenterAI)
	{
		Score += DefensiveCoverCenterCurrentRoleBonus;
	}

	if (PlayerRole == ESoccerPlayerRole::Midfielder)
	{
		Score += DefensiveCoverCenterMidfielderBonus;
	}
	else if (PlayerRole == ESoccerPlayerRole::Defender)
	{
		Score += DefensiveCoverCenterDefenderBonus;
	}
	else if (PlayerRole == ESoccerPlayerRole::Forward)
	{
		Score -= DefensiveCoverCenterForwardPenalty;
	}

	FSoccerSlotTacticalInstruction IndividualInstruction;

	if (
		TryGetOpenPlayIndividualInstructionForCharacter(
			Candidate,
			IndividualInstruction
		)
		)
	{
		switch (IndividualInstruction.DefensiveInstruction)
		{
		case ESoccerIndividualDefensiveInstruction::Cover:
		case ESoccerIndividualDefensiveInstruction::ProtectCenter:
			Score += IndividualPreferredRoleScoreBonus;
			break;

		case ESoccerIndividualDefensiveInstruction::HoldPosition:
			Score += IndividualSecondaryRoleScoreBonus;
			break;

		case ESoccerIndividualDefensiveInstruction::PressBall:
			Score -= IndividualAvoidRoleScorePenalty;
			break;

		case ESoccerIndividualDefensiveInstruction::MarkTightly:
			Score -= IndividualSecondaryRoleScoreBonus;
			break;

		default:
			break;
		}
	}

	return Score;
}

ASoccerAICharacter* ASoccerMatchManager::FindBestDefensiveCoverCenterAI(
	ESoccerTeam DefendingTeam,
	const TArray<const ASoccerAICharacter*>& ExcludedCharacters
) const
{
	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return nullptr;
	}

	ASoccerAICharacter* BestCharacter = nullptr;
	float BestScore = -TNumericLimits<float>::Max();

	for (TActorIterator<ASoccerAICharacter> It(World); It; ++It)
	{
		ASoccerAICharacter* Candidate = *It;

		if (!IsValid(Candidate))
		{
			continue;
		}

		if (Candidate->GetTeam() != DefendingTeam)
		{
			continue;
		}

		if (Candidate->GetPlayerRole() == ESoccerPlayerRole::Goalkeeper)
		{
			continue;
		}

		if (ExcludedCharacters.Contains(Candidate))
		{
			continue;
		}

		const float CandidateScore =
			ScoreDefensiveCoverCenterCandidate(
				DefendingTeam,
				Candidate
			);

		if (CandidateScore > BestScore)
		{
			BestScore = CandidateScore;
			BestCharacter = Candidate;
		}
	}

	return BestCharacter;
}

//////

// ============================================================
// THROW IN
// ============================================================

bool ASoccerMatchManager::IsThrowInRestartActive() const
{
	return
		IsThrowInMatchStateActive() ||
		IsThrowInDelayPositioningActive() ||
		MatchPlayState == ESoccerMatchPlayState::ThrowInSetup ||
		MatchPlayState == ESoccerMatchPlayState::ThrowInPositioning ||
		MatchPlayState == ESoccerMatchPlayState::ThrowInExecuting ||
		bThrowInExecutionActive;
}

bool ASoccerMatchManager::IsThrowInDelayPositioningActive() const
{
	return
		bThrowInStagedDuringBallOutOfPlayDelay &&
		IsBallOutOfPlayDelayActive() &&
		IsRestartContextActive() &&
		ActiveRestartType == ESoccerRestartType::ThrowIn &&
		ActiveRestartTeam == ThrowInTeam &&
		IsValid(ThrowInTakerAI) &&
		IsValid(ThrowInReceiverAI);
}

bool ASoccerMatchManager::IsThrowInTaker(
	const ASoccerAICharacter* SoccerAICharacter
) const
{
	return
		!bThrowInHumanTakerClaimed &&
		!bThrowInHumanTakerCommitted &&
		IsValid(SoccerAICharacter) &&
		SoccerAICharacter == ThrowInTakerAI &&
		(IsThrowInRestartActive() || bThrowInExecutionActive);
}

bool ASoccerMatchManager::IsHumanThrowInTaker(
	const AThirdPersonCppCharacter* HumanCharacter
) const
{
	return
		IsValid(HumanCharacter) &&
		IsValid(ThrowInHumanTaker) &&
		HumanCharacter == ThrowInHumanTaker &&
		(bThrowInHumanTakerClaimed || bThrowInHumanTakerCommitted);
}

bool ASoccerMatchManager::CanHumanThrowInTakerExecuteNow(
	const AThirdPersonCppCharacter* HumanCharacter
) const
{
	return
		bThrowInHumanTakerCommitted &&
		bThrowInHumanExecutionAuthorized &&
		!bThrowInBallReleased &&
		bThrowInExecutionActive &&
		MatchPlayState == ESoccerMatchPlayState::ThrowInExecuting &&
		IsHumanThrowInTaker(HumanCharacter) &&
		IsValid(HumanCharacter) &&
		HumanCharacter->IsHoldingThrowInBall();
}

bool ASoccerMatchManager::IsHumanThrowInMovementLocked(
	const AThirdPersonCppCharacter* HumanCharacter
) const
{
	return
		IsValid(HumanCharacter) &&
		IsValid(ThrowInHumanTaker) &&
		HumanCharacter == ThrowInHumanTaker &&
		bThrowInHumanTakerCommitted &&
		bThrowInExecutionActive;
}

bool ASoccerMatchManager::TryStartHumanThrowInToTarget(
	AThirdPersonCppCharacter* HumanCharacter,
	const FVector& RequestedTargetLocation
)
{
	if (
		!CanHumanThrowInTakerExecuteNow(HumanCharacter) ||
		bThrowInHumanTargetSelected ||
		bThrowInHumanMontageStarted
	)
	{
		return false;
	}

	if (!PrepareHumanThrowInDirectionAndStartLocation(RequestedTargetLocation))
	{
		return false;
	}

	// The click commits the restart. Off-ball targets remain valid but stop
	// changing while the human walks to the exact throw start and animates.
	LockActiveRestartLivePositioning();
	bThrowInHumanTargetSelected = true;
	bThrowInHumanRepositioningForTarget = true;

	ASoccerDebugManager::Message(
		this,
		ESoccerDebugCategory::Restarts,
		TEXT("LATERAL HUMANO: destino elegido, acomodando ejecutor"),
		FColor::Cyan
	);

	return true;
}

float ASoccerMatchManager::GetThrowInPickupMoveAcceptanceRadius() const
{
	return FMath::Max(1.0f, ThrowInPickupMoveAcceptanceRadius);
}

bool ASoccerMatchManager::IsThrowInTakerAnimationLocked(
	const ASoccerAICharacter* SoccerAICharacter
) const
{
	return
		bThrowInExecutionActive &&
		!bThrowInHumanTakerCommitted &&
		IsValid(SoccerAICharacter) &&
		SoccerAICharacter == ThrowInTakerAI;
}

void ASoccerMatchManager::UpdateBallBoundaryTrackingAndDetectOutOfPlay()
{
	if (!IsValid(SoccerBall))
	{
		return;
	}

	const FVector CurrentLocation = SoccerBall->GetActorLocation();

	if (!bBallBoundarySampleInitialized)
	{
		PreviousBallBoundarySampleLocation = CurrentLocation;
		bBallBoundarySampleInitialized = true;
		return;
	}

	const float BallRadius = SoccerBall->GetBallRadiusCm();
	const FVector CurrentLocal = IsValid(SoccerField)
		? SoccerField->WorldToPitchLocal(CurrentLocation)
		: CurrentLocation;

	const bool bBallFullyInsidePitch =
		FMath::Abs(CurrentLocal.X) <=
			SoccerFieldDimensions::HalfPitchLengthCm - BallRadius &&
		FMath::Abs(CurrentLocal.Y) <=
			SoccerFieldDimensions::HalfPitchWidthCm - BallRadius;

	if (bBallOutOfPlayLatched)
	{
		if (bBallFullyInsidePitch)
		{
			bBallOutOfPlayLatched = false;
		}

		PreviousBallBoundarySampleLocation = CurrentLocation;
		return;
	}

	if (
		!bHasLastTouchTeam ||
		SoccerBall->GetAttachParentActor() != nullptr
	)
	{
		PreviousBallBoundarySampleLocation = CurrentLocation;
		return;
	}

	bool bTouchlineCrossing = false;
	float BoundarySign = 0.0f;
	FVector CrossingLocation = FVector::ZeroVector;

	if (
		TryFindFirstPitchBoundaryCrossing(
			PreviousBallBoundarySampleLocation,
			CurrentLocation,
			BallRadius,
			bTouchlineCrossing,
			BoundarySign,
			CrossingLocation
		)
	)
	{
		bBallOutOfPlayLatched = true;

		if (bTouchlineCrossing)
		{
			if (bEnableThrowInRule)
			{
				FVector TouchlineLocal = IsValid(SoccerField)
					? SoccerField->WorldToPitchLocal(CrossingLocation)
					: CrossingLocation;

				TouchlineLocal.X = FMath::Clamp(
					TouchlineLocal.X,
					SoccerFieldDimensions::LeftGoalLineX + 35.0f,
					SoccerFieldDimensions::RightGoalLineX - 35.0f
				);
				TouchlineLocal.Y =
					BoundarySign * SoccerFieldDimensions::HalfPitchWidthCm;
				TouchlineLocal.Z = BallRadius;

				const FVector TouchlineLocation = IsValid(SoccerField)
					? SoccerField->PitchLocalToWorld(TouchlineLocal)
					: TouchlineLocal;

				const FVector WidthDirection = IsValid(SoccerField)
					? SoccerField->GetPitchWidthWorldDirection()
					: FVector::RightVector;
				const FVector InwardDirection =
					WidthDirection * -BoundarySign;

				BeginBallOutOfPlayDelay(
					ESoccerRestartType::ThrowIn,
					GetOppositeTeam(LastTouchTeam),
					TouchlineLocation,
					InwardDirection,
					0.0f
				);
			}
		}
		else if (bEnableGoalLineRestartRule)
		{
			// Goals have their own detector. A crossing through the actual goal
			// mouth must not be converted into a goal kick/corner.
			if (
				!IsPotentialGoalCrossing(
					PreviousBallBoundarySampleLocation,
					CurrentLocation,
					BoundarySign
				)
			)
			{
				const ESoccerTeam DefendingTeam =
					GetDefendingTeamForGoalLine(BoundarySign);
				const bool bLastTouchedByDefender =
					LastTouchTeam == DefendingTeam;
				const ESoccerGoalLineRestartType RestartType =
					bLastTouchedByDefender
						? ESoccerGoalLineRestartType::CornerKick
						: ESoccerGoalLineRestartType::GoalKick;
				const ESoccerTeam RestartTeam =
					bLastTouchedByDefender
						? GetOppositeTeam(DefendingTeam)
						: DefendingTeam;

				BeginBallOutOfPlayDelay(
					RestartType == ESoccerGoalLineRestartType::CornerKick
						? ESoccerRestartType::CornerKick
						: ESoccerRestartType::GoalKick,
					RestartTeam,
					CrossingLocation,
					FVector::ZeroVector,
					BoundarySign
				);
			}
		}
	}

	PreviousBallBoundarySampleLocation = CurrentLocation;
}

bool ASoccerMatchManager::TryFindFirstPitchBoundaryCrossing(
	const FVector& PreviousLocation,
	const FVector& CurrentLocation,
	float BallRadius,
	bool& bOutTouchline,
	float& OutBoundarySign,
	FVector& OutCrossingLocation
) const
{
	bOutTouchline = false;
	OutBoundarySign = 0.0f;
	OutCrossingLocation = FVector::ZeroVector;

	const FVector PreviousLocal = IsValid(SoccerField)
		? SoccerField->WorldToPitchLocal(PreviousLocation)
		: PreviousLocation;
	const FVector CurrentLocal = IsValid(SoccerField)
		? SoccerField->WorldToPitchLocal(CurrentLocation)
		: CurrentLocation;

	const float ExpandedHalfLength =
		SoccerFieldDimensions::HalfPitchLengthCm + BallRadius;
	const float ExpandedHalfWidth =
		SoccerFieldDimensions::HalfPitchWidthCm + BallRadius;

	const bool bPreviousInside =
		FMath::Abs(PreviousLocal.X) <= ExpandedHalfLength &&
		FMath::Abs(PreviousLocal.Y) <= ExpandedHalfWidth;
	const bool bCurrentInside =
		FMath::Abs(CurrentLocal.X) <= ExpandedHalfLength &&
		FMath::Abs(CurrentLocal.Y) <= ExpandedHalfWidth;

	if (!bPreviousInside || bCurrentInside)
	{
		return false;
	}

	const FVector Segment = CurrentLocal - PreviousLocal;
	float BestAlpha = TNumericLimits<float>::Max();
	bool bFound = false;
	FVector BestLocalCrossing = FVector::ZeroVector;

	auto ConsiderBoundary = [&]
	(
		float Alpha,
		bool bTouchline,
		float BoundarySign
	)
	{
		if (
			Alpha < 0.0f ||
			Alpha > 1.0f ||
			Alpha >= BestAlpha
		)
		{
			return;
		}

		const FVector Point =
			FMath::Lerp(PreviousLocal, CurrentLocal, Alpha);

		if (
			bTouchline &&
			FMath::Abs(Point.X) > ExpandedHalfLength + 1.0f
		)
		{
			return;
		}

		if (
			!bTouchline &&
			FMath::Abs(Point.Y) > ExpandedHalfWidth + 1.0f
		)
		{
			return;
		}

		BestAlpha = Alpha;
		bFound = true;
		bOutTouchline = bTouchline;
		OutBoundarySign = BoundarySign;
		BestLocalCrossing = Point;
	};

	if (Segment.Y > KINDA_SMALL_NUMBER && CurrentLocal.Y > ExpandedHalfWidth)
	{
		ConsiderBoundary(
			(ExpandedHalfWidth - PreviousLocal.Y) / Segment.Y,
			true,
			1.0f
		);
	}
	if (Segment.Y < -KINDA_SMALL_NUMBER && CurrentLocal.Y < -ExpandedHalfWidth)
	{
		ConsiderBoundary(
			(-ExpandedHalfWidth - PreviousLocal.Y) / Segment.Y,
			true,
			-1.0f
		);
	}
	if (Segment.X > KINDA_SMALL_NUMBER && CurrentLocal.X > ExpandedHalfLength)
	{
		ConsiderBoundary(
			(ExpandedHalfLength - PreviousLocal.X) / Segment.X,
			false,
			1.0f
		);
	}
	if (Segment.X < -KINDA_SMALL_NUMBER && CurrentLocal.X < -ExpandedHalfLength)
	{
		ConsiderBoundary(
			(-ExpandedHalfLength - PreviousLocal.X) / Segment.X,
			false,
			-1.0f
		);
	}

	if (!bFound)
	{
		return false;
	}

	OutCrossingLocation = IsValid(SoccerField)
		? SoccerField->PitchLocalToWorld(BestLocalCrossing)
		: BestLocalCrossing;
	return true;
}

bool ASoccerMatchManager::IsPotentialGoalCrossing(
	const FVector& PreviousLocation,
	const FVector& CurrentLocation,
	float GoalLineSign
) const
{
	const FVector PreviousLocal = IsValid(SoccerField)
		? SoccerField->WorldToPitchLocal(PreviousLocation)
		: PreviousLocation;
	const FVector CurrentLocal = IsValid(SoccerField)
		? SoccerField->WorldToPitchLocal(CurrentLocation)
		: CurrentLocation;
	const FVector Segment = CurrentLocal - PreviousLocal;

	if (FMath::Abs(Segment.X) <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const float GoalLineX =
		SoccerFieldDimensions::GetGoalLineX(GoalLineSign);
	const float Alpha =
		(GoalLineX - PreviousLocal.X) / Segment.X;

	if (Alpha < 0.0f || Alpha > 1.0f)
	{
		return false;
	}

	const FVector GoalPlaneCrossing =
		FMath::Lerp(PreviousLocal, CurrentLocal, Alpha);
	const float AllowedHalfWidth =
		SoccerFieldDimensions::GoalHalfWidthCm +
		FMath::Max(0.0f, GoalLinePotentialGoalSideMargin);

	if (FMath::Abs(GoalPlaneCrossing.Y - SoccerFieldDimensions::CenterY) > AllowedHalfWidth)
	{
		return false;
	}

	const float MinZ = -FMath::Abs(GoalLinePotentialGoalBottomMargin);
	const float MaxZ =
		SoccerFieldDimensions::GoalHeightCm +
		FMath::Max(0.0f, GoalLinePotentialGoalTopMargin);

	return GoalPlaneCrossing.Z >= MinZ && GoalPlaneCrossing.Z <= MaxZ;
}

ESoccerTeam ASoccerMatchManager::GetDefendingTeamForGoalLine(
	float GoalLineSign
) const
{
	const float NormalizedGoalLineSign =
		SoccerFieldDimensions::NormalizeGoalLineSign(GoalLineSign);

	return FMath::IsNearlyEqual(
		NormalizedGoalLineSign,
		GetOwnGoalLineSign(ESoccerTeam::PlayerTeam)
	)
		? ESoccerTeam::PlayerTeam
		: ESoccerTeam::OpponentTeam;
}

bool ASoccerMatchManager::IsBallOutOfPlayDelayActive() const
{
	return
		MatchPlayState ==
			ESoccerMatchPlayState::BallOutOfPlayDelay &&
		PendingBallOutOfPlayRestartType !=
			ESoccerRestartType::None;
}

void ASoccerMatchManager::BeginBallOutOfPlayDelay(
	ESoccerRestartType RestartType,
	ESoccerTeam RestartTeam,
	const FVector& RestartReferenceLocation,
	const FVector& RequestedThrowInInwardDirection,
	float GoalLineSign
)
{
	const bool bSupportedRestart =
		RestartType == ESoccerRestartType::ThrowIn ||
		RestartType == ESoccerRestartType::CornerKick ||
		RestartType == ESoccerRestartType::GoalKick;

	if (GetWorld() == nullptr || !IsValid(SoccerBall) || !bSupportedRestart)
	{
		return;
	}

	PendingBallOutOfPlayRestartType = RestartType;
	PendingBallOutOfPlayRestartTeam = RestartTeam;
	PendingBallOutOfPlayRestartReferenceLocation = RestartReferenceLocation;
	PendingBallOutOfPlayThrowInInwardDirection = RequestedThrowInInwardDirection.GetSafeNormal();
	PendingBallOutOfPlayGoalLineSign =
		FMath::IsNearlyZero(GoalLineSign) ? 0.0f : (GoalLineSign > 0.0f ? 1.0f : -1.0f);

	RequestMatchStateTransition(
		ESoccerMatchStateTransition::BallOutOfPlayDelay
	);
}

void ASoccerMatchManager::CompleteBallOutOfPlayDelay()
{
	if (!IsBallOutOfPlayDelayActive())
	{
		CancelBallOutOfPlayDelay();
		return;
	}

	const ESoccerRestartType RestartType =
		PendingBallOutOfPlayRestartType;
	const ESoccerTeam RestartTeam =
		PendingBallOutOfPlayRestartTeam;
	const FVector RestartReferenceLocation =
		PendingBallOutOfPlayRestartReferenceLocation;
	const FVector SavedThrowInInwardDirection =
		PendingBallOutOfPlayThrowInInwardDirection;
	const float GoalLineSign =
		PendingBallOutOfPlayGoalLineSign;
	const bool bUseStagedThrowIn =
		RestartType == ESoccerRestartType::ThrowIn &&
		IsThrowInDelayPositioningActive();
	const bool bUseStagedGoalLineRestart =
		(RestartType == ESoccerRestartType::CornerKick ||
		 RestartType == ESoccerRestartType::GoalKick) &&
		IsGoalLineRestartDelayPositioningActive();

	CancelBallOutOfPlayDelay();

	// Give the concrete restart function a neutral source state. This also
	// guarantees a safe fallback to normal play if no valid taker is found.
	MatchPlayState = ESoccerMatchPlayState::Playing;

	if (RestartType == ESoccerRestartType::ThrowIn)
	{
		if (bUseStagedThrowIn)
		{
			bThrowInStagedDuringBallOutOfPlayDelay = false;

			if (ActivateMatchState(
				MakeUnique<FSoccerThrowInPreparationState>()
			))
			{
				return;
			}

			// An assigned participant may have become invalid on the transition
			// frame. Re-run the normal configuration path as a safe fallback.
		}

		StartThrowIn(
			RestartTeam,
			RestartReferenceLocation,
			SavedThrowInInwardDirection
		);
		return;
	}

	if (RestartType == ESoccerRestartType::CornerKick)
	{
		if (
			bUseStagedGoalLineRestart &&
			GoalLineRestart.GetType() ==
				ESoccerGoalLineRestartType::CornerKick
			)
		{
			bGoalLineRestartStagedDuringBallOutOfPlayDelay = false;

			if (ActivateMatchState(
				MakeUnique<FSoccerCornerPreparationState>()
			))
			{
				return;
			}
		}

		ActivateMatchState(MakeUnique<FSoccerCornerConfigurationState>(
			RestartTeam,
			RestartReferenceLocation,
			GoalLineSign
		));
		return;
	}

	if (
		bUseStagedGoalLineRestart &&
		GoalLineRestart.GetType() ==
			ESoccerGoalLineRestartType::GoalKick
		)
	{
		bGoalLineRestartStagedDuringBallOutOfPlayDelay = false;

		if (ActivateMatchState(
			MakeUnique<FSoccerGoalKickPreparationState>()
		))
		{
			return;
		}
	}

	ActivateMatchState(MakeUnique<FSoccerGoalKickConfigurationState>(
		RestartTeam,
		RestartReferenceLocation,
		GoalLineSign
	));
}

void ASoccerMatchManager::CancelBallOutOfPlayDelay()
{
	PendingBallOutOfPlayRestartType = ESoccerRestartType::None;
	PendingBallOutOfPlayRestartTeam = ESoccerTeam::PlayerTeam;
	PendingBallOutOfPlayRestartReferenceLocation = FVector::ZeroVector;
	PendingBallOutOfPlayThrowInInwardDirection = FVector::ZeroVector;
	PendingBallOutOfPlayGoalLineSign = 0.0f;
}

void ASoccerMatchManager::StartThrowIn(
	ESoccerTeam RestartTeam,
	const FVector& TouchlineLocation,
	const FVector& InwardDirection
)
{
	if (GetWorld() == nullptr || !IsValid(SoccerBall))
	{
		return;
	}

	ActivateMatchState(MakeUnique<FSoccerThrowInConfigurationState>(
		RestartTeam,
		TouchlineLocation,
		InwardDirection
	));
}

void ASoccerMatchManager::ResetHumanThrowInTakerRuntime()
{
	ThrowInHumanTaker = nullptr;
	bThrowInHumanTakerClaimed = false;
	bThrowInHumanTakerCommitted = false;
	bThrowInHumanExecutionAuthorized = false;
	bThrowInHumanTargetSelected = false;
	bThrowInHumanRepositioningForTarget = false;
	bThrowInHumanMontageStarted = false;
	ThrowInHumanTargetLocation = FVector::ZeroVector;
}

bool ASoccerMatchManager::UpdateHumanThrowInTakerClaimDuringPreparation()
{
	if (
		bThrowInHumanTakerCommitted ||
		!IsRestartContextActive() ||
		ActiveRestartType != ESoccerRestartType::ThrowIn ||
		MatchPlayState != ESoccerMatchPlayState::ThrowInSetup
	)
	{
		return false;
	}

	const bool bPreviousClaim = bThrowInHumanTakerClaimed;
	AThirdPersonCppCharacter* CandidateHuman =
		FindHumanCharacterForTeam(ThrowInTeam);

	ThrowInHumanTaker = IsValid(CandidateHuman)
		? CandidateHuman
		: nullptr;
	bThrowInHumanExecutionAuthorized = false;

	if (!IsValid(ThrowInHumanTaker))
	{
		bThrowInHumanTakerClaimed = false;
		return bPreviousClaim;
	}

	const float ClaimRadius = FMath::Max(
		50.0f,
		ThrowInHumanTakerClaimRadius
	);
	const float ReleaseRadius = FMath::Max(
		ClaimRadius,
		ThrowInHumanTakerReleaseRadius
	);
	const float DistanceToRestart = FVector::Dist2D(
		ThrowInHumanTaker->GetActorLocation(),
		ThrowInLocation
	);

	bThrowInHumanTakerClaimed = bPreviousClaim
		? DistanceToRestart <= ReleaseRadius
		: DistanceToRestart <= ClaimRadius;

	if (bPreviousClaim != bThrowInHumanTakerClaimed)
	{
		ASoccerDebugManager::Message(
			this,
			ESoccerDebugCategory::Restarts,
			bThrowInHumanTakerClaimed
				? TEXT("LATERAL: humano reclama el saque")
				: TEXT("LATERAL: humano cede el saque al bot"),
			bThrowInHumanTakerClaimed
				? FColor::Cyan
				: FColor::Yellow
		);
	}

	return bPreviousClaim != bThrowInHumanTakerClaimed;
}

bool ASoccerMatchManager::PrepareHumanThrowInDirectionAndStartLocation(
	const FVector& RequestedTargetLocation
)
{
	if (
		!bThrowInHumanTakerCommitted ||
		!IsValid(ThrowInHumanTaker)
	)
	{
		return false;
	}

	FVector TargetLocation = RequestedTargetLocation;
	if (IsValid(SoccerField))
	{
		TargetLocation = SoccerField->ClampWorldLocationInsidePitch(
			TargetLocation,
			80.0f
		);
	}
	else
	{
		TargetLocation = SoccerFieldDimensions::ClampLocationInsidePitch(
			TargetLocation,
			80.0f
		);
	}

	FVector RawDirection = TargetLocation - ThrowInLocation;
	RawDirection.Z = 0.0f;

	if (RawDirection.Size2D() < 120.0f)
	{
		RawDirection = ThrowInInwardDirection;
		TargetLocation = ThrowInLocation + ThrowInInwardDirection * 700.0f;
	}
	RawDirection = RawDirection.GetSafeNormal();

	const FVector AlongTouchline = IsValid(SoccerField)
		? SoccerField->GetPitchLengthWorldDirection()
		: FVector::ForwardVector;

	const float InwardComponent = FMath::Max(
		0.05f,
		FVector::DotProduct(RawDirection, ThrowInInwardDirection)
	);
	float AlongComponent = FVector::DotProduct(
		RawDirection,
		AlongTouchline
	);
	const float MaximumSideTangent = FMath::Tan(
		FMath::DegreesToRadians(
			FMath::Clamp(ThrowInMaximumSideAngleDegrees, 0.0f, 85.0f)
		)
	);
	AlongComponent = FMath::Clamp(
		AlongComponent,
		-InwardComponent * MaximumSideTangent,
		InwardComponent * MaximumSideTangent
	);

	ThrowInDirection = (
		ThrowInInwardDirection * InwardComponent +
		AlongTouchline * AlongComponent
	).GetSafeNormal();
	if (ThrowInDirection.IsNearlyZero())
	{
		ThrowInDirection = ThrowInInwardDirection;
	}
	ThrowInRightDirection = FVector::CrossProduct(
		FVector::UpVector,
		ThrowInDirection
	).GetSafeNormal();

	FVector WorldMotionAtRelease = FVector::ZeroVector;
	FVector2D ReleaseLocalDisplacement = FVector2D::ZeroVector;
	const bool bHasReleaseCurveDisplacement =
		bUseThrowInCurveMotion &&
		EvaluateThrowInLocalDisplacementNormalized(
			ThrowInReleaseNormalizedTime,
			ReleaseLocalDisplacement
		);

	if (bHasReleaseCurveDisplacement)
	{
		WorldMotionAtRelease =
			ThrowInDirection *
			(ReleaseLocalDisplacement.X * ThrowInForwardMotionScale) +
			ThrowInRightDirection *
			(ReleaseLocalDisplacement.Y * ThrowInLateralMotionScale);
	}

	FVector DesiredReleaseLocation =
		ThrowInLocation -
		ThrowInInwardDirection * ThrowInDesiredCapsuleOutsideOffsetAtRelease;

	ThrowInOutsideStartLocation = bHasReleaseCurveDisplacement
		? DesiredReleaseLocation - WorldMotionAtRelease
		: ThrowInLocation -
			ThrowInInwardDirection * ThrowInFallbackOutsideDistance;
	ThrowInOutsideStartLocation.Z = ThrowInHumanTaker->GetActorLocation().Z;

	TargetLocation.Z = IsValid(SoccerBall)
		? SoccerBall->GetActorLocation().Z
		: TargetLocation.Z;
	ThrowInHumanTargetLocation = TargetLocation;
	return true;
}

bool ASoccerMatchManager::CompleteHumanThrowInRelease()
{
	if (
		!IsValid(ThrowInHumanTaker) ||
		!IsValid(SoccerBall) ||
		!CanHumanThrowInTakerExecuteNow(ThrowInHumanTaker) ||
		!bThrowInHumanTargetSelected
	)
	{
		return false;
	}

	FVector TargetLocation = ThrowInHumanTargetLocation;
	TargetLocation.Z = SoccerBall->GetActorLocation().Z;

	// Throw-ins never create an offside offence directly.
	ClearPendingOffsideSnapshot();

	if (!TryRegisterIntentionalBallTouch(ThrowInHumanTaker))
	{
		return false;
	}

	if (!ThrowInHumanTaker->ReleaseHeldThrowInBallToAirTarget(
		TargetLocation,
		ThrowInPassHorizontalSpeed,
		ThrowInPassMinTravelTime,
		ThrowInPassMaxTravelTime
	))
	{
		return false;
	}

	AThirdPersonCppCharacter* CompletedThrower = ThrowInHumanTaker;

	// The release frame is the restart boundary. Keep the throw-in accessory
	// runtime alive for montage completion/return-to-field, but end the legal
	// restart context immediately so normal open play can resume.
	EndRestartContext();
	ClearPendingOffsideSnapshot();
	StartNoRetouchRestriction(CompletedThrower);
	StartAttackRunReleaseForTeam(ThrowInTeam);

	bThrowInBallReleased = true;
	bThrowInHumanExecutionAuthorized = false;
	PossessingCharacter = nullptr;
	PossessionTeam = ESoccerPossessionTeam::None;
	MatchPlayState = ESoccerMatchPlayState::Playing;
	ClearAssignedAI();
	bThrowInReturningToField = true;

	ASoccerDebugManager::Message(
		this,
		ESoccerDebugCategory::Restarts,
		TEXT("Saque lateral realizado por el humano"),
		FColor::Green
	);

	RequestMatchStateTransition(ESoccerMatchStateTransition::Playing);
	return true;
}

bool ASoccerMatchManager::ConfigureThrowInRestart(
	ESoccerTeam RestartTeam,
	const FVector& TouchlineLocation,
	const FVector& InwardDirection
)
{
	UWorld* World = GetWorld();

	if (World == nullptr || !IsValid(SoccerBall))
	{
		return false;
	}

	ResetHumanThrowInTakerRuntime();

	ThrowInTeam = RestartTeam;
	ThrowInLocation = TouchlineLocation;
	ThrowInInwardDirection = InwardDirection.GetSafeNormal();

	if (ThrowInInwardDirection.IsNearlyZero())
	{
		return false;
	}

	ThrowInSetupStartTime = World->GetTimeSeconds();
	ThrowInTakerAI =
		FindBestThrowInTakerForTeam(RestartTeam, TouchlineLocation);
	ThrowInReceiverAI =
		FindBestThrowInReceiverForTeam(
			RestartTeam,
			ThrowInTakerAI,
			TouchlineLocation
		);

	if (!IsValid(ThrowInTakerAI) || !IsValid(ThrowInReceiverAI))
	{
		ThrowInTakerAI = nullptr;
		ThrowInReceiverAI = nullptr;
		return false;
	}

	RecalculateThrowInGeometry();
	return true;
}

bool ASoccerMatchManager::StageThrowInDuringBallOutOfPlayDelay(
	ESoccerTeam RestartTeam,
	const FVector& TouchlineLocation,
	const FVector& InwardDirection
)
{
	if (
		!IsBallOutOfPlayDelayActive() ||
		!ConfigureThrowInRestart(
			RestartTeam,
			TouchlineLocation,
			InwardDirection
		)
		)
	{
		return false;
	}

	BeginRestartContext(
		ESoccerRestartType::ThrowIn,
		RestartTeam,
		TouchlineLocation
	);

	bThrowInStagedDuringBallOutOfPlayDelay = true;
	CaptureActiveRestartAITargetLocations(false);
	return true;
}

ASoccerAICharacter* ASoccerMatchManager::FindBestThrowInTakerForTeam(
	ESoccerTeam Team,
	const FVector& TouchlineLocation
) const
{
	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return nullptr;
	}

	ASoccerAICharacter* BestCandidate = nullptr;
	float BestScore = TNumericLimits<float>::Max();

	for (TActorIterator<ASoccerAICharacter> It(World); It; ++It)
	{
		ASoccerAICharacter* Candidate = *It;

		if (!IsValid(Candidate) || Candidate->GetTeam() != Team)
		{
			continue;
		}

		float Score = FVector::Dist2D(
			Candidate->GetActorLocation(),
			TouchlineLocation
		);

		switch (Candidate->GetPlayerRole())
		{
		case ESoccerPlayerRole::Midfielder:
			Score += ThrowInMidfielderRolePenalty;
			break;

		case ESoccerPlayerRole::Forward:
			Score += ThrowInForwardRolePenalty;
			break;

		case ESoccerPlayerRole::Goalkeeper:
			Score += ThrowInGoalkeeperRolePenalty;
			break;

		case ESoccerPlayerRole::Defender:
		default:
			break;
		}

		AActor* HomePositionActor = Candidate->GetHomePositionActor();

		if (IsValid(HomePositionActor))
		{
			const FVector HomeLocation = GetTeamRebasedFieldReferenceLocation(
				Candidate->GetTeam(),
				HomePositionActor->GetActorLocation()
			);

			Score +=
				FVector::Dist2D(
					HomeLocation,
					TouchlineLocation
				) * ThrowInHomeDisruptionWeight;
		}

		if (Score < BestScore)
		{
			BestScore = Score;
			BestCandidate = Candidate;
		}
	}

	return BestCandidate;
}

ASoccerAICharacter* ASoccerMatchManager::FindBestThrowInReceiverForTeam(
	ESoccerTeam Team,
	const ASoccerAICharacter* Thrower,
	const FVector& TouchlineLocation
) const
{
	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return nullptr;
	}

	ASoccerAICharacter* BestCandidate = nullptr;
	float BestScore = -TNumericLimits<float>::Max();

	for (TActorIterator<ASoccerAICharacter> It(World); It; ++It)
	{
		ASoccerAICharacter* Candidate = *It;

		if (
			!IsValid(Candidate) ||
			Candidate->GetTeam() != Team ||
			Candidate == Thrower ||
			Candidate->GetPlayerRole() == ESoccerPlayerRole::Goalkeeper
			)
		{
			continue;
		}

		const float Distance = FVector::Dist2D(
			Candidate->GetActorLocation(),
			TouchlineLocation
		);

		float Score = -FMath::Abs(Distance - 900.0f);

		if (Candidate->GetPlayerRole() == ESoccerPlayerRole::Midfielder)
		{
			Score += 260.0f;
		}
		else if (Candidate->GetPlayerRole() == ESoccerPlayerRole::Forward)
		{
			Score += 120.0f;
		}

		Score -=
			CountOpponentsAroundLocation(
				Team,
				Candidate->GetActorLocation(),
				420.0f
			) * 220.0f;

		if (Score > BestScore)
		{
			BestScore = Score;
			BestCandidate = Candidate;
		}
	}

	return BestCandidate;
}

void ASoccerMatchManager::RecalculateThrowInGeometry()
{
	if (!IsValid(ThrowInTakerAI))
	{
		return;
	}

	ThrowInReceiverMoveLocation = BuildThrowInReceiverMoveLocation();

	FVector RawDirection =
		ThrowInReceiverMoveLocation - ThrowInLocation;
	RawDirection.Z = 0.0f;
	RawDirection = RawDirection.GetSafeNormal();

	if (RawDirection.IsNearlyZero())
	{
		RawDirection = ThrowInInwardDirection;
	}

	const FVector AlongTouchline = IsValid(SoccerField)
		? SoccerField->GetPitchLengthWorldDirection()
		: FVector::ForwardVector;
	const float InwardComponent = FMath::Max(
		0.05f,
		FVector::DotProduct(RawDirection, ThrowInInwardDirection)
	);

	float AlongComponent =
		FVector::DotProduct(RawDirection, AlongTouchline);

	const float MaximumSideTangent =
		FMath::Tan(FMath::DegreesToRadians(
			FMath::Clamp(ThrowInMaximumSideAngleDegrees, 0.0f, 85.0f)
		));

	AlongComponent = FMath::Clamp(
		AlongComponent,
		-InwardComponent * MaximumSideTangent,
		InwardComponent * MaximumSideTangent
	);

	ThrowInDirection =
		(ThrowInInwardDirection * InwardComponent +
		 AlongTouchline * AlongComponent).GetSafeNormal();

	if (ThrowInDirection.IsNearlyZero())
	{
		ThrowInDirection = ThrowInInwardDirection;
	}

	ThrowInRightDirection =
		FVector::CrossProduct(FVector::UpVector, ThrowInDirection)
		.GetSafeNormal();

	FVector WorldMotionAtRelease = FVector::ZeroVector;
	FVector2D ReleaseLocalDisplacement = FVector2D::ZeroVector;

	const bool bHasReleaseCurveDisplacement =
		bUseThrowInCurveMotion &&
		EvaluateThrowInLocalDisplacementNormalized(
			ThrowInReleaseNormalizedTime,
			ReleaseLocalDisplacement
		);

	if (bHasReleaseCurveDisplacement)
	{
		WorldMotionAtRelease =
			ThrowInDirection *
			(ReleaseLocalDisplacement.X * ThrowInForwardMotionScale)
			+
			ThrowInRightDirection *
			(ReleaseLocalDisplacement.Y * ThrowInLateralMotionScale);
	}

	// Queremos que la cápsula llegue al punto reglamentario de saque
	// en el frame de liberación, aunque la carrera sea diagonal.
	// Por eso restamos el vector COMPLETO de la curva: no solo su
	// componente perpendicular a la banda, sino también el avance
	// longitudinal que de otro modo desplazaría el saque varios metros.
	FVector DesiredReleaseLocation =
		ThrowInLocation -
		ThrowInInwardDirection *
		ThrowInDesiredCapsuleOutsideOffsetAtRelease;

	if (bHasReleaseCurveDisplacement)
	{
		ThrowInOutsideStartLocation =
			DesiredReleaseLocation - WorldMotionAtRelease;
	}
	else
	{
		ThrowInOutsideStartLocation =
			ThrowInLocation -
			ThrowInInwardDirection * ThrowInFallbackOutsideDistance;
	}

	// Quinta migracion: la preparacion termina junto a la pelota.
	// El ejecutor la toma aqui y despues se desplaza con ella hasta
	// el inicio exterior exacto calculado a partir de las curvas.
	ThrowInStagingLocation =
		ThrowInLocation +
		ThrowInInwardDirection * ThrowInPickupInsideDistance;

	ThrowInStagingLocation.Z = ThrowInTakerAI->GetActorLocation().Z;
	ThrowInOutsideStartLocation.Z = ThrowInTakerAI->GetActorLocation().Z;

	ThrowInStagingLocation =
		ProjectLocationToNavigation(ThrowInStagingLocation, ThrowInTakerAI);
}

FVector ASoccerMatchManager::BuildThrowInReceiverMoveLocation() const
{
	if (!IsValid(ThrowInReceiverAI))
	{
		return ThrowInLocation + ThrowInInwardDirection * 700.0f;
	}

	if (IsActiveRestartLivePositioningActive())
	{
		if (const FRestartLivePositioningPlan* ReceiverPlan =
			ActiveRestartLiveAttackingPlans.Find(ThrowInReceiverAI))
		{
			if (!ReceiverPlan->CommittedTargetLocation.IsNearlyZero())
			{
				return ReceiverPlan->CommittedTargetLocation;
			}
		}
	}

	const FVector PitchLengthDirection = IsValid(SoccerField)
		? SoccerField->GetPitchLengthWorldDirection()
		: FVector::ForwardVector;
	const float AlongOffset = FMath::Clamp(
		FVector::DotProduct(
			ThrowInReceiverAI->GetActorLocation() - ThrowInLocation,
			PitchLengthDirection
		),
		-900.0f,
		900.0f
	);

	FVector DesiredLocation =
		ThrowInLocation +
		ThrowInInwardDirection * 700.0f +
		PitchLengthDirection * AlongOffset;

	if (IsValid(SoccerField))
	{
		DesiredLocation =
			SoccerField->ClampWorldLocationInsidePitch(DesiredLocation, 180.0f);
	}
	else
	{
		DesiredLocation = SoccerFieldDimensions::ClampLocationInsidePitch(
			DesiredLocation,
			180.0f
		);
	}

	DesiredLocation.Z = ThrowInReceiverAI->GetActorLocation().Z;

	return ProjectLocationToNavigation(
		DesiredLocation,
		ThrowInReceiverAI
	);
}

FVector ASoccerMatchManager::BuildThrowInOpponentMoveLocation(
	const ASoccerAICharacter* SoccerAICharacter
) const
{
	if (!IsValid(SoccerAICharacter))
	{
		return FVector::ZeroVector;
	}

	const ESoccerAIOrder Order =
		GetDefaultRestartAttackOrderForCharacter(SoccerAICharacter);
	const FVector DesiredLocation =
		BuildAttackShapeLocation(SoccerAICharacter, Order);

	return BuildCircularRestartOpponentMoveLocation(
		SoccerAICharacter,
		DesiredLocation,
		ThrowInLocation,
		ThrowInOpponentRequiredDistance,
		ThrowInOpponentMoveExtraDistance
	);
}

FVector ASoccerMatchManager::GetThrowInMoveLocation(
	const ASoccerAICharacter* SoccerAICharacter
) const
{
	if (!IsThrowInRestartActive() || !IsValid(SoccerAICharacter))
	{
		return FVector::ZeroVector;
	}

	if (SoccerAICharacter == ThrowInTakerAI)
	{
		if (bThrowInHumanTakerClaimed || bThrowInHumanTakerCommitted)
		{
			return BuildNonFreeKickFallbackTakerMoveLocation(SoccerAICharacter);
		}

		return
			(MatchPlayState == ESoccerMatchPlayState::ThrowInSetup ||
			 IsThrowInDelayPositioningActive())
			? ThrowInStagingLocation
			: ThrowInOutsideStartLocation;
	}

	// Un arquero que no ejecuta el lateral conserva su posicion especifica.
	// La forma colectiva del saque esta pensada para jugadores de campo y,
	// aplicada al arquero, puede arrastrarlo varios metros fuera del arco.
	if (SoccerAICharacter->GetPlayerRole() == ESoccerPlayerRole::Goalkeeper)
	{
		return ProjectLocationToNavigation(
			GetGoalkeeperMoveLocation(SoccerAICharacter),
			SoccerAICharacter
		);
	}

	if (SoccerAICharacter == ThrowInReceiverAI)
	{
		return ThrowInReceiverMoveLocation;
	}

	if (SoccerAICharacter->GetTeam() != ThrowInTeam)
	{
		return BuildThrowInOpponentMoveLocation(SoccerAICharacter);
	}

	const ESoccerAIOrder Order =
		GetDefaultRestartAttackOrderForCharacter(SoccerAICharacter);

	FVector DesiredLocation =
		BuildAttackShapeLocation(SoccerAICharacter, Order);

	return ProjectLocationToNavigation(
		DesiredLocation,
		SoccerAICharacter
	);
}

bool ASoccerMatchManager::EvaluateThrowInLocalDisplacementNormalized(
	float NormalizedTime,
	FVector2D& OutLocalDisplacement
) const
{
	OutLocalDisplacement = FVector2D::ZeroVector;

	if (ThrowInMotionCurveTable == nullptr)
	{
		return false;
	}

	const FRealCurve* ForwardCurve =
		ThrowInMotionCurveTable->FindCurve(
			FName(TEXT("Forward")),
			TEXT("ThrowInCurveMotion"),
			false
		);

	const FRealCurve* LateralCurve =
		ThrowInMotionCurveTable->FindCurve(
			FName(TEXT("Lateral")),
			TEXT("ThrowInCurveMotion"),
			false
		);

	if (ForwardCurve == nullptr || LateralCurve == nullptr)
	{
		return false;
	}

	float MinTime = 0.0f;
	float MaxTime = 0.0f;
	ForwardCurve->GetTimeRange(MinTime, MaxTime);

	const float CurveTime = FMath::Lerp(
		MinTime,
		MaxTime,
		FMath::Clamp(NormalizedTime, 0.0f, 1.0f)
	);

	OutLocalDisplacement.X = ForwardCurve->Eval(CurveTime);
	OutLocalDisplacement.Y = LateralCurve->Eval(CurveTime);
	return true;
}

bool ASoccerMatchManager::ApplyThrowInCurveMotionNormalized(
	float NormalizedTime
)
{
	if (!bThrowInCurveMotionInitialized)
	{
		return false;
	}

	ASoccerCharacterBase* ActiveThrower = nullptr;
	if (bThrowInHumanTakerCommitted && IsValid(ThrowInHumanTaker))
	{
		ActiveThrower = ThrowInHumanTaker;
	}
	else if (IsValid(ThrowInTakerAI))
	{
		ActiveThrower = ThrowInTakerAI;
	}

	if (!IsValid(ActiveThrower))
	{
		return false;
	}

	FVector2D LocalDisplacement;
	if (!EvaluateThrowInLocalDisplacementNormalized(
		NormalizedTime,
		LocalDisplacement
	))
	{
		return false;
	}

	const FVector DesiredHorizontalOffset =
		ThrowInDirection *
		(LocalDisplacement.X * ThrowInForwardMotionScale) +
		ThrowInRightDirection *
		(LocalDisplacement.Y * ThrowInLateralMotionScale);

	FVector DesiredLocation =
		ThrowInCurveMotionStartLocation + DesiredHorizontalOffset;
	DesiredLocation.Z = ActiveThrower->GetActorLocation().Z;

	FVector Delta = DesiredLocation - ActiveThrower->GetActorLocation();
	Delta.Z = 0.0f;

	if (bThrowInHumanTakerCommitted && ActiveThrower == ThrowInHumanTaker)
	{
		ThrowInHumanTaker->MoveThrowInByWorldDelta(Delta);
	}
	else if (IsValid(ThrowInTakerAI))
	{
		ThrowInTakerAI->MoveThrowInByWorldDelta(Delta);
	}

	if (
		ASoccerDebugManager::IsWorldDrawingEnabled(
			this,
			ESoccerDebugCategory::Restarts
		) &&
		GetWorld() != nullptr
	)
	{
		DrawDebugLine(
			GetWorld(),
			ThrowInCurveMotionStartLocation,
			DesiredLocation,
			FColor::Yellow,
			false,
			0.05f,
			0,
			3.0f
		);
	}

	return true;
}

void ASoccerMatchManager::UpdateThrowInReturnToField(float DeltaTime)
{
	const bool bHumanThrower =
		bThrowInHumanTakerCommitted && IsValid(ThrowInHumanTaker);

	if (!bHumanThrower && !IsValid(ThrowInTakerAI))
	{
		CancelThrowInRestart();
		return;
	}

	float MontagePosition = 0.0f;
	float MontageLength = 0.0f;
	const bool bThrowInMontageStillActive = bHumanThrower
		? ThrowInHumanTaker->GetThrowInMontagePlaybackState(
			MontagePosition,
			MontageLength
		)
		: ThrowInTakerAI->GetThrowInMontagePlaybackState(
			MontagePosition,
			MontageLength
		);

	if (bThrowInMontageStillActive)
	{
		if (bThrowInCurveMotionInitialized)
		{
			const float NormalizedTime = FMath::Clamp(
				MontagePosition /
					FMath::Max(MontageLength, KINDA_SMALL_NUMBER),
				0.0f,
				1.0f
			);
			ApplyThrowInCurveMotionNormalized(NormalizedTime);
		}

		if (bHumanThrower)
		{
			ThrowInHumanTaker->ClearThrowInScriptedMovementVelocity();
		}
		else
		{
			ThrowInTakerAI->ClearScriptedLocomotionVelocity();
		}
		return;
	}

	if (bThrowInCurveMotionInitialized)
	{
		ApplyThrowInCurveMotionNormalized(1.0f);
		bThrowInCurveMotionInitialized = false;
	}

	FVector ReturnLocation =
		ThrowInLocation +
		ThrowInInwardDirection * ThrowInStagingInsideDistance;

	if (bHumanThrower)
	{
		ReturnLocation.Z = ThrowInHumanTaker->GetActorLocation().Z;
		const FVector CurrentLocation = ThrowInHumanTaker->GetActorLocation();
		FVector ToReturn = ReturnLocation - CurrentLocation;
		ToReturn.Z = 0.0f;
		const float DistanceToReturn = ToReturn.Size();

		if (DistanceToReturn > 4.0f)
		{
			const FVector MoveDirection = ToReturn.GetSafeNormal();
			ThrowInHumanTaker->SetActorRotation(MoveDirection.Rotation());
			ThrowInHumanTaker->SetThrowInScriptedMovementVelocity(
				MoveDirection * FMath::Max(1.0f, ThrowInReturnToFieldSpeed)
			);
			return;
		}

		ThrowInHumanTaker->ClearThrowInScriptedMovementVelocity();
		ThrowInHumanTaker->SetActorLocation(
			ReturnLocation,
			false,
			nullptr,
			ETeleportType::TeleportPhysics
		);
		bThrowInReturningToField = false;
		FinishThrowInExecution();
		return;
	}

	ReturnLocation.Z = ThrowInTakerAI->GetActorLocation().Z;
	ReturnLocation = ProjectLocationToNavigation(
		ReturnLocation,
		ThrowInTakerAI
	);

	const FVector CurrentLocation = ThrowInTakerAI->GetActorLocation();
	const FVector NewLocation = FMath::VInterpConstantTo(
		CurrentLocation,
		ReturnLocation,
		DeltaTime,
		FMath::Max(1.0f, ThrowInReturnToFieldSpeed)
	);

	FVector ScriptedVelocity =
		DeltaTime > KINDA_SMALL_NUMBER
		? (NewLocation - CurrentLocation) / DeltaTime
		: FVector::ZeroVector;
	ScriptedVelocity.Z = 0.0f;

	if (!ScriptedVelocity.IsNearlyZero())
	{
		ThrowInTakerAI->SetActorRotation(
			ScriptedVelocity.GetSafeNormal().Rotation()
		);
	}

	ThrowInTakerAI->SetActorLocation(
		NewLocation,
		true,
		nullptr,
		ETeleportType::None
	);
	ThrowInTakerAI->SetScriptedLocomotionVelocity(
		ScriptedVelocity,
		ESoccerAIMovementMode::Jog,
		ESoccerAIMovementReason::NearbyReposition
	);

	if (FVector::Dist2D(NewLocation, ReturnLocation) <= 4.0f)
	{
		ThrowInTakerAI->SetActorLocation(
			ReturnLocation,
			false,
			nullptr,
			ETeleportType::TeleportPhysics
		);
		ThrowInTakerAI->ClearScriptedLocomotionVelocity();
		bThrowInReturningToField = false;
		FinishThrowInExecution();
	}
}

void ASoccerMatchManager::FinishThrowInExecution()
{
	if (!bThrowInExecutionActive)
	{
		return;
	}

	// Return-to-field starts only after ThrowInExecutionState has released
	// the ball. Reaching Finish without release means the execution was invalid.
	if (!bThrowInBallReleased)
	{
		CancelThrowInRestart();
		return;
	}

	if (IsValid(ThrowInTakerAI))
	{
		ThrowInTakerAI->ClearScriptedLocomotionVelocity();
	}

	if (IsValid(ThrowInHumanTaker))
	{
		ThrowInHumanTaker->ClearThrowInScriptedMovementVelocity();
	}

	bThrowInExecutionActive = false;
	bThrowInReturningToField = false;
	bThrowInCurveMotionInitialized = false;
	bThrowInStagedDuringBallOutOfPlayDelay = false;

	if (MatchPlayState == ESoccerMatchPlayState::ThrowInExecuting)
	{
		MatchPlayState = ESoccerMatchPlayState::Playing;
	}

	ThrowInTakerAI = nullptr;
	ThrowInReceiverAI = nullptr;
	ResetHumanThrowInTakerRuntime();
}

void ASoccerMatchManager::CancelThrowInRestart()
{
	bThrowInStagedDuringBallOutOfPlayDelay = false;

	if (ActiveRestartType == ESoccerRestartType::ThrowIn)
	{
		EndRestartContext();
	}

	if (IsValid(ThrowInTakerAI))
	{
		ThrowInTakerAI->ClearScriptedLocomotionVelocity();

		if (ThrowInTakerAI->IsAIPossessingBall())
		{
			ThrowInTakerAI->ReleaseAIBall();
		}
	}

	if (IsValid(ThrowInHumanTaker))
	{
		ThrowInHumanTaker->CancelHeldThrowInBall();
		ThrowInHumanTaker->ClearThrowInScriptedMovementVelocity();
	}

	if (IsValid(SoccerBall))
	{
		if (SoccerBall->GetAttachParentActor() != nullptr)
		{
			SoccerBall->DetachFromActor(
				FDetachmentTransformRules::KeepWorldTransform
			);
		}

		SoccerBall->SetActorEnableCollision(true);
		SoccerBall->SetPossessed(false);
	}

	bThrowInExecutionActive = false;
	bThrowInBallReleased = false;
	bThrowInReturningToField = false;
	bThrowInCurveMotionInitialized = false;
	ThrowInTakerAI = nullptr;
	ThrowInReceiverAI = nullptr;
	ResetHumanThrowInTakerRuntime();

	if (
		MatchPlayState == ESoccerMatchPlayState::ThrowInSetup ||
		MatchPlayState == ESoccerMatchPlayState::ThrowInPositioning ||
		MatchPlayState == ESoccerMatchPlayState::ThrowInExecuting
		)
	{
		MatchPlayState = ESoccerMatchPlayState::Playing;
	}
}


// ============================================================
// GOAL LINE RESTARTS: GOAL KICK / CORNER KICK
// ============================================================

bool ASoccerMatchManager::IsGoalLineRestartActive() const
{
	return
		GoalLineRestart.IsActive(*this) ||
		IsGoalLineRestartDelayPositioningActive();
}

bool ASoccerMatchManager::IsGoalLineRestartTaker(
	const ASoccerAICharacter* SoccerAICharacter
) const
{
	const ESoccerRestartType ExpectedType =
		GoalLineRestart.RestartType == ESoccerGoalLineRestartType::CornerKick
		? ESoccerRestartType::CornerKick
		: ESoccerRestartType::GoalKick;

	return
		IsGoalLineRestartActive() &&
		!IsNonFreeKickHumanTakerClaimedFor(ExpectedType) &&
		IsValid(SoccerAICharacter) &&
		SoccerAICharacter == GoalLineRestart.TakerAI;
}

bool ASoccerMatchManager::IsGoalLineRestartDelayPositioningActive() const
{
	if (
		!bGoalLineRestartStagedDuringBallOutOfPlayDelay ||
		!IsBallOutOfPlayDelayActive() ||
		!IsRestartContextActive() ||
		ActiveRestartTeam != GoalLineRestart.GetRestartTeam()
		)
	{
		return false;
	}

	if (
		GoalLineRestart.GetType() ==
			ESoccerGoalLineRestartType::CornerKick
		)
	{
		return
			PendingBallOutOfPlayRestartType ==
				ESoccerRestartType::CornerKick &&
			ActiveRestartType == ESoccerRestartType::CornerKick &&
			GoalLineRestart.IsCornerConfigured();
	}

	if (
		GoalLineRestart.GetType() ==
			ESoccerGoalLineRestartType::GoalKick
		)
	{
		return
			PendingBallOutOfPlayRestartType ==
				ESoccerRestartType::GoalKick &&
			ActiveRestartType == ESoccerRestartType::GoalKick &&
			GoalLineRestart.IsGoalKickConfigured();
	}

	return false;
}

bool ASoccerMatchManager::IsGoalLineRestartTakerMovementLocked(
	const ASoccerAICharacter* SoccerAICharacter
) const
{
	const bool bCornerExecutionLock =
		IsGoalLineRestartTaker(SoccerAICharacter) &&
		GoalLineRestart.RestartType ==
			ESoccerGoalLineRestartType::CornerKick &&
		(
			MatchPlayState ==
			ESoccerMatchPlayState::GoalLineRestartPositioning ||
			MatchPlayState ==
			ESoccerMatchPlayState::GoalLineRestartTaking
		);

	const bool bReturnLock =
		GoalLineRestart.bCornerReturnToFieldActive &&
		IsValid(SoccerAICharacter) &&
		SoccerAICharacter == GoalLineRestart.CornerReturningTakerAI;

	return bCornerExecutionLock || bReturnLock;
}

bool ASoccerMatchManager::IsGoalKickTakerFinalApproach(
	const ASoccerAICharacter* SoccerAICharacter
) const
{
	return
		IsGoalLineRestartTaker(SoccerAICharacter) &&
		GoalLineRestart.RestartType ==
			ESoccerGoalLineRestartType::GoalKick &&
		MatchPlayState ==
			ESoccerMatchPlayState::GoalLineRestartTaking;
}

float ASoccerMatchManager::
GetGoalKickRunUpMoveAcceptanceRadius() const
{
	return FMath::Max(1.0f, GoalKickRunUpMoveAcceptanceRadius);
}

ESoccerGoalLineRestartType
ASoccerMatchManager::GetGoalLineRestartType() const
{
	return GoalLineRestart.GetType();
}

bool ASoccerMatchManager::ConfigureGoalLineRestart(
	ESoccerGoalLineRestartType RestartType,
	ESoccerTeam RestartTeam,
	const FVector& CrossingLocation,
	float GoalLineSign
)
{
	UWorld* World = GetWorld();

	if (
		World == nullptr ||
		!IsValid(SoccerBall) ||
		RestartType == ESoccerGoalLineRestartType::None
		)
	{
		return false;
	}

	const float NormalizedGoalLineSign =
		GoalLineSign >= 0.0f ? 1.0f : -1.0f;
	const FVector BallLocation =
		RestartType == ESoccerGoalLineRestartType::CornerKick
		? BuildCornerKickBallLocation(
			CrossingLocation,
			NormalizedGoalLineSign
		)
		: BuildGoalKickBallLocation(
			CrossingLocation,
			NormalizedGoalLineSign
		);

	if (RestartType == ESoccerGoalLineRestartType::CornerKick)
	{
		GoalLineRestart.ConfigureCorner(
			RestartTeam,
			GetOppositeTeam(RestartTeam),
			NormalizedGoalLineSign,
			CrossingLocation,
			BallLocation,
			World->GetTimeSeconds()
		);
	}
	else
	{
		GoalLineRestart.ConfigureGoalKick(
			RestartTeam,
			NormalizedGoalLineSign,
			CrossingLocation,
			BallLocation,
			World->GetTimeSeconds()
		);
	}

	ASoccerAICharacter* TakerAI =
		FindBestGoalLineRestartTakerForTeam(
			RestartTeam,
			RestartType,
			BallLocation
		);
	ASoccerAICharacter* ReceiverAI =
		FindBestGoalLineRestartReceiverForTeam(
			RestartTeam,
			RestartType,
			TakerAI,
			BallLocation
		);

	if (RestartType == ESoccerGoalLineRestartType::CornerKick)
	{
		GoalLineRestart.SetCornerParticipants(TakerAI, ReceiverAI);
	}
	else
	{
		GoalLineRestart.SetGoalKickParticipants(TakerAI, ReceiverAI);
	}

	const bool bConfigured =
		RestartType == ESoccerGoalLineRestartType::CornerKick
		? GoalLineRestart.IsCornerConfigured()
		: GoalLineRestart.IsGoalKickConfigured();

	if (!bConfigured)
	{
		GoalLineRestart.ResetRuntime();
		return false;
	}

	RecalculateGoalLineRestartGeometry();
	return true;
}

bool ASoccerMatchManager::StageGoalLineRestartDuringBallOutOfPlayDelay(
	ESoccerRestartType RestartType,
	ESoccerTeam RestartTeam,
	const FVector& CrossingLocation,
	float GoalLineSign
)
{
	const bool bCornerKick =
		RestartType == ESoccerRestartType::CornerKick;
	const bool bGoalKick =
		RestartType == ESoccerRestartType::GoalKick;

	if (!IsBallOutOfPlayDelayActive() || (!bCornerKick && !bGoalKick))
	{
		return false;
	}

	const ESoccerGoalLineRestartType GoalLineRestartType =
		bCornerKick
		? ESoccerGoalLineRestartType::CornerKick
		: ESoccerGoalLineRestartType::GoalKick;

	if (!ConfigureGoalLineRestart(
		GoalLineRestartType,
		RestartTeam,
		CrossingLocation,
		GoalLineSign
	))
	{
		return false;
	}

	BeginRestartContext(
		RestartType,
		RestartTeam,
		GoalLineRestart.GetBallLocation()
	);

	bGoalLineRestartStagedDuringBallOutOfPlayDelay = true;
	CaptureActiveRestartAITargetLocations(false);
	return true;
}

FVector ASoccerMatchManager::BuildGoalKickBallLocation(
	const FVector& CrossingLocation,
	float GoalLineSign
) const
{
	const float NormalizedGoalLineSign =
		SoccerFieldDimensions::NormalizeGoalLineSign(GoalLineSign);
	const FVector CrossingLocal = IsValid(SoccerField)
		? SoccerField->WorldToPitchLocal(CrossingLocation)
		: CrossingLocation;
	const FVector LastTouchLocal = IsValid(SoccerField)
		? SoccerField->WorldToPitchLocal(LastTouchLocation)
		: LastTouchLocation;

	float LateralSign = FMath::Sign(CrossingLocal.Y);
	if (FMath::IsNearlyZero(LateralSign))
	{
		LateralSign = FMath::Sign(LastTouchLocal.Y);
	}
	if (FMath::IsNearlyZero(LateralSign))
	{
		LateralSign = 1.0f;
	}

	const float MaxLateralOffset = FMath::Max(
		0.0f,
		SoccerFieldDimensions::GoalAreaHalfWidthCm - 55.0f
	);
	const float InwardDistance = FMath::Clamp(
		GoalKickBallInwardDistance,
		55.0f,
		SoccerFieldDimensions::GoalAreaDepthCm - 35.0f
	);
	const float BallRadius = IsValid(SoccerBall)
		? SoccerBall->GetBallRadiusCm()
		: 22.0f;

	FVector LocalLocation(
		NormalizedGoalLineSign *
			(SoccerFieldDimensions::HalfPitchLengthCm - InwardDistance),
		LateralSign * FMath::Min(
			FMath::Abs(GoalKickBallLateralOffset),
			MaxLateralOffset
		),
		BallRadius
	);

	return IsValid(SoccerField)
		? SoccerField->PitchLocalToWorld(LocalLocation)
		: LocalLocation;
}

FVector ASoccerMatchManager::BuildCornerKickBallLocation(
	const FVector& CrossingLocation,
	float GoalLineSign
) const
{
	const FVector CrossingLocal = IsValid(SoccerField)
		? SoccerField->WorldToPitchLocal(CrossingLocation)
		: CrossingLocation;
	const FVector LastTouchLocal = IsValid(SoccerField)
		? SoccerField->WorldToPitchLocal(LastTouchLocation)
		: LastTouchLocation;

	float TouchlineSign = FMath::Sign(CrossingLocal.Y);
	if (FMath::IsNearlyZero(TouchlineSign))
	{
		TouchlineSign = FMath::Sign(LastTouchLocal.Y);
	}
	if (FMath::IsNearlyZero(TouchlineSign))
	{
		TouchlineSign = 1.0f;
	}

	const float Inset = FMath::Clamp(
		CornerKickBallInsetFromLines,
		5.0f,
		SoccerFieldDimensions::CornerArcRadiusCm - 5.0f
	);
	const float BallRadius = IsValid(SoccerBall)
		? SoccerBall->GetBallRadiusCm()
		: 22.0f;

	if (IsValid(SoccerField))
	{
		return SoccerField->GetCornerWorldLocation(
			GoalLineSign,
			TouchlineSign,
			Inset,
			BallRadius
		);
	}

	return SoccerFieldDimensions::GetCornerLocalLocation(
		GoalLineSign,
		TouchlineSign,
		Inset,
		BallRadius
	);
}

ASoccerAICharacter*
ASoccerMatchManager::FindBestGoalLineRestartTakerForTeam(
	ESoccerTeam Team,
	ESoccerGoalLineRestartType RestartType,
	const FVector& RestartLocation
) const
{
	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return nullptr;
	}

	ASoccerAICharacter* BestCandidate = nullptr;
	float BestScore = TNumericLimits<float>::Max();

	for (TActorIterator<ASoccerAICharacter> It(World); It; ++It)
	{
		ASoccerAICharacter* Candidate = *It;

		if (!IsValid(Candidate) || Candidate->GetTeam() != Team)
		{
			continue;
		}

		float Score = FVector::Dist2D(
			Candidate->GetActorLocation(),
			RestartLocation
		);

		const ESoccerPlayerRole PlayerRole = Candidate->GetPlayerRole();

		if (
			RestartType == ESoccerGoalLineRestartType::CornerKick &&
			PlayerRole == ESoccerPlayerRole::Goalkeeper
			)
		{
			continue;
		}

		if (RestartType == ESoccerGoalLineRestartType::GoalKick)
		{
			if (PlayerRole == ESoccerPlayerRole::Goalkeeper)
			{
				Score -= GoalLineRestartGoalkeeperGoalKickBonus;
			}
			else if (PlayerRole == ESoccerPlayerRole::Forward)
			{
				Score += 450.0f;
			}
		}
		else if (
			RestartType == ESoccerGoalLineRestartType::CornerKick &&
			PlayerRole == ESoccerPlayerRole::Goalkeeper
			)
		{
			Score += GoalLineRestartGoalkeeperCornerPenalty;
		}

		if (AActor* HomeActor = Candidate->GetHomePositionActor())
		{
			Score += FVector::Dist2D(
				GetTeamRebasedFieldReferenceLocation(
					Candidate->GetTeam(),
					HomeActor->GetActorLocation()
				),
				RestartLocation
			) * GoalLineRestartHomeDisruptionWeight;
		}

		if (Score < BestScore)
		{
			BestScore = Score;
			BestCandidate = Candidate;
		}
	}

	return BestCandidate;
}

ASoccerAICharacter*
ASoccerMatchManager::FindBestGoalLineRestartReceiverForTeam(
	ESoccerTeam Team,
	ESoccerGoalLineRestartType RestartType,
	const ASoccerAICharacter* Taker,
	const FVector& RestartLocation
) const
{
	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return nullptr;
	}

	const FVector AttackDirection =
		GetFieldAttackDirectionForTeam(Team);

	FVector ReferenceTarget = RestartLocation + AttackDirection * 850.0f;

	if (RestartType == ESoccerGoalLineRestartType::CornerKick)
	{
		ReferenceTarget =
			GetOpponentGoalReferenceLocation(Team) -
			AttackDirection * 650.0f;
	}

	ASoccerAICharacter* BestCandidate = nullptr;
	float BestScore = TNumericLimits<float>::Max();

	for (TActorIterator<ASoccerAICharacter> It(World); It; ++It)
	{
		ASoccerAICharacter* Candidate = *It;

		if (
			!IsValid(Candidate) ||
			Candidate == Taker ||
			Candidate->GetTeam() != Team
			)
		{
			continue;
		}

		float Score = FVector::Dist2D(
			Candidate->GetActorLocation(),
			ReferenceTarget
		);

		const ESoccerPlayerRole PlayerRole = Candidate->GetPlayerRole();

		if (PlayerRole == ESoccerPlayerRole::Goalkeeper)
		{
			continue;
		}
		else if (RestartType == ESoccerGoalLineRestartType::GoalKick)
		{
			if (PlayerRole == ESoccerPlayerRole::Forward)
			{
				Score += 250.0f;
			}
			else if (PlayerRole == ESoccerPlayerRole::Defender)
			{
				Score -= 120.0f;
			}
		}
		else if (RestartType == ESoccerGoalLineRestartType::CornerKick)
		{
			if (PlayerRole == ESoccerPlayerRole::Forward)
			{
				Score -= 260.0f;
			}
			else if (PlayerRole == ESoccerPlayerRole::Midfielder)
			{
				Score -= 110.0f;
			}
			else if (PlayerRole == ESoccerPlayerRole::Defender)
			{
				Score += 140.0f;
			}
		}

		if (Score < BestScore)
		{
			BestScore = Score;
			BestCandidate = Candidate;
		}
	}

	return BestCandidate;
}

void ASoccerMatchManager::RecalculateGoalLineRestartGeometry()
{
	if (
		!IsValid(GoalLineRestart.TakerAI) ||
		!IsValid(GoalLineRestart.ReceiverAI)
		)
	{
		return;
	}

	GoalLineRestart.ReceiverMoveLocation =
		BuildGoalLineRestartReceiverMoveLocation();

	const FVector ExecutionReceiverLocation =
		GetActiveRestartExecutionTargetLocation(
			GoalLineRestart.ReceiverMoveLocation
		);

	GoalLineRestart.KickDirection =
		ExecutionReceiverLocation -
		GoalLineRestart.BallLocation;
	GoalLineRestart.KickDirection.Z = 0.0f;

	if (!GoalLineRestart.KickDirection.Normalize())
	{
		GoalLineRestart.KickDirection =
			GetFieldAttackDirectionForTeam(GoalLineRestart.RestartTeam);
	}

	if (GoalLineRestart.RestartType == ESoccerGoalLineRestartType::GoalKick)
	{
		GoalLineRestart.GoalKickRunUpStartLocation =
			GoalLineRestart.BallLocation -
			GoalLineRestart.KickDirection *
			FMath::Max(50.0f, GoalKickRunUpDistance);

		GoalLineRestart.GoalKickRunUpStartLocation.Z =
			GoalLineRestart.TakerAI->GetActorLocation().Z;

		GoalLineRestart.GoalKickRunUpStartLocation =
			ProjectLocationToNavigation(
				GoalLineRestart.GoalKickRunUpStartLocation,
				GoalLineRestart.TakerAI
			);

		GoalLineRestart.GoalKickRunThroughLocation =
			GoalLineRestart.BallLocation +
			GoalLineRestart.KickDirection *
			FMath::Max(50.0f, GoalKickRunThroughDistance);

		GoalLineRestart.GoalKickRunThroughLocation.Z =
			GoalLineRestart.TakerAI->GetActorLocation().Z;

		GoalLineRestart.GoalKickRunThroughLocation =
			ProjectLocationToNavigation(
				GoalLineRestart.GoalKickRunThroughLocation,
				GoalLineRestart.TakerAI
			);

		GoalLineRestart.TakerMoveLocation = GoalLineRestart.GoalKickRunUpStartLocation;
		GoalLineRestart.CornerStagingLocation = FVector::ZeroVector;
		GoalLineRestart.CornerOutsideStartLocation = FVector::ZeroVector;
		GoalLineRestart.CornerRunThroughLocation = FVector::ZeroVector;
		GoalLineRestart.bCornerFinalRunActive = false;
		return;
	}

	GoalLineRestart.GoalKickRunUpStartLocation = FVector::ZeroVector;
	GoalLineRestart.GoalKickRunThroughLocation = FVector::ZeroVector;
	GoalLineRestart.bGoalKickFinalRunActive = false;

	const FVector BallLocal = IsValid(SoccerField)
		? SoccerField->WorldToPitchLocal(GoalLineRestart.BallLocation)
		: GoalLineRestart.BallLocation;
	const float TouchlineSign = BallLocal.Y >= 0.0f ? 1.0f : -1.0f;

	const FVector PitchLengthDirection = IsValid(SoccerField)
		? SoccerField->GetPitchLengthWorldDirection()
		: FVector::ForwardVector;
	const FVector PitchWidthDirection = IsValid(SoccerField)
		? SoccerField->GetPitchWidthWorldDirection()
		: FVector::RightVector;

	const FVector GoalLineInward =
		PitchLengthDirection * -SoccerFieldDimensions::NormalizeGoalLineSign(
			GoalLineRestart.GoalLineSign
		);
	const FVector TouchlineInward = PitchWidthDirection * -TouchlineSign;

	GoalLineRestart.bCornerFinalRunActive = false;

	GoalLineRestart.CornerOutsideStartLocation =
		GoalLineRestart.BallLocation -
		GoalLineRestart.KickDirection *
		FMath::Max(50.0f, CornerKickRunUpDistance);
	GoalLineRestart.CornerOutsideStartLocation.Z =
		GoalLineRestart.TakerAI->GetActorLocation().Z;

	GoalLineRestart.CornerRunThroughLocation =
		GoalLineRestart.BallLocation +
		GoalLineRestart.KickDirection *
		FMath::Max(50.0f, CornerKickRunThroughDistance);
	GoalLineRestart.CornerRunThroughLocation.Z =
		GoalLineRestart.TakerAI->GetActorLocation().Z;

	GoalLineRestart.CornerStagingLocation =
		GoalLineRestart.BallLocation +
		GoalLineInward * CornerKickStagingInsideDistance +
		TouchlineInward * CornerKickStagingInsideDistance;
	GoalLineRestart.CornerStagingLocation.Z =
		GoalLineRestart.TakerAI->GetActorLocation().Z;

	GoalLineRestart.CornerStagingLocation =
		ProjectLocationToNavigation(
			GoalLineRestart.CornerStagingLocation,
			GoalLineRestart.TakerAI
		);

	GoalLineRestart.TakerMoveLocation = GoalLineRestart.CornerStagingLocation;
}

FVector ASoccerMatchManager::BuildGoalLineRestartReceiverMoveLocation() const
{
	if (!IsValid(GoalLineRestart.ReceiverAI))
	{
		return GoalLineRestart.BallLocation;
	}

	if (
		IsActiveRestartLivePositioningActive() &&
		ActiveRestartType == ESoccerRestartType::CornerKick
	)
	{
		if (const FRestartLivePositioningPlan* ReceiverPlan =
			ActiveRestartLiveAttackingPlans.Find(GoalLineRestart.ReceiverAI))
		{
			if (!ReceiverPlan->CommittedTargetLocation.IsNearlyZero())
			{
				return ReceiverPlan->CommittedTargetLocation;
			}
		}
	}

	const FVector AttackDirection =
		GetFieldAttackDirectionForTeam(GoalLineRestart.RestartTeam);

	FVector LateralDirection =
		FVector::CrossProduct(FVector::UpVector, AttackDirection);
	LateralDirection.Z = 0.0f;

	if (!LateralDirection.Normalize())
	{
		LateralDirection = FVector::RightVector;
	}

	const FVector FieldCenter = IsValid(SoccerField)
		? SoccerField->GetPitchCenterWorldLocation()
		: FVector::ZeroVector;
	const float ReceiverSide =
		FVector::DotProduct(
			GoalLineRestart.ReceiverAI->GetActorLocation() - FieldCenter,
			LateralDirection
		) >= 0.0f
		? 1.0f
		: -1.0f;

	FVector DesiredLocation;

	if (GoalLineRestart.RestartType == ESoccerGoalLineRestartType::GoalKick)
	{
		DesiredLocation =
			GoalLineRestart.BallLocation +
			AttackDirection * 850.0f +
			LateralDirection * ReceiverSide * 300.0f;
	}
	else
	{
		DesiredLocation =
			GetOpponentGoalReferenceLocation(GoalLineRestart.RestartTeam) -
			AttackDirection * 650.0f +
			LateralDirection * ReceiverSide * 210.0f;
	}

	if (IsValid(SoccerField))
	{
		DesiredLocation =
			SoccerField->ClampWorldLocationInsidePitch(DesiredLocation, 120.0f);
	}
	else
	{
		DesiredLocation = SoccerFieldDimensions::ClampLocationInsidePitch(
			DesiredLocation,
			120.0f
		);
	}
	DesiredLocation.Z = GoalLineRestart.ReceiverAI->GetActorLocation().Z;

	return ProjectLocationToNavigation(
		DesiredLocation,
		GoalLineRestart.ReceiverAI
	);
}

bool ASoccerMatchManager::IsLocationInsidePenaltyAreaForTeam(
	const FVector& Location,
	ESoccerTeam DefendingTeam,
	float ExtraDepth,
	float ExtraHalfWidth
) const
{
	const float GoalLineSign = GetOwnGoalLineSign(DefendingTeam);

	if (IsValid(SoccerField))
	{
		return SoccerField->IsWorldLocationInsidePenaltyArea(
			Location,
			GoalLineSign,
			ExtraDepth,
			ExtraHalfWidth
		);
	}

	return SoccerFieldDimensions::IsLocationInsidePenaltyArea2D(
		Location,
		GoalLineSign,
		ExtraDepth,
		ExtraHalfWidth
	);
}

bool ASoccerMatchManager::IsLocationInsideGoalAreaForTeam(
	const FVector& Location,
	ESoccerTeam DefendingTeam,
	float MarginCm
) const
{
	const float GoalLineSign = GetOwnGoalLineSign(DefendingTeam);

	if (IsValid(SoccerField))
	{
		return SoccerField->IsWorldLocationInsideGoalArea(
			Location,
			GoalLineSign,
			MarginCm
		);
	}

	return SoccerFieldDimensions::IsLocationInsideGoalArea2D(
		Location,
		GoalLineSign,
		MarginCm
	);
}

FVector ASoccerMatchManager::BuildGoalLineRestartOpponentMoveLocation(
	const ASoccerAICharacter* SoccerAICharacter
) const
{
	if (!IsValid(SoccerAICharacter))
	{
		return FVector::ZeroVector;
	}

	FVector DesiredLocation =
		BuildDefendCompactShapeLocation(SoccerAICharacter);
	DesiredLocation.Z = SoccerAICharacter->GetActorLocation().Z;

	if (GoalLineRestart.RestartType == ESoccerGoalLineRestartType::GoalKick)
	{
		return BuildPenaltyAreaRestartOpponentMoveLocation(
			SoccerAICharacter,
			DesiredLocation
		);
	}

	return BuildCircularRestartOpponentMoveLocation(
		SoccerAICharacter,
		DesiredLocation,
		GoalLineRestart.BallLocation,
		CornerKickOpponentRequiredDistance,
		CornerKickOpponentMoveExtraDistance
	);
}

FVector ASoccerMatchManager::GetGoalLineRestartMoveLocation(
	const ASoccerAICharacter* SoccerAICharacter
) const
{
	if (!IsGoalLineRestartActive() || !IsValid(SoccerAICharacter))
	{
		return FVector::ZeroVector;
	}

	if (SoccerAICharacter == GoalLineRestart.TakerAI)
	{
		const ESoccerRestartType ExpectedType =
			GoalLineRestart.RestartType == ESoccerGoalLineRestartType::CornerKick
			? ESoccerRestartType::CornerKick
			: ESoccerRestartType::GoalKick;

		if (IsNonFreeKickHumanTakerClaimedFor(ExpectedType))
		{
			return BuildNonFreeKickFallbackTakerMoveLocation(SoccerAICharacter);
		}
		// El corner usa movimiento manual fuera del NavMesh.
		if (
			GoalLineRestart.RestartType ==
				ESoccerGoalLineRestartType::CornerKick &&
			(
				MatchPlayState ==
					ESoccerMatchPlayState::GoalLineRestartPositioning ||
				MatchPlayState ==
					ESoccerMatchPlayState::GoalLineRestartTaking
			)
			)
		{
			return FVector::ZeroVector;
		}

		if (
			GoalLineRestart.RestartType ==
				ESoccerGoalLineRestartType::GoalKick
			)
		{
			return GoalLineRestart.bGoalKickFinalRunActive
				? GoalLineRestart.GoalKickRunThroughLocation
				: GoalLineRestart.GoalKickRunUpStartLocation;
		}

		return GoalLineRestart.TakerMoveLocation;
	}

	const ESoccerPlayerRole PlayerRole =
		SoccerAICharacter->GetPlayerRole();

	if (PlayerRole == ESoccerPlayerRole::Goalkeeper)
	{
		const FVector GoalkeeperLocation =
			GetGoalkeeperMoveLocation(SoccerAICharacter);

		return ProjectLocationToNavigation(
			GoalkeeperLocation,
			SoccerAICharacter
		);
	}

	if (SoccerAICharacter == GoalLineRestart.ReceiverAI)
	{
		return GoalLineRestart.ReceiverMoveLocation;
	}

	if (SoccerAICharacter->GetTeam() != GoalLineRestart.RestartTeam)
	{
		return BuildGoalLineRestartOpponentMoveLocation(
			SoccerAICharacter
		);
	}

	const ESoccerAIOrder Order =
		GetDefaultRestartAttackOrderForCharacter(SoccerAICharacter);

	const FVector DesiredLocation =
		BuildAttackShapeLocation(SoccerAICharacter, Order);

	return ProjectLocationToNavigation(
		DesiredLocation,
		SoccerAICharacter
	);
}

void ASoccerMatchManager::UpdateCornerKickReturnToField(
	float DeltaTime
)
{
	if (!GoalLineRestart.bCornerReturnToFieldActive)
	{
		return;
	}

	if (!IsValid(GoalLineRestart.CornerReturningTakerAI))
	{
		GoalLineRestart.bCornerReturnToFieldActive = false;
		GoalLineRestart.CornerReturningTakerAI = nullptr;
		return;
	}

	// Let the authored kick montage finish before the special outside-to-field
	// return movement takes ownership of the corner taker.
	if (GoalLineRestart.CornerReturningTakerAI->IsAIKickMontageActive())
	{
		return;
	}

	const FVector CurrentLocation =
		GoalLineRestart.CornerReturningTakerAI->GetActorLocation();
	const FVector NewLocation = FMath::VInterpConstantTo(
		CurrentLocation,
		GoalLineRestart.CornerReturnLocation,
		DeltaTime,
		FMath::Max(1.0f, CornerKickOutsidePositioningSpeed)
	);

	FVector ScriptedVelocity =
		DeltaTime > KINDA_SMALL_NUMBER
		? (NewLocation - CurrentLocation) / DeltaTime
		: FVector::ZeroVector;
	ScriptedVelocity.Z = 0.0f;

	if (!ScriptedVelocity.IsNearlyZero())
	{
		GoalLineRestart.CornerReturningTakerAI->SetActorRotation(
			ScriptedVelocity.GetSafeNormal().Rotation()
		);
	}

	GoalLineRestart.CornerReturningTakerAI->SetActorLocation(
		NewLocation,
		true,
		nullptr,
		ETeleportType::None
	);

	GoalLineRestart.CornerReturningTakerAI->SetScriptedLocomotionVelocity(
		ScriptedVelocity,
		ESoccerAIMovementMode::Jog,
		ESoccerAIMovementReason::NearbyReposition
	);

	if (FVector::Dist2D(NewLocation, GoalLineRestart.CornerReturnLocation) <= 4.0f)
	{
		GoalLineRestart.CornerReturningTakerAI->SetActorLocation(
			GoalLineRestart.CornerReturnLocation,
			false,
			nullptr,
			ETeleportType::TeleportPhysics
		);

		GoalLineRestart.CornerReturningTakerAI->ClearScriptedLocomotionVelocity();
		GoalLineRestart.bCornerReturnToFieldActive = false;
		GoalLineRestart.CornerReturningTakerAI = nullptr;
		GoalLineRestart.CornerReturnLocation = FVector::ZeroVector;
	}
}

void ASoccerMatchManager::CompleteGoalLineRestart()
{
	if (
		!IsValid(SoccerBall) ||
		!IsValid(GoalLineRestart.TakerAI) ||
		!IsValid(GoalLineRestart.ReceiverAI) ||
		GoalLineRestart.RestartType == ESoccerGoalLineRestartType::None
		)
	{
		CancelGoalLineRestart();
		return;
	}

	if (
		GoalLineRestart.RestartType == ESoccerGoalLineRestartType::GoalKick &&
		!GoalLineRestart.HasGoalKickContactConfirmed() &&
		!GoalLineRestart.bAIKickMontageStarted
		)
	{
		return;
	}

	if (
		GoalLineRestart.RestartType == ESoccerGoalLineRestartType::CornerKick &&
		!GoalLineRestart.HasCornerKickContactConfirmed() &&
		!GoalLineRestart.bAIKickMontageStarted
		)
	{
		return;
	}

	const ESoccerGoalLineRestartType CompletedType = GoalLineRestart.RestartType;
	ASoccerAICharacter* CompletedTaker = GoalLineRestart.TakerAI;
	ASoccerAICharacter* CompletedReceiver = GoalLineRestart.ReceiverAI;

	const bool bUseStoredMontageTarget =
		GoalLineRestart.bAIKickMontageStarted;

	FVector TargetLocation =
		bUseStoredMontageTarget
		? GoalLineRestart.PendingAIKickTargetLocation
		: GetActiveRestartExecutionTargetLocation(
			CompletedReceiver->GetActorLocation()
		);

	if (!bUseStoredMontageTarget)
	{
		TargetLocation.Z = SoccerBall->GetActorLocation().Z;
	}

	const float HorizontalSpeed =
		CompletedType == ESoccerGoalLineRestartType::CornerKick
		? CornerKickPassHorizontalSpeed
		: GoalKickPassHorizontalSpeed;
	const float MinTravelTime =
		CompletedType == ESoccerGoalLineRestartType::CornerKick
		? CornerKickPassMinTravelTime
		: GoalKickPassMinTravelTime;
	const float MaxTravelTime =
		CompletedType == ESoccerGoalLineRestartType::CornerKick
		? CornerKickPassMaxTravelTime
		: GoalKickPassMaxTravelTime;

	if (!GoalLineRestart.bAIKickMontageStarted)
	{
		FVector FacingDirection = TargetLocation - CompletedTaker->GetActorLocation();
		FacingDirection.Z = 0.0f;

		if (FacingDirection.Normalize())
		{
			CompletedTaker->SetActorRotation(FacingDirection.Rotation());
		}

		if (CompletedTaker->StartAIKickMontageForRestart(
			SoccerBall,
			TargetLocation,
			HorizontalSpeed,
			MinTravelTime,
			MaxTravelTime,
			true
		))
		{
			GoalLineRestart.bAIKickMontageStarted = true;
			GoalLineRestart.PendingAIKickTargetLocation =
				CompletedTaker->GetPendingAIKickTarget();
			return;
		}

		// Fallback path only: the authored montage did not start, so restore
		// the old behavior of stopping the run immediately before the kick.
		CompletedTaker->ClearScriptedLocomotionVelocity();
	}

	const bool bMontageImpactAlreadyLaunchedBall =
		GoalLineRestart.bAIKickMontageStarted &&
		CompletedTaker->HasAIKickMontageImpactedBall();

	if (GoalLineRestart.bAIKickMontageStarted && !bMontageImpactAlreadyLaunchedBall)
	{
		if (CompletedTaker->IsAIKickMontageActive())
		{
			return;
		}

		// Interrupted before impact. Clear only the montage runtime so the
		// execution state can re-evaluate contact and try again.
		GoalLineRestart.bAIKickMontageStarted = false;
		GoalLineRestart.PendingAIKickTargetLocation = FVector::ZeroVector;
		return;
	}

	ASoccerCharacterBase* CompletedExecutionReceiver =
		GetActiveRestartExecutionReceiver();
	const bool bPassWasIntendedForHuman =
		IsActiveRestartExecutionReceiverHuman();

	MatchPlayState = ESoccerMatchPlayState::Playing;
	ClearPendingOffsideSnapshot();

	if (!TryRegisterIntentionalBallTouch(CompletedTaker))
	{
		CancelGoalLineRestart();
		return;
	}

	EndRestartContext();
	ClearPendingOffsideSnapshot();
	StartNoRetouchRestriction(CompletedTaker);
	StartAttackRunReleaseForTeam(GoalLineRestart.RestartTeam);

	if (!bMontageImpactAlreadyLaunchedBall)
	{
		CompletedTaker->PlayAIKickAnimationForRestart();
		SoccerBall->KickToAirTarget(
			TargetLocation,
			HorizontalSpeed,
			MinTravelTime,
			MaxTravelTime
		);
	}

	PossessingCharacter = nullptr;
	PossessionTeam = ESoccerPossessionTeam::None;
	ClearAssignedAI();

	if (bPassWasIntendedForHuman && IsValid(CompletedExecutionReceiver))
	{
		RegisterOpenPlayPassIntent(
			CompletedTaker,
			CompletedExecutionReceiver,
			TargetLocation
		);
	}

	if (CompletedType == ESoccerGoalLineRestartType::CornerKick)
	{
		GoalLineRestart.bCornerReturnToFieldActive = true;
		GoalLineRestart.CornerReturningTakerAI = CompletedTaker;
		GoalLineRestart.CornerReturnLocation = GoalLineRestart.CornerStagingLocation;
		GoalLineRestart.ResetCornerFinalRunRuntime(*this);
	}

	GoalLineRestart.bAIKickMontageStarted = false;
	GoalLineRestart.PendingAIKickTargetLocation = FVector::ZeroVector;
	GoalLineRestart.RestartType = ESoccerGoalLineRestartType::None;
	GoalLineRestart.TakerAI = nullptr;
	GoalLineRestart.ReceiverAI = nullptr;

	if (ASoccerDebugManager::IsEnabled(this, ESoccerDebugCategory::Restarts))
	{
		ASoccerDebugManager::Message(
			this, ESoccerDebugCategory::Restarts,
			CompletedType == ESoccerGoalLineRestartType::CornerKick
			? TEXT("CORNER ejecutado") : TEXT("SAQUE DE ARCO ejecutado"),
			FColor::Green);
	}
}

void ASoccerMatchManager::CancelGoalLineRestart()
{
	const bool bWasActive = IsGoalLineRestartActive();
	bGoalLineRestartStagedDuringBallOutOfPlayDelay = false;

	if (
		ActiveRestartType == ESoccerRestartType::CornerKick ||
		ActiveRestartType == ESoccerRestartType::GoalKick
		)
	{
		EndRestartContext();
	}

	if (bWasActive && IsValid(SoccerBall))
	{
		if (SoccerBall->GetAttachParentActor() != nullptr)
		{
			SoccerBall->DetachFromActor(
				FDetachmentTransformRules::KeepWorldTransform
			);
		}

		SoccerBall->SetActorEnableCollision(true);
		SoccerBall->SetPossessed(false);
	}

	if (bWasActive)
	{
		MatchPlayState = ESoccerMatchPlayState::Playing;
	}

	if (IsValid(GoalLineRestart.TakerAI))
	{
		if (GoalLineRestart.bAIKickMontageStarted)
		{
			GoalLineRestart.TakerAI->CancelAIKickMontageBeforeImpact();
		}

		GoalLineRestart.TakerAI->ClearScriptedLocomotionVelocity();
	}

	if (IsValid(GoalLineRestart.CornerReturningTakerAI))
	{
		GoalLineRestart.CornerReturningTakerAI->ClearScriptedLocomotionVelocity();
	}

	GoalLineRestart.ResetGoalKickFinalRunRuntime(*this);
	GoalLineRestart.ResetCornerFinalRunRuntime(*this);

	GoalLineRestart.ResetRuntime();
}


void ASoccerMatchManager::FindSoccerField()
{
	SoccerField = nullptr;

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	for (TActorIterator<ASoccerField> It(World); It; ++It)
	{
		ASoccerField* Candidate = *It;
		if (!IsValid(Candidate))
		{
			continue;
		}

		SoccerField = Candidate;
		return;
	}
}

void ASoccerMatchManager::InitializeTeamFieldSides()
{
	// Stable first-half convention. If the level starts with the teams on the
	// opposite ends, the actual goalkeeper positions below override it.
	PlayerTeamOwnGoalLineSign = -1.0f;
	OpponentTeamOwnGoalLineSign = 1.0f;

	auto CaptureInitialSigns = [this]()
	{
		PlayerTeamInitialOwnGoalLineSign = PlayerTeamOwnGoalLineSign;
		OpponentTeamInitialOwnGoalLineSign = OpponentTeamOwnGoalLineSign;
	};

	if (!IsValid(SoccerField))
	{
		CaptureInitialSigns();
		return;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		CaptureInitialSigns();
		return;
	}

	bool bFoundPlayerGoalkeeper = false;
	bool bFoundOpponentGoalkeeper = false;
	float PlayerGoalkeeperSign = -1.0f;
	float OpponentGoalkeeperSign = 1.0f;

	for (TActorIterator<ASoccerAICharacter> It(World); It; ++It)
	{
		const ASoccerAICharacter* Candidate = *It;
		if (
			!IsValid(Candidate) ||
			Candidate->GetPlayerRole() != ESoccerPlayerRole::Goalkeeper
			)
		{
			continue;
		}

		const float CandidateSign =
			SoccerField->GetNearestGoalLineSign(Candidate->GetActorLocation());

		if (Candidate->GetTeam() == ESoccerTeam::PlayerTeam)
		{
			PlayerGoalkeeperSign = CandidateSign;
			bFoundPlayerGoalkeeper = true;
		}
		else
		{
			OpponentGoalkeeperSign = CandidateSign;
			bFoundOpponentGoalkeeper = true;
		}
	}

	if (bFoundPlayerGoalkeeper && bFoundOpponentGoalkeeper)
	{
		if (!FMath::IsNearlyEqual(PlayerGoalkeeperSign, OpponentGoalkeeperSign))
		{
			PlayerTeamOwnGoalLineSign = PlayerGoalkeeperSign;
			OpponentTeamOwnGoalLineSign = OpponentGoalkeeperSign;
		}
	}
	else if (bFoundPlayerGoalkeeper)
	{
		PlayerTeamOwnGoalLineSign = PlayerGoalkeeperSign;
		OpponentTeamOwnGoalLineSign = -PlayerGoalkeeperSign;
	}
	else if (bFoundOpponentGoalkeeper)
	{
		OpponentTeamOwnGoalLineSign = OpponentGoalkeeperSign;
		PlayerTeamOwnGoalLineSign = -OpponentGoalkeeperSign;
	}

	CaptureInitialSigns();
}

void ASoccerMatchManager::CaptureInitialHumanFieldReferences()
{
	bHasPlayerTeamInitialHumanFieldReference = false;
	bHasOpponentTeamInitialHumanFieldReference = false;

	if (AThirdPersonCppCharacter* PlayerHuman =
		FindHumanCharacterForTeam(ESoccerTeam::PlayerTeam))
	{
		PlayerTeamInitialHumanFieldReferenceLocation =
			PlayerHuman->GetActorLocation();
		bHasPlayerTeamInitialHumanFieldReference = true;
	}

	if (AThirdPersonCppCharacter* OpponentHuman =
		FindHumanCharacterForTeam(ESoccerTeam::OpponentTeam))
	{
		OpponentTeamInitialHumanFieldReferenceLocation =
			OpponentHuman->GetActorLocation();
		bHasOpponentTeamInitialHumanFieldReference = true;
	}
}

void ASoccerMatchManager::ApplySecondHalfFieldSideSwap()
{
	// Resolve from the immutable first-half baseline instead of multiplying the
	// current values. This makes the operation deterministic even if the method
	// is ever called again while recovering a state transition.
	PlayerTeamOwnGoalLineSign =
		-SoccerFieldDimensions::NormalizeGoalLineSign(
			PlayerTeamInitialOwnGoalLineSign
		);

	OpponentTeamOwnGoalLineSign =
		-SoccerFieldDimensions::NormalizeGoalLineSign(
			OpponentTeamInitialOwnGoalLineSign
		);

	RepositionHumansForCurrentFieldSide();
}

void ASoccerMatchManager::RepositionHumansForCurrentFieldSide()
{
	auto RepositionHuman = [this](
		ESoccerTeam Team,
		bool bHasInitialReference,
		const FVector& InitialReferenceLocation
	)
	{
		AThirdPersonCppCharacter* HumanCharacter =
			FindHumanCharacterForTeam(Team);

		if (!IsValid(HumanCharacter) || !bHasInitialReference)
		{
			return;
		}

		const FVector TargetLocation =
			GetTeamRebasedFieldReferenceLocation(
				Team,
				InitialReferenceLocation
			);

		HumanCharacter->SetActorLocation(
			TargetLocation,
			false,
			nullptr,
			ETeleportType::TeleportPhysics
		);

		if (UCharacterMovementComponent* Movement =
			HumanCharacter->GetCharacterMovement())
		{
			Movement->StopMovementImmediately();
		}
	};

	RepositionHuman(
		ESoccerTeam::PlayerTeam,
		bHasPlayerTeamInitialHumanFieldReference,
		PlayerTeamInitialHumanFieldReferenceLocation
	);

	RepositionHuman(
		ESoccerTeam::OpponentTeam,
		bHasOpponentTeamInitialHumanFieldReference,
		OpponentTeamInitialHumanFieldReferenceLocation
	);
}

ASoccerInstantReplayManager* ASoccerMatchManager::GetInstantReplayManager() const
{
	return InstantReplayManager;
}

void ASoccerMatchManager::InitializeInstantReplayRecorder()
{
	InstantReplayManager = nullptr;
	bOwnsInstantReplayManager = false;

	if (!bEnableInstantReplayRecording)
	{
		return;
	}

	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return;
	}

	/*
	 * Reuse a recorder deliberately placed in the level if one exists. The
	 * normal path needs no editor setup: MatchManager simply creates one.
	 */
	for (TActorIterator<ASoccerInstantReplayManager> It(World); It; ++It)
	{
		ASoccerInstantReplayManager* ExistingRecorder = *It;

		if (IsValid(ExistingRecorder))
		{
			InstantReplayManager = ExistingRecorder;
			break;
		}
	}

	if (!IsValid(InstantReplayManager))
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = this;
		SpawnParameters.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		InstantReplayManager = World->SpawnActor<ASoccerInstantReplayManager>(
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			SpawnParameters
		);

		bOwnsInstantReplayManager = IsValid(InstantReplayManager);
	}

	if (!IsValid(InstantReplayManager))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[InstantReplay] MatchManager could not create the recorder.")
		);
		return;
	}

	InstantReplayManager->InitializeRecorder(
		this,
		SoccerBall,
		InstantReplayHistorySeconds,
		InstantReplaySamplesPerSecond,
		InstantReplayManualPlaybackSeconds
	);

	FSoccerGoalReplayCameraConfig LeftCamera;
	LeftCamera.Distance = InstantReplayGoalLeftCameraDistance;
	LeftCamera.InfieldOffset = InstantReplayGoalLeftCameraInfieldOffset;
	LeftCamera.Height = InstantReplayGoalLeftCameraHeight;
	LeftCamera.FOV = InstantReplayGoalLeftCameraFOV;

	FSoccerGoalReplayCameraConfig RightCamera;
	RightCamera.Distance = InstantReplayGoalRightCameraDistance;
	RightCamera.InfieldOffset = InstantReplayGoalRightCameraInfieldOffset;
	RightCamera.Height = InstantReplayGoalRightCameraHeight;
	RightCamera.FOV = InstantReplayGoalRightCameraFOV;

	FSoccerGoalReplayCameraConfig FrontCamera;
	FrontCamera.Distance = InstantReplayGoalFrontCameraDistance;
	FrontCamera.Height = InstantReplayGoalFrontCameraHeight;
	FrontCamera.FOV = InstantReplayGoalFrontCameraFOV;

	FSoccerGoalReplayCameraConfig BehindCamera;
	BehindCamera.Distance = InstantReplayGoalBehindCameraDistance;
	BehindCamera.Height = InstantReplayGoalBehindCameraHeight;
	BehindCamera.FOV = InstantReplayGoalBehindCameraFOV;

	InstantReplayManager->ConfigureGoalReplayPresentation(
		LeftCamera,
		RightCamera,
		FrontCamera,
		BehindCamera,
		InstantReplayGoalCameraFadeSeconds,
		InstantReplayGoalCameraCollisionPadding
	);
}

void ASoccerMatchManager::ShutdownInstantReplayRecorder()
{
	if (!IsValid(InstantReplayManager))
	{
		InstantReplayManager = nullptr;
		bOwnsInstantReplayManager = false;
		return;
	}

	InstantReplayManager->SetRecordingEnabled(false);

	if (bOwnsInstantReplayManager)
	{
		InstantReplayManager->Destroy();
	}

	InstantReplayManager = nullptr;
	bOwnsInstantReplayManager = false;
}

void ASoccerMatchManager::TryStartGoalInstantReplay(ESoccerTeam ScoringTeam)
{
    if (
        !bEnableInstantReplayAfterGoal ||
        !IsValid(InstantReplayManager)
    )
    {
        return;
    }

	PendingGoalReplayScoringTeam = ScoringTeam;

	const float PostEventSeconds = FMath::Max(
		0.0f,
		InstantReplayGoalPostEventSeconds
	);

	if (PostEventSeconds > KINDA_SMALL_NUMBER)
	{
		GetWorldTimerManager().ClearTimer(GoalReplayStartTimerHandle);
		GetWorldTimerManager().SetTimer(
			GoalReplayStartTimerHandle,
			this,
			&ASoccerMatchManager::StartPendingGoalInstantReplay,
			PostEventSeconds,
			false
		);
		return;
	}

	StartPendingGoalInstantReplay();
}

void ASoccerMatchManager::StartPendingGoalInstantReplay()
{
	GetWorldTimerManager().ClearTimer(GoalReplayStartTimerHandle);

	if (
		MatchPlayState != ESoccerMatchPlayState::GoalScored ||
		!bEnableInstantReplayAfterGoal ||
		!IsValid(InstantReplayManager)
	)
	{
		return;
	}

	const float PreEventSeconds = FMath::Max(
		0.0f,
		InstantReplayGoalPreEventSeconds
	);
	const float PostEventSeconds = FMath::Max(
		0.0f,
		InstantReplayGoalPostEventSeconds
	);
    const float RequestedSeconds = FMath::Clamp(
		PreEventSeconds + PostEventSeconds,
        1.0f,
        FMath::Max(1.0f, InstantReplayHistorySeconds)
    );

	const float ScoredGoalLineSign =
		GetOpponentGoalLineSign(PendingGoalReplayScoringTeam);

    if (!InstantReplayManager->StartEventReplay(
        ESoccerInstantReplayPlaybackReason::Goal,
        RequestedSeconds,
        ScoredGoalLineSign
    ))
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("[InstantReplay] Goal replay could not start; normal goal flow continues.")
        );
    }
}


void ASoccerMatchManager::FindSoccerBall()
{
	SoccerBall = nullptr;

	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return;
	}

	for (TActorIterator<ASoccerBall> It(World); It; ++It)
	{
		SoccerBall = *It;

		if (IsValid(SoccerBall))
		{
			PreviousBallBoundarySampleLocation =
				SoccerBall->GetActorLocation();
			bBallBoundarySampleInitialized = true;
		}

		return;
	}
}

void ASoccerMatchManager::DetectPossession()
{
	PossessionTeam = ESoccerPossessionTeam::None;
	PossessingCharacter = nullptr;

	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return;
	}

	for (TActorIterator<ASoccerCharacterBase> It(World); It; ++It)
	{
		ASoccerCharacterBase* Candidate = *It;

		if (!IsValid(Candidate))
		{
			continue;
		}

		// La bandera base alimenta al AnimBlueprint y puede quedar con el valor
		// del frame anterior. Para la logica del partido usamos el control efectivo
		// de cada tipo de personaje, incluso durante una accion fisica antes del impacto.
		bool bCandidateReallyPossessesBall =
			Candidate->GetSoccerIsPossessingBall();

		if (const ASoccerAICharacter* CandidateAI =
			Cast<ASoccerAICharacter>(Candidate))
		{
			bCandidateReallyPossessesBall =
				CandidateAI->IsAIPossessingBall();
		}
		else if (const AThirdPersonCppCharacter* CandidateHuman =
			Cast<AThirdPersonCppCharacter>(Candidate))
		{
			bCandidateReallyPossessesBall =
				CandidateHuman->HasHumanLogicalBallControl();
		}

		if (!bCandidateReallyPossessesBall)
		{
			continue;
		}

		PossessingCharacter = Candidate;
		PossessionTeam = ConvertTeamToPossessionTeam(Candidate->GetTeam());

		return;
	}
}

void ASoccerMatchManager::AssignFreeBallRoles()
{
	ClearExplicitIndividualMarkingAssignmentsForTeam(
		ESoccerTeam::PlayerTeam
	);
	ClearExplicitIndividualMarkingAssignmentsForTeam(
		ESoccerTeam::OpponentTeam
	);

	FindClosestTwoAICharactersToBall(
		ESoccerTeam::PlayerTeam,
		PlayerTeamPressureAI,
		PlayerTeamSupportAI
	);

	if (ShouldAIYieldBallRecoveryToHuman(
		ESoccerTeam::PlayerTeam,
		PlayerTeamPressureAI
	))
	{
		PlayerTeamPressureAI = nullptr;
	}

	FindClosestTwoAICharactersToBall(
		ESoccerTeam::OpponentTeam,
		OpponentTeamPressureAI,
		OpponentTeamSupportAI
	);

	if (ShouldAIYieldBallRecoveryToHuman(
		ESoccerTeam::OpponentTeam,
		OpponentTeamPressureAI
	))
	{
		OpponentTeamPressureAI = nullptr;
	}
}

void ASoccerMatchManager::AssignAttackDefenseRoles()
{
	if (!HasActiveAttack())
	{
		AssignFreeBallRoles();
		return;
	}

	UpdateCollectiveTacticalTransitionTracking();

	ESoccerTeam AttackingTeam =
		GetCurrentAttackingTeam();

	ESoccerTeam DefendingTeam =
		GetCurrentDefendingTeam();

	ESoccerTeam EmergencyDefendingTeam = DefendingTeam;
	const bool bDangerousLooseBallEmergency =
		TryGetDangerousLooseBallEmergencyDefendingTeam(
			EmergencyDefendingTeam
		);

	if (bDangerousLooseBallEmergency)
	{
		DefendingTeam = EmergencyDefendingTeam;
		AttackingTeam = GetOppositeTeam(EmergencyDefendingTeam);
	}

	// Explicit man marking only belongs to the side currently defending.
	ClearExplicitIndividualMarkingAssignmentsForTeam(AttackingTeam);

	const bool bAttackingTeamHasCurrentPossession =
		PossessionTeam != ESoccerPossessionTeam::None &&
		DoesTeamHavePossession(AttackingTeam);

	ASoccerAICharacter* AttackingRecoveryAI = nullptr;
	ASoccerAICharacter* UnusedSecondClosestAttacker = nullptr;

	if (!bAttackingTeamHasCurrentPossession)
	{
		const bool bPassIntentControlsRecovery =
			HasOpenPlayPassIntentForTeam(AttackingTeam);

		if (bPassIntentControlsRecovery)
		{
			// Si el receptor es un bot, solo ese bot ataca el pase. Si es el
			// humano, ningun bot del equipo compite con el durante el trayecto.
			ASoccerAICharacter* IntendedReceiverAI =
				Cast<ASoccerAICharacter>(OpenPlayIntendedReceiver);

			if (
				IsValid(IntendedReceiverAI) &&
				IntendedReceiverAI->GetTeam() == AttackingTeam &&
				IntendedReceiverAI->GetPlayerRole() !=
					ESoccerPlayerRole::Goalkeeper &&
				CanCharacterTouchBallNow(IntendedReceiverAI)
				)
			{
				AttackingRecoveryAI = IntendedReceiverAI;
			}
		}
		else
		{
			FindClosestTwoAICharactersToBall(
				AttackingTeam,
				AttackingRecoveryAI,
				UnusedSecondClosestAttacker
			);
		}
	}

	if (ShouldAIYieldBallRecoveryToHuman(
		AttackingTeam,
		AttackingRecoveryAI
	))
	{
		AttackingRecoveryAI = nullptr;
	}

	TArray<const ASoccerAICharacter*> ExcludedAttackers;

	if (AttackingRecoveryAI != nullptr)
	{
		ExcludedAttackers.Add(AttackingRecoveryAI);
	}

	ASoccerAICharacter* AttackingSupportAI =
		FindBestAttackingSupportAI(
			AttackingTeam,
			ExcludedAttackers
		);

	if (AttackingSupportAI != nullptr)
	{
		ExcludedAttackers.Add(AttackingSupportAI);
	}

	ASoccerAICharacter* AttackingSecondaryAI =
		FindBestAttackingSecondaryAI(
			AttackingTeam,
			ExcludedAttackers,
			AttackingSupportAI
		);

	if (AttackingTeam == ESoccerTeam::PlayerTeam)
	{
		PlayerTeamPressureAI =
			bAttackingTeamHasCurrentPossession
			? nullptr
			: AttackingRecoveryAI;

		PlayerTeamSupportAI = AttackingSupportAI;
		PlayerTeamCoverAI = AttackingSecondaryAI;
	}
	else
	{
		OpponentTeamPressureAI =
			bAttackingTeamHasCurrentPossession
			? nullptr
			: AttackingRecoveryAI;

		OpponentTeamSupportAI = AttackingSupportAI;
		OpponentTeamCoverAI = AttackingSecondaryAI;
	}

	ASoccerAICharacter* DefendingPressure = nullptr;
	ASoccerAICharacter* DefendingGoalLane = nullptr;
	ASoccerAICharacter* DefendingCoverCenter = nullptr;
	ASoccerAICharacter* DefendingMarker = nullptr;
	ASoccerCharacterBase* DangerousReceiver = nullptr;

	if (bDangerousLooseBallEmergency)
	{
		ASoccerAICharacter* UnusedSecondEmergencyDefender = nullptr;
		FindClosestTwoAICharactersToBall(
			DefendingTeam,
			DefendingPressure,
			UnusedSecondEmergencyDefender
		);
	}
	else
	{
		DefendingPressure =
			FindBestDefensivePressureAI(DefendingTeam);
	}

	if (ShouldAIYieldBallRecoveryToHuman(
		DefendingTeam,
		DefendingPressure
	))
	{
		DefendingPressure = nullptr;
	}

	TArray<const ASoccerAICharacter*> ExcludedDefenders;

	if (DefendingPressure != nullptr)
	{
		ExcludedDefenders.Add(DefendingPressure);
	}

	DefendingGoalLane =
		FindBestDefensiveGoalLaneAI(
			DefendingTeam,
			ExcludedDefenders,
			DefendingPressure
		);

	if (DefendingGoalLane != nullptr)
	{
		ExcludedDefenders.Add(DefendingGoalLane);
	}

	DefendingCoverCenter =
		FindBestDefensiveCoverCenterAI(
			DefendingTeam,
			ExcludedDefenders
		);

	if (DefendingCoverCenter != nullptr)
	{
		ExcludedDefenders.Add(DefendingCoverCenter);
	}

	// Stage 8: pressure, goal-lane protection and central cover are reserved
	// before man-marking requests. The remaining explicit requests are then
	// coordinated one-to-one so two players do not chase the same receiver.
	RebuildExplicitIndividualMarkingAssignmentsForTeam(
		DefendingTeam,
		ExcludedDefenders
	);

	TArray<const ASoccerCharacterBase*> ExplicitlyMarkedReceivers;
	const TMap<ASoccerAICharacter*, ASoccerCharacterBase*>& ExplicitMarks =
		GetExplicitIndividualMarkingAssignmentsForTeamInternal(DefendingTeam);

	for (
		const TPair<ASoccerAICharacter*, ASoccerCharacterBase*>& ExplicitMark :
		ExplicitMarks
		)
	{
		if (IsValid(ExplicitMark.Key))
		{
			ExcludedDefenders.AddUnique(ExplicitMark.Key);
		}

		if (IsValid(ExplicitMark.Value))
		{
			ExplicitlyMarkedReceivers.AddUnique(ExplicitMark.Value);
		}
	}

	const TMap<ASoccerAICharacter*, ASoccerCharacterBase*>& CollectiveMarks =
		GetCollectiveManMarkingAssignmentsForTeamInternal(DefendingTeam);

	for (
		const TPair<ASoccerAICharacter*, ASoccerCharacterBase*>& CollectiveMark :
		CollectiveMarks
		)
	{
		if (IsValid(CollectiveMark.Key))
		{
			ExcludedDefenders.AddUnique(CollectiveMark.Key);
		}

		if (IsValid(CollectiveMark.Value))
		{
			ExplicitlyMarkedReceivers.AddUnique(CollectiveMark.Value);
		}
	}

	DangerousReceiver =
		FindMostDangerousAttackingReceiver(
			DefendingTeam,
			ExplicitlyMarkedReceivers
		);

	DefendingMarker =
		FindBestDefensiveMarkerAI(
			DefendingTeam,
			DangerousReceiver,
			ExcludedDefenders
		);

	if (DefendingTeam == ESoccerTeam::PlayerTeam)
	{
		PlayerTeamPressureAI = DefendingPressure;
		PlayerTeamSupportAI = DefendingGoalLane;
		PlayerTeamCoverAI = DefendingCoverCenter;
		PlayerTeamDefensiveMarkerAI = DefendingMarker;
		PlayerTeamDefensiveMarkedReceiver = DangerousReceiver;
	}
	else
	{
		OpponentTeamPressureAI = DefendingPressure;
		OpponentTeamSupportAI = DefendingGoalLane;
		OpponentTeamCoverAI = DefendingCoverCenter;
		OpponentTeamDefensiveMarkerAI = DefendingMarker;
		OpponentTeamDefensiveMarkedReceiver = DangerousReceiver;
	}
}

ASoccerAICharacter* ASoccerMatchManager::FindActiveAIAutoPassRecoveryCharacterForTeam(
	ESoccerTeam Team
) const
{
	if (!IsValid(SoccerBall))
	{
		return nullptr;
	}

	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return nullptr;
	}

	ASoccerAICharacter* BestCharacter = nullptr;
	float BestAutoPassAge = TNumericLimits<float>::Max();

	for (TActorIterator<ASoccerAICharacter> It(World); It; ++It)
	{
		ASoccerAICharacter* Candidate = *It;

		if (!IsValid(Candidate))
		{
			continue;
		}

		if (Candidate->GetTeam() != Team)
		{
			continue;
		}

		if (Candidate->GetPlayerRole() == ESoccerPlayerRole::Goalkeeper)
		{
			continue;
		}

		if (
			!Candidate->IsAIAutoPassActive() ||
			Candidate->GetAIAutoPassBall() != SoccerBall
			)
		{
			continue;
		}

		// TryRegisterAIKickTouchForRules registra al autor antes de ejecutar
		// el autopase. Si otro jugador toca despues, la reserva deja de ser
		// valida aunque el Character aun no haya limpiado su estado local.
		if (
			IsValid(LastTouchCharacter) &&
			LastTouchCharacter != Candidate
			)
		{
			continue;
		}

		if (!CanCharacterTouchBallNow(Candidate))
		{
			continue;
		}

		const float AutoPassAge =
			FMath::Max(0.0f, Candidate->GetTimeSinceAIAutoPassStarted());

		if (AutoPassAge < BestAutoPassAge)
		{
			BestAutoPassAge = AutoPassAge;
			BestCharacter = Candidate;
		}
	}

	return BestCharacter;
}

float ASoccerMatchManager::GetBallRecoveryRankingTime(
	const ASoccerCharacterBase* Candidate
) const
{
	if (!IsValid(Candidate) || !IsValid(SoccerBall))
	{
		return TNumericLimits<float>::Max();
	}

	FSoccerBallInterceptionResult InterceptionResult;

	if (Candidate->FindBestBallInterception(
		SoccerBall,
		InterceptionResult
	))
	{
		if (InterceptionResult.bCanArriveInTime)
		{
			return FMath::Max(
				InterceptionResult.BallArrivalTime,
				InterceptionResult.PlayerArrivalTime
			);
		}

		return
			FreeBallUnreachableCandidatePenalty +
			InterceptionResult.PlayerArrivalTime +
			FMath::Max(
				0.0f,
				-InterceptionResult.ArrivalTimeMargin
			);
	}

	return
		FreeBallNoPredictionCandidatePenalty +
		Candidate->EstimateArrivalTimeToLocation(
			SoccerBall->GetActorLocation()
		);
}

bool ASoccerMatchManager::ShouldAIYieldBallRecoveryToHuman(
	ESoccerTeam Team,
	const ASoccerAICharacter* CandidateAI
) const
{
	if (
		!bEnableHumanBallClaimPriority ||
		MatchPlayState != ESoccerMatchPlayState::Playing ||
		IsRestartContextActive() ||
		!IsValid(SoccerBall) ||
		!IsValid(CandidateAI) ||
		CandidateAI->GetTeam() != Team
		)
	{
		return false;
	}

	AThirdPersonCppCharacter* HumanCharacter =
		FindHumanCharacterForTeam(Team);

	if (
		!IsValid(HumanCharacter) ||
		!HumanCharacter->HasActiveHumanBallClaim() ||
		!CanCharacterTouchBallNow(HumanCharacter)
		)
	{
		return false;
	}

	// Close to the team's own goal, defensive safety deliberately wins over
	// the anti-crowding preference. The existing emergency depth is the single
	// source of truth for this exception.
	if (
		bAllowAIHumanClaimDefensiveEmergencyHelp &&
		IsImmediateDefensiveDangerForTeam(Team)
		)
	{
		return false;
	}

	// Preserve the existing reservation for the author of an AI auto-pass. It
	// is an intentional continuation of possession, not a generic teammate
	// joining a loose-ball race.
	if (
		FindActiveAIAutoPassRecoveryCharacterForTeam(Team) ==
			CandidateAI
		)
	{
		return false;
	}

	const float HumanRankingTime =
		GetBallRecoveryRankingTime(HumanCharacter);

	const float AIRankingTime =
		GetBallRecoveryRankingTime(CandidateAI);

	const float InvalidRankingThreshold =
		TNumericLimits<float>::Max() * 0.5f;

	if (
		!FMath::IsFinite(HumanRankingTime) ||
		HumanRankingTime >= InvalidRankingThreshold
		)
	{
		// If the human has no usable route/prediction, do not suppress the bot.
		return false;
	}

	if (
		!FMath::IsFinite(AIRankingTime) ||
		AIRankingTime >= InvalidRankingThreshold
		)
	{
		return true;
	}

	const float AIInterceptionAdvantage =
		HumanRankingTime - AIRankingTime;

	return AIInterceptionAdvantage < FMath::Max(
		0.0f,
		HumanBallClaimAIRequiredTimeAdvantage
	);
}

void ASoccerMatchManager::FindClosestTwoAICharactersToBall(
	ESoccerTeam Team,
	ASoccerAICharacter*& OutClosest,
	ASoccerAICharacter*& OutSecondClosest
)
{
	OutClosest = nullptr;
	OutSecondClosest = nullptr;

	if (!IsValid(SoccerBall))
	{
		return;
	}

	// Un autopase ya es una decision de recuperacion tomada por su autor.
	// Mientras siga siendo el ultimo jugador que toco esa misma pelota, no
	// hacemos competir a un companero mediante el ranking generico de pelota
	// libre. El segundo recuperador queda vacio deliberadamente.
	if (ASoccerAICharacter* AutoPassRecoveryCharacter =
		FindActiveAIAutoPassRecoveryCharacterForTeam(Team))
	{
		OutClosest = AutoPassRecoveryCharacter;
		return;
	}

	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return;
	}

	struct FRecoveryCandidate
	{
		ASoccerAICharacter* Character = nullptr;
		float RankingTime = TNumericLimits<float>::Max();
	};

	TArray<FRecoveryCandidate> Candidates;

	for (TActorIterator<ASoccerAICharacter> It(World); It; ++It)
	{
		ASoccerAICharacter* Candidate = *It;

		if (!IsValid(Candidate))
		{
			continue;
		}

		if (Candidate->GetTeam() != Team)
		{
			continue;
		}

		if (Candidate->GetPlayerRole() == ESoccerPlayerRole::Goalkeeper)
		{
			continue;
		}

		if (!CanCharacterTouchBallNow(Candidate))
		{
			continue;
		}

		const float RankingTime =
			GetBallRecoveryRankingTime(Candidate);

		if (!FMath::IsFinite(RankingTime))
		{
			continue;
		}

		FRecoveryCandidate Entry;
		Entry.Character = Candidate;
		Entry.RankingTime = RankingTime;
		Candidates.Add(Entry);
	}

	Candidates.Sort(
		[](const FRecoveryCandidate& A, const FRecoveryCandidate& B)
		{
			return A.RankingTime < B.RankingTime;
		}
	);

	if (Candidates.Num() <= 0)
	{
		return;
	}

	ASoccerAICharacter* PreviousChaser =
		Team == ESoccerTeam::PlayerTeam
		? LastPlayerTeamFreeBallChaser
		: LastOpponentTeamFreeBallChaser;

	int32 SelectedIndex = 0;

	if (IsValid(PreviousChaser))
	{
		for (int32 Index = 0; Index < Candidates.Num(); ++Index)
		{
			if (Candidates[Index].Character != PreviousChaser)
			{
				continue;
			}

			const float ChallengerAdvantage =
				Candidates[Index].RankingTime -
				Candidates[0].RankingTime;

			const float PreviousChaserDistanceToBall =
				FVector::Dist2D(
					PreviousChaser->GetActorLocation(),
					SoccerBall->GetActorLocation()
				);

			const bool bPreviousChaserCommittedAtCloseRange =
				PreviousChaserDistanceToBall <=
				FMath::Max(0.0f, FreeBallChaserCommitDistance);

			if (
				bPreviousChaserCommittedAtCloseRange ||
				ChallengerAdvantage <=
				FreeBallChaserSwitchRequiredTimeAdvantage
				)
			{
				SelectedIndex = Index;
			}

			break;
		}
	}

	OutClosest = Candidates[SelectedIndex].Character;

	for (int32 Index = 0; Index < Candidates.Num(); ++Index)
	{
		if (Index == SelectedIndex)
		{
			continue;
		}

		OutSecondClosest = Candidates[Index].Character;
		break;
	}

	if (Team == ESoccerTeam::PlayerTeam)
	{
		LastPlayerTeamFreeBallChaser = OutClosest;
	}
	else
	{
		LastOpponentTeamFreeBallChaser = OutClosest;
	}
}

ESoccerPossessionTeam ASoccerMatchManager::ConvertTeamToPossessionTeam(
	ESoccerTeam Team
) const
{
	if (Team == ESoccerTeam::PlayerTeam)
	{
		return ESoccerPossessionTeam::PlayerTeam;
	}

	return ESoccerPossessionTeam::OpponentTeam;
}

bool ASoccerMatchManager::DoesTeamHavePossession(ESoccerTeam Team) const
{
	return ConvertTeamToPossessionTeam(Team) == PossessionTeam;
}

void ASoccerMatchManager::ClearAttackState()
{
	ClearOpenPlayPassIntent();
	ClearFreeBallChaserMemory();

	bHasLastTouchTeam = false;
	LastTouchCharacter = nullptr;
	LastTouchLocation = FVector::ZeroVector;
	ClearAttackRunRelease();
	ClearPendingOffsideSnapshot();

	// A restart is not a turnover. Reset collective transition history so the
	// first controlled possession after kickoff/throw/free kick establishes a
	// fresh baseline instead of triggering a false counter/regroup window.
	bHasCollectiveLastControlledPossessionTeam = false;
	PlayerTeamLastCollectivePossessionGainTime = -1000.0f;
	OpponentTeamLastCollectivePossessionGainTime = -1000.0f;
	PlayerTeamLastCollectivePossessionLossTime = -1000.0f;
	OpponentTeamLastCollectivePossessionLossTime = -1000.0f;
}

void ASoccerMatchManager::ClearFreeBallChaserMemory()
{
	LastPlayerTeamFreeBallChaser = nullptr;
	LastOpponentTeamFreeBallChaser = nullptr;
}

void ASoccerMatchManager::UpdateActiveRestartRestrictionSystem(
	float DeltaTime
)
{
	UpdateActiveRestartHumanRestrictionIndicator();
	UpdateActiveRestartIllegalBotRecovery(DeltaTime);
}

ESoccerRestartRestrictionShape
ASoccerMatchManager::GetActiveRestartRestrictionShape() const
{
	if (!IsRestartContextActive())
	{
		return ESoccerRestartRestrictionShape::None;
	}

	switch (ActiveRestartType)
	{
	case ESoccerRestartType::OffsideFreeKick:
	case ESoccerRestartType::DirectFreeKick:
	case ESoccerRestartType::ThrowIn:
	case ESoccerRestartType::CornerKick:
		return ESoccerRestartRestrictionShape::Circle;

	case ESoccerRestartType::GoalKick:
		return ESoccerRestartRestrictionShape::PenaltyArea;

	case ESoccerRestartType::PenaltyKick:
		return ESoccerRestartRestrictionShape::None;

	case ESoccerRestartType::Kickoff:
	case ESoccerRestartType::None:
	default:
		return ESoccerRestartRestrictionShape::None;
	}
}

FVector ASoccerMatchManager::GetActiveRestartRestrictionCenter() const
{
	switch (ActiveRestartType)
	{
	case ESoccerRestartType::OffsideFreeKick:
	case ESoccerRestartType::DirectFreeKick:
		return FreeKickRestart.GetRestartLocation();

	case ESoccerRestartType::ThrowIn:
		return ThrowInLocation;

	case ESoccerRestartType::CornerKick:
		return GoalLineRestart.BallLocation;

	case ESoccerRestartType::GoalKick:
		return GetOwnGoalReferenceLocation(GoalLineRestart.DefendingTeam);

	case ESoccerRestartType::Kickoff:
	case ESoccerRestartType::None:
	default:
		return ActiveRestartLocation;
	}
}

float ASoccerMatchManager::GetActiveRestartRestrictionRadius() const
{
	switch (ActiveRestartType)
	{
	case ESoccerRestartType::OffsideFreeKick:
	case ESoccerRestartType::DirectFreeKick:
		return FMath::Max(0.0f, OffsideRestartOpponentRequiredDistance);

	case ESoccerRestartType::ThrowIn:
		return FMath::Max(0.0f, ThrowInOpponentRequiredDistance);

	case ESoccerRestartType::CornerKick:
		return FMath::Max(0.0f, CornerKickOpponentRequiredDistance);

	case ESoccerRestartType::GoalKick:
	case ESoccerRestartType::Kickoff:
	case ESoccerRestartType::None:
	default:
		return 0.0f;
	}
}

bool ASoccerMatchManager::IsLocationIllegalForActiveRestart(
	const FVector& Location,
	ESoccerTeam CharacterTeam
) const
{
	if (!IsRestartContextActive() || CharacterTeam == ActiveRestartTeam)
	{
		return false;
	}

	const ESoccerRestartRestrictionShape RestrictionShape =
		GetActiveRestartRestrictionShape();

	if (RestrictionShape == ESoccerRestartRestrictionShape::Circle)
	{
		return FVector::Dist2D(
			Location,
			GetActiveRestartRestrictionCenter()
		) < GetActiveRestartRestrictionRadius();
	}

	if (RestrictionShape == ESoccerRestartRestrictionShape::PenaltyArea)
	{
		return IsLocationInsidePenaltyAreaForTeam(
			Location,
			GoalLineRestart.DefendingTeam
		);
	}

	return false;
}

bool ASoccerMatchManager::IsCharacterIllegalForActiveRestart(
	const ASoccerCharacterBase* Character
) const
{
	return
		IsValid(Character) &&
		IsLocationIllegalForActiveRestart(
			Character->GetActorLocation(),
			Character->GetTeam()
		);
}

bool ASoccerMatchManager::AreActiveRestartOpponentsLegal() const
{
	if (GetActiveRestartRestrictionShape() ==
		ESoccerRestartRestrictionShape::None)
	{
		return true;
	}

	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return false;
	}

	for (TActorIterator<ASoccerCharacterBase> It(World); It; ++It)
	{
		const ASoccerCharacterBase* Candidate = *It;

		if (IsCharacterIllegalForActiveRestart(Candidate))
		{
			return false;
		}
	}

	return true;
}

FVector ASoccerMatchManager::BuildCircularRestartOpponentMoveLocation(
	const ASoccerAICharacter* SoccerAICharacter,
	const FVector& DesiredTacticalLocation,
	const FVector& RestrictionCenter,
	float RequiredDistance,
	float ExtraDistance
) const
{
	if (!IsValid(SoccerAICharacter))
	{
		return FVector::ZeroVector;
	}

	FVector ProjectedTacticalLocation = ProjectLocationToNavigation(
		DesiredTacticalLocation,
		SoccerAICharacter
	);

	const FVector CurrentLocation = SoccerAICharacter->GetActorLocation();

	if (ProjectedTacticalLocation.IsNearlyZero())
	{
		ProjectedTacticalLocation = CurrentLocation;
	}
	const float SafeRequiredDistance = FMath::Max(0.0f, RequiredDistance);
	const float SafeTargetDistance =
		SafeRequiredDistance + FMath::Max(20.0f, ExtraDistance);

	const bool bCurrentLocationIllegal =
		FVector::Dist2D(CurrentLocation, RestrictionCenter) <
		SafeRequiredDistance;

	const bool bDesiredLocationIllegal =
		FVector::Dist2D(ProjectedTacticalLocation, RestrictionCenter) <
		SafeRequiredDistance;

	if (!bCurrentLocationIllegal && !bDesiredLocationIllegal)
	{
		return ProjectedTacticalLocation;
	}

	const FVector ReferenceLocation =
		bCurrentLocationIllegal ? CurrentLocation : ProjectedTacticalLocation;

	FVector BaseAwayDirection = ReferenceLocation - RestrictionCenter;
	BaseAwayDirection.Z = 0.0f;

	if (BaseAwayDirection.IsNearlyZero())
	{
		BaseAwayDirection =
			GetOwnGoalReferenceLocation(SoccerAICharacter->GetTeam()) -
			RestrictionCenter;
		BaseAwayDirection.Z = 0.0f;
	}

	if (!BaseAwayDirection.Normalize())
	{
		BaseAwayDirection =
			-GetFieldAttackDirectionForTeam(SoccerAICharacter->GetTeam());
		BaseAwayDirection.Z = 0.0f;
		BaseAwayDirection.Normalize();
	}

	auto DistancePointToSegment2D = [](
		const FVector& Point,
		const FVector& SegmentStart,
		const FVector& SegmentEnd
	)
	{
		FVector Segment = SegmentEnd - SegmentStart;
		Segment.Z = 0.0f;

		FVector ToPoint = Point - SegmentStart;
		ToPoint.Z = 0.0f;

		const float SegmentSizeSquared = Segment.SizeSquared();

		if (SegmentSizeSquared <= KINDA_SMALL_NUMBER)
		{
			return ToPoint.Size();
		}

		const float Alpha = FMath::Clamp(
			FVector::DotProduct(ToPoint, Segment) / SegmentSizeSquared,
			0.0f,
			1.0f
		);

		return FVector::Dist2D(
			Point,
			SegmentStart + Segment * Alpha
		);
	};

	const float AngleStep = FMath::Clamp(
		RestartRestrictionEscapeAngleStepDegrees,
		5.0f,
		80.0f
	);

	const float CandidateAngles[] =
	{
		0.0f,
		AngleStep,
		-AngleStep,
		AngleStep * 2.0f,
		-AngleStep * 2.0f,
		AngleStep * 3.0f,
		-AngleStep * 3.0f,
		AngleStep * 4.0f,
		-AngleStep * 4.0f,
		180.0f
	};

	const float SafeFieldInset = FMath::Clamp(
		FMath::Max(0.0f, RestartRestrictionFieldInset),
		0.0f,
		FMath::Min(
			SoccerFieldDimensions::HalfPitchLengthCm - 1.0f,
			SoccerFieldDimensions::HalfPitchWidthCm - 1.0f
		)
	);

	auto ClampRestartCandidateInsideField =
		[this, SafeFieldInset, &CurrentLocation](FVector CandidateLocation)
	{
		if (IsValid(SoccerField))
		{
			CandidateLocation = SoccerField->ClampWorldLocationInsidePitch(
				CandidateLocation,
				SafeFieldInset
			);
		}
		else
		{
			CandidateLocation = SoccerFieldDimensions::ClampLocationInsidePitch(
				CandidateLocation,
				SafeFieldInset
			);
		}
		CandidateLocation.Z = CurrentLocation.Z;
		return CandidateLocation;
	};

	FVector BestMoveLocation = ClampRestartCandidateInsideField(
		RestrictionCenter + BaseAwayDirection * SafeTargetDistance
	);

	float BestScore = -TNumericLimits<float>::Max();
	UWorld* World = GetWorld();

	for (const float CandidateAngle : CandidateAngles)
	{
		FVector CandidateDirection = BaseAwayDirection.RotateAngleAxis(
			CandidateAngle,
			FVector::UpVector
		);
		CandidateDirection.Z = 0.0f;

		if (!CandidateDirection.Normalize())
		{
			continue;
		}

		FVector CandidateLocation =
			RestrictionCenter + CandidateDirection * SafeTargetDistance;

		CandidateLocation =
			ClampRestartCandidateInsideField(CandidateLocation);

		CandidateLocation = ProjectLocationToNavigation(
			CandidateLocation,
			SoccerAICharacter
		);

		CandidateLocation =
			ClampRestartCandidateInsideField(CandidateLocation);

		if (FVector::Dist2D(CandidateLocation, RestrictionCenter) <
			SafeRequiredDistance + 10.0f)
		{
			continue;
		}

		float CandidateScore =
			-FMath::Abs(CandidateAngle) * 1.5f -
			FVector::Dist2D(CurrentLocation, CandidateLocation) * 0.02f;

		if (World != nullptr)
		{
			for (TActorIterator<ASoccerCharacterBase> It(World); It; ++It)
			{
				const ASoccerCharacterBase* OtherCharacter = *It;

				if (!IsValid(OtherCharacter) ||
					OtherCharacter == SoccerAICharacter)
				{
					continue;
				}

				const float CorridorClearance = DistancePointToSegment2D(
					OtherCharacter->GetActorLocation(),
					CurrentLocation,
					CandidateLocation
				);

				const float ClearanceShortage = FMath::Max(
					0.0f,
					RestartRestrictionEscapeBodyClearance -
					CorridorClearance
				);

				CandidateScore -= ClearanceShortage * 8.0f;
			}
		}

		if (CandidateScore > BestScore)
		{
			BestScore = CandidateScore;
			BestMoveLocation = CandidateLocation;
		}
	}

	if (BestScore <= -TNumericLimits<float>::Max() * 0.5f)
	{
		for (int32 AngleIndex = 0; AngleIndex < 16; ++AngleIndex)
		{
			const float CandidateAngle =
				static_cast<float>(AngleIndex) * 22.5f;

			FVector CandidateDirection = BaseAwayDirection.RotateAngleAxis(
				CandidateAngle,
				FVector::UpVector
			);
			CandidateDirection.Z = 0.0f;

			if (!CandidateDirection.Normalize())
			{
				continue;
			}

			FVector CandidateLocation =
				RestrictionCenter +
				CandidateDirection * SafeTargetDistance;

			CandidateLocation =
				ClampRestartCandidateInsideField(CandidateLocation);

			CandidateLocation = ProjectLocationToNavigation(
				CandidateLocation,
				SoccerAICharacter
			);

			CandidateLocation =
				ClampRestartCandidateInsideField(CandidateLocation);

			if (
				FVector::Dist2D(CandidateLocation, RestrictionCenter) >=
					SafeRequiredDistance + 10.0f
			)
			{
				BestMoveLocation = CandidateLocation;
				break;
			}
		}
	}

	return BestMoveLocation;
}

FVector ASoccerMatchManager::BuildPenaltyAreaRestartOpponentMoveLocation(
	const ASoccerAICharacter* SoccerAICharacter,
	const FVector& DesiredTacticalLocation
) const
{
	if (!IsValid(SoccerAICharacter))
	{
		return FVector::ZeroVector;
	}

	FVector ProjectedTacticalLocation = ProjectLocationToNavigation(
		DesiredTacticalLocation,
		SoccerAICharacter
	);

	const FVector CurrentLocation = SoccerAICharacter->GetActorLocation();

	if (ProjectedTacticalLocation.IsNearlyZero())
	{
		ProjectedTacticalLocation = CurrentLocation;
	}

	const FVector OwnGoal =
		GetOwnGoalReferenceLocation(GoalLineRestart.DefendingTeam);
	const FVector InwardDirection =
		GetFieldAttackDirectionForTeam(GoalLineRestart.DefendingTeam);

	FVector LateralDirection =
		FVector::CrossProduct(FVector::UpVector, InwardDirection);
	LateralDirection.Z = 0.0f;

	if (!LateralDirection.Normalize())
	{
		LateralDirection = FVector::RightVector;
	}

	const float ExtraDistance =
		FMath::Max(20.0f, GoalKickOpponentPenaltyAreaExtraDistance);
	const float PenaltyDepth = SoccerFieldDimensions::PenaltyAreaDepthCm;
	const float PenaltyHalfWidth =
		SoccerFieldDimensions::PenaltyAreaHalfWidthCm;

	auto GetPenaltyCoordinates = [&](
		const FVector& Location,
		float& OutDepth,
		float& OutLateral
	)
	{
		FVector FromGoal = Location - OwnGoal;
		FromGoal.Z = 0.0f;

		OutDepth = FVector::DotProduct(
			FromGoal,
			InwardDirection
		);
		OutLateral = FVector::DotProduct(
			FromGoal,
			LateralDirection
		);
	};

	auto SegmentIntersectsSafePenaltyArea = [&](
		const FVector& SegmentStart,
		const FVector& SegmentEnd
	)
	{
		float StartDepth = 0.0f;
		float StartLateral = 0.0f;
		float EndDepth = 0.0f;
		float EndLateral = 0.0f;

		GetPenaltyCoordinates(
			SegmentStart,
			StartDepth,
			StartLateral
		);
		GetPenaltyCoordinates(
			SegmentEnd,
			EndDepth,
			EndLateral
		);

		float EnterAlpha = 0.0f;
		float ExitAlpha = 1.0f;

		auto ClipAxis = [&](
			float StartValue,
			float DeltaValue,
			float MinimumValue,
			float MaximumValue
		)
		{
			if (FMath::Abs(DeltaValue) <= KINDA_SMALL_NUMBER)
			{
				return
					StartValue >= MinimumValue &&
					StartValue <= MaximumValue;
			}

			float AxisEnterAlpha =
				(MinimumValue - StartValue) / DeltaValue;
			float AxisExitAlpha =
				(MaximumValue - StartValue) / DeltaValue;

			if (AxisEnterAlpha > AxisExitAlpha)
			{
				Swap(AxisEnterAlpha, AxisExitAlpha);
			}

			EnterAlpha = FMath::Max(
				EnterAlpha,
				AxisEnterAlpha
			);
			ExitAlpha = FMath::Min(
				ExitAlpha,
				AxisExitAlpha
			);

			return EnterAlpha <= ExitAlpha;
		};

		const float DepthDelta = EndDepth - StartDepth;
		const float LateralDelta = EndLateral - StartLateral;

		if (!ClipAxis(
			StartDepth,
			DepthDelta,
			-25.0f,
			PenaltyDepth + ExtraDistance
		))
		{
			return false;
		}

		return ClipAxis(
			StartLateral,
			LateralDelta,
			-PenaltyHalfWidth - ExtraDistance,
			PenaltyHalfWidth + ExtraDistance
		);
	};

	const bool bCurrentLocationIllegal =
		IsLocationInsidePenaltyAreaForTeam(
			CurrentLocation,
			GoalLineRestart.DefendingTeam
		);
	const bool bDesiredLocationIllegal =
		IsLocationInsidePenaltyAreaForTeam(
			ProjectedTacticalLocation,
			GoalLineRestart.DefendingTeam
		);

	if (!bCurrentLocationIllegal && !bDesiredLocationIllegal)
	{
		if (!SegmentIntersectsSafePenaltyArea(
			CurrentLocation,
			ProjectedTacticalLocation
			))
		{
			return ProjectedTacticalLocation;
		}

		// Los dos extremos pueden ser legales y aun asi el camino mas corto
		// atravesar el area penal. En ese caso se usa como waypoint una de
		// las esquinas exteriores. El bot permanece fuera durante todo el
		// recorrido y deja de alternar entre entrar y volver a salir.
		float CurrentDepth = 0.0f;
		float CurrentLateral = 0.0f;
		float DesiredDepth = 0.0f;
		float DesiredLateral = 0.0f;

		GetPenaltyCoordinates(
			CurrentLocation,
			CurrentDepth,
			CurrentLateral
		);
		GetPenaltyCoordinates(
			ProjectedTacticalLocation,
			DesiredDepth,
			DesiredLateral
		);

		float PreferredLateral = CurrentLateral;

		if (
			FMath::Abs(CurrentLateral) <=
				PenaltyHalfWidth + 5.0f &&
			FMath::Abs(DesiredLateral) >
				PenaltyHalfWidth + 5.0f
			)
		{
			PreferredLateral = DesiredLateral;
		}

		if (FMath::IsNearlyZero(PreferredLateral))
		{
			PreferredLateral =
				FMath::IsNearlyZero(DesiredLateral)
				? 1.0f
				: DesiredLateral;
		}

		const float PreferredSideSign =
			PreferredLateral >= 0.0f ? 1.0f : -1.0f;

		FVector SafeCornerWaypoint =
			OwnGoal +
			InwardDirection * (PenaltyDepth + ExtraDistance) +
			LateralDirection *
			PreferredSideSign *
			(PenaltyHalfWidth + ExtraDistance);
		SafeCornerWaypoint.Z = CurrentLocation.Z;
		SafeCornerWaypoint = ProjectLocationToNavigation(
			SafeCornerWaypoint,
			SoccerAICharacter
		);

		if (
			!SafeCornerWaypoint.IsNearlyZero() &&
			!IsLocationInsidePenaltyAreaForTeam(
				SafeCornerWaypoint,
				GoalLineRestart.DefendingTeam
			)
			)
		{
			return SafeCornerWaypoint;
		}

		// Es preferible conservar la posicion legal actual antes que enviar
		// al bot por una ruta cuyo segmento vuelve a cruzar el area.
		return CurrentLocation;
	}

	const FVector ReferenceLocation =
		bCurrentLocationIllegal ? CurrentLocation : ProjectedTacticalLocation;

	float CurrentDepth = 0.0f;
	float CurrentLateral = 0.0f;
	GetPenaltyCoordinates(
		ReferenceLocation,
		CurrentDepth,
		CurrentLateral
	);

	TArray<FVector> CandidateLocations;
	CandidateLocations.Reserve(3);

	FVector FrontCandidate =
		OwnGoal +
		InwardDirection * (PenaltyDepth + ExtraDistance) +
		LateralDirection * FMath::Clamp(
			CurrentLateral,
			-PenaltyHalfWidth - ExtraDistance,
			PenaltyHalfWidth + ExtraDistance
		);
	FrontCandidate.Z = CurrentLocation.Z;
	CandidateLocations.Add(FrontCandidate);

	const float SideDepth = FMath::Clamp(
		CurrentDepth,
		35.0f,
		PenaltyDepth - 35.0f
	);

	const float PreferredSideSign =
		CurrentLateral >= 0.0f ? 1.0f : -1.0f;
	const float SideSigns[] =
	{
		PreferredSideSign,
		-PreferredSideSign
	};

	for (const float SideSign : SideSigns)
	{
		FVector SideCandidate =
			OwnGoal +
			InwardDirection * SideDepth +
			LateralDirection *
			SideSign * (PenaltyHalfWidth + ExtraDistance);
		SideCandidate.Z = CurrentLocation.Z;
		CandidateLocations.Add(SideCandidate);
	}

	FVector BestLocation = CurrentLocation;
	float BestScore = TNumericLimits<float>::Max();

	for (int32 CandidateIndex = 0;
		CandidateIndex < CandidateLocations.Num();
		++CandidateIndex)
	{
		FVector CandidateLocation =
			ProjectLocationToNavigation(
				CandidateLocations[CandidateIndex],
				SoccerAICharacter
			);

		if (
			CandidateLocation.IsNearlyZero() ||
			IsLocationInsidePenaltyAreaForTeam(
				CandidateLocation,
				GoalLineRestart.DefendingTeam
			)
			)
		{
			continue;
		}

		float CandidateScore = FVector::Dist2D(
			CurrentLocation,
			CandidateLocation
		);

		// La segunda salida lateral es la del lado opuesto del area.
		// Solo debe usarse como ultimo recurso, no por diferencias minimas
		// causadas por proyeccion o redondeo.
		if (CandidateIndex == 2)
		{
			CandidateScore += PenaltyHalfWidth * 2.0f;
		}

		if (CandidateScore < BestScore)
		{
			BestScore = CandidateScore;
			BestLocation = CandidateLocation;
		}
	}

	return BestLocation;
}

FVector ASoccerMatchManager::BuildActiveRestartOpponentLegalLocation(
	const ASoccerAICharacter* SoccerAICharacter
) const
{
	if (!IsValid(SoccerAICharacter) ||
		SoccerAICharacter->GetTeam() == ActiveRestartTeam)
	{
		return FVector::ZeroVector;
	}

	switch (ActiveRestartType)
	{
	case ESoccerRestartType::OffsideFreeKick:
	case ESoccerRestartType::DirectFreeKick:
		return BuildOffsideRestartOpponentMoveLocation(SoccerAICharacter);

	case ESoccerRestartType::ThrowIn:
		return BuildThrowInOpponentMoveLocation(SoccerAICharacter);

	case ESoccerRestartType::CornerKick:
	case ESoccerRestartType::GoalKick:
		return BuildGoalLineRestartOpponentMoveLocation(SoccerAICharacter);

	case ESoccerRestartType::Kickoff:
	case ESoccerRestartType::None:
	default:
		return FVector::ZeroVector;
	}
}

float ASoccerMatchManager::GetActiveRestartRestrictionViolationDepth(
	const FVector& Location
) const
{
	const ESoccerRestartRestrictionShape Shape =
		GetActiveRestartRestrictionShape();

	if (Shape == ESoccerRestartRestrictionShape::Circle)
	{
		return FMath::Max(
			0.0f,
			GetActiveRestartRestrictionRadius() -
			FVector::Dist2D(
				Location,
				GetActiveRestartRestrictionCenter()
			)
		);
	}

	if (Shape == ESoccerRestartRestrictionShape::PenaltyArea)
	{
		if (!IsLocationInsidePenaltyAreaForTeam(
			Location,
			GoalLineRestart.DefendingTeam))
		{
			return 0.0f;
		}

		const FVector OwnGoal =
			GetOwnGoalReferenceLocation(GoalLineRestart.DefendingTeam);
		const FVector InwardDirection =
			GetFieldAttackDirectionForTeam(GoalLineRestart.DefendingTeam);
		FVector LateralDirection =
			FVector::CrossProduct(FVector::UpVector, InwardDirection);
		LateralDirection.Z = 0.0f;

		if (!LateralDirection.Normalize())
		{
			LateralDirection = FVector::RightVector;
		}

		const FVector FromGoal = Location - OwnGoal;
		const float Depth = FVector::DotProduct(
			FromGoal,
			InwardDirection
		);
		const float Lateral = FMath::Abs(
			FVector::DotProduct(FromGoal, LateralDirection)
		);

		const float FrontExitDistance = FMath::Max(
			0.0f,
			SoccerFieldDimensions::PenaltyAreaDepthCm - Depth
		);
		const float SideExitDistance = FMath::Max(
			0.0f,
			SoccerFieldDimensions::PenaltyAreaHalfWidthCm - Lateral
		);

		return FMath::Min(FrontExitDistance, SideExitDistance);
	}

	return 0.0f;
}

bool ASoccerMatchManager::IsActiveRestartHumanIndicatorEnabled() const
{
	switch (ActiveRestartType)
	{
	case ESoccerRestartType::OffsideFreeKick:
	case ESoccerRestartType::DirectFreeKick:
		return bEnableOffsideRestartHumanRadiusIndicator;

	case ESoccerRestartType::ThrowIn:
		return bEnableThrowInHumanRestrictionIndicator;

	case ESoccerRestartType::CornerKick:
		return bEnableCornerKickHumanRestrictionIndicator;

	case ESoccerRestartType::GoalKick:
		return bEnableGoalKickHumanPenaltyAreaIndicator;

	case ESoccerRestartType::Kickoff:
	case ESoccerRestartType::None:
	default:
		return false;
	}
}

bool ASoccerMatchManager::IsHumanOpponentIllegalForActiveRestart() const
{
	if (!IsRestartContextActive())
	{
		return false;
	}

	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return false;
	}

	for (TActorIterator<AThirdPersonCppCharacter> It(World); It; ++It)
	{
		const AThirdPersonCppCharacter* HumanCharacter = *It;

		if (IsCharacterIllegalForActiveRestart(HumanCharacter))
		{
			return true;
		}
	}

	return false;
}

void ASoccerMatchManager::UpdateActiveRestartHumanRestrictionIndicator()
{
	UWorld* World = GetWorld();

	if (World == nullptr ||
		UGameplayStatics::IsGamePaused(World) ||
		!IsActiveRestartHumanIndicatorEnabled() ||
		GetActiveRestartRestrictionShape() ==
			ESoccerRestartRestrictionShape::None ||
		!IsHumanOpponentIllegalForActiveRestart())
	{
		DestroyActiveRestartHumanRestrictionIndicator();
		return;
	}

	if (!IsValid(ActiveRestartHumanRestrictionActor))
	{
		SpawnActiveRestartHumanRestrictionIndicator();
	}
}

void ASoccerMatchManager::SpawnActiveRestartHumanRestrictionIndicator()
{
	DestroyActiveRestartHumanRestrictionIndicator();

	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return;
	}

	TSubclassOf<ASoccerRestartRadiusActor> IndicatorClass =
		(ActiveRestartType == ESoccerRestartType::OffsideFreeKick ||
		 ActiveRestartType == ESoccerRestartType::DirectFreeKick) &&
		OffsideRestartHumanRadiusActorClass
		? OffsideRestartHumanRadiusActorClass
		: RestartHumanRestrictionIndicatorActorClass;

	if (!IndicatorClass)
	{
		IndicatorClass = ASoccerRestartRadiusActor::StaticClass();
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ActiveRestartHumanRestrictionActor =
		World->SpawnActor<ASoccerRestartRadiusActor>(
			IndicatorClass,
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			SpawnParameters
		);

	if (!IsValid(ActiveRestartHumanRestrictionActor))
	{
		return;
	}

	const ESoccerRestartRestrictionShape Shape =
		GetActiveRestartRestrictionShape();

	const bool bUseLegacyOffsidePresentation =
		ActiveRestartType == ESoccerRestartType::OffsideFreeKick;
	const float IndicatorCenterZ = bUseLegacyOffsidePresentation
		? OffsideRestartHumanRadiusCircleCenterZ
		: RestartHumanRestrictionIndicatorCenterZ;
	const float IndicatorThickness = bUseLegacyOffsidePresentation
		? OffsideRestartHumanRadiusCircleThickness
		: RestartHumanRestrictionIndicatorThickness;
	const FLinearColor IndicatorColor = bUseLegacyOffsidePresentation
		? OffsideRestartHumanRadiusCircleColor
		: RestartHumanRestrictionIndicatorColor;

	if (Shape == ESoccerRestartRestrictionShape::Circle)
	{
		FVector CircleCenter = GetActiveRestartRestrictionCenter();
		CircleCenter.Z = IndicatorCenterZ;

		ActiveRestartHumanRestrictionActor->ConfigureRadiusCircle(
			CircleCenter,
			GetActiveRestartRestrictionRadius(),
			IndicatorThickness,
			IndicatorColor
		);
		return;
	}

	if (Shape == ESoccerRestartRestrictionShape::PenaltyArea)
	{
		const FVector InwardDirection =
			GetFieldAttackDirectionForTeam(GoalLineRestart.DefendingTeam);
		const float GoalLineSign =
			GetOwnGoalLineSign(GoalLineRestart.DefendingTeam);

		FVector RectangleCenter = IsValid(SoccerField)
			? SoccerField->GetGoalCenterWorldLocation(
				GoalLineSign,
				IndicatorCenterZ + 4.0f
			)
			: FVector(
				SoccerFieldDimensions::GetGoalLineX(GoalLineSign),
				SoccerFieldDimensions::CenterY,
				IndicatorCenterZ + 4.0f
			);

		RectangleCenter +=
			InwardDirection *
			(SoccerFieldDimensions::PenaltyAreaDepthCm * 0.5f);

		ActiveRestartHumanRestrictionActor->ConfigurePenaltyAreaRectangle(
			RectangleCenter,
			InwardDirection,
			SoccerFieldDimensions::PenaltyAreaDepthCm,
			SoccerFieldDimensions::PenaltyAreaHalfWidthCm,
			IndicatorThickness,
			IndicatorColor
		);
	}
}

void ASoccerMatchManager::DestroyActiveRestartHumanRestrictionIndicator()
{
	if (IsValid(ActiveRestartHumanRestrictionActor))
	{
		ActiveRestartHumanRestrictionActor->Destroy();
	}

	ActiveRestartHumanRestrictionActor = nullptr;
}

FVector ASoccerMatchManager::BuildRestartEmergencyRecoveryStep(
	const ASoccerAICharacter* SoccerAICharacter,
	const FVector& LegalTargetLocation,
	float DeltaTime
) const
{
	if (!IsValid(SoccerAICharacter) || DeltaTime <= KINDA_SMALL_NUMBER)
	{
		return IsValid(SoccerAICharacter)
			? SoccerAICharacter->GetActorLocation()
			: FVector::ZeroVector;
	}

	const FVector CurrentLocation = SoccerAICharacter->GetActorLocation();
	FVector DirectDirection = LegalTargetLocation - CurrentLocation;
	DirectDirection.Z = 0.0f;

	if (!DirectDirection.Normalize())
	{
		return CurrentLocation;
	}

	const float StepDistance =
		FMath::Max(1.0f, RestartIllegalBotEmergencySpeed) * DeltaTime;
	const float AngleStep = FMath::Clamp(
		RestartRestrictionEscapeAngleStepDegrees,
		5.0f,
		80.0f
	);
	const float CandidateAngles[] =
	{
		0.0f,
		AngleStep,
		-AngleStep,
		AngleStep * 2.0f,
		-AngleStep * 2.0f
	};

	auto DistancePointToSegment2D = [](
		const FVector& Point,
		const FVector& SegmentStart,
		const FVector& SegmentEnd
	)
	{
		FVector Segment = SegmentEnd - SegmentStart;
		Segment.Z = 0.0f;
		FVector ToPoint = Point - SegmentStart;
		ToPoint.Z = 0.0f;
		const float SegmentSizeSquared = Segment.SizeSquared();

		if (SegmentSizeSquared <= KINDA_SMALL_NUMBER)
		{
			return ToPoint.Size();
		}

		const float Alpha = FMath::Clamp(
			FVector::DotProduct(ToPoint, Segment) / SegmentSizeSquared,
			0.0f,
			1.0f
		);
		return FVector::Dist2D(
			Point,
			SegmentStart + Segment * Alpha
		);
	};

	FVector BestStepLocation = CurrentLocation;
	float BestScore = -TNumericLimits<float>::Max();
	UWorld* World = GetWorld();

	for (const float CandidateAngle : CandidateAngles)
	{
		FVector CandidateDirection = DirectDirection.RotateAngleAxis(
			CandidateAngle,
			FVector::UpVector
		);
		CandidateDirection.Z = 0.0f;

		if (!CandidateDirection.Normalize())
		{
			continue;
		}

		FVector CandidateLocation =
			CurrentLocation + CandidateDirection * StepDistance;
		CandidateLocation.Z = CurrentLocation.Z;
		CandidateLocation = ProjectLocationToNavigation(
			CandidateLocation,
			SoccerAICharacter
		);

		const float ViolationDepth =
			GetActiveRestartRestrictionViolationDepth(CandidateLocation);

		float CandidateScore =
			-ViolationDepth * 1000.0f -
			FVector::Dist2D(CandidateLocation, LegalTargetLocation) -
			FMath::Abs(CandidateAngle) * 0.5f;

		if (ViolationDepth <= KINDA_SMALL_NUMBER)
		{
			CandidateScore += 1000000.0f;
		}

		if (World != nullptr)
		{
			for (TActorIterator<ASoccerCharacterBase> It(World); It; ++It)
			{
				const ASoccerCharacterBase* OtherCharacter = *It;

				if (!IsValid(OtherCharacter) ||
					OtherCharacter == SoccerAICharacter)
				{
					continue;
				}

				const float CorridorClearance = DistancePointToSegment2D(
					OtherCharacter->GetActorLocation(),
					CurrentLocation,
					CandidateLocation
				);
				const float ClearanceShortage = FMath::Max(
					0.0f,
					RestartRestrictionEscapeBodyClearance -
					CorridorClearance
				);
				CandidateScore -= ClearanceShortage * 12.0f;
			}
		}

		if (CandidateScore > BestScore)
		{
			BestScore = CandidateScore;
			BestStepLocation = CandidateLocation;
		}
	}

	return BestStepLocation;
}

void ASoccerMatchManager::UpdateActiveRestartIllegalBotRecovery(
	float DeltaTime
)
{
	if (GetActiveRestartRestrictionShape() ==
		ESoccerRestartRestrictionShape::None)
	{
		ResetActiveRestartRestrictionRecovery();
		return;
	}

	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return;
	}

	const float CurrentTime = World->GetTimeSeconds();
	TSet<ASoccerAICharacter*> CurrentIllegalBots;

	for (TActorIterator<ASoccerAICharacter> It(World); It; ++It)
	{
		ASoccerAICharacter* Candidate = *It;

		if (!IsValid(Candidate) ||
			Candidate->GetTeam() == ActiveRestartTeam)
		{
			continue;
		}

		if (!IsCharacterIllegalForActiveRestart(Candidate))
		{
			ActiveRestartIllegalBotSinceTimes.Remove(Candidate);
			ActiveRestartEmergencyTargetLocations.Remove(Candidate);

			if (ActiveRestartEmergencyAssistedBots.Remove(Candidate) > 0)
			{
				Candidate->ClearScriptedLocomotionVelocity();
			}
			continue;
		}

		CurrentIllegalBots.Add(Candidate);

		float* IllegalSince =
			ActiveRestartIllegalBotSinceTimes.Find(Candidate);

		if (IllegalSince == nullptr)
		{
			ActiveRestartIllegalBotSinceTimes.Add(Candidate, CurrentTime);
			continue;
		}

		if (CurrentTime - *IllegalSince < RestartIllegalBotEmergencyDelay)
		{
			continue;
		}

		FVector LegalTargetLocation = FVector::ZeroVector;

		if (const FVector* ExistingTarget =
			ActiveRestartEmergencyTargetLocations.Find(Candidate))
		{
			LegalTargetLocation = *ExistingTarget;
		}
		else
		{
			LegalTargetLocation =
				BuildActiveRestartOpponentLegalLocation(Candidate);

			if (
				LegalTargetLocation.IsNearlyZero() ||
				IsLocationIllegalForActiveRestart(
					LegalTargetLocation,
					Candidate->GetTeam()
				)
				)
			{
				continue;
			}

			// El destino de emergencia queda fijo mientras el bot siga
			// ilegal. Recalcularlo cada frame cerca de una esquina del area
			// hacia alternar entre la salida frontal y la lateral.
			ActiveRestartEmergencyTargetLocations.Add(
				Candidate,
				LegalTargetLocation
			);
		}

		ActiveRestartAITargetLocations.Add(
			Candidate,
			LegalTargetLocation
		);

		if (
			ActiveRestartType == ESoccerRestartType::OffsideFreeKick ||
			ActiveRestartType == ESoccerRestartType::DirectFreeKick
		)
		{
			FreeKickRestart.AdoptOpponentRecoveryTarget(
				Candidate,
				LegalTargetLocation
			);
		}

		const FVector CurrentLocation = Candidate->GetActorLocation();
		const FVector StepLocation = BuildRestartEmergencyRecoveryStep(
			Candidate,
			LegalTargetLocation,
			DeltaTime
		);

		Candidate->SetActorLocation(
			StepLocation,
			true,
			nullptr,
			ETeleportType::None
		);

		const FVector ActualLocation = Candidate->GetActorLocation();
		FVector ScriptedVelocity =
			DeltaTime > KINDA_SMALL_NUMBER
			? (ActualLocation - CurrentLocation) / DeltaTime
			: FVector::ZeroVector;
		ScriptedVelocity.Z = 0.0f;

		if (!ScriptedVelocity.IsNearlyZero())
		{
			Candidate->SetActorRotation(
				ScriptedVelocity.GetSafeNormal().Rotation()
			);
			Candidate->SetScriptedLocomotionVelocity(
				ScriptedVelocity,
				ESoccerAIMovementMode::Jog,
				ESoccerAIMovementReason::NearbyReposition
			);
			ActiveRestartEmergencyAssistedBots.Add(Candidate);
		}
	}

	for (auto It = ActiveRestartIllegalBotSinceTimes.CreateIterator(); It; ++It)
	{
		if (!IsValid(It.Key()) || !CurrentIllegalBots.Contains(It.Key()))
		{
			ActiveRestartEmergencyTargetLocations.Remove(It.Key());
			It.RemoveCurrent();
		}
	}
}

void ASoccerMatchManager::ResetActiveRestartRestrictionRecovery()
{
	for (ASoccerAICharacter* AssistedBot :
		ActiveRestartEmergencyAssistedBots)
	{
		if (IsValid(AssistedBot))
		{
			AssistedBot->ClearScriptedLocomotionVelocity();
		}
	}

	ActiveRestartEmergencyAssistedBots.Empty();
	ActiveRestartIllegalBotSinceTimes.Empty();
	ActiveRestartEmergencyTargetLocations.Empty();
}

bool ASoccerMatchManager::IsNonFreeKickHumanTakerClaimedFor(
	ESoccerRestartType ExpectedRestartType
) const
{
	return
		bActiveNonFreeKickHumanTakerClaimed &&
		ActiveNonFreeKickHumanTakerType == ExpectedRestartType &&
		IsValid(ActiveNonFreeKickHumanTaker);
}

bool ASoccerMatchManager::UpdateNonFreeKickHumanTakerClaimDuringPreparation(
	ESoccerRestartType ExpectedRestartType
)
{
	if (
		!IsRestartContextActive() ||
		ActiveRestartType != ExpectedRestartType ||
		(
			ExpectedRestartType != ESoccerRestartType::Kickoff &&
			ExpectedRestartType != ESoccerRestartType::GoalKick &&
			ExpectedRestartType != ESoccerRestartType::CornerKick
		)
	)
	{
		return false;
	}

	AThirdPersonCppCharacter* CandidateHuman =
		FindHumanCharacterForTeam(ActiveRestartTeam);

	const bool bPreviousClaim =
		IsNonFreeKickHumanTakerClaimedFor(ExpectedRestartType);

	ActiveNonFreeKickHumanTaker = IsValid(CandidateHuman)
		? CandidateHuman
		: nullptr;
	ActiveNonFreeKickHumanTakerType = ExpectedRestartType;
	bActiveNonFreeKickHumanExecutionAuthorized = false;

	if (!IsValid(ActiveNonFreeKickHumanTaker))
	{
		bActiveNonFreeKickHumanTakerClaimed = false;
		return bPreviousClaim;
	}

	const float ClaimRadius =
		FMath::Max(50.0f, RestartHumanTakerClaimRadius);
	const float ReleaseRadius =
		FMath::Max(ClaimRadius, RestartHumanTakerReleaseRadius);

	const float DistanceToRestart = FVector::Dist2D(
		ActiveNonFreeKickHumanTaker->GetActorLocation(),
		ActiveRestartLocation
	);

	bActiveNonFreeKickHumanTakerClaimed = bPreviousClaim
		? DistanceToRestart <= ReleaseRadius
		: DistanceToRestart <= ClaimRadius;

	if (bPreviousClaim != bActiveNonFreeKickHumanTakerClaimed)
	{
		const TCHAR* RestartLabel = TEXT("REANUDACION");
		if (ExpectedRestartType == ESoccerRestartType::Kickoff)
		{
			RestartLabel = TEXT("KICKOFF");
		}
		else if (ExpectedRestartType == ESoccerRestartType::GoalKick)
		{
			RestartLabel = TEXT("SAQUE DE ARCO");
		}
		else if (ExpectedRestartType == ESoccerRestartType::CornerKick)
		{
			RestartLabel = TEXT("CORNER");
		}

		ASoccerDebugManager::Message(
			this,
			ESoccerDebugCategory::Restarts,
			FString::Printf(
				TEXT("%s: humano %s el saque"),
				RestartLabel,
				bActiveNonFreeKickHumanTakerClaimed
					? TEXT("reclama")
					: TEXT("cede")
			),
			bActiveNonFreeKickHumanTakerClaimed
				? FColor::Cyan
				: FColor::Yellow
		);
	}

	return bPreviousClaim != bActiveNonFreeKickHumanTakerClaimed;
}

bool ASoccerMatchManager::ShouldNonFreeKickHumanTakerKeepExecutionClaim(
	ESoccerRestartType ExpectedRestartType
) const
{
	if (
		!bActiveNonFreeKickHumanExecutionAuthorized ||
		!IsNonFreeKickHumanTakerClaimedFor(ExpectedRestartType)
	)
	{
		return false;
	}

	const float ReleaseRadius = FMath::Max(
		FMath::Max(50.0f, RestartHumanTakerClaimRadius),
		RestartHumanTakerReleaseRadius
	);

	return FVector::Dist2D(
		ActiveNonFreeKickHumanTaker->GetActorLocation(),
		ActiveRestartLocation
	) <= ReleaseRadius;
}

void ASoccerMatchManager::AuthorizeNonFreeKickHumanTakerExecution(
	ESoccerRestartType ExpectedRestartType
)
{
	bActiveNonFreeKickHumanExecutionAuthorized =
		IsNonFreeKickHumanTakerClaimedFor(ExpectedRestartType) &&
		IsRestartContextActive() &&
		ActiveRestartType == ExpectedRestartType;
}

void ASoccerMatchManager::ReleaseNonFreeKickHumanTakerClaim()
{
	bActiveNonFreeKickHumanTakerClaimed = false;
	bActiveNonFreeKickHumanExecutionAuthorized = false;
}

void ASoccerMatchManager::ResetNonFreeKickHumanTakerRuntime()
{
	ActiveNonFreeKickHumanTaker = nullptr;
	ActiveNonFreeKickHumanTakerType = ESoccerRestartType::None;
	bActiveNonFreeKickHumanTakerClaimed = false;
	bActiveNonFreeKickHumanExecutionAuthorized = false;
}

FVector ASoccerMatchManager::BuildNonFreeKickFallbackTakerMoveLocation(
	const ASoccerAICharacter* SoccerAICharacter
) const
{
	if (!IsValid(SoccerAICharacter))
	{
		return FVector::ZeroVector;
	}

	if (SoccerAICharacter->GetPlayerRole() == ESoccerPlayerRole::Goalkeeper)
	{
		return ProjectLocationToNavigation(
			GetGoalkeeperMoveLocation(SoccerAICharacter),
			SoccerAICharacter
		);
	}

	const ESoccerAIOrder RestartAttackOrder =
		GetDefaultRestartAttackOrderForCharacter(SoccerAICharacter);

	FVector DesiredLocation = BuildAttackShapeLocation(
		SoccerAICharacter,
		RestartAttackOrder
	);
	DesiredLocation = ApplyOffsideSafetyToAttackMoveLocation(
		SoccerAICharacter,
		DesiredLocation,
		RestartAttackOrder
	);

	return ProjectLocationToNavigation(DesiredLocation, SoccerAICharacter);
}

void ASoccerMatchManager::BeginRestartContext(
	ESoccerRestartType RestartType,
	ESoccerTeam RestartTeam,
	const FVector& RestartLocation
)
{
	DestroyActiveRestartHumanRestrictionIndicator();
	ResetActiveRestartRestrictionRecovery();
	ClearActiveRestartExecutionReceiver();
	ResetActiveRestartLivePositioning();

	bRestartContextActive = RestartType != ESoccerRestartType::None;
	ActiveRestartType = RestartType;
	ActiveRestartTeam = RestartTeam;
	ActiveRestartLocation = RestartLocation;
	bActiveRestartPreviousLegalConditionsSatisfied = false;
	ActiveRestartAITargetLocations.Empty();
	ActiveRestartLastPositioningRecoveryTime =
		GetWorld() != nullptr
		? GetWorld()->GetTimeSeconds()
		: -1000.0f;
	ResetActiveRestartReadyHold();
}

void ASoccerMatchManager::EndRestartContext()
{
	DestroyActiveRestartHumanRestrictionIndicator();
	ResetActiveRestartRestrictionRecovery();
	ClearActiveRestartExecutionReceiver();
	ResetActiveRestartLivePositioning();
	ResetKickoffRunUpState();
	FreeKickRestart.ResetRuntime(*this);
	GoalLineRestart.ResetGoalKickFinalRunRuntime(*this);
	GoalLineRestart.ResetCornerFinalRunRuntime(*this);
	ResetNonFreeKickHumanTakerRuntime();

	if (ActiveRestartType == ESoccerRestartType::ThrowIn)
	{
		bThrowInStagedDuringBallOutOfPlayDelay = false;
	}
	else if (
		ActiveRestartType == ESoccerRestartType::CornerKick ||
		ActiveRestartType == ESoccerRestartType::GoalKick
		)
	{
		bGoalLineRestartStagedDuringBallOutOfPlayDelay = false;
	}

	bRestartContextActive = false;
	ActiveRestartType = ESoccerRestartType::None;
	ActiveRestartLocation = FVector::ZeroVector;
	bActiveRestartPreviousLegalConditionsSatisfied = false;
	bCapturingActiveRestartAITargetLocations = false;
	ActiveRestartAITargetLocations.Empty();
	ActiveRestartLastPositioningRecoveryTime = -1000.0f;
	ResetActiveRestartReadyHold();
}

bool ASoccerMatchManager::IsRestartContextActive() const
{
	return bRestartContextActive &&
		ActiveRestartType != ESoccerRestartType::None;
}

void ASoccerMatchManager::ResetActiveRestartReadyHold()
{
	ActiveRestartAllBotsReadySince = -1000.0f;
}

bool ASoccerMatchManager::IsActiveRestartLivePositioningActive() const
{
	const bool bSupportedRestart =
		ActiveRestartType == ESoccerRestartType::ThrowIn ||
		ActiveRestartType == ESoccerRestartType::CornerKick;

	return
		bActiveRestartLivePositioning &&
		IsRestartContextActive() &&
		bSupportedRestart;
}

bool ASoccerMatchManager::HasActiveRestartLivePositioningPlan(
	const ASoccerAICharacter* SoccerAICharacter
) const
{
	return
		IsActiveRestartLivePositioningActive() &&
		IsValid(SoccerAICharacter) &&
		(
			ActiveRestartLiveAttackingPlans.Contains(SoccerAICharacter) ||
			ActiveRestartLiveDefensivePlans.Contains(SoccerAICharacter)
		);
}

float ASoccerMatchManager::GetActiveRestartLivePositioningMoveAcceptanceRadius()
	const
{
	return FMath::Max(5.0f, RestartLiveMoveAcceptanceRadius);
}

void ASoccerMatchManager::CommitActiveRestartLivePositioningForHumanAction()
{
	LockActiveRestartLivePositioning();
}

void ASoccerMatchManager::ResetActiveRestartLivePositioning()
{
	bActiveRestartLivePositioning = false;
	bActiveRestartLivePositioningLocked = false;
	ActiveRestartLivePositioningStartTime = -1000.0f;
	ActiveRestartLivePositioningAIWaitEndTime = -1000.0f;
	ActiveRestartLiveNextDefensiveDecisionTime = -1000.0f;
	ActiveRestartLiveDefensiveDecisionIndex = 0;
	ActiveRestartLiveAttackingPlans.Empty();
	ActiveRestartLiveDefensivePlans.Empty();
}

int32 ASoccerMatchManager::BuildActiveRestartLivePositioningSeed(
	const ASoccerCharacterBase* SubjectCharacter,
	int32 DecisionIndex,
	int32 Salt
) const
{
	uint32 Seed = HashCombine(
		GetTypeHash(ActiveRestartLivePositioningSequence),
		GetTypeHash(DecisionIndex)
	);
	Seed = HashCombine(Seed, GetTypeHash(Salt));

	if (IsValid(SubjectCharacter))
	{
		Seed = HashCombine(Seed, GetTypeHash(SubjectCharacter->GetUniqueID()));
	}

	return static_cast<int32>(Seed & 0x7fffffffu);
}

void ASoccerMatchManager::BeginActiveRestartLivePositioning()
{
	UWorld* World = GetWorld();
	const bool bSupportedRestart =
		(
			ActiveRestartType == ESoccerRestartType::ThrowIn &&
			bEnableThrowInLivePositioning
		) ||
		(
			ActiveRestartType == ESoccerRestartType::CornerKick &&
			bEnableCornerKickLivePositioning
		);
	if (
		!bEnableRestartLivePositioning ||
		!bSupportedRestart ||
		World == nullptr ||
		GetNetMode() == NM_Client ||
		!IsRestartContextActive()
	)
	{
		return;
	}

	ResetActiveRestartLivePositioning();
	bActiveRestartLivePositioning = true;
	bActiveRestartLivePositioningLocked = false;
	ActiveRestartLivePositioningSequence++;
	if (ActiveRestartLivePositioningSequence <= 0)
	{
		ActiveRestartLivePositioningSequence = 1;
	}

	const float CurrentWorldTime = World->GetTimeSeconds();
	ActiveRestartLivePositioningStartTime = CurrentWorldTime;

	const float SafeMinimumWait = FMath::Max(
		0.0f,
		RestartLiveAIWaitMinTime
	);
	const float SafeMaximumWait = FMath::Max(
		SafeMinimumWait,
		RestartLiveAIWaitMaxTime
	);
	FRandomStream WaitRandom(BuildActiveRestartLivePositioningSeed(
		nullptr,
		0,
		101
	));
	ActiveRestartLivePositioningAIWaitEndTime =
		CurrentWorldTime +
		WaitRandom.FRandRange(SafeMinimumWait, SafeMaximumWait);

	InitializeActiveRestartLivePositioningPlans(CurrentWorldTime);

	ASoccerDebugManager::Message(
		this,
		ESoccerDebugCategory::Restarts,
		ActiveRestartType == ESoccerRestartType::CornerKick
			? TEXT("CORNER: comienza disputa dinamica de posiciones")
			: TEXT("LATERAL: comienza disputa dinamica de posiciones"),
		FColor::Cyan
	);
}

void ASoccerMatchManager::LockActiveRestartLivePositioning()
{
	if (!IsActiveRestartLivePositioningActive())
	{
		return;
	}

	bActiveRestartLivePositioningLocked = true;
}

bool ASoccerMatchManager::IsActiveRestartAILivePositioningWaitComplete() const
{
	if (!IsActiveRestartLivePositioningActive())
	{
		return true;
	}

	if (bActiveRestartLivePositioningLocked)
	{
		return true;
	}

	const UWorld* World = GetWorld();
	return
		World == nullptr ||
		World->GetTimeSeconds() >=
			ActiveRestartLivePositioningAIWaitEndTime;
}

FVector ASoccerMatchManager::SanitizeActiveRestartLivePositioningTarget(
	const ASoccerAICharacter* SoccerAICharacter,
	const FVector& RequestedLocation
) const
{
	if (!IsValid(SoccerAICharacter))
	{
		return RequestedLocation;
	}

	const float SafeFieldInset = FMath::Max(0.0f, RestartLiveFieldInset);
	FVector ResultLocation = RequestedLocation;

	if (IsValid(SoccerField))
	{
		ResultLocation = SoccerField->ClampWorldLocationInsidePitch(
			ResultLocation,
			SafeFieldInset
		);
	}
	else
	{
		ResultLocation = SoccerFieldDimensions::ClampLocationInsidePitch(
			ResultLocation,
			SafeFieldInset
		);
	}

	ResultLocation.Z = SoccerAICharacter->GetActorLocation().Z;
	ResultLocation = ProjectLocationToNavigation(
		ResultLocation,
		SoccerAICharacter
	);

	if (IsValid(SoccerField))
	{
		ResultLocation = SoccerField->ClampWorldLocationInsidePitch(
			ResultLocation,
			SafeFieldInset
		);
	}
	else
	{
		ResultLocation = SoccerFieldDimensions::ClampLocationInsidePitch(
			ResultLocation,
			SafeFieldInset
		);
	}

	ResultLocation.Z = SoccerAICharacter->GetActorLocation().Z;
	return ResultLocation;
}

bool ASoccerMatchManager::IsActiveRestartLiveTaker(
	const ASoccerCharacterBase* Character
) const
{
	if (!IsValid(Character))
	{
		return false;
	}

	if (const ASoccerAICharacter* SoccerAICharacter =
		Cast<ASoccerAICharacter>(Character))
	{
		if (ActiveRestartType == ESoccerRestartType::ThrowIn)
		{
			return IsThrowInTaker(SoccerAICharacter);
		}

		if (ActiveRestartType == ESoccerRestartType::CornerKick)
		{
			return IsGoalLineRestartTaker(SoccerAICharacter);
		}
	}

	if (const AThirdPersonCppCharacter* HumanCharacter =
		Cast<AThirdPersonCppCharacter>(Character))
	{
		if (ActiveRestartType == ESoccerRestartType::ThrowIn)
		{
			return IsHumanThrowInTaker(HumanCharacter);
		}

		if (ActiveRestartType == ESoccerRestartType::CornerKick)
		{
			return IsHumanFootRestartTaker(HumanCharacter);
		}
	}

	return false;
}

void ASoccerMatchManager::InitializeActiveRestartLivePositioningPlans(
	float CurrentWorldTime
)
{
	UWorld* World = GetWorld();
	if (
		World == nullptr ||
		!IsActiveRestartLivePositioningActive()
	)
	{
		return;
	}

	ActiveRestartLiveAttackingPlans.Empty();
	ActiveRestartLiveDefensivePlans.Empty();

	TArray<ASoccerAICharacter*> AttackingCharacters;
	TArray<ASoccerAICharacter*> DefendingCharacters;

	for (TActorIterator<ASoccerAICharacter> It(World); It; ++It)
	{
		ASoccerAICharacter* CandidateCharacter = *It;
		if (
			!IsValid(CandidateCharacter) ||
			CandidateCharacter->IsHidden() ||
			CandidateCharacter->ActorHasTag(
				FName(TEXT("SubstitutionPresentation"))
			) ||
			CandidateCharacter->GetPlayerRole() ==
				ESoccerPlayerRole::Goalkeeper
		)
		{
			continue;
		}

		if (CandidateCharacter->GetTeam() == ActiveRestartTeam)
		{
			if (!IsActiveRestartLiveTaker(CandidateCharacter))
			{
				AttackingCharacters.Add(CandidateCharacter);
			}
		}
		else
		{
			DefendingCharacters.Add(CandidateCharacter);
		}
	}

	AttackingCharacters.Sort(
		[](const ASoccerAICharacter& A, const ASoccerAICharacter& B)
		{
			return A.GetUniqueID() < B.GetUniqueID();
		}
	);
	DefendingCharacters.Sort(
		[](const ASoccerAICharacter& A, const ASoccerAICharacter& B)
		{
			return A.GetUniqueID() < B.GetUniqueID();
		}
	);

	auto GetFixedTarget = [this](const ASoccerAICharacter* Character)
	{
		if (const FVector* CapturedTarget =
			ActiveRestartAITargetLocations.Find(Character))
		{
			return *CapturedTarget;
		}

		return BuildActiveRestartMoveLocation(Character);
	};

	for (ASoccerAICharacter* AttackingCharacter : AttackingCharacters)
	{
		FVector BaseTarget = GetFixedTarget(AttackingCharacter);
		if (BaseTarget.IsNearlyZero())
		{
			BaseTarget = AttackingCharacter->GetActorLocation();
		}
		BaseTarget = SanitizeActiveRestartLivePositioningTarget(
			AttackingCharacter,
			BaseTarget
		);

		FRestartLivePositioningPlan NewPlan;
		NewPlan.BaseTargetLocation = BaseTarget;
		NewPlan.CommittedTargetLocation = BaseTarget;
		NewPlan.NextDecisionWorldTime = CurrentWorldTime;
		ActiveRestartLiveAttackingPlans.Add(AttackingCharacter, NewPlan);
	}

	for (ASoccerAICharacter* DefendingCharacter : DefendingCharacters)
	{
		FVector BaseTarget = GetFixedTarget(DefendingCharacter);
		if (BaseTarget.IsNearlyZero())
		{
			BaseTarget = DefendingCharacter->GetActorLocation();
		}
		BaseTarget = SanitizeActiveRestartLivePositioningTarget(
			DefendingCharacter,
			BaseTarget
		);

		FRestartLivePositioningPlan NewPlan;
		NewPlan.BaseTargetLocation = BaseTarget;
		NewPlan.CommittedTargetLocation = BaseTarget;
		NewPlan.NextDecisionWorldTime = CurrentWorldTime;
		ActiveRestartLiveDefensivePlans.Add(DefendingCharacter, NewPlan);
	}

	UpdateActiveRestartLiveAttackingPlans(CurrentWorldTime, true);
	UpdateActiveRestartLiveDefensivePlans(CurrentWorldTime, true);

	int32 RepositioningAttackerCount = 0;
	for (const TPair<const ASoccerAICharacter*, FRestartLivePositioningPlan>& Pair :
		ActiveRestartLiveAttackingPlans)
	{
		if (
			IsValid(Pair.Key) &&
			FVector::Dist2D(
				Pair.Value.BaseTargetLocation,
				Pair.Value.CommittedTargetLocation
			) >= FMath::Max(
				20.0f,
				RestartLiveAttackMinimumRelocationDistance
			)
		)
		{
			RepositioningAttackerCount++;
		}
	}

	ASoccerDebugManager::Message(
		this,
		ESoccerDebugCategory::Restarts,
		FString::Printf(
			TEXT("%s: atacantes con nueva posicion %d/%d"),
			ActiveRestartType == ESoccerRestartType::CornerKick
				? TEXT("CORNER")
				: TEXT("LATERAL"),
			RepositioningAttackerCount,
			ActiveRestartLiveAttackingPlans.Num()
		),
		RepositioningAttackerCount > 0 ? FColor::Green : FColor::Orange
	);
}

float ASoccerMatchManager::ScoreActiveRestartLiveAttackingCandidate(
	const ASoccerAICharacter* SoccerAICharacter,
	const FVector& CandidateLocation,
	const FVector& BaseLocation,
	int32 DecisionIndex,
	int32 CandidateIndex
) const
{
	if (
		!IsValid(SoccerAICharacter) ||
		SoccerAICharacter->GetTeam() != ActiveRestartTeam
	)
	{
		return -BIG_NUMBER;
	}

	const bool bCornerKick =
		ActiveRestartType == ESoccerRestartType::CornerKick;
	const FVector RestartBallLocation = bCornerKick
		? GoalLineRestart.GetBallLocation()
		: ThrowInLocation;
	const ASoccerAICharacter* RestartTakerAI = bCornerKick
		? GoalLineRestart.GetTaker()
		: ThrowInTakerAI;

	float Score = 1000.0f;
	Score -= FVector::Dist2D(CandidateLocation, BaseLocation) *
		FMath::Max(0.0f, RestartLiveAttackBaseDistancePenalty);

	const float OwnArrivalTime =
		SoccerAICharacter->EstimateArrivalTimeToLocation(CandidateLocation);
	const float OpponentArrivalTime =
		GetEarliestOpponentArrivalTimeToLocation(
			ActiveRestartTeam,
			CandidateLocation
		);
	// Do not use a direct OpponentArrival - OwnArrival margin here. Because the
	// player has already reached the fixed setup target, OwnArrival is zero at
	// that exact point and used to give it an artificial permanent advantage.
	// Reward genuinely free space strongly and charge only a modest travel cost
	// for offering a few metres away.
	const float MaximumRelevantArrivalTime = 2.50f;
	if (
		FMath::IsFinite(OpponentArrivalTime) &&
		OpponentArrivalTime < BIG_NUMBER * 0.5f
	)
	{
		Score += FMath::Clamp(
			OpponentArrivalTime,
			0.0f,
			MaximumRelevantArrivalTime
		) *
			FMath::Max(
				0.0f,
				RestartLiveAttackArrivalMarginWeight
			);
	}

	if (
		FMath::IsFinite(OwnArrivalTime) &&
		OwnArrivalTime < BIG_NUMBER * 0.5f
	)
	{
		Score -= FMath::Clamp(
			OwnArrivalTime,
			0.0f,
			MaximumRelevantArrivalTime
		) * FMath::Max(
			0.0f,
			RestartLiveAttackOwnTravelTimePenaltyWeight
		);
	}

	if (IsOpponentBlockingLaneBetweenLocations(
		ActiveRestartTeam,
		RestartBallLocation,
		CandidateLocation,
		FMath::Max(0.0f, RestartReceiverAerialLaneHalfWidth)
	))
	{
		Score -= FMath::Max(0.0f, RestartLiveAttackBlockedLanePenalty);
	}

	const float RestartReceiverScore = ScoreRestartPassReceiverCandidate(
		ActiveRestartTeam,
		RestartTakerAI,
		SoccerAICharacter,
		CandidateLocation,
		true
	);
	if (RestartReceiverScore > -BIG_NUMBER * 0.5f)
	{
		Score += RestartReceiverScore * 0.20f;
	}
	else
	{
		Score -= 520.0f;
	}

	if (bCornerKick)
	{
		const FVector AttackedGoalLocation =
			GetOpponentGoalReferenceLocation(ActiveRestartTeam);
		const float IdealGoalDistance =
			SoccerFieldDimensions::ScaleAuthoredLongitudinalDistance(
				FMath::Max(100.0f, RestartLiveCornerIdealGoalDistance)
			);
		Score -= FMath::Abs(
			FVector::Dist2D(CandidateLocation, AttackedGoalLocation) -
			IdealGoalDistance
		) * FMath::Max(
			0.0f,
			RestartLiveCornerGoalDistancePenaltyWeight
		);
	}
	else
	{
		const float IdealPassDistance =
			SoccerFieldDimensions::ScaleAuthoredLongitudinalDistance(900.0f);
		Score -= FMath::Abs(
			FVector::Dist2D(RestartBallLocation, CandidateLocation) -
			IdealPassDistance
		) * 0.12f;
	}

	UWorld* World = GetWorld();
	if (World != nullptr)
	{
		const float SafeTeammateRadius = FMath::Max(
			20.0f,
			RestartLiveAttackTeammateAvoidRadius
		);
		const float SafeReservationRadius = FMath::Max(
			20.0f,
			RestartLiveAttackReservationRadius
		);
		const float SafeCrowdingPenalty = FMath::Max(
			0.0f,
			RestartLiveAttackCrowdingPenalty
		);

		for (TActorIterator<ASoccerCharacterBase> It(World); It; ++It)
		{
			const ASoccerCharacterBase* Teammate = *It;
			if (
				!IsValid(Teammate) ||
				Teammate == SoccerAICharacter ||
				Teammate->GetTeam() != ActiveRestartTeam
			)
			{
				continue;
			}

			// AI teammates reserve their committed destination; a human teammate
			// contributes only their observed position and current movement. This
			// informs bot decisions without ever assigning movement to the human.
			const FVector TeammateReferenceLocation =
				GetActiveRestartLiveThreatLocation(Teammate);

			const float TeammateDistance = FVector::Dist2D(
				CandidateLocation,
				TeammateReferenceLocation
			);
			if (TeammateDistance < SafeTeammateRadius)
			{
				Score -=
					(1.0f - TeammateDistance / SafeTeammateRadius) *
					SafeCrowdingPenalty;
			}

			if (
				Cast<ASoccerAICharacter>(Teammate) != nullptr &&
				TeammateDistance < SafeReservationRadius
			)
			{
				Score -=
					(1.0f - TeammateDistance / SafeReservationRadius) *
					SafeCrowdingPenalty * 1.35f;
			}
		}
	}

	FVector AttackDirection = GetFieldAttackDirectionForTeam(ActiveRestartTeam);
	AttackDirection.Z = 0.0f;
	AttackDirection = AttackDirection.GetSafeNormal();
	const float ForwardProgress = FVector::DotProduct(
		CandidateLocation - BaseLocation,
		AttackDirection
	);

	const ESoccerPlayerRole PlayerRole =
		SoccerAICharacter->GetPlayerRole();
	if (PlayerRole == ESoccerPlayerRole::Forward)
	{
		Score += FMath::Clamp(ForwardProgress, -300.0f, 360.0f) * 0.32f;
		if (bCornerKick)
		{
			Score += 90.0f;
		}
	}
	else if (PlayerRole == ESoccerPlayerRole::Midfielder)
	{
		Score += FMath::Clamp(ForwardProgress, -260.0f, 300.0f) * 0.16f;
		if (bCornerKick)
		{
			Score += 45.0f;
		}
	}
	else if (PlayerRole == ESoccerPlayerRole::Defender)
	{
		Score -= FMath::Max(0.0f, ForwardProgress - 120.0f) * 0.28f;
	}

	if (CandidateIndex >= 0)
	{
		FRandomStream DecisionRandom(BuildActiveRestartLivePositioningSeed(
			SoccerAICharacter,
			DecisionIndex,
			1000 + CandidateIndex
		));
		const float SafeScoreJitter = FMath::Max(
			0.0f,
			RestartLiveAttackDecisionScoreJitter
		);
		Score += DecisionRandom.FRandRange(-SafeScoreJitter, SafeScoreJitter);
	}

	return Score;
}

void ASoccerMatchManager::UpdateActiveRestartLiveAttackingPlans(
	float CurrentWorldTime,
	bool bForceDecision
)
{
	if (
		!IsActiveRestartLivePositioningActive() ||
		bActiveRestartLivePositioningLocked
	)
	{
		return;
	}

	TArray<const ASoccerAICharacter*> AttackingCharacters;
	ActiveRestartLiveAttackingPlans.GetKeys(AttackingCharacters);
	AttackingCharacters.RemoveAll(
		[](const ASoccerAICharacter* Character)
		{
			return !IsValid(Character);
		}
	);
	AttackingCharacters.Sort(
		[](const ASoccerAICharacter& A, const ASoccerAICharacter& B)
		{
			return A.GetUniqueID() < B.GetUniqueID();
		}
	);

	const bool bCornerKick =
		ActiveRestartType == ESoccerRestartType::CornerKick;
	FVector PrimarySearchDirection = bCornerKick
		? GetFieldAttackDirectionForTeam(ActiveRestartTeam)
		: ThrowInInwardDirection;
	PrimarySearchDirection.Z = 0.0f;
	PrimarySearchDirection = PrimarySearchDirection.GetSafeNormal();

	FVector SecondarySearchDirection;
	if (bCornerKick)
	{
		SecondarySearchDirection = FVector::CrossProduct(
			FVector::UpVector,
			PrimarySearchDirection
		);
	}
	else
	{
		SecondarySearchDirection = IsValid(SoccerField)
			? SoccerField->GetPitchLengthWorldDirection()
			: FVector::ForwardVector;
	}
	SecondarySearchDirection.Z = 0.0f;
	SecondarySearchDirection = SecondarySearchDirection.GetSafeNormal();

	const float SafeMinimumDecisionInterval = FMath::Max(
		0.10f,
		RestartLiveAttackDecisionMinInterval
	);
	const float SafeMaximumDecisionInterval = FMath::Max(
		SafeMinimumDecisionInterval,
		RestartLiveAttackDecisionMaxInterval
	);
	const int32 SafeMaximumRepositions = FMath::Max(
		0,
		RestartLiveMaximumAttackingRepositions
	);

	for (const ASoccerAICharacter* AttackingCharacter : AttackingCharacters)
	{
		if (!IsValid(AttackingCharacter))
		{
			ActiveRestartLiveAttackingPlans.Remove(AttackingCharacter);
			continue;
		}

		FRestartLivePositioningPlan* Plan =
			ActiveRestartLiveAttackingPlans.Find(AttackingCharacter);
		if (
			Plan == nullptr ||
			(!bForceDecision &&
			 CurrentWorldTime < Plan->NextDecisionWorldTime)
		)
		{
			continue;
		}

		if (Plan->CommittedTargetLocation.IsNearlyZero())
		{
			Plan->CommittedTargetLocation = Plan->BaseTargetLocation;
		}

		const int32 CurrentDecisionIndex = Plan->DecisionIndex;
		const float CurrentTargetScore =
			ScoreActiveRestartLiveAttackingCandidate(
				AttackingCharacter,
				Plan->CommittedTargetLocation,
				Plan->BaseTargetLocation,
				CurrentDecisionIndex,
				-1
			);

		FVector BestTarget = Plan->CommittedTargetLocation;
		float BestTargetScore = CurrentTargetScore;

		float PlayerSearchScale = 1.0f;
		const ESoccerPlayerRole PlayerRole =
			AttackingCharacter->GetPlayerRole();
		if (PlayerRole == ESoccerPlayerRole::Defender)
		{
			PlayerSearchScale = 0.68f;
		}
		else if (PlayerRole == ESoccerPlayerRole::Forward)
		{
			PlayerSearchScale = 1.12f;
		}

		const float PrimarySearchStep = bCornerKick
			? SoccerFieldDimensions::ScaleAuthoredLongitudinalDistance(
				FMath::Max(20.0f, RestartLiveCornerDepthSearchStep)
			) * PlayerSearchScale
			: SoccerFieldDimensions::ScaleAuthoredLateralDistance(
				FMath::Max(20.0f, RestartLiveThrowInInwardSearchStep)
			) * PlayerSearchScale;
		const float SecondarySearchStep = bCornerKick
			? SoccerFieldDimensions::ScaleAuthoredLateralDistance(
				FMath::Max(20.0f, RestartLiveCornerWidthSearchStep)
			) * PlayerSearchScale
			: SoccerFieldDimensions::ScaleAuthoredLongitudinalDistance(
				FMath::Max(20.0f, RestartLiveThrowInAlongLineSearchStep)
			) * PlayerSearchScale;

		const FVector2D CandidateOffsets[] =
		{
			FVector2D(0.0f, 0.0f),
			FVector2D(1.0f, 0.0f),
			FVector2D(-0.55f, 0.0f),
			FVector2D(0.0f, 1.0f),
			FVector2D(0.0f, -1.0f),
			FVector2D(0.80f, 1.0f),
			FVector2D(0.80f, -1.0f),
			FVector2D(-0.35f, 0.85f),
			FVector2D(-0.35f, -0.85f)
		};

		for (int32 CandidateIndex = 0;
			 CandidateIndex < UE_ARRAY_COUNT(CandidateOffsets);
			 ++CandidateIndex)
		{
			const FVector2D& CandidateOffset =
				CandidateOffsets[CandidateIndex];
			FVector CandidateLocation =
				Plan->BaseTargetLocation +
				PrimarySearchDirection *
					(CandidateOffset.X * PrimarySearchStep) +
				SecondarySearchDirection *
					(CandidateOffset.Y * SecondarySearchStep);
			CandidateLocation = SanitizeActiveRestartLivePositioningTarget(
				AttackingCharacter,
				CandidateLocation
			);

			float CandidateScore =
				ScoreActiveRestartLiveAttackingCandidate(
					AttackingCharacter,
					CandidateLocation,
					Plan->BaseTargetLocation,
					CurrentDecisionIndex,
					CandidateIndex
				);

			const bool bInitialSupportingOffer =
				bForceDecision &&
				CurrentDecisionIndex == 0 &&
				CandidateIndex > 0;
			if (bInitialSupportingOffer)
			{
				CandidateScore += FMath::Max(
					0.0f,
					RestartLiveAttackInitialOfferMovementBonus
				);
			}

			if (CandidateScore > BestTargetScore)
			{
				BestTargetScore = CandidateScore;
				BestTarget = CandidateLocation;
			}
		}

		const float RequiredImprovement = bForceDecision
			? 0.0f
			: FMath::Max(
				0.0f,
				RestartLiveAttackMinimumScoreImprovement
			);
		const bool bCanChangeTarget =
			Plan->RepositionCount < SafeMaximumRepositions;
		const bool bTargetIsMateriallyDifferent =
			FVector::Dist2D(
				BestTarget,
				Plan->CommittedTargetLocation
			) >= FMath::Max(
				20.0f,
				RestartLiveAttackMinimumRelocationDistance
			);

		if (
			bCanChangeTarget &&
			bTargetIsMateriallyDifferent &&
			BestTargetScore >= CurrentTargetScore + RequiredImprovement
		)
		{
			Plan->CommittedTargetLocation = BestTarget;
			Plan->LastEvaluatedScore = BestTargetScore;
			Plan->RepositionCount++;
		}
		else
		{
			Plan->LastEvaluatedScore = CurrentTargetScore;
		}

		Plan->DecisionIndex++;
		FRandomStream IntervalRandom(BuildActiveRestartLivePositioningSeed(
			AttackingCharacter,
			Plan->DecisionIndex,
			2001
		));
		Plan->NextDecisionWorldTime =
			CurrentWorldTime +
			IntervalRandom.FRandRange(
				SafeMinimumDecisionInterval,
				SafeMaximumDecisionInterval
			);
	}
}

FVector ASoccerMatchManager::GetActiveRestartLiveThreatLocation(
	const ASoccerCharacterBase* AttackingCharacter
) const
{
	if (!IsValid(AttackingCharacter))
	{
		return FVector::ZeroVector;
	}

	FVector ThreatLocation = AttackingCharacter->GetActorLocation();
	if (const ASoccerAICharacter* AttackingAI =
		Cast<ASoccerAICharacter>(AttackingCharacter))
	{
		if (const FRestartLivePositioningPlan* Plan =
			ActiveRestartLiveAttackingPlans.Find(AttackingAI))
		{
			ThreatLocation = Plan->CommittedTargetLocation;
		}
	}
	else
	{
		FVector HumanVelocity = AttackingCharacter->GetVelocity();
		HumanVelocity.Z = 0.0f;
		ThreatLocation += HumanVelocity * FMath::Max(
			0.0f,
			RestartLiveHumanMotionLookAheadTime
		);
	}

	const float SafeFieldInset = FMath::Max(0.0f, RestartLiveFieldInset);
	if (IsValid(SoccerField))
	{
		ThreatLocation = SoccerField->ClampWorldLocationInsidePitch(
			ThreatLocation,
			SafeFieldInset
		);
	}
	else
	{
		ThreatLocation = SoccerFieldDimensions::ClampLocationInsidePitch(
			ThreatLocation,
			SafeFieldInset
		);
	}
	ThreatLocation.Z = AttackingCharacter->GetActorLocation().Z;
	return ThreatLocation;
}

void ASoccerMatchManager::UpdateActiveRestartLiveDefensivePlans(
	float CurrentWorldTime,
	bool bForceDecision
)
{
	if (
		!IsActiveRestartLivePositioningActive() ||
		bActiveRestartLivePositioningLocked ||
		(!bForceDecision &&
		 CurrentWorldTime < ActiveRestartLiveNextDefensiveDecisionTime)
	)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	const bool bCornerKick =
		ActiveRestartType == ESoccerRestartType::CornerKick;
	const FVector RestartBallLocation = bCornerKick
		? GoalLineRestart.GetBallLocation()
		: ThrowInLocation;
	const FVector DefendingGoalLocation =
		GetOpponentGoalReferenceLocation(ActiveRestartTeam);

	struct FRestartLiveThreat
	{
		ASoccerCharacterBase* Character = nullptr;
		FVector TargetLocation = FVector::ZeroVector;
		float ThreatScore = -BIG_NUMBER;
		bool bCoveredByHuman = false;
	};

	const ESoccerTeam DefendingTeam = GetOppositeTeam(ActiveRestartTeam);
	AThirdPersonCppCharacter* HumanDefender =
		FindHumanCharacterForTeam(DefendingTeam);
	FVector HumanCoverageLocation = FVector::ZeroVector;
	if (IsValid(HumanDefender))
	{
		FVector HumanVelocity = HumanDefender->GetVelocity();
		HumanVelocity.Z = 0.0f;
		HumanCoverageLocation =
			HumanDefender->GetActorLocation() +
			HumanVelocity * FMath::Max(
				0.0f,
				RestartLiveHumanMotionLookAheadTime
			);
	}

	TArray<FRestartLiveThreat> Threats;
	for (TActorIterator<ASoccerCharacterBase> It(World); It; ++It)
	{
		ASoccerCharacterBase* CandidateAttacker = *It;
		if (
			!IsValid(CandidateAttacker) ||
			CandidateAttacker->IsHidden() ||
			CandidateAttacker->ActorHasTag(
				FName(TEXT("SubstitutionPresentation"))
			) ||
			CandidateAttacker->GetTeam() != ActiveRestartTeam ||
			CandidateAttacker->GetPlayerRole() ==
				ESoccerPlayerRole::Goalkeeper
		)
		{
			continue;
		}

		if (IsActiveRestartLiveTaker(CandidateAttacker))
		{
			continue;
		}

		FRestartLiveThreat Threat;
		Threat.Character = CandidateAttacker;
		Threat.TargetLocation =
			GetActiveRestartLiveThreatLocation(CandidateAttacker);

		if (bCornerKick)
		{
			const float IdealGoalDistance =
				SoccerFieldDimensions::ScaleAuthoredLongitudinalDistance(
					FMath::Max(100.0f, RestartLiveCornerIdealGoalDistance)
				);
			Threat.ThreatScore =
				1120.0f -
				FMath::Abs(
					FVector::Dist2D(
						Threat.TargetLocation,
						DefendingGoalLocation
					) - IdealGoalDistance
				) * FMath::Max(
					0.0f,
					RestartLiveCornerGoalDistancePenaltyWeight
				);
		}
		else
		{
			const float DistanceFromRestart = FVector::Dist2D(
				RestartBallLocation,
				Threat.TargetLocation
			);
			const float IdealPassDistance =
				SoccerFieldDimensions::ScaleAuthoredLongitudinalDistance(
					900.0f
				);
			Threat.ThreatScore =
				1000.0f -
				FMath::Abs(
					DistanceFromRestart - IdealPassDistance
				) * 0.16f;
		}

		if (!IsOpponentBlockingLaneBetweenLocations(
			ActiveRestartTeam,
			RestartBallLocation,
			Threat.TargetLocation,
			FMath::Max(0.0f, RestartReceiverAerialLaneHalfWidth)
		))
		{
			Threat.ThreatScore += 150.0f;
		}

		const ESoccerPlayerRole PlayerRole =
			CandidateAttacker->GetPlayerRole();
		if (PlayerRole == ESoccerPlayerRole::Forward)
		{
			Threat.ThreatScore += 100.0f;
		}
		else if (PlayerRole == ESoccerPlayerRole::Midfielder)
		{
			Threat.ThreatScore += 70.0f;
		}

		const float SafeHumanCoverageRadius = FMath::Max(
			20.0f,
			RestartLiveHumanCoverageRadius
		) * FMath::Clamp(
			RestartLiveHumanCoverageCredit,
			0.0f,
			1.0f
		);
		Threat.bCoveredByHuman =
			IsValid(HumanDefender) &&
			SafeHumanCoverageRadius > 0.0f &&
			FVector::Dist2D(
				HumanCoverageLocation,
				Threat.TargetLocation
			) <= SafeHumanCoverageRadius;

		Threats.Add(Threat);
	}

	Threats.Sort(
		[](const FRestartLiveThreat& A, const FRestartLiveThreat& B)
		{
			if (!FMath::IsNearlyEqual(A.ThreatScore, B.ThreatScore, 0.1f))
			{
				return A.ThreatScore > B.ThreatScore;
			}

			const uint32 AId = IsValid(A.Character)
				? A.Character->GetUniqueID()
				: MAX_uint32;
			const uint32 BId = IsValid(B.Character)
				? B.Character->GetUniqueID()
				: MAX_uint32;
			return AId < BId;
		}
	);

	TArray<const ASoccerAICharacter*> DefendingCharacters;
	ActiveRestartLiveDefensivePlans.GetKeys(DefendingCharacters);
	DefendingCharacters.RemoveAll(
		[](const ASoccerAICharacter* Character)
		{
			return !IsValid(Character);
		}
	);
	DefendingCharacters.Sort(
		[](const ASoccerAICharacter& A, const ASoccerAICharacter& B)
		{
			return A.GetUniqueID() < B.GetUniqueID();
		}
	);

	auto FindThreat = [&Threats](ASoccerCharacterBase* Character)
		-> const FRestartLiveThreat*
	{
		for (const FRestartLiveThreat& Threat : Threats)
		{
			if (Threat.Character == Character)
			{
				return &Threat;
			}
		}
		return nullptr;
	};

	auto BuildMarkTarget = [
		this,
		bCornerKick,
		RestartBallLocation,
		DefendingGoalLocation
	](
		const ASoccerAICharacter* DefendingCharacter,
		const FRestartLivePositioningPlan& Plan,
		const FRestartLiveThreat& Threat
	)
	{
		FVector MarkingReferenceDirection =
			(bCornerKick ? DefendingGoalLocation : RestartBallLocation) -
			Threat.TargetLocation;
		MarkingReferenceDirection.Z = 0.0f;
		MarkingReferenceDirection =
			MarkingReferenceDirection.GetSafeNormal();
		if (MarkingReferenceDirection.IsNearlyZero())
		{
			MarkingReferenceDirection =
				-GetFieldAttackDirectionForTeam(ActiveRestartTeam);
		}
		const float MarkingDistance = bCornerKick
			? RestartLiveCornerDefenderGoalSideDistance
			: RestartLiveDefenderMarkingDistance;

		FVector DesiredMarkTarget =
			Threat.TargetLocation +
			MarkingReferenceDirection * FMath::Max(
				20.0f,
				MarkingDistance
			);
		DesiredMarkTarget = FMath::Lerp(
			DesiredMarkTarget,
			Plan.BaseTargetLocation,
			FMath::Clamp(
				RestartLiveDefensiveShapeRetention,
				0.0f,
				1.0f
			)
		);
		DesiredMarkTarget = SanitizeActiveRestartLivePositioningTarget(
			DefendingCharacter,
			DesiredMarkTarget
		);
		DesiredMarkTarget = BuildCircularRestartOpponentMoveLocation(
			DefendingCharacter,
			DesiredMarkTarget,
			RestartBallLocation,
			bCornerKick
				? CornerKickOpponentRequiredDistance
				: ThrowInOpponentRequiredDistance,
			bCornerKick
				? CornerKickOpponentMoveExtraDistance
				: ThrowInOpponentMoveExtraDistance
		);
		return SanitizeActiveRestartLivePositioningTarget(
			DefendingCharacter,
			DesiredMarkTarget
		);
	};

	TSet<ASoccerCharacterBase*> AssignedThreats;
	TSet<const ASoccerAICharacter*> RetainedDefenders;

	for (const ASoccerAICharacter* DefendingCharacter : DefendingCharacters)
	{
		FRestartLivePositioningPlan* Plan =
			ActiveRestartLiveDefensivePlans.Find(DefendingCharacter);
		if (Plan == nullptr)
		{
			continue;
		}

		ASoccerCharacterBase* PreviousMarkedAttacker =
			Plan->MarkedAttacker.Get();
		const FRestartLiveThreat* PreviousThreat =
			FindThreat(PreviousMarkedAttacker);
		if (
			PreviousThreat != nullptr &&
			!PreviousThreat->bCoveredByHuman &&
			CurrentWorldTime < Plan->MarkCommitUntilWorldTime &&
			!AssignedThreats.Contains(PreviousMarkedAttacker)
		)
		{
			AssignedThreats.Add(PreviousMarkedAttacker);
			RetainedDefenders.Add(DefendingCharacter);
			Plan->CommittedTargetLocation = BuildMarkTarget(
				DefendingCharacter,
				*Plan,
				*PreviousThreat
			);
		}
	}

	const float SafeMinimumMarkHold = FMath::Max(
		0.10f,
		RestartLiveDefenderMarkMinHoldTime
	);
	const float SafeMaximumMarkHold = FMath::Max(
		SafeMinimumMarkHold,
		RestartLiveDefenderMarkMaxHoldTime
	);

	for (const ASoccerAICharacter* DefendingCharacter : DefendingCharacters)
	{
		if (!IsValid(DefendingCharacter))
		{
			ActiveRestartLiveDefensivePlans.Remove(DefendingCharacter);
			continue;
		}

		FRestartLivePositioningPlan* Plan =
			ActiveRestartLiveDefensivePlans.Find(DefendingCharacter);
		if (Plan == nullptr || RetainedDefenders.Contains(DefendingCharacter))
		{
			continue;
		}

		ASoccerCharacterBase* PreviousMarkedAttacker =
			Plan->MarkedAttacker.Get();
		const FRestartLiveThreat* BestThreat = nullptr;
		float BestAssignmentScore = -BIG_NUMBER;

		for (const FRestartLiveThreat& Threat : Threats)
		{
			if (
				!IsValid(Threat.Character) ||
				Threat.bCoveredByHuman ||
				AssignedThreats.Contains(Threat.Character)
			)
			{
				continue;
			}

			float AssignmentScore =
				Threat.ThreatScore -
				FVector::Dist2D(
					DefendingCharacter->GetActorLocation(),
					Threat.TargetLocation
				) * 0.18f;
			if (Threat.Character == PreviousMarkedAttacker)
			{
				AssignmentScore += FMath::Max(
					0.0f,
					RestartLiveDefenderMarkRetentionBonus
				);
			}

			if (AssignmentScore > BestAssignmentScore)
			{
				BestAssignmentScore = AssignmentScore;
				BestThreat = &Threat;
			}
		}

		if (BestThreat == nullptr)
		{
			Plan->MarkedAttacker.Reset();
			Plan->MarkCommitUntilWorldTime = -1000.0f;
			Plan->CommittedTargetLocation = Plan->BaseTargetLocation;
			continue;
		}

		Plan->MarkedAttacker = BestThreat->Character;
		AssignedThreats.Add(BestThreat->Character);
		FRandomStream HoldRandom(BuildActiveRestartLivePositioningSeed(
			DefendingCharacter,
			ActiveRestartLiveDefensiveDecisionIndex,
			3001
		));
		Plan->MarkCommitUntilWorldTime =
			CurrentWorldTime +
			HoldRandom.FRandRange(
				SafeMinimumMarkHold,
				SafeMaximumMarkHold
			);
		Plan->CommittedTargetLocation = BuildMarkTarget(
			DefendingCharacter,
			*Plan,
			*BestThreat
		);
	}

	ActiveRestartLiveDefensiveDecisionIndex++;
	const float SafeMinimumDecisionInterval = FMath::Max(
		0.05f,
		RestartLiveDefenseDecisionMinInterval
	);
	const float SafeMaximumDecisionInterval = FMath::Max(
		SafeMinimumDecisionInterval,
		RestartLiveDefenseDecisionMaxInterval
	);
	FRandomStream IntervalRandom(BuildActiveRestartLivePositioningSeed(
		nullptr,
		ActiveRestartLiveDefensiveDecisionIndex,
		4001
	));
	ActiveRestartLiveNextDefensiveDecisionTime =
		CurrentWorldTime +
		IntervalRandom.FRandRange(
			SafeMinimumDecisionInterval,
			SafeMaximumDecisionInterval
		);
}

void ASoccerMatchManager::UpdateActiveRestartLivePositioning(float DeltaTime)
{
	(void)DeltaTime;

	if (!bActiveRestartLivePositioning)
	{
		return;
	}

	const bool bSupportedRestart =
		(
			ActiveRestartType == ESoccerRestartType::ThrowIn &&
			bEnableThrowInLivePositioning
		) ||
		(
			ActiveRestartType == ESoccerRestartType::CornerKick &&
			bEnableCornerKickLivePositioning
		);
	if (
		!bEnableRestartLivePositioning ||
		!bSupportedRestart ||
		!IsRestartContextActive()
	)
	{
		ResetActiveRestartLivePositioning();
		return;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	if (!bActiveRestartLivePositioningLocked)
	{
		const float CurrentWorldTime = World->GetTimeSeconds();
		UpdateActiveRestartLiveAttackingPlans(CurrentWorldTime, false);
		UpdateActiveRestartLiveDefensivePlans(CurrentWorldTime, false);
	}

	DrawActiveRestartLivePositioningDebug();
}

void ASoccerMatchManager::CommitBestActiveRestartLiveReceiver()
{
	const bool bCornerKick =
		ActiveRestartType == ESoccerRestartType::CornerKick;
	ASoccerAICharacter* RestartTakerAI = bCornerKick
		? GoalLineRestart.GetTaker()
		: ThrowInTakerAI;
	if (
		!IsActiveRestartLivePositioningActive() ||
		!IsValid(RestartTakerAI)
	)
	{
		return;
	}

	const ASoccerAICharacter* BestReceiver = nullptr;
	float BestReceiverScore = -BIG_NUMBER;

	for (const TPair<const ASoccerAICharacter*, FRestartLivePositioningPlan>& Pair :
		ActiveRestartLiveAttackingPlans)
	{
		const ASoccerAICharacter* CandidateReceiver = Pair.Key;
		if (
			!IsValid(CandidateReceiver) ||
			CandidateReceiver == RestartTakerAI ||
			CandidateReceiver->GetTeam() != ActiveRestartTeam ||
			CandidateReceiver->GetPlayerRole() ==
				ESoccerPlayerRole::Goalkeeper
		)
		{
			continue;
		}

		const FVector CandidateTarget =
			Pair.Value.CommittedTargetLocation;
		float CandidateScore = ScoreRestartPassReceiverCandidate(
			ActiveRestartTeam,
			RestartTakerAI,
			CandidateReceiver,
			CandidateTarget,
			true
		);
		if (CandidateScore <= -BIG_NUMBER * 0.5f)
		{
			CandidateScore = Pair.Value.LastEvaluatedScore - 800.0f;
		}
		CandidateScore -= FVector::Dist2D(
			CandidateReceiver->GetActorLocation(),
			CandidateTarget
		) * 0.20f;

		if (
			CandidateScore > BestReceiverScore ||
			(
				FMath::IsNearlyEqual(
					CandidateScore,
					BestReceiverScore,
					0.1f
				) &&
				(
					BestReceiver == nullptr ||
					CandidateReceiver->GetUniqueID() <
						BestReceiver->GetUniqueID()
				)
			)
		)
		{
			BestReceiver = CandidateReceiver;
			BestReceiverScore = CandidateScore;
		}
	}

	if (!IsValid(BestReceiver))
	{
		return;
	}

	ASoccerAICharacter* PreviousReceiver = bCornerKick
		? GoalLineRestart.GetReceiver()
		: ThrowInReceiverAI;
	ASoccerAICharacter* CommittedReceiver =
		const_cast<ASoccerAICharacter*>(BestReceiver);
	if (bCornerKick)
	{
		GoalLineRestart.SetCornerParticipants(
			GoalLineRestart.GetTaker(),
			CommittedReceiver
		);
	}
	else
	{
		ThrowInReceiverAI = CommittedReceiver;
	}

	if (const FRestartLivePositioningPlan* ReceiverPlan =
		ActiveRestartLiveAttackingPlans.Find(BestReceiver))
	{
		if (bCornerKick)
		{
			GoalLineRestart.ReceiverMoveLocation =
				ReceiverPlan->CommittedTargetLocation;
		}
		else
		{
			ThrowInReceiverMoveLocation =
				ReceiverPlan->CommittedTargetLocation;
		}
	}

	if (PreviousReceiver != CommittedReceiver)
	{
		ASoccerDebugManager::Message(
			this,
			ESoccerDebugCategory::Restarts,
			FString::Printf(
				TEXT("%s: receptor dinamico elegido %s"),
				bCornerKick
					? TEXT("CORNER")
					: TEXT("LATERAL"),
				*CommittedReceiver->GetName()
			),
			FColor::Green
		);
	}
}

void ASoccerMatchManager::DrawActiveRestartLivePositioningDebug() const
{
	if (
		!bDrawRestartLivePositioningDebug ||
		!IsActiveRestartLivePositioningActive()
	)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	for (const TPair<const ASoccerAICharacter*, FRestartLivePositioningPlan>& Pair :
		ActiveRestartLiveAttackingPlans)
	{
		if (!IsValid(Pair.Key))
		{
			continue;
		}

		DrawDebugLine(
			World,
			Pair.Key->GetActorLocation(),
			Pair.Value.CommittedTargetLocation,
			FColor::Green,
			false,
			0.0f,
			0,
			2.0f
		);
		DrawDebugSphere(
			World,
			Pair.Value.CommittedTargetLocation,
			28.0f,
			8,
			FColor::Green,
			false,
			0.0f,
			0,
			2.0f
		);
	}

	for (const TPair<const ASoccerAICharacter*, FRestartLivePositioningPlan>& Pair :
		ActiveRestartLiveDefensivePlans)
	{
		if (!IsValid(Pair.Key))
		{
			continue;
		}

		DrawDebugLine(
			World,
			Pair.Key->GetActorLocation(),
			Pair.Value.CommittedTargetLocation,
			FColor::Orange,
			false,
			0.0f,
			0,
			2.0f
		);
		DrawDebugSphere(
			World,
			Pair.Value.CommittedTargetLocation,
			28.0f,
			8,
			FColor::Orange,
			false,
			0.0f,
			0,
			2.0f
		);

		ASoccerCharacterBase* MarkedAttacker =
			Pair.Value.MarkedAttacker.Get();
		if (IsValid(MarkedAttacker))
		{
			DrawDebugLine(
				World,
				Pair.Value.CommittedTargetLocation,
				GetActiveRestartLiveThreatLocation(MarkedAttacker),
				FColor::Red,
				false,
				0.0f,
				0,
				1.5f
			);
		}
	}
}

FVector ASoccerMatchManager::GetActiveRestartMoveLocation(
	const ASoccerAICharacter* SoccerAICharacter
) const
{
	if (!IsRestartContextActive() || !IsValid(SoccerAICharacter))
	{
		return FVector::ZeroVector;
	}

	// Final movement phases replace the previously captured tactical target.
	if (IsKickoffFinalRunActiveForCharacter(SoccerAICharacter))
	{
		return GetKickoffMoveLocation(SoccerAICharacter);
	}

	if (IsOffsideRestartFinalRunActiveForCharacter(SoccerAICharacter))
	{
		return GetOffsideRestartMoveLocation(SoccerAICharacter);
	}

	if (IsGoalKickTakerFinalApproach(SoccerAICharacter))
	{
		return GetGoalLineRestartMoveLocation(SoccerAICharacter);
	}

	if (IsActiveRestartLivePositioningActive())
	{
		if (const FRestartLivePositioningPlan* AttackingPlan =
			ActiveRestartLiveAttackingPlans.Find(SoccerAICharacter))
		{
			if (!AttackingPlan->CommittedTargetLocation.IsNearlyZero())
			{
				return AttackingPlan->CommittedTargetLocation;
			}
		}

		if (const FRestartLivePositioningPlan* DefensivePlan =
			ActiveRestartLiveDefensivePlans.Find(SoccerAICharacter))
		{
			if (!DefensivePlan->CommittedTargetLocation.IsNearlyZero())
			{
				return DefensivePlan->CommittedTargetLocation;
			}
		}
	}

	if (!bCapturingActiveRestartAITargetLocations)
	{
		if (const FVector* AssignedLocation =
			ActiveRestartAITargetLocations.Find(SoccerAICharacter))
		{
			return *AssignedLocation;
		}
	}

	return BuildActiveRestartMoveLocation(SoccerAICharacter);
}

FVector ASoccerMatchManager::BuildActiveRestartMoveLocation(
	const ASoccerAICharacter* SoccerAICharacter
) const
{
	if (!IsRestartContextActive() || !IsValid(SoccerAICharacter))
	{
		return FVector::ZeroVector;
	}

	switch (ActiveRestartType)
	{
	case ESoccerRestartType::Kickoff:
		return GetKickoffMoveLocation(SoccerAICharacter);

	case ESoccerRestartType::OffsideFreeKick:
	case ESoccerRestartType::DirectFreeKick:
		return GetOffsideRestartMoveLocation(SoccerAICharacter);

	case ESoccerRestartType::PenaltyKick:
		return BuildPenaltyKickMoveLocation(SoccerAICharacter);

	case ESoccerRestartType::ThrowIn:
		return GetThrowInMoveLocation(SoccerAICharacter);

	case ESoccerRestartType::CornerKick:
	case ESoccerRestartType::GoalKick:
		return GetGoalLineRestartMoveLocation(SoccerAICharacter);

	case ESoccerRestartType::None:
	default:
		return FVector::ZeroVector;
	}
}

void ASoccerMatchManager::CaptureActiveRestartAITargetLocations(
	bool bLegalPhase
)
{
	if (!IsRestartContextActive())
	{
		return;
	}

	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return;
	}

	if (
		(ActiveRestartType == ESoccerRestartType::OffsideFreeKick ||
		 ActiveRestartType == ESoccerRestartType::DirectFreeKick) &&
		bLegalPhase &&
		!FreeKickRestart.IsFinalRunActive()
		)
	{
		FreeKickRestart.RecalculateRunUpGeometry(*this);
	}

	if (
		ActiveRestartType == ESoccerRestartType::GoalKick &&
		!GoalLineRestart.bGoalKickFinalRunActive
		)
	{
		RecalculateGoalLineRestartGeometry();
	}

	bCapturingActiveRestartAITargetLocations = true;
	ActiveRestartAITargetLocations.Empty();

	for (TActorIterator<ASoccerAICharacter> It(World); It; ++It)
	{
		const ASoccerAICharacter* Candidate = *It;

		if (!IsValid(Candidate))
		{
			continue;
		}

		const FVector TargetLocation =
			BuildActiveRestartMoveLocation(Candidate);

		if (!TargetLocation.IsNearlyZero())
		{
			ActiveRestartAITargetLocations.Add(
				Candidate,
				TargetLocation
			);
		}
	}

	bCapturingActiveRestartAITargetLocations = false;
	bActiveRestartPreviousLegalConditionsSatisfied = bLegalPhase;
	ActiveRestartLastPositioningRecoveryTime = World->GetTimeSeconds();
	ResetActiveRestartReadyHold();
}

float ASoccerMatchManager::GetActiveRestartAcceptanceRadius(
	const ASoccerAICharacter* SoccerAICharacter
) const
{
	if (!IsValid(SoccerAICharacter))
	{
		return RestartDefaultBotAcceptanceRadius;
	}

	switch (ActiveRestartType)
	{
	case ESoccerRestartType::Kickoff:
		return
			MatchPlayState == ESoccerMatchPlayState::KickoffTaking &&
			SoccerAICharacter == KickoffTakerAI
			? KickoffRunUpAcceptanceRadius
			: KickoffHomeAcceptanceRadius;

	case ESoccerRestartType::OffsideFreeKick:
	case ESoccerRestartType::DirectFreeKick:
		if (FreeKickRestart.IsDefensiveWallMember(SoccerAICharacter))
		{
			return FMath::Clamp(
				FreeKickWallReadyAcceptanceRadius,
				1.0f,
				80.0f
			);
		}
		if (SoccerAICharacter == FreeKickRestart.GetTaker())
		{
			return FreeKickRestart.IsHumanTakerClaimed()
				? RestartDefaultBotAcceptanceRadius
				: OffsideRestartRunUpAcceptanceRadius;
		}
		if (SoccerAICharacter == FreeKickRestart.GetReceiver())
		{
			return OffsideRestartReceiverReadyDistance;
		}
		break;

	case ESoccerRestartType::ThrowIn:
		if (SoccerAICharacter == ThrowInTakerAI)
		{
			return
				(bThrowInHumanTakerClaimed || bThrowInHumanTakerCommitted)
				? RestartDefaultBotAcceptanceRadius
				: ThrowInPickupReadyDistance;
		}
		if (SoccerAICharacter == ThrowInReceiverAI)
		{
			return ThrowInReceiverReadyDistance;
		}
		break;

	case ESoccerRestartType::CornerKick:
		if (SoccerAICharacter == GoalLineRestart.TakerAI)
		{
			return IsNonFreeKickHumanTakerClaimedFor(ESoccerRestartType::CornerKick)
				? RestartDefaultBotAcceptanceRadius
				: GoalLineRestartTakerReadyDistance;
		}
		if (SoccerAICharacter == GoalLineRestart.ReceiverAI)
		{
			return GoalLineRestartReceiverReadyDistance;
		}
		break;

	case ESoccerRestartType::GoalKick:
		if (SoccerAICharacter == GoalLineRestart.TakerAI)
		{
			return IsNonFreeKickHumanTakerClaimedFor(ESoccerRestartType::GoalKick)
				? RestartDefaultBotAcceptanceRadius
				: GoalKickRunUpAcceptanceRadius;
		}
		if (SoccerAICharacter == GoalLineRestart.ReceiverAI)
		{
			return GoalLineRestartReceiverReadyDistance;
		}
		break;

	case ESoccerRestartType::PenaltyKick:
		if (PenaltyKickRestart.IsDefendingGoalkeeper(SoccerAICharacter))
		{
			return GetPenaltyKickGoalkeeperMoveAcceptanceRadius();
		}
		break;

	case ESoccerRestartType::None:
	default:
		break;
	}

	return RestartDefaultBotAcceptanceRadius;
}

bool ASoccerMatchManager::AreAllActiveRestartBotsSettled() const
{
	if (!IsRestartContextActive())
	{
		return false;
	}

	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return false;
	}

	for (TActorIterator<ASoccerAICharacter> It(World); It; ++It)
	{
		const ASoccerAICharacter* Candidate = *It;

		if (!IsValid(Candidate))
		{
			continue;
		}

		const FVector TargetLocation =
			GetActiveRestartMoveLocation(Candidate);

		if (TargetLocation.IsNearlyZero())
		{
			continue;
		}

		const float AcceptanceRadius =
			GetActiveRestartAcceptanceRadius(Candidate);

		if (FVector::Dist2D(
			Candidate->GetActorLocation(),
			TargetLocation
		) > AcceptanceRadius)
		{
			return false;
		}

		const UCharacterMovementComponent* Movement =
			Candidate->GetCharacterMovement();

		if (
			Movement != nullptr &&
			Movement->Velocity.Size2D() > RestartReadyMaximumBotSpeed
			)
		{
			return false;
		}
	}

	return true;
}

bool ASoccerMatchManager::AreActiveRestartLegalConditionsSatisfied() const
{
	if (!IsRestartContextActive())
	{
		return false;
	}

	switch (ActiveRestartType)
	{
	case ESoccerRestartType::Kickoff:
		return AreKickoffPlayersInLegalPositions();

	case ESoccerRestartType::OffsideFreeKick:
	case ESoccerRestartType::DirectFreeKick:
		// Free kicks use a bot-only clearance buffer to avoid repeatedly
		// toggling legal/illegal as an opponent settles on the circle edge.
		return FreeKickRestart.AreOpponentsClear(*this);

	case ESoccerRestartType::ThrowIn:
	case ESoccerRestartType::CornerKick:
	case ESoccerRestartType::GoalKick:
		return AreActiveRestartOpponentsLegal();

	case ESoccerRestartType::PenaltyKick:
		return true;

	case ESoccerRestartType::None:
	default:
		return false;
	}
}

bool ASoccerMatchManager::UpdateActiveRestartReadiness(
	float SetupStartTime,
	float MinimumSetupTime
)
{
	UWorld* World = GetWorld();

	if (!IsRestartContextActive() || World == nullptr)
	{
		ResetActiveRestartReadyHold();
		return false;
	}

	const float CurrentTime = World->GetTimeSeconds();

	if (CurrentTime - SetupStartTime < MinimumSetupTime)
	{
		ResetActiveRestartReadyHold();
		return false;
	}

	// Free-kick defenders first finish one committed escape from the protected
	// circle. After reaching that point they may make at most one tactical
	// reposition, and only if its complete NavMesh path remains legal.
	if (
		(ActiveRestartType == ESoccerRestartType::OffsideFreeKick ||
		 ActiveRestartType == ESoccerRestartType::DirectFreeKick) &&
		FreeKickRestart.UpdateOpponentPositioningAfterEscape(*this)
		)
	{
		const bool bCurrentLegalConditionsSatisfied =
			AreActiveRestartLegalConditionsSatisfied();

		CaptureActiveRestartAITargetLocations(
			bCurrentLegalConditionsSatisfied
		);
		return false;
	}

	const bool bLegalConditionsSatisfied =
		AreActiveRestartLegalConditionsSatisfied();

	if (
		bLegalConditionsSatisfied !=
		bActiveRestartPreviousLegalConditionsSatisfied
		)
	{
		// Al pasar de la fase de despeje a la fase legal se toman destinos
		// nuevos y fijos. Asi el ejecutor avanza a la pelota y los demas
		// dejan de perseguir objetivos que se recalculan mientras caminan.
		CaptureActiveRestartAITargetLocations(
			bLegalConditionsSatisfied
		);
		return false;
	}

	if (!bLegalConditionsSatisfied || !AreAllActiveRestartBotsSettled())
	{
		ResetActiveRestartReadyHold();

		if (
			CurrentTime - ActiveRestartLastPositioningRecoveryTime >=
			RestartPositioningRecoveryInterval
			)
		{
			// Recuperacion sin atajos: vuelve a calcular posiciones y rutas,
			// pero el saque sigue bloqueado hasta que todos realmente lleguen.
			CaptureActiveRestartAITargetLocations(
				bLegalConditionsSatisfied
			);
			ActiveRestartLastPositioningRecoveryTime = CurrentTime;
		}

		return false;
	}

	if (ActiveRestartAllBotsReadySince < 0.0f)
	{
		ActiveRestartAllBotsReadySince = CurrentTime;

		ASoccerDebugManager::Message(
			this, ESoccerDebugCategory::Restarts,
			TEXT("REANUDACION: todos los bots acomodados"),
			FColor(180, 180, 180));
	}

	return CurrentTime - ActiveRestartAllBotsReadySince >=
		RestartReadyHoldTime;
}

bool ASoccerMatchManager::IsImmediateDefensiveDangerForTeam(
	ESoccerTeam Team
) const
{
	if (
		MatchPlayState != ESoccerMatchPlayState::Playing ||
		IsRestartContextActive() ||
		!IsValid(SoccerBall)
		)
	{
		return false;
	}

	if (
		PossessionTeam != ESoccerPossessionTeam::None &&
		DoesTeamHavePossession(Team)
		)
	{
		return false;
	}

	const float BallDepthAlpha = GetAttackDepthAlphaForLocation(
		SoccerBall->GetActorLocation(),
		Team
	);

	return BallDepthAlpha <= FMath::Clamp(
		DefensiveEmergencyReactionDepthAlpha,
		0.0f,
		1.0f
	);
}

bool ASoccerMatchManager::IsDangerousLooseBallEmergencyForTeam(
	ESoccerTeam Team
) const
{
	return
		PossessionTeam == ESoccerPossessionTeam::None &&
		!IsValid(PossessingCharacter) &&
		IsImmediateDefensiveDangerForTeam(Team);
}

bool ASoccerMatchManager::TryGetDangerousLooseBallEmergencyDefendingTeam(
	ESoccerTeam& OutDefendingTeam
) const
{
	const bool bPlayerTeamEmergency =
		IsDangerousLooseBallEmergencyForTeam(ESoccerTeam::PlayerTeam);
	const bool bOpponentTeamEmergency =
		IsDangerousLooseBallEmergencyForTeam(ESoccerTeam::OpponentTeam);

	if (!bPlayerTeamEmergency && !bOpponentTeamEmergency)
	{
		return false;
	}

	if (bPlayerTeamEmergency && !bOpponentTeamEmergency)
	{
		OutDefendingTeam = ESoccerTeam::PlayerTeam;
		return true;
	}

	if (bOpponentTeamEmergency && !bPlayerTeamEmergency)
	{
		OutDefendingTeam = ESoccerTeam::OpponentTeam;
		return true;
	}

	const float PlayerDepth = GetAttackDepthAlphaForLocation(
		SoccerBall->GetActorLocation(),
		ESoccerTeam::PlayerTeam
	);
	const float OpponentDepth = GetAttackDepthAlphaForLocation(
		SoccerBall->GetActorLocation(),
		ESoccerTeam::OpponentTeam
	);

	OutDefendingTeam =
		PlayerDepth <= OpponentDepth
		? ESoccerTeam::PlayerTeam
		: ESoccerTeam::OpponentTeam;

	return true;
}

bool ASoccerMatchManager::IsTeamCurrentlyAttacking(ESoccerTeam Team) const
{
	ESoccerTeam EmergencyDefendingTeam = ESoccerTeam::PlayerTeam;
	if (
		TryGetDangerousLooseBallEmergencyDefendingTeam(
			EmergencyDefendingTeam
		)
		)
	{
		return Team != EmergencyDefendingTeam;
	}

	return HasActiveAttack() && GetCurrentAttackingTeam() == Team;
}

bool ASoccerMatchManager::IsTeamCurrentlyDefending(ESoccerTeam Team) const
{
	ESoccerTeam EmergencyDefendingTeam = ESoccerTeam::PlayerTeam;
	if (
		TryGetDangerousLooseBallEmergencyDefendingTeam(
			EmergencyDefendingTeam
		)
		)
	{
		return Team == EmergencyDefendingTeam;
	}

	return HasActiveAttack() && GetCurrentAttackingTeam() != Team;
}

FVector ASoccerMatchManager::GetGoalkeeperMoveLocation(
	const ASoccerAICharacter* SoccerAICharacter
) const
{
	if (SoccerAICharacter == nullptr)
	{
		return FVector::ZeroVector;
	}

	AActor* HomePositionActor = SoccerAICharacter->GetHomePositionActor();

	if (HomePositionActor == nullptr)
	{
		return SoccerAICharacter->GetActorLocation();
	}

	const ESoccerTeam Team = SoccerAICharacter->GetTeam();
	const FVector OwnGoalCenter = GetOwnGoalReferenceLocation(Team);
	FVector InwardDirection = GetFieldAttackDirectionForTeam(Team);
	FVector LateralDirection = GetFieldRightDirectionForTeam(Team);

	if (InwardDirection.IsNearlyZero())
	{
		InwardDirection = FVector::ForwardVector;
	}
	if (LateralDirection.IsNearlyZero())
	{
		LateralDirection = FVector::RightVector;
	}

	const FVector HomeLocation = GetTeamRebasedFieldReferenceLocation(
		Team,
		HomePositionActor->GetActorLocation()
	);
	const float CharacterZ = SoccerAICharacter->GetActorLocation().Z;
	const float HomeDepthFromGoalLine = FMath::Max(
		0.0f,
		FVector::DotProduct(HomeLocation - OwnGoalCenter, InwardDirection)
	);

	// HomePosition sigue definiendo la profundidad minima/de reposo, pero la
	// referencia lateral y la normal del arco proceden de ASoccerField.
	FVector GoalkeeperMoveLocation =
		OwnGoalCenter + InwardDirection * HomeDepthFromGoalLine;
	GoalkeeperMoveLocation.Z = CharacterZ;

	if (!IsValid(SoccerBall))
	{
		return GoalkeeperMoveLocation;
	}

	const FVector BallLocation = SoccerBall->GetActorLocation();
	const FVector FromGoalToBall = BallLocation - OwnGoalCenter;
	const float BallDepthFromGoalLine = FVector::DotProduct(
		FromGoalToBall,
		InwardDirection
	);

	// Si la pelota esta sobre/detras de la linea de gol, los sistemas de
	// atajada/reanudacion tienen prioridad. Como posicion base segura vuelve
	// al centro y no intenta perseguir una referencia exterior.
	if (BallDepthFromGoalLine <= 1.0f)
	{
		return GoalkeeperMoveLocation;
	}

	// Profundidad dinamica: conserva la logica anterior, ahora expresada en el
	// marco geometrico del campo en lugar de usar X mundial.
	if (
		bUseGoalkeeperDynamicDepth &&
		MatchPlayState == ESoccerMatchPlayState::Playing
		)
	{
		const float MaximumDepthFromGoalLine = FMath::Max(
			HomeDepthFromGoalLine,
			GoalkeeperDynamicMaximumDepthFromGoal
		);
		const float FullBallDistance = FMath::Max(
			1.0f,
			SoccerFieldDimensions::ScaleAuthoredLongitudinalDistance(
				GoalkeeperDynamicDepthFullBallDistance
			)
		);
		const float DistanceFactor = FMath::Clamp(
			BallDepthFromGoalLine / FullBallDistance,
			0.0f,
			1.0f
		);

		FVector ToBallDirection = FromGoalToBall;
		ToBallDirection.Z = 0.0f;
		ToBallDirection = ToBallDirection.GetSafeNormal();

		const float AngleFactor = !ToBallDirection.IsNearlyZero()
			? FMath::Clamp(
				FVector::DotProduct(ToBallDirection, InwardDirection),
				0.0f,
				1.0f
			)
			: 0.0f;
		const float DynamicDepthAlpha = DistanceFactor * AngleFactor;
		const float DynamicDepthFromGoalLine = FMath::Lerp(
			HomeDepthFromGoalLine,
			MaximumDepthFromGoalLine,
			DynamicDepthAlpha
		);

		GoalkeeperMoveLocation =
			OwnGoalCenter + InwardDirection * DynamicDepthFromGoalLine;
		GoalkeeperMoveLocation.Z = CharacterZ;
	}

	const float PostSafetyMargin = FMath::Clamp(
		GoalkeeperPostSafetyMargin,
		0.0f,
		SoccerFieldDimensions::GoalHalfWidthCm - 5.0f
	);
	const float GeometricMaximumLateralOffset =
		SoccerFieldDimensions::GoalHalfWidthCm - PostSafetyMargin;
	const float ConfiguredMaximumLateralOffset =
		FMath::Max(0.0f, GoalkeeperLateralMoveRange);
	const float MaximumLateralOffset = FMath::Min(
		ConfiguredMaximumLateralOffset,
		GeometricMaximumLateralOffset
	);

	if (MaximumLateralOffset <= KINDA_SMALL_NUMBER)
	{
		return GoalkeeperMoveLocation;
	}

	const float BallLateralFromGoalCenter = FVector::DotProduct(
		FromGoalToBall,
		LateralDirection
	);
	const float AbsoluteBallLateral = FMath::Abs(BallLateralFromGoalCenter);

	if (AbsoluteBallLateral <= KINDA_SMALL_NUMBER)
	{
		return GoalkeeperMoveLocation;
	}

	const float BallAngleFromGoalNormalDegrees = FMath::RadiansToDegrees(
		FMath::Atan2(
			AbsoluteBallLateral,
			FMath::Max(1.0f, BallDepthFromGoalLine)
		)
	);
	const float FullCoverageAngleDegrees = FMath::Max(
		1.0f,
		GoalkeeperFullLateralCoverageAngleDegrees
	);
	const float NormalizedCoverageAngle = FMath::Clamp(
		BallAngleFromGoalNormalDegrees / FullCoverageAngleDegrees,
		0.0f,
		1.0f
	);
	const float ResponseExponent = FMath::Max(
		0.05f,
		GoalkeeperLateralResponseExponent
	);
	const float CoverageResponse = FMath::Pow(
		NormalizedCoverageAngle,
		ResponseExponent
	);
	const float BaseFollowAlpha = FMath::Clamp(
		GoalkeeperBallFollowAlpha,
		0.0f,
		1.0f
	);
	const float NearDepth = FMath::Max(
		0.0f,
		GoalkeeperNearGoalFullFollowDepth
	);
	const float FarDepth = FMath::Max(
		NearDepth + 1.0f,
		GoalkeeperFarGoalBaseFollowDepth
	);
	const float DistanceBlend = FMath::Clamp(
		(BallDepthFromGoalLine - NearDepth) /
		(FarDepth - NearDepth),
		0.0f,
		1.0f
	);
	const float EffectiveFollowAlpha = FMath::Lerp(
		1.0f,
		BaseFollowAlpha,
		DistanceBlend
	);
	const float LateralSign = BallLateralFromGoalCenter >= 0.0f
		? 1.0f
		: -1.0f;
	const float AngleBasedLateralOffset =
		LateralSign *
		MaximumLateralOffset *
		CoverageResponse *
		EffectiveFollowAlpha;

	float FinalLateralOffset = FMath::Clamp(
		AngleBasedLateralOffset,
		-MaximumLateralOffset,
		MaximumLateralOffset
	);

	// Cuando el arquero se adelanta, mezclamos la respuesta angular con la
	// bisectriz exacta pelota-postes intersectada contra SU plano de profundidad.
	if (
		bUseGoalkeeperDynamicDepth &&
		MatchPlayState == ESoccerMatchPlayState::Playing
		)
	{
		const float CurrentDepthFromGoalLine = FMath::Max(
			0.0f,
			FVector::DotProduct(
				GoalkeeperMoveLocation - OwnGoalCenter,
				InwardDirection
			)
		);
		const float MaximumDepthFromGoalLine = FMath::Max(
			HomeDepthFromGoalLine,
			GoalkeeperDynamicMaximumDepthFromGoal
		);
		const float DepthAdvanceRange =
			MaximumDepthFromGoalLine - HomeDepthFromGoalLine;
		const float DepthAdvanceAlpha = DepthAdvanceRange > KINDA_SMALL_NUMBER
			? FMath::Clamp(
				(CurrentDepthFromGoalLine - HomeDepthFromGoalLine) /
				DepthAdvanceRange,
				0.0f,
				1.0f
			)
			: 0.0f;

		if (
			DepthAdvanceAlpha > KINDA_SMALL_NUMBER &&
			BallDepthFromGoalLine > CurrentDepthFromGoalLine + 1.0f
			)
		{
			FVector LowerPostLocation =
				OwnGoalCenter -
				LateralDirection * SoccerFieldDimensions::GoalHalfWidthCm;
			FVector UpperPostLocation =
				OwnGoalCenter +
				LateralDirection * SoccerFieldDimensions::GoalHalfWidthCm;
			LowerPostLocation.Z = BallLocation.Z;
			UpperPostLocation.Z = BallLocation.Z;

			FVector DirectionToLowerPost = LowerPostLocation - BallLocation;
			FVector DirectionToUpperPost = UpperPostLocation - BallLocation;
			DirectionToLowerPost.Z = 0.0f;
			DirectionToUpperPost.Z = 0.0f;
			DirectionToLowerPost = DirectionToLowerPost.GetSafeNormal();
			DirectionToUpperPost = DirectionToUpperPost.GetSafeNormal();

			FVector GoalAngleBisector =
				DirectionToLowerPost + DirectionToUpperPost;
			GoalAngleBisector.Z = 0.0f;
			GoalAngleBisector = GoalAngleBisector.GetSafeNormal();

			const float BisectorDepthRate = FVector::DotProduct(
				GoalAngleBisector,
				InwardDirection
			);

			if (
				!GoalAngleBisector.IsNearlyZero() &&
				FMath::Abs(BisectorDepthRate) > KINDA_SMALL_NUMBER
				)
			{
				const float BallDepth = FVector::DotProduct(
					BallLocation - OwnGoalCenter,
					InwardDirection
				);
				const float IntersectionTime =
					(CurrentDepthFromGoalLine - BallDepth) /
					BisectorDepthRate;

				if (IntersectionTime >= 0.0f)
				{
					const FVector BisectorIntersection =
						BallLocation + GoalAngleBisector * IntersectionTime;
					const float BisectorLateralOffset = FVector::DotProduct(
						BisectorIntersection - OwnGoalCenter,
						LateralDirection
					);
					const float AdvancedMaximumLateralOffset = FMath::Lerp(
						MaximumLateralOffset,
						SoccerFieldDimensions::PenaltyAreaHalfWidthCm,
						DepthAdvanceAlpha
					);
					const float SafeBisectorLateralOffset = FMath::Clamp(
						BisectorLateralOffset,
						-AdvancedMaximumLateralOffset,
						AdvancedMaximumLateralOffset
					);

					FinalLateralOffset = FMath::Lerp(
						FinalLateralOffset,
						SafeBisectorLateralOffset,
						DepthAdvanceAlpha
					);
					FinalLateralOffset = FMath::Clamp(
						FinalLateralOffset,
						-AdvancedMaximumLateralOffset,
						AdvancedMaximumLateralOffset
					);
				}
			}
		}
	}

	const float FinalDepthFromGoalLine = FMath::Max(
		0.0f,
		FVector::DotProduct(
			GoalkeeperMoveLocation - OwnGoalCenter,
			InwardDirection
		)
	);
	GoalkeeperMoveLocation =
		OwnGoalCenter +
		InwardDirection * FinalDepthFromGoalLine +
		LateralDirection * FinalLateralOffset;
	GoalkeeperMoveLocation.Z = CharacterZ;

	return GoalkeeperMoveLocation;
}

FVector ASoccerMatchManager::GetShotTargetLocation(
	const ASoccerAICharacter* SoccerAICharacter
) const
{
	if (SoccerAICharacter == nullptr)
	{
		return FVector::ZeroVector;
	}

	if (IsValid(SoccerField))
	{
		return SoccerField->GetGoalCenterWorldLocation(
			GetOpponentGoalLineSign(SoccerAICharacter->GetTeam())
		);
	}

	// Compatibility fallback for a malformed level with no ASoccerField.
	// It intentionally does not restore the removed Target Point path.
	FVector FallbackLocation =
		SoccerAICharacter->GetActorLocation() +
		SoccerAICharacter->GetActorForwardVector() *
		AttackFallbackForwardDistance;

	return FallbackLocation;
}

FVector ASoccerMatchManager::GetOpenPlayCarryIntentTargetLocation(
	const ASoccerAICharacter* SoccerAICharacter
) const
{
	if (!IsValid(SoccerAICharacter))
	{
		return FVector::ZeroVector;
	}

	const FVector DefaultTarget =
		GetShotTargetLocation(SoccerAICharacter);

	const ESoccerTeam Team = SoccerAICharacter->GetTeam();

	if (
		!ShouldApplyCollectiveTacticsToOpenPlay(Team) ||
		!IsTeamCurrentlyAttacking(Team)
		)
	{
		return DefaultTarget;
	}

	const FSoccerTeamTacticalPlan& TacticalPlan =
		GetTacticalPlanForTeamInternal(Team);

	if (TacticalPlan.AttackChannel == ESoccerAttackChannel::Balanced)
	{
		return DefaultTarget;
	}

	const FVector OwnGoalLocation = GetOwnGoalReferenceLocation(Team);
	const FVector OpponentGoalLocation = GetOpponentGoalReferenceLocation(Team);

	FVector AttackDirection = OpponentGoalLocation - OwnGoalLocation;
	AttackDirection.Z = 0.0f;

	const float FieldLength = AttackDirection.Size2D();

	if (FieldLength <= KINDA_SMALL_NUMBER)
	{
		return DefaultTarget;
	}

	AttackDirection /= FieldLength;

	const FVector RightDirection = GetFieldRightDirectionForTeam(Team);

	if (RightDirection.IsNearlyZero())
	{
		return DefaultTarget;
	}

	const float CurrentDepth = FVector::DotProduct(
		SoccerAICharacter->GetActorLocation() - OwnGoalLocation,
		AttackDirection
	);

	// Once the carrier is already in the finishing zone, the actual goal target
	// must win over a preferred build-up channel so the grid never steers him
	// backward just to return to the selected lane.
	if (CurrentDepth >= FieldLength - 650.0f)
	{
		return DefaultTarget;
	}

	const float DesiredDepth = FMath::Clamp(
		CurrentDepth +
			SoccerFieldDimensions::ScaleAuthoredLongitudinalDistance(
				CollectiveAttackChannelCarryLookAheadDistance
			),
		120.0f,
		FieldLength - 320.0f
	);

	float DesiredLateral = 0.0f;
	const float WidthScale = GetCollectiveAttackWidthScale(Team);

	const float ScaledCarryLateralOffset =
		SoccerFieldDimensions::ScaleAuthoredLateralDistance(
			CollectiveAttackChannelCarryLateralOffset
		);

	if (TacticalPlan.AttackChannel == ESoccerAttackChannel::Left)
	{
		DesiredLateral = -ScaledCarryLateralOffset * WidthScale;
	}
	else if (TacticalPlan.AttackChannel == ESoccerAttackChannel::Right)
	{
		DesiredLateral = ScaledCarryLateralOffset * WidthScale;
	}

	FVector TargetLocation =
		OwnGoalLocation +
		AttackDirection * DesiredDepth +
		RightDirection * DesiredLateral;

	TargetLocation.Z = SoccerAICharacter->GetActorLocation().Z;

	return TargetLocation;
}

FVector ASoccerMatchManager::GetOwnGoalCenterLocation(
	ESoccerTeam Team
) const
{
	return GetOwnGoalReferenceLocation(Team);
}


ESoccerMatchPlayState ASoccerMatchManager::GetMatchPlayState() const
{
	return MatchPlayState;
}

int32 ASoccerMatchManager::GetPlayerTeamScore() const
{
	return PlayerTeamScore;
}

int32 ASoccerMatchManager::GetOpponentTeamScore() const
{
	return OpponentTeamScore;
}

ESoccerFormationSystem ASoccerMatchManager::GetFormationSystemForTeam(
	ESoccerTeam Team
) const
{
	return
		Team == ESoccerTeam::PlayerTeam
		? PlayerTeamFormationSystem
		: OpponentTeamFormationSystem;
}

void ASoccerMatchManager::SetFormationSystemForTeam(
	ESoccerTeam Team,
	ESoccerFormationSystem FormationSystem
)
{
	const FSoccerFormationDefinition& Definition =
		SoccerFormationLibrary::GetDefinition(FormationSystem);

	if (!SoccerFormationLibrary::IsValidSevenASideDefinition(Definition))
	{
		return;
	}

	const ESoccerFormationSystem PreviousFormationSystem =
		GetFormationSystemForTeam(Team);

	if (PreviousFormationSystem != FormationSystem)
	{
		BeginLiveTacticalShapeTransitionForTeam(Team);
	}

	if (Team == ESoccerTeam::PlayerTeam)
	{
		PlayerTeamFormationSystem = FormationSystem;
	}
	else
	{
		OpponentTeamFormationSystem = FormationSystem;
	}

	RebuildFormationAssignmentsForTeam(
		Team,
		PreviousFormationSystem,
		true
	);

	EnsureSlotTacticalInstructionsForTeam(Team);

	// Either side may be referenced by slot ID in an explicit marking request.
	// Rebuild on the next tactical update instead of keeping stale slot targets.
	ClearExplicitIndividualMarkingAssignmentsForTeam(ESoccerTeam::PlayerTeam);
	ClearExplicitIndividualMarkingAssignmentsForTeam(ESoccerTeam::OpponentTeam);
}

FSoccerFormationDefinition ASoccerMatchManager::GetFormationDefinitionForTeam(
	ESoccerTeam Team
) const
{
	return SoccerFormationLibrary::GetDefinition(
		GetFormationSystemForTeam(Team)
	);
}

const FSoccerTeamTacticalPlan& ASoccerMatchManager::GetTacticalPlanForTeamInternal(
	ESoccerTeam Team
) const
{
	return
		Team == ESoccerTeam::PlayerTeam
		? PlayerTeamTacticalPlan
		: OpponentTeamTacticalPlan;
}

FSoccerTeamTacticalPlan ASoccerMatchManager::GetTacticalPlanForTeam(
	ESoccerTeam Team
) const
{
	return GetTacticalPlanForTeamInternal(Team);
}

void ASoccerMatchManager::SetTacticalPlanForTeam(
	ESoccerTeam Team,
	const FSoccerTeamTacticalPlan& TacticalPlan
)
{
	const FSoccerTeamTacticalPlan PreviousPlan =
		GetTacticalPlanForTeamInternal(Team);

	if (!AreTeamTacticalPlansEqual(PreviousPlan, TacticalPlan))
	{
		BeginLiveTacticalShapeTransitionForTeam(Team);
	}

	if (Team == ESoccerTeam::PlayerTeam)
	{
		PlayerTeamTacticalPlan = TacticalPlan;
	}
	else
	{
		OpponentTeamTacticalPlan = TacticalPlan;
	}

	// A live menu change should be reflected at the next role assignment without
	// disturbing any restart setup currently in progress.
	if (
		MatchPlayState == ESoccerMatchPlayState::Playing &&
		!IsRestartContextActive()
		)
	{
		ClearAssignedAI();
	}
}

bool ASoccerMatchManager::ShouldUseCollectiveTacticsForOpenPlay(
	ESoccerTeam Team
) const
{
	return ShouldApplyCollectiveTacticsToOpenPlay(Team);
}

bool ASoccerMatchManager::ShouldApplyCollectiveTacticsToOpenPlay(
	ESoccerTeam Team
) const
{
	(void)Team;

	return
		bUseCollectiveTacticsInOpenPlay &&
		MatchPlayState == ESoccerMatchPlayState::Playing &&
		!IsRestartContextActive();
}

void ASoccerMatchManager::UpdateCollectiveTacticalTransitionTracking()
{
	if (
		!bUseCollectiveTacticsInOpenPlay ||
		MatchPlayState != ESoccerMatchPlayState::Playing ||
		IsRestartContextActive() ||
		PossessionTeam == ESoccerPossessionTeam::None
		)
	{
		return;
	}

	const ESoccerTeam ControlledTeam =
		PossessionTeam == ESoccerPossessionTeam::PlayerTeam
		? ESoccerTeam::PlayerTeam
		: ESoccerTeam::OpponentTeam;

	if (!bHasCollectiveLastControlledPossessionTeam)
	{
		bHasCollectiveLastControlledPossessionTeam = true;
		CollectiveLastControlledPossessionTeam = ControlledTeam;
		return;
	}

	if (ControlledTeam == CollectiveLastControlledPossessionTeam)
	{
		return;
	}

	const UWorld* World = GetWorld();

	if (World == nullptr)
	{
		CollectiveLastControlledPossessionTeam = ControlledTeam;
		return;
	}

	const float CurrentTime = World->GetTimeSeconds();
	const ESoccerTeam LosingTeam = CollectiveLastControlledPossessionTeam;

	if (ControlledTeam == ESoccerTeam::PlayerTeam)
	{
		PlayerTeamLastCollectivePossessionGainTime = CurrentTime;
	}
	else
	{
		OpponentTeamLastCollectivePossessionGainTime = CurrentTime;
	}

	if (LosingTeam == ESoccerTeam::PlayerTeam)
	{
		PlayerTeamLastCollectivePossessionLossTime = CurrentTime;
	}
	else
	{
		OpponentTeamLastCollectivePossessionLossTime = CurrentTime;
	}

	CollectiveLastControlledPossessionTeam = ControlledTeam;
}

bool ASoccerMatchManager::IsCollectiveAttackingTransitionActiveForTeam(
	ESoccerTeam Team
) const
{
	if (
		!ShouldApplyCollectiveTacticsToOpenPlay(Team) ||
		!IsTeamCurrentlyAttacking(Team)
		)
	{
		return false;
	}

	const UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return false;
	}

	const float GainTime =
		Team == ESoccerTeam::PlayerTeam
		? PlayerTeamLastCollectivePossessionGainTime
		: OpponentTeamLastCollectivePossessionGainTime;

	return
		World->GetTimeSeconds() - GainTime <=
		FMath::Max(0.0f, CollectiveTransitionDuration);
}

bool ASoccerMatchManager::IsCollectiveDefensiveTransitionActiveForTeam(
	ESoccerTeam Team
) const
{
	if (
		!ShouldApplyCollectiveTacticsToOpenPlay(Team) ||
		!IsTeamCurrentlyDefending(Team)
		)
	{
		return false;
	}

	const UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return false;
	}

	const float LossTime =
		Team == ESoccerTeam::PlayerTeam
		? PlayerTeamLastCollectivePossessionLossTime
		: OpponentTeamLastCollectivePossessionLossTime;

	return
		World->GetTimeSeconds() - LossTime <=
		FMath::Max(0.0f, CollectiveTransitionDuration);
}

float ASoccerMatchManager::GetCollectiveDefensiveBlockDepthOffsetAlpha(
	ESoccerTeam Team
) const
{
	if (!ShouldApplyCollectiveTacticsToOpenPlay(Team))
	{
		return 0.0f;
	}

	const FSoccerTeamTacticalPlan& TacticalPlan =
		GetTacticalPlanForTeamInternal(Team);

	float OffsetAlpha = 0.0f;

	if (TacticalPlan.DefensiveBlock == ESoccerDefensiveBlock::Low)
	{
		OffsetAlpha -= CollectiveDefensiveBlockDepthShiftAlpha;
	}
	else if (TacticalPlan.DefensiveBlock == ESoccerDefensiveBlock::High)
	{
		OffsetAlpha += CollectiveDefensiveBlockDepthShiftAlpha;
	}

	if (IsCollectiveDefensiveTransitionActiveForTeam(Team))
	{
		if (TacticalPlan.DefensiveTransition == ESoccerDefensiveTransition::Regroup)
		{
			OffsetAlpha -= CollectiveRegroupExtraDepthShiftAlpha;
		}
		else if (TacticalPlan.DefensiveTransition == ESoccerDefensiveTransition::CounterPress)
		{
			OffsetAlpha += CollectiveCounterPressExtraDepthShiftAlpha;
		}
	}

	return OffsetAlpha;
}

float ASoccerMatchManager::GetCollectiveAttackWidthScale(ESoccerTeam Team) const
{
	if (!ShouldApplyCollectiveTacticsToOpenPlay(Team))
	{
		return 1.0f;
	}

	const ESoccerAttackingWidth Width =
		GetTacticalPlanForTeamInternal(Team).AttackingWidth;

	if (Width == ESoccerAttackingWidth::Narrow)
	{
		return FMath::Clamp(CollectiveNarrowAttackWidthScale, 0.4f, 1.0f);
	}

	if (Width == ESoccerAttackingWidth::Wide)
	{
		return FMath::Clamp(CollectiveWideAttackWidthScale, 1.0f, 1.8f);
	}

	return 1.0f;
}

float ASoccerMatchManager::GetCollectiveAttackChannelLateralShift(
	ESoccerTeam Team
) const
{
	if (!ShouldApplyCollectiveTacticsToOpenPlay(Team))
	{
		return 0.0f;
	}

	const ESoccerAttackChannel Channel =
		GetTacticalPlanForTeamInternal(Team).AttackChannel;

	const float ScaledShapeShift =
		SoccerFieldDimensions::ScaleAuthoredLateralDistance(
			CollectiveAttackChannelShapeShift
		);

	if (Channel == ESoccerAttackChannel::Left)
	{
		return -ScaledShapeShift;
	}

	if (Channel == ESoccerAttackChannel::Right)
	{
		return ScaledShapeShift;
	}

	return 0.0f;
}

float ASoccerMatchManager::GetCollectivePassToSpaceLeadDistance(
	ESoccerTeam Team
) const
{
	float LeadDistance = AttackDecisionPassToSpaceLeadDistance;

	if (!ShouldApplyCollectiveTacticsToOpenPlay(Team))
	{
		return LeadDistance;
	}

	const FSoccerTeamTacticalPlan& TacticalPlan =
		GetTacticalPlanForTeamInternal(Team);

	if (TacticalPlan.BuildUpStyle == ESoccerBuildUpStyle::ShortPossession)
	{
		LeadDistance *= 0.84f;
	}
	else if (TacticalPlan.BuildUpStyle == ESoccerBuildUpStyle::Direct)
	{
		LeadDistance *= 1.16f;
	}

	if (TacticalPlan.AttackingTempo == ESoccerAttackingTempo::Patient)
	{
		LeadDistance *= 0.92f;
	}
	else if (TacticalPlan.AttackingTempo == ESoccerAttackingTempo::Fast)
	{
		LeadDistance *= 1.08f;
	}

	if (IsCollectiveAttackingTransitionActiveForTeam(Team))
	{
		if (TacticalPlan.AttackingTransition == ESoccerAttackingTransition::RetainPossession)
		{
			LeadDistance *= 0.90f;
		}
		else if (TacticalPlan.AttackingTransition == ESoccerAttackingTransition::CounterAttack)
		{
			LeadDistance *= 1.18f;
		}
	}

	return FMath::Clamp(LeadDistance, 320.0f, 1100.0f);
}

float ASoccerMatchManager::GetCollectivePassRequiredScoreAdjustment(
	ESoccerTeam Team
) const
{
	if (!ShouldApplyCollectiveTacticsToOpenPlay(Team))
	{
		return 0.0f;
	}

	const FSoccerTeamTacticalPlan& TacticalPlan =
		GetTacticalPlanForTeamInternal(Team);

	float Adjustment = 0.0f;

	if (TacticalPlan.BuildUpStyle == ESoccerBuildUpStyle::ShortPossession)
	{
		Adjustment -= 55.0f;
	}
	else if (TacticalPlan.BuildUpStyle == ESoccerBuildUpStyle::Direct)
	{
		Adjustment += 10.0f;
	}

	if (TacticalPlan.AttackingTempo == ESoccerAttackingTempo::Patient)
	{
		Adjustment -= 35.0f;
	}
	else if (TacticalPlan.AttackingTempo == ESoccerAttackingTempo::Fast)
	{
		Adjustment += 20.0f;
	}

	if (IsCollectiveAttackingTransitionActiveForTeam(Team))
	{
		if (TacticalPlan.AttackingTransition == ESoccerAttackingTransition::RetainPossession)
		{
			Adjustment -= 70.0f;
		}
		else if (TacticalPlan.AttackingTransition == ESoccerAttackingTransition::CounterAttack)
		{
			Adjustment -= 20.0f;
		}
	}

	return Adjustment;
}

bool ASoccerMatchManager::ShouldCollectivePressureEngageBall(
	ESoccerTeam DefendingTeam,
	const ASoccerAICharacter* PressureCharacter
) const
{
	if (
		!IsValid(PressureCharacter) ||
		!ShouldApplyCollectiveTacticsToOpenPlay(DefendingTeam)
		)
	{
		return true;
	}

	const FSoccerTeamTacticalPlan& TacticalPlan =
		GetTacticalPlanForTeamInternal(DefendingTeam);

	const FVector ThreatLocation =
		GetCurrentDefensiveThreatLocation(DefendingTeam);

	if (ThreatLocation.IsNearlyZero())
	{
		return true;
	}

	const float ThreatDepthAlpha =
		GetAttackDepthAlphaForLocation(ThreatLocation, DefendingTeam);

	// Close to goal, team safety overrides a low-press instruction.
	if (ThreatDepthAlpha <= CollectiveLowPressEmergencyThreatDepthAlpha)
	{
		return true;
	}

	float MaxEngagementDistance = CollectiveMediumPressMaxEngagementDistance;

	if (TacticalPlan.PressingIntensity == ESoccerPressingIntensity::Low)
	{
		MaxEngagementDistance = CollectiveLowPressMaxEngagementDistance;
	}
	else if (TacticalPlan.PressingIntensity == ESoccerPressingIntensity::High)
	{
		MaxEngagementDistance = CollectiveHighPressMaxEngagementDistance;
	}

	if (IsCollectiveDefensiveTransitionActiveForTeam(DefendingTeam))
	{
		if (TacticalPlan.DefensiveTransition == ESoccerDefensiveTransition::Regroup)
		{
			MaxEngagementDistance = FMath::Min(
				MaxEngagementDistance,
				CollectiveRegroupPressMaxEngagementDistance
			);
		}
		else if (TacticalPlan.DefensiveTransition == ESoccerDefensiveTransition::CounterPress)
		{
			MaxEngagementDistance = FMath::Max(
				MaxEngagementDistance,
				CollectiveHighPressMaxEngagementDistance
			);
		}
	}

	const float DistanceToThreat = FVector::Dist2D(
		PressureCharacter->GetActorLocation(),
		ThreatLocation
	);

	return DistanceToThreat <= FMath::Max(100.0f, MaxEngagementDistance);
}

FSoccerSlotTacticalInstruction
ASoccerMatchManager::GetSlotTacticalInstructionForTeam(
	ESoccerTeam Team,
	FName SlotId
) const
{
	FSoccerSlotTacticalInstruction Result;
	Result.SlotId = SlotId;

	if (SlotId.IsNone())
	{
		return Result;
	}

	const TArray<FSoccerSlotTacticalInstruction>& Instructions =
		GetSlotTacticalInstructionsForTeamInternal(Team);

	for (const FSoccerSlotTacticalInstruction& Instruction : Instructions)
	{
		if (Instruction.SlotId == SlotId)
		{
			return Instruction;
		}
	}

	return Result;
}

bool ASoccerMatchManager::TryGetOpenPlayIndividualInstructionForCharacter(
	const ASoccerCharacterBase* Character,
	FSoccerSlotTacticalInstruction& OutInstruction
) const
{
	OutInstruction = FSoccerSlotTacticalInstruction();

	if (
		!bUseIndividualTacticalInstructionsInOpenPlay ||
		!IsValid(Character) ||
		MatchPlayState != ESoccerMatchPlayState::Playing ||
		IsRestartContextActive()
		)
	{
		return false;
	}

	const FName SlotId =
		GetAssignedFormationSlotId(
			const_cast<ASoccerCharacterBase*>(Character)
		);

	if (SlotId.IsNone())
	{
		return false;
	}

	OutInstruction =
		GetSlotTacticalInstructionForTeam(
			Character->GetTeam(),
			SlotId
		);

	return !OutInstruction.SlotId.IsNone();
}

ESoccerAIOrder ASoccerMatchManager::ApplyIndividualAttackInstructionToOrder(
	const ASoccerAICharacter* SoccerAICharacter,
	ESoccerAIOrder BaseOrder
) const
{
	FSoccerSlotTacticalInstruction Instruction;

	if (
		!TryGetOpenPlayIndividualInstructionForCharacter(
			SoccerAICharacter,
			Instruction
		)
		)
	{
		return BaseOrder;
	}

	switch (Instruction.AttackInstruction)
	{
	case ESoccerIndividualAttackInstruction::HoldPosition:
		return ESoccerAIOrder::MaintainTeamShape;

	case ESoccerIndividualAttackInstruction::LinkPlay:
	case ESoccerIndividualAttackInstruction::ComeShort:
		return ESoccerAIOrder::AttackSupportShort;

	case ESoccerIndividualAttackInstruction::MakeForwardRuns:
		return ESoccerAIOrder::AttackRunIntoSpace;

	case ESoccerIndividualAttackInstruction::StayWide:
		return ESoccerAIOrder::AttackWideSupport;

	case ESoccerIndividualAttackInstruction::TargetPlayer:
		return ESoccerAIOrder::AttackSupportForward;

	case ESoccerIndividualAttackInstruction::Balanced:
	default:
		return BaseOrder;
	}
}


TMap<ASoccerAICharacter*, ASoccerCharacterBase*>&
ASoccerMatchManager::GetMutableExplicitIndividualMarkingAssignmentsForTeam(
	ESoccerTeam Team
)
{
	return
		Team == ESoccerTeam::PlayerTeam
		? PlayerTeamExplicitIndividualMarkingAssignments
		: OpponentTeamExplicitIndividualMarkingAssignments;
}

const TMap<ASoccerAICharacter*, ASoccerCharacterBase*>&
ASoccerMatchManager::GetExplicitIndividualMarkingAssignmentsForTeamInternal(
	ESoccerTeam Team
) const
{
	return
		Team == ESoccerTeam::PlayerTeam
		? PlayerTeamExplicitIndividualMarkingAssignments
		: OpponentTeamExplicitIndividualMarkingAssignments;
}

TMap<ASoccerAICharacter*, ASoccerCharacterBase*>&
ASoccerMatchManager::GetMutableCollectiveManMarkingAssignmentsForTeam(
	ESoccerTeam Team
)
{
	return
		Team == ESoccerTeam::PlayerTeam
		? PlayerTeamCollectiveManMarkingAssignments
		: OpponentTeamCollectiveManMarkingAssignments;
}

const TMap<ASoccerAICharacter*, ASoccerCharacterBase*>&
ASoccerMatchManager::GetCollectiveManMarkingAssignmentsForTeamInternal(
	ESoccerTeam Team
) const
{
	return
		Team == ESoccerTeam::PlayerTeam
		? PlayerTeamCollectiveManMarkingAssignments
		: OpponentTeamCollectiveManMarkingAssignments;
}

void ASoccerMatchManager::ClearExplicitIndividualMarkingAssignmentsForTeam(
	ESoccerTeam Team
)
{
	GetMutableExplicitIndividualMarkingAssignmentsForTeam(Team).Empty();
	GetMutableCollectiveManMarkingAssignmentsForTeam(Team).Empty();
}

ASoccerCharacterBase* ASoccerMatchManager::ResolveRequestedIndividualMarkingTarget(
	const ASoccerAICharacter* Marker
) const
{
	if (
		!bUseExplicitIndividualMarking ||
		!IsValid(Marker) ||
		Marker->GetPlayerRole() == ESoccerPlayerRole::Goalkeeper ||
		!IsTeamCurrentlyDefending(Marker->GetTeam())
		)
	{
		return nullptr;
	}

	FSoccerSlotTacticalInstruction Instruction;
	if (
		!TryGetOpenPlayIndividualInstructionForCharacter(
			Marker,
			Instruction
		) ||
		Instruction.DefensiveInstruction !=
			ESoccerIndividualDefensiveInstruction::MarkTightly ||
		Instruction.MarkingTargetSlotId.IsNone()
		)
	{
		return nullptr;
	}

	const ESoccerTeam AttackingTeam =
		GetOppositeTeam(Marker->GetTeam());

	ASoccerCharacterBase* Target =
		GetFormationSlotAssignedCharacter(
			AttackingTeam,
			Instruction.MarkingTargetSlotId
		);

	if (
		!IsValid(Target) ||
		Target->GetTeam() != AttackingTeam ||
		Target->GetPlayerRole() == ESoccerPlayerRole::Goalkeeper ||
		Target == PossessingCharacter
		)
	{
		return nullptr;
	}

	return Target;
}

ASoccerCharacterBase* ASoccerMatchManager::GetExplicitIndividualMarkingTarget(
	const ASoccerAICharacter* Marker
) const
{
	if (!IsValid(Marker))
	{
		return nullptr;
	}

	if (
		ShouldApplyCollectiveTacticsToOpenPlay(Marker->GetTeam()) &&
		GetTacticalPlanForTeamInternal(Marker->GetTeam()).MarkingStyle ==
			ESoccerMarkingStyle::Zonal
		)
	{
		return nullptr;
	}

	const TMap<ASoccerAICharacter*, ASoccerCharacterBase*>& Assignments =
		GetExplicitIndividualMarkingAssignmentsForTeamInternal(
			Marker->GetTeam()
		);

	ASoccerCharacterBase* const* FoundTarget =
		Assignments.Find(const_cast<ASoccerAICharacter*>(Marker));

	if (FoundTarget == nullptr || !IsValid(*FoundTarget))
	{
		return nullptr;
	}

	// Do not honor a stale runtime map between tactical refreshes if possession,
	// formation or the slot instruction has already changed.
	ASoccerCharacterBase* CurrentRequestedTarget =
		ResolveRequestedIndividualMarkingTarget(Marker);

	if (CurrentRequestedTarget != *FoundTarget)
	{
		return nullptr;
	}

	return *FoundTarget;
}

ASoccerCharacterBase* ASoccerMatchManager::GetCollectiveManMarkingTarget(
	const ASoccerAICharacter* Marker
) const
{
	if (
		!IsValid(Marker) ||
		!ShouldApplyCollectiveTacticsToOpenPlay(Marker->GetTeam()) ||
		!IsTeamCurrentlyDefending(Marker->GetTeam()) ||
		GetTacticalPlanForTeamInternal(Marker->GetTeam()).MarkingStyle !=
			ESoccerMarkingStyle::ManToMan
		)
	{
		return nullptr;
	}

	const TMap<ASoccerAICharacter*, ASoccerCharacterBase*>& Assignments =
		GetCollectiveManMarkingAssignmentsForTeamInternal(Marker->GetTeam());

	ASoccerCharacterBase* const* FoundTarget =
		Assignments.Find(const_cast<ASoccerAICharacter*>(Marker));

	if (
		FoundTarget == nullptr ||
		!IsValid(*FoundTarget) ||
		*FoundTarget == PossessingCharacter
		)
	{
		return nullptr;
	}

	return *FoundTarget;
}

ASoccerCharacterBase* ASoccerMatchManager::GetDefensiveMarkedReceiverForCharacter(
	const ASoccerAICharacter* Marker
) const
{
	if (!IsValid(Marker))
	{
		return nullptr;
	}

	if (ASoccerCharacterBase* ExplicitTarget =
		GetExplicitIndividualMarkingTarget(Marker))
	{
		return ExplicitTarget;
	}

	if (ASoccerCharacterBase* CollectiveTarget =
		GetCollectiveManMarkingTarget(Marker))
	{
		return CollectiveTarget;
	}

	const ESoccerTeam DefendingTeam = Marker->GetTeam();
	const ASoccerAICharacter* AutomaticMarker =
		DefendingTeam == ESoccerTeam::PlayerTeam
		? PlayerTeamDefensiveMarkerAI
		: OpponentTeamDefensiveMarkerAI;

	if (Marker != AutomaticMarker)
	{
		return nullptr;
	}

	ASoccerCharacterBase* AutomaticReceiver =
		DefendingTeam == ESoccerTeam::PlayerTeam
		? PlayerTeamDefensiveMarkedReceiver
		: OpponentTeamDefensiveMarkedReceiver;

	return IsValid(AutomaticReceiver) ? AutomaticReceiver : nullptr;
}

void ASoccerMatchManager::RebuildExplicitIndividualMarkingAssignmentsForTeam(
	ESoccerTeam DefendingTeam,
	const TArray<const ASoccerAICharacter*>& ExcludedCharacters
)
{
	TMap<ASoccerAICharacter*, ASoccerCharacterBase*>& ExplicitAssignments =
		GetMutableExplicitIndividualMarkingAssignmentsForTeam(DefendingTeam);

	TMap<ASoccerAICharacter*, ASoccerCharacterBase*>& CollectiveAssignments =
		GetMutableCollectiveManMarkingAssignmentsForTeam(DefendingTeam);

	const TMap<ASoccerAICharacter*, ASoccerCharacterBase*> PreviousExplicitAssignments =
		ExplicitAssignments;

	const TMap<ASoccerAICharacter*, ASoccerCharacterBase*> PreviousCollectiveAssignments =
		CollectiveAssignments;

	ExplicitAssignments.Empty();
	CollectiveAssignments.Empty();

	if (
		MatchPlayState != ESoccerMatchPlayState::Playing ||
		IsRestartContextActive() ||
		!IsTeamCurrentlyDefending(DefendingTeam)
		)
	{
		return;
	}

	const bool bCollectiveTacticsActive =
		ShouldApplyCollectiveTacticsToOpenPlay(DefendingTeam);

	const ESoccerMarkingStyle CollectiveMarkingStyle =
		bCollectiveTacticsActive
		? GetTacticalPlanForTeamInternal(DefendingTeam).MarkingStyle
		: ESoccerMarkingStyle::Mixed;

	// Collective marking style is authoritative over individual requests. In a
	// zonal scheme no defender is pulled into a fixed one-to-one assignment.
	if (CollectiveMarkingStyle == ESoccerMarkingStyle::Zonal)
	{
		return;
	}

	const bool bCollectiveManToMan =
		CollectiveMarkingStyle == ESoccerMarkingStyle::ManToMan;

	if (!bUseExplicitIndividualMarking && !bCollectiveManToMan)
	{
		return;
	}

	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return;
	}

	struct FExplicitMarkRequest
	{
		ASoccerAICharacter* Marker = nullptr;
		ASoccerCharacterBase* Target = nullptr;
		float Score = -TNumericLimits<float>::Max();
	};

	TArray<FExplicitMarkRequest> Requests;

	if (bUseExplicitIndividualMarking)
	{
		for (TActorIterator<ASoccerAICharacter> It(World); It; ++It)
		{
			ASoccerAICharacter* CandidateMarker = *It;

			if (
				!IsValid(CandidateMarker) ||
				CandidateMarker->GetTeam() != DefendingTeam ||
				CandidateMarker->GetPlayerRole() == ESoccerPlayerRole::Goalkeeper ||
				ExcludedCharacters.Contains(CandidateMarker)
				)
			{
				continue;
			}

			ASoccerCharacterBase* RequestedTarget =
				ResolveRequestedIndividualMarkingTarget(CandidateMarker);

			if (!IsValid(RequestedTarget))
			{
				continue;
			}

			float RequestScore =
				ScoreDefensiveMarkerCandidate(
					DefendingTeam,
					CandidateMarker,
					RequestedTarget
				);

			ASoccerCharacterBase* const* PreviousTarget =
				PreviousExplicitAssignments.Find(CandidateMarker);

			if (
				PreviousTarget != nullptr &&
				*PreviousTarget == RequestedTarget
				)
			{
				RequestScore += IndividualMarkingCurrentAssignmentBonus;
			}

			FExplicitMarkRequest Request;
			Request.Marker = CandidateMarker;
			Request.Target = RequestedTarget;
			Request.Score = RequestScore;
			Requests.Add(Request);
		}
	}

	Requests.Sort(
		[](const FExplicitMarkRequest& A, const FExplicitMarkRequest& B)
		{
			if (!FMath::IsNearlyEqual(A.Score, B.Score))
			{
				return A.Score > B.Score;
			}

			const int32 AId = IsValid(A.Marker)
				? A.Marker->GetUniqueID()
				: TNumericLimits<int32>::Max();
			const int32 BId = IsValid(B.Marker)
				? B.Marker->GetUniqueID()
				: TNumericLimits<int32>::Max();

			return AId < BId;
		}
	);

	TSet<ASoccerCharacterBase*> UsedTargets;

	for (const FExplicitMarkRequest& Request : Requests)
	{
		if (
			!IsValid(Request.Marker) ||
			!IsValid(Request.Target) ||
			UsedTargets.Contains(Request.Target)
			)
		{
			continue;
		}

		ExplicitAssignments.Add(Request.Marker, Request.Target);
		UsedTargets.Add(Request.Target);
	}

	if (!bCollectiveManToMan)
	{
		return;
	}

	// Man-to-man fills the remaining safe-to-use defenders against remaining
	// off-ball attackers. Pressure, goal-lane and central-cover players were
	// excluded by the caller and therefore keep those higher-priority duties.
	struct FAutomaticManMarkPair
	{
		ASoccerAICharacter* Marker = nullptr;
		ASoccerCharacterBase* Target = nullptr;
		float Score = -TNumericLimits<float>::Max();
	};

	TArray<ASoccerAICharacter*> AvailableMarkers;
	TArray<ASoccerCharacterBase*> AvailableTargets;

	for (TActorIterator<ASoccerAICharacter> It(World); It; ++It)
	{
		ASoccerAICharacter* CandidateMarker = *It;

		if (
			!IsValid(CandidateMarker) ||
			CandidateMarker->GetTeam() != DefendingTeam ||
			CandidateMarker->GetPlayerRole() == ESoccerPlayerRole::Goalkeeper ||
			ExcludedCharacters.Contains(CandidateMarker) ||
			ExplicitAssignments.Contains(CandidateMarker)
			)
		{
			continue;
		}

		AvailableMarkers.Add(CandidateMarker);
	}

	const ESoccerTeam AttackingTeam = GetOppositeTeam(DefendingTeam);

	for (TActorIterator<ASoccerCharacterBase> It(World); It; ++It)
	{
		ASoccerCharacterBase* CandidateTarget = *It;

		if (
			!IsValid(CandidateTarget) ||
			CandidateTarget->GetTeam() != AttackingTeam ||
			CandidateTarget->GetPlayerRole() == ESoccerPlayerRole::Goalkeeper ||
			CandidateTarget == PossessingCharacter ||
			UsedTargets.Contains(CandidateTarget)
			)
		{
			continue;
		}

		AvailableTargets.Add(CandidateTarget);
	}

	TArray<FAutomaticManMarkPair> AutomaticPairs;
	AutomaticPairs.Reserve(AvailableMarkers.Num() * AvailableTargets.Num());

	for (ASoccerAICharacter* CandidateMarker : AvailableMarkers)
	{
		for (ASoccerCharacterBase* CandidateTarget : AvailableTargets)
		{
			FAutomaticManMarkPair Pair;
			Pair.Marker = CandidateMarker;
			Pair.Target = CandidateTarget;
			Pair.Score = ScoreDefensiveMarkerCandidate(
				DefendingTeam,
				CandidateMarker,
				CandidateTarget
			);

			ASoccerCharacterBase* const* PreviousTarget =
				PreviousCollectiveAssignments.Find(CandidateMarker);

			if (
				PreviousTarget != nullptr &&
				*PreviousTarget == CandidateTarget
				)
			{
				Pair.Score += IndividualMarkingCurrentAssignmentBonus;
			}

			AutomaticPairs.Add(Pair);
		}
	}

	AutomaticPairs.Sort(
		[](const FAutomaticManMarkPair& A, const FAutomaticManMarkPair& B)
		{
			if (!FMath::IsNearlyEqual(A.Score, B.Score))
			{
				return A.Score > B.Score;
			}

			const int32 AMarkerId = IsValid(A.Marker)
				? A.Marker->GetUniqueID()
				: TNumericLimits<int32>::Max();
			const int32 BMarkerId = IsValid(B.Marker)
				? B.Marker->GetUniqueID()
				: TNumericLimits<int32>::Max();

			if (AMarkerId != BMarkerId)
			{
				return AMarkerId < BMarkerId;
			}

			const int32 ATargetId = IsValid(A.Target)
				? A.Target->GetUniqueID()
				: TNumericLimits<int32>::Max();
			const int32 BTargetId = IsValid(B.Target)
				? B.Target->GetUniqueID()
				: TNumericLimits<int32>::Max();

			return ATargetId < BTargetId;
		}
	);

	TSet<ASoccerAICharacter*> UsedCollectiveMarkers;

	for (const FAutomaticManMarkPair& Pair : AutomaticPairs)
	{
		if (
			!IsValid(Pair.Marker) ||
			!IsValid(Pair.Target) ||
			UsedCollectiveMarkers.Contains(Pair.Marker) ||
			UsedTargets.Contains(Pair.Target)
			)
		{
			continue;
		}

		CollectiveAssignments.Add(Pair.Marker, Pair.Target);
		UsedCollectiveMarkers.Add(Pair.Marker);
		UsedTargets.Add(Pair.Target);
	}
}

void ASoccerMatchManager::SetSlotTacticalInstructionForTeam(
	ESoccerTeam Team,
	const FSoccerSlotTacticalInstruction& Instruction
)
{
	if (Instruction.SlotId.IsNone())
	{
		return;
	}

	const FSoccerFormationDefinition& CurrentDefinition =
		SoccerFormationLibrary::GetDefinition(
			GetFormationSystemForTeam(Team)
		);

	if (FindFormationSlotById(CurrentDefinition, Instruction.SlotId) == nullptr)
	{
		return;
	}

	TArray<FSoccerSlotTacticalInstruction>& Instructions =
		GetMutableSlotTacticalInstructionsForTeam(Team);

	for (FSoccerSlotTacticalInstruction& ExistingInstruction : Instructions)
	{
		if (ExistingInstruction.SlotId == Instruction.SlotId)
		{
			ExistingInstruction = Instruction;
			ClearExplicitIndividualMarkingAssignmentsForTeam(Team);
			return;
		}
	}

	Instructions.Add(Instruction);
	ClearExplicitIndividualMarkingAssignmentsForTeam(Team);
}

void ASoccerMatchManager::SetSlotTacticalInstructionsForTeam(
	ESoccerTeam Team,
	const TArray<FSoccerSlotTacticalInstruction>& Instructions
)
{
	TArray<FSoccerSlotTacticalInstruction>& StoredInstructions =
		GetMutableSlotTacticalInstructionsForTeam(Team);

	StoredInstructions.Empty();
	EnsureSlotTacticalInstructionsForTeam(Team);

	for (const FSoccerSlotTacticalInstruction& Instruction : Instructions)
	{
		SetSlotTacticalInstructionForTeam(Team, Instruction);
	}

	ClearExplicitIndividualMarkingAssignmentsForTeam(Team);
}

TArray<FSoccerSlotTacticalInstruction>
ASoccerMatchManager::GetSlotTacticalInstructionsForTeam(
	ESoccerTeam Team
) const
{
	TArray<FSoccerSlotTacticalInstruction> Result;

	const FSoccerFormationDefinition& CurrentDefinition =
		SoccerFormationLibrary::GetDefinition(
			GetFormationSystemForTeam(Team)
		);

	Result.Reserve(CurrentDefinition.Slots.Num());

	for (const FSoccerFormationSlot& FormationSlot : CurrentDefinition.Slots)
	{
		Result.Add(
			GetSlotTacticalInstructionForTeam(
				Team,
				FormationSlot.SlotId
			)
		);
	}

	return Result;
}

TArray<FSoccerSlotTacticalInstruction>&
ASoccerMatchManager::GetMutableSlotTacticalInstructionsForTeam(
	ESoccerTeam Team
)
{
	return
		Team == ESoccerTeam::PlayerTeam
		? PlayerTeamSlotTacticalInstructions
		: OpponentTeamSlotTacticalInstructions;
}

const TArray<FSoccerSlotTacticalInstruction>&
ASoccerMatchManager::GetSlotTacticalInstructionsForTeamInternal(
	ESoccerTeam Team
) const
{
	return
		Team == ESoccerTeam::PlayerTeam
		? PlayerTeamSlotTacticalInstructions
		: OpponentTeamSlotTacticalInstructions;
}

void ASoccerMatchManager::EnsureSlotTacticalInstructionsForTeam(
	ESoccerTeam Team
)
{
	TArray<FSoccerSlotTacticalInstruction>& Instructions =
		GetMutableSlotTacticalInstructionsForTeam(Team);

	const FSoccerFormationDefinition& CurrentDefinition =
		SoccerFormationLibrary::GetDefinition(
			GetFormationSystemForTeam(Team)
		);

	for (const FSoccerFormationSlot& FormationSlot : CurrentDefinition.Slots)
	{
		bool bAlreadyExists = false;

		for (const FSoccerSlotTacticalInstruction& Instruction : Instructions)
		{
			if (Instruction.SlotId == FormationSlot.SlotId)
			{
				bAlreadyExists = true;
				break;
			}
		}

		if (!bAlreadyExists)
		{
			FSoccerSlotTacticalInstruction NewInstruction;
			NewInstruction.SlotId = FormationSlot.SlotId;
			Instructions.Add(NewInstruction);
		}
	}
}

bool ASoccerMatchManager::ApplyPersistentDirectorTechnicalSetupToPlayerTeam(
	bool bApplyStartingLineupProfiles
)
{
	if (!bUsePersistentDirectorTechnicalSetupForPlayerTeam)
	{
		return false;
	}

	UWorld* World = GetWorld();
	USoccerGameInstance* SoccerGameInstance =
		World != nullptr
			? Cast<USoccerGameInstance>(World->GetGameInstance())
			: nullptr;

	if (
		!IsValid(SoccerGameInstance) ||
		!SoccerGameInstance->HasLoadedTeamSetup()
	)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[DirectorMatch] Persistent PlayerTeam setup unavailable; keeping match defaults.")
		);
		return false;
	}

	const FSoccerTeamSetup PersistentSetup =
		SoccerGameInstance->GetCurrentTeamSetup();

	const FSoccerFormationDefinition& PersistentFormation =
		SoccerFormationLibrary::GetDefinition(PersistentSetup.FormationSystem);

	if (!SoccerFormationLibrary::IsValidSevenASideDefinition(PersistentFormation))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[DirectorMatch] Saved formation is invalid; keeping match defaults.")
		);
		return false;
	}

	if (bApplyStartingLineupProfiles && TotalMatchElapsedSeconds <= 0.01f)
	{
		InitializeMatchSquadState(
			ESoccerTeam::PlayerTeam,
			SoccerGameInstance->GetCurrentMatchSetup().PlayerTeamClubId,
			PersistentSetup.StartingLineupBySlot,
			PersistentSetup.BenchPlayerIds
		);
	}

	const ESoccerFormationSystem PreviousFormationSystem =
		PlayerTeamFormationSystem;

	PlayerTeamFormationSystem = PersistentSetup.FormationSystem;
	PlayerTeamTacticalPlan = PersistentSetup.TacticalPlan;
	PlayerTeamSlotTacticalInstructions = PersistentSetup.SlotInstructions;

	const bool bPreserveExistingAssignments =
		PlayerTeamFormationAssignments.Num() > 0;

	RebuildFormationAssignmentsForTeam(
		ESoccerTeam::PlayerTeam,
		PreviousFormationSystem,
		bPreserveExistingAssignments
	);
	EnsureSlotTacticalInstructionsForTeam(ESoccerTeam::PlayerTeam);

	int32 AppliedProfileCount = 0;
	int32 MissingProfileCount = 0;
	int32 EmptyLineupSlotCount = 0;

	if (bApplyStartingLineupProfiles)
	{
		for (const FSoccerFormationSlot& FormationSlot : PersistentFormation.Slots)
		{
			const FName* PlayerIdPtr =
				PersistentSetup.StartingLineupBySlot.Find(FormationSlot.SlotId);

			if (PlayerIdPtr == nullptr || PlayerIdPtr->IsNone())
			{
				++EmptyLineupSlotCount;
				continue;
			}

			ASoccerCharacterBase* MatchCharacter =
				GetFormationSlotAssignedCharacter(
					ESoccerTeam::PlayerTeam,
					FormationSlot.SlotId
				);

			if (!IsValid(MatchCharacter))
			{
				++MissingProfileCount;
				UE_LOG(
					LogTemp,
					Warning,
					TEXT("[DirectorMatch] No runtime character found for slot '%s' (PlayerId '%s')."),
					*FormationSlot.SlotId.ToString(),
					*PlayerIdPtr->ToString()
				);
				continue;
			}

			USoccerPlayerProfile* PlayerProfile =
				SoccerGameInstance->FindPlayerProfileById(*PlayerIdPtr);

			if (!IsValid(PlayerProfile))
			{
				++MissingProfileCount;
				UE_LOG(
					LogTemp,
					Warning,
					TEXT("[DirectorMatch] Could not resolve PlayerId '%s' for slot '%s'; existing actor profile is preserved."),
					*PlayerIdPtr->ToString(),
					*FormationSlot.SlotId.ToString()
				);
				continue;
			}

			MatchCharacter->SetPlayerProfileForMatch(PlayerProfile);
			ApplySelectedClubKitToCharacter(MatchCharacter);
			++AppliedProfileCount;

			UE_LOG(
				LogTemp,
				Display,
				TEXT("[DirectorMatch] Slot %s -> %s -> actor %s%s."),
				*FormationSlot.SlotId.ToString(),
				*PlayerProfile->Identity.PlayerId.ToString(),
				*MatchCharacter->GetName(),
				Cast<AThirdPersonCppCharacter>(MatchCharacter) != nullptr
					? TEXT(" [HUMAN]")
					: TEXT("")
			);
		}
	}

	// The start-of-match menu opens after the first kickoff state has already
	// been created. If the coach changed the formation/lineup there, rebuild the
	// kickoff once so taker/receiver and Preparation positions use the new setup.
	if (
		bApplyStartingLineupProfiles &&
		TotalMatchElapsedSeconds <= 0.01f &&
		IsKickoffMatchStateActive()
	)
	{
		const ESoccerTeam KickoffTeamToReconfigure = PendingKickoffTeam;
		StartKickoff(KickoffTeamToReconfigure);
	}

	UE_LOG(
		LogTemp,
		Display,
		TEXT("[DirectorMatch] PlayerTeam setup applied: formation=%d, starters=%d/7, profilesApplied=%d, emptySlots=%d, unresolved=%d, lineupProfiles=%s."),
		static_cast<int32>(PlayerTeamFormationSystem),
		PersistentSetup.StartingLineupBySlot.Num(),
		AppliedProfileCount,
		EmptyLineupSlotCount,
		MissingProfileCount,
		bApplyStartingLineupProfiles ? TEXT("YES") : TEXT("NO")
	);

	return true;
}

bool ASoccerMatchManager::ShouldShowFormationMenuAtMatchStart() const
{
	return bShowFormationMenuAtMatchStart;
}

bool ASoccerMatchManager::ShouldPauseGameWhileFormationMenuOpen() const
{
	return bPauseGameWhileFormationMenuOpen;
}

FVector ASoccerMatchManager::GetFormationSlotWorldLocation(
	ESoccerTeam Team,
	const FSoccerFormationSlot& Slot
) const
{
	// Prefer the authoritative SoccerField conversion so normalized formation
	// coordinates automatically follow pitch size, translation, rotation and
	// the team's current field end.
	if (IsValid(SoccerField))
	{
		FVector ResolvedWorldLocation = FVector::ZeroVector;

		if (
			SoccerFormationLibrary::TryResolveSlotWorldLocation(
				SoccerField,
				GetOwnGoalLineSign(Team),
				Slot,
				ResolvedWorldLocation,
				0.0f
			)
		)
		{
			return ResolvedWorldLocation;
		}
	}

	// Legacy-safe fallback for levels that temporarily have no ASoccerField.
	const FVector OwnGoalLocation =
		GetOwnGoalReferenceLocation(Team);

	const FVector OpponentGoalLocation =
		GetOpponentGoalReferenceLocation(Team);

	FVector AttackVector =
		OpponentGoalLocation - OwnGoalLocation;

	AttackVector.Z = 0.0f;

	const float FieldLength = AttackVector.Size();

	if (FieldLength <= 1.0f)
	{
		return FVector::ZeroVector;
	}

	const FVector AttackDirection =
		AttackVector / FieldLength;

	const FVector RightDirection =
		GetFieldRightDirectionForTeam(Team);

	const float SafeDepthAlpha =
		FMath::Clamp(Slot.DepthAlpha, 0.0f, 1.0f);

	const float SafeLateralAlpha =
		FMath::Clamp(Slot.LateralAlpha, -1.0f, 1.0f);

	FVector FallbackLocation =
		OwnGoalLocation +
		AttackDirection * (SafeDepthAlpha * FieldLength) +
		RightDirection *
			(SafeLateralAlpha * SoccerFieldDimensions::HalfPitchWidthCm);

	FallbackLocation.Z = GetActorLocation().Z;

	return FallbackLocation;
}

FName ASoccerMatchManager::GetAssignedFormationSlotId(
	ASoccerCharacterBase* Character
) const
{
	if (!IsValid(Character))
	{
		return NAME_None;
	}

	const TMap<ASoccerCharacterBase*, FName>& Assignments =
		GetFormationAssignmentsForTeamInternal(Character->GetTeam());

	const FName* SlotId = Assignments.Find(Character);
	return SlotId != nullptr ? *SlotId : NAME_None;
}

bool ASoccerMatchManager::GetAssignedFormationSlotForCharacter(
	ASoccerCharacterBase* Character,
	FSoccerFormationSlot& OutSlot
) const
{
	if (!IsValid(Character))
	{
		return false;
	}

	const FName SlotId = GetAssignedFormationSlotId(Character);
	if (SlotId.IsNone())
	{
		return false;
	}

	const FSoccerFormationDefinition& Definition =
		SoccerFormationLibrary::GetDefinition(
			GetFormationSystemForTeam(Character->GetTeam())
		);

	const FSoccerFormationSlot* Slot =
		FindFormationSlotById(Definition, SlotId);

	if (Slot == nullptr)
	{
		return false;
	}

	OutSlot = *Slot;
	return true;
}

ASoccerCharacterBase* ASoccerMatchManager::GetFormationSlotAssignedCharacter(
	ESoccerTeam Team,
	FName SlotId
) const
{
	if (SlotId.IsNone())
	{
		return nullptr;
	}

	const TMap<ASoccerCharacterBase*, FName>& Assignments =
		GetFormationAssignmentsForTeamInternal(Team);

	for (const TPair<ASoccerCharacterBase*, FName>& Assignment : Assignments)
	{
		if (Assignment.Value == SlotId && IsValid(Assignment.Key))
		{
			return Assignment.Key;
		}
	}

	return nullptr;
}

bool ASoccerMatchManager::ShouldUseFormationForOpenPlayStructure(
	const ASoccerCharacterBase* Character
) const
{
	return
		bUseFormationForOpenPlayStructure &&
		IsValid(Character) &&
		MatchPlayState == ESoccerMatchPlayState::Playing &&
		!IsRestartContextActive();
}

bool ASoccerMatchManager::TryGetFormationStructuralReference(
	const ASoccerCharacterBase* Character,
	FSoccerFormationSlot& OutSlot,
	FVector& OutWorldLocation
) const
{
	OutSlot = FSoccerFormationSlot();
	OutWorldLocation = FVector::ZeroVector;

	if (!ShouldUseFormationForOpenPlayStructure(Character))
	{
		return false;
	}

	const ESoccerTeam Team = Character->GetTeam();
	const TMap<ASoccerCharacterBase*, FName>& Assignments =
		GetFormationAssignmentsForTeamInternal(Team);

	FName SlotId = NAME_None;

	for (const TPair<ASoccerCharacterBase*, FName>& Assignment : Assignments)
	{
		if (Assignment.Key == Character)
		{
			SlotId = Assignment.Value;
			break;
		}
	}

	if (SlotId.IsNone())
	{
		return false;
	}

	const FSoccerFormationDefinition& Definition =
		SoccerFormationLibrary::GetDefinition(GetFormationSystemForTeam(Team));

	const FSoccerFormationSlot* Slot =
		FindFormationSlotById(Definition, SlotId);

	if (Slot == nullptr)
	{
		return false;
	}

	FVector SlotLocation = GetFormationSlotWorldLocation(Team, *Slot);
	if (SlotLocation.IsNearlyZero())
	{
		return false;
	}

	SlotLocation.Z = Character->GetActorLocation().Z;

	OutSlot = *Slot;
	OutWorldLocation = SlotLocation;
	return true;
}

TMap<ASoccerCharacterBase*, FName>& ASoccerMatchManager::GetMutableFormationAssignmentsForTeam(
	ESoccerTeam Team
)
{
	return Team == ESoccerTeam::PlayerTeam
		? PlayerTeamFormationAssignments
		: OpponentTeamFormationAssignments;
}

const TMap<ASoccerCharacterBase*, FName>& ASoccerMatchManager::GetFormationAssignmentsForTeamInternal(
	ESoccerTeam Team
) const
{
	return Team == ESoccerTeam::PlayerTeam
		? PlayerTeamFormationAssignments
		: OpponentTeamFormationAssignments;
}

const FSoccerFormationSlot* ASoccerMatchManager::FindFormationSlotById(
	const FSoccerFormationDefinition& Definition,
	FName SlotId
) const
{
	if (SlotId.IsNone())
	{
		return nullptr;
	}

	for (const FSoccerFormationSlot& Slot : Definition.Slots)
	{
		if (Slot.SlotId == SlotId)
		{
			return &Slot;
		}
	}

	return nullptr;
}

void ASoccerMatchManager::CollectFormationPlayersForTeam(
	ESoccerTeam Team,
	TArray<ASoccerCharacterBase*>& OutPlayers
) const
{
	OutPlayers.Reset();

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	for (TActorIterator<ASoccerCharacterBase> It(World); It; ++It)
	{
		ASoccerCharacterBase* Candidate = *It;
		if (
			IsValid(Candidate) &&
			!Candidate->ActorHasTag(FName(TEXT("SubstitutionPresentation"))) &&
			Candidate->GetTeam() == Team
		)
		{
			OutPlayers.Add(Candidate);
		}
	}

	OutPlayers.Sort(
		[](const ASoccerCharacterBase& A, const ASoccerCharacterBase& B)
		{
			return A.GetName() < B.GetName();
		}
	);
}

FVector2D ASoccerMatchManager::GetFormationReferenceCoordinatesForCharacter(
	ESoccerTeam Team,
	const ASoccerCharacterBase* Character
) const
{
	if (!IsValid(Character))
	{
		return FVector2D(0.5f, 0.0f);
	}

	FVector ReferenceLocation = Character->GetActorLocation();

	const ASoccerAICharacter* AICharacter =
		Cast<ASoccerAICharacter>(Character);
	if (AICharacter != nullptr && IsValid(AICharacter->GetHomePositionActor()))
	{
		ReferenceLocation = GetTeamRebasedFieldReferenceLocation(
			Team,
			AICharacter->GetHomePositionActor()->GetActorLocation()
		);
	}

	const FVector OwnGoalLocation = GetOwnGoalReferenceLocation(Team);
	const FVector OpponentGoalLocation = GetOpponentGoalReferenceLocation(Team);

	FVector AttackVector = OpponentGoalLocation - OwnGoalLocation;
	AttackVector.Z = 0.0f;

	const float FieldLength = AttackVector.Size();
	if (FieldLength <= 1.0f)
	{
		return FVector2D(0.5f, 0.0f);
	}

	const FVector AttackDirection = AttackVector / FieldLength;
	const FVector RightDirection = GetFieldRightDirectionForTeam(Team);

	FVector FromOwnGoal = ReferenceLocation - OwnGoalLocation;
	FromOwnGoal.Z = 0.0f;

	const float DepthAlpha = FMath::Clamp(
		FVector::DotProduct(FromOwnGoal, AttackDirection) / FieldLength,
		0.0f,
		1.0f
	);

	const float LateralAlpha = FMath::Clamp(
		FVector::DotProduct(FromOwnGoal, RightDirection) /
			FMath::Max(1.0f, SoccerFieldDimensions::HalfPitchWidthCm),
		-1.0f,
		1.0f
	);

	return FVector2D(DepthAlpha, LateralAlpha);
}

float ASoccerMatchManager::CalculateFormationAssignmentCost(
	ESoccerTeam Team,
	const ASoccerCharacterBase* Character,
	const FSoccerFormationSlot& CandidateSlot,
	const FSoccerFormationSlot* PreviousSlot,
	FName PreviousSlotId
) const
{
	if (!IsValid(Character))
	{
		return 1000000.0f;
	}

	const ESoccerPlayerRole PlayerRole = Character->GetPlayerRole();
	const bool bPlayerIsGoalkeeper =
		PlayerRole == ESoccerPlayerRole::Goalkeeper;
	const bool bSlotIsGoalkeeper =
		CandidateSlot.PlayerRole == ESoccerPlayerRole::Goalkeeper;

	// Keep the goalkeeper role isolated. Field-role adaptation is allowed,
	// goalkeeper adaptation is not.
	if (bPlayerIsGoalkeeper != bSlotIsGoalkeeper)
	{
		return 500000.0f;
	}

	float Cost = 0.0f;

	if (!PreviousSlotId.IsNone() && PreviousSlotId == CandidateSlot.SlotId)
	{
		// Strongly preserve identity when the same structural slot exists in the
		// new formation. This prevents unnecessary player swapping at runtime.
		Cost -= 10000.0f;
	}

	if (PlayerRole != CandidateSlot.PlayerRole)
	{
		const bool bAdjacentRole =
			(PlayerRole == ESoccerPlayerRole::Defender && CandidateSlot.PlayerRole == ESoccerPlayerRole::Midfielder) ||
			(PlayerRole == ESoccerPlayerRole::Midfielder && CandidateSlot.PlayerRole == ESoccerPlayerRole::Defender) ||
			(PlayerRole == ESoccerPlayerRole::Midfielder && CandidateSlot.PlayerRole == ESoccerPlayerRole::Forward) ||
			(PlayerRole == ESoccerPlayerRole::Forward && CandidateSlot.PlayerRole == ESoccerPlayerRole::Midfielder);

		Cost += bAdjacentRole ? 600.0f : 1200.0f;
	}

	FVector2D ReferenceCoordinates;
	if (PreviousSlot != nullptr)
	{
		ReferenceCoordinates = FVector2D(
			PreviousSlot->DepthAlpha,
			PreviousSlot->LateralAlpha
		);
	}
	else
	{
		ReferenceCoordinates =
			GetFormationReferenceCoordinatesForCharacter(Team, Character);
	}

	const float DepthDifference = FMath::Abs(
		ReferenceCoordinates.X - CandidateSlot.DepthAlpha
	);
	const float LateralDifference = FMath::Abs(
		ReferenceCoordinates.Y - CandidateSlot.LateralAlpha
	);

	// Lateral continuity is slightly more important for assignment identity:
	// a left-sided player should not swap to the right just because a new line
	// is a few centimeters closer in depth.
	Cost += DepthDifference * 220.0f;
	Cost += LateralDifference * 320.0f;

	return Cost;
}

void ASoccerMatchManager::RebuildFormationAssignmentsForTeam(
	ESoccerTeam Team,
	ESoccerFormationSystem PreviousFormationSystem,
	bool bPreserveExistingAssignments
)
{
	const FSoccerFormationDefinition& NewDefinition =
		SoccerFormationLibrary::GetDefinition(GetFormationSystemForTeam(Team));

	if (!SoccerFormationLibrary::IsValidSevenASideDefinition(NewDefinition))
	{
		return;
	}

	TArray<ASoccerCharacterBase*> Players;
	CollectFormationPlayersForTeam(Team, Players);

	TMap<ASoccerCharacterBase*, FName>& Assignments =
		GetMutableFormationAssignmentsForTeam(Team);

	const TMap<ASoccerCharacterBase*, FName> PreviousAssignments = Assignments;
	Assignments.Reset();

	if (Players.Num() == 0)
	{
		return;
	}

	if (Players.Num() != NewDefinition.Slots.Num())
	{
		ASoccerDebugManager::Message(
			this,
			ESoccerDebugCategory::Formation,
			FString::Printf(
				TEXT("Formation assignment warning: team=%d players=%d slots=%d"),
				static_cast<int32>(Team),
				Players.Num(),
				NewDefinition.Slots.Num()
			),
			FColor::Yellow
		);
	}

	const FSoccerFormationDefinition& PreviousDefinition =
		SoccerFormationLibrary::GetDefinition(PreviousFormationSystem);

	TMap<ASoccerCharacterBase*, FName> PreviousSlotIds;
	if (bPreserveExistingAssignments)
	{
		for (const TPair<ASoccerCharacterBase*, FName>& Assignment : PreviousAssignments)
		{
			if (IsValid(Assignment.Key) && !Assignment.Value.IsNone())
			{
				PreviousSlotIds.Add(Assignment.Key, Assignment.Value);
			}
		}
	}

	const int32 AssignmentCount = FMath::Min(Players.Num(), NewDefinition.Slots.Num());
	if (AssignmentCount <= 0)
	{
		return;
	}

	// Seven-a-side means at most 7! = 5040 permutations. Exhaustive matching
	// gives deterministic globally coherent assignments and runs only on setup
	// or a formation change, never per Tick.
	TArray<int32> SlotPermutation;
	SlotPermutation.Reserve(NewDefinition.Slots.Num());
	for (int32 SlotIndex = 0; SlotIndex < NewDefinition.Slots.Num(); ++SlotIndex)
	{
		SlotPermutation.Add(SlotIndex);
	}

	TArray<int32> BestPermutation;
	float BestCost = TNumericLimits<float>::Max();

	do
	{
		float TotalCost = 0.0f;

		for (int32 PlayerIndex = 0; PlayerIndex < AssignmentCount; ++PlayerIndex)
		{
			ASoccerCharacterBase* Character = Players[PlayerIndex];
			const FSoccerFormationSlot& CandidateSlot =
				NewDefinition.Slots[SlotPermutation[PlayerIndex]];

			const FName* PreviousSlotIdPtr = PreviousSlotIds.Find(Character);
			const FName PreviousSlotId =
				PreviousSlotIdPtr != nullptr ? *PreviousSlotIdPtr : NAME_None;

			const FSoccerFormationSlot* PreviousSlot =
				FindFormationSlotById(PreviousDefinition, PreviousSlotId);

			TotalCost += CalculateFormationAssignmentCost(
				Team,
				Character,
				CandidateSlot,
				PreviousSlot,
				PreviousSlotId
			);
		}

		if (TotalCost < BestCost)
		{
			BestCost = TotalCost;
			BestPermutation = SlotPermutation;
		}
	}
	while (std::next_permutation(SlotPermutation.GetData(), SlotPermutation.GetData() + SlotPermutation.Num()));

	if (BestPermutation.Num() != NewDefinition.Slots.Num())
	{
		return;
	}

	Assignments.Reserve(AssignmentCount);

	for (int32 PlayerIndex = 0; PlayerIndex < AssignmentCount; ++PlayerIndex)
	{
		Assignments.Add(
			Players[PlayerIndex],
			NewDefinition.Slots[BestPermutation[PlayerIndex]].SlotId
		);
	}
}


void ASoccerMatchManager::HandleGoalScored(ESoccerTeam ScoringTeam)
{
	// Defensa extra:
	// SoccerGoalTrigger ya valida Playing, pero esto evita goles duplicados
	// si otro sistema llama directo a HandleGoalScored.
	if (MatchPlayState != ESoccerMatchPlayState::Playing)
	{
		return;
	}

	CancelBallOutOfPlayDelay();
	CancelThrowInRestart();
	CancelGoalLineRestart();

	if (ScoringTeam == ESoccerTeam::PlayerTeam)
	{
		PlayerTeamScore++;
	}
	else
	{
		OpponentTeamScore++;
	}

	PendingKickoffTeam =
		GetOppositeTeam(ScoringTeam);

	// Desde este momento el partido deja de estar en juego.
	// Esto evita que SoccerGoalTrigger vuelva a contar el mismo gol.
	MatchPlayState = ESoccerMatchPlayState::GoalScored;

	// Cortamos la lógica de posesión/juego, pero NO tocamos la física
	// ni la posición de la pelota. Queremos verla seguir dentro del arco.
	PossessionTeam = ESoccerPossessionTeam::None;
	PossessingCharacter = nullptr;

	ClearAttackState();
	ClearAssignedAI();
	ClearNoRetouchRestriction();
	ClearAttackRunRelease();

	ReleaseAllAIBallPossessions();
	ReleaseAllHumanBallPossessions();

	const float GoalReplayPostDelay =
		bEnableInstantReplayAfterGoal && IsValid(InstantReplayManager)
		? FMath::Max(0.0f, InstantReplayGoalPostEventSeconds)
		: 0.0f;
	const float EffectiveGoalResetDelay =
		FMath::Max(0.0f, GoalResetDelay) + GoalReplayPostDelay;

	if (GEngine)
	{
		const FString ScoreMessage = FString::Printf(
			TEXT("GOOOL - PlayerTeam %d - %d OpponentTeam"),
			PlayerTeamScore,
			OpponentTeamScore
		);

		GEngine->AddOnScreenDebugMessage(
			-1,
			EffectiveGoalResetDelay,
			FColor::Green,
			ScoreMessage
		);
	}

	if (GetWorld() != nullptr)
	{
		GetWorldTimerManager().ClearTimer(
			GoalResetTimerHandle
		);

		GetWorldTimerManager().SetTimer(
			GoalResetTimerHandle,
			this,
			&ASoccerMatchManager::ResetAfterGoal,
			EffectiveGoalResetDelay,
			false
		);
	}
	else
	{
		ResetAfterGoal();
	}

	/*
	 * The replay may wait briefly to record action after the goal. The reset
	 * timer includes that delay and then pauses naturally during playback.
	 */
	TryStartGoalInstantReplay(ScoringTeam);
}

void ASoccerMatchManager::ResetAfterGoal()
{
	if (GetWorld() != nullptr)
	{
		GetWorldTimerManager().ClearTimer(
			GoalResetTimerHandle
		);
		GetWorldTimerManager().ClearTimer(
			GoalReplayStartTimerHandle
		);
	}

	PossessionTeam = ESoccerPossessionTeam::None;
	PossessingCharacter = nullptr;

	ClearAttackState();
	ClearAssignedAI();
	ClearNoRetouchRestriction();
	ClearAttackRunRelease();

	ReleaseAllAIBallPossessions();
	ReleaseAllHumanBallPossessions();

	// StartKickoff ya llama a ResetBallToCenter().
	// Por eso el teletransporte de la pelota ocurre recién ahora,
	// después de GoalResetDelay segundos.
	StartKickoff(PendingKickoffTeam);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			1.5f,
			FColor::Yellow,
			TEXT("Jugadores volviendo a puestos para el saque")
		);
	}
}

void ASoccerMatchManager::ResetBallToCenter()
{
	if (!IsValid(SoccerBall))
	{
		FindSoccerBall();
	}

	if (!IsValid(SoccerBall))
	{
		return;
	}

	const float BallRadius = SoccerBall->GetBallRadiusCm();
	FVector ResetLocation = IsValid(SoccerField)
		? SoccerField->GetPitchCenterWorldLocation(BallRadius)
		: FVector(
			SoccerFieldDimensions::HalfwayLineX,
			SoccerFieldDimensions::CenterY,
			BallRadius
		);

	// Preserve the existing Blueprint tuning as additional vertical clearance.
	ResetLocation.Z += FMath::Max(0.0f, BallResetHeight - BallRadius);

	SoccerBall->SetPossessed(false);

	SoccerBall->SetActorLocation(
		ResetLocation,
		false,
		nullptr,
		ETeleportType::TeleportPhysics
	);

	SoccerBall->StopBallKeepingPhysics();
}

void ASoccerMatchManager::ReleaseAllAIBallPossessions()
{
	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return;
	}

	for (TActorIterator<ASoccerAICharacter> It(World); It; ++It)
	{
		ASoccerAICharacter* SoccerAICharacter = *It;

		if (!IsValid(SoccerAICharacter))
		{
			continue;
		}

		SoccerAICharacter->ReleaseAIBall();
		SoccerAICharacter->SetAIChasingBall(false);
	}
}

void ASoccerMatchManager::ClearAssignedAI()
{
	PlayerTeamPressureAI = nullptr;
	OpponentTeamPressureAI = nullptr;

	PlayerTeamSupportAI = nullptr;
	OpponentTeamSupportAI = nullptr;

	PlayerTeamCoverAI = nullptr;
	OpponentTeamCoverAI = nullptr;

	PlayerTeamDefensiveMarkerAI = nullptr;
	OpponentTeamDefensiveMarkerAI = nullptr;

	PlayerTeamDefensiveMarkedReceiver = nullptr;
	OpponentTeamDefensiveMarkedReceiver = nullptr;

	ClearExplicitIndividualMarkingAssignmentsForTeam(
		ESoccerTeam::PlayerTeam
	);
	ClearExplicitIndividualMarkingAssignmentsForTeam(
		ESoccerTeam::OpponentTeam
	);
}

FVector ASoccerMatchManager::GetOwnGoalReferenceLocation(
	ESoccerTeam Team
) const
{
	const float GoalLineSign = GetOwnGoalLineSign(Team);

	if (IsValid(SoccerField))
	{
		return SoccerField->GetGoalCenterWorldLocation(GoalLineSign);
	}

	return SoccerFieldDimensions::GetGoalCenterLocalLocation(GoalLineSign);
}

FVector ASoccerMatchManager::GetOpponentGoalReferenceLocation(
	ESoccerTeam Team
) const
{
	const float GoalLineSign = GetOpponentGoalLineSign(Team);

	if (IsValid(SoccerField))
	{
		return SoccerField->GetGoalCenterWorldLocation(GoalLineSign);
	}

	return SoccerFieldDimensions::GetGoalCenterLocalLocation(GoalLineSign);
}

ESoccerFieldZone ASoccerMatchManager::GetFieldZoneForTeam(
	const FVector& WorldLocation,
	ESoccerTeam Team
) const
{
	const FVector OwnGoalLocation =
		GetOwnGoalReferenceLocation(Team);

	const FVector OpponentGoalLocation =
		GetOpponentGoalReferenceLocation(Team);

	FVector AttackVector =
		OpponentGoalLocation - OwnGoalLocation;

	AttackVector.Z = 0.0f;

	const float FieldLength = AttackVector.Size();

	if (FieldLength <= 1.0f)
	{
		return ESoccerFieldZone::ZoneC;
	}

	const FVector AttackDirection =
		AttackVector / FieldLength;

	FVector FromOwnGoal =
		WorldLocation - OwnGoalLocation;

	FromOwnGoal.Z = 0.0f;

	const float DistanceAlongField =
		FVector::DotProduct(
			FromOwnGoal,
			AttackDirection
		);

	const float FieldAlpha =
		FMath::Clamp(
			DistanceAlongField / FieldLength,
			0.0f,
			0.999f
		);

	const int32 ZoneIndex =
		FMath::Clamp(
			FMath::FloorToInt(FieldAlpha * 6.0f),
			0,
			5
		);

	switch (ZoneIndex)
	{
	case 0:
		return ESoccerFieldZone::ZoneA;

	case 1:
		return ESoccerFieldZone::ZoneB;

	case 2:
		return ESoccerFieldZone::ZoneC;

	case 3:
		return ESoccerFieldZone::ZoneD;

	case 4:
		return ESoccerFieldZone::ZoneE;

	case 5:
	default:
		return ESoccerFieldZone::ZoneF;
	}
}

ESoccerFieldZone ASoccerMatchManager::GetCharacterFieldZone(
	const ASoccerCharacterBase* Character
) const
{
	if (!IsValid(Character))
	{
		return ESoccerFieldZone::ZoneC;
	}

	return GetFieldZoneForTeam(
		Character->GetActorLocation(),
		Character->GetTeam()
	);
}

ESoccerFieldZone ASoccerMatchManager::GetBallFieldZoneForTeam(
	ESoccerTeam Team
) const
{
	if (!IsValid(SoccerBall))
	{
		return ESoccerFieldZone::ZoneC;
	}

	return GetFieldZoneForTeam(
		SoccerBall->GetActorLocation(),
		Team
	);
}

ESoccerBallSituation ASoccerMatchManager::GetBallSituationForCharacter(
	const ASoccerCharacterBase* Character
) const
{
	if (!IsValid(Character))
	{
		return ESoccerBallSituation::FreeBall;
	}

	if (!IsValid(PossessingCharacter))
	{
		return ESoccerBallSituation::FreeBall;
	}

	if (PossessingCharacter == Character)
	{
		return ESoccerBallSituation::SelfPossession;
	}

	if (PossessingCharacter->GetTeam() == Character->GetTeam())
	{
		return ESoccerBallSituation::OwnTeamPossession;
	}

	return ESoccerBallSituation::OpponentTeamPossession;
}

FVector ASoccerMatchManager::ProjectLocationToNavigation(
	const FVector& DesiredLocation,
	const ASoccerAICharacter* SoccerAICharacter
) const
{
	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return DesiredLocation;
	}

	UNavigationSystemV1* NavigationSystem =
		FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);

	if (NavigationSystem == nullptr)
	{
		return DesiredLocation;
	}

	FNavLocation ProjectedLocation;

	const bool bProjected =
		NavigationSystem->ProjectPointToNavigation(
			DesiredLocation,
			ProjectedLocation,
			FVector(
				NavigationProjectionExtent,
				NavigationProjectionExtent,
				NavigationProjectionExtent
			)
		);

	if (bProjected)
	{
		FVector Result = ProjectedLocation.Location;

		if (SoccerAICharacter != nullptr)
		{
			Result.Z = SoccerAICharacter->GetActorLocation().Z;
		}

		return Result;
	}

	return DesiredLocation;
}

bool ASoccerMatchManager::HasActiveAttack() const
{
	return IsRestartContextActive() || bHasLastTouchTeam;
}

ESoccerTeam ASoccerMatchManager::GetCurrentAttackingTeam() const
{
	return IsRestartContextActive()
		? ActiveRestartTeam
		: LastTouchTeam;
}

ESoccerTeam ASoccerMatchManager::GetCurrentDefendingTeam() const
{
	return GetOppositeTeam(GetCurrentAttackingTeam());
}

ASoccerCharacterBase* ASoccerMatchManager::GetLastTouchCharacter() const
{
	return LastTouchCharacter;
}

bool ASoccerMatchManager::RegisterGoalkeeperReboundTouch(
	ASoccerAICharacter* Goalkeeper
)
{
	if (
		!IsValid(Goalkeeper) ||
		Goalkeeper->GetPlayerRole() != ESoccerPlayerRole::Goalkeeper
		)
	{
		return false;
	}

	if (!TryRegisterIntentionalBallTouch(Goalkeeper))
	{
		return false;
	}

	// The save has already touched and released the ball. Make the logical
	// possession state match the physics immediately instead of waiting for the
	// next periodic possession scan.
	PossessingCharacter = nullptr;
	PossessionTeam = ESoccerPossessionTeam::None;

	ClearAssignedAI();
	MatchStateUpdateAccumulator = 0.0f;

	if (
		MatchPlayState == ESoccerMatchPlayState::Playing &&
		!IsRestartContextActive()
		)
	{
		if (HasActiveAttack())
		{
			AssignAttackDefenseRoles();
		}
		else
		{
			AssignFreeBallRoles();
		}
	}

	return true;
}

bool ASoccerMatchManager::BeginIntentionalLooseBallTouch(
	ASoccerCharacterBase* TouchingCharacter
)
{
	if (!IsValid(TouchingCharacter) || !TryRegisterIntentionalBallTouch(TouchingCharacter))
	{
		return false;
	}

	PossessingCharacter = nullptr;
	PossessionTeam = ESoccerPossessionTeam::None;
	const float CurrentTime = GetWorld() != nullptr
		? GetWorld()->GetTimeSeconds()
		: 0.0f;
	IntentionalLooseBallClaimUnlockTime =
		CurrentTime + FMath::Max(0.0f, IntentionalLooseBallClaimDelay);

	ClearFreeBallChaserMemory();
	ClearAssignedAI();
	MatchStateUpdateAccumulator = 0.0f;

	if (
		MatchPlayState == ESoccerMatchPlayState::Playing &&
		!IsRestartContextActive()
		)
	{
		if (HasActiveAttack())
		{
			AssignAttackDefenseRoles();
		}
		else
		{
			AssignFreeBallRoles();
		}
	}

	return true;
}

void ASoccerMatchManager::ReleaseControlledBallPossession(
	ASoccerCharacterBase* ReleasingCharacter
)
{
	if (
		!IsValid(ReleasingCharacter) ||
		PossessingCharacter != ReleasingCharacter
		)
	{
		return;
	}

	PossessingCharacter = nullptr;
	PossessionTeam = ESoccerPossessionTeam::None;

	ClearFreeBallChaserMemory();
	ClearAssignedAI();
	MatchStateUpdateAccumulator = 0.0f;

	if (
		MatchPlayState == ESoccerMatchPlayState::Playing &&
		!IsRestartContextActive()
		)
	{
		AssignFreeBallRoles();
	}
}

bool ASoccerMatchManager::CanCharacterClaimLooseBallNow(
	const ASoccerCharacterBase* Character
) const
{
	if (!IsValid(Character))
	{
		return false;
	}

	const float CurrentTime = GetWorld() != nullptr
		? GetWorld()->GetTimeSeconds()
		: 0.0f;
	return CurrentTime >= IntentionalLooseBallClaimUnlockTime;
}

void ASoccerMatchManager::RegisterIntentionalBallTouch(
	ASoccerCharacterBase* TouchingCharacter
)
{
	TryRegisterIntentionalBallTouch(TouchingCharacter);
}

bool ASoccerMatchManager::TryRegisterIntentionalBallTouch(
	ASoccerCharacterBase* TouchingCharacter
)
{
	if (!IsValid(TouchingCharacter))
	{
		return false;
	}

	if (IsBallOutOfPlayDelayActive())
	{
		return false;
	}

	const AThirdPersonCppCharacter* TouchingHuman =
		Cast<AThirdPersonCppCharacter>(TouchingCharacter);

	const bool bCompletingHumanThrowInTouch =
		IsValid(TouchingHuman) &&
		CanHumanThrowInTakerExecuteNow(TouchingHuman);

	// Human input must respect the restart phase before a touch is registered.
	// Throw-ins are the one hand-possession exception: ordinary human ball input
	// remains locked, but the execution state may explicitly register the legal
	// release touch once the montage reaches its release frame.
	if (
		IsValid(TouchingHuman) &&
		!CanHumanStartBallActionNow(TouchingHuman) &&
		!bCompletingHumanThrowInTouch
	)
	{
		return false;
	}

	// A penalty first touch must be fully registered before we release the
	// restart and before we install the taker's no-retouch restriction.
	// Previously we enabled the restriction here and then the same first touch
	// immediately failed the generic no-retouch check below. That left the ball
	// untouched while MatchPlayState had already returned to Playing.
	const bool bCompletingPenaltyKickTouch = IsPenaltyKickRestartActive();

	const bool bCompletingHumanFreeKickTouch =
		IsValid(TouchingHuman) &&
		FreeKickRestart.CanHumanTakerExecute(*this, TouchingHuman);

	const bool bCompletingHumanNonFreeKickRestartTouch =
		IsValid(TouchingHuman) &&
		!bCompletingHumanFreeKickTouch &&
		CanHumanFootRestartTakerExecuteNow(TouchingHuman) &&
		(
			ActiveRestartType == ESoccerRestartType::Kickoff ||
			ActiveRestartType == ESoccerRestartType::GoalKick ||
			ActiveRestartType == ESoccerRestartType::CornerKick
		);

	if (
		bCompletingPenaltyKickTouch &&
		!PenaltyKickRestart.CanAcceptFirstTouch(*this, TouchingCharacter)
	)
	{
		return false;
	}

	// Durante un lateral pendiente nadie puede registrar un toque normal.
	// El ejecutor del lateral se registra explícitamente al liberar la pelota.
	if (
		IsThrowInRestartActive() &&
		!bThrowInBallReleased &&
		TouchingCharacter != ThrowInTakerAI &&
		!bCompletingHumanThrowInTouch
		)
	{
		return false;
	}

	// Regla fuerte: ningún otro jugador puede registrar un toque sobre la
	// pelota mientras permanezca asegurada en las manos del arquero.
	if (IsBallProtectedFromCharacter(TouchingCharacter))
	{
		return false;
	}

	if (bNoRetouchRestrictionActive)
	{
		if (!IsValid(NoRetouchRestrictedCharacter))
		{
			ClearNoRetouchRestriction();
		}
		else if (TouchingCharacter == NoRetouchRestrictedCharacter)
		{
			return false;
		}
		else
		{
			ClearNoRetouchRestriction();
		}
	}

	// A persistent pass request ends as soon as the requesting human touches
	// the ball, regardless of whether possession is established afterwards.
	if (
		ActiveHumanPassRequestType != ESoccerHumanPassRequestType::None &&
		TouchingCharacter == ActiveHumanPassRequestingHuman
	)
	{
		ClearActiveHumanPassRequest(
			TEXT("Pedido de pase desactivado: el humano toco la pelota"),
			FLinearColor(0.55f, 0.85f, 1.0f, 1.0f),
			true
		);
	}

	// El primer toque posterior al golpeo termina la reserva del receptor,
	// incluso si ese toque luego deriva en una infraccion de offside.
	ClearOpenPlayPassIntent();

	if (TryHandlePendingOffsideTouch(TouchingCharacter))
	{
		return false;
	}

	bHasLastTouchTeam = true;
	LastTouchTeam = TouchingCharacter->GetTeam();
	LastTouchCharacter = TouchingCharacter;
	LastTouchLocation = TouchingCharacter->GetActorLocation();
	RefreshPendingOffsideSnapshotForTouch(TouchingCharacter);

	if (bCompletingPenaltyKickTouch)
	{
		PenaltyKickRestart.OnFirstTouchRegistered(*this, TouchingCharacter);
	}

	if (bCompletingHumanFreeKickTouch)
	{
		const ESoccerRestartType CompletedRestartType =
			FreeKickRestart.IsSupportedType(ActiveRestartType)
			? ActiveRestartType
			: ESoccerRestartType::DirectFreeKick;

		DestroyActiveRestartHumanRestrictionIndicator();

		// The real first touch is the restart boundary. From this instant the ball
		// is live, the taker receives the normal no-retouch restriction, and the
		// explicit state object is asked to hand control back to Playing.
		MatchPlayState = ESoccerMatchPlayState::Playing;
		EndRestartContext();
		StartNoRetouchRestriction(TouchingCharacter);
		StartAttackRunReleaseForTeam(TouchingCharacter->GetTeam());
		ClearAssignedAI();
		RequestMatchStateTransition(ESoccerMatchStateTransition::Playing);

		ASoccerDebugManager::Message(
			this,
			ESoccerDebugCategory::Restarts,
			CompletedRestartType == ESoccerRestartType::DirectFreeKick
				? TEXT("Tiro libre directo realizado por el humano")
				: TEXT("Tiro libre por offside realizado por el humano"),
			FColor::Green
		);
	}

	if (bCompletingHumanNonFreeKickRestartTouch)
	{
		const ESoccerRestartType CompletedRestartType = ActiveRestartType;

		DestroyActiveRestartHumanRestrictionIndicator();

		// Goal kicks and corners never create an offside offence directly. The
		// generic touch registration above may have prepared a snapshot, so erase
		// it before open play begins.
		if (
			CompletedRestartType == ESoccerRestartType::GoalKick ||
			CompletedRestartType == ESoccerRestartType::CornerKick
		)
		{
			ClearPendingOffsideSnapshot();
		}

		MatchPlayState = ESoccerMatchPlayState::Playing;
		EndRestartContext();

		if (
			CompletedRestartType == ESoccerRestartType::GoalKick ||
			CompletedRestartType == ESoccerRestartType::CornerKick
		)
		{
			GoalLineRestart.ResetRuntime();
		}

		StartNoRetouchRestriction(TouchingCharacter);
		StartAttackRunReleaseForTeam(TouchingCharacter->GetTeam());
		ClearAssignedAI();
		RequestMatchStateTransition(ESoccerMatchStateTransition::Playing);

		const TCHAR* CompletedMessage = TEXT("Reanudacion realizada por el humano");
		if (CompletedRestartType == ESoccerRestartType::Kickoff)
		{
			CompletedMessage = TEXT("Saque del centro realizado por el humano");
		}
		else if (CompletedRestartType == ESoccerRestartType::GoalKick)
		{
			CompletedMessage = TEXT("Saque de arco realizado por el humano");
		}
		else if (CompletedRestartType == ESoccerRestartType::CornerKick)
		{
			CompletedMessage = TEXT("Corner realizado por el humano");
		}

		ASoccerDebugManager::Message(
			this,
			ESoccerDebugCategory::Restarts,
			CompletedMessage,
			FColor::Green
		);
	}

	return true;
}

bool ASoccerMatchManager::CanCharacterTouchBallNow(
	const ASoccerCharacterBase* Character
) const
{
	if (IsBallOutOfPlayDelayActive())
	{
		return false;
	}

	if (
		IsThrowInRestartActive() &&
		!bThrowInBallReleased
		)
	{
		return false;
	}

	if (IsGoalLineRestartActive())
	{
		const AThirdPersonCppCharacter* HumanCharacter =
			Cast<const AThirdPersonCppCharacter>(Character);

		if (
			!IsValid(HumanCharacter) ||
			!CanHumanFootRestartTakerExecuteNow(HumanCharacter)
		)
		{
			return false;
		}
	}

	if (IsBallProtectedFromCharacter(Character))
	{
		return false;
	}

	if (!bNoRetouchRestrictionActive)
	{
		return true;
	}

	if (!IsValid(NoRetouchRestrictedCharacter))
	{
		return true;
	}

	if (!IsValid(Character))
	{
		return false;
	}

	return Character != NoRetouchRestrictedCharacter;
}

bool ASoccerMatchManager::CanHumanStartBallActionNow(
	const AThirdPersonCppCharacter* Character
) const
{
	if (!IsValid(Character))
	{
		return false;
	}

	// Penalty execution already has a dedicated human-taker flow. Preparation
	// stays locked; only the designated human may act once the taking phase has
	// actually begun.
	if (IsPenaltyKickRestartActive())
	{
		return
			IsHumanPenaltyTaker(Character) &&
			MatchPlayState == ESoccerMatchPlayState::PenaltyKickTaking &&
			CanCharacterTouchBallNow(Character);
	}

	// Stationary foot restarts use the same strong input lock during Configuration
	// and Preparation. Once Execution starts, only the human who currently owns
	// that restart receives a temporary exception.
	if (CanHumanFootRestartTakerExecuteNow(Character))
	{
		return CanCharacterTouchBallNow(Character);
	}

	// A human set-piece impact ends the restart context immediately, while the
	// explicit state transition to Playing is applied on the manager tick. Do not
	// clear the just-finished kick animation in that one-frame handoff.
	if (
		PendingMatchStateTransition == ESoccerMatchStateTransition::Playing &&
		MatchPlayState == ESoccerMatchPlayState::Playing &&
		!IsRestartContextActive()
	)
	{
		return CanCharacterTouchBallNow(Character);
	}

	// Configuration states can exist for one tick before their Preparation
	// helper creates the restart context. They must already be locked, otherwise
	// a click in that narrow window can arm a chase/kick against the restart ball.
	if (
		ActiveMatchState &&
		ActiveMatchState->GetStateId() != ESoccerMatchStateId::Playing
		)
	{
		return false;
	}

	// All other direct human ball actions require genuine open play. Checking
	// both bridges is intentional: some restart helpers set MatchPlayState to
	// Playing immediately before their AI taker registers the first touch, while
	// the restart context remains active until that touch is completed.
	if (
		MatchPlayState != ESoccerMatchPlayState::Playing ||
		IsRestartContextActive()
		)
	{
		return false;
	}

	return CanCharacterTouchBallNow(Character);
}

void ASoccerMatchManager::StartNoRetouchRestriction(
	ASoccerCharacterBase* RestrictedCharacter
)
{
	if (!IsValid(RestrictedCharacter))
	{
		ClearNoRetouchRestriction();
		return;
	}

	bNoRetouchRestrictionActive = true;
	NoRetouchRestrictedCharacter = RestrictedCharacter;
}

void ASoccerMatchManager::ClearNoRetouchRestriction()
{
	bNoRetouchRestrictionActive = false;
	NoRetouchRestrictedCharacter = nullptr;
}

ESoccerTeam ASoccerMatchManager::GetOppositeTeam(ESoccerTeam Team) const
{
	return
		Team == ESoccerTeam::PlayerTeam
		? ESoccerTeam::OpponentTeam
		: ESoccerTeam::PlayerTeam;
}

float ASoccerMatchManager::GetFieldLateralOffsetForTeam(
	const FVector& WorldLocation,
	ESoccerTeam Team
) const
{
	const FVector OwnGoalLocation =
		GetOwnGoalReferenceLocation(Team);

	const FVector RightDirection =
		GetFieldRightDirectionForTeam(Team);

	if (RightDirection.IsNearlyZero())
	{
		return 0.0f;
	}

	return FVector::DotProduct(
		WorldLocation - OwnGoalLocation,
		RightDirection
	);
}

FVector ASoccerMatchManager::GetFieldAttackDirectionForTeam(
	ESoccerTeam Team
) const
{
	const FVector OwnGoalLocation =
		GetOwnGoalReferenceLocation(Team);

	const FVector OpponentGoalLocation =
		GetOpponentGoalReferenceLocation(Team);

	FVector AttackDirection =
		OpponentGoalLocation - OwnGoalLocation;

	AttackDirection.Z = 0.0f;

	return AttackDirection.GetSafeNormal();
}

FVector ASoccerMatchManager::GetFieldRightDirectionForTeam(
	ESoccerTeam Team
) const
{
	const FVector AttackDirection =
		GetFieldAttackDirectionForTeam(Team);

	if (AttackDirection.IsNearlyZero())
	{
		return FVector::RightVector;
	}

	FVector RightDirection =
		FVector::CrossProduct(
			FVector::UpVector,
			AttackDirection
		);

	RightDirection.Z = 0.0f;

	return RightDirection.GetSafeNormal();
}

void ASoccerMatchManager::DrawFormationDebug() const
{
	if (!ASoccerDebugManager::IsWorldDrawingEnabled(
		this,
		ESoccerDebugCategory::Formation
	))
	{
		return;
	}

	DrawFormationDebugForTeam(ESoccerTeam::PlayerTeam);
	DrawFormationDebugForTeam(ESoccerTeam::OpponentTeam);
}

void ASoccerMatchManager::DrawIndividualMarkingDebug() const
{
	if (!ASoccerDebugManager::IsWorldDrawingEnabled(
		this,
		ESoccerDebugCategory::IndividualMarking
	))
	{
		return;
	}

	DrawIndividualMarkingDebugForTeam(ESoccerTeam::PlayerTeam);
	DrawIndividualMarkingDebugForTeam(ESoccerTeam::OpponentTeam);
}

void ASoccerMatchManager::DrawIndividualMarkingDebugForTeam(
	ESoccerTeam Team
) const
{
	const TMap<ASoccerAICharacter*, ASoccerCharacterBase*>& ExplicitMarks =
		GetExplicitIndividualMarkingAssignmentsForTeamInternal(Team);

	for (
		const TPair<ASoccerAICharacter*, ASoccerCharacterBase*>& ExplicitMark :
		ExplicitMarks
		)
	{
		if (!IsValid(ExplicitMark.Key) || !IsValid(ExplicitMark.Value))
		{
			continue;
		}

		FVector MarkerLocation = ExplicitMark.Key->GetActorLocation();
		FVector TargetLocation = ExplicitMark.Value->GetActorLocation();
		MarkerLocation.Z += FormationDebugHeight + 25.0f;
		TargetLocation.Z += FormationDebugHeight + 25.0f;

		ASoccerDebugManager::DrawLine(
			this,
			ESoccerDebugCategory::IndividualMarking,
			MarkerLocation,
			TargetLocation,
			FColor::Magenta,
			0.05f,
			2.5f
		);
	}
}

void ASoccerMatchManager::DrawFormationDebugForTeam(
	ESoccerTeam Team
) const
{
	const ESoccerFormationSystem FormationSystem =
		GetFormationSystemForTeam(Team);

	const FSoccerFormationDefinition& Definition =
		SoccerFormationLibrary::GetDefinition(FormationSystem);

	if (!SoccerFormationLibrary::IsValidSevenASideDefinition(Definition))
	{
		return;
	}

	const bool bPlayerTeam = Team == ESoccerTeam::PlayerTeam;
	const FColor TeamColor =
		bPlayerTeam ? FColor::Cyan : FColor(255, 165, 0);
	const FString TeamPrefix = bPlayerTeam ? TEXT("P") : TEXT("O");

	for (const FSoccerFormationSlot& Slot : Definition.Slots)
	{
		FVector SlotLocation = GetFormationSlotWorldLocation(Team, Slot);

		if (SlotLocation.IsNearlyZero())
		{
			continue;
		}

		SlotLocation.Z += FormationDebugHeight;

		ASoccerDebugManager::DrawSphere(
			this,
			ESoccerDebugCategory::Formation,
			SlotLocation,
			FormationDebugSphereRadius,
			TeamColor,
			0.05f,
			12,
			2.0f
		);

		ASoccerCharacterBase* AssignedCharacter =
			GetFormationSlotAssignedCharacter(Team, Slot.SlotId);

		FString AssignedName = TEXT("unassigned");
		if (IsValid(AssignedCharacter))
		{
			AssignedName = AssignedCharacter->GetName();

			FVector CharacterLocation = AssignedCharacter->GetActorLocation();
			CharacterLocation.Z = SlotLocation.Z;

			ASoccerDebugManager::DrawLine(
				this,
				ESoccerDebugCategory::Formation,
				CharacterLocation,
				SlotLocation,
				TeamColor,
				0.05f,
				1.5f
			);
		}

		ASoccerDebugManager::DrawString(
			this,
			ESoccerDebugCategory::Formation,
			SlotLocation + FVector(0.0f, 0.0f, FormationDebugSphereRadius + 18.0f),
			FString::Printf(
				TEXT("%s %s <- %s"),
				*TeamPrefix,
				*Slot.SlotId.ToString(),
				*AssignedName
			),
			TeamColor,
			0.05f
		);
	}

	const TMap<ASoccerAICharacter*, ASoccerCharacterBase*>& ExplicitMarks =
		GetExplicitIndividualMarkingAssignmentsForTeamInternal(Team);

	ASoccerDebugManager::SetPersistentLine(
		this,
		ESoccerDebugCategory::Formation,
		bPlayerTeam ? 0 : 1,
		FString::Printf(
			TEXT("%s formation: %s | assigned %d/%d | explicit marks %d | open-play structure %s"),
			bPlayerTeam ? TEXT("PlayerTeam") : TEXT("OpponentTeam"),
			*Definition.DisplayName.ToString(),
			GetFormationAssignmentsForTeamInternal(Team).Num(),
			Definition.Slots.Num(),
			ExplicitMarks.Num(),
			bUseFormationForOpenPlayStructure ? TEXT("ON") : TEXT("OFF")
		),
		TeamColor
	);
}

bool ASoccerMatchManager::AreTeamTacticalPlansEqual(
	const FSoccerTeamTacticalPlan& A,
	const FSoccerTeamTacticalPlan& B
) const
{
	return
		A.BuildUpStyle == B.BuildUpStyle &&
		A.AttackChannel == B.AttackChannel &&
		A.AttackingWidth == B.AttackingWidth &&
		A.AttackingTempo == B.AttackingTempo &&
		A.AttackingTransition == B.AttackingTransition &&
		A.DefensiveBlock == B.DefensiveBlock &&
		A.PressingIntensity == B.PressingIntensity &&
		A.MarkingStyle == B.MarkingStyle &&
		A.DefensiveTransition == B.DefensiveTransition;
}

void ASoccerMatchManager::BeginLiveTacticalShapeTransitionForTeam(
	ESoccerTeam Team
)
{
	if (
		!bUseLiveTacticalShapeTransition ||
		MatchPlayState != ESoccerMatchPlayState::Playing ||
		IsRestartContextActive()
		)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	TMap<const ASoccerAICharacter*, FVector>& StartLocations =
		Team == ESoccerTeam::PlayerTeam
		? PlayerTeamLiveTacticalShapeStartLocations
		: OpponentTeamLiveTacticalShapeStartLocations;

	// Capture into a temporary map first. If a transition is already running,
	// BuildDynamicTeamShapeLocation needs the existing map to evaluate the
	// currently visible intermediate target. Resetting it before the capture
	// would make a rapid second change snap to the first change's endpoint.
	TMap<const ASoccerAICharacter*, FVector> NewStartLocations;

	for (TActorIterator<ASoccerAICharacter> It(World); It; ++It)
	{
		ASoccerAICharacter* Candidate = *It;

		if (
			!IsValid(Candidate) ||
			Candidate->GetTeam() != Team ||
			Candidate->GetPlayerRole() == ESoccerPlayerRole::Goalkeeper
			)
		{
			continue;
		}

		// BuildDynamicTeamShapeLocation intentionally includes an already-running
		// transition. If the user changes strategy again mid-transition, the new
		// transition therefore starts from the currently visible/effective shape
		// instead of snapping back to either endpoint.
		const FVector CurrentEffectiveShapeLocation =
			BuildDynamicTeamShapeLocation(Team, Candidate);

		if (!CurrentEffectiveShapeLocation.IsNearlyZero())
		{
			NewStartLocations.Add(Candidate, CurrentEffectiveShapeLocation);
		}
	}

	StartLocations = MoveTemp(NewStartLocations);

	const bool bHasSnapshot = StartLocations.Num() > 0;
	const float CurrentTime = World->GetTimeSeconds();

	if (Team == ESoccerTeam::PlayerTeam)
	{
		bPlayerTeamLiveTacticalShapeTransitionActive = bHasSnapshot;
		PlayerTeamLiveTacticalShapeTransitionStartTime = CurrentTime;
	}
	else
	{
		bOpponentTeamLiveTacticalShapeTransitionActive = bHasSnapshot;
		OpponentTeamLiveTacticalShapeTransitionStartTime = CurrentTime;
	}
}

void ASoccerMatchManager::ClearLiveTacticalShapeTransitionForTeam(
	ESoccerTeam Team
)
{
	if (Team == ESoccerTeam::PlayerTeam)
	{
		bPlayerTeamLiveTacticalShapeTransitionActive = false;
		PlayerTeamLiveTacticalShapeTransitionStartTime = -1000.0f;
		PlayerTeamLiveTacticalShapeStartLocations.Reset();
	}
	else
	{
		bOpponentTeamLiveTacticalShapeTransitionActive = false;
		OpponentTeamLiveTacticalShapeTransitionStartTime = -1000.0f;
		OpponentTeamLiveTacticalShapeStartLocations.Reset();
	}
}

float ASoccerMatchManager::GetLiveTacticalShapeTransitionAlpha(
	ESoccerTeam Team
) const
{
	const bool bTransitionActive =
		Team == ESoccerTeam::PlayerTeam
		? bPlayerTeamLiveTacticalShapeTransitionActive
		: bOpponentTeamLiveTacticalShapeTransitionActive;

	if (!bTransitionActive || !bUseLiveTacticalShapeTransition)
	{
		return 1.0f;
	}

	const UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return 1.0f;
	}

	const float StartTime =
		Team == ESoccerTeam::PlayerTeam
		? PlayerTeamLiveTacticalShapeTransitionStartTime
		: OpponentTeamLiveTacticalShapeTransitionStartTime;

	const float SafeDuration =
		FMath::Max(0.10f, LiveTacticalShapeTransitionDuration);

	const float RawAlpha = FMath::Clamp(
		(World->GetTimeSeconds() - StartTime) / SafeDuration,
		0.0f,
		1.0f
	);

	// SmoothStep avoids both a sudden initial acceleration and an abrupt stop
	// when the structural target reaches the new tactic.
	return RawAlpha * RawAlpha * (3.0f - 2.0f * RawAlpha);
}

FVector ASoccerMatchManager::ApplyLiveTacticalShapeTransitionToLocation(
	ESoccerTeam Team,
	const ASoccerAICharacter* SoccerAICharacter,
	const FVector& DesiredLocation
) const
{
	if (
		!bUseLiveTacticalShapeTransition ||
		!IsValid(SoccerAICharacter) ||
		SoccerAICharacter->GetPlayerRole() == ESoccerPlayerRole::Goalkeeper ||
		DesiredLocation.IsNearlyZero()
		)
	{
		return DesiredLocation;
	}

	const bool bTransitionActive =
		Team == ESoccerTeam::PlayerTeam
		? bPlayerTeamLiveTacticalShapeTransitionActive
		: bOpponentTeamLiveTacticalShapeTransitionActive;

	if (!bTransitionActive)
	{
		return DesiredLocation;
	}

	const TMap<const ASoccerAICharacter*, FVector>& StartLocations =
		Team == ESoccerTeam::PlayerTeam
		? PlayerTeamLiveTacticalShapeStartLocations
		: OpponentTeamLiveTacticalShapeStartLocations;

	const FVector* StartLocation = StartLocations.Find(SoccerAICharacter);

	if (StartLocation == nullptr)
	{
		return DesiredLocation;
	}

	const float TransitionAlpha =
		GetLiveTacticalShapeTransitionAlpha(Team);

	FVector BlendedLocation = FMath::Lerp(
		*StartLocation,
		DesiredLocation,
		TransitionAlpha
	);

	BlendedLocation.Z = SoccerAICharacter->GetActorLocation().Z;

	return ProjectLocationToNavigation(
		BlendedLocation,
		SoccerAICharacter
	);
}

void ASoccerMatchManager::UpdateLiveTacticalShapeTransitions()
{
	if (!bUseLiveTacticalShapeTransition)
	{
		ClearLiveTacticalShapeTransitionForTeam(ESoccerTeam::PlayerTeam);
		ClearLiveTacticalShapeTransitionForTeam(ESoccerTeam::OpponentTeam);
		return;
	}

	const UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	const float SafeDuration =
		FMath::Max(0.10f, LiveTacticalShapeTransitionDuration);
	const float CurrentTime = World->GetTimeSeconds();

	if (
		bPlayerTeamLiveTacticalShapeTransitionActive &&
		CurrentTime - PlayerTeamLiveTacticalShapeTransitionStartTime >= SafeDuration
		)
	{
		ClearLiveTacticalShapeTransitionForTeam(ESoccerTeam::PlayerTeam);
	}

	if (
		bOpponentTeamLiveTacticalShapeTransitionActive &&
		CurrentTime - OpponentTeamLiveTacticalShapeTransitionStartTime >= SafeDuration
		)
	{
		ClearLiveTacticalShapeTransitionForTeam(ESoccerTeam::OpponentTeam);
	}
}

FVector ASoccerMatchManager::BuildDynamicTeamShapeLocation(
	ESoccerTeam Team,
	const ASoccerAICharacter* SoccerAICharacter
) const
{
	const FVector DesiredLocation =
		BuildDynamicTeamShapeLocationRaw(Team, SoccerAICharacter);

	return ApplyLiveTacticalShapeTransitionToLocation(
		Team,
		SoccerAICharacter,
		DesiredLocation
	);
}

FVector ASoccerMatchManager::BuildDynamicTeamShapeLocationRaw(
	ESoccerTeam Team,
	const ASoccerAICharacter* SoccerAICharacter
) const
{
	if (SoccerAICharacter == nullptr)
	{
		return FVector::ZeroVector;
	}

	AActor* HomePositionActor =
		SoccerAICharacter->GetHomePositionActor();

	const FVector LegacyHomeLocation =
		HomePositionActor != nullptr
		? GetTeamRebasedFieldReferenceLocation(
			Team,
			HomePositionActor->GetActorLocation()
		)
		: SoccerAICharacter->GetActorLocation();

	FSoccerFormationSlot FormationSlot;
	FVector FormationSlotLocation = FVector::ZeroVector;

	const bool bHasFormationStructure =
		TryGetFormationStructuralReference(
			SoccerAICharacter,
			FormationSlot,
			FormationSlotLocation
		);

	const FVector StructuralHomeLocation =
		bHasFormationStructure
		? FormationSlotLocation
		: LegacyHomeLocation;

	const FVector OwnGoalLocation =
		GetOwnGoalReferenceLocation(Team);

	const FVector OpponentGoalLocation =
		GetOpponentGoalReferenceLocation(Team);

	FVector AttackDirection =
		OpponentGoalLocation - OwnGoalLocation;

	AttackDirection.Z = 0.0f;

	const float FieldLength =
		AttackDirection.Size();

	if (FieldLength <= 1.0f)
	{
		return StructuralHomeLocation;
	}

	AttackDirection =
		AttackDirection / FieldLength;

	FVector RightDirection =
		FVector::CrossProduct(
			FVector::UpVector,
			AttackDirection
		);

	RightDirection.Z = 0.0f;
	RightDirection = RightDirection.GetSafeNormal();

	if (RightDirection.IsNearlyZero())
	{
		RightDirection = FVector::RightVector;
	}

	const FVector BallLocation =
		IsValid(SoccerBall)
		? SoccerBall->GetActorLocation()
		: StructuralHomeLocation;

	const float BallDepth =
		FVector::DotProduct(
			BallLocation - OwnGoalLocation,
			AttackDirection
		);

	const float BallAlpha =
		FMath::Clamp(
			BallDepth / FieldLength,
			0.0f,
			1.0f
		);

	const float StructuralDepth =
		FVector::DotProduct(
			StructuralHomeLocation - OwnGoalLocation,
			AttackDirection
		);

	const float StructuralDepthAlpha =
		FMath::Clamp(
			StructuralDepth / FieldLength,
			0.0f,
			1.0f
		);

	const ESoccerTeamPhase TeamPhase =
		GetTeamPhase(Team);

	float DesiredDepthAlpha =
		GetDynamicShapeBaseDepthAlpha(
			Team,
			SoccerAICharacter
		);

	if (bHasFormationStructure)
	{
		// Preserve the slot's own depth separation while reusing the old phase
		// compression/advance amounts. This lets DM/AM or five-defender shapes
		// remain distinct without changing the player's legacy PlayerRole.
		const float NeutralRoleDepthAlpha =
			GetFormationNeutralDepthAlphaForRole(FormationSlot.PlayerRole);

		const float PhaseRoleDepthAlpha =
			GetDynamicShapeBaseDepthAlphaForRole(
				TeamPhase,
				FormationSlot.PlayerRole
			);

		DesiredDepthAlpha =
			FormationSlot.DepthAlpha +
			(PhaseRoleDepthAlpha - NeutralRoleDepthAlpha);
	}

	if (TeamPhase == ESoccerTeamPhase::Defending)
	{
		// Formation owns each player's relative slot depth; collective tactics move
		// the whole defensive block higher/lower without destroying that spacing.
		DesiredDepthAlpha +=
			GetCollectiveDefensiveBlockDepthOffsetAlpha(Team);

		DesiredDepthAlpha +=
			BallAlpha *
			DynamicShapeDefensiveBallDepthInfluence;
	}
	else if (TeamPhase == ESoccerTeamPhase::Attacking)
	{
		DesiredDepthAlpha +=
			(BallAlpha - 0.5f) *
			DynamicShapeAttackingBallDepthInfluence;
	}
	else
	{
		const FVector NeutralLocation =
			FMath::Lerp(
				StructuralHomeLocation,
				BallLocation,
				DynamicShapeNeutralHomeToBallAlpha
			);

		FVector Result = NeutralLocation;
		Result.Z = SoccerAICharacter->GetActorLocation().Z;

		return ProjectLocationToNavigation(
			Result,
			SoccerAICharacter
		);
	}

	DesiredDepthAlpha =
		FMath::Clamp(
			DesiredDepthAlpha,
			0.08f,
			0.90f
		);

	const float FinalDepthAlpha =
		FMath::Lerp(
			StructuralDepthAlpha,
			DesiredDepthAlpha,
			DynamicShapeDepthBlend
		);

	const float FinalDepth =
		FinalDepthAlpha * FieldLength;

	const float StructuralLateralOffset =
		FVector::DotProduct(
			StructuralHomeLocation - OwnGoalLocation,
			RightDirection
		);

	const float BallLateralOffset =
		FVector::DotProduct(
			BallLocation - OwnGoalLocation,
			RightDirection
		);

	float LateralShift =
		BallLateralOffset *
		DynamicShapeBallSideShiftAlpha;

	const float ScaledDynamicShapeMaxLateralShift =
		SoccerFieldDimensions::ScaleAuthoredLateralDistance(
			DynamicShapeMaxLateralShift
		);

	LateralShift =
		FMath::Clamp(
			LateralShift,
			-ScaledDynamicShapeMaxLateralShift,
			ScaledDynamicShapeMaxLateralShift
		);

	const float StructuralLateralKeepAlpha =
		bHasFormationStructure
		? FormationDynamicShapeLateralKeepAlpha
		: DynamicShapeHomeLateralKeepAlpha;

	float TacticalStructuralLateralOffset = StructuralLateralOffset;
	float TacticalChannelShift = 0.0f;

	if (
		TeamPhase == ESoccerTeamPhase::Attacking &&
		SoccerAICharacter->GetPlayerRole() != ESoccerPlayerRole::Goalkeeper &&
		ShouldApplyCollectiveTacticsToOpenPlay(Team)
		)
	{
		const FSoccerTeamTacticalPlan& TacticalPlan =
			GetTacticalPlanForTeamInternal(Team);

		TacticalStructuralLateralOffset *=
			GetCollectiveAttackWidthScale(Team);

		if (TacticalPlan.AttackChannel == ESoccerAttackChannel::Center)
		{
			TacticalStructuralLateralOffset *= 0.72f;
			LateralShift *= 0.82f;
		}
		else
		{
			// The structural block shifts less than an active support/run order.
			TacticalChannelShift =
				GetCollectiveAttackChannelLateralShift(Team) * 0.55f;
		}
	}

	float FinalLateralOffset =
		TacticalStructuralLateralOffset * StructuralLateralKeepAlpha
		+ LateralShift
		+ TacticalChannelShift;

	if (bHasFormationStructure)
	{
		const float MaximumFormationLateralOffset =
			FMath::Max(
				0.0f,
				SoccerFieldDimensions::HalfPitchWidthCm - 120.0f
			);

		FinalLateralOffset =
			FMath::Clamp(
				FinalLateralOffset,
				-MaximumFormationLateralOffset,
				MaximumFormationLateralOffset
			);
	}

	FVector ShapeLocation =
		OwnGoalLocation
		+ AttackDirection * FinalDepth
		+ RightDirection * FinalLateralOffset;

	ShapeLocation.Z =
		SoccerAICharacter->GetActorLocation().Z;

	return ProjectLocationToNavigation(
		ShapeLocation,
		SoccerAICharacter
	);
}

float ASoccerMatchManager::GetDynamicShapeBaseDepthAlpha(
	ESoccerTeam Team,
	const ASoccerAICharacter* SoccerAICharacter
) const
{
	if (SoccerAICharacter == nullptr)
	{
		return 0.5f;
	}

	return GetDynamicShapeBaseDepthAlphaForRole(
		GetTeamPhase(Team),
		SoccerAICharacter->GetPlayerRole()
	);
}

float ASoccerMatchManager::GetDynamicShapeBaseDepthAlphaForRole(
	ESoccerTeamPhase TeamPhase,
	ESoccerPlayerRole PlayerRole
) const
{
	if (TeamPhase == ESoccerTeamPhase::Defending)
	{
		switch (PlayerRole)
		{
		case ESoccerPlayerRole::Defender:
			return DynamicShapeDefendingDefenderDepthAlpha;

		case ESoccerPlayerRole::Midfielder:
			return DynamicShapeDefendingMidfielderDepthAlpha;

		case ESoccerPlayerRole::Forward:
			return DynamicShapeDefendingForwardDepthAlpha;

		default:
			return 0.35f;
		}
	}

	if (TeamPhase == ESoccerTeamPhase::Attacking)
	{
		switch (PlayerRole)
		{
		case ESoccerPlayerRole::Defender:
			return DynamicShapeAttackingDefenderDepthAlpha;

		case ESoccerPlayerRole::Midfielder:
			return DynamicShapeAttackingMidfielderDepthAlpha;

		case ESoccerPlayerRole::Forward:
			return DynamicShapeAttackingForwardDepthAlpha;

		default:
			return 0.55f;
		}
	}

	return GetFormationNeutralDepthAlphaForRole(PlayerRole);
}

float ASoccerMatchManager::GetFormationNeutralDepthAlphaForRole(
	ESoccerPlayerRole PlayerRole
) const
{
	// These are neutral structural baselines, not tactical instructions. They
	// approximate the common seven-a-side slot depths and are used only to
	// preserve the old attack/defend phase displacement around each formation
	// slot's own depth.
	switch (PlayerRole)
	{
	case ESoccerPlayerRole::Goalkeeper:
		return 0.055f;

	case ESoccerPlayerRole::Defender:
		return 0.25f;

	case ESoccerPlayerRole::Midfielder:
		return 0.49f;

	case ESoccerPlayerRole::Forward:
		return 0.71f;

	default:
		return 0.50f;
	}
}

void ASoccerMatchManager::StartKickoff(ESoccerTeam TeamTakingKickoff)
{
	if (!ActivateMatchState(
		MakeUnique<FSoccerKickoffConfigurationState>(TeamTakingKickoff)
	))
	{
		EndRestartContext();
		MatchPlayState = ESoccerMatchPlayState::Playing;
		ActivateMatchState(MakeUnique<FSoccerPlayingState>());
		StartCurrentHalfClockIfNeeded();
	}
}

FVector ASoccerMatchManager::GetKickoffCenterLocation() const
{
	const float LocalZ = IsValid(SoccerBall)
		? SoccerBall->GetBallRadiusCm()
		: 0.0f;

	if (IsValid(SoccerField))
	{
		return SoccerField->GetPitchCenterWorldLocation(LocalZ);
	}

	return FVector(
		SoccerFieldDimensions::HalfwayLineX,
		SoccerFieldDimensions::CenterY,
		LocalZ
	);
}

bool ASoccerMatchManager::IsCharacterInOwnHalfForKickoff(
	const ASoccerCharacterBase* Character
) const
{
	if (!IsValid(Character))
	{
		return true;
	}

	FVector OwnGoalLocation = FVector::ZeroVector;
	FVector AttackDirection = FVector::ZeroVector;
	float FieldLength = 0.0f;

	if (
		!TryGetAttackFieldFrame(
			Character->GetTeam(),
			OwnGoalLocation,
			AttackDirection,
			FieldLength
		)
		)
	{
		return true;
	}

	const float CharacterProgress =
		FVector::DotProduct(
			Character->GetActorLocation() - OwnGoalLocation,
			AttackDirection
		);

	const float MidfieldProgress =
		FieldLength * 0.5f;

	return CharacterProgress <= MidfieldProgress + KickoffOwnHalfTolerance;
}

bool ASoccerMatchManager::IsCharacterInsideKickoffCenterCircle(
	const ASoccerCharacterBase* Character
) const
{
	if (!IsValid(Character))
	{
		return false;
	}

	const FVector CenterLocation =
		GetKickoffCenterLocation();

	const float DistanceToCenter =
		FVector::Dist2D(
			Character->GetActorLocation(),
			CenterLocation
		);

	return DistanceToCenter <
		SoccerFieldDimensions::CenterCircleRadiusCm +
		KickoffCenterCircleExtraDistance;
}

bool ASoccerMatchManager::IsCharacterInLegalKickoffPosition(
	const ASoccerCharacterBase* Character
) const
{
	if (!IsValid(Character))
	{
		return true;
	}

	// Only the currently selected taker may enter the center circle and approach
	// the ball. If the human claims the kickoff, the fallback AI loses this
	// exception and must return to a normal legal kickoff position.
	if (
		IsValid(KickoffTakerAI) &&
		Character == KickoffTakerAI &&
		!IsNonFreeKickHumanTakerClaimedFor(ESoccerRestartType::Kickoff)
		)
	{
		return true;
	}

	if (
		IsNonFreeKickHumanTakerClaimedFor(ESoccerRestartType::Kickoff) &&
		Character == ActiveNonFreeKickHumanTaker
	)
	{
		return true;
	}

	if (!IsCharacterInOwnHalfForKickoff(Character))
	{
		return false;
	}

	if (IsCharacterInsideKickoffCenterCircle(Character))
	{
		return false;
	}

	return true;
}

bool ASoccerMatchManager::AreKickoffPlayersInLegalPositions() const
{
	if (!bKickoffRequiresLegalPlayerPositions)
	{
		return true;
	}

	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return true;
	}

	for (TActorIterator<ASoccerCharacterBase> It(World); It; ++It)
	{
		const ASoccerCharacterBase* Candidate = *It;

		if (!IsValid(Candidate))
		{
			continue;
		}

		if (!IsCharacterInLegalKickoffPosition(Candidate))
		{
			return false;
		}
	}

	return true;
}

ASoccerAICharacter* ASoccerMatchManager::FindKickoffTaker(
	ESoccerTeam TeamTakingKickoff
) const
{
	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return nullptr;
	}

	const FVector CenterLocation = GetKickoffCenterLocation();

	ASoccerAICharacter* BestCharacter = nullptr;
	float BestScore = -TNumericLimits<float>::Max();

	for (TActorIterator<ASoccerAICharacter> It(World); It; ++It)
	{
		ASoccerAICharacter* Candidate = *It;

		if (!IsValid(Candidate))
		{
			continue;
		}

		if (Candidate->GetTeam() != TeamTakingKickoff)
		{
			continue;
		}

		if (Candidate->GetPlayerRole() == ESoccerPlayerRole::Goalkeeper)
		{
			continue;
		}

		const float DistanceToCenter =
			FVector::Dist2D(
				Candidate->GetActorLocation(),
				CenterLocation
			);

		float Score =
			-DistanceToCenter;

		if (Candidate->GetPlayerRole() == ESoccerPlayerRole::Forward)
		{
			Score += 10000.0f;
		}
		else if (Candidate->GetPlayerRole() == ESoccerPlayerRole::Midfielder)
		{
			Score += 1000.0f;
		}

		if (Score > BestScore)
		{
			BestScore = Score;
			BestCharacter = Candidate;
		}
	}

	return BestCharacter;
}

ASoccerAICharacter* ASoccerMatchManager::FindKickoffReceiver(
	ESoccerTeam TeamTakingKickoff,
	const ASoccerAICharacter* KickoffTaker
) const
{
	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return nullptr;
	}

	const FVector CenterLocation = GetKickoffCenterLocation();

	ASoccerAICharacter* BestCharacter = nullptr;
	float BestScore = -TNumericLimits<float>::Max();

	for (TActorIterator<ASoccerAICharacter> It(World); It; ++It)
	{
		ASoccerAICharacter* Candidate = *It;

		if (!IsValid(Candidate))
		{
			continue;
		}

		if (Candidate == KickoffTaker)
		{
			continue;
		}

		if (Candidate->GetTeam() != TeamTakingKickoff)
		{
			continue;
		}

		if (Candidate->GetPlayerRole() == ESoccerPlayerRole::Goalkeeper)
		{
			continue;
		}

		const float DistanceFromCenter =
			FVector::Dist2D(
				Candidate->GetActorLocation(),
				CenterLocation
			);

		if (DistanceFromCenter < KickoffReceiverMinDistanceFromCenter)
		{
			continue;
		}

		float Score =
			-DistanceFromCenter;

		if (Candidate->GetPlayerRole() == ESoccerPlayerRole::Midfielder)
		{
			Score += 3000.0f;
		}
		else if (Candidate->GetPlayerRole() == ESoccerPlayerRole::Forward)
		{
			Score += 1200.0f;
		}

		if (Score > BestScore)
		{
			BestScore = Score;
			BestCharacter = Candidate;
		}
	}

	return BestCharacter;
}

bool ASoccerMatchManager::BuildRestartKickRunGeometryFromCurrentTaker(
	const ASoccerAICharacter* Taker,
	float RunThroughDistance,
	FVector& OutRunDirection,
	FVector& OutRunThroughLocation
) const
{
	OutRunDirection = FVector::ForwardVector;
	OutRunThroughLocation = FVector::ZeroVector;

	if (!IsValid(Taker) || !IsValid(SoccerBall))
	{
		return false;
	}

	const FVector BallLocation = SoccerBall->GetActorLocation();

	OutRunDirection = BallLocation - Taker->GetActorLocation();
	OutRunDirection.Z = 0.0f;

	if (!OutRunDirection.Normalize())
	{
		OutRunDirection = Taker->GetActorForwardVector();
		OutRunDirection.Z = 0.0f;

		if (!OutRunDirection.Normalize())
		{
			OutRunDirection = FVector::ForwardVector;
		}
	}

	// Se conserva el XY exacto de la recta ejecutor-pelota. Proyectar el
	// destino podia desplazarlo lateralmente y hacer que la carrera pasara
	// al costado del balon.
	OutRunThroughLocation =
		BallLocation +
		OutRunDirection * FMath::Max(50.0f, RunThroughDistance);
	OutRunThroughLocation.Z = Taker->GetActorLocation().Z;

	return !OutRunThroughLocation.IsNearlyZero();
}

void ASoccerMatchManager::InitializeRestartKickContactTracking(
	FRestartKickContactTracker& Tracker,
	const ASoccerAICharacter* Taker
) const
{
	ResetRestartKickContactTracking(Tracker);

	if (IsValid(Taker))
	{
		Tracker.bHasPreviousTakerLocation = true;
		Tracker.PreviousTakerLocation = Taker->GetActorLocation();
	}
}

void ASoccerMatchManager::ResetRestartKickContactTracking(
	FRestartKickContactTracker& Tracker
) const
{
	Tracker.bContactConfirmed = false;
	Tracker.bHasPreviousTakerLocation = false;
	Tracker.PreviousTakerLocation = FVector::ZeroVector;
}

ERestartKickContactResult ASoccerMatchManager::EvaluateRestartKickContact(
	const ASoccerAICharacter* Taker,
	const FVector& RunDirection,
	float MaxBallSurfaceGap,
	float MinimumFacingDot,
	FRestartKickContactTracker& Tracker
) const
{
	if (Tracker.bContactConfirmed)
	{
		return ERestartKickContactResult::Contact;
	}

	if (!IsValid(Taker) || !IsValid(SoccerBall))
	{
		ResetRestartKickContactTracking(Tracker);
		return ERestartKickContactResult::None;
	}

	FVector FlatRunDirection = RunDirection;
	FlatRunDirection.Z = 0.0f;

	if (!FlatRunDirection.Normalize())
	{
		return ERestartKickContactResult::None;
	}

	const FVector CurrentTakerLocation = Taker->GetActorLocation();
	const FVector BallLocation = SoccerBall->GetActorLocation();

	const UCapsuleComponent* CapsuleComponent =
		Taker->GetCapsuleComponent();

	const float CapsuleRadius =
		CapsuleComponent != nullptr
		? CapsuleComponent->GetScaledCapsuleRadius()
		: 42.0f;

	const float BallRadius = SoccerBall->GetBallRadiusCm();
	const float SafeSurfaceGap = FMath::Max(0.0f, MaxBallSurfaceGap);
	const float MaximumContactCenterDistance =
		CapsuleRadius + BallRadius + SafeSurfaceGap;

	const float CenterDistance = FVector::Dist2D(
		CurrentTakerLocation,
		BallLocation
	);

	const float SurfaceGap = FMath::Max(
		0.0f,
		CenterDistance - CapsuleRadius - BallRadius
	);

	FVector ForwardDirection = Taker->GetActorForwardVector();
	ForwardDirection.Z = 0.0f;
	ForwardDirection = ForwardDirection.GetSafeNormal();

	FVector ToBall = BallLocation - CurrentTakerLocation;
	ToBall.Z = 0.0f;
	ToBall = ToBall.GetSafeNormal();

	const bool bCurrentContactIsValid =
		SurfaceGap <= SafeSurfaceGap &&
		!ForwardDirection.IsNearlyZero() &&
		FVector::DotProduct(
			ForwardDirection,
			FlatRunDirection
		) >= MinimumFacingDot &&
		!ToBall.IsNearlyZero() &&
		FVector::DotProduct(ForwardDirection, ToBall) > 0.0f;

	bool bSweptContactIsValid = false;

	FVector CurrentFlat = CurrentTakerLocation;
	FVector BallFlat = BallLocation;
	CurrentFlat.Z = 0.0f;
	BallFlat.Z = 0.0f;

	const float CurrentBallForwardDistance = FVector::DotProduct(
		BallFlat - CurrentFlat,
		FlatRunDirection
	);

	if (Tracker.bHasPreviousTakerLocation)
	{
		FVector PreviousFlat = Tracker.PreviousTakerLocation;
		PreviousFlat.Z = 0.0f;

		const FVector TravelVector = CurrentFlat - PreviousFlat;
		const float TravelDistance = TravelVector.Size2D();

		if (TravelDistance > KINDA_SMALL_NUMBER)
		{
			const FVector TravelDirection = TravelVector / TravelDistance;
			const float MinimumTravelDot = FMath::Clamp(
				MinimumFacingDot * 0.75f,
				0.25f,
				0.95f
			);

			const FVector ClosestPointOnTravel =
				FMath::ClosestPointOnSegment(
					BallFlat,
					PreviousFlat,
					CurrentFlat
				);

			const float ClosestCenterDistance = FVector::Dist2D(
				ClosestPointOnTravel,
				BallFlat
			);

			const float PreviousBallForwardDistance = FVector::DotProduct(
				BallFlat - PreviousFlat,
				FlatRunDirection
			);

			const float SegmentMinimumForwardDistance = FMath::Min(
				PreviousBallForwardDistance,
				CurrentBallForwardDistance
			);
			const float SegmentMaximumForwardDistance = FMath::Max(
				PreviousBallForwardDistance,
				CurrentBallForwardDistance
			);

			const bool bSegmentCrossedContactBand =
				SegmentMaximumForwardDistance >=
					-MaximumContactCenterDistance &&
				SegmentMinimumForwardDistance <=
					MaximumContactCenterDistance;

			bSweptContactIsValid =
				FVector::DotProduct(
					TravelDirection,
					FlatRunDirection
				) >= MinimumTravelDot &&
				bSegmentCrossedContactBand &&
				ClosestCenterDistance <= MaximumContactCenterDistance;
		}
	}

	Tracker.PreviousTakerLocation = CurrentTakerLocation;
	Tracker.bHasPreviousTakerLocation = true;

	if (bCurrentContactIsValid || bSweptContactIsValid)
	{
		Tracker.bContactConfirmed = true;
		return ERestartKickContactResult::Contact;
	}

	// Si el centro del jugador ya quedo mas alla de toda la banda de
	// contacto, la condicion normal no puede recuperarse sin volver atras.
	if (CurrentBallForwardDistance < -MaximumContactCenterDistance)
	{
		return ERestartKickContactResult::MissedBall;
	}

	return ERestartKickContactResult::None;
}

void ASoccerMatchManager::RecalculateKickoffRunUpGeometry()
{
	if (
		!IsValid(SoccerBall) ||
		!IsValid(KickoffTakerAI) ||
		!IsValid(KickoffReceiverAI)
		)
	{
		KickoffKickDirection = FVector::ForwardVector;
		KickoffRunDirection = FVector::ForwardVector;
		KickoffRunUpStartLocation = FVector::ZeroVector;
		KickoffRunThroughLocation = FVector::ZeroVector;
		return;
	}

	const FVector BallLocation = SoccerBall->GetActorLocation();

	KickoffKickDirection =
		KickoffReceiverAI->GetActorLocation() - BallLocation;
	KickoffKickDirection.Z = 0.0f;

	if (!KickoffKickDirection.Normalize())
	{
		KickoffKickDirection =
			GetFieldAttackDirectionForTeam(PendingKickoffTeam);
	}

	KickoffRunDirection = KickoffKickDirection;

	KickoffRunUpStartLocation =
		BallLocation -
		KickoffKickDirection * FMath::Max(50.0f, KickoffRunUpDistance);
	KickoffRunUpStartLocation.Z =
		KickoffTakerAI->GetActorLocation().Z;
	KickoffRunUpStartLocation =
		ProjectLocationToNavigation(
			KickoffRunUpStartLocation,
			KickoffTakerAI
		);

	KickoffRunThroughLocation =
		BallLocation +
		KickoffKickDirection * FMath::Max(50.0f, KickoffRunThroughDistance);
	KickoffRunThroughLocation.Z =
		KickoffTakerAI->GetActorLocation().Z;
	KickoffRunThroughLocation =
		ProjectLocationToNavigation(
			KickoffRunThroughLocation,
			KickoffTakerAI
		);
}

void ASoccerMatchManager::ResetKickoffRunUpState()
{
	bKickoffFinalRunActive = false;
	bKickoffAIKickMontageStarted = false;
	KickoffPendingAIKickTargetLocation = FVector::ZeroVector;
	KickoffKickDirection = FVector::ForwardVector;
	KickoffRunDirection = FVector::ForwardVector;
	KickoffRunUpStartLocation = FVector::ZeroVector;
	KickoffRunThroughLocation = FVector::ZeroVector;
	ResetRestartKickContactTracking(KickoffKickContactTracker);
}

bool ASoccerMatchManager::IsKickoffTaker(
	const ASoccerAICharacter* SoccerAICharacter
) const
{
	return
		!IsNonFreeKickHumanTakerClaimedFor(ESoccerRestartType::Kickoff) &&
		IsValid(SoccerAICharacter) &&
		SoccerAICharacter == KickoffTakerAI;
}

bool ASoccerMatchManager::IsKickoffFinalRunActiveForCharacter(
	const ASoccerAICharacter* SoccerAICharacter
) const
{
	return
		bKickoffFinalRunActive &&
		MatchPlayState == ESoccerMatchPlayState::KickoffTaking &&
		IsValid(SoccerAICharacter) &&
		SoccerAICharacter == KickoffTakerAI;
}

float ASoccerMatchManager::GetKickoffRunUpMoveAcceptanceRadius() const
{
	return FMath::Max(1.0f, KickoffRunUpMoveAcceptanceRadius);
}

FVector ASoccerMatchManager::GetKickoffMoveLocation(
	const ASoccerAICharacter* SoccerAICharacter
) const
{
	if (SoccerAICharacter == nullptr)
	{
		return FVector::ZeroVector;
	}

	if (
		SoccerAICharacter == KickoffTakerAI &&
		IsNonFreeKickHumanTakerClaimedFor(ESoccerRestartType::Kickoff)
	)
	{
		if (AActor* HomePositionActor = SoccerAICharacter->GetHomePositionActor())
		{
			FVector HomeLocation = GetTeamRebasedFieldReferenceLocation(
				SoccerAICharacter->GetTeam(),
				HomePositionActor->GetActorLocation()
			);
			HomeLocation.Z = SoccerAICharacter->GetActorLocation().Z;
			return HomeLocation;
		}

		return SoccerAICharacter->GetActorLocation();
	}

	if (
		MatchPlayState == ESoccerMatchPlayState::KickoffTaking &&
		SoccerAICharacter == KickoffTakerAI
		)
	{
		if (bKickoffFinalRunActive)
		{
			return KickoffRunThroughLocation;
		}

		return KickoffRunUpStartLocation;
	}

	AActor* HomePositionActor =
		SoccerAICharacter->GetHomePositionActor();

	if (HomePositionActor != nullptr)
	{
		FVector HomeLocation = GetTeamRebasedFieldReferenceLocation(
			SoccerAICharacter->GetTeam(),
			HomePositionActor->GetActorLocation()
		);

		HomeLocation.Z =
			SoccerAICharacter->GetActorLocation().Z;

		return HomeLocation;
	}

	return SoccerAICharacter->GetActorLocation();
}

float ASoccerMatchManager::GetAttackDepthAlphaForLocation(
	const FVector& Location,
	ESoccerTeam Team
) const
{
	const FVector OwnGoalLocation =
		GetOwnGoalReferenceLocation(Team);

	const FVector OpponentGoalLocation =
		GetOpponentGoalReferenceLocation(Team);

	FVector AttackDirection =
		OpponentGoalLocation - OwnGoalLocation;

	AttackDirection.Z = 0.0f;

	const float FieldLength =
		AttackDirection.Size();

	if (FieldLength <= 1.0f)
	{
		return 0.5f;
	}

	AttackDirection =
		AttackDirection / FieldLength;

	const float Depth =
		FVector::DotProduct(
			Location - OwnGoalLocation,
			AttackDirection
		);

	return FMath::Clamp(
		Depth / FieldLength,
		0.0f,
		1.0f
	);
}

int32 ASoccerMatchManager::CountAdvancedTeammatesForCarrier(
	const ASoccerAICharacter* BallCarrier,
	float MinDepthAlpha
) const
{
	if (!IsValid(BallCarrier))
	{
		return 0;
	}

	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return 0;
	}

	int32 Count = 0;

	const ESoccerTeam Team =
		BallCarrier->GetTeam();

	for (TActorIterator<ASoccerCharacterBase> It(World); It; ++It)
	{
		const ASoccerCharacterBase* Teammate = *It;

		if (!IsValid(Teammate))
		{
			continue;
		}

		if (Teammate == BallCarrier)
		{
			continue;
		}

		if (Teammate->GetTeam() != Team)
		{
			continue;
		}

		const float TeammateDepthAlpha =
			GetAttackDepthAlphaForLocation(
				Teammate->GetActorLocation(),
				Team
			);

		if (TeammateDepthAlpha >= MinDepthAlpha)
		{
			Count++;
		}
	}

	return Count;
}

int32 ASoccerMatchManager::CountCoverTeammatesBehindCarrier(
	const ASoccerAICharacter* BallCarrier,
	float MinBehindDepthAlphaGap
) const
{
	if (!IsValid(BallCarrier))
	{
		return 0;
	}

	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return 0;
	}

	const ESoccerTeam Team =
		BallCarrier->GetTeam();

	const float CarrierDepthAlpha =
		GetAttackDepthAlphaForLocation(
			BallCarrier->GetActorLocation(),
			Team
		);

	int32 Count = 0;

	for (TActorIterator<ASoccerCharacterBase> It(World); It; ++It)
	{
		const ASoccerCharacterBase* Teammate = *It;

		if (!IsValid(Teammate))
		{
			continue;
		}

		if (Teammate == BallCarrier)
		{
			continue;
		}

		if (Teammate->GetTeam() != Team)
		{
			continue;
		}

		const float TeammateDepthAlpha =
			GetAttackDepthAlphaForLocation(
				Teammate->GetActorLocation(),
				Team
			);

		if (
			TeammateDepthAlpha <=
			CarrierDepthAlpha - MinBehindDepthAlphaGap
			)
		{
			Count++;
		}
	}

	return Count;
}

int32 ASoccerMatchManager::CountOpponentPressureAroundCarrier(
	const ASoccerAICharacter* BallCarrier,
	float PressureRadius
) const
{
	if (!IsValid(BallCarrier))
	{
		return 0;
	}

	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return 0;
	}

	int32 Count = 0;

	const FVector CarrierLocation =
		BallCarrier->GetActorLocation();

	for (TActorIterator<ASoccerCharacterBase> It(World); It; ++It)
	{
		const ASoccerCharacterBase* Candidate = *It;

		if (!IsValid(Candidate))
		{
			continue;
		}

		if (Candidate == BallCarrier)
		{
			continue;
		}

		if (Candidate->GetTeam() == BallCarrier->GetTeam())
		{
			continue;
		}

		const float Distance =
			FVector::Dist2D(
				CarrierLocation,
				Candidate->GetActorLocation()
			);

		if (Distance <= PressureRadius)
		{
			Count++;
		}
	}

	return Count;
}

bool ASoccerMatchManager::HasClearForwardLaneForCarrier(
	const ASoccerAICharacter* BallCarrier,
	float LookAheadDistance,
	float LaneHalfWidth
) const
{
	if (!IsValid(BallCarrier))
	{
		return false;
	}

	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return false;
	}

	const ESoccerTeam Team =
		BallCarrier->GetTeam();

	const FVector OwnGoalLocation =
		GetOwnGoalReferenceLocation(Team);

	const FVector OpponentGoalLocation =
		GetOpponentGoalReferenceLocation(Team);

	FVector AttackDirection =
		OpponentGoalLocation - OwnGoalLocation;

	AttackDirection.Z = 0.0f;

	if (AttackDirection.IsNearlyZero())
	{
		return false;
	}

	AttackDirection =
		AttackDirection.GetSafeNormal();

	FVector RightDirection =
		GetFieldRightDirectionForTeam(Team);

	RightDirection.Z = 0.0f;

	if (RightDirection.IsNearlyZero())
	{
		RightDirection =
			FVector::CrossProduct(
				FVector::UpVector,
				AttackDirection
			).GetSafeNormal();
	}

	const FVector CarrierLocation =
		BallCarrier->GetActorLocation();

	for (TActorIterator<ASoccerCharacterBase> It(World); It; ++It)
	{
		const ASoccerCharacterBase* Opponent = *It;

		if (!IsValid(Opponent))
		{
			continue;
		}

		if (Opponent == BallCarrier)
		{
			continue;
		}

		if (Opponent->GetTeam() == Team)
		{
			continue;
		}

		FVector ToOpponent =
			Opponent->GetActorLocation() - CarrierLocation;

		ToOpponent.Z = 0.0f;

		const float ForwardDistance =
			FVector::DotProduct(
				ToOpponent,
				AttackDirection
			);

		if (
			ForwardDistance <= 0.0f ||
			ForwardDistance > LookAheadDistance
			)
		{
			continue;
		}

		const float LateralDistance =
			FMath::Abs(
				FVector::DotProduct(
					ToOpponent,
					RightDirection
				)
			);

		if (LateralDistance <= LaneHalfWidth)
		{
			return false;
		}
	}

	return true;
}

float ASoccerMatchManager::GetBallCarrierForwardFreedomScore(
	const ASoccerAICharacter* BallCarrier
) const
{
	if (!IsValid(BallCarrier))
	{
		return 0.0f;
	}

	const ESoccerPlayerRole PlayerRole =
		BallCarrier->GetPlayerRole();

	if (PlayerRole == ESoccerPlayerRole::Forward)
	{
		return 1.0f;
	}

	if (PlayerRole == ESoccerPlayerRole::Goalkeeper)
	{
		return 0.05f;
	}

	const ESoccerTeam Team =
		BallCarrier->GetTeam();

	const float CarrierDepthAlpha =
		GetAttackDepthAlphaForLocation(
			BallCarrier->GetActorLocation(),
			Team
		);

	float SoftMaxDepthAlpha = 1.0f;
	float HardMaxDepthAlpha = 1.0f;
	float Score = 0.5f;

	if (PlayerRole == ESoccerPlayerRole::Defender)
	{
		SoftMaxDepthAlpha =
			AttackCarrierDefenderSoftMaxDepthAlpha;

		HardMaxDepthAlpha =
			AttackCarrierDefenderHardMaxDepthAlpha;

		Score = 0.42f;
	}
	else if (PlayerRole == ESoccerPlayerRole::Midfielder)
	{
		SoftMaxDepthAlpha =
			AttackCarrierMidfielderSoftMaxDepthAlpha;

		HardMaxDepthAlpha =
			AttackCarrierMidfielderHardMaxDepthAlpha;

		Score = 0.58f;
	}

	const bool bHasClearLane =
		HasClearForwardLaneForCarrier(
			BallCarrier,
			AttackCarrierForwardLaneLookAheadDistance,
			AttackCarrierForwardLaneHalfWidth
		);

	const int32 PressureCount =
		CountOpponentPressureAroundCarrier(
			BallCarrier,
			AttackCarrierPressureRadius
		);

	const int32 AdvancedTeammatesCount =
		CountAdvancedTeammatesForCarrier(
			BallCarrier,
			AttackCarrierCrowdedHighDepthAlpha
		);

	const int32 CoverBehindCount =
		CountCoverTeammatesBehindCarrier(
			BallCarrier,
			AttackCarrierCoverBehindDepthGapAlpha
		);

	if (bHasClearLane)
	{
		Score += 0.24f;
	}
	else
	{
		Score -= 0.18f;
	}

	if (PressureCount == 0)
	{
		Score += 0.12f;
	}
	else if (PressureCount == 1)
	{
		Score -= 0.08f;
	}
	else
	{
		Score -= 0.22f;
	}

	if (CoverBehindCount >= AttackCarrierRequiredCoverBehindCount)
	{
		Score += 0.20f;
	}
	else
	{
		Score -= 0.25f;
	}

	if (AdvancedTeammatesCount >= AttackCarrierCrowdedHighCount)
	{
		Score -= 0.28f;
	}
	else if (AdvancedTeammatesCount <= 1)
	{
		Score += 0.08f;
	}

	if (CarrierDepthAlpha > SoftMaxDepthAlpha)
	{
		const float PastSoftAlpha =
			FMath::Clamp(
				(
					CarrierDepthAlpha - SoftMaxDepthAlpha
					)
				/
				FMath::Max(
					HardMaxDepthAlpha - SoftMaxDepthAlpha,
					0.01f
				),
				0.0f,
				1.0f
			);

		Score -= FMath::Lerp(
			0.12f,
			0.42f,
			PastSoftAlpha
		);
	}

	if (CarrierDepthAlpha > HardMaxDepthAlpha)
	{
		Score -= 0.20f;
	}

	return FMath::Clamp(
		Score,
		0.0f,
		1.0f
	);
}

bool ASoccerMatchManager::CanBallCarrierAdvanceAggressively(
	const ASoccerAICharacter* BallCarrier
) const
{
	if (!IsValid(BallCarrier))
	{
		return false;
	}

	const ESoccerPlayerRole PlayerRole =
		BallCarrier->GetPlayerRole();

	if (PlayerRole == ESoccerPlayerRole::Goalkeeper)
	{
		return false;
	}

	const float FreedomScore =
		GetBallCarrierForwardFreedomScore(BallCarrier);

	const ESoccerTeam Team = BallCarrier->GetTeam();

	// Preserve the historical forward freedom unless the team has explicitly
	// requested a short post-recovery retention phase.
	if (PlayerRole == ESoccerPlayerRole::Forward)
	{
		if (
			ShouldApplyCollectiveTacticsToOpenPlay(Team) &&
			IsCollectiveAttackingTransitionActiveForTeam(Team) &&
			GetTacticalPlanForTeamInternal(Team).AttackingTransition ==
				ESoccerAttackingTransition::RetainPossession
			)
		{
			return FreedomScore >= 0.50f;
		}

		return true;
	}

	float RequiredFreedom = AttackCarrierAdvanceFreedomThreshold;

	if (ShouldApplyCollectiveTacticsToOpenPlay(Team))
	{
		const FSoccerTeamTacticalPlan& TacticalPlan =
			GetTacticalPlanForTeamInternal(Team);

		if (TacticalPlan.BuildUpStyle == ESoccerBuildUpStyle::ShortPossession)
		{
			RequiredFreedom += 0.07f;
		}
		else if (TacticalPlan.BuildUpStyle == ESoccerBuildUpStyle::Direct)
		{
			RequiredFreedom -= 0.07f;
		}

		if (TacticalPlan.AttackingTempo == ESoccerAttackingTempo::Patient)
		{
			RequiredFreedom += 0.04f;
		}
		else if (TacticalPlan.AttackingTempo == ESoccerAttackingTempo::Fast)
		{
			RequiredFreedom -= 0.04f;
		}

		if (IsCollectiveAttackingTransitionActiveForTeam(Team))
		{
			if (TacticalPlan.AttackingTransition == ESoccerAttackingTransition::RetainPossession)
			{
				RequiredFreedom += 0.10f;
			}
			else if (TacticalPlan.AttackingTransition == ESoccerAttackingTransition::CounterAttack)
			{
				RequiredFreedom -= 0.11f;
			}
		}
	}

	RequiredFreedom = FMath::Clamp(RequiredFreedom, 0.34f, 0.82f);

	return FreedomScore >= RequiredFreedom;
}

bool ASoccerMatchManager::ShouldBallCarrierPreferConservativeAction(
	const ASoccerAICharacter* BallCarrier
) const
{
	if (!IsValid(BallCarrier))
	{
		return true;
	}

	const ESoccerPlayerRole PlayerRole =
		BallCarrier->GetPlayerRole();

	if (PlayerRole == ESoccerPlayerRole::Goalkeeper)
	{
		return true;
	}

	return !CanBallCarrierAdvanceAggressively(BallCarrier);
}

ASoccerCharacterBase* ASoccerMatchManager::GetCurrentAttackingBallCarrier(
	ESoccerTeam Team
) const
{
	if (
		IsValid(PossessingCharacter) &&
		PossessingCharacter->GetTeam() == Team
		)
	{
		return PossessingCharacter;
	}

	return nullptr;
}

float ASoccerMatchManager::GetCarrierSoftMaxDepthAlphaForRole(
	ESoccerPlayerRole PlayerRole
) const
{
	if (PlayerRole == ESoccerPlayerRole::Defender)
	{
		return AttackCarrierDefenderSoftMaxDepthAlpha;
	}

	if (PlayerRole == ESoccerPlayerRole::Midfielder)
	{
		return AttackCarrierMidfielderSoftMaxDepthAlpha;
	}

	if (PlayerRole == ESoccerPlayerRole::Forward)
	{
		return 0.94f;
	}

	if (PlayerRole == ESoccerPlayerRole::Goalkeeper)
	{
		return 0.22f;
	}

	return 0.75f;
}

bool ASoccerMatchManager::IsCharacterBreakingAttackingRole(
	const ASoccerCharacterBase* Character
) const
{
	if (!IsValid(Character))
	{
		return false;
	}

	if (!HasActiveAttack())
	{
		return false;
	}

	const ESoccerTeam Team =
		Character->GetTeam();

	if (!IsTeamCurrentlyAttacking(Team))
	{
		return false;
	}

	const ESoccerPlayerRole PlayerRole =
		Character->GetPlayerRole();

	// Por ahora no consideramos que un delantero "rompa" el ataque.
	// Su trabajo natural es atacar profundidad.
	if (
		PlayerRole == ESoccerPlayerRole::Forward ||
		PlayerRole == ESoccerPlayerRole::Goalkeeper
		)
	{
		return false;
	}

	const float CharacterDepthAlpha =
		GetAttackDepthAlphaForLocation(
			Character->GetActorLocation(),
			Team
		);

	const float SoftMaxDepthAlpha =
		GetCarrierSoftMaxDepthAlphaForRole(PlayerRole);

	const float TriggerDepthAlpha =
		FMath::Max(
			AttackCompensationMinCarrierDepthAlpha,
			SoftMaxDepthAlpha + AttackCompensationTriggerExtraDepthAlpha
		);

	return CharacterDepthAlpha >= TriggerDepthAlpha;
}

ASoccerAICharacter* ASoccerMatchManager::FindBestAttackCompensationAI(
	ESoccerTeam Team
) const
{
	ASoccerCharacterBase* AdvancedCarrier =
		GetCurrentAttackingBallCarrier(Team);

	if (!IsCharacterBreakingAttackingRole(AdvancedCarrier))
	{
		return nullptr;
	}

	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return nullptr;
	}

	const ESoccerPlayerRole CarrierRole =
		AdvancedCarrier->GetPlayerRole();

	const float CarrierDepthAlpha =
		GetAttackDepthAlphaForLocation(
			AdvancedCarrier->GetActorLocation(),
			Team
		);

	ASoccerAICharacter* BestCandidate = nullptr;
	float BestScore = -TNumericLimits<float>::Max();

	for (TActorIterator<ASoccerAICharacter> It(World); It; ++It)
	{
		ASoccerAICharacter* Candidate = *It;

		if (!IsValid(Candidate))
		{
			continue;
		}

		if (Candidate == AdvancedCarrier)
		{
			continue;
		}

		if (Candidate->GetTeam() != Team)
		{
			continue;
		}

		if (Candidate->IsAIPossessingBall())
		{
			continue;
		}

		const ESoccerPlayerRole CandidateRole =
			Candidate->GetPlayerRole();

		if (CandidateRole == ESoccerPlayerRole::Goalkeeper)
		{
			continue;
		}

		const float CandidateDepthAlpha =
			GetAttackDepthAlphaForLocation(
				Candidate->GetActorLocation(),
				Team
			);

		float Score = 0.0f;

		// Si el que rompió fue un defensor,
		// preferimos que compense un mediocampista.
		if (CarrierRole == ESoccerPlayerRole::Defender)
		{
			if (CandidateRole == ESoccerPlayerRole::Midfielder)
			{
				Score += 4000.0f;
			}
			else if (CandidateRole == ESoccerPlayerRole::Defender)
			{
				Score += 2500.0f;
			}
			else if (CandidateRole == ESoccerPlayerRole::Forward)
			{
				Score += 500.0f;
			}
		}

		// Si el que rompió fue un mediocampista,
		// preferimos que compense un defensor.
		else if (CarrierRole == ESoccerPlayerRole::Midfielder)
		{
			if (CandidateRole == ESoccerPlayerRole::Defender)
			{
				Score += 4000.0f;
			}
			else if (CandidateRole == ESoccerPlayerRole::Midfielder)
			{
				Score += 2500.0f;
			}
			else if (CandidateRole == ESoccerPlayerRole::Forward)
			{
				Score += 500.0f;
			}
		}

		// Es mejor que compense alguien que todavía está por detrás.
		if (CandidateDepthAlpha <= CarrierDepthAlpha - 0.04f)
		{
			Score += 1200.0f;
		}
		else
		{
			Score -= 1400.0f;
		}

		const FVector CompensationLocation =
			BuildAttackCompensationCoverLocation(
				Candidate,
				AdvancedCarrier
			);

		const float DistanceToCompensationLocation =
			FVector::Dist2D(
				Candidate->GetActorLocation(),
				CompensationLocation
			);

		Score -= DistanceToCompensationLocation * 0.15f;

		if (Score > BestScore)
		{
			BestScore = Score;
			BestCandidate = Candidate;
		}
	}

	return BestCandidate;
}

FVector ASoccerMatchManager::BuildAttackCompensationCoverLocation(
	const ASoccerAICharacter* CompensatingCharacter,
	const ASoccerCharacterBase* AdvancedCarrier
) const
{
	if (!IsValid(CompensatingCharacter) || !IsValid(AdvancedCarrier))
	{
		return FVector::ZeroVector;
	}

	const ESoccerTeam Team =
		CompensatingCharacter->GetTeam();

	const FVector OwnGoalLocation =
		GetOwnGoalReferenceLocation(Team);

	const FVector OpponentGoalLocation =
		GetOpponentGoalReferenceLocation(Team);

	FVector AttackDirection =
		OpponentGoalLocation - OwnGoalLocation;

	AttackDirection.Z = 0.0f;

	const float FieldLength =
		AttackDirection.Size();

	if (FieldLength <= 1.0f)
	{
		return CompensatingCharacter->GetActorLocation();
	}

	AttackDirection =
		AttackDirection / FieldLength;

	const FVector RightDirection =
		GetFieldRightDirectionForTeam(Team);

	if (RightDirection.IsNearlyZero())
	{
		return CompensatingCharacter->GetActorLocation();
	}

	const float CarrierDepthAlpha =
		GetAttackDepthAlphaForLocation(
			AdvancedCarrier->GetActorLocation(),
			Team
		);

	float CoverDepthAlpha =
		CarrierDepthAlpha - AttackCompensationCoverBehindGapAlpha;

	CoverDepthAlpha =
		FMath::Clamp(
			CoverDepthAlpha,
			AttackCompensationMinCoverDepthAlpha,
			AttackCompensationMaxCoverDepthAlpha
		);

	const float CarrierLateralOffset =
		GetFieldLateralOffsetForTeam(
			AdvancedCarrier->GetActorLocation(),
			Team
		);

	AActor* CompensatorHomeActor =
		CompensatingCharacter->GetHomePositionActor();

	const FVector RawCompensatorHomeLocation =
		CompensatorHomeActor != nullptr
		? CompensatorHomeActor->GetActorLocation()
		: CompensatingCharacter->GetActorLocation();

	const FVector CompensatorHomeLocation = GetTeamRebasedFieldReferenceLocation(
		Team,
		RawCompensatorHomeLocation
	);

	const float CompensatorHomeLateralOffset =
		GetFieldLateralOffsetForTeam(
			CompensatorHomeLocation,
			Team
		);

	const float CoverLateralOffset =
		FMath::Lerp(
			CompensatorHomeLateralOffset,
			CarrierLateralOffset,
			AttackCompensationLateralFollowAlpha
		);

	FVector CoverLocation =
		OwnGoalLocation
		+ AttackDirection * CoverDepthAlpha * FieldLength
		+ RightDirection * CoverLateralOffset;

	CoverLocation.Z =
		CompensatingCharacter->GetActorLocation().Z;

	CoverLocation =
		ProjectLocationToNavigation(
			CoverLocation,
			CompensatingCharacter
		);

	return AdjustAttackMoveLocationUsingSpace(
		CompensatingCharacter,
		CoverLocation,
		ESoccerAIOrder::AttackCompensateCover
	);
}

FVector ASoccerMatchManager::GetAttackCompensateCoverMoveLocation(
	const ASoccerAICharacter* SoccerAICharacter
) const
{
	if (!IsValid(SoccerAICharacter))
	{
		return FVector::ZeroVector;
	}

	ASoccerCharacterBase* AdvancedCarrier =
		GetCurrentAttackingBallCarrier(
			SoccerAICharacter->GetTeam()
		);

	if (!IsCharacterBreakingAttackingRole(AdvancedCarrier))
	{
		return BuildAttackShapeLocation(
			SoccerAICharacter,
			ESoccerAIOrder::AttackRestDefense
		);
	}

	return BuildAttackCompensationCoverLocation(
		SoccerAICharacter,
		AdvancedCarrier
	);
}

bool ASoccerMatchManager::IsOpponentBlockingLaneToLocation(
	const ASoccerAICharacter* SoccerAICharacter,
	const FVector& TargetLocation,
	float LaneHalfWidth
) const
{
	if (!IsValid(SoccerAICharacter))
	{
		return false;
	}

	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return false;
	}

	const ESoccerTeam Team =
		SoccerAICharacter->GetTeam();

	const FVector StartLocation =
		GetAttackReferenceLocation(Team);

	FVector ToTarget =
		TargetLocation - StartLocation;

	ToTarget.Z = 0.0f;

	const float SegmentLength =
		ToTarget.Size();

	if (SegmentLength <= 1.0f)
	{
		return false;
	}

	const FVector SegmentDirection =
		ToTarget / SegmentLength;

	for (TActorIterator<ASoccerCharacterBase> It(World); It; ++It)
	{
		const ASoccerCharacterBase* Opponent = *It;

		if (!IsValid(Opponent))
		{
			continue;
		}

		if (Opponent->GetTeam() == Team)
		{
			continue;
		}

		FVector ToOpponent =
			Opponent->GetActorLocation() - StartLocation;

		ToOpponent.Z = 0.0f;

		const float AlongSegment =
			FVector::DotProduct(
				ToOpponent,
				SegmentDirection
			);

		if (
			AlongSegment <= 0.0f ||
			AlongSegment >= SegmentLength
			)
		{
			continue;
		}

		const FVector ClosestPoint =
			StartLocation + SegmentDirection * AlongSegment;

		const float DistanceToLane =
			FVector::Dist2D(
				Opponent->GetActorLocation(),
				ClosestPoint
			);

		if (DistanceToLane <= LaneHalfWidth)
		{
			return true;
		}
	}

	return false;
}

float ASoccerMatchManager::ScoreAttackSpaceCandidate(
	const ASoccerAICharacter* SoccerAICharacter,
	const FVector& CandidateLocation,
	const FVector& BaseLocation,
	ESoccerAIOrder AttackOrder
) const
{
	if (!IsValid(SoccerAICharacter))
	{
		return -TNumericLimits<float>::Max();
	}

	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return -TNumericLimits<float>::Max();
	}

	const ESoccerTeam Team =
		SoccerAICharacter->GetTeam();

	const float DistanceFromBase =
		FVector::Dist2D(
			CandidateLocation,
			BaseLocation
		);

	float Score =
		1000.0f -
		DistanceFromBase * AttackSpaceBaseLocationPenaltyWeight;

	for (TActorIterator<ASoccerCharacterBase> It(World); It; ++It)
	{
		const ASoccerCharacterBase* OtherCharacter = *It;

		if (!IsValid(OtherCharacter))
		{
			continue;
		}

		if (OtherCharacter == SoccerAICharacter)
		{
			continue;
		}

		const float DistanceToCandidate =
			FVector::Dist2D(
				OtherCharacter->GetActorLocation(),
				CandidateLocation
			);

		if (OtherCharacter->GetTeam() == Team)
		{
			if (DistanceToCandidate < AttackSpaceTeammateAvoidRadius)
			{
				const float CrowdingAlpha =
					1.0f -
					DistanceToCandidate / AttackSpaceTeammateAvoidRadius;

				Score -=
					CrowdingAlpha *
					AttackSpaceTeammateAvoidRadius *
					AttackSpaceTeammatePenaltyWeight;
			}
		}
		else
		{
			if (DistanceToCandidate < AttackSpaceOpponentAvoidRadius)
			{
				const float PressureAlpha =
					1.0f -
					DistanceToCandidate / AttackSpaceOpponentAvoidRadius;

				Score -=
					PressureAlpha *
					AttackSpaceOpponentAvoidRadius *
					AttackSpaceOpponentPenaltyWeight;
			}
		}
	}

	if (
		IsOpponentBlockingLaneToLocation(
			SoccerAICharacter,
			CandidateLocation,
			AttackSpaceLaneBlockHalfWidth
		)
		)
	{
		Score -= AttackSpaceBlockedLanePenalty;
	}

	const FVector OwnGoalLocation =
		GetOwnGoalReferenceLocation(Team);

	const FVector OpponentGoalLocation =
		GetOpponentGoalReferenceLocation(Team);

	FVector AttackDirection =
		OpponentGoalLocation - OwnGoalLocation;

	AttackDirection.Z = 0.0f;

	if (!AttackDirection.IsNearlyZero())
	{
		AttackDirection =
			AttackDirection.GetSafeNormal();

		const float CandidateDepth =
			FVector::DotProduct(
				CandidateLocation - OwnGoalLocation,
				AttackDirection
			);

		const float BaseDepth =
			FVector::DotProduct(
				BaseLocation - OwnGoalLocation,
				AttackDirection
			);

		const float DepthDelta =
			CandidateDepth - BaseDepth;

		if (AttackOrder == ESoccerAIOrder::AttackRunIntoSpace)
		{
			Score += FMath::Clamp(
				DepthDelta,
				-200.0f,
				400.0f
			) * 0.35f;
		}
		else if (AttackOrder == ESoccerAIOrder::AttackSupportShort)
		{
			if (DepthDelta > 180.0f)
			{
				Score -= DepthDelta * 0.25f;
			}
		}
		else if (AttackOrder == ESoccerAIOrder::AttackRestDefense)
		{
			if (DepthDelta > 120.0f)
			{
				Score -= DepthDelta * 0.55f;
			}
		}
	}

	if (AttackOrder == ESoccerAIOrder::AttackWideSupport)
	{
		const float CandidateLateral =
			FMath::Abs(
				GetFieldLateralOffsetForTeam(
					CandidateLocation,
					Team
				)
			);

		const float BaseLateral =
			FMath::Abs(
				GetFieldLateralOffsetForTeam(
					BaseLocation,
					Team
				)
			);

		Score +=
			FMath::Clamp(
				CandidateLateral - BaseLateral,
				-200.0f,
				350.0f
			) * 0.25f;
	}

	return Score;
}

FVector ASoccerMatchManager::AdjustAttackMoveLocationUsingSpace(
	const ASoccerAICharacter* SoccerAICharacter,
	const FVector& BaseLocation,
	ESoccerAIOrder AttackOrder
) const
{
	if (!IsValid(SoccerAICharacter))
	{
		return BaseLocation;
	}

	const ESoccerTeam Team =
		SoccerAICharacter->GetTeam();

	const FVector OwnGoalLocation =
		GetOwnGoalReferenceLocation(Team);

	const FVector OpponentGoalLocation =
		GetOpponentGoalReferenceLocation(Team);

	FVector AttackDirection =
		OpponentGoalLocation - OwnGoalLocation;

	AttackDirection.Z = 0.0f;

	const float FieldLength =
		AttackDirection.Size();

	if (FieldLength <= 1.0f)
	{
		return BaseLocation;
	}

	AttackDirection =
		AttackDirection / FieldLength;

	FVector RightDirection =
		GetFieldRightDirectionForTeam(Team);

	RightDirection.Z = 0.0f;

	if (RightDirection.IsNearlyZero())
	{
		return BaseLocation;
	}

	RightDirection =
		RightDirection.GetSafeNormal();

	const float ScaledAttackSpaceDepthSearchStep =
		SoccerFieldDimensions::ScaleAuthoredLongitudinalDistance(
			AttackSpaceDepthSearchStep
		);
	const float ScaledAttackSpaceLateralSearchStep =
		SoccerFieldDimensions::ScaleAuthoredLateralDistance(
			AttackSpaceLateralSearchStep
		);
	const float ScaledAttackShapeMaxLateralOffset =
		FMath::Min(
			SoccerFieldDimensions::ScaleAuthoredLateralDistance(
				AttackShapeMaxLateralOffset
			),
			FMath::Max(0.0f, SoccerFieldDimensions::HalfPitchWidthCm - 120.0f)
		);

	const float BaseLateral =
		GetFieldLateralOffsetForTeam(
			BaseLocation,
			Team
		);

	float LaneSign = 0.0f;

	if (BaseLateral > 80.0f)
	{
		LaneSign = 1.0f;
	}
	else if (BaseLateral < -80.0f)
	{
		LaneSign = -1.0f;
	}

	if (LaneSign == 0.0f)
	{
		AActor* HomeActor =
			SoccerAICharacter->GetHomePositionActor();

		if (HomeActor != nullptr)
		{
			const float HomeLateral =
				GetFieldLateralOffsetForTeam(
					GetTeamRebasedFieldReferenceLocation(
						Team,
						HomeActor->GetActorLocation()
					),
					Team
				);

			if (HomeLateral > 80.0f)
			{
				LaneSign = 1.0f;
			}
			else if (HomeLateral < -80.0f)
			{
				LaneSign = -1.0f;
			}
		}
	}

	if (LaneSign == 0.0f)
	{
		LaneSign = 1.0f;
	}

	TArray<FVector2D> CandidateOffsets;

	CandidateOffsets.Add(FVector2D(0.0f, 0.0f));

	if (AttackOrder == ESoccerAIOrder::AttackSupportShort)
	{
		CandidateOffsets.Add(FVector2D(-ScaledAttackSpaceDepthSearchStep, 0.0f));
		CandidateOffsets.Add(FVector2D(0.0f, ScaledAttackSpaceLateralSearchStep));
		CandidateOffsets.Add(FVector2D(0.0f, -ScaledAttackSpaceLateralSearchStep));
		CandidateOffsets.Add(FVector2D(-ScaledAttackSpaceDepthSearchStep, ScaledAttackSpaceLateralSearchStep));
		CandidateOffsets.Add(FVector2D(-ScaledAttackSpaceDepthSearchStep, -ScaledAttackSpaceLateralSearchStep));
	}
	else if (AttackOrder == ESoccerAIOrder::AttackSupportForward)
	{
		CandidateOffsets.Add(FVector2D(ScaledAttackSpaceDepthSearchStep, 0.0f));
		CandidateOffsets.Add(FVector2D(0.0f, ScaledAttackSpaceLateralSearchStep));
		CandidateOffsets.Add(FVector2D(0.0f, -ScaledAttackSpaceLateralSearchStep));
		CandidateOffsets.Add(FVector2D(ScaledAttackSpaceDepthSearchStep, ScaledAttackSpaceLateralSearchStep));
		CandidateOffsets.Add(FVector2D(ScaledAttackSpaceDepthSearchStep, -ScaledAttackSpaceLateralSearchStep));
		CandidateOffsets.Add(FVector2D(-ScaledAttackSpaceDepthSearchStep, ScaledAttackSpaceLateralSearchStep));
		CandidateOffsets.Add(FVector2D(-ScaledAttackSpaceDepthSearchStep, -ScaledAttackSpaceLateralSearchStep));
	}
	else if (AttackOrder == ESoccerAIOrder::AttackRunIntoSpace)
	{
		CandidateOffsets.Add(FVector2D(ScaledAttackSpaceDepthSearchStep, 0.0f));
		CandidateOffsets.Add(FVector2D(ScaledAttackSpaceDepthSearchStep, ScaledAttackSpaceLateralSearchStep));
		CandidateOffsets.Add(FVector2D(ScaledAttackSpaceDepthSearchStep, -ScaledAttackSpaceLateralSearchStep));
		CandidateOffsets.Add(FVector2D(ScaledAttackSpaceDepthSearchStep * 1.6f, ScaledAttackSpaceLateralSearchStep * 0.5f));
		CandidateOffsets.Add(FVector2D(ScaledAttackSpaceDepthSearchStep * 1.6f, -ScaledAttackSpaceLateralSearchStep * 0.5f));
	}
	else if (AttackOrder == ESoccerAIOrder::AttackWideSupport)
	{
		CandidateOffsets.Add(FVector2D(0.0f, LaneSign * ScaledAttackSpaceLateralSearchStep));
		CandidateOffsets.Add(FVector2D(ScaledAttackSpaceDepthSearchStep, LaneSign * ScaledAttackSpaceLateralSearchStep));
		CandidateOffsets.Add(FVector2D(-ScaledAttackSpaceDepthSearchStep * 0.5f, LaneSign * ScaledAttackSpaceLateralSearchStep));
		CandidateOffsets.Add(FVector2D(0.0f, LaneSign * ScaledAttackSpaceLateralSearchStep * 1.6f));
	}
	else if (
		AttackOrder == ESoccerAIOrder::AttackRestDefense ||
		AttackOrder == ESoccerAIOrder::AttackCompensateCover
		)
	{
		CandidateOffsets.Add(FVector2D(0.0f, ScaledAttackSpaceLateralSearchStep * 0.5f));
		CandidateOffsets.Add(FVector2D(0.0f, -ScaledAttackSpaceLateralSearchStep * 0.5f));
		CandidateOffsets.Add(FVector2D(-ScaledAttackSpaceDepthSearchStep * 0.5f, 0.0f));
	}
	else
	{
		CandidateOffsets.Add(FVector2D(0.0f, ScaledAttackSpaceLateralSearchStep));
		CandidateOffsets.Add(FVector2D(0.0f, -ScaledAttackSpaceLateralSearchStep));
	}

	FVector BestLocation = BaseLocation;

	float BestScore =
		ScoreAttackSpaceCandidate(
			SoccerAICharacter,
			BaseLocation,
			BaseLocation,
			AttackOrder
		);

	for (const FVector2D& CandidateOffset : CandidateOffsets)
	{
		FVector CandidateLocation =
			BaseLocation
			+ AttackDirection * CandidateOffset.X
			+ RightDirection * CandidateOffset.Y;

		const float CandidateDepth =
			FVector::DotProduct(
				CandidateLocation - OwnGoalLocation,
				AttackDirection
			);

		const float CandidateLateral =
			FVector::DotProduct(
				CandidateLocation - OwnGoalLocation,
				RightDirection
			);

		const float ClampedDepth =
			FMath::Clamp(
				CandidateDepth,
				120.0f,
				FieldLength - 320.0f
			);

		const float ClampedLateral =
			FMath::Clamp(
				CandidateLateral,
				-ScaledAttackShapeMaxLateralOffset,
				ScaledAttackShapeMaxLateralOffset
			);

		CandidateLocation =
			OwnGoalLocation
			+ AttackDirection * ClampedDepth
			+ RightDirection * ClampedLateral;

		CandidateLocation.Z =
			SoccerAICharacter->GetActorLocation().Z;

		CandidateLocation =
			ProjectLocationToNavigation(
				CandidateLocation,
				SoccerAICharacter
			);

		const float CandidateScore =
			ScoreAttackSpaceCandidate(
				SoccerAICharacter,
				CandidateLocation,
				BaseLocation,
				AttackOrder
			);

		if (CandidateScore > BestScore)
		{
			BestScore = CandidateScore;
			BestLocation = CandidateLocation;
		}
	}

	return BestLocation;
}

int32 ASoccerMatchManager::CountOpponentsAroundLocation(
	ESoccerTeam Team,
	const FVector& Location,
	float Radius
) const
{
	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return 0;
	}

	int32 Count = 0;

	for (TActorIterator<ASoccerCharacterBase> It(World); It; ++It)
	{
		const ASoccerCharacterBase* Character = *It;

		if (!IsValid(Character))
		{
			continue;
		}

		if (Character->GetTeam() == Team)
		{
			continue;
		}

		const float Distance =
			FVector::Dist2D(
				Character->GetActorLocation(),
				Location
			);

		if (Distance <= Radius)
		{
			Count++;
		}
	}

	return Count;
}

int32 ASoccerMatchManager::CountTeammatesAroundLocation(
	ESoccerTeam Team,
	const FVector& Location,
	float Radius,
	const ASoccerCharacterBase* IgnoreA,
	const ASoccerCharacterBase* IgnoreB
) const
{
	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return 0;
	}

	int32 Count = 0;

	for (TActorIterator<ASoccerCharacterBase> It(World); It; ++It)
	{
		const ASoccerCharacterBase* Character = *It;

		if (!IsValid(Character))
		{
			continue;
		}

		if (Character == IgnoreA || Character == IgnoreB)
		{
			continue;
		}

		if (Character->GetTeam() != Team)
		{
			continue;
		}

		const float Distance =
			FVector::Dist2D(
				Character->GetActorLocation(),
				Location
			);

		if (Distance <= Radius)
		{
			Count++;
		}
	}

	return Count;
}

bool ASoccerMatchManager::IsOpponentBlockingLaneBetweenLocations(
	ESoccerTeam Team,
	const FVector& StartLocation,
	const FVector& TargetLocation,
	float LaneHalfWidth
) const
{
	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return false;
	}

	FVector ToTarget =
		TargetLocation - StartLocation;

	ToTarget.Z = 0.0f;

	const float SegmentLength =
		ToTarget.Size();

	if (SegmentLength <= 1.0f)
	{
		return false;
	}

	const FVector SegmentDirection =
		ToTarget / SegmentLength;

	for (TActorIterator<ASoccerCharacterBase> It(World); It; ++It)
	{
		const ASoccerCharacterBase* Opponent = *It;

		if (!IsValid(Opponent))
		{
			continue;
		}

		if (Opponent->GetTeam() == Team)
		{
			continue;
		}

		FVector ToOpponent =
			Opponent->GetActorLocation() - StartLocation;

		ToOpponent.Z = 0.0f;

		const float AlongSegment =
			FVector::DotProduct(
				ToOpponent,
				SegmentDirection
			);

		if (AlongSegment <= 0.0f || AlongSegment >= SegmentLength)
		{
			continue;
		}

		const FVector ClosestPoint =
			StartLocation + SegmentDirection * AlongSegment;

		const float DistanceToLane =
			FVector::Dist2D(
				Opponent->GetActorLocation(),
				ClosestPoint
			);

		if (DistanceToLane <= LaneHalfWidth)
		{
			return true;
		}
	}

	return false;
}

bool ASoccerMatchManager::BuildAttackPassTargetLocation(
	const ASoccerAICharacter* BallCarrier,
	const ASoccerCharacterBase* Receiver,
	ESoccerAIOrder ReceiverOrder,
	ESoccerAttackPassType PassType,
	FVector& OutTargetLocation
) const
{
	OutTargetLocation = FVector::ZeroVector;

	if (!IsValid(BallCarrier) || !IsValid(Receiver))
	{
		return false;
	}

	const ESoccerTeam Team =
		BallCarrier->GetTeam();

	FVector TargetLocation =
		Receiver->GetActorLocation();

	const FVector OwnGoalLocation =
		GetOwnGoalReferenceLocation(Team);

	const FVector OpponentGoalLocation =
		GetOpponentGoalReferenceLocation(Team);

	FVector AttackDirection =
		OpponentGoalLocation - OwnGoalLocation;

	AttackDirection.Z = 0.0f;

	if (!AttackDirection.IsNearlyZero())
	{
		AttackDirection =
			AttackDirection.GetSafeNormal();
	}

	const float CarrierDepthAlpha =
		GetAttackDepthAlphaForLocation(
			BallCarrier->GetActorLocation(),
			Team
		);

	const float ReceiverDepthAlpha =
		GetAttackDepthAlphaForLocation(
			Receiver->GetActorLocation(),
			Team
		);

	FVector ReceiverVelocity = Receiver->GetVelocity();
	ReceiverVelocity.Z = 0.0f;

	const float ReceiverSpeed = ReceiverVelocity.Size2D();
	const float ForwardRunDot = ReceiverSpeed > KINDA_SMALL_NUMBER
		? FVector::DotProduct(ReceiverVelocity / ReceiverSpeed, AttackDirection)
		: 0.0f;

	const bool bReceiverHasForwardOrder =
		ReceiverOrder == ESoccerAIOrder::AttackRunIntoSpace ||
		ReceiverOrder == ESoccerAIOrder::AttackWideSupport;

	if (PassType == ESoccerAttackPassType::ForwardSpace)
	{
		const bool bMovingForward =
			ReceiverSpeed >= FMath::Max(0.0f, AttackPassForwardRunMinSpeed) &&
			ForwardRunDot >= AttackPassForwardRunMinDot;

		const bool bReceiverIsAhead =
			ReceiverDepthAlpha > CarrierDepthAlpha + 0.02f;

		if (
			!bEnableAttackForwardSpacePass ||
			AttackDirection.IsNearlyZero() ||
			(!bMovingForward && !(bReceiverHasForwardOrder && bReceiverIsAhead))
		)
		{
			return false;
		}

		FVector LeadOffset = ReceiverVelocity * FMath::Max(0.0f, AttackPassForwardLeadTime);
		float ForwardLead = FVector::DotProduct(LeadOffset, AttackDirection);
		ForwardLead = FMath::Clamp(
			FMath::Max(ForwardLead, AttackPassForwardMinLeadDistance),
			0.0f,
			FMath::Max(AttackPassForwardMinLeadDistance, AttackPassForwardMaxLeadDistance)
		);

		const FVector LateralLead = LeadOffset - AttackDirection * FVector::DotProduct(LeadOffset, AttackDirection);
		TargetLocation = Receiver->GetActorLocation() + AttackDirection * ForwardLead + LateralLead;
	}
	else if (PassType == ESoccerAttackPassType::RetentionSpace)
	{
		if (
			AttackDirection.IsNearlyZero() ||
			ReceiverDepthAlpha > CarrierDepthAlpha + AttackPassRetentionMaxDepthAdvantage
		)
		{
			return false;
		}

		TargetLocation = Receiver->GetActorLocation()
			- AttackDirection * FMath::Max(0.0f, AttackPassRetentionLeadDistance);
	}
	else
	{
		FVector LeadOffset = ReceiverVelocity * FMath::Max(0.0f, AttackPassToFeetLeadTime);
		const float MaxLead = FMath::Max(0.0f, AttackPassToFeetMaxLeadDistance);
		if (MaxLead > 0.0f && LeadOffset.Size2D() > MaxLead)
		{
			LeadOffset = LeadOffset.GetSafeNormal2D() * MaxLead;
		}
		TargetLocation += LeadOffset;
	}

	const float FieldInset = FMath::Max(0.0f, AttackPassTargetFieldInset);
	if (IsValid(SoccerField))
	{
		TargetLocation = SoccerField->ClampWorldLocationInsidePitch(TargetLocation, FieldInset);
	}
	else
	{
		TargetLocation = SoccerFieldDimensions::ClampLocationInsidePitch(TargetLocation, FieldInset);
	}

	TargetLocation.Z =
		IsValid(SoccerBall)
		? SoccerBall->GetActorLocation().Z
		: BallCarrier->GetActorLocation().Z;

	if (PassType == ESoccerAttackPassType::ForwardSpace)
	{
		const float ReceiverArrival = Receiver->EstimateArrivalTimeToLocation(TargetLocation);
		const float OpponentArrival = GetEarliestOpponentArrivalTimeToLocation(Team, TargetLocation);
		if (
			FMath::IsFinite(OpponentArrival) &&
			OpponentArrival - ReceiverArrival < AttackPassForwardMinArrivalMargin
		)
		{
			return false;
		}
	}

	OutTargetLocation = TargetLocation;
	return true;
}

float ASoccerMatchManager::GetEarliestOpponentArrivalTimeToLocation(
	ESoccerTeam Team,
	const FVector& TargetLocation
) const
{
	float EarliestArrival = TNumericLimits<float>::Max();
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return EarliestArrival;
	}

	for (TActorIterator<ASoccerCharacterBase> It(World); It; ++It)
	{
		const ASoccerCharacterBase* Candidate = *It;
		if (!IsValid(Candidate) || Candidate->GetTeam() == Team)
		{
			continue;
		}

		EarliestArrival = FMath::Min(
			EarliestArrival,
			Candidate->EstimateArrivalTimeToLocation(TargetLocation)
		);
	}

	return EarliestArrival;
}

float ASoccerMatchManager::ScoreAttackPassOption(
	const ASoccerAICharacter* BallCarrier,
	const ASoccerCharacterBase* Receiver,
	const FVector& PassTargetLocation,
	ESoccerAttackPassType PassType
) const
{
	if (!IsValid(BallCarrier) || !IsValid(Receiver))
	{
		return -TNumericLimits<float>::Max();
	}

	if (!CanCharacterBePassReceiverNow(BallCarrier, Receiver))
	{
		return -TNumericLimits<float>::Max();
	}

	if (BallCarrier == Receiver)
	{
		return -TNumericLimits<float>::Max();
	}

	const ESoccerTeam Team =
		BallCarrier->GetTeam();

	if (Receiver->GetTeam() != Team)
	{
		return -TNumericLimits<float>::Max();
	}

	const FVector CarrierLocation =
		BallCarrier->GetActorLocation();

	const float PassDistance =
		FVector::Dist2D(
			CarrierLocation,
			PassTargetLocation
		);

	if (
		PassDistance < AttackDecisionMinPassDistance ||
		PassDistance > AttackDecisionMaxPassDistance
		)
	{
		return -TNumericLimits<float>::Max();
	}

	float Score = 500.0f;

	const float CarrierDepthAlpha =
		GetAttackDepthAlphaForLocation(
			CarrierLocation,
			Team
		);

	const float TargetDepthAlpha =
		GetAttackDepthAlphaForLocation(
			PassTargetLocation,
			Team
		);

	const float ProgressAlpha =
		TargetDepthAlpha - CarrierDepthAlpha;

	// Progresar suma, pero tampoco obligamos siempre a pasar hacia adelante.
	Score += ProgressAlpha * 900.0f;

	if (PassType == ESoccerAttackPassType::ForwardSpace)
	{
		Score += 260.0f;
	}
	else if (PassType == ESoccerAttackPassType::RetentionSpace)
	{
		Score += AttackPassRetentionSafetyBonus;
	}

	const ESoccerPlayerRole ReceiverRole =
		Receiver->GetPlayerRole();

	if (ReceiverRole == ESoccerPlayerRole::Forward)
	{
		Score += 220.0f;
	}
	else if (ReceiverRole == ESoccerPlayerRole::Midfielder)
	{
		Score += 130.0f;
	}
	else if (ReceiverRole == ESoccerPlayerRole::Defender)
	{
		Score += 30.0f;
	}

	const int32 OpponentsNearTarget =
		CountOpponentsAroundLocation(
			Team,
			PassTargetLocation,
			AttackDecisionPassTargetOpponentRadius
		);

	Score -= OpponentsNearTarget * 260.0f;

	if (
		PassType == ESoccerAttackPassType::ToFeet &&
		CountOpponentsAroundLocation(
			Team,
			Receiver->GetActorLocation(),
			FMath::Max(0.0f, AttackPassToFeetCriticalOpponentRadius)
		) > 0
	)
	{
		Score -= FMath::Max(0.0f, AttackPassToFeetCriticalPressurePenalty);
	}

	const int32 TeammatesNearTarget =
		CountTeammatesAroundLocation(
			Team,
			PassTargetLocation,
			AttackDecisionPassTargetTeammateRadius,
			BallCarrier,
			Receiver
		);

	Score -= TeammatesNearTarget * 130.0f;

	const bool bLaneBlocked =
		IsOpponentBlockingLaneBetweenLocations(
			Team,
			CarrierLocation,
			PassTargetLocation,
			AttackDecisionPassLaneHalfWidth
		);

	if (bLaneBlocked)
	{
		Score -= 520.0f;
	}

	// Pase hacia atrás seguro: útil cuando el portador debe conservar.
	const bool bCarrierShouldBeConservative =
		ShouldBallCarrierPreferConservativeAction(BallCarrier);

	if (bCarrierShouldBeConservative && ProgressAlpha < 0.02f)
	{
		Score += 180.0f;
	}

	// Defensor/mediocampista con equipo roto: mejor pase que autopase.
	if (bCarrierShouldBeConservative && ReceiverRole != ESoccerPlayerRole::Forward)
	{
		Score += 90.0f;
	}

	// Si el target está muy alto y lleno, bajamos riesgo.
	if (TargetDepthAlpha > 0.82f && OpponentsNearTarget >= 2)
	{
		Score -= 260.0f;
	}

	if (ShouldApplyCollectiveTacticsToOpenPlay(Team))
	{
		const FSoccerTeamTacticalPlan& TacticalPlan =
			GetTacticalPlanForTeamInternal(Team);

		const float StyleStrength =
			FMath::Max(0.0f, CollectivePassStyleScoreStrength);

		const float TransitionStrength =
			FMath::Max(0.0f, CollectiveTransitionPassScoreStrength);

		const float PassDistanceAlpha = FMath::Clamp(
			(PassDistance - AttackDecisionMinPassDistance) /
			FMath::Max(
				AttackDecisionMaxPassDistance - AttackDecisionMinPassDistance,
				1.0f
			),
			0.0f,
			1.0f
		);

		if (TacticalPlan.BuildUpStyle == ESoccerBuildUpStyle::ShortPossession)
		{
			Score += (1.0f - PassDistanceAlpha) * StyleStrength;

			if (ProgressAlpha <= 0.15f)
			{
				Score += StyleStrength * 0.45f;
			}

			if (PassType == ESoccerAttackPassType::ForwardSpace)
			{
				Score -= StyleStrength * 0.55f;
			}
		}
		else if (TacticalPlan.BuildUpStyle == ESoccerBuildUpStyle::Direct)
		{
			Score += FMath::Max(0.0f, ProgressAlpha) * StyleStrength * 2.8f;
			Score += PassDistanceAlpha * StyleStrength * 0.55f;

			if (PassType == ESoccerAttackPassType::ForwardSpace)
			{
				Score += StyleStrength;
			}
		}

		if (TacticalPlan.AttackingTempo == ESoccerAttackingTempo::Patient)
		{
			if (!bLaneBlocked && ProgressAlpha <= 0.18f)
			{
				Score += StyleStrength * 0.55f;
			}
		}
		else if (TacticalPlan.AttackingTempo == ESoccerAttackingTempo::Fast)
		{
			Score += FMath::Max(0.0f, ProgressAlpha) * StyleStrength * 1.8f;

			if (PassType == ESoccerAttackPassType::ForwardSpace)
			{
				Score += StyleStrength * 0.55f;
			}
		}

		const float TargetLateralOffset =
			GetFieldLateralOffsetForTeam(PassTargetLocation, Team);

		const float AbsoluteLateralAlpha = FMath::Clamp(
			FMath::Abs(TargetLateralOffset) /
			FMath::Max(SoccerFieldDimensions::HalfPitchWidthCm, 1.0f),
			0.0f,
			1.0f
		);

		if (TacticalPlan.AttackChannel == ESoccerAttackChannel::Left)
		{
			Score += TargetLateralOffset < -180.0f
				? StyleStrength * 0.90f
				: -StyleStrength * 0.45f;
		}
		else if (TacticalPlan.AttackChannel == ESoccerAttackChannel::Right)
		{
			Score += TargetLateralOffset > 180.0f
				? StyleStrength * 0.90f
				: -StyleStrength * 0.45f;
		}
		else if (TacticalPlan.AttackChannel == ESoccerAttackChannel::Center)
		{
			Score += (1.0f - AbsoluteLateralAlpha) * StyleStrength * 0.70f;
			Score -= AbsoluteLateralAlpha * StyleStrength * 0.35f;
		}

		if (TacticalPlan.AttackingWidth == ESoccerAttackingWidth::Narrow)
		{
			Score += (1.0f - AbsoluteLateralAlpha) * StyleStrength * 0.35f;
		}
		else if (TacticalPlan.AttackingWidth == ESoccerAttackingWidth::Wide)
		{
			Score += AbsoluteLateralAlpha * StyleStrength * 0.35f;
		}

		if (IsCollectiveAttackingTransitionActiveForTeam(Team))
		{
			if (TacticalPlan.AttackingTransition == ESoccerAttackingTransition::RetainPossession)
			{
				if (ProgressAlpha <= 0.12f && !bLaneBlocked)
				{
					Score += TransitionStrength;
				}

				if (PassType == ESoccerAttackPassType::ForwardSpace)
				{
					Score -= TransitionStrength * 0.55f;
				}
			}
			else if (TacticalPlan.AttackingTransition == ESoccerAttackingTransition::CounterAttack)
			{
				Score += FMath::Max(0.0f, ProgressAlpha) * TransitionStrength * 2.8f;

				if (PassType == ESoccerAttackPassType::ForwardSpace)
				{
					Score += TransitionStrength;
				}
			}
		}
	}

	return Score;
}

float ASoccerMatchManager::ScoreRestartPassReceiverCandidate(
	ESoccerTeam RestartTeam,
	const ASoccerAICharacter* TakerAI,
	const ASoccerCharacterBase* Receiver,
	const FVector& TargetLocation,
	bool bAerialPass
) const
{
	if (
		!IsValid(TakerAI) ||
		!IsValid(Receiver) ||
		Receiver == TakerAI ||
		Receiver->GetTeam() != RestartTeam ||
		Receiver->GetPlayerRole() == ESoccerPlayerRole::Goalkeeper ||
		!CanCharacterBePassReceiverNow(TakerAI, Receiver)
	)
	{
		return -TNumericLimits<float>::Max();
	}

	const FVector PassStartLocation = IsValid(SoccerBall)
		? SoccerBall->GetActorLocation()
		: TakerAI->GetActorLocation();

	const float PassDistance = FVector::Dist2D(
		PassStartLocation,
		TargetLocation
	);

	const float MinimumDistance =
		FMath::Max(0.0f, RestartReceiverMinimumPassDistance);
	const float MaximumDistance =
		FMath::Max(MinimumDistance + 1.0f, RestartReceiverMaximumPassDistance);

	if (PassDistance < MinimumDistance || PassDistance > MaximumDistance)
	{
		return -TNumericLimits<float>::Max();
	}

	const int32 CriticalOpponents = CountOpponentsAroundLocation(
		RestartTeam,
		TargetLocation,
		FMath::Max(0.0f, RestartReceiverCriticalOpponentRadius)
	);

	if (CriticalOpponents > 0)
	{
		return -TNumericLimits<float>::Max();
	}

	const int32 OpponentsNearReceiver = CountOpponentsAroundLocation(
		RestartTeam,
		TargetLocation,
		FMath::Max(0.0f, RestartReceiverOpponentPressureRadius)
	);

	const float LaneHalfWidth = bAerialPass
		? RestartReceiverAerialLaneHalfWidth
		: RestartReceiverGroundLaneHalfWidth;

	const bool bLaneBlocked = IsOpponentBlockingLaneBetweenLocations(
		RestartTeam,
		PassStartLocation,
		TargetLocation,
		FMath::Max(0.0f, LaneHalfWidth)
	);

	// A clearly occupied ground lane is not an acceptable autonomous restart
	// pass. Aerial restarts can clear a narrow blocker, but still pay a penalty.
	if (bLaneBlocked && !bAerialPass)
	{
		return -TNumericLimits<float>::Max();
	}

	const float DistanceAlpha = FMath::Clamp(
		(PassDistance - MinimumDistance) /
		FMath::Max(1.0f, MaximumDistance - MinimumDistance),
		0.0f,
		1.0f
	);

	const float StartDepthAlpha = GetAttackDepthAlphaForLocation(
		PassStartLocation,
		RestartTeam
	);
	const float TargetDepthAlpha = GetAttackDepthAlphaForLocation(
		TargetLocation,
		RestartTeam
	);
	const float ProgressAlpha = TargetDepthAlpha - StartDepthAlpha;

	float Score = 1000.0f;
	Score += ProgressAlpha * 350.0f;
	Score -= DistanceAlpha * 180.0f;
	Score -= OpponentsNearReceiver * 240.0f;

	if (bLaneBlocked)
	{
		Score -= 140.0f;
	}

	const int32 NearbyTeammates = CountTeammatesAroundLocation(
		RestartTeam,
		TargetLocation,
		520.0f,
		TakerAI,
		Receiver
	);
	Score -= NearbyTeammates * 55.0f;

	switch (Receiver->GetPlayerRole())
	{
	case ESoccerPlayerRole::Forward:
		Score += 70.0f;
		break;
	case ESoccerPlayerRole::Midfielder:
		Score += 50.0f;
		break;
	case ESoccerPlayerRole::Defender:
		Score += 20.0f;
		break;
	default:
		break;
	}

	return Score;
}

void ASoccerMatchManager::SelectActiveRestartExecutionReceiver(
	ESoccerTeam RestartTeam,
	ASoccerAICharacter* TakerAI,
	ASoccerAICharacter* PlannedReceiverAI,
	bool bAerialPass
)
{
	// The decision is frozen for the whole execution, including a missed run-up
	// retry. A new restart clears this runtime selection in BeginRestartContext.
	if (IsValid(ActiveRestartExecutionReceiver))
	{
		return;
	}

	ActiveRestartExecutionReceiver = PlannedReceiverAI;
	bActiveRestartExecutionReceiverIsHuman = false;

	if (
		!bEnableRestartHumanReceiverSelection ||
		!IsValid(TakerAI) ||
		!IsValid(PlannedReceiverAI)
	)
	{
		return;
	}

	AThirdPersonCppCharacter* HumanReceiver =
		FindHumanCharacterForTeam(RestartTeam);

	if (
		!IsValid(HumanReceiver) ||
		HumanReceiver->GetTeam() != RestartTeam ||
		HumanReceiver->GetPlayerRole() == ESoccerPlayerRole::Goalkeeper ||
		!CanCharacterBePassReceiverNow(TakerAI, HumanReceiver)
	)
	{
		return;
	}

	const FVector PlannedTargetLocation = PlannedReceiverAI->GetActorLocation();
	const FVector HumanTargetLocation = HumanReceiver->GetActorLocation();

	const float PlannedScore = ScoreRestartPassReceiverCandidate(
		RestartTeam,
		TakerAI,
		PlannedReceiverAI,
		PlannedTargetLocation,
		bAerialPass
	);
	const float HumanScore = ScoreRestartPassReceiverCandidate(
		RestartTeam,
		TakerAI,
		HumanReceiver,
		HumanTargetLocation,
		bAerialPass
	);

	const float MinimumCandidateScore = RestartReceiverMinimumCandidateScore;
	if (!FMath::IsFinite(HumanScore) || HumanScore < MinimumCandidateScore)
	{
		return;
	}

	const float HumanAdjustedScore =
		HumanScore + FMath::Max(0.0f, RestartHumanReceiverPreferenceBonus);

	if (FMath::IsFinite(PlannedScore) && HumanAdjustedScore < PlannedScore)
	{
		return;
	}

	const float SelectionChance = FMath::Clamp(
		RestartHumanReceiverSelectionChance,
		0.0f,
		1.0f
	);

	if (FMath::FRand() > SelectionChance)
	{
		return;
	}

	ActiveRestartExecutionReceiver = HumanReceiver;
	bActiveRestartExecutionReceiverIsHuman = true;

	ASoccerDebugManager::Message(
		this,
		ESoccerDebugCategory::Restarts,
		TEXT("RESTART: humano elegido como receptor"),
		FColor::Cyan
	);
}

FVector ASoccerMatchManager::GetActiveRestartExecutionTargetLocation(
	const FVector& FallbackLocation
) const
{
	if (!IsValid(ActiveRestartExecutionReceiver))
	{
		return FallbackLocation;
	}

	FVector TargetLocation = ActiveRestartExecutionReceiver->GetActorLocation();
	TargetLocation.Z = FallbackLocation.Z;
	return TargetLocation;
}

void ASoccerMatchManager::ClearActiveRestartExecutionReceiver()
{
	ActiveRestartExecutionReceiver = nullptr;
	bActiveRestartExecutionReceiverIsHuman = false;
}

bool ASoccerMatchManager::FindBestAttackPassOption(
	const ASoccerAICharacter* BallCarrier,
	ASoccerCharacterBase*& OutReceiver,
	FVector& OutTargetLocation,
	ESoccerAttackPassType& OutPassType,
	float& OutScore,
	bool bUsePossessionRetentionThreshold,
	bool bRequireCurrentPossession
) const
{
	OutReceiver = nullptr;
	OutTargetLocation = FVector::ZeroVector;
	OutPassType = ESoccerAttackPassType::ToFeet;
	OutScore = -TNumericLimits<float>::Max();

	if (!IsValid(BallCarrier))
	{
		return false;
	}

	if (bRequireCurrentPossession && !BallCarrier->IsAIPossessingBall())
	{
		return false;
	}

	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return false;
	}

	const ESoccerTeam Team =
		BallCarrier->GetTeam();

	for (TActorIterator<ASoccerCharacterBase> It(World); It; ++It)
	{
		ASoccerCharacterBase* Candidate = *It;

		if (!IsValid(Candidate))
		{
			continue;
		}

		if (Candidate == BallCarrier)
		{
			continue;
		}

		if (Candidate->GetTeam() != Team)
		{
			continue;
		}

		if (Candidate->GetPlayerRole() == ESoccerPlayerRole::Goalkeeper)
		{
			continue;
		}

		ESoccerAIOrder CandidateOrder =
			ESoccerAIOrder::MaintainTeamShape;

		if (const ASoccerAICharacter* CandidateAI =
			Cast<ASoccerAICharacter>(Candidate))
		{
			CandidateOrder =
				GetAIOrderForCharacter(CandidateAI);
		}

		const int32 PassTypeCount = bUsePossessionRetentionThreshold ? 3 : 2;
		const ESoccerAttackPassType PassTypes[3] =
		{
			ESoccerAttackPassType::ForwardSpace,
			ESoccerAttackPassType::ToFeet,
			ESoccerAttackPassType::RetentionSpace
		};

		for (int32 PassTypeIndex = 0; PassTypeIndex < PassTypeCount; ++PassTypeIndex)
		{
			const ESoccerAttackPassType CandidatePassType = PassTypes[PassTypeIndex];
			FVector CandidateTargetLocation = FVector::ZeroVector;
			if (!BuildAttackPassTargetLocation(
				BallCarrier,
				Candidate,
				CandidateOrder,
				CandidatePassType,
				CandidateTargetLocation
			))
			{
				continue;
			}

			if (
				CandidatePassType == ESoccerAttackPassType::RetentionSpace &&
				IsOpponentBlockingLaneBetweenLocations(
					Team,
					BallCarrier->GetActorLocation(),
					CandidateTargetLocation,
					AttackDecisionPassLaneHalfWidth
				)
			)
			{
				continue;
			}

			const float CandidateScore = ScoreAttackPassOption(
				BallCarrier,
				Candidate,
				CandidateTargetLocation,
				CandidatePassType
			);

			if (CandidateScore > OutScore)
			{
				OutScore = CandidateScore;
				OutReceiver = Candidate;
				OutTargetLocation = CandidateTargetLocation;
				OutPassType = CandidatePassType;
			}
		}
	}

	const bool bCarrierShouldBeConservative =
		ShouldBallCarrierPreferConservativeAction(BallCarrier);

	const float NormalRequiredScore =
		bCarrierShouldBeConservative
		? AttackDecisionMinForcedPassScore
		: AttackDecisionMinPassScore;

	float RequiredScore =
		bUsePossessionRetentionThreshold
		? FMath::Min(
			NormalRequiredScore,
			AttackDecisionMinRetentionPassScore
		)
		: NormalRequiredScore;

	RequiredScore += GetCollectivePassRequiredScoreAdjustment(Team);
	RequiredScore = FMath::Max(100.0f, RequiredScore);

	return IsValid(OutReceiver) && OutScore >= RequiredScore;
}

void ASoccerMatchManager::UpdateHumanPassRequestState()
{
	if (ActiveHumanPassRequestType == ESoccerHumanPassRequestType::None)
	{
		return;
	}

	if (!bEnableHumanPassRequests || !IsValid(ActiveHumanPassRequestingHuman))
	{
		ClearActiveHumanPassRequest(
			TEXT("Pedido de pase desactivado"),
			FLinearColor(0.68f, 0.68f, 0.68f, 1.0f),
			false
		);
		return;
	}

	if (ActiveHumanPassRequestingHuman->HasHumanLogicalBallControl())
	{
		ClearActiveHumanPassRequest(
			TEXT("Pedido de pase desactivado: el humano recibio la pelota"),
			FLinearColor(0.55f, 0.85f, 1.0f, 1.0f),
			true
		);
		return;
	}

	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return;
	}

	const float AutoCancelTime =
		FMath::Max(0.0f, HumanPassRequestAutoCancelTime);

	if (
		AutoCancelTime > 0.0f &&
		World->GetTimeSeconds() - LastHumanPassRequestInputTime >= AutoCancelTime
	)
	{
		ClearActiveHumanPassRequest(
			TEXT("Pedido de pase vencido"),
			FLinearColor(1.0f, 0.65f, 0.15f, 1.0f),
			true
		);
	}
}

void ASoccerMatchManager::ClearActiveHumanPassRequest(
	const FString& FeedbackMessage,
	const FLinearColor& FeedbackColor,
	bool bShowFeedback
)
{
	ActiveHumanPassRequestType = ESoccerHumanPassRequestType::None;
	ActiveHumanPassRequestingHuman = nullptr;
	LastHumanPassRequestInputTime = -1000.0f;
	LastHumanPassRequestEvaluationTime = -1000.0f;
	LastHumanPassRequestEvaluatedPasser = nullptr;

	if (bShowFeedback && !FeedbackMessage.IsEmpty())
	{
		SetHumanPassRequestHUDBrief(
			FeedbackMessage,
			FeedbackColor
		);
	}
}

void ASoccerMatchManager::SetHumanPassRequestHUDBrief(
	const FString& Message,
	const FLinearColor& Color
)
{
	HumanPassRequestHUDBriefText = Message;
	HumanPassRequestHUDBriefColor = Color;

	const UWorld* World = GetWorld();
	const float CurrentTime =
		World != nullptr
		? World->GetTimeSeconds()
		: 0.0f;

	HumanPassRequestHUDBriefExpireTime =
		CurrentTime +
		FMath::Max(0.1f, HumanPassRequestHUDBriefDuration);
}

bool ASoccerMatchManager::ShouldShowHumanPassRequestHUD() const
{
	return bShowHumanPassRequestHUD;
}

bool ASoccerMatchManager::HasActiveHumanPassRequest() const
{
	return
		ActiveHumanPassRequestType != ESoccerHumanPassRequestType::None &&
		IsValid(ActiveHumanPassRequestingHuman);
}

ESoccerHumanPassRequestType
ASoccerMatchManager::GetActiveHumanPassRequestType() const
{
	return
		HasActiveHumanPassRequest()
		? ActiveHumanPassRequestType
		: ESoccerHumanPassRequestType::None;
}

float ASoccerMatchManager::GetActiveHumanPassRequestRemainingTime() const
{
	if (!HasActiveHumanPassRequest())
	{
		return 0.0f;
	}

	const float AutoCancelTime =
		FMath::Max(0.0f, HumanPassRequestAutoCancelTime);

	if (AutoCancelTime <= 0.0f)
	{
		return -1.0f;
	}

	const UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return AutoCancelTime;
	}

	return FMath::Max(
		0.0f,
		AutoCancelTime -
		(World->GetTimeSeconds() - LastHumanPassRequestInputTime)
	);
}

bool ASoccerMatchManager::GetHumanPassRequestHUDBrief(
	FString& OutMessage,
	FLinearColor& OutColor
) const
{
	OutMessage.Reset();
	OutColor = FLinearColor::White;

	if (!bShowHumanPassRequestHUD || HumanPassRequestHUDBriefText.IsEmpty())
	{
		return false;
	}

	const UWorld* World = GetWorld();

	if (
		World != nullptr &&
		World->GetTimeSeconds() > HumanPassRequestHUDBriefExpireTime
	)
	{
		return false;
	}

	OutMessage = HumanPassRequestHUDBriefText;
	OutColor = HumanPassRequestHUDBriefColor;
	return true;
}

ASoccerAICharacter* ASoccerMatchManager::FindCurrentHumanPassRequestPasser(
	const AThirdPersonCppCharacter* RequestingHuman
) const
{
	if (!IsValid(RequestingHuman))
	{
		return nullptr;
	}

	ASoccerAICharacter* Passer =
		Cast<ASoccerAICharacter>(PossessingCharacter);

	if (
		IsValid(Passer) &&
		Passer->IsAIPossessingBall() &&
		Passer->GetTeam() == RequestingHuman->GetTeam()
	)
	{
		return Passer;
	}

	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return nullptr;
	}

	for (TActorIterator<ASoccerAICharacter> It(World); It; ++It)
	{
		ASoccerAICharacter* Candidate = *It;

		if (
			IsValid(Candidate) &&
			Candidate->GetTeam() == RequestingHuman->GetTeam() &&
			Candidate->IsAIPossessingBall()
		)
		{
			return Candidate;
		}
	}

	return nullptr;
}

void ASoccerMatchManager::ShowHumanPassRequestDebugMessage(
	const FString& Message,
	const FColor& Color
)
{
	SetHumanPassRequestHUDBrief(
		Message,
		FLinearColor(Color)
	);

	ASoccerDebugManager::Message(
		this, ESoccerDebugCategory::MatchRules,
		Message, Color);
}

FVector ASoccerMatchManager::BuildHumanRequestedPassTargetLocation(
	const ASoccerAICharacter* Passer,
	const AThirdPersonCppCharacter* RequestingHuman,
	ESoccerHumanPassRequestType RequestType
) const
{
	if (!IsValid(Passer) || !IsValid(RequestingHuman))
	{
		return FVector::ZeroVector;
	}

	const float LeadTime =
		RequestType == ESoccerHumanPassRequestType::AerialHeader
		? HumanPassRequestAerialLeadTime
		: HumanPassRequestNormalLeadTime;

	FVector HumanVelocity = RequestingHuman->GetVelocity();
	HumanVelocity.Z = 0.0f;

	FVector LeadOffset = HumanVelocity * FMath::Max(0.0f, LeadTime);

	const float MaximumLeadDistance =
		FMath::Max(0.0f, HumanPassRequestMaximumLeadDistance);

	if (
		MaximumLeadDistance > 0.0f &&
		LeadOffset.Size2D() > MaximumLeadDistance
	)
	{
		LeadOffset =
			LeadOffset.GetSafeNormal2D() * MaximumLeadDistance;
	}

	FVector TargetLocation =
		RequestingHuman->GetActorLocation() + LeadOffset;

	const float SafeFieldInset =
		FMath::Max(0.0f, HumanPassRequestFieldInset);

	if (IsValid(SoccerField))
	{
		TargetLocation =
			SoccerField->ClampWorldLocationInsidePitch(TargetLocation, SafeFieldInset);
	}
	else
	{
		TargetLocation = SoccerFieldDimensions::ClampLocationInsidePitch(
			TargetLocation,
			SafeFieldInset
		);
	}

	if (RequestType == ESoccerHumanPassRequestType::AerialHeader)
	{
		float GroundZ = RequestingHuman->GetActorLocation().Z - 96.0f;

		if (const UCapsuleComponent* Capsule =
			RequestingHuman->GetCapsuleComponent())
		{
			GroundZ =
				RequestingHuman->GetActorLocation().Z -
				Capsule->GetScaledCapsuleHalfHeight();
		}

		TargetLocation.Z =
			GroundZ +
			FMath::Clamp(
				HumanPassRequestAerialArrivalHeight,
				100.0f,
				260.0f
			);
	}
	else if (IsValid(SoccerBall))
	{
		TargetLocation.Z = SoccerBall->GetActorLocation().Z;
	}

	return TargetLocation;
}

bool ASoccerMatchManager::IsHumanRequestedPassSafe(
	const ASoccerAICharacter* Passer,
	const AThirdPersonCppCharacter* RequestingHuman,
	ESoccerHumanPassRequestType RequestType,
	const FVector& PassTargetLocation,
	float& OutSafetyScore,
	FString& OutRejectReason
) const
{
	OutSafetyScore = 0.0f;
	OutRejectReason.Reset();

	if (!IsValid(Passer) || !IsValid(RequestingHuman))
	{
		OutRejectReason = TEXT("solicitud invalida");
		return false;
	}

	if (PassTargetLocation.IsNearlyZero())
	{
		OutRejectReason = TEXT("no se pudo calcular el destino");
		return false;
	}

	if (!CanCharacterBePassReceiverNow(Passer, RequestingHuman))
	{
		OutRejectReason = TEXT("el humano no puede recibir ahora");
		return false;
	}

	const float PassDistance = FVector::Dist2D(
		Passer->GetActorLocation(),
		PassTargetLocation
	);

	const float MinimumDistance =
		FMath::Max(0.0f, HumanPassRequestMinDistance);

	const float MaximumDistance =
		FMath::Max(MinimumDistance + 1.0f, HumanPassRequestMaxDistance);

	if (PassDistance < MinimumDistance)
	{
		OutRejectReason = TEXT("el humano esta demasiado cerca");
		return false;
	}

	if (PassDistance > MaximumDistance)
	{
		OutRejectReason = TEXT("el humano esta demasiado lejos");
		return false;
	}

	const ESoccerTeam Team = Passer->GetTeam();

	const int32 OpponentsPressuringPasser =
		CountOpponentsAroundLocation(
			Team,
			Passer->GetActorLocation(),
			FMath::Max(0.0f, HumanPassRequestPasserPressureRadius)
		);

	if (OpponentsPressuringPasser > 0)
	{
		OutRejectReason =
			TEXT("el poseedor esta bajo presion inmediata");
		return false;
	}

	const int32 OpponentsAtCriticalReceiverDistance =
		CountOpponentsAroundLocation(
			Team,
			PassTargetLocation,
			FMath::Max(
				0.0f,
				HumanPassRequestCriticalReceiverPressureRadius
			)
		);

	if (OpponentsAtCriticalReceiverDistance > 0)
	{
		OutRejectReason =
			TEXT("un rival llegaria pegado al receptor");
		return false;
	}

	const int32 OpponentsNearReceiver =
		CountOpponentsAroundLocation(
			Team,
			PassTargetLocation,
			FMath::Max(0.0f, HumanPassRequestReceiverPressureRadius)
		);

	const float LaneHalfWidth =
		RequestType == ESoccerHumanPassRequestType::AerialHeader
		? HumanPassRequestAerialLaneHalfWidth
		: HumanPassRequestNormalLaneHalfWidth;

	const bool bLaneBlocked =
		IsOpponentBlockingLaneBetweenLocations(
			Team,
			Passer->GetActorLocation(),
			PassTargetLocation,
			FMath::Max(0.0f, LaneHalfWidth)
		);

	/*
	 * A normal pass cannot safely cross a clearly occupied ground lane.
	 * An aerial request may still clear a narrow blocker, so it receives a
	 * penalty instead of an automatic rejection.
	 */
	if (
		bLaneBlocked &&
		RequestType == ESoccerHumanPassRequestType::Normal
	)
	{
		OutRejectReason = TEXT("la linea del pase esta bloqueada");
		return false;
	}

	const float DistanceAlpha = FMath::Clamp(
		(PassDistance - MinimumDistance) /
		FMath::Max(1.0f, MaximumDistance - MinimumDistance),
		0.0f,
		1.0f
	);

	float SafetyScore = 1.0f;
	SafetyScore -= OpponentsNearReceiver * 0.22f;
	SafetyScore -= DistanceAlpha * 0.12f;

	if (bLaneBlocked)
	{
		SafetyScore -= 0.22f;
	}

	SafetyScore = FMath::Clamp(SafetyScore, 0.0f, 1.0f);
	OutSafetyScore = SafetyScore;

	const float RequiredSafetyScore =
		RequestType == ESoccerHumanPassRequestType::AerialHeader
		? HumanPassRequestAerialMinimumSafetyScore
		: HumanPassRequestNormalMinimumSafetyScore;

	if (SafetyScore < FMath::Clamp(RequiredSafetyScore, 0.0f, 1.0f))
	{
		OutRejectReason = TEXT("el riesgo de intercepcion es demasiado alto");
		return false;
	}

	return true;
}

bool ASoccerMatchManager::ToggleHumanPassRequest(
	AThirdPersonCppCharacter* RequestingHuman,
	ESoccerHumanPassRequestType RequestType
)
{
	if (!bEnableHumanPassRequests || !IsValid(RequestingHuman))
	{
		return false;
	}

	if (
		RequestType != ESoccerHumanPassRequestType::Normal &&
		RequestType != ESoccerHumanPassRequestType::AerialHeader
	)
	{
		return false;
	}

	// Pressing the already active key toggles the request off.
	if (
		ActiveHumanPassRequestingHuman == RequestingHuman &&
		ActiveHumanPassRequestType == RequestType
	)
	{
		const TCHAR* TypeText =
			RequestType == ESoccerHumanPassRequestType::AerialHeader
			? TEXT("aereo")
			: TEXT("a los pies");

		ClearActiveHumanPassRequest(
			FString::Printf(
				TEXT("Pedido %s desactivado"),
				TypeText
			),
			FLinearColor(0.68f, 0.68f, 0.68f, 1.0f),
			true
		);
		return true;
	}

	if (RequestingHuman->HasHumanLogicalBallControl())
	{
		ClearActiveHumanPassRequest(
			TEXT("No puedes pedir un pase mientras posees la pelota"),
			FLinearColor(1.0f, 0.65f, 0.15f, 1.0f),
			true
		);
		return false;
	}

	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return false;
	}

	ActiveHumanPassRequestType = RequestType;
	ActiveHumanPassRequestingHuman = RequestingHuman;
	LastHumanPassRequestInputTime = World->GetTimeSeconds();
	LastHumanPassRequestEvaluationTime = -1000.0f;
	LastHumanPassRequestEvaluatedPasser = nullptr;

	const TCHAR* TypeText =
		RequestType == ESoccerHumanPassRequestType::AerialHeader
		? TEXT("AEREO")
		: TEXT("A LOS PIES");

	ShowHumanPassRequestDebugMessage(
		FString::Printf(
			TEXT("Pedido %s activo"),
			TypeText
		),
		RequestType == ESoccerHumanPassRequestType::AerialHeader
		? FColor(255, 205, 75)
		: FColor(80, 210, 255)
	);

	// Respond immediately when a teammate already has physical possession.
	if (ASoccerAICharacter* Passer =
		FindCurrentHumanPassRequestPasser(RequestingHuman))
	{
		TryExecuteActiveHumanPassRequest(Passer);
	}
	else
	{
		ShowHumanPassRequestDebugMessage(
			FString::Printf(
				TEXT("Pedido %s activo: esperando posesion de un companero"),
				TypeText
			),
			FColor(180, 210, 255)
		);
	}

	return true;
}

bool ASoccerMatchManager::TryExecuteActiveHumanPassRequest(
	ASoccerAICharacter* Passer
)
{
	if (!HasActiveHumanPassRequest() || !IsValid(Passer))
	{
		return false;
	}

	AThirdPersonCppCharacter* RequestingHuman =
		ActiveHumanPassRequestingHuman;

	if (!IsValid(RequestingHuman))
	{
		ClearActiveHumanPassRequest(
			TEXT("Pedido de pase desactivado"),
			FLinearColor(0.68f, 0.68f, 0.68f, 1.0f),
			false
		);
		return false;
	}

	if (
		MatchPlayState != ESoccerMatchPlayState::Playing ||
		IsRestartContextActive() ||
		!IsValid(SoccerBall) ||
		!Passer->IsAIPossessingBall() ||
		Passer->GetTeam() != RequestingHuman->GetTeam()
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
	const float EvaluationCooldown =
		FMath::Max(0.0f, HumanPassRequestCooldown);

	if (
		LastHumanPassRequestEvaluatedPasser == Passer &&
		CurrentTime - LastHumanPassRequestEvaluationTime < EvaluationCooldown
	)
	{
		return false;
	}

	LastHumanPassRequestEvaluatedPasser = Passer;
	LastHumanPassRequestEvaluationTime = CurrentTime;

	if (
		Passer->GetPlayerRole() == ESoccerPlayerRole::Goalkeeper ||
		Passer->IsGoalkeeperHoldingBall() ||
		Passer->IsGoalkeeperActionActive() ||
		Passer->IsAIDribbleTurnAutoPassActive() ||
		Passer->IsAerialActionLocked()
	)
	{
		ShowHumanPassRequestDebugMessage(
			TEXT("Pedido activo: el companero no puede pasar ahora"),
			FColor::Orange
		);
		return false;
	}

	const ESoccerHumanPassRequestType RequestType =
		ActiveHumanPassRequestType;

	const FVector PassTargetLocation =
		BuildHumanRequestedPassTargetLocation(
			Passer,
			RequestingHuman,
			RequestType
		);

	float SafetyScore = 0.0f;
	FString RejectReason;

	if (!IsHumanRequestedPassSafe(
		Passer,
		RequestingHuman,
		RequestType,
		PassTargetLocation,
		SafetyScore,
		RejectReason
	))
	{
		ShowHumanPassRequestDebugMessage(
			FString::Printf(
				TEXT("Pedido activo, esperando: %s"),
				*RejectReason
			),
			FColor::Orange
		);
		return false;
	}

	if (!TryRegisterIntentionalBallTouch(Passer))
	{
		ShowHumanPassRequestDebugMessage(
			TEXT("Pedido activo: una regla del partido impide el pase"),
			FColor::Red
		);
		return false;
	}

	StartAttackRunReleaseForTeam(Passer->GetTeam());

	if (RequestType == ESoccerHumanPassRequestType::AerialHeader)
	{
		Passer->KickAIBallToAirTarget(
			PassTargetLocation,
			HumanPassRequestAerialHorizontalSpeed,
			HumanPassRequestAerialMinTravelTime,
			HumanPassRequestAerialMaxTravelTime
		);
	}
	else
	{
		Passer->KickAIBallToTarget(
			PassTargetLocation,
			HumanPassRequestNormalHorizontalSpeed,
			HumanPassRequestNormalMinTravelTime,
			HumanPassRequestNormalMaxTravelTime
		);
	}

	RegisterOpenPlayPassIntent(
		Passer,
		RequestingHuman,
		PassTargetLocation
	);


	if (ASoccerAIController* PasserController =
		Cast<ASoccerAIController>(Passer->GetController()))
	{
		PasserController->StopMovement();
	}

	const TCHAR* PassTypeText =
		RequestType == ESoccerHumanPassRequestType::AerialHeader
		? TEXT("aereo")
		: TEXT("a los pies");

	ShowHumanPassRequestDebugMessage(
		FString::Printf(
			TEXT("Pase %s enviado | seguridad %.2f"),
			PassTypeText,
			SafetyScore
		),
		FColor::Green
	);

	return true;
}

	bool ASoccerMatchManager::ShouldBallCarrierPreferShot(
		const ASoccerAICharacter* BallCarrier
	) const
	{
		if (!IsValid(BallCarrier))
		{
			return false;
		}

		const ESoccerTeam Team =
			BallCarrier->GetTeam();

		const float CarrierDepthAlpha =
			GetAttackDepthAlphaForLocation(
				BallCarrier->GetActorLocation(),
				Team
			);

		if (CarrierDepthAlpha < AttackDecisionShotMinDepthAlpha)
		{
			return false;
		}

		const FVector CarrierLocation =
			BallCarrier->GetActorLocation();

		const FVector ShotTarget =
			GetOpponentGoalReferenceLocation(Team);

		const bool bShotLaneBlocked =
			IsOpponentBlockingLaneBetweenLocations(
				Team,
				CarrierLocation,
				ShotTarget,
				AttackDecisionShotLaneHalfWidth
			);

		if (bShotLaneBlocked)
		{
			return false;
		}

		const int32 PressureCount =
			CountOpponentPressureAroundCarrier(
				BallCarrier,
				AttackCarrierPressureRadius
			);

		if (PressureCount >= 2)
		{
			return false;
		}

		return true;
	}

void ASoccerMatchManager::StartAttackRunReleaseForTeam(
	ESoccerTeam Team
)
{
	bAttackRunReleaseActive = true;
	AttackRunReleaseTeam = Team;

	AttackRunReleaseStartTime =
		GetWorld() != nullptr
		? GetWorld()->GetTimeSeconds()
		: 0.0f;
}

void ASoccerMatchManager::ClearAttackRunRelease()
{
	bAttackRunReleaseActive = false;
	AttackRunReleaseStartTime = -1000.0f;
}

bool ASoccerMatchManager::IsAttackRunReleaseActiveForTeam(
	ESoccerTeam Team
) const
{
	if (!bAttackRunReleaseActive)
	{
		return false;
	}

	if (AttackRunReleaseTeam != Team)
	{
		return false;
	}

	const UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return false;
	}

	const float CurrentTime =
		World->GetTimeSeconds();

	return CurrentTime - AttackRunReleaseStartTime <= AttackRunReleaseDuration;
}

bool ASoccerMatchManager::ShouldApplyOffsideSafetyToAttackOrder(
	ESoccerAIOrder CurrentOrder
) const
{
	return
		CurrentOrder == ESoccerAIOrder::AttackSupportShort ||
		CurrentOrder == ESoccerAIOrder::AttackSupportForward ||
		CurrentOrder == ESoccerAIOrder::AttackRunIntoSpace ||
		CurrentOrder == ESoccerAIOrder::AttackWideSupport;
}

bool ASoccerMatchManager::TryGetAttackFieldFrame(
	ESoccerTeam AttackingTeam,
	FVector& OutOwnGoalLocation,
	FVector& OutAttackDirection,
	float& OutFieldLength
) const
{
	OutOwnGoalLocation =
		GetOwnGoalReferenceLocation(AttackingTeam);

	const FVector OpponentGoalLocation =
		GetOwnGoalReferenceLocation(GetOppositeTeam(AttackingTeam));

	OutAttackDirection =
		OpponentGoalLocation - OutOwnGoalLocation;

	OutAttackDirection.Z = 0.0f;

	OutFieldLength =
		OutAttackDirection.Size2D();

	if (OutFieldLength <= KINDA_SMALL_NUMBER)
	{
		OutAttackDirection = FVector::ZeroVector;
		OutFieldLength = 0.0f;
		return false;
	}

	OutAttackDirection /= OutFieldLength;

	return true;
}

bool ASoccerMatchManager::TryGetOffsideSafeProgressForTeam(
	ESoccerTeam AttackingTeam,
	float& OutSafeProgress,
	FVector& OutOwnGoalLocation,
	FVector& OutAttackDirection
) const
{
	float FieldLength = 0.0f;

	if (
		!TryGetAttackFieldFrame(
			AttackingTeam,
			OutOwnGoalLocation,
			OutAttackDirection,
			FieldLength
		)
		)
	{
		return false;
	}

	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return false;
	}

	const ESoccerTeam DefendingTeam =
		GetOppositeTeam(AttackingTeam);

	bool bFoundFieldDefender = false;
	float LastFieldDefenderProgress =
		-TNumericLimits<float>::Max();

	for (TActorIterator<ASoccerCharacterBase> It(World); It; ++It)
	{
		const ASoccerCharacterBase* Candidate = *It;

		if (!IsValid(Candidate))
		{
			continue;
		}

		if (Candidate->GetTeam() != DefendingTeam)
		{
			continue;
		}

		// Usamos "último jugador de campo", no arquero.
		if (Candidate->GetPlayerRole() == ESoccerPlayerRole::Goalkeeper)
		{
			continue;
		}

		const float CandidateProgress =
			FVector::DotProduct(
				Candidate->GetActorLocation() - OutOwnGoalLocation,
				OutAttackDirection
			);

		if (
			!bFoundFieldDefender ||
			CandidateProgress > LastFieldDefenderProgress
			)
		{
			bFoundFieldDefender = true;
			LastFieldDefenderProgress = CandidateProgress;
		}
	}

	if (!bFoundFieldDefender)
	{
		return false;
	}

	const float MidfieldProgress =
		FieldLength * 0.5f;

	const float SafeProgressBehindDefender =
		LastFieldDefenderProgress -
		OffsideSafetyDistanceBehindLastFieldPlayer;

	// Nunca obligamos a los bots a bajar más allá de mitad de cancha
	// por una línea defensiva demasiado alta. En mitad propia no hay offside.
	OutSafeProgress =
		FMath::Max(
			MidfieldProgress,
			SafeProgressBehindDefender
		);

	return true;
}

FVector ASoccerMatchManager::ApplyOffsideSafetyToAttackMoveLocation(
	const ASoccerAICharacter* SoccerAICharacter,
	const FVector& DesiredLocation,
	ESoccerAIOrder CurrentOrder
) const
{
	if (!bEnableOffsideAwareAttackPositioning)
	{
		return DesiredLocation;
	}

	if (!IsValid(SoccerAICharacter))
	{
		return DesiredLocation;
	}

	if (!ShouldApplyOffsideSafetyToAttackOrder(CurrentOrder))
	{
		return DesiredLocation;
	}

	const ESoccerTeam AttackingTeam =
		SoccerAICharacter->GetTeam();

	// Cuando la pelota ya fue jugada, permitimos que los atacantes
	// rompan la línea durante una ventana corta.
	if (
		IsAttackRunReleaseActiveForTeam(AttackingTeam) &&
		(
			CurrentOrder == ESoccerAIOrder::AttackRunIntoSpace ||
			CurrentOrder == ESoccerAIOrder::AttackSupportForward ||
			CurrentOrder == ESoccerAIOrder::AttackWideSupport
			)
		)
	{
		return DesiredLocation;
	}

	float SafeProgress = 0.0f;
	FVector OwnGoalLocation = FVector::ZeroVector;
	FVector AttackDirection = FVector::ZeroVector;

	if (
		!TryGetOffsideSafeProgressForTeam(
			AttackingTeam,
			SafeProgress,
			OwnGoalLocation,
			AttackDirection
		)
		)
	{
		return DesiredLocation;
	}

	const float DesiredProgress =
		FVector::DotProduct(
			DesiredLocation - OwnGoalLocation,
			AttackDirection
		);

	if (DesiredProgress <= SafeProgress)
	{
		return DesiredLocation;
	}

	FVector CorrectedLocation =
		DesiredLocation -
		AttackDirection * (DesiredProgress - SafeProgress);

	CorrectedLocation.Z =
		DesiredLocation.Z;

	return CorrectedLocation;
}

bool ASoccerMatchManager::ShouldDebugFreezeAICharacter(
	const ASoccerAICharacter* SoccerAICharacter
) const
{
	if (!IsValid(SoccerAICharacter))
	{
		return false;
	}

	const bool bIsGoalkeeper =
		SoccerAICharacter->GetPlayerRole() ==
		ESoccerPlayerRole::Goalkeeper;

	const bool bShouldFreezeByTeam =
		(
			bDebugFreezeOpponentTeam &&
			SoccerAICharacter->GetTeam() == ESoccerTeam::OpponentTeam
			)
		||
		(
			bDebugFreezePlayerTeam &&
			SoccerAICharacter->GetTeam() == ESoccerTeam::PlayerTeam
			);

	const bool bShouldFreezeByFieldPlayerDebug =
		bDebugFreezeAllFieldPlayers &&
		!bIsGoalkeeper;

	const bool bShouldFreezeThisAI =
		bShouldFreezeByFieldPlayerDebug ||
		(
			bShouldFreezeByTeam &&
			(
				!bIsGoalkeeper ||
				!bDebugKeepGoalkeepersActiveWhenFreezing
				)
			);

	return bShouldFreezeThisAI;
}

bool ASoccerMatchManager::ShouldDebugFrozenAIReleaseBall() const
{
	return bDebugFrozenTeamReleaseBall;
}
