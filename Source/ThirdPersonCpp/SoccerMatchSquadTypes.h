#pragma once

#include "CoreMinimal.h"
#include "SoccerTeamTypes.h"
#include "SoccerMatchSquadTypes.generated.h"

USTRUCT(BlueprintType)
struct FSoccerMatchSubstitutionRequest
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Soccer|Match Squad")
    ESoccerTeam Team = ESoccerTeam::PlayerTeam;

    UPROPERTY(BlueprintReadOnly, Category = "Soccer|Match Squad")
    FName OutgoingPlayerId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category = "Soccer|Match Squad")
    FName IncomingPlayerId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category = "Soccer|Match Squad")
    FName FormationSlotId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category = "Soccer|Match Squad")
    float RequestedAtMatchSeconds = 0.0f;
};

USTRUCT(BlueprintType)
struct FSoccerMatchSubstitutionRecord
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Soccer|Match Squad")
    ESoccerTeam Team = ESoccerTeam::PlayerTeam;

    UPROPERTY(BlueprintReadOnly, Category = "Soccer|Match Squad")
    FName OutgoingPlayerId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category = "Soccer|Match Squad")
    FName IncomingPlayerId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category = "Soccer|Match Squad")
    FName FormationSlotId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category = "Soccer|Match Squad")
    float ExecutedAtMatchSeconds = 0.0f;
};

/** Runtime-only squad state for one side of the current encounter. */
USTRUCT(BlueprintType)
struct FSoccerMatchSquadState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Soccer|Match Squad")
    FName ClubId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category = "Soccer|Match Squad")
    TMap<FName, FName> ActivePlayerByFormationSlot;

    UPROPERTY(BlueprintReadOnly, Category = "Soccer|Match Squad")
    TArray<FName> AvailableBenchPlayerIds;

    /** A substituted player cannot return in this first ruleset. */
    UPROPERTY(BlueprintReadOnly, Category = "Soccer|Match Squad")
    TArray<FName> WithdrawnPlayerIds;

    UPROPERTY(BlueprintReadOnly, Category = "Soccer|Match Squad")
    TArray<FSoccerMatchSubstitutionRecord> SubstitutionHistory;

    UPROPERTY(BlueprintReadOnly, Category = "Soccer|Match Squad")
    int32 MaximumSubstitutions = 3;

    UPROPERTY(BlueprintReadOnly, Category = "Soccer|Match Squad")
    bool bInitialized = false;

    bool HasPendingEligibleSubstitutes() const
    {
        return
            bInitialized &&
            AvailableBenchPlayerIds.Num() > 0 &&
            SubstitutionHistory.Num() < MaximumSubstitutions;
    }
};
