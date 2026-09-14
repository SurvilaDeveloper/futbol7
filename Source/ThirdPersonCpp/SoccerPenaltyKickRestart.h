#pragma once

#include "CoreMinimal.h"
#include "SoccerTeamTypes.h"

class ASoccerMatchManager;
class ASoccerCharacterBase;
class ASoccerAICharacter;
class AThirdPersonCppCharacter;
class FSoccerPenaltyConfigurationState;
class FSoccerPenaltyPreparationState;
class FSoccerPenaltyExecutionState;

// Restart-specific module for penalty kicks.
// It owns penalty runtime state and penalty-only geometry/selection logic.
// ASoccerMatchManager remains the global match authority and public facade.
class FSoccerPenaltyKickRestart
{
	friend class FSoccerPenaltyConfigurationState;
	friend class FSoccerPenaltyPreparationState;
	friend class FSoccerPenaltyExecutionState;

public:
	void Cancel(ASoccerMatchManager& Manager);

	bool IsActive(const ASoccerMatchManager& Manager) const;
	bool IsTaker(const ASoccerCharacterBase* Character) const;
	bool IsHumanTaker(const AThirdPersonCppCharacter* Character) const;
	bool IsDefendingGoalkeeper(const ASoccerAICharacter* Character) const;
	bool AreNonParticipantsInLegalPositions(
		const ASoccerMatchManager& Manager
	) const;

	// Two-phase first-touch handling: validation happens before the generic
	// touch registration; completion happens only after that registration succeeds.
	bool CanAcceptFirstTouch(
		const ASoccerMatchManager& Manager,
		const ASoccerCharacterBase* TouchingCharacter
	) const;
	void OnFirstTouchRegistered(
		ASoccerMatchManager& Manager,
		ASoccerCharacterBase* TouchingCharacter
	);

	FVector BuildMoveLocation(
		const ASoccerMatchManager& Manager,
		const ASoccerAICharacter* SoccerAICharacter
	) const;

	FVector GetSpotLocation(
		const ASoccerMatchManager& Manager,
		ESoccerTeam AttackingTeam
	) const;

private:
	void CompleteByAI(ASoccerMatchManager& Manager);

	ASoccerAICharacter* FindAITaker(
		const ASoccerMatchManager& Manager,
		ESoccerTeam Team
	) const;

	FVector GetRunUpLocation(const ASoccerMatchManager& Manager) const;
	FVector GetShotTargetLocation(const ASoccerMatchManager& Manager) const;
	FVector GetDefendingGoalkeeperCenterLocation(const ASoccerMatchManager& Manager) const;
	bool IsDefendingGoalkeeperReady(const ASoccerMatchManager& Manager) const;
	void RecoverDefendingGoalkeeperToCenter(ASoccerMatchManager& Manager) const;
	bool IsNonParticipantInLegalPosition(
		const ASoccerMatchManager& Manager,
		const ASoccerCharacterBase* Character
	) const;
	FVector EnforceLegalOutfieldTarget(
		const ASoccerMatchManager& Manager,
		const ASoccerAICharacter* SoccerAICharacter,
		const FVector& DesiredTarget
	) const;
	void ResetRuntimeState();

private:
	ESoccerTeam RestartTeam = ESoccerTeam::PlayerTeam;
	FVector SpotLocation = FVector::ZeroVector;
	FVector IncidentLocation = FVector::ZeroVector;
	float SetupStartTime = 0.0f;

	TWeakObjectPtr<ASoccerCharacterBase> Taker;
	TWeakObjectPtr<ASoccerAICharacter> TakerAI;
	TWeakObjectPtr<AThirdPersonCppCharacter> TakerHuman;
	TWeakObjectPtr<ASoccerAICharacter> GoalkeeperAI;

	bool bUsesHumanTaker = false;
	bool bTaken = false;
};
