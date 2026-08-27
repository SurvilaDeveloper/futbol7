#pragma once

#include "CoreMinimal.h"
#include "SoccerMatchPeriod.generated.h"

UENUM(BlueprintType)
enum class ESoccerMatchPeriod : uint8
{
	FirstHalf UMETA(DisplayName = "First Half"),
	HalfTime UMETA(DisplayName = "Half Time"),
	SecondHalf UMETA(DisplayName = "Second Half"),
	FullTime UMETA(DisplayName = "Full Time")
};
