#pragma once

#include "CoreMinimal.h"
#include "SoccerMatchState.h"

class FSoccerKickoffExecutionState final : public ISoccerMatchState
{
public:
	virtual ESoccerMatchStateId GetStateId() const override
	{
		return ESoccerMatchStateId::KickoffExecution;
	}

	virtual ESoccerStatePhase GetPhase() const override
	{
		return ESoccerStatePhase::Execution;
	}

	virtual bool Enter(ASoccerMatchManager& Manager) override;
	virtual void Tick(ASoccerMatchManager& Manager, float DeltaTime) override;
};
