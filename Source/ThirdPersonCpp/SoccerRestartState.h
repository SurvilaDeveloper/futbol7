#pragma once

#include "CoreMinimal.h"

// Shared contact result/tracker used by restart modules that need a real
// run-up contact with the ball. Keeping these outside SoccerMatchManager
// lets each restart own its runtime contact state.
enum class ERestartKickContactResult : uint8
{
	None,
	Contact,
	MissedBall
};

struct FRestartKickContactTracker
{
	bool bContactConfirmed = false;
	bool bHasPreviousTakerLocation = false;
	FVector PreviousTakerLocation = FVector::ZeroVector;
};

