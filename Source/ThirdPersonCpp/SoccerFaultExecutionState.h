#pragma once

#include "SoccerMatchState.h"

class FSoccerFaultExecutionState final : public ISoccerMatchState
{
public:
	virtual ESoccerMatchStateId GetStateId() const override { return ESoccerMatchStateId::FaultExecution; }
	virtual ESoccerStatePhase GetPhase() const override { return ESoccerStatePhase::Execution; }
	virtual bool Enter(ASoccerMatchManager& Manager) override;
	virtual void Tick(ASoccerMatchManager& Manager, float DeltaTime) override;
};
