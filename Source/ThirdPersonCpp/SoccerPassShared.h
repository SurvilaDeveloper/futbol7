#pragma once
#include "CoreMinimal.h"

namespace SoccerPassShared
{
    // Original 6AN rolling-pass calibration. No new trajectory model.
    inline float GroundPassRequiredInput6AN(float DistanceCm, float Damping, float HorizontalLaunchScale)
    {
        return (DistanceCm * (Damping + 0.30f) + 250.f) /
            FMath::Max(0.1f, HorizontalLaunchScale);
    }

    inline float GroundPassInput6AN(float DistanceCm, float Damping, float HorizontalLaunchScale)
    {
        return FMath::Clamp(GroundPassRequiredInput6AN(DistanceCm, Damping, HorizontalLaunchScale), 1100.f, 4400.f);
    }

    // Original match aerial-header lane geometry, independent of teams/mode.
    inline bool IsPlayerInPassLane(const FVector& Start, const FVector& Target,
        const FVector& PlayerLocation, float LaneHalfWidth)
    {
        FVector Segment = Target - Start;
        Segment.Z = 0.f;
        const float LengthSquared = Segment.SizeSquared();
        if (LengthSquared <= KINDA_SMALL_NUMBER) return false;
        FVector ToPlayer = PlayerLocation - Start;
        ToPlayer.Z = 0.f;
        const float Alpha = FVector::DotProduct(ToPlayer, Segment) / LengthSquared;
        return Alpha > 0.08f && Alpha < 0.92f &&
            FVector::Dist2D(PlayerLocation, Start + Segment * Alpha) <= FMath::Max(0.f, LaneHalfWidth);
    }

    // Original open-play pass-intent expiry criterion. Role, team, restart
    // and possession checks remain the responsibility of each game mode.
    inline bool IsPassTravelActive(float Elapsed, float Speed, float ReceiverDistance,
        float MaxLifetime, float SlowGrace, float MinimumSpeed, float AbandonedDistance)
    {
        if (Elapsed > FMath::Max(0.1f, MaxLifetime)) return false;
        return !(Elapsed >= FMath::Max(0.f, SlowGrace) &&
            Speed <= FMath::Max(0.f, MinimumSpeed) &&
            ReceiverDistance > FMath::Max(0.f, AbandonedDistance));
    }
}
