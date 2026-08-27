#pragma once

#include "SoccerMatchState.h"
#include "SoccerTeamTypes.h"

class FSoccerOffsideConfigurationState final : public ISoccerMatchState
{
public:
	FSoccerOffsideConfigurationState(ESoccerTeam InRestartTeam, const FVector& InRestartLocation)
		: RestartTeam(InRestartTeam)
		, RestartLocation(InRestartLocation)
	{
	}

	virtual ESoccerMatchStateId GetStateId() const override { return ESoccerMatchStateId::OffsideConfiguration; }
	virtual ESoccerStatePhase GetPhase() const override { return ESoccerStatePhase::Configuration; }
	virtual bool Enter(ASoccerMatchManager& Manager) override;
	virtual void Tick(ASoccerMatchManager& Manager, float DeltaTime) override;

private:
	ESoccerTeam RestartTeam;
	FVector RestartLocation;
	bool bConfigured = false;
};
