#pragma once

#include "SoccerMatchState.h"
#include "SoccerTeamTypes.h"

class FSoccerThrowInConfigurationState final : public ISoccerMatchState
{
public:
    FSoccerThrowInConfigurationState(
        ESoccerTeam InRestartTeam,
        const FVector& InTouchlineLocation,
        const FVector& InInwardDirection
    )
        : RestartTeam(InRestartTeam)
        , TouchlineLocation(InTouchlineLocation)
        , InwardDirection(InInwardDirection)
    {
    }

    virtual ESoccerMatchStateId GetStateId() const override { return ESoccerMatchStateId::ThrowInConfiguration; }
    virtual ESoccerStatePhase GetPhase() const override { return ESoccerStatePhase::Configuration; }
    virtual bool Enter(ASoccerMatchManager& Manager) override;
    virtual void Tick(ASoccerMatchManager& Manager, float DeltaTime) override;

private:
    ESoccerTeam RestartTeam;
    FVector TouchlineLocation = FVector::ZeroVector;
    FVector InwardDirection = FVector::RightVector;
    bool bConfigured = false;
};
