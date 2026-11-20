#pragma once
#include "CoreMinimal.h"
#include "SoccerMatchHistory5L.h"

/** Totals rebuilt from valid saved FULL-TIME reports. No new files are written. */
struct FSoccerClubTotals5M
{
    FString Name;
    int32 Played = 0, Wins = 0, Draws = 0, Losses = 0;
    int32 GoalsFor = 0, GoalsAgainst = 0;
    int32 Shots = 0, ShotsOnTarget = 0, Saves = 0;
    int32 Corners = 0, FreeKicks = 0;
};

/** Records with registered actions (not necessarily every appearance). */
struct FSoccerPlayerTotals5M
{
    FString Club, PlayerKey, Name;
    int32 Records = 0, Goals = 0, Assists = 0, OwnGoals = 0;
    int32 Shots = 0, ShotsOnTarget = 0, Saves = 0;
    int32 PassesAttempted = 0, PassesCompleted = 0;
    int32 Recoveries = 0, Interceptions = 0, CleanTackles = 0;
    float RatingSum = 0.0f;
};

class FSoccerHistoricalStats5M
{
public:
    static void Aggregate(const TArray<FSoccerArchivedMatch5L>& Matches,
        TArray<FSoccerClubTotals5M>& Clubs,
        TArray<FSoccerPlayerTotals5M>& Players);
};
