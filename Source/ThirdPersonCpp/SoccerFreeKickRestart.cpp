#include "SoccerFreeKickRestart.h"

#include "SoccerMatchManager.h"
#include "SoccerAICharacter.h"
#include "SoccerCharacterBase.h"
#include "ThirdPersonCppCharacter.h"
#include "SoccerBall.h"
#include "SoccerField.h"
#include "SoccerFieldDimensions.h"
#include "SoccerDebugManager.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"

bool FSoccerFreeKickRestart::IsSupportedType(ESoccerRestartType InRestartType) const
{
	return
		InRestartType == ESoccerRestartType::OffsideFreeKick ||
		InRestartType == ESoccerRestartType::DirectFreeKick;
}

bool FSoccerFreeKickRestart::IsActive(const ASoccerMatchManager& Manager) const
{
	return
		IsSupportedType(RestartType) &&
		(
			Manager.MatchPlayState == ESoccerMatchPlayState::OffsideRestartSetup ||
			Manager.MatchPlayState == ESoccerMatchPlayState::OffsideRestartTaking
		);
}

bool FSoccerFreeKickRestart::IsTaker(
	const ASoccerMatchManager& Manager,
	const ASoccerAICharacter* SoccerAICharacter
) const
{
	return
		IsActive(Manager) &&
		!bHumanTakerClaimed &&
		IsValid(SoccerAICharacter) &&
		SoccerAICharacter == TakerAI;
}

bool FSoccerFreeKickRestart::IsHumanTaker(
	const ASoccerMatchManager& Manager,
	const AThirdPersonCppCharacter* HumanCharacter
) const
{
	return
		IsActive(Manager) &&
		bHumanTakerClaimed &&
		IsValid(HumanTaker) &&
		IsValid(HumanCharacter) &&
		HumanCharacter == HumanTaker;
}

bool FSoccerFreeKickRestart::CanHumanTakerExecute(
	const ASoccerMatchManager& Manager,
	const AThirdPersonCppCharacter* HumanCharacter
) const
{
	return
		IsHumanTaker(Manager, HumanCharacter) &&
		bHumanExecutionAuthorized &&
		!bFinalRunActive &&
		Manager.MatchPlayState == ESoccerMatchPlayState::OffsideRestartTaking;
}

bool FSoccerFreeKickRestart::IsFinalRunActiveForCharacter(
	const ASoccerMatchManager& Manager,
	const ASoccerAICharacter* SoccerAICharacter
) const
{
	return
		bFinalRunActive &&
		Manager.MatchPlayState == ESoccerMatchPlayState::OffsideRestartTaking &&
		IsValid(SoccerAICharacter) &&
		SoccerAICharacter == TakerAI;
}

ASoccerAICharacter* FSoccerFreeKickRestart::FindClosestTakerForTeam(
	ASoccerMatchManager& Manager,
	ESoccerTeam Team,
	const FVector& InRestartLocation
) const
{
	UWorld* World = Manager.GetWorld();
	if (World == nullptr)
	{
		return nullptr;
	}

	ASoccerAICharacter* BestCharacter = nullptr;
	float BestDistance = TNumericLimits<float>::Max();

	for (TActorIterator<ASoccerAICharacter> It(World); It; ++It)
	{
		ASoccerAICharacter* Candidate = *It;
		if (!IsValid(Candidate) || Candidate->GetTeam() != Team)
		{
			continue;
		}

		const ESoccerPlayerRole PlayerRole = Candidate->GetPlayerRole();
		if (PlayerRole == ESoccerPlayerRole::Goalkeeper)
		{
			continue;
		}

		const float Distance = FVector::Dist2D(
			Candidate->GetActorLocation(),
			InRestartLocation
		);

		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			BestCharacter = Candidate;
		}
	}

	return BestCharacter;
}

ASoccerAICharacter* FSoccerFreeKickRestart::FindBestReceiverForTeam(
	ASoccerMatchManager& Manager,
	ESoccerTeam Team,
	const ASoccerAICharacter* RestartTaker,
	const FVector& InRestartLocation
) const
{
	UWorld* World = Manager.GetWorld();
	if (World == nullptr)
	{
		return nullptr;
	}

	ASoccerAICharacter* BestCharacter = nullptr;
	float BestScore = -TNumericLimits<float>::Max();

	const FVector OpponentGoalLocation =
		Manager.GetOwnGoalReferenceLocation(Manager.GetOppositeTeam(Team));

	for (TActorIterator<ASoccerAICharacter> It(World); It; ++It)
	{
		ASoccerAICharacter* Candidate = *It;
		if (
			!IsValid(Candidate) ||
			Candidate->GetTeam() != Team ||
			Candidate == RestartTaker
		)
		{
			continue;
		}

		const ESoccerPlayerRole PlayerRole = Candidate->GetPlayerRole();
		if (PlayerRole == ESoccerPlayerRole::Goalkeeper)
		{
			continue;
		}

		const float DistanceToRestart = FVector::Dist2D(
			Candidate->GetActorLocation(),
			InRestartLocation
		);

		const float DistanceToOpponentGoal = FVector::Dist2D(
			Candidate->GetActorLocation(),
			OpponentGoalLocation
		);

		float Score =
			3000.0f -
			DistanceToRestart * 0.45f -
			DistanceToOpponentGoal * 0.12f;

		if (PlayerRole == ESoccerPlayerRole::Midfielder)
		{
			Score += 450.0f;
		}
		else if (PlayerRole == ESoccerPlayerRole::Forward)
		{
			Score += 280.0f;
		}

		if (Score > BestScore)
		{
			BestScore = Score;
			BestCharacter = Candidate;
		}
	}

	return BestCharacter;
}

bool FSoccerFreeKickRestart::IsRuntimeValid() const
{
	return IsValid(TakerAI) && IsValid(ReceiverAI);
}

bool FSoccerFreeKickRestart::Configure(
	ASoccerMatchManager& Manager,
	ESoccerRestartType InRestartType,
	ESoccerTeam InRestartTeam,
	const FVector& InRestartLocation
)
{
	if (!IsSupportedType(InRestartType))
	{
		return false;
	}

	UWorld* World = Manager.GetWorld();
	if (World == nullptr)
	{
		return false;
	}

	ResetRuntime(Manager);

	RestartType = InRestartType;
	RestartTeam = InRestartTeam;

	// The reported infringement point may be a body-contact point or inherit
	// the height of an airborne ball.  A restart only keeps its horizontal
	// position: the ball centre must rest one ball radius above the pitch.
	RestartLocation = InRestartLocation;
	const float BallRadius = IsValid(Manager.SoccerBall)
		? Manager.SoccerBall->GetBallRadiusCm()
		: 11.0f;

	if (IsValid(Manager.SoccerField))
	{
		FVector LocalRestartLocation =
			Manager.SoccerField->WorldToPitchLocal(RestartLocation);
		LocalRestartLocation.Z = BallRadius;
		RestartLocation =
			Manager.SoccerField->PitchLocalToWorld(LocalRestartLocation);
	}
	else
	{
		RestartLocation.Z = BallRadius;
	}
	SetupStartTime = World->GetTimeSeconds();

	TakerAI = FindClosestTakerForTeam(Manager, RestartTeam, RestartLocation);
	HumanTaker = Manager.FindHumanCharacterForTeam(RestartTeam);
	ReceiverAI = FindBestReceiverForTeam(
		Manager,
		RestartTeam,
		TakerAI,
		RestartLocation
	);

	if (!IsRuntimeValid())
	{
		ResetRuntime(Manager);
		return false;
	}

	return true;
}

bool FSoccerFreeKickRestart::EnterPreparation(ASoccerMatchManager& Manager)
{
	if (!IsRuntimeValid() || !IsValid(Manager.SoccerBall))
	{
		return false;
	}

	UWorld* World = Manager.GetWorld();
	if (World == nullptr)
	{
		return false;
	}

	SetupStartTime = World->GetTimeSeconds();
	Manager.PossessingCharacter = nullptr;
	Manager.PossessionTeam = ESoccerPossessionTeam::None;

	Manager.BeginRestartContext(RestartType, RestartTeam, RestartLocation);

	Manager.SoccerBall->SetPossessed(false);
	Manager.SoccerBall->StopBallKeepingPhysics();
	Manager.SoccerBall->SetActorLocation(
		RestartLocation,
		false,
		nullptr,
		ETeleportType::TeleportPhysics
	);

	// Freeze the attacking plan once for this restart. The receiver identity was
	// already selected in Configure(); here we also snapshot where it waits and
	// where the taker waits while opponents complete the required clearance.
	ReceiverHoldLocation = BuildReceiverDesiredMoveLocation(Manager);
	InitializeOpponentPositioningPlan(Manager);

	Manager.MatchPlayState = ESoccerMatchPlayState::OffsideRestartSetup;
	UpdateHumanTakerClaimDuringPreparation(Manager);
	RecalculateRunUpGeometry(Manager);

	TakerWaitingLocation = BuildTakerWaitingLocation(Manager, TakerAI);
	Manager.CaptureActiveRestartAITargetLocations(false);
	return true;
}

bool FSoccerFreeKickRestart::IsPreparationReady(ASoccerMatchManager& Manager)
{
	if (!IsRuntimeValid())
	{
		return false;
	}

	// The human may claim or release the restart while everybody is positioning.
	// Any change invalidates the current ready hold because the fallback taker
	// receives a different target.
	if (UpdateHumanTakerClaimDuringPreparation(Manager))
	{
		RecalculateRunUpGeometry(Manager);
		Manager.CaptureActiveRestartAITargetLocations(
			AreOpponentsClear(Manager)
		);
		return false;
	}

	return Manager.UpdateActiveRestartReadiness(
		SetupStartTime,
		Manager.OffsideRestartMinSetupTime
	);
}

bool FSoccerFreeKickRestart::EnterExecution(ASoccerMatchManager& Manager)
{
	if (!IsRuntimeValid() || !IsValid(Manager.SoccerBall))
	{
		return false;
	}

	// If the human still owns the restart when Preparation finishes, the whistle
	// opens a genuine human execution window. The fallback AI stays parked until
	// the human either kicks or leaves the release radius.
	if (
		bHumanTakerClaimed &&
		IsValid(HumanTaker)
	)
	{
		bFinalRunActive = false;
		bHumanExecutionAuthorized = true;
		Manager.ResetRestartKickContactTracking(ContactTracker);
		Manager.MatchPlayState = ESoccerMatchPlayState::OffsideRestartTaking;
		Manager.ResetActiveRestartReadyHold();

		ASoccerDebugManager::Message(
			&Manager,
			ESoccerDebugCategory::Restarts,
			TEXT("TIRO LIBRE: humano habilitado para ejecutar"),
			FColor::Cyan
		);

		return true;
	}

	bHumanExecutionAuthorized = false;
	BeginFinalRun(Manager);
	return bFinalRunActive;
}

bool FSoccerFreeKickRestart::ShouldKeepBallFixedDuringExecution() const
{
	if (
		bAIKickMontageStarted &&
		IsValid(TakerAI) &&
		TakerAI->HasAIKickMontageImpactedBall()
		)
	{
		return false;
	}

	return true;
}

bool FSoccerFreeKickRestart::TickExecutionAndCompleteIfNeeded(ASoccerMatchManager& Manager)
{
	if (!IsRuntimeValid())
	{
		return true;
	}

	if (bHumanExecutionAuthorized)
	{
		if (ShouldHumanKeepExecutionClaim(Manager))
		{
			return false;
		}

		bHumanTakerClaimed = false;
		bHumanExecutionAuthorized = false;
		RecalculateRunUpGeometry(Manager);
		Manager.CaptureActiveRestartAITargetLocations(
			AreOpponentsClear(Manager)
		);

		ASoccerDebugManager::Message(
			&Manager,
			ESoccerDebugCategory::Restarts,
			TEXT("TIRO LIBRE: humano se alejo, ejecutor bot reasignado"),
			FColor::Yellow
		);
	}

	// Once the authored restart montage has started, contact geometry no longer
	// owns the kick. Wait for the timed foot impact, then complete the match-state
	// bookkeeping without launching the ball a second time.
	if (bAIKickMontageStarted)
	{
		if (IsValid(TakerAI) && TakerAI->HasAIKickMontageImpactedBall())
		{
			Complete(Manager);
			return !IsRuntimeValid();
		}

		if (IsValid(TakerAI) && TakerAI->IsAIKickMontageActive())
		{
			return false;
		}

		// The montage was interrupted before impact. Return to the normal run-up
		// recovery instead of leaving the restart permanently inactive.
		bAIKickMontageStarted = false;
		PendingAIKickTargetLocation = FVector::ZeroVector;
		RecoverFinalRunAfterMiss(Manager);
		return false;
	}

	if (!bFinalRunActive)
	{
		if (Manager.UpdateActiveRestartReadiness(0.0f, 0.0f))
		{
			BeginFinalRun(Manager);
		}
		return false;
	}

	if (!IsTakerAtBallContact(Manager))
	{
		return false;
	}

	Complete(Manager);
	return !IsRuntimeValid();
}

void FSoccerFreeKickRestart::Complete(ASoccerMatchManager& Manager)
{
	Manager.DestroyActiveRestartHumanRestrictionIndicator();

	if (!IsValid(Manager.SoccerBall) || !IsValid(TakerAI) || !IsValid(ReceiverAI))
	{
		ResetRuntime(Manager);
		Manager.EndRestartContext();
		Manager.MatchPlayState = ESoccerMatchPlayState::Playing;
		Manager.ClearAssignedAI();
		return;
	}

	ASoccerAICharacter* CompletedTaker = TakerAI;
	ASoccerAICharacter* CompletedReceiver = ReceiverAI;

	FVector PassTargetLocation =
		bAIKickMontageStarted
		? PendingAIKickTargetLocation
		: Manager.GetActiveRestartExecutionTargetLocation(
			CompletedReceiver->GetActorLocation()
		);
	PassTargetLocation.Z = Manager.SoccerBall->GetActorLocation().Z;

	// First contact with the stationary ball now starts the authored montage.
	// The restart remains active until the montage's timed impact actually moves
	// the ball. Missing/unplayable montages keep the old immediate fallback.
	if (!bAIKickMontageStarted)
	{
		if (CompletedTaker->StartAIKickMontageForRestart(
			Manager.SoccerBall,
			PassTargetLocation,
			Manager.OffsideRestartPassHorizontalSpeed,
			Manager.OffsideRestartPassMinTravelTime,
			Manager.OffsideRestartPassMaxTravelTime,
			false
		))
		{
			bAIKickMontageStarted = true;
			PendingAIKickTargetLocation = CompletedTaker->GetPendingAIKickTarget();
			bFinalRunActive = false;
			Manager.ResetRestartKickContactTracking(ContactTracker);
			return;
		}
	}

	const bool bMontageImpactAlreadyLaunchedBall =
		bAIKickMontageStarted &&
		CompletedTaker->HasAIKickMontageImpactedBall();

	if (bAIKickMontageStarted && !bMontageImpactAlreadyLaunchedBall)
	{
		return;
	}

	const ESoccerRestartType CompletedRestartType = RestartType;
	ASoccerCharacterBase* CompletedExecutionReceiver =
		Manager.GetActiveRestartExecutionReceiver();
	const bool bPassWasIntendedForHuman =
		Manager.IsActiveRestartExecutionReceiverHuman();

	Manager.MatchPlayState = ESoccerMatchPlayState::Playing;

	if (!Manager.TryRegisterIntentionalBallTouch(CompletedTaker))
	{
		ResetRuntime(Manager);
		Manager.EndRestartContext();
		Manager.ClearAssignedAI();
		return;
	}

	Manager.EndRestartContext();
	Manager.StartNoRetouchRestriction(CompletedTaker);
	Manager.StartAttackRunReleaseForTeam(CompletedTaker->GetTeam());

	if (!bMontageImpactAlreadyLaunchedBall)
	{
		CompletedTaker->PlayAIKickAnimationForRestart();
		Manager.SoccerBall->KickToTarget(
			PassTargetLocation,
			Manager.OffsideRestartPassHorizontalSpeed,
			Manager.OffsideRestartPassMinTravelTime,
			Manager.OffsideRestartPassMaxTravelTime
		);
	}

	ResetRuntime(Manager);
	Manager.ClearAssignedAI();

	if (bPassWasIntendedForHuman && IsValid(CompletedExecutionReceiver))
	{
		Manager.RegisterOpenPlayPassIntent(
			CompletedTaker,
			CompletedExecutionReceiver,
			PassTargetLocation
		);
	}

	ASoccerDebugManager::Message(
		&Manager,
		ESoccerDebugCategory::Restarts,
		CompletedRestartType == ESoccerRestartType::DirectFreeKick
			? TEXT("Tiro libre directo realizado")
			: TEXT("Tiro libre por offside realizado"),
		FColor::Green
	);
}

FVector FSoccerFreeKickRestart::BuildReceiverMoveLocation(const ASoccerMatchManager& Manager) const
{
	if (!ReceiverHoldLocation.IsNearlyZero())
	{
		return ReceiverHoldLocation;
	}

	return BuildReceiverDesiredMoveLocation(Manager);
}

FVector FSoccerFreeKickRestart::BuildReceiverDesiredMoveLocation(const ASoccerMatchManager& Manager) const
{
	if (!IsValid(ReceiverAI))
	{
		return RestartLocation;
	}

	FVector OwnGoalLocation = FVector::ZeroVector;
	FVector AttackDirection = FVector::ZeroVector;
	float FieldLength = 0.0f;

	if (!Manager.TryGetAttackFieldFrame(RestartTeam, OwnGoalLocation, AttackDirection, FieldLength))
	{
		return ReceiverAI->GetActorLocation();
	}

	FVector LateralDirection = FVector::CrossProduct(FVector::UpVector, AttackDirection);
	LateralDirection.Z = 0.0f;
	if (!LateralDirection.Normalize())
	{
		LateralDirection = FVector::RightVector;
	}

	const float ReceiverSide = ReceiverAI->GetActorLocation().Y >= RestartLocation.Y ? 1.0f : -1.0f;

	FVector ReceiverLocation =
		RestartLocation +
		AttackDirection * Manager.OffsideRestartReceiverForwardDistance +
		LateralDirection * ReceiverSide * Manager.OffsideRestartReceiverLateralDistance;
	ReceiverLocation.Z = RestartLocation.Z;

	const ESoccerAIOrder ReceiverOrder =
		Manager.GetDefaultRestartAttackOrderForCharacter(ReceiverAI);

	ReceiverLocation = Manager.ApplyOffsideSafetyToAttackMoveLocation(
		ReceiverAI,
		ReceiverLocation,
		ReceiverOrder
	);

	return Manager.ProjectLocationToNavigation(ReceiverLocation, ReceiverAI);
}

bool FSoccerFreeKickRestart::IsCharacterTooClose(
	const ASoccerMatchManager& Manager,
	const ASoccerCharacterBase* Character
) const
{
	if (
		!IsActive(Manager) ||
		!IsValid(Character) ||
		Character->GetTeam() == RestartTeam
	)
	{
		return false;
	}

	const ASoccerAICharacter* OpponentAI =
		Cast<const ASoccerAICharacter>(Character);

	if (
		IsValid(OpponentAI) &&
		OpponentAI->GetPlayerRole() == ESoccerPlayerRole::Goalkeeper
	)
	{
		return false;
	}

	// Keep the exact regulatory radius for the human. Bots get a small
	// additional clearance band so legality cannot chatter when path following
	// stops them a few centimetres either side of the circle boundary.
	float RequiredDistance =
		FMath::Max(0.0f, Manager.OffsideRestartOpponentRequiredDistance);

	if (Cast<const ASoccerAICharacter>(Character) != nullptr)
	{
		RequiredDistance +=
			FMath::Max(0.0f, Manager.OffsideRestartOpponentLegalBuffer);
	}

	return FVector::Dist2D(
		Character->GetActorLocation(),
		RestartLocation
	) < RequiredDistance;
}

bool FSoccerFreeKickRestart::AreOpponentsClear(const ASoccerMatchManager& Manager) const
{
	if (!IsActive(Manager))
	{
		return true;
	}

	UWorld* World = Manager.GetWorld();
	if (World == nullptr)
	{
		return false;
	}

	for (TActorIterator<ASoccerCharacterBase> It(World); It; ++It)
	{
		const ASoccerCharacterBase* Candidate = *It;

		if (IsCharacterTooClose(Manager, Candidate))
		{
			return false;
		}
	}

	return true;
}

bool FSoccerFreeKickRestart::ShouldDefendingCharacterFaceBall(
	const ASoccerAICharacter* SoccerAICharacter
) const
{
	if (
		!bDefensiveWallActive ||
		RestartType != ESoccerRestartType::DirectFreeKick ||
		!IsValid(SoccerAICharacter) ||
		SoccerAICharacter->GetTeam() == RestartTeam
	)
	{
		return false;
	}

	return
		SoccerAICharacter->GetPlayerRole() == ESoccerPlayerRole::Goalkeeper ||
		DefensiveWallMembers.Contains(SoccerAICharacter);
}

bool FSoccerFreeKickRestart::IsDefensiveWallMember(
	const ASoccerAICharacter* SoccerAICharacter
) const
{
	return
		bDefensiveWallActive &&
		IsValid(SoccerAICharacter) &&
		DefensiveWallMembers.Contains(SoccerAICharacter);
}

FVector FSoccerFreeKickRestart::BuildTakerWaitingLocation(
	const ASoccerMatchManager& Manager,
	const ASoccerAICharacter* SoccerAICharacter
) const
{
	if (!IsValid(SoccerAICharacter))
	{
		return RestartLocation;
	}

	FVector AttackDirection = Manager.GetFieldAttackDirectionForTeam(RestartTeam);
	AttackDirection.Z = 0.0f;
	if (!AttackDirection.Normalize())
	{
		AttackDirection = FVector(1.0f, 0.0f, 0.0f);
	}

	FVector LateralDirection = FVector::CrossProduct(FVector::UpVector, AttackDirection);
	LateralDirection.Z = 0.0f;
	if (!LateralDirection.Normalize())
	{
		LateralDirection = FVector(0.0f, 1.0f, 0.0f);
	}

	float WaitingSideSign = 1.0f;
	float ClosestIllegalOpponentDistanceSquared = TNumericLimits<float>::Max();
	UWorld* World = Manager.GetWorld();

	if (World != nullptr)
	{
		for (TActorIterator<ASoccerCharacterBase> It(World); It; ++It)
		{
			const ASoccerCharacterBase* Candidate = *It;
			if (
				!IsValid(Candidate) ||
				Candidate->GetTeam() == RestartTeam ||
				!IsCharacterTooClose(Manager, Candidate)
			)
			{
				continue;
			}

			const float DistanceSquared = FVector::DistSquared2D(
				Candidate->GetActorLocation(),
				RestartLocation
			);

			if (DistanceSquared >= ClosestIllegalOpponentDistanceSquared)
			{
				continue;
			}

			ClosestIllegalOpponentDistanceSquared = DistanceSquared;
			FVector RelativeLocation = Candidate->GetActorLocation() - RestartLocation;
			RelativeLocation.Z = 0.0f;
			const float OpponentSide = FVector::DotProduct(RelativeLocation, LateralDirection);
			WaitingSideSign = OpponentSide >= 0.0f ? -1.0f : 1.0f;
		}
	}

	FVector WaitingLocation =
		RestartLocation -
		AttackDirection * Manager.OffsideRestartTakerWaitingBackDistance +
		LateralDirection * WaitingSideSign * Manager.OffsideRestartTakerWaitingLateralDistance;
	WaitingLocation.Z = SoccerAICharacter->GetActorLocation().Z;

	return Manager.ProjectLocationToNavigation(WaitingLocation, SoccerAICharacter);
}

FVector FSoccerFreeKickRestart::BuildOpponentMoveLocation(
	const ASoccerMatchManager& Manager,
	const ASoccerAICharacter* SoccerAICharacter
) const
{
	if (!IsValid(SoccerAICharacter))
	{
		return FVector::ZeroVector;
	}

	if (SoccerAICharacter->GetPlayerRole() == ESoccerPlayerRole::Goalkeeper)
	{
		if (bDefensiveWallActive && !GoalkeeperWallHoldLocation.IsNearlyZero())
		{
			return GoalkeeperWallHoldLocation;
		}

		return Manager.ProjectLocationToNavigation(
			Manager.GetGoalkeeperMoveLocation(SoccerAICharacter),
			SoccerAICharacter
		);
	}

	if (const FVector* LockedLocation =
		OpponentHoldLocations.Find(SoccerAICharacter))
	{
		return *LockedLocation;
	}

	return BuildOpponentDesiredMoveLocation(Manager, SoccerAICharacter);
}

FVector FSoccerFreeKickRestart::BuildOpponentDesiredMoveLocation(
	const ASoccerMatchManager& Manager,
	const ASoccerAICharacter* SoccerAICharacter
) const
{
	if (!IsValid(SoccerAICharacter))
	{
		return FVector::ZeroVector;
	}

	if (SoccerAICharacter->GetPlayerRole() == ESoccerPlayerRole::Goalkeeper)
	{
		if (bDefensiveWallActive && !GoalkeeperWallHoldLocation.IsNearlyZero())
		{
			return GoalkeeperWallHoldLocation;
		}

		return Manager.ProjectLocationToNavigation(
			Manager.GetGoalkeeperMoveLocation(SoccerAICharacter),
			SoccerAICharacter
		);
	}

	const FVector DesiredLocation = Manager.BuildDefendCompactShapeLocation(SoccerAICharacter);
	return Manager.BuildCircularRestartOpponentMoveLocation(
		SoccerAICharacter,
		DesiredLocation,
		RestartLocation,
		Manager.OffsideRestartOpponentRequiredDistance +
			FMath::Max(0.0f, Manager.OffsideRestartOpponentLegalBuffer),
		Manager.OffsideRestartOpponentMoveExtraDistance
	);
}


bool FSoccerFreeKickRestart::DoesNavigationPathAvoidRestartCircle(
	const ASoccerMatchManager& Manager,
	const ASoccerAICharacter* SoccerAICharacter,
	const FVector& TargetLocation,
	float ProtectedRadius
) const
{
	if (!IsValid(SoccerAICharacter) || TargetLocation.IsNearlyZero())
	{
		return false;
	}

	UWorld* World = Manager.GetWorld();
	if (World == nullptr)
	{
		return false;
	}

	const float SafeRadius = FMath::Max(0.0f, ProtectedRadius);
	const FVector StartLocation = SoccerAICharacter->GetActorLocation();

	if (
		FVector::Dist2D(StartLocation, RestartLocation) < SafeRadius ||
		FVector::Dist2D(TargetLocation, RestartLocation) < SafeRadius
	)
	{
		return false;
	}

	UNavigationPath* NavigationPath =
		UNavigationSystemV1::FindPathToLocationSynchronously(
			World,
			StartLocation,
			TargetLocation,
			const_cast<ASoccerAICharacter*>(SoccerAICharacter)
		);

	if (
		!IsValid(NavigationPath) ||
		!NavigationPath->IsValid() ||
		NavigationPath->IsPartial() ||
		NavigationPath->PathPoints.Num() < 2
	)
	{
		return false;
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

	for (
		int32 PointIndex = 1;
		PointIndex < NavigationPath->PathPoints.Num();
		++PointIndex
	)
	{
		if (
			DistancePointToSegment2D(
				RestartLocation,
				NavigationPath->PathPoints[PointIndex - 1],
				NavigationPath->PathPoints[PointIndex]
			) < SafeRadius
		)
		{
			return false;
		}
	}

	return true;
}

void FSoccerFreeKickRestart::InitializeOpponentPositioningPlan(
	ASoccerMatchManager& Manager
)
{
	OpponentHoldLocations.Empty();
	OpponentsCompletingMandatoryEscape.Empty();
	OpponentsThatUsedLegalReposition.Empty();
	DefensiveWallMembers.Empty();
	GoalkeeperWallHoldLocation = FVector::ZeroVector;
	bDefensiveWallActive = false;

	UWorld* World = Manager.GetWorld();
	if (World == nullptr)
	{
		return;
	}

	const float RequiredClearance =
		FMath::Max(
			0.0f,
			Manager.OffsideRestartOpponentRequiredDistance +
				FMath::Max(0.0f, Manager.OffsideRestartOpponentLegalBuffer)
		);

	const float ProtectedPathRadius =
		RequiredClearance +
		FMath::Max(0.0f, Manager.FreeKickOpponentPathSafetyMargin);

	InitializeDefensiveWall(Manager);

	for (TActorIterator<ASoccerAICharacter> It(World); It; ++It)
	{
		ASoccerAICharacter* Candidate = *It;

		if (
			!IsValid(Candidate) ||
			Candidate->GetTeam() == RestartTeam ||
			Candidate->GetPlayerRole() == ESoccerPlayerRole::Goalkeeper
		)
		{
			continue;
		}

		if (DefensiveWallMembers.Contains(Candidate))
		{
			continue;
		}

		const FVector CurrentLocation = Candidate->GetActorLocation();
		const bool bStartsIllegal =
			FVector::Dist2D(CurrentLocation, RestartLocation) <
			RequiredClearance;

		if (bStartsIllegal)
		{
			FVector EscapeLocation =
				BuildOpponentDesiredMoveLocation(Manager, Candidate);

			if (
				EscapeLocation.IsNearlyZero() ||
				FVector::Dist2D(EscapeLocation, RestartLocation) <
					RequiredClearance
			)
			{
				EscapeLocation =
					Manager.BuildCircularRestartOpponentMoveLocation(
						Candidate,
						CurrentLocation,
						RestartLocation,
						RequiredClearance,
						Manager.OffsideRestartOpponentMoveExtraDistance
					);
			}

			if (!EscapeLocation.IsNearlyZero())
			{
				OpponentHoldLocations.Add(Candidate, EscapeLocation);
				OpponentsCompletingMandatoryEscape.Add(Candidate);
			}

			continue;
		}

		// A player that already satisfies the rule may take one initial tactical
		// position, but only if the actual NavMesh route remains outside the
		// protected circle. Otherwise its current legal position is safer.
		FVector DesiredLocation =
			BuildOpponentDesiredMoveLocation(Manager, Candidate);

		if (
			DesiredLocation.IsNearlyZero() ||
			FVector::Dist2D(DesiredLocation, RestartLocation) <
				RequiredClearance ||
			!DoesNavigationPathAvoidRestartCircle(
				Manager,
				Candidate,
				DesiredLocation,
				ProtectedPathRadius
			)
		)
		{
			DesiredLocation = CurrentLocation;
		}

		OpponentHoldLocations.Add(Candidate, DesiredLocation);
		OpponentsThatUsedLegalReposition.Add(Candidate);
	}
}

void FSoccerFreeKickRestart::InitializeDefensiveWall(
	ASoccerMatchManager& Manager
)
{
	if (
		RestartType != ESoccerRestartType::DirectFreeKick ||
		!Manager.bEnableFreeKickDefensiveWall
	)
	{
		return;
	}

	UWorld* World = Manager.GetWorld();
	if (World == nullptr)
	{
		return;
	}

	const ESoccerTeam DefendingTeam = Manager.GetOppositeTeam(RestartTeam);
	const FVector OwnGoalLocation =
		Manager.GetOwnGoalReferenceLocation(DefendingTeam);
	FVector GoalToBall = RestartLocation - OwnGoalLocation;
	GoalToBall.Z = 0.0f;
	const float GoalDistance = GoalToBall.Size();

	const float MaximumGoalDistance = FMath::Max(
		100.0f,
		Manager.FreeKickWallMaximumGoalDistance
	);
	if (GoalDistance > MaximumGoalDistance || GoalDistance <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	FVector GoalNormal =
		Manager.GetFieldAttackDirectionForTeam(DefendingTeam);
	GoalNormal.Z = 0.0f;
	if (!GoalNormal.Normalize())
	{
		return;
	}

	const float DirectnessDot = FMath::Clamp(
		FVector::DotProduct(GoalToBall / GoalDistance, GoalNormal),
		-1.0f,
		1.0f
	);
	const float GoalAngleDegrees =
		FMath::RadiansToDegrees(FMath::Acos(DirectnessDot));
	if (
		GoalAngleDegrees > FMath::Clamp(
			Manager.FreeKickWallMaximumGoalAngleDegrees,
			0.0f,
			89.0f
		)
	)
	{
		return;
	}

	struct FWallCandidate
	{
		ASoccerAICharacter* Character = nullptr;
		int32 RolePriority = 0;
		float TravelDistanceSquared = 0.0f;
	};

	TArray<FWallCandidate> Candidates;
	ASoccerAICharacter* Goalkeeper = nullptr;
	float MaximumWallPlayerDiameter = 0.0f;

	for (TActorIterator<ASoccerAICharacter> It(World); It; ++It)
	{
		ASoccerAICharacter* Candidate = *It;
		if (!IsValid(Candidate) || Candidate->GetTeam() != DefendingTeam)
		{
			continue;
		}

		if (Candidate->GetPlayerRole() == ESoccerPlayerRole::Goalkeeper)
		{
			Goalkeeper = Candidate;
			continue;
		}

		FWallCandidate Entry;
		Entry.Character = Candidate;
		Entry.RolePriority =
			Candidate->GetPlayerRole() == ESoccerPlayerRole::Defender
			? 0
			: Candidate->GetPlayerRole() == ESoccerPlayerRole::Midfielder
				? 1
				: 2;
		Entry.TravelDistanceSquared = FVector::DistSquared2D(
			Candidate->GetActorLocation(),
			RestartLocation
		);
		Candidates.Add(Entry);

		if (const UCapsuleComponent* Capsule = Candidate->GetCapsuleComponent())
		{
			MaximumWallPlayerDiameter = FMath::Max(
				MaximumWallPlayerDiameter,
				Capsule->GetScaledCapsuleRadius() * 2.0f
			);
		}
	}

	const int32 MaximumSelectablePlayers = FMath::Max(
		0,
		Candidates.Num() -
			FMath::Max(0, Manager.FreeKickWallMinimumNonWallOutfieldPlayers)
	);
	if (MaximumSelectablePlayers <= 0)
	{
		return;
	}

	const int32 MinimumPlayers = FMath::Max(
		1,
		Manager.FreeKickWallMinimumPlayers
	);
	const int32 MaximumPlayers = FMath::Max(
		MinimumPlayers,
		Manager.FreeKickWallMaximumPlayers
	);
	const float MaximumPlayersDistance = FMath::Clamp(
		Manager.FreeKickWallMinimumGoalDistanceForMaximumPlayers,
		100.0f,
		MaximumGoalDistance
	);
	const float DistanceAlpha = FMath::Clamp(
		(GoalDistance - MaximumPlayersDistance) /
			FMath::Max(1.0f, MaximumGoalDistance - MaximumPlayersDistance),
		0.0f,
		1.0f
	);
	const int32 DesiredPlayers = FMath::RoundToInt(
		FMath::Lerp(
			static_cast<float>(MaximumPlayers),
			static_cast<float>(MinimumPlayers),
			DistanceAlpha
		)
	);
	const int32 WallPlayerCount = FMath::Clamp(
		DesiredPlayers,
		1,
		MaximumSelectablePlayers
	);

	FVector GoalRightDirection =
		Manager.GetFieldRightDirectionForTeam(DefendingTeam);
	GoalRightDirection.Z = 0.0f;
	if (!GoalRightDirection.Normalize())
	{
		GoalRightDirection = FVector::RightVector;
	}

	const float BallLateral = FVector::DotProduct(
		RestartLocation - OwnGoalLocation,
		GoalRightDirection
	);
	const float ProtectedSideSign = BallLateral >= 0.0f ? 1.0f : -1.0f;
	const FVector ProtectedGoalLocation =
		OwnGoalLocation +
		GoalRightDirection * ProtectedSideSign *
		SoccerFieldDimensions::GoalHalfWidthCm *
		FMath::Clamp(
			Manager.FreeKickWallProtectedGoalLateralAlpha,
			0.0f,
			1.0f
		);

	FVector BallToProtectedGoal = ProtectedGoalLocation - RestartLocation;
	BallToProtectedGoal.Z = 0.0f;
	if (!BallToProtectedGoal.Normalize())
	{
		return;
	}

	FVector WallLateralDirection = FVector::CrossProduct(
		FVector::UpVector,
		BallToProtectedGoal
	);
	WallLateralDirection.Z = 0.0f;
	if (!WallLateralDirection.Normalize())
	{
		return;
	}

	const float WallDistanceFromBall =
		FMath::Max(0.0f, Manager.OffsideRestartOpponentRequiredDistance) +
		FMath::Max(0.0f, Manager.OffsideRestartOpponentLegalBuffer) +
		FMath::Max(1.0f, Manager.FreeKickWallMoveAcceptanceRadius) +
		FMath::Max(0.0f, Manager.FreeKickWallExtraDistanceFromBall);
	const FVector WallCenter =
		RestartLocation + BallToProtectedGoal * WallDistanceFromBall;

	Candidates.Sort(
		[](const FWallCandidate& A, const FWallCandidate& B)
		{
			if (A.RolePriority != B.RolePriority)
			{
				return A.RolePriority < B.RolePriority;
			}
			return A.TravelDistanceSquared < B.TravelDistanceSquared;
		}
	);

	const float PlayerSpacing = FMath::Max3(
		40.0f,
		Manager.FreeKickWallPlayerSpacing,
		MaximumWallPlayerDiameter +
			FMath::Max(0.0f, Manager.FreeKickWallMinimumBodyGap)
	);
	for (int32 WallIndex = 0; WallIndex < WallPlayerCount; ++WallIndex)
	{
		ASoccerAICharacter* WallPlayer = Candidates[WallIndex].Character;
		if (!IsValid(WallPlayer))
		{
			continue;
		}

		const float CenteredIndex =
			static_cast<float>(WallIndex) -
			(static_cast<float>(WallPlayerCount - 1) * 0.5f);
		FVector WallLocation =
			WallCenter + WallLateralDirection * CenteredIndex * PlayerSpacing;
		WallLocation.Z = WallPlayer->GetActorLocation().Z;
		WallLocation = Manager.ProjectLocationToNavigation(
			WallLocation,
			WallPlayer
		);

		OpponentHoldLocations.Add(WallPlayer, WallLocation);
		DefensiveWallMembers.Add(WallPlayer);
		OpponentsThatUsedLegalReposition.Add(WallPlayer);

		if (
			FVector::Dist2D(
				WallPlayer->GetActorLocation(),
				RestartLocation
			) < WallDistanceFromBall
		)
		{
			OpponentsCompletingMandatoryEscape.Add(WallPlayer);
		}
	}

	if (DefensiveWallMembers.Num() <= 0)
	{
		return;
	}

	bDefensiveWallActive = true;

	if (IsValid(Goalkeeper))
	{
		FVector GoalkeeperLocation =
			Manager.GetGoalkeeperMoveLocation(Goalkeeper);
		GoalkeeperLocation -=
			GoalRightDirection * ProtectedSideSign *
			FMath::Max(
				0.0f,
				Manager.FreeKickWallGoalkeeperOppositeSideOffset
			);
		GoalkeeperLocation.Z = Goalkeeper->GetActorLocation().Z;
		GoalkeeperWallHoldLocation = Manager.ProjectLocationToNavigation(
			GoalkeeperLocation,
			Goalkeeper
		);
	}

	UE_LOG(
		LogTemp,
		Display,
		TEXT("[FreeKickWall] team=%d players=%d goalDistance=%.0f angle=%.1f."),
		static_cast<int32>(DefendingTeam),
		DefensiveWallMembers.Num(),
		GoalDistance,
		GoalAngleDegrees
	);
}

bool FSoccerFreeKickRestart::UpdateOpponentPositioningAfterEscape(
	ASoccerMatchManager& Manager
)
{
	if (
		!IsActive(Manager) ||
		OpponentsCompletingMandatoryEscape.Num() <= 0
	)
	{
		return false;
	}

	const float RequiredClearance =
		FMath::Max(
			0.0f,
			Manager.OffsideRestartOpponentRequiredDistance +
				FMath::Max(0.0f, Manager.OffsideRestartOpponentLegalBuffer)
		);

	const float ProtectedPathRadius =
		RequiredClearance +
		FMath::Max(0.0f, Manager.FreeKickOpponentPathSafetyMargin);

	const float ArrivalDistance =
		FMath::Max(
			1.0f,
			Manager.FreeKickOpponentEscapeArrivalDistance
		);

	const float MinimumRepositionDistance =
		FMath::Max(
			0.0f,
			Manager.FreeKickOpponentLegalRepositionMinDistance
		);

	bool bChangedAnyTarget = false;

	TArray<const ASoccerAICharacter*> EscapingOpponents;
	EscapingOpponents.Reserve(
		OpponentsCompletingMandatoryEscape.Num()
	);

	for (const ASoccerAICharacter* Candidate :
		OpponentsCompletingMandatoryEscape)
	{
		EscapingOpponents.Add(Candidate);
	}

	for (const ASoccerAICharacter* Candidate : EscapingOpponents)
	{
		if (!IsValid(Candidate))
		{
			OpponentsCompletingMandatoryEscape.Remove(Candidate);
			OpponentHoldLocations.Remove(Candidate);
			OpponentsThatUsedLegalReposition.Remove(Candidate);
			continue;
		}

		const FVector* EscapeLocation =
			OpponentHoldLocations.Find(Candidate);

		if (
			EscapeLocation == nullptr ||
			IsCharacterTooClose(Manager, Candidate) ||
			FVector::Dist2D(
				Candidate->GetActorLocation(),
				*EscapeLocation
			) > ArrivalDistance
		)
		{
			continue;
		}

		// The regulatory escape is complete. From this point on the player is
		// never sent through the circle again.
		OpponentsCompletingMandatoryEscape.Remove(Candidate);

		if (
			!Manager.bAllowFreeKickOpponentLegalReposition ||
			OpponentsThatUsedLegalReposition.Contains(Candidate)
		)
		{
			continue;
		}

		OpponentsThatUsedLegalReposition.Add(Candidate);

		FVector DesiredLocation =
			BuildOpponentDesiredMoveLocation(Manager, Candidate);

		if (
			DesiredLocation.IsNearlyZero() ||
			FVector::Dist2D(DesiredLocation, RestartLocation) <
				RequiredClearance ||
			FVector::Dist2D(
				Candidate->GetActorLocation(),
				DesiredLocation
			) < MinimumRepositionDistance ||
			!DoesNavigationPathAvoidRestartCircle(
				Manager,
				Candidate,
				DesiredLocation,
				ProtectedPathRadius
			)
		)
		{
			continue;
		}

		OpponentHoldLocations.Add(Candidate, DesiredLocation);
		bChangedAnyTarget = true;
	}

	return bChangedAnyTarget;
}

void FSoccerFreeKickRestart::AdoptOpponentRecoveryTarget(
	const ASoccerAICharacter* SoccerAICharacter,
	const FVector& LegalTargetLocation
)
{
	if (
		!IsValid(SoccerAICharacter) ||
		LegalTargetLocation.IsNearlyZero() ||
		SoccerAICharacter->GetTeam() == RestartTeam ||
		SoccerAICharacter->GetPlayerRole() == ESoccerPlayerRole::Goalkeeper
	)
	{
		return;
	}

	if (DefensiveWallMembers.Contains(SoccerAICharacter))
	{
		return;
	}

	OpponentHoldLocations.Add(
		SoccerAICharacter,
		LegalTargetLocation
	);
	OpponentsCompletingMandatoryEscape.Add(SoccerAICharacter);
}


FVector FSoccerFreeKickRestart::GetMoveLocation(
	const ASoccerMatchManager& Manager,
	const ASoccerAICharacter* SoccerAICharacter
) const
{
	if (!IsActive(Manager) || !IsValid(SoccerAICharacter))
	{
		return FVector::ZeroVector;
	}

	if (SoccerAICharacter == TakerAI)
	{
		if (bHumanTakerClaimed)
		{
			return BuildFallbackTakerHoldLocation(Manager, SoccerAICharacter);
		}

		if (!AreOpponentsClear(Manager))
		{
			return !TakerWaitingLocation.IsNearlyZero()
				? TakerWaitingLocation
				: BuildTakerWaitingLocation(Manager, SoccerAICharacter);
		}
		if (bFinalRunActive)
		{
			return RunThroughLocation;
		}
		if (!RunUpStartLocation.IsNearlyZero())
		{
			return RunUpStartLocation;
		}
		return Manager.ProjectLocationToNavigation(RestartLocation, SoccerAICharacter);
	}

	if (SoccerAICharacter == ReceiverAI)
	{
		return BuildReceiverMoveLocation(Manager);
	}

	if (SoccerAICharacter->GetPlayerRole() == ESoccerPlayerRole::Goalkeeper)
	{
		// El arquero defensor conserva la cobertura especial de barrera.
		if (SoccerAICharacter->GetTeam() != RestartTeam)
		{
			return BuildOpponentMoveLocation(Manager, SoccerAICharacter);
		}

		// El arquero del equipo ejecutor tampoco participa de la forma ofensiva
		// de los jugadores de campo: acompana lateralmente la pelota sin dejar
		// su profundidad segura durante la reanudacion.
		return Manager.ProjectLocationToNavigation(
			Manager.GetGoalkeeperMoveLocation(SoccerAICharacter),
			SoccerAICharacter
		);
	}

	if (SoccerAICharacter->GetTeam() == RestartTeam)
	{
		const ESoccerAIOrder RestartAttackOrder =
			Manager.GetDefaultRestartAttackOrderForCharacter(SoccerAICharacter);

		FVector DesiredLocation = Manager.BuildAttackShapeLocation(
			SoccerAICharacter,
			RestartAttackOrder
		);
		DesiredLocation = Manager.ApplyOffsideSafetyToAttackMoveLocation(
			SoccerAICharacter,
			DesiredLocation,
			RestartAttackOrder
		);
		return Manager.ProjectLocationToNavigation(DesiredLocation, SoccerAICharacter);
	}

	return BuildOpponentMoveLocation(Manager, SoccerAICharacter);
}

bool FSoccerFreeKickRestart::UpdateHumanTakerClaimDuringPreparation(
	ASoccerMatchManager& Manager
)
{
	AThirdPersonCppCharacter* CandidateHuman =
		Manager.FindHumanCharacterForTeam(RestartTeam);

	HumanTaker = IsValid(CandidateHuman)
		? CandidateHuman
		: nullptr;

	const bool bPreviousClaim = bHumanTakerClaimed;

	if (!IsValid(HumanTaker))
	{
		bHumanTakerClaimed = false;
		return bPreviousClaim != bHumanTakerClaimed;
	}

	const float ClaimRadius =
		FMath::Max(50.0f, Manager.FreeKickHumanTakerClaimRadius);
	const float ReleaseRadius =
		FMath::Max(
			ClaimRadius,
			Manager.FreeKickHumanTakerReleaseRadius
		);

	const float DistanceToRestart = FVector::Dist2D(
		HumanTaker->GetActorLocation(),
		RestartLocation
	);

	bHumanTakerClaimed =
		bPreviousClaim
		? DistanceToRestart <= ReleaseRadius
		: DistanceToRestart <= ClaimRadius;

	if (bPreviousClaim != bHumanTakerClaimed)
	{
		ASoccerDebugManager::Message(
			&Manager,
			ESoccerDebugCategory::Restarts,
			bHumanTakerClaimed
				? TEXT("TIRO LIBRE: humano reclama el saque")
				: TEXT("TIRO LIBRE: humano cede el saque al bot"),
			bHumanTakerClaimed ? FColor::Cyan : FColor::Yellow
		);
	}

	return bPreviousClaim != bHumanTakerClaimed;
}

bool FSoccerFreeKickRestart::ShouldHumanKeepExecutionClaim(
	const ASoccerMatchManager& Manager
) const
{
	if (
		!bHumanTakerClaimed ||
		!bHumanExecutionAuthorized ||
		!IsValid(HumanTaker)
	)
	{
		return false;
	}

	const float ReleaseRadius =
		FMath::Max(
			FMath::Max(50.0f, Manager.FreeKickHumanTakerClaimRadius),
			Manager.FreeKickHumanTakerReleaseRadius
		);

	return FVector::Dist2D(
		HumanTaker->GetActorLocation(),
		RestartLocation
	) <= ReleaseRadius;
}

FVector FSoccerFreeKickRestart::BuildFallbackTakerHoldLocation(
	const ASoccerMatchManager& Manager,
	const ASoccerAICharacter* SoccerAICharacter
) const
{
	if (!IsValid(SoccerAICharacter))
	{
		return FVector::ZeroVector;
	}

	const ESoccerAIOrder RestartAttackOrder =
		Manager.GetDefaultRestartAttackOrderForCharacter(SoccerAICharacter);

	FVector DesiredLocation = Manager.BuildAttackShapeLocation(
		SoccerAICharacter,
		RestartAttackOrder
	);
	DesiredLocation = Manager.ApplyOffsideSafetyToAttackMoveLocation(
		SoccerAICharacter,
		DesiredLocation,
		RestartAttackOrder
	);

	if (DesiredLocation.IsNearlyZero())
	{
		DesiredLocation = SoccerAICharacter->GetActorLocation();
	}

	// Keep the fallback visibly clear of the human run-up even if the normal
	// tactical shape happens to place this bot near the restart spot.
	const float MinimumDistance =
		FMath::Max(
			500.0f,
			FMath::Max(
				Manager.FreeKickHumanTakerClaimRadius,
				Manager.FreeKickHumanTakerReleaseRadius
			) + 100.0f
		);

	FVector FromRestart = DesiredLocation - RestartLocation;
	FromRestart.Z = 0.0f;

	if (FromRestart.Size() < MinimumDistance)
	{
		if (!FromRestart.Normalize())
		{
			FVector AttackDirection =
				Manager.GetFieldAttackDirectionForTeam(RestartTeam);
			AttackDirection.Z = 0.0f;
			if (!AttackDirection.Normalize())
			{
				AttackDirection = FVector::ForwardVector;
			}

			FromRestart =
				FVector::CrossProduct(FVector::UpVector, AttackDirection);
			FromRestart.Z = 0.0f;
			if (!FromRestart.Normalize())
			{
				FromRestart = FVector::RightVector;
			}
		}

		DesiredLocation =
			RestartLocation + FromRestart * MinimumDistance;
		DesiredLocation.Z = SoccerAICharacter->GetActorLocation().Z;
	}

	return Manager.ProjectLocationToNavigation(
		DesiredLocation,
		SoccerAICharacter
	);
}

void FSoccerFreeKickRestart::RecalculateRunUpGeometry(ASoccerMatchManager& Manager)
{
	if (!IsValid(Manager.SoccerBall) || !IsValid(TakerAI) || !IsValid(ReceiverAI))
	{
		KickDirection = FVector::ForwardVector;
		RunUpStartLocation = FVector::ZeroVector;
		RunThroughLocation = FVector::ZeroVector;
		return;
	}

	const FVector BallLocation = Manager.SoccerBall->GetActorLocation();
	const FVector ReceiverTargetLocation = BuildReceiverMoveLocation(Manager);
	KickDirection = ReceiverTargetLocation - BallLocation;
	KickDirection.Z = 0.0f;

	if (!KickDirection.Normalize())
	{
		KickDirection = Manager.GetFieldAttackDirectionForTeam(RestartTeam);
		KickDirection.Z = 0.0f;
		if (!KickDirection.Normalize())
		{
			KickDirection = FVector::ForwardVector;
		}
	}

	RunUpStartLocation =
		BallLocation - KickDirection * FMath::Max(50.0f, Manager.OffsideRestartRunUpDistance);
	RunUpStartLocation.Z = TakerAI->GetActorLocation().Z;
	RunUpStartLocation = Manager.ProjectLocationToNavigation(RunUpStartLocation, TakerAI);

	RunThroughLocation =
		BallLocation + KickDirection * FMath::Max(50.0f, Manager.OffsideRestartRunThroughDistance);
	RunThroughLocation.Z = TakerAI->GetActorLocation().Z;
	RunThroughLocation = Manager.ProjectLocationToNavigation(RunThroughLocation, TakerAI);
}

void FSoccerFreeKickRestart::BeginFinalRun(ASoccerMatchManager& Manager)
{
	if (!IsValid(TakerAI) || !IsValid(ReceiverAI) || !IsValid(Manager.SoccerBall))
	{
		return;
	}

	bHumanTakerClaimed = false;
	bHumanExecutionAuthorized = false;

	Manager.SelectActiveRestartExecutionReceiver(
		RestartTeam,
		TakerAI,
		ReceiverAI,
		false
	);

	const FVector BallLocation = Manager.SoccerBall->GetActorLocation();
	const FVector ReceiverTargetLocation =
		Manager.GetActiveRestartExecutionTargetLocation(
			BuildReceiverMoveLocation(Manager)
		);
	KickDirection = ReceiverTargetLocation - BallLocation;
	KickDirection.Z = 0.0f;

	if (!KickDirection.Normalize())
	{
		KickDirection = Manager.GetFieldAttackDirectionForTeam(RestartTeam);
	}

	if (!Manager.BuildRestartKickRunGeometryFromCurrentTaker(
		TakerAI,
		Manager.OffsideRestartRunThroughDistance,
		RunDirection,
		RunThroughLocation
	))
	{
		return;
	}

	bFinalRunActive = true;
	Manager.InitializeRestartKickContactTracking(ContactTracker, TakerAI);
	Manager.MatchPlayState = ESoccerMatchPlayState::OffsideRestartTaking;
	Manager.ResetActiveRestartReadyHold();
}

bool FSoccerFreeKickRestart::IsTakerAtBallContact(ASoccerMatchManager& Manager)
{
	if (!bFinalRunActive)
	{
		Manager.ResetRestartKickContactTracking(ContactTracker);
		return false;
	}

	const ERestartKickContactResult ContactResult = Manager.EvaluateRestartKickContact(
		TakerAI,
		RunDirection,
		Manager.OffsideRestartMaxBallSurfaceGapForKick,
		Manager.OffsideRestartMinimumFacingDot,
		ContactTracker
	);

	if (ContactResult == ERestartKickContactResult::MissedBall)
	{
		RecoverFinalRunAfterMiss(Manager);
		return false;
	}

	return ContactResult == ERestartKickContactResult::Contact;
}

void FSoccerFreeKickRestart::RecoverFinalRunAfterMiss(ASoccerMatchManager& Manager)
{
	bFinalRunActive = false;
	Manager.ResetRestartKickContactTracking(ContactTracker);
	RecalculateRunUpGeometry(Manager);

	if (IsValid(TakerAI) && !RunUpStartLocation.IsNearlyZero())
	{
		Manager.ActiveRestartAITargetLocations.Add(TakerAI, RunUpStartLocation);
	}

	Manager.ResetActiveRestartReadyHold();
}

void FSoccerFreeKickRestart::ResetRuntime(ASoccerMatchManager& Manager)
{
	if (bAIKickMontageStarted && IsValid(TakerAI))
	{
		TakerAI->CancelAIKickMontageBeforeImpact();
	}

	bFinalRunActive = false;
	bAIKickMontageStarted = false;
	PendingAIKickTargetLocation = FVector::ZeroVector;
	RestartType = ESoccerRestartType::None;
	RestartTeam = ESoccerTeam::PlayerTeam;
	RestartLocation = FVector::ZeroVector;
	SetupStartTime = -1000.0f;
	TakerAI = nullptr;
	ReceiverAI = nullptr;
	HumanTaker = nullptr;
	bHumanTakerClaimed = false;
	bHumanExecutionAuthorized = false;
	TakerWaitingLocation = FVector::ZeroVector;
	ReceiverHoldLocation = FVector::ZeroVector;
	OpponentHoldLocations.Empty();
	OpponentsCompletingMandatoryEscape.Empty();
	OpponentsThatUsedLegalReposition.Empty();
	DefensiveWallMembers.Empty();
	GoalkeeperWallHoldLocation = FVector::ZeroVector;
	bDefensiveWallActive = false;
	KickDirection = FVector::ForwardVector;
	RunDirection = FVector::ForwardVector;
	RunUpStartLocation = FVector::ZeroVector;
	RunThroughLocation = FVector::ZeroVector;
	Manager.ResetRestartKickContactTracking(ContactTracker);
}
