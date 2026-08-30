#include "SoccerGoalLineRestart.h"
#include "SoccerMatchManager.h"
#include "SoccerAICharacter.h"

bool FSoccerGoalLineRestart::IsActive(const ASoccerMatchManager& Manager) const
{
	return
		Manager.MatchPlayState == ESoccerMatchPlayState::GoalLineRestartSetup ||
		Manager.MatchPlayState == ESoccerMatchPlayState::GoalLineRestartPositioning ||
		Manager.MatchPlayState == ESoccerMatchPlayState::GoalLineRestartTaking;
}

bool FSoccerGoalLineRestart::IsTaker(
	const ASoccerMatchManager& Manager,
	const ASoccerAICharacter* SoccerAICharacter
) const
{
	return
		IsActive(Manager) &&
		IsValid(SoccerAICharacter) &&
		SoccerAICharacter == TakerAI;
}


void FSoccerGoalLineRestart::ConfigureCorner(
	ESoccerTeam InRestartTeam,
	ESoccerTeam InDefendingTeam,
	float InGoalLineSign,
	const FVector& InCrossingLocation,
	const FVector& InBallLocation,
	float InSetupStartTime
)
{
	RestartType = ESoccerGoalLineRestartType::CornerKick;
	RestartTeam = InRestartTeam;
	DefendingTeam = InDefendingTeam;
	GoalLineSign = InGoalLineSign >= 0.0f ? 1.0f : -1.0f;
	CrossingLocation = InCrossingLocation;
	BallLocation = InBallLocation;
	SetupStartTime = InSetupStartTime;
	TakerAI = nullptr;
	ReceiverAI = nullptr;
	bAIKickMontageStarted = false;
	PendingAIKickTargetLocation = FVector::ZeroVector;
}

void FSoccerGoalLineRestart::SetCornerParticipants(
	ASoccerAICharacter* InTakerAI,
	ASoccerAICharacter* InReceiverAI
)
{
	TakerAI = InTakerAI;
	ReceiverAI = InReceiverAI;
}

bool FSoccerGoalLineRestart::IsCornerConfigured() const
{
	return
		RestartType == ESoccerGoalLineRestartType::CornerKick &&
		IsValid(TakerAI) &&
		IsValid(ReceiverAI);
}


void FSoccerGoalLineRestart::ConfigureGoalKick(
	ESoccerTeam InRestartTeam,
	float InGoalLineSign,
	const FVector& InCrossingLocation,
	const FVector& InBallLocation,
	float InSetupStartTime
)
{
	RestartType = ESoccerGoalLineRestartType::GoalKick;
	RestartTeam = InRestartTeam;
	DefendingTeam = InRestartTeam;
	GoalLineSign = InGoalLineSign >= 0.0f ? 1.0f : -1.0f;
	CrossingLocation = InCrossingLocation;
	BallLocation = InBallLocation;
	SetupStartTime = InSetupStartTime;
	TakerAI = nullptr;
	ReceiverAI = nullptr;
	bAIKickMontageStarted = false;
	PendingAIKickTargetLocation = FVector::ZeroVector;
}

void FSoccerGoalLineRestart::SetGoalKickParticipants(
	ASoccerAICharacter* InTakerAI,
	ASoccerAICharacter* InReceiverAI
)
{
	TakerAI = InTakerAI;
	ReceiverAI = InReceiverAI;
}

bool FSoccerGoalLineRestart::IsGoalKickConfigured() const
{
	return
		RestartType == ESoccerGoalLineRestartType::GoalKick &&
		IsValid(TakerAI) &&
		IsValid(ReceiverAI);
}

void FSoccerGoalLineRestart::BeginCornerFinalRunRuntime(
	const ASoccerMatchManager& Manager,
	const FVector& InRunDirection,
	const FVector& InRunThroughLocation
)
{
	CornerRunDirection = InRunDirection;
	CornerRunThroughLocation = InRunThroughLocation;
	bCornerFinalRunActive = true;
	Manager.InitializeRestartKickContactTracking(
		CornerContactTracker,
		TakerAI
	);
}

ERestartKickContactResult FSoccerGoalLineRestart::EvaluateCornerKickContact(
	const ASoccerMatchManager& Manager
)
{
	if (
		!bCornerFinalRunActive ||
		RestartType != ESoccerGoalLineRestartType::CornerKick
	)
	{
		Manager.ResetRestartKickContactTracking(CornerContactTracker);
		return ERestartKickContactResult::None;
	}

	return Manager.EvaluateRestartKickContact(
		TakerAI,
		CornerRunDirection,
		Manager.CornerKickMaxBallSurfaceGapForKick,
		Manager.CornerKickMinimumFacingDot,
		CornerContactTracker
	);
}

void FSoccerGoalLineRestart::ResetCornerFinalRunRuntime(
	const ASoccerMatchManager& Manager
)
{
	bCornerFinalRunActive = false;
	CornerRunDirection = FVector::ForwardVector;
	CornerRunThroughLocation = FVector::ZeroVector;
	Manager.ResetRestartKickContactTracking(CornerContactTracker);
}

void FSoccerGoalLineRestart::BeginGoalKickFinalRunRuntime(
	const ASoccerMatchManager& Manager,
	const FVector& InRunDirection,
	const FVector& InRunThroughLocation
)
{
	GoalKickRunDirection = InRunDirection;
	GoalKickRunThroughLocation = InRunThroughLocation;
	TakerMoveLocation = InRunThroughLocation;
	bGoalKickFinalRunActive = true;
	Manager.InitializeRestartKickContactTracking(
		GoalKickContactTracker,
		TakerAI
	);
}

ERestartKickContactResult FSoccerGoalLineRestart::EvaluateGoalKickContact(
	const ASoccerMatchManager& Manager
)
{
	if (
		!bGoalKickFinalRunActive ||
		RestartType != ESoccerGoalLineRestartType::GoalKick
	)
	{
		Manager.ResetRestartKickContactTracking(GoalKickContactTracker);
		return ERestartKickContactResult::None;
	}

	return Manager.EvaluateRestartKickContact(
		TakerAI,
		GoalKickRunDirection,
		Manager.GoalKickMaxBallSurfaceGapForKick,
		Manager.GoalKickMinimumFacingDot,
		GoalKickContactTracker
	);
}

void FSoccerGoalLineRestart::ResetGoalKickFinalRunRuntime(
	const ASoccerMatchManager& Manager
)
{
	bGoalKickFinalRunActive = false;
	GoalKickRunDirection = FVector::ForwardVector;
	GoalKickRunUpStartLocation = FVector::ZeroVector;
	GoalKickRunThroughLocation = FVector::ZeroVector;
	Manager.ResetRestartKickContactTracking(GoalKickContactTracker);
}

void FSoccerGoalLineRestart::ResetRuntime()
{
	TakerAI = nullptr;
	ReceiverAI = nullptr;
	RestartType = ESoccerGoalLineRestartType::None;
	RestartTeam = ESoccerTeam::PlayerTeam;
	DefendingTeam = ESoccerTeam::OpponentTeam;
	CrossingLocation = FVector::ZeroVector;
	BallLocation = FVector::ZeroVector;
	TakerMoveLocation = FVector::ZeroVector;
	ReceiverMoveLocation = FVector::ZeroVector;
	KickDirection = FVector::ForwardVector;
	GoalLineSign = 1.0f;
	SetupStartTime = -1000.0f;
	bAIKickMontageStarted = false;
	PendingAIKickTargetLocation = FVector::ZeroVector;

	bGoalKickFinalRunActive = false;
	GoalKickRunDirection = FVector::ForwardVector;
	GoalKickRunUpStartLocation = FVector::ZeroVector;
	GoalKickRunThroughLocation = FVector::ZeroVector;
	GoalKickContactTracker = FRestartKickContactTracker();

	CornerStagingLocation = FVector::ZeroVector;
	CornerOutsideStartLocation = FVector::ZeroVector;
	CornerRunThroughLocation = FVector::ZeroVector;
	bCornerFinalRunActive = false;
	CornerRunDirection = FVector::ForwardVector;
	CornerContactTracker = FRestartKickContactTracker();

	bCornerReturnToFieldActive = false;
	CornerReturningTakerAI = nullptr;
	CornerReturnLocation = FVector::ZeroVector;
}
