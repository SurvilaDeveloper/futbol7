#pragma once

#include "SoccerMatchState.h"

class FSoccerThrowInPreparationState final : public ISoccerMatchState
{
public:
    virtual ESoccerMatchStateId GetStateId() const override { return ESoccerMatchStateId::ThrowInPreparation; }
    virtual ESoccerStatePhase GetPhase() const override { return ESoccerStatePhase::Preparation; }
    virtual bool Enter(ASoccerMatchManager& Manager) override;
    virtual void Tick(ASoccerMatchManager& Manager, float DeltaTime) override;
};
