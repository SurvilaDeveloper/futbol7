#include "SoccerPenaltyFoulDelayState.h"

#include "SoccerMatchManager.h"
#include "SoccerBall.h"
#include "SoccerDebugManager.h"

#include "Engine/World.h"

bool FSoccerPenaltyFoulDelayState::Enter(ASoccerMatchManager& Manager)
{
	UWorld* World = Manager.GetWorld();
	if (World == nullptr || !IsValid(Manager.SoccerBall))
	{
		return false;
	}

	// The foul has already been awarded. From this point on, boundary restarts
	// or stale restart contexts must not steal the incident while we let the
	// physical aftermath of the tackle finish on screen.
	Manager.CancelBallOutOfPlayDelay();
	Manager.CancelThrowInRestart();
	Manager.CancelGoalLineRestart();
	Manager.DestroyActiveRestartHumanRestrictionIndicator();

	Manager.ClearPendingOffsideSnapshot();
	Manager.ClearNoRetouchRestriction();
	Manager.ClearAttackRunRelease();
	Manager.ClearOpenPlayPassIntent();
	Manager.ClearAttackState();
	Manager.ClearAssignedAI();

	// Stop tactical possession without touching tackle/fall/evasion animation
	// state. The ball remains where the foul actually happened and keeps its
	// current physics until penalty configuration starts.
	Manager.ReleaseAllAIBallPossessions();
	Manager.ReleaseAllHumanBallPossessions();
	Manager.PossessingCharacter = nullptr;
	Manager.PossessionTeam = ESoccerPossessionTeam::None;

	Manager.PendingPenaltyFoulRestartTeam = RestartTeam;
	Manager.PendingPenaltyFoulIncidentLocation = IncidentLocation;
	Manager.MatchStateUpdateAccumulator = 0.0f;
	Manager.MatchPlayState = ESoccerMatchPlayState::PenaltyFoulDelay;

	Manager.SoccerBall->SetActorEnableCollision(true);
	Manager.SoccerBall->SetPossessed(false);
	ElapsedSeconds = 0.0f;

	ASoccerDebugManager::Message(
		&Manager,
		ESoccerDebugCategory::Restarts,
		FString::Printf(
			TEXT("FALTA PARA PENAL: configuracion en %.2f s"),
			FMath::Max(0.0f, Manager.PenaltyFoulContinuationDuration)
		),
		FColor::Yellow
	);

	return true;
}

void FSoccerPenaltyFoulDelayState::Tick(
	ASoccerMatchManager& Manager,
	float DeltaTime
)
{
	ElapsedSeconds += FMath::Max(0.0f, DeltaTime);

	if (
		ElapsedSeconds >=
			FMath::Max(0.0f, Manager.PenaltyFoulContinuationDuration)
	)
	{
		Manager.RequestMatchStateTransition(
			ESoccerMatchStateTransition::PenaltyConfigurationAfterFoulDelay
		);
	}
}
