#pragma once

#include "CoreMinimal.h"
#include "SoccerMatchState.h"
#include "SoccerTeamTypes.h"

class FSoccerPenaltyFoulDelayState final : public ISoccerMatchState
{
public:
	FSoccerPenaltyFoulDelayState(
		ESoccerTeam InRestartTeam,
		const FVector& InIncidentLocation
	)
		: RestartTeam(InRestartTeam)
		, IncidentLocation(InIncidentLocation)
	{
	}

	virtual ESoccerMatchStateId GetStateId() const override
	{
		return ESoccerMatchStateId::PenaltyFoulDelay;
	}

	virtual ESoccerStatePhase GetPhase() const override
	{
		return ESoccerStatePhase::None;
	}

	virtual bool Enter(ASoccerMatchManager& Manager) override;
	virtual void Tick(ASoccerMatchManager& Manager, float DeltaTime) override;

private:
	ESoccerTeam RestartTeam = ESoccerTeam::PlayerTeam;
	FVector IncidentLocation = FVector::ZeroVector;
	float ElapsedSeconds = 0.0f;
};
