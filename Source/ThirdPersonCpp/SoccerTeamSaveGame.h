#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "SoccerTeamSetupTypes.h"
#include "SoccerTeamSaveGame.generated.h"

/**
 * Disk persistence container for the coach/team configuration.
 *
 * This SaveGame intentionally stores IDs and plain data only. Player DataAssets,
 * spawned characters and match-only state are resolved by later layers.
 */
UCLASS()
class THIRDPERSONCPP_API USoccerTeamSaveGame : public USaveGame
{
    GENERATED_BODY()

public:
    USoccerTeamSaveGame();

    /** File/schema version. Kept separate from FSoccerTeamSetup::DataVersion. */
    UPROPERTY()
    int32 SaveFormatVersion = 1;

    UPROPERTY()
    FSoccerTeamSetup TeamSetup;
};
