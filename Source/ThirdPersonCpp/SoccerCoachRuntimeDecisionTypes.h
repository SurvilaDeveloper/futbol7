#pragma once

#include "CoreMinimal.h"
#include "SoccerOpponentCoachTypes.h"
#include "SoccerCoachRuntimeDecisionTypes.generated.h"

/** Read-only evidence for the most recent in-match AI coach decision. */
USTRUCT(BlueprintType)
struct FSoccerCoachRuntimeDecision
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Soccer|AI Coach|Runtime")
    FName CoachId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category = "Soccer|AI Coach|Runtime")
    ESoccerOpponentCoachMode PreviousMode = ESoccerOpponentCoachMode::Baseline;

    UPROPERTY(BlueprintReadOnly, Category = "Soccer|AI Coach|Runtime")
    ESoccerOpponentCoachMode NewMode = ESoccerOpponentCoachMode::Baseline;

    UPROPERTY(BlueprintReadOnly, Category = "Soccer|AI Coach|Runtime")
    int32 ScoreDifference = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Soccer|AI Coach|Runtime")
    float MatchProgress = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Soccer|AI Coach|Runtime")
    float EffectiveDecisionIntervalSeconds = 0.5f;

    UPROPERTY(BlueprintReadOnly, Category = "Soccer|AI Coach|Runtime")
    float EffectiveChangeThreshold = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Soccer|AI Coach|Runtime")
    FString Reason;

    UPROPERTY(BlueprintReadOnly, Category = "Soccer|AI Coach|Runtime")
    bool bValid = false;
};
