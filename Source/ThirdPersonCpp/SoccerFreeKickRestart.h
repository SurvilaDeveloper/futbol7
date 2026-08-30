#pragma once

#include "CoreMinimal.h"
#include "SoccerRestartState.h"
#include "SoccerTeamTypes.h"

class ASoccerMatchManager;
class ASoccerAICharacter;
class ASoccerCharacterBase;
class AThirdPersonCppCharacter;

// Low-level technical helper shared temporarily by the independent Offside and
// Fault state families. It owns reusable run-up/contact geometry, but it no
// longer owns the match-state lifecycle or tactical meaning of either restart.
class FSoccerFreeKickRestart
{
public:
	void ResetRuntime(ASoccerMatchManager& Manager);

	// Explicit-state helpers used independently by Offside and Fault. The state
	// families are separate even while these low-level mechanics are shared.
	bool Configure(
		ASoccerMatchManager& Manager,
		ESoccerRestartType RestartType,
		ESoccerTeam RestartTeam,
		const FVector& RestartLocation
	);
	bool EnterPreparation(ASoccerMatchManager& Manager);
	bool IsPreparationReady(ASoccerMatchManager& Manager);
	bool EnterExecution(ASoccerMatchManager& Manager);
	bool TickExecutionAndCompleteIfNeeded(ASoccerMatchManager& Manager);
	bool IsRuntimeValid() const;

	bool IsSupportedType(ESoccerRestartType RestartType) const;
	bool IsActive(const ASoccerMatchManager& Manager) const;
	bool IsTaker(const ASoccerMatchManager& Manager, const ASoccerAICharacter* SoccerAICharacter) const;
	bool IsHumanTaker(const ASoccerMatchManager& Manager, const AThirdPersonCppCharacter* HumanCharacter) const;
	bool CanHumanTakerExecute(const ASoccerMatchManager& Manager, const AThirdPersonCppCharacter* HumanCharacter) const;
	bool IsFinalRunActiveForCharacter(const ASoccerMatchManager& Manager, const ASoccerAICharacter* SoccerAICharacter) const;

	ASoccerAICharacter* GetTaker() const { return TakerAI; }
	AThirdPersonCppCharacter* GetHumanTaker() const { return HumanTaker; }
	bool IsHumanTakerClaimed() const { return bHumanTakerClaimed; }
	ASoccerAICharacter* GetReceiver() const { return ReceiverAI; }
	ESoccerTeam GetRestartTeam() const { return RestartTeam; }
	const FVector& GetRestartLocation() const { return RestartLocation; }
	bool IsFinalRunActive() const { return bFinalRunActive; }
	bool ShouldKeepBallFixedDuringExecution() const;

	FVector GetMoveLocation(const ASoccerMatchManager& Manager, const ASoccerAICharacter* SoccerAICharacter) const;
	FVector BuildReceiverMoveLocation(const ASoccerMatchManager& Manager) const;
	FVector BuildOpponentMoveLocation(const ASoccerMatchManager& Manager, const ASoccerAICharacter* SoccerAICharacter) const;
bool AreOpponentsClear(const ASoccerMatchManager& Manager) const;
	bool IsCharacterTooClose(const ASoccerMatchManager& Manager, const ASoccerCharacterBase* Character) const;

	void RecalculateRunUpGeometry(ASoccerMatchManager& Manager);

	// While the free kick is waiting, an illegal opponent first completes one
	// committed escape. Once it reaches that legal point it may make at most
	// one tactical reposition, and only through a NavMesh path that never cuts
	// back through the protected circle.
	bool UpdateOpponentPositioningAfterEscape(ASoccerMatchManager& Manager);

	// If generic restart recovery has to choose a different legal escape point,
	// adopt it as the committed free-kick target instead of reverting later.
	void AdoptOpponentRecoveryTarget(
		const ASoccerAICharacter* SoccerAICharacter,
		const FVector& LegalTargetLocation
	);

private:
	void Complete(ASoccerMatchManager& Manager);
	void BeginFinalRun(ASoccerMatchManager& Manager);
	bool IsTakerAtBallContact(ASoccerMatchManager& Manager);
	void RecoverFinalRunAfterMiss(ASoccerMatchManager& Manager);

	bool UpdateHumanTakerClaimDuringPreparation(ASoccerMatchManager& Manager);
	bool ShouldHumanKeepExecutionClaim(const ASoccerMatchManager& Manager) const;
	FVector BuildFallbackTakerHoldLocation(
		const ASoccerMatchManager& Manager,
		const ASoccerAICharacter* SoccerAICharacter
	) const;

	ASoccerAICharacter* FindClosestTakerForTeam(
		ASoccerMatchManager& Manager,
		ESoccerTeam Team,
		const FVector& InRestartLocation
	) const;

	ASoccerAICharacter* FindBestReceiverForTeam(
		ASoccerMatchManager& Manager,
		ESoccerTeam Team,
		const ASoccerAICharacter* RestartTaker,
		const FVector& InRestartLocation
	) const;

	FVector BuildTakerWaitingLocation(
		const ASoccerMatchManager& Manager,
		const ASoccerAICharacter* SoccerAICharacter
	) const;

	FVector BuildReceiverDesiredMoveLocation(
		const ASoccerMatchManager& Manager
	) const;

	FVector BuildOpponentDesiredMoveLocation(
		const ASoccerMatchManager& Manager,
		const ASoccerAICharacter* SoccerAICharacter
	) const;

	void InitializeOpponentPositioningPlan(ASoccerMatchManager& Manager);

	bool DoesNavigationPathAvoidRestartCircle(
		const ASoccerMatchManager& Manager,
		const ASoccerAICharacter* SoccerAICharacter,
		const FVector& TargetLocation,
		float ProtectedRadius
	) const;

	ESoccerRestartType RestartType = ESoccerRestartType::None;
	ESoccerTeam RestartTeam = ESoccerTeam::PlayerTeam;
	FVector RestartLocation = FVector::ZeroVector;
	float SetupStartTime = -1000.0f;

	ASoccerAICharacter* TakerAI = nullptr;
	ASoccerAICharacter* ReceiverAI = nullptr;
	AThirdPersonCppCharacter* HumanTaker = nullptr;
	bool bHumanTakerClaimed = false;
	bool bHumanExecutionAuthorized = false;

	// The selected receiver and these preparation targets are snapshots owned
	// by this restart. They are not recomputed because another player moved.
	FVector TakerWaitingLocation = FVector::ZeroVector;
	FVector ReceiverHoldLocation = FVector::ZeroVector;
	TMap<const ASoccerAICharacter*, FVector> OpponentHoldLocations;
	TSet<const ASoccerAICharacter*> OpponentsCompletingMandatoryEscape;
	TSet<const ASoccerAICharacter*> OpponentsThatUsedLegalReposition;

	bool bFinalRunActive = false;
	bool bAIKickMontageStarted = false;
	FVector PendingAIKickTargetLocation = FVector::ZeroVector;
	FVector KickDirection = FVector::ForwardVector;
	FVector RunDirection = FVector::ForwardVector;
	FVector RunUpStartLocation = FVector::ZeroVector;
	FVector RunThroughLocation = FVector::ZeroVector;
	FRestartKickContactTracker ContactTracker;
};
