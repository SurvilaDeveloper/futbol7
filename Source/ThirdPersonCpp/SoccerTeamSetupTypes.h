#pragma once

#include "CoreMinimal.h"
#include "SoccerFormationTypes.h"
#include "SoccerTacticTypes.h"
#include "SoccerPlayerInstructionTypes.h"
#include "SoccerTeamSetupTypes.generated.h"

/**
 * Persistent coach-side configuration for the user's team.
 *
 * Player references are stored only as stable PlayerIds (for example PLAYER_0001),
 * never as actor pointers or direct runtime character references. This allows the
 * setup to survive level changes, editor restarts and future roster/UI refactors.
 */
USTRUCT(BlueprintType)
struct FSoccerTeamSetup
{
    GENERATED_BODY()

    /** Version of the data model inside this structure. Reserved for migrations. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soccer|Team Setup")
    int32 DataVersion = 1;

    /** Structural seven-a-side formation currently selected by the coach. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soccer|Team Setup")
    ESoccerFormationSystem FormationSystem = ESoccerFormationSystem::OneThreeTwoOne;

    /** Collective tactical plan currently selected by the coach. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soccer|Team Setup")
    FSoccerTeamTacticalPlan TacticalPlan;

    /** Per-formation-slot tactical instructions. SlotId is the stable key. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soccer|Team Setup")
    TArray<FSoccerSlotTacticalInstruction> SlotInstructions;

    /**
     * Complete user-team roster represented by stable USoccerPlayerProfile::PlayerId values.
     * Order is intentionally persistent so a future squad UI can keep a stable ordering.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soccer|Team Setup")
    TArray<FName> SquadPlayerIds;

    /**
     * Formation SlotId -> PlayerId.
     * Only the seven slots defined by FormationSystem are legal keys.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soccer|Team Setup")
    TMap<FName, FName> StartingLineupBySlot;

    /** Substitute players, stored by stable PlayerId and in user-selected order. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soccer|Team Setup")
    TArray<FName> BenchPlayerIds;
};
