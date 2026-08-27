#pragma once

#include "SoccerMatchState.h"
#include "SoccerTeamTypes.h"

class FSoccerGoalKickConfigurationState final : public ISoccerMatchState
{
public:
	FSoccerGoalKickConfigurationState(
		ESoccerTeam InRestartTeam,
		const FVector& InCrossingLocation,
		float InGoalLineSign
	)
		: RestartTeam(InRestartTeam)
		, CrossingLocation(InCrossingLocation)
		, GoalLineSign(InGoalLineSign)
	{
	}

	virtual ESoccerMatchStateId GetStateId() const override { return ESoccerMatchStateId::GoalKickConfiguration; }
	virtual ESoccerStatePhase GetPhase() const override { return ESoccerStatePhase::Configuration; }
	virtual bool Enter(ASoccerMatchManager& Manager) override;
	virtual void Tick(ASoccerMatchManager& Manager, float DeltaTime) override;

private:
	ESoccerTeam RestartTeam;
	FVector CrossingLocation = FVector::ZeroVector;
	float GoalLineSign = 1.0f;
	bool bConfigured = false;
};
