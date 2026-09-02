#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SoccerCoachMatchPlanTypes.h"
#include "SoccerCoachPlanningLibrary.generated.h"

class USoccerCoachProfile;
class USoccerSquadCatalog;
class USoccerPlayerProfile;

/** Deterministic utility planner used before an AI-controlled match. */
UCLASS()
class THIRDPERSONCPP_API USoccerCoachPlanningLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Soccer|AI Coach|Planning")
    static bool BuildPreMatchPlan(
        const USoccerCoachProfile* CoachProfile,
        const USoccerSquadCatalog* SquadCatalog,
        FSoccerCoachMatchPlan& OutPlan
    );

private:
    static int32 CalculateTacticalFitScore(
        const USoccerCoachProfile* CoachProfile,
        const USoccerPlayerProfile* PlayerProfile
    );

    static int32 CalculateCandidateScore(
        const USoccerCoachProfile* CoachProfile,
        const USoccerPlayerProfile* PlayerProfile,
        ESoccerFormationSystem FormationSystem,
        FName FormationSlotId,
        FSoccerCoachLineupDecision& OutDecision
    );
};
