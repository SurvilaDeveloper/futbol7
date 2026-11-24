#pragma once

#include "CoreMinimal.h"

// Shared match/training arithmetic for a contested sliding tackle.
// The ball is carried by a moving player. This is deliberately separate
// from the predicted PHYSICAL trajectory used for an unpossessed ball.
// No match-state, actor or animation dependencies: it can also be called
// from the training GameMode while the tactical AIController Tick is off.
namespace SoccerTacklePredictionShared
{
    FORCEINLINE FVector PredictCarriedBallLocation(
        const FVector& BallLocation,
        const FVector& CarrierVelocity,
        float LeadSeconds)
    {
        FVector PlanarVelocity = CarrierVelocity;
        PlanarVelocity.Z = 0.0f;
        FVector TargetLocation = BallLocation + PlanarVelocity * FMath::Max(0.0f, LeadSeconds);
        TargetLocation.Z = BallLocation.Z;
        return TargetLocation;
    }

    FORCEINLINE float EstimateSlideArrivalSeconds(
        const FVector& TacklerLocation,
        const FVector& TargetLocation,
        float EstimatedAverageSlideSpeed,
        float EstimatedLaunchDelay)
    {
        return FMath::Max(0.0f, EstimatedLaunchDelay) +
            FVector::Dist2D(TacklerLocation, TargetLocation) /
            FMath::Max(1.0f, EstimatedAverageSlideSpeed);
    }
}
