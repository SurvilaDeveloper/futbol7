#pragma once

#include "CoreMinimal.h"
#include "SoccerPlayerProfileTypes.h"
#include "SoccerLineupTypes.generated.h"

/**
 * Coarse UI-friendly interpretation of a player's suitability for a formation slot.
 * The numeric OverallScore remains the authoritative 0-100 value.
 */
UENUM(BlueprintType)
enum class ESoccerLineupSuitabilityBand : uint8
{
    Ideal UMETA(DisplayName = "Ideal"),
    VeryGood UMETA(DisplayName = "Very Good"),
    Good UMETA(DisplayName = "Good"),
    Acceptable UMETA(DisplayName = "Acceptable"),
    Improvised UMETA(DisplayName = "Improvised"),
    Poor UMETA(DisplayName = "Poor")
};

/**
 * Read-only assessment used by the future coach UI.
 *
 * PositionFamiliarity comes from the player's permanent position preferences.
 * AbilityScore comes from the abilities relevant to the slot's role/formation line.
 * OverallScore combines both without preventing the coach from using an improvised player.
 */
USTRUCT(BlueprintType)
struct FSoccerPlayerSlotSuitability
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Soccer|Lineup")
    bool bValid = false;

    UPROPERTY(BlueprintReadOnly, Category = "Soccer|Lineup")
    FName SlotId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category = "Soccer|Lineup")
    ESoccerPlayerNaturalPosition NaturalPosition = ESoccerPlayerNaturalPosition::CentralMidfielder;

    UPROPERTY(BlueprintReadOnly, Category = "Soccer|Lineup", meta = (ClampMin = "0", ClampMax = "100"))
    int32 PositionFamiliarity = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Soccer|Lineup", meta = (ClampMin = "0", ClampMax = "100"))
    int32 AbilityScore = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Soccer|Lineup", meta = (ClampMin = "0", ClampMax = "100"))
    int32 OverallScore = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Soccer|Lineup")
    ESoccerLineupSuitabilityBand SuitabilityBand = ESoccerLineupSuitabilityBand::Poor;
};
