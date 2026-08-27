#pragma once

#include "CoreMinimal.h"
#include "SoccerMatchState.h"
#include "SoccerTeamTypes.h"

class FSoccerOffsideReviewFreezeState final : public ISoccerMatchState
{
public:
	FSoccerOffsideReviewFreezeState(
		ESoccerTeam InRestartTeam,
		const FVector& InRestartLocation
	)
		: RestartTeam(InRestartTeam)
		, RestartLocation(InRestartLocation)
	{
	}

	virtual ESoccerMatchStateId GetStateId() const override
	{
		return ESoccerMatchStateId::OffsideReviewFreeze;
	}

	virtual ESoccerStatePhase GetPhase() const override
	{
		return ESoccerStatePhase::None;
	}

	virtual bool Enter(ASoccerMatchManager& Manager) override;
	virtual void Tick(ASoccerMatchManager& Manager, float DeltaTime) override;
	virtual void Exit(ASoccerMatchManager& Manager) override;

private:
	ESoccerTeam RestartTeam = ESoccerTeam::PlayerTeam;
	FVector RestartLocation = FVector::ZeroVector;
	bool bSkipPresentation = false;
};
