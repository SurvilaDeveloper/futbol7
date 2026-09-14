#include "SoccerPenaltyKickRestart.h"

#include "SoccerMatchManager.h"
#include "SoccerAICharacter.h"
#include "SoccerBall.h"
#include "SoccerCharacterBase.h"
#include "SoccerDebugManager.h"
#include "SoccerField.h"
#include "SoccerFieldDimensions.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ThirdPersonCppCharacter.h"
#include "Engine/World.h"
#include "EngineUtils.h"

void FSoccerPenaltyKickRestart::CompleteByAI(ASoccerMatchManager& Manager)
{
	if (!IsValid(Manager.SoccerBall) || !TakerAI.IsValid() || bTaken)
	{
		return;
	}

	ASoccerAICharacter* AITaker = TakerAI.Get();
	const FVector ShotTarget = GetShotTargetLocation(Manager);

	if (!Manager.TryRegisterIntentionalBallTouch(AITaker))
	{
		return;
	}

	AITaker->PlayAIKickAnimationForRestart();
	Manager.SoccerBall->ChargedKickToTarget(
		ShotTarget,
		FMath::Max(100.0f, Manager.PenaltyKickAIShotHorizontalSpeed)
	);
}

void FSoccerPenaltyKickRestart::Cancel(ASoccerMatchManager& Manager)
{
	if (Manager.ActiveRestartType == ESoccerRestartType::PenaltyKick)
	{
		Manager.EndRestartContext();
	}

	ResetRuntimeState();
}

bool FSoccerPenaltyKickRestart::IsActive(const ASoccerMatchManager& Manager) const
{
	return
		Manager.ActiveRestartType == ESoccerRestartType::PenaltyKick &&
		(
			Manager.MatchPlayState == ESoccerMatchPlayState::PenaltyKickSetup ||
			Manager.MatchPlayState == ESoccerMatchPlayState::PenaltyKickTaking
		);
}

bool FSoccerPenaltyKickRestart::IsTaker(const ASoccerCharacterBase* Character) const
{
	return IsValid(Character) && Character == Taker.Get();
}

bool FSoccerPenaltyKickRestart::IsHumanTaker(const AThirdPersonCppCharacter* Character) const
{
	return
		IsValid(Character) &&
		bUsesHumanTaker &&
		Character == TakerHuman.Get();
}

bool FSoccerPenaltyKickRestart::IsDefendingGoalkeeper(
	const ASoccerAICharacter* Character
) const
{
	return IsValid(Character) && Character == GoalkeeperAI.Get();
}

bool FSoccerPenaltyKickRestart::AreNonParticipantsInLegalPositions(
	const ASoccerMatchManager& Manager
) const
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

		if (!IsNonParticipantInLegalPosition(Manager, Candidate))
		{
			return false;
		}
	}

	return true;
}

bool FSoccerPenaltyKickRestart::IsNonParticipantInLegalPosition(
	const ASoccerMatchManager& Manager,
	const ASoccerCharacterBase* Character
) const
{
	if (
		!IsValid(Character) ||
		Character->IsHidden() ||
		Character->ActorHasTag(FName(TEXT("SubstitutionPresentation"))) ||
		IsTaker(Character) ||
		Character == GoalkeeperAI.Get()
	)
	{
		return true;
	}

	const ASoccerField* Field = Manager.GetSoccerField();
	const FVector RuleLocation = IsValid(Field)
		? Field->WorldToPitchLocal(Character->GetActorLocation())
		: Character->GetActorLocation();
	const float DefendingGoalLineSign =
		Manager.GetOpponentGoalLineSign(RestartTeam);
	constexpr float RuleToleranceCm = 5.0f;

	if (
		SoccerFieldDimensions::IsLocationInsidePenaltyArea2D(
			RuleLocation,
			DefendingGoalLineSign
		) ||
		SoccerFieldDimensions::IsLocationInsidePenaltyDistanceCircle2D(
			RuleLocation,
			DefendingGoalLineSign,
			-RuleToleranceCm
		) ||
		SoccerFieldDimensions::GetPenaltySpotBehindProgress2D(
			RuleLocation,
			DefendingGoalLineSign
		) < -RuleToleranceCm ||
		!SoccerFieldDimensions::IsLocationInsidePitch2D(RuleLocation)
	)
	{
		return false;
	}

	return true;
}

bool FSoccerPenaltyKickRestart::CanAcceptFirstTouch(
	const ASoccerMatchManager& Manager,
	const ASoccerCharacterBase* TouchingCharacter
) const
{
	return
		IsActive(Manager) &&
		Manager.MatchPlayState == ESoccerMatchPlayState::PenaltyKickTaking &&
		IsTaker(TouchingCharacter) &&
		AreNonParticipantsInLegalPositions(Manager) &&
		!bTaken;
}

void FSoccerPenaltyKickRestart::OnFirstTouchRegistered(
	ASoccerMatchManager& Manager,
	ASoccerCharacterBase* TouchingCharacter
)
{
	if (!CanAcceptFirstTouch(Manager, TouchingCharacter))
	{
		return;
	}

	bTaken = true;
	Manager.EndRestartContext();
	Manager.StartNoRetouchRestriction(TouchingCharacter);
	Manager.StartAttackRunReleaseForTeam(TouchingCharacter->GetTeam());
	Manager.RequestMatchStateTransition(ESoccerMatchStateTransition::Playing);

	ASoccerDebugManager::Message(
		&Manager,
		ESoccerDebugCategory::Restarts,
		TEXT("PENAL: primer toque registrado; remate valido y vuelve el juego"),
		FColor::Green
	);
}

ASoccerAICharacter* FSoccerPenaltyKickRestart::FindAITaker(
	const ASoccerMatchManager& Manager,
	ESoccerTeam Team
) const
{
	UWorld* World = Manager.GetWorld();
	if (World == nullptr)
	{
		return nullptr;
	}

	ASoccerAICharacter* Best = nullptr;
	float BestScore = TNumericLimits<float>::Max();

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

		float PlayerRolePenalty = 0.0f;
		if (PlayerRole == ESoccerPlayerRole::Forward)
		{
			PlayerRolePenalty = 0.0f;
		}
		else if (PlayerRole == ESoccerPlayerRole::Midfielder)
		{
			PlayerRolePenalty = 100.0f;
		}
		else
		{
			PlayerRolePenalty = 220.0f;
		}

		const float Score =
			FVector::Dist2D(Candidate->GetActorLocation(), SpotLocation) +
			PlayerRolePenalty;

		if (Score < BestScore)
		{
			BestScore = Score;
			Best = Candidate;
		}
	}

	return Best;
}

FVector FSoccerPenaltyKickRestart::GetSpotLocation(
	const ASoccerMatchManager& Manager,
	ESoccerTeam AttackingTeam
) const
{
	const float GoalLineSign =
		Manager.GetOpponentGoalLineSign(AttackingTeam);
	const float BallRadius = IsValid(Manager.SoccerBall)
		? Manager.SoccerBall->GetBallRadiusCm()
		: 22.0f;

	if (const ASoccerField* Field = Manager.GetSoccerField())
	{
		return Field->GetPenaltySpotWorldLocation(
			GoalLineSign,
			BallRadius
		);
	}

	return SoccerFieldDimensions::GetPenaltySpotLocalLocation(
		GoalLineSign,
		BallRadius
	);
}

FVector FSoccerPenaltyKickRestart::GetRunUpLocation(const ASoccerMatchManager& Manager) const
{
	const FVector Goal = Manager.GetOpponentGoalReferenceLocation(RestartTeam);
	FVector AttackDirection = Goal - Manager.GetOwnGoalReferenceLocation(RestartTeam);
	AttackDirection.Z = 0.0f;
	AttackDirection = AttackDirection.GetSafeNormal();

	FVector RunUp = SpotLocation -
		AttackDirection * FMath::Max(50.0f, Manager.PenaltyKickRunUpDistance);
	RunUp.Z = SpotLocation.Z;
	return RunUp;
}

FVector FSoccerPenaltyKickRestart::GetShotTargetLocation(
	const ASoccerMatchManager& Manager
) const
{
	const float GoalLineSign =
		Manager.GetOpponentGoalLineSign(RestartTeam);
	FVector Goal = Manager.GetOpponentGoalReferenceLocation(RestartTeam);

	if (const ASoccerField* Field = Manager.GetSoccerField())
	{
		Goal = Field->GetGoalCenterWorldLocation(GoalLineSign);
	}

	const FVector Right = Manager.GetFieldRightDirectionForTeam(RestartTeam);
	const float MaxLateral = FMath::Max(
		0.0f,
		SoccerFieldDimensions::GoalHalfWidthCm -
		FMath::Max(0.0f, Manager.PenaltyKickAITargetSideMargin)
	);

	const float SideSign = FMath::RandBool() ? 1.0f : -1.0f;
	const float Lateral = SideSign * FMath::FRandRange(
		MaxLateral * 0.35f,
		MaxLateral
	);
	Goal += Right * Lateral;
	Goal.Z = SpotLocation.Z;
	return Goal;
}

FVector FSoccerPenaltyKickRestart::GetDefendingGoalkeeperCenterLocation(
	const ASoccerMatchManager& Manager
) const
{
	if (!GoalkeeperAI.IsValid())
	{
		return FVector::ZeroVector;
	}

	const ASoccerAICharacter* Goalkeeper = GoalkeeperAI.Get();
	const float GoalLineSign =
		Manager.GetOwnGoalLineSign(Goalkeeper->GetTeam());

	if (const ASoccerField* Field = Manager.GetSoccerField())
	{
		FVector GoalCenter = Field->GetGoalCenterWorldLocation(GoalLineSign);
		GoalCenter.Z = Goalkeeper->GetActorLocation().Z;
		return GoalCenter;
	}

	return FVector(
		SoccerFieldDimensions::GetGoalLineX(GoalLineSign),
		SoccerFieldDimensions::CenterY,
		Goalkeeper->GetActorLocation().Z
	);
}

bool FSoccerPenaltyKickRestart::IsDefendingGoalkeeperReady(
	const ASoccerMatchManager& Manager
) const
{
	if (!GoalkeeperAI.IsValid())
	{
		return false;
	}

	const ASoccerAICharacter* Goalkeeper = GoalkeeperAI.Get();
	const FVector GoalCenter = GetDefendingGoalkeeperCenterLocation(Manager);
	if (GoalCenter.IsNearlyZero())
	{
		return false;
	}

	if (FVector::Dist2D(Goalkeeper->GetActorLocation(), GoalCenter) >
		FMath::Max(5.0f, Manager.PenaltyKickGoalkeeperCenterAcceptanceRadius))
	{
		return false;
	}

	const UCharacterMovementComponent* Movement = Goalkeeper->GetCharacterMovement();
	return Movement == nullptr ||
		Movement->Velocity.Size2D() <=
		FMath::Max(0.0f, Manager.PenaltyKickGoalkeeperReadyMaximumSpeed);
}

void FSoccerPenaltyKickRestart::RecoverDefendingGoalkeeperToCenter(
	ASoccerMatchManager& Manager
) const
{
	if (!GoalkeeperAI.IsValid())
	{
		return;
	}

	ASoccerAICharacter* Goalkeeper = GoalkeeperAI.Get();
	const FVector GoalCenter = GetDefendingGoalkeeperCenterLocation(Manager);
	if (GoalCenter.IsNearlyZero())
	{
		return;
	}

	Goalkeeper->StopGoalkeeperActionMontage(0.05f);

	if (UCharacterMovementComponent* Movement = Goalkeeper->GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
	}

	Goalkeeper->SetActorLocation(
		GoalCenter,
		false,
		nullptr,
		ETeleportType::TeleportPhysics
	);

	FVector FaceDirection = SpotLocation - GoalCenter;
	FaceDirection.Z = 0.0f;
	if (!FaceDirection.IsNearlyZero())
	{
		Goalkeeper->SetActorRotation(FaceDirection.Rotation());
	}
}

FVector FSoccerPenaltyKickRestart::EnforceLegalOutfieldTarget(
	const ASoccerMatchManager& Manager,
	const ASoccerAICharacter* SoccerAICharacter,
	const FVector& DesiredTarget
) const
{
	if (!IsValid(SoccerAICharacter))
	{
		return DesiredTarget;
	}

	const ASoccerField* Field = Manager.GetSoccerField();
	FVector LocalTarget = IsValid(Field)
		? Field->WorldToPitchLocal(DesiredTarget)
		: DesiredTarget;
	const float DefendingGoalLineSign =
		Manager.GetOpponentGoalLineSign(RestartTeam);
	const float InwardSign = -SoccerFieldDimensions::NormalizeGoalLineSign(
		DefendingGoalLineSign
	);
	const float GoalLineX = SoccerFieldDimensions::GetGoalLineX(
		DefendingGoalLineSign
	);

	// Primero garantiza que el objetivo quede detrás del frente del área.
	const float MinimumAreaDepth =
		SoccerFieldDimensions::PenaltyAreaDepthCm +
		FMath::Max(100.0f, Manager.PenaltyKickOtherPlayersExtraDepth);
	const float CurrentAreaDepth =
		(LocalTarget.X - GoalLineX) * InwardSign;

	if (CurrentAreaDepth < MinimumAreaDepth)
	{
		LocalTarget.X = GoalLineX + InwardSign * MinimumAreaDepth;
	}

	// El margen nunca puede ser menor que el radio de aceptación del MoveTo;
	// de lo contrario el bot podría dar su movimiento por terminado dentro de
	// la distancia reglamentaria aunque su destino matemático fuese legal.
	const float SafeArcClearance = FMath::Max(
		FMath::Max(20.0f, Manager.PenaltyKickOtherPlayersArcClearance),
		FMath::Max(5.0f, Manager.PenaltyKickOtherPlayersMoveAcceptanceRadius) +
			20.0f
	);
	const float TeamRadialSeparation =
		SoccerAICharacter->GetTeam() == RestartTeam
		? FMath::Max(50.0f, Manager.PenaltyKickTeamLineSeparation)
		: 0.0f;
	const float RequiredDistance =
		SoccerFieldDimensions::PenaltyArcRadiusCm +
		SafeArcClearance +
		TeamRadialSeparation;
	const FVector LocalSpot =
		SoccerFieldDimensions::GetPenaltySpotLocalLocation(
			DefendingGoalLineSign,
			LocalTarget.Z
		);
	FVector FromSpot = LocalTarget - LocalSpot;
	FromSpot.Z = 0.0f;

	if (FromSpot.SizeSquared2D() < RequiredDistance * RequiredDistance)
	{
		if (!FromSpot.Normalize())
		{
			FromSpot = FVector(InwardSign, 0.0f, 0.0f);
		}

		LocalTarget = LocalSpot + FromSpot * RequiredDistance;
	}

	FVector Result = IsValid(Field)
		? Field->PitchLocalToWorld(LocalTarget)
		: LocalTarget;
	Result.Z = SoccerAICharacter->GetActorLocation().Z;
	return Result;
}

FVector FSoccerPenaltyKickRestart::BuildMoveLocation(
	const ASoccerMatchManager& Manager,
	const ASoccerAICharacter* SoccerAICharacter
) const
{
	if (!IsActive(Manager) || !IsValid(SoccerAICharacter))
	{
		return FVector::ZeroVector;
	}

	if (SoccerAICharacter == GoalkeeperAI.Get())
	{
		return GetDefendingGoalkeeperCenterLocation(Manager);
	}

	if (SoccerAICharacter == TakerAI.Get())
	{
		FVector Target =
			Manager.MatchPlayState == ESoccerMatchPlayState::PenaltyKickTaking
			? SpotLocation
			: GetRunUpLocation(Manager);
		Target.Z = SoccerAICharacter->GetActorLocation().Z;
		return Target;
	}

	const ESoccerTeam CharacterTeam = SoccerAICharacter->GetTeam();
	const ESoccerPlayerRole PlayerRole = SoccerAICharacter->GetPlayerRole();

	if (CharacterTeam == RestartTeam && PlayerRole == ESoccerPlayerRole::Goalkeeper)
	{
		const FVector OwnGoal = Manager.GetOwnGoalReferenceLocation(CharacterTeam);
		FVector TowardField = Manager.GetOpponentGoalReferenceLocation(CharacterTeam) - OwnGoal;
		TowardField.Z = 0.0f;
		TowardField = TowardField.GetSafeNormal();

		FVector Target = OwnGoal +
			TowardField * FMath::Max(0.0f, Manager.PenaltyKickAttackingGoalkeeperForwardOffset);
		Target.Z = SoccerAICharacter->GetActorLocation().Z;
		return Target;
	}

	UWorld* World = Manager.GetWorld();
	if (World == nullptr)
	{
		return SoccerAICharacter->GetActorLocation();
	}

	const ESoccerTeam DefendingTeam = Manager.GetOppositeTeam(RestartTeam);
	const FVector DefendingGoal = Manager.GetOwnGoalReferenceLocation(DefendingTeam);
	FVector AwayFromDefendingGoal = Manager.GetOwnGoalReferenceLocation(RestartTeam) - DefendingGoal;
	AwayFromDefendingGoal.Z = 0.0f;
	AwayFromDefendingGoal = AwayFromDefendingGoal.GetSafeNormal();

	const FVector FieldRight = Manager.GetFieldRightDirectionForTeam(RestartTeam);

	TArray<ASoccerAICharacter*> SlotPlayers;
	for (TActorIterator<ASoccerAICharacter> It(World); It; ++It)
	{
		ASoccerAICharacter* Candidate = *It;
		if (
			!IsValid(Candidate) ||
			Candidate->GetTeam() != CharacterTeam ||
			Candidate == GoalkeeperAI.Get() ||
			Candidate == TakerAI.Get() ||
			Candidate->GetPlayerRole() == ESoccerPlayerRole::Goalkeeper
		)
		{
			continue;
		}

		SlotPlayers.Add(Candidate);
	}

	SlotPlayers.Sort(
		[&DefendingGoal, &FieldRight](const ASoccerAICharacter& A, const ASoccerAICharacter& B)
		{
			const float LateralA = FVector::DotProduct(A.GetActorLocation() - DefendingGoal, FieldRight);
			const float LateralB = FVector::DotProduct(B.GetActorLocation() - DefendingGoal, FieldRight);
			if (!FMath::IsNearlyEqual(LateralA, LateralB, 1.0f))
			{
				return LateralA < LateralB;
			}
			return A.GetUniqueID() < B.GetUniqueID();
		}
	);

	int32 SlotIndex = SlotPlayers.IndexOfByKey(const_cast<ASoccerAICharacter*>(SoccerAICharacter));
	if (SlotIndex == INDEX_NONE)
	{
		SlotIndex = 0;
	}

	const int32 SlotCount = FMath::Max(1, SlotPlayers.Num());
	const float CenteredSlot =
		static_cast<float>(SlotIndex) - (static_cast<float>(SlotCount - 1) * 0.5f);
	const float LateralOffset =
		CenteredSlot * FMath::Max(100.0f, Manager.PenaltyKickSlotLateralSpacing);

	const float BaseDepth =
		SoccerFieldDimensions::PenaltyAreaDepthCm +
		FMath::Max(100.0f, Manager.PenaltyKickOtherPlayersExtraDepth);

	const bool bAttackingTeamPlayer = CharacterTeam == RestartTeam;
	const float TeamDepthOffset =
		bAttackingTeamPlayer
		? FMath::Max(50.0f, Manager.PenaltyKickTeamLineSeparation)
		: 0.0f;

	const float StaggerDepth = (SlotIndex % 2 == 0) ? 0.0f : 90.0f;

	FVector Target = DefendingGoal +
		AwayFromDefendingGoal * (BaseDepth + TeamDepthOffset + StaggerDepth) +
		FieldRight * LateralOffset;
	Target.Z = SoccerAICharacter->GetActorLocation().Z;
	return EnforceLegalOutfieldTarget(
		Manager,
		SoccerAICharacter,
		Target
	);
}

void FSoccerPenaltyKickRestart::ResetRuntimeState()
{
	RestartTeam = ESoccerTeam::PlayerTeam;
	SpotLocation = FVector::ZeroVector;
	IncidentLocation = FVector::ZeroVector;
	SetupStartTime = 0.0f;
	Taker.Reset();
	TakerAI.Reset();
	TakerHuman.Reset();
	GoalkeeperAI.Reset();
	bUsesHumanTaker = false;
	bTaken = false;
}
