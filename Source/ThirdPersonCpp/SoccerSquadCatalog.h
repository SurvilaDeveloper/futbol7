#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SoccerSquadCatalog.generated.h"

class USoccerPlayerProfile;
class USoccerClubProfile;

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

    /** Club that owns this initial content-side roster. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soccer Squad | Club")
    USoccerClubProfile* ClubProfile = nullptr;

    /** Preferred club when the Director Technical screen starts. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soccer Squad | Club")
    bool bDefaultHumanControlledClub = false;

    /**
     * Legacy compatibility flag from the single-PlayerTeam model. Existing
     * assets keep working; new club catalogs should use
     * bDefaultHumanControlledClub instead.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soccer Squad")
    bool bDefaultPlayerTeamCatalog = false;

    /** Permanent player definitions available to the coach. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soccer Squad")
    TArray<USoccerPlayerProfile*> PlayerProfiles;

    UFUNCTION(BlueprintPure, Category = "Soccer Squad")
    FName GetClubId() const;

    UFUNCTION(BlueprintPure, Category = "Soccer Squad")
    bool HasValidClubAssociation() const;
};
