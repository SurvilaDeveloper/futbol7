#include "SoccerOffsideReviewFreezeState.h"
#include "SoccerMatchManager.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/PlatformTime.h"

bool FSoccerOffsideReviewFreezeState::Enter(ASoccerMatchManager& Manager)
{
	UWorld* World = Manager.GetWorld();
	if (World == nullptr)
	{
		return false;
	}

	Manager.DestroyActiveRestartHumanRestrictionIndicator();
	Manager.ClearPendingOffsideSnapshot();

	Manager.PendingOffsideFreezeRestartTeam = RestartTeam;
	Manager.PendingOffsideFreezeRestartLocation = RestartLocation;
	Manager.OffsideFreezeStartRealTime = FPlatformTime::Seconds();
	Manager.MatchPlayState = ESoccerMatchPlayState::OffsideReviewFreeze;

	if (!Manager.bEnableOffsideFreezePresentation || Manager.OffsideFreezeDuration <= 0.0f)
	{
		bSkipPresentation = true;
		return true;
	}

	Manager.SpawnOffsideFreezeLine(RestartLocation);

	const bool bGameWasAlreadyPaused = UGameplayStatics::IsGamePaused(World);
	Manager.bOffsideFreezeAppliedGamePause = false;

	if (!bGameWasAlreadyPaused)
	{
		Manager.bOffsideFreezeAppliedGamePause = UGameplayStatics::SetGamePaused(World, true);
	}

	if (!bGameWasAlreadyPaused && !Manager.bOffsideFreezeAppliedGamePause)
	{
		bSkipPresentation = true;
		Manager.DestroyOffsideFreezeLine();
	}

	if (GEngine && !bSkipPresentation)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			Manager.OffsideFreezeDuration,
			FColor::Red,
			TEXT("OFFSIDE")
		);
	}

	return true;
}

void FSoccerOffsideReviewFreezeState::Tick(ASoccerMatchManager& Manager, float DeltaTime)
{
	(void)DeltaTime;

	UWorld* World = Manager.GetWorld();
	if (World == nullptr)
	{
		return;
	}

	if (bSkipPresentation)
	{
		Manager.RequestMatchStateTransition(
			ESoccerMatchStateTransition::OffsideConfigurationAfterFreeze
		);
		return;
	}

	if (Manager.bOffsideFreezeAppliedGamePause && !UGameplayStatics::IsGamePaused(World))
	{
		UGameplayStatics::SetGamePaused(World, true);
	}

	const double ElapsedRealSeconds =
		FPlatformTime::Seconds() - Manager.OffsideFreezeStartRealTime;

	if (ElapsedRealSeconds >= static_cast<double>(Manager.OffsideFreezeDuration))
	{
		Manager.RequestMatchStateTransition(
			ESoccerMatchStateTransition::OffsideConfigurationAfterFreeze
		);
	}
}

void FSoccerOffsideReviewFreezeState::Exit(ASoccerMatchManager& Manager)
{
	UWorld* World = Manager.GetWorld();

	Manager.DestroyOffsideFreezeLine();

	if (
		World != nullptr &&
		Manager.bOffsideFreezeAppliedGamePause &&
		UGameplayStatics::IsGamePaused(World)
	)
	{
		UGameplayStatics::SetGamePaused(World, false);
	}

	Manager.bOffsideFreezeAppliedGamePause = false;
	Manager.OffsideFreezeStartRealTime = -1.0;
}
