#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "SoccerTacticalPresetTypes.h"
#include "SoccerTacticalPresetSaveGame.generated.h"

/*
 * Local persistence container for the user's tactical preset library.
 * The storage implementation is intentionally isolated from match/UI code so
 * it can later be replaced or complemented by account/backend persistence.
 */
UCLASS()
class THIRDPERSONCPP_API USoccerTacticalPresetSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	USoccerTacticalPresetSaveGame();

	UPROPERTY()
	int32 SaveFormatVersion = 1;

	UPROPERTY()
	TArray<FSoccerTacticalPreset> Presets;

	// Stage 16C: stable preset IDs assigned to the four in-match quick slots.
	// The preset data itself is not duplicated here; rename/overwrite therefore
	// keeps the shortcut valid while delete can clear the reference safely.
	UPROPERTY()
	TArray<FGuid> QuickPresetSlotIds;
};
