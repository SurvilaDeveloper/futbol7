#pragma once

#include "SoccerMatchState.h"
#include "SoccerTeamTypes.h"

class FSoccerPenaltyConfigurationState final : public ISoccerMatchState
{
public:
	FSoccerPenaltyConfigurationState(ESoccerTeam InRestartTeam, const FVector& InIncidentLocation)
		: RestartTeam(InRestartTeam)
		, IncidentLocation(InIncidentLocation)
	{
	}

	virtual ESoccerMatchStateId GetStateId() const override
	{
		return ESoccerMatchStateId::PenaltyConfiguration;
	}

	virtual ESoccerStatePhase GetPhase() const override
	{
		return ESoccerStatePhase::Configuration;
	}

	virtual bool Enter(ASoccerMatchManager& Manager) override;
	virtual void Tick(ASoccerMatchManager& Manager, float DeltaTime) override;

private:
	ESoccerTeam RestartTeam;
	FVector IncidentLocation;
	bool bConfigured = false;
};
