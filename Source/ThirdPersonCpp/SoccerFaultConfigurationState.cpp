#include "SoccerFaultConfigurationState.h"

#include "SoccerMatchManager.h"
#include "SoccerFreeKickRestart.h"
#include "SoccerBall.h"
#include "SoccerDebugManager.h"

bool FSoccerFaultConfigurationState::Enter(ASoccerMatchManager& Manager)
{
	// Fault owns its own configuration path. It deliberately does not enter the
	// Offside state family even though both currently reuse low-level free-kick helpers.
	Manager.CancelBallOutOfPlayDelay();
	Manager.CancelThrowInRestart();
	Manager.CancelGoalLineRestart();
	Manager.DestroyActiveRestartHumanRestrictionIndicator();
	Manager.ClearPendingOffsideSnapshot();
	Manager.ClearNoRetouchRestriction();
	Manager.ClearAttackRunRelease();
	Manager.ClearAssignedAI();
	Manager.ReleaseAllAIBallPossessions();
	Manager.ReleaseAllHumanBallPossessions();

	if (!Manager.FreeKickRestart.Configure(
		Manager,
		ESoccerRestartType::DirectFreeKick,
		RestartTeam,
		RestartLocation
	))
	{
		ASoccerDebugManager::Message(
			&Manager,
			ESoccerDebugCategory::Restarts,
			TEXT("FAULT CONFIGURATION FALLIDA"),
			FColor::Red
		);
		return false;
	}

	// Once the referee awards a fault, the restart location becomes authoritative.
	// A sliding-tackle contact tracker may still be finishing its last frames, so
	// pin the ball immediately before any delayed tackle response can leave it at
	// the post-collision location.
	if (IsValid(Manager.SoccerBall))
	{
		const FVector& GroundedRestartLocation =
			Manager.FreeKickRestart.GetRestartLocation();
		Manager.SoccerBall->SetPossessed(false);
		Manager.SoccerBall->StopBallKeepingPhysics();
		Manager.SoccerBall->SetActorLocation(
			GroundedRestartLocation,
			false,
			nullptr,
			ETeleportType::TeleportPhysics
		);
	}

	bConfigured = true;
	ASoccerDebugManager::Message(
		&Manager,
		ESoccerDebugCategory::Restarts,
		TEXT("FAULT CONFIGURATION: ejecutor y receptor seleccionados"),
		FColor::Yellow
	);
	return true;
}

void FSoccerFaultConfigurationState::Tick(ASoccerMatchManager& Manager, float DeltaTime)
{
	(void)DeltaTime;
	if (bConfigured)
	{
		Manager.RequestMatchStateTransition(ESoccerMatchStateTransition::FaultPreparation);
	}
}
