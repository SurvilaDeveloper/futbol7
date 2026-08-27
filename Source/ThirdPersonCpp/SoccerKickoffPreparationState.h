#pragma once

#include "CoreMinimal.h"
#include "SoccerMatchState.h"

class FSoccerKickoffPreparationState final : public ISoccerMatchState
{
public:
	virtual ESoccerMatchStateId GetStateId() const override
	{
		return ESoccerMatchStateId::KickoffPreparation;
	}

	virtual ESoccerStatePhase GetPhase() const override
	{
		return ESoccerStatePhase::Preparation;
	}

	virtual bool Enter(ASoccerMatchManager& Manager) override;
	virtual void Tick(ASoccerMatchManager& Manager, float DeltaTime) override;
};
