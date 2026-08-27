#pragma once

#include "SoccerMatchState.h"

class FSoccerCornerExecutionState final : public ISoccerMatchState
{
public:
    virtual ESoccerMatchStateId GetStateId() const override { return ESoccerMatchStateId::CornerExecution; }
    virtual ESoccerStatePhase GetPhase() const override { return ESoccerStatePhase::Execution; }
    virtual bool Enter(ASoccerMatchManager& Manager) override;
    virtual void Tick(ASoccerMatchManager& Manager, float DeltaTime) override;
};
