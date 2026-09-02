#pragma once

#include "CoreMinimal.h"
#include "SoccerFormationTypes.h"
#include "SoccerTacticTypes.h"
#include "SoccerCoachMatchPlanTypes.generated.h"

USTRUCT(BlueprintType)
struct FSoccerCoachLineupDecision
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Soccer|Coach Plan")
    FName FormationSlotId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category = "Soccer|Coach Plan")
    FName PlayerId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category = "Soccer|Coach Plan")
    int32 TotalScore = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Soccer|Coach Plan")
    int32 AbilityScore = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Soccer|Coach Plan")
    int32 PositionFamiliarity = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Soccer|Coach Plan")
    int32 TacticalFitScore = 0;
};

/** Temporary pre-match decision produced by one AI coach for one club. */
USTRUCT(BlueprintType)
struct FSoccerCoachMatchPlan
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Soccer|Coach Plan")
    int32 DataVersion = 1;

    UPROPERTY(BlueprintReadOnly, Category = "Soccer|Coach Plan")
    FName ClubId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category = "Soccer|Coach Plan")
    FName CoachId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category = "Soccer|Coach Plan")
    ESoccerFormationSystem FormationSystem = ESoccerFormationSystem::OneThreeTwoOne;

    UPROPERTY(BlueprintReadOnly, Category = "Soccer|Coach Plan")
    FSoccerTeamTacticalPlan TacticalPlan;

    UPROPERTY(BlueprintReadOnly, Category = "Soccer|Coach Plan")
    TMap<FName, FName> StartingLineupBySlot;

    UPROPERTY(BlueprintReadOnly, Category = "Soccer|Coach Plan")
    TArray<FName> BenchPlayerIds;

    UPROPERTY(BlueprintReadOnly, Category = "Soccer|Coach Plan")
    TArray<FSoccerCoachLineupDecision> LineupDecisions;

    UPROPERTY(BlueprintReadOnly, Category = "Soccer|Coach Plan")
    int32 FormationPreferenceScore = 50;

    UPROPERTY(BlueprintReadOnly, Category = "Soccer|Coach Plan")
    int32 StartingLineupScore = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Soccer|Coach Plan")
    int32 OverallPlanScore = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Soccer|Coach Plan")
    bool bComplete = false;
};
