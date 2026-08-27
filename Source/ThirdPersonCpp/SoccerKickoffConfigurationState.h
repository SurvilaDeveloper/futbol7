#pragma once

#include "CoreMinimal.h"
#include "SoccerMatchState.h"
#include "SoccerTeamTypes.h"

class FSoccerKickoffConfigurationState final : public ISoccerMatchState
{
public:
	explicit FSoccerKickoffConfigurationState(ESoccerTeam InRestartTeam)
		: RestartTeam(InRestartTeam)
	{
	}

	virtual ESoccerMatchStateId GetStateId() const override
	{
		return ESoccerMatchStateId::KickoffConfiguration;
	}

	virtual ESoccerStatePhase GetPhase() const override
	{
		return ESoccerStatePhase::Configuration;
	}

	virtual bool Enter(ASoccerMatchManager& Manager) override;
	virtual void Tick(ASoccerMatchManager& Manager, float DeltaTime) override;

private:
	ESoccerTeam RestartTeam = ESoccerTeam::PlayerTeam;
	bool bConfigured = false;
};
