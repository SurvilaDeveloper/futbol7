#pragma once

#include "CoreMinimal.h"

class ASoccerMatchManager;

// Explicit match-state identifiers. The migration starts with Penalty and will
// expand one independent family at a time (corner, goal kick, offside, fault, etc.).
enum class ESoccerMatchStateId : uint8
{
	None,
	Playing,
	BallOutOfPlayDelay,
	PenaltyFoulDelay,
	OffsideReviewFreeze,
	PenaltyConfiguration,
	PenaltyPreparation,
	PenaltyExecution,
	OffsideConfiguration,
	OffsidePreparation,
	OffsideExecution,
	FaultConfiguration,
	FaultPreparation,
	FaultExecution,
	CornerConfiguration,
	CornerPreparation,
	CornerExecution,
	GoalKickConfiguration,
	GoalKickPreparation,
	GoalKickExecution,
	ThrowInConfiguration,
	ThrowInPreparation,
	ThrowInExecution,
	KickoffConfiguration,
	KickoffPreparation,
	KickoffExecution
};

// State changes are requested while a state is ticking and applied by the
// MatchManager only after Tick() returns. This prevents destroying a state
// object while one of its own methods is still executing.

enum class ESoccerStatePhase : uint8
{
	None,
	Configuration,
	Preparation,
	Execution
};

enum class ESoccerMatchStateTransition : uint8
{
	None,
	PenaltyConfigurationAfterFoulDelay,
	PenaltyPreparation,
	PenaltyExecution,
	OffsidePreparation,
	OffsideExecution,
	FaultPreparation,
	FaultExecution,
	CornerPreparation,
	CornerExecution,
	GoalKickPreparation,
	GoalKickExecution,
	ThrowInPreparation,
	ThrowInExecution,
	KickoffPreparation,
	KickoffExecution,
	BallOutOfPlayDelay,
	CompleteBallOutOfPlayDelay,
	OffsideReviewFreeze,
	OffsideConfigurationAfterFreeze,
	Playing
};

class ISoccerMatchState
{
public:
	virtual ~ISoccerMatchState() = default;

	virtual ESoccerMatchStateId GetStateId() const = 0;
	virtual ESoccerStatePhase GetPhase() const = 0;
	virtual bool Enter(ASoccerMatchManager& Manager) = 0;
	virtual void Tick(ASoccerMatchManager& Manager, float DeltaTime) = 0;
	virtual void Exit(ASoccerMatchManager& Manager) {}
};
