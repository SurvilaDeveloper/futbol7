#pragma once

#include "CoreMinimal.h"
#include "SoccerRestartState.h"
#include "SoccerTeamTypes.h"

class ASoccerMatchManager;
class ASoccerAICharacter;

// Runtime/state owner for goal-line restarts (goal kicks and corner kicks).
// The match manager remains the global authority and public facade while
// this module owns restart-specific runtime state. The execution/geometry
// functions will continue migrating here incrementally.
class FSoccerGoalLineRestart
{
	friend class ASoccerMatchManager;

public:
	bool IsActive(const ASoccerMatchManager& Manager) const;
	bool IsTaker(const ASoccerMatchManager& Manager, const ASoccerAICharacter* SoccerAICharacter) const;
	ESoccerGoalLineRestartType GetType() const { return RestartType; }
	ASoccerAICharacter* GetTaker() const { return TakerAI; }
	ASoccerAICharacter* GetReceiver() const { return ReceiverAI; }

	// Controlled API used by the explicit Corner state family.
	// Corner and GoalKick state families use this controlled runtime API.
	void ConfigureCorner(
		ESoccerTeam InRestartTeam,
		ESoccerTeam InDefendingTeam,
		float InGoalLineSign,
		const FVector& InCrossingLocation,
		const FVector& InBallLocation,
		float InSetupStartTime
	);
	void SetCornerParticipants(ASoccerAICharacter* InTakerAI, ASoccerAICharacter* InReceiverAI);
	bool IsCornerConfigured() const;
	bool IsCornerFinalRunActive() const { return bCornerFinalRunActive; }
	void BeginCornerFinalRunRuntime(
		const ASoccerMatchManager& Manager,
		const FVector& InRunDirection,
		const FVector& InRunThroughLocation
	);
	ERestartKickContactResult EvaluateCornerKickContact(
		const ASoccerMatchManager& Manager
	);
	bool HasCornerKickContactConfirmed() const { return CornerContactTracker.bContactConfirmed; }
	void ResetCornerFinalRunRuntime(const ASoccerMatchManager& Manager);
	const FVector& GetCornerRunDirection() const { return CornerRunDirection; }
	const FVector& GetCornerRunThroughLocation() const { return CornerRunThroughLocation; }
	void SetCornerFinalRunActive(bool bInActive) { bCornerFinalRunActive = bInActive; }
	const FVector& GetCornerOutsideStartLocation() const { return CornerOutsideStartLocation; }
	const FVector& GetKickDirection() const { return KickDirection; }
	bool IsCornerReturnToFieldActive() const { return bCornerReturnToFieldActive; }

	// Controlled API used by the explicit GoalKick state family.
	void ConfigureGoalKick(
		ESoccerTeam InRestartTeam,
		float InGoalLineSign,
		const FVector& InCrossingLocation,
		const FVector& InBallLocation,
		float InSetupStartTime
	);
	void SetGoalKickParticipants(ASoccerAICharacter* InTakerAI, ASoccerAICharacter* InReceiverAI);
	bool IsGoalKickConfigured() const;
	bool IsGoalKickFinalRunActive() const { return bGoalKickFinalRunActive; }
	void BeginGoalKickFinalRunRuntime(
		const ASoccerMatchManager& Manager,
		const FVector& InRunDirection,
		const FVector& InRunThroughLocation
	);
	ERestartKickContactResult EvaluateGoalKickContact(
		const ASoccerMatchManager& Manager
	);
	bool HasGoalKickContactConfirmed() const { return GoalKickContactTracker.bContactConfirmed; }
	void ResetGoalKickFinalRunRuntime(const ASoccerMatchManager& Manager);
	const FVector& GetGoalKickRunDirection() const { return GoalKickRunDirection; }
const FVector& GetGoalKickRunUpStartLocation() const { return GoalKickRunUpStartLocation; }
	ESoccerTeam GetRestartTeam() const { return RestartTeam; }
const FVector& GetBallLocation() const { return BallLocation; }
	float GetSetupStartTime() const { return SetupStartTime; }

	void ResetRuntime();

private:
	ASoccerAICharacter* TakerAI = nullptr;
	ASoccerAICharacter* ReceiverAI = nullptr;

	ESoccerGoalLineRestartType RestartType = ESoccerGoalLineRestartType::None;
	ESoccerTeam RestartTeam = ESoccerTeam::PlayerTeam;
	ESoccerTeam DefendingTeam = ESoccerTeam::OpponentTeam;

	FVector CrossingLocation = FVector::ZeroVector;
	FVector BallLocation = FVector::ZeroVector;
	FVector TakerMoveLocation = FVector::ZeroVector;
	FVector ReceiverMoveLocation = FVector::ZeroVector;
	FVector KickDirection = FVector::ForwardVector;

	float GoalLineSign = 1.0f;
	float SetupStartTime = -1000.0f;

	// Goal kick runtime.
	bool bGoalKickFinalRunActive = false;
	FVector GoalKickRunDirection = FVector::ForwardVector;
	FVector GoalKickRunUpStartLocation = FVector::ZeroVector;
	FVector GoalKickRunThroughLocation = FVector::ZeroVector;
	FRestartKickContactTracker GoalKickContactTracker;

	// Corner kick runtime.
	FVector CornerStagingLocation = FVector::ZeroVector;
	FVector CornerOutsideStartLocation = FVector::ZeroVector;
	FVector CornerRunThroughLocation = FVector::ZeroVector;
	bool bCornerFinalRunActive = false;
	FVector CornerRunDirection = FVector::ForwardVector;
	FRestartKickContactTracker CornerContactTracker;

	// The corner taker can remain under scripted control briefly after the
	// restart has completed so it returns to the field from outside the line.
	bool bCornerReturnToFieldActive = false;
	ASoccerAICharacter* CornerReturningTakerAI = nullptr;
	FVector CornerReturnLocation = FVector::ZeroVector;
};
