#pragma once

#include "CoreMinimal.h"
#include "SoccerTeamTypes.h"
#include "SoccerFormationTypes.generated.h"

/*
 * Seven-a-side formation preset. The leading 1 is always the goalkeeper.
 * These values describe team structure only; they do not issue tactical orders.
 */
UENUM(BlueprintType)
enum class ESoccerFormationSystem : uint8
{
	OneThreeTwoOne UMETA(DisplayName = "1-3-2-1"),
	OneTwoThreeOne UMETA(DisplayName = "1-2-3-1"),
	OneThreeThree UMETA(DisplayName = "1-3-3"),
	OneTwoTwoTwo UMETA(DisplayName = "1-2-2-2"),
	OneThreeOneTwo UMETA(DisplayName = "1-3-1-2"),
	OneTwoOneTwoOne UMETA(DisplayName = "1-2-1-2-1"),
	OneTwoOneThree UMETA(DisplayName = "1-2-1-3"),
	OneFourOneOne UMETA(DisplayName = "1-4-1-1"),
	OneFiveOne UMETA(DisplayName = "1-5-1")
};

/*
 * Structural line inside a formation. This is deliberately separate from
 * ESoccerPlayerRole: a midfielder can occupy a defensive, central or attacking
 * midfield line without changing the broad gameplay role already used by AI.
 */
UENUM(BlueprintType)
enum class ESoccerFormationLine : uint8
{
	Goalkeeper UMETA(DisplayName = "Goalkeeper"),
	Defense UMETA(DisplayName = "Defense"),
	DefensiveMidfield UMETA(DisplayName = "Defensive Midfield"),
	Midfield UMETA(DisplayName = "Midfield"),
	AttackingMidfield UMETA(DisplayName = "Attacking Midfield"),
	Attack UMETA(DisplayName = "Attack")
};

/*
 * Lateral lane from the perspective of the team looking toward the opponent's
 * goal. Five lanes are enough to represent the widest preset (1-5-1).
 */
UENUM(BlueprintType)
enum class ESoccerFormationLane : uint8
{
	Left UMETA(DisplayName = "Left"),
	LeftCenter UMETA(DisplayName = "Left Center"),
	Center UMETA(DisplayName = "Center"),
	RightCenter UMETA(DisplayName = "Right Center"),
	Right UMETA(DisplayName = "Right")
};

USTRUCT(BlueprintType)
struct FSoccerFormationSlot
{
	GENERATED_BODY()

	/* Stable identifier intended for future player-to-slot assignments and UI. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soccer|Formation")
	FName SlotId = NAME_None;

	/* Existing broad gameplay role. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soccer|Formation")
	ESoccerPlayerRole PlayerRole = ESoccerPlayerRole::Midfielder;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soccer|Formation")
	ESoccerFormationLine FormationLine = ESoccerFormationLine::Midfield;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soccer|Formation")
	ESoccerFormationLane FormationLane = ESoccerFormationLane::Center;

	/*
	 * 0 = own goal line, 1 = opponent goal line.
	 * This makes one preset usable by either team regardless of attack direction.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soccer|Formation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DepthAlpha = 0.5f;

	/* -1 = team-left touchline, 0 = center, +1 = team-right touchline. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soccer|Formation", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float LateralAlpha = 0.0f;
};

USTRUCT(BlueprintType)
struct FSoccerFormationDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soccer|Formation")
	ESoccerFormationSystem System = ESoccerFormationSystem::OneThreeTwoOne;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soccer|Formation")
	FText DisplayName;

	/* Exactly seven slots for this project: one goalkeeper + six field players. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soccer|Formation")
	TArray<FSoccerFormationSlot> Slots;
};
