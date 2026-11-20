#pragma once

#include "CoreMinimal.h"
#include "SoccerMatchManager.h"

/** Portable value-only record: never stores Actor/UObject references. */
struct FSoccerArchivedMatch5L
{
    FString MatchId;
    FString PlayedAt;
    FSoccerFinalMatchReport5K Report;
};

/** Stage 5L - one JSON per completed match in ProjectSavedDir/SoccerMatchHistory. */
class FSoccerMatchHistory5L
{
public:
    static bool SaveCompletedMatch(const FSoccerFinalMatchReport5K& Report);
    static void LoadRecent(TArray<FSoccerArchivedMatch5L>& OutMatches, int32 MaxMatches = 100);
    static FString GetHistoryDirectory();
};
