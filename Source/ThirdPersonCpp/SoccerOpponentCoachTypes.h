#pragma once

#include "CoreMinimal.h"
#include "SoccerOpponentCoachTypes.generated.h"

/*
 * High-level state of the opponent coach. The coach never issues movement
 * commands directly: it only chooses among the same formation/tactical APIs
 * already available to the human team's coach screen.
 */
UENUM(BlueprintType)
enum class ESoccerOpponentCoachMode : uint8
{
	Baseline UMETA(DisplayName = "Baseline"),
	ProtectLead UMETA(DisplayName = "Protect Lead"),
	LockDown UMETA(DisplayName = "Lock Down"),
	ChaseGame UMETA(DisplayName = "Chase Game"),
	AllOutAttack UMETA(DisplayName = "All Out Attack")
};
