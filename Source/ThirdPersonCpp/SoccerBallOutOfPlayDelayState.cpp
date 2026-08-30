#include "SoccerBallOutOfPlayDelayState.h"
#include "SoccerMatchManager.h"
#include "SoccerBall.h"
#include "SoccerDebugManager.h"

bool FSoccerBallOutOfPlayDelayState::Enter(ASoccerMatchManager& Manager)
{
	UWorld* World = Manager.GetWorld();

	const bool bSupportedRestart =
		RestartType == ESoccerRestartType::ThrowIn ||
		RestartType == ESoccerRestartType::CornerKick ||
		RestartType == ESoccerRestartType::GoalKick;

	if (World == nullptr || !IsValid(Manager.SoccerBall) || !bSupportedRestart)
	{
		return false;
	}

	Manager.CancelBallOutOfPlayDelay();
	Manager.CancelThrowInRestart();
	Manager.CancelGoalLineRestart();
	Manager.DestroyActiveRestartHumanRestrictionIndicator();

	Manager.ClearPendingOffsideSnapshot();
	Manager.ClearNoRetouchRestriction();
	Manager.ClearAttackRunRelease();
	Manager.ClearAttackState();
	Manager.ClearAssignedAI();

	Manager.ReleaseAllAIBallPossessions();
	Manager.ReleaseAllHumanBallPossessions();
	Manager.PossessingCharacter = nullptr;
	Manager.PossessionTeam = ESoccerPossessionTeam::None;

	Manager.PendingBallOutOfPlayRestartType = RestartType;
	Manager.PendingBallOutOfPlayRestartTeam = RestartTeam;
	Manager.PendingBallOutOfPlayRestartReferenceLocation = RestartReferenceLocation;
	Manager.PendingBallOutOfPlayThrowInInwardDirection = ThrowInInwardDirection.GetSafeNormal();
	Manager.PendingBallOutOfPlayGoalLineSign =
		FMath::IsNearlyZero(GoalLineSign) ? 0.0f : (GoalLineSign > 0.0f ? 1.0f : -1.0f);

	Manager.MatchStateUpdateAccumulator = 0.0f;
	Manager.MatchPlayState = ESoccerMatchPlayState::BallOutOfPlayDelay;

	const bool bThrowInTacticalPositioningStarted =
		RestartType == ESoccerRestartType::ThrowIn &&
		Manager.StageThrowInDuringBallOutOfPlayDelay(
			RestartTeam,
			RestartReferenceLocation,
			ThrowInInwardDirection
		);
	const bool bGoalLineTacticalPositioningStarted =
		(RestartType == ESoccerRestartType::CornerKick ||
		 RestartType == ESoccerRestartType::GoalKick) &&
		Manager.StageGoalLineRestartDuringBallOutOfPlayDelay(
			RestartType,
			RestartTeam,
			RestartReferenceLocation,
			GoalLineSign
		);

	Manager.SoccerBall->SetActorEnableCollision(true);
	Manager.SoccerBall->SetPossessed(false);
	ElapsedSeconds = 0.0f;

	if (ASoccerDebugManager::IsEnabled(&Manager, ESoccerDebugCategory::Restarts))
	{
		const TCHAR* RestartLabel =
			RestartType == ESoccerRestartType::ThrowIn ? TEXT("lateral") :
			RestartType == ESoccerRestartType::CornerKick ? TEXT("corner") :
			TEXT("saque de arco");

		ASoccerDebugManager::Message(
			&Manager,
			ESoccerDebugCategory::Restarts,
			FString::Printf(
				TEXT("PELOTA AFUERA: %s en %.2f s"),
				RestartLabel,
				FMath::Max(0.0f, Manager.BallOutOfPlayContinuationDuration)
			),
			FColor::Yellow
		);

		if (
			RestartType == ESoccerRestartType::ThrowIn &&
			bThrowInTacticalPositioningStarted
			)
		{
			ASoccerDebugManager::Message(
				&Manager,
				ESoccerDebugCategory::Restarts,
				TEXT("LATERAL: equipos acomodandose durante el delay"),
				FColor::Cyan
			);
		}

		if (bGoalLineTacticalPositioningStarted)
		{
			ASoccerDebugManager::Message(
				&Manager,
				ESoccerDebugCategory::Restarts,
				TEXT("LINEA DE FONDO: equipos acomodandose durante el delay"),
				FColor::Cyan
			);
		}
	}

	return true;
}

void FSoccerBallOutOfPlayDelayState::Tick(ASoccerMatchManager& Manager, float DeltaTime)
{
	ElapsedSeconds += FMath::Max(0.0f, DeltaTime);

	if (ElapsedSeconds >= FMath::Max(0.0f, Manager.BallOutOfPlayContinuationDuration))
	{
		Manager.RequestMatchStateTransition(
			ESoccerMatchStateTransition::CompleteBallOutOfPlayDelay
		);
	}
}
