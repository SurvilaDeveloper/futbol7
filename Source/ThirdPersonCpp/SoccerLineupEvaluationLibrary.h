#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SoccerFormationTypes.h"
#include "SoccerLineupTypes.h"
#include "SoccerLineupEvaluationLibrary.generated.h"

class USoccerPlayerProfile;

/**
 * Pure lineup-evaluation helpers shared by the future manager screen and gameplay tools.
 * They never modify the player, formation or save data.
 */
UCLASS()
class THIRDPERSONCPP_API USoccerLineupEvaluationLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /**
     * Evaluates one player for one legal slot of a formation.
     * A low score is advisory only: the coach is always allowed to improvise.
     */
    UFUNCTION(BlueprintPure, Category = "Soccer|Lineup|Evaluation")
    static FSoccerPlayerSlotSuitability EvaluatePlayerForFormationSlot(
        const USoccerPlayerProfile* PlayerProfile,
        ESoccerFormationSystem FormationSystem,
        FName FormationSlotId
    );

    UFUNCTION(BlueprintPure, Category = "Soccer|Lineup|Evaluation")
    static ESoccerLineupSuitabilityBand GetSuitabilityBandForScore(int32 OverallScore);

    UFUNCTION(BlueprintPure, Category = "Soccer|Lineup|Evaluation")
    static FText GetSuitabilityBandDisplayName(ESoccerLineupSuitabilityBand SuitabilityBand);

private:
    static int32 CalculateAbilityScoreForSlot(
        const USoccerPlayerProfile* PlayerProfile,
        const FSoccerFormationSlot& FormationSlot
    );
};
