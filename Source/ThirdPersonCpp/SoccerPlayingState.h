#pragma once

#include "CoreMinimal.h"
#include "SoccerMatchState.h"

class FSoccerPlayingState final : public ISoccerMatchState
{
public:
	virtual ESoccerMatchStateId GetStateId() const override
	{
		return ESoccerMatchStateId::Playing;
	}

	virtual ESoccerStatePhase GetPhase() const override
	{
		return ESoccerStatePhase::None;
	}

	virtual bool Enter(ASoccerMatchManager& Manager) override;
	virtual void Tick(ASoccerMatchManager& Manager, float DeltaTime) override;
	virtual void Exit(ASoccerMatchManager& Manager) override;
};
