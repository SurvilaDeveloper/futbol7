#pragma once

#include "SoccerMatchState.h"
#include "SoccerTeamTypes.h"

class FSoccerFaultConfigurationState final : public ISoccerMatchState
{
public:
	FSoccerFaultConfigurationState(ESoccerTeam InRestartTeam, const FVector& InRestartLocation)
		: RestartTeam(InRestartTeam)
		, RestartLocation(InRestartLocation)
	{
	}

	virtual ESoccerMatchStateId GetStateId() const override { return ESoccerMatchStateId::FaultConfiguration; }
	virtual ESoccerStatePhase GetPhase() const override { return ESoccerStatePhase::Configuration; }
	virtual bool Enter(ASoccerMatchManager& Manager) override;
	virtual void Tick(ASoccerMatchManager& Manager, float DeltaTime) override;

private:
	ESoccerTeam RestartTeam;
	FVector RestartLocation;
	bool bConfigured = false;
};
