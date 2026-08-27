#pragma once

#include "CoreMinimal.h"
#include "SoccerTacticTypes.generated.h"

/*
 * Team-level tactical intent. These values deliberately describe HOW the team
 * wants to play, not WHERE the seven players start; formation owns structure.
 * Stage 5 stores and edits this plan, while later stages will connect each axis
 * to the existing attack/defense decision systems.
 */

UENUM(BlueprintType)
enum class ESoccerBuildUpStyle : uint8
{
	ShortPossession UMETA(DisplayName = "Short Possession"),
	Balanced UMETA(DisplayName = "Balanced"),
	Direct UMETA(DisplayName = "Direct")
};

UENUM(BlueprintType)
enum class ESoccerAttackChannel : uint8
{
	Balanced UMETA(DisplayName = "Balanced"),
	Left UMETA(DisplayName = "Left"),
	Center UMETA(DisplayName = "Center"),
	Right UMETA(DisplayName = "Right")
};

UENUM(BlueprintType)
enum class ESoccerAttackingWidth : uint8
{
	Narrow UMETA(DisplayName = "Narrow"),
	Balanced UMETA(DisplayName = "Balanced"),
	Wide UMETA(DisplayName = "Wide")
};

UENUM(BlueprintType)
enum class ESoccerAttackingTempo : uint8
{
	Patient UMETA(DisplayName = "Patient"),
	Balanced UMETA(DisplayName = "Balanced"),
	Fast UMETA(DisplayName = "Fast")
};

UENUM(BlueprintType)
enum class ESoccerAttackingTransition : uint8
{
	RetainPossession UMETA(DisplayName = "Retain Possession"),
	Balanced UMETA(DisplayName = "Balanced"),
	CounterAttack UMETA(DisplayName = "Counter Attack")
};

UENUM(BlueprintType)
enum class ESoccerDefensiveBlock : uint8
{
	Low UMETA(DisplayName = "Low"),
	Medium UMETA(DisplayName = "Medium"),
	High UMETA(DisplayName = "High")
};

UENUM(BlueprintType)
enum class ESoccerPressingIntensity : uint8
{
	Low UMETA(DisplayName = "Low"),
	Medium UMETA(DisplayName = "Medium"),
	High UMETA(DisplayName = "High")
};

UENUM(BlueprintType)
enum class ESoccerMarkingStyle : uint8
{
	Zonal UMETA(DisplayName = "Zonal"),
	Mixed UMETA(DisplayName = "Mixed"),
	ManToMan UMETA(DisplayName = "Man To Man")
};

UENUM(BlueprintType)
enum class ESoccerDefensiveTransition : uint8
{
	Regroup UMETA(DisplayName = "Regroup"),
	Balanced UMETA(DisplayName = "Balanced"),
	CounterPress UMETA(DisplayName = "Counter Press")
};

USTRUCT(BlueprintType)
struct FSoccerTeamTacticalPlan
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soccer|Tactics|Attack")
	ESoccerBuildUpStyle BuildUpStyle = ESoccerBuildUpStyle::Balanced;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soccer|Tactics|Attack")
	ESoccerAttackChannel AttackChannel = ESoccerAttackChannel::Balanced;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soccer|Tactics|Attack")
	ESoccerAttackingWidth AttackingWidth = ESoccerAttackingWidth::Balanced;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soccer|Tactics|Attack")
	ESoccerAttackingTempo AttackingTempo = ESoccerAttackingTempo::Balanced;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soccer|Tactics|Transition")
	ESoccerAttackingTransition AttackingTransition =
		ESoccerAttackingTransition::Balanced;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soccer|Tactics|Defense")
	ESoccerDefensiveBlock DefensiveBlock = ESoccerDefensiveBlock::Medium;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soccer|Tactics|Defense")
	ESoccerPressingIntensity PressingIntensity = ESoccerPressingIntensity::Medium;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soccer|Tactics|Defense")
	ESoccerMarkingStyle MarkingStyle = ESoccerMarkingStyle::Mixed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soccer|Tactics|Transition")
	ESoccerDefensiveTransition DefensiveTransition =
		ESoccerDefensiveTransition::Balanced;
};
