#pragma once

#include "SoccerMatchState.h"

class FSoccerFaultPreparationState final : public ISoccerMatchState
{
public:
	virtual ESoccerMatchStateId GetStateId() const override { return ESoccerMatchStateId::FaultPreparation; }
	virtual ESoccerStatePhase GetPhase() const override { return ESoccerStatePhase::Preparation; }
	virtual bool Enter(ASoccerMatchManager& Manager) override;
	virtual void Tick(ASoccerMatchManager& Manager, float DeltaTime) override;
};
