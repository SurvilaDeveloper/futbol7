#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SoccerSquadCatalog.generated.h"

class USoccerPlayerProfile;

/**
 * Content-side roster catalog for the technical-director layer.
 *
 * The catalog answers "which player profiles belong to this squad?". User
 * decisions such as starters, bench order and formation do NOT live here;
 * those remain in USoccerGameInstance / USoccerTeamSaveGame.
 */
UCLASS(BlueprintType)
class THIRDPERSONCPP_API USoccerSquadCatalog : public UDataAsset
{
    GENERATED_BODY()

public:
    /** Stable content identifier. Example: PLAYER_TEAM_FIRST_SQUAD. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soccer Squad")
    FName CatalogId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soccer Squad")
    FText DisplayName;

    /**
     * When several catalogs exist, the manager screen prefers the one marked as
     * the default PlayerTeam roster. Only one should normally be checked.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soccer Squad")
    bool bDefaultPlayerTeamCatalog = true;

    /** Permanent player definitions available to the coach. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soccer Squad")
    TArray<USoccerPlayerProfile*> PlayerProfiles;
};
