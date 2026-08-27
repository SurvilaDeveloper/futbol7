#pragma once

#include "CoreMinimal.h"
#include "SoccerMatchState.h"
#include "SoccerRestartState.h"
#include "SoccerTeamTypes.h"

class FSoccerBallOutOfPlayDelayState final : public ISoccerMatchState
{
public:
	FSoccerBallOutOfPlayDelayState(
		ESoccerRestartType InRestartType,
		ESoccerTeam InRestartTeam,
		const FVector& InRestartReferenceLocation,
		const FVector& InThrowInInwardDirection,
		float InGoalLineSign
	)
		: RestartType(InRestartType)
		, RestartTeam(InRestartTeam)
		, RestartReferenceLocation(InRestartReferenceLocation)
		, ThrowInInwardDirection(InThrowInInwardDirection)
		, GoalLineSign(InGoalLineSign)
	{
	}

	virtual ESoccerMatchStateId GetStateId() const override
	{
		return ESoccerMatchStateId::BallOutOfPlayDelay;
	}

	virtual ESoccerStatePhase GetPhase() const override
	{
		return ESoccerStatePhase::None;
	}

	virtual bool Enter(ASoccerMatchManager& Manager) override;
	virtual void Tick(ASoccerMatchManager& Manager, float DeltaTime) override;

private:
	ESoccerRestartType RestartType = ESoccerRestartType::None;
	ESoccerTeam RestartTeam = ESoccerTeam::PlayerTeam;
	FVector RestartReferenceLocation = FVector::ZeroVector;
	FVector ThrowInInwardDirection = FVector::ZeroVector;
	float GoalLineSign = 0.0f;
	float ElapsedSeconds = 0.0f;
};
