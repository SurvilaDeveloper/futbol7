#pragma once

#include "CoreMinimal.h"
#include "SoccerMatchSetupTypes.generated.h"

/**
 * Independent description of one scheduled or manually-created encounter.
 * A future fixture can populate the same structure used today by the pre-match
 * club-selection screen.
 */
USTRUCT(BlueprintType)
struct FSoccerMatchSetup
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soccer|Match Setup")
    int32 DataVersion = 1;

    /** Optional stable fixture/match ID. None means a standalone friendly. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soccer|Match Setup")
    FName MatchId = NAME_None;

    /** Optional future competition identity. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soccer|Match Setup")
    FName CompetitionId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soccer|Match Setup")
    int32 RoundNumber = 0;

    /** Club mapped to the human-controlled PlayerTeam side for this encounter. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soccer|Match Setup")
    FName PlayerTeamClubId = NAME_None;

    /** Rival mapped to OpponentTeam for this encounter. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soccer|Match Setup")
    FName OpponentTeamClubId = NAME_None;

    /** Preserved separately because PlayerTeam is control ownership, not venue side. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soccer|Match Setup")
    bool bPlayerTeamIsHome = true;

    bool HasTwoDifferentClubs() const
    {
        return
            !PlayerTeamClubId.IsNone() &&
            !OpponentTeamClubId.IsNone() &&
            PlayerTeamClubId != OpponentTeamClubId;
    }
};
