#include "SoccerCornerConfigurationState.h"

#include "SoccerMatchManager.h"
#include "SoccerGoalLineRestart.h"
#include "SoccerAICharacter.h"
#include "SoccerBall.h"
#include "SoccerDebugManager.h"
#include "Engine/World.h"

bool FSoccerCornerConfigurationState::Enter(ASoccerMatchManager& Manager)
{
    UWorld* World = Manager.GetWorld();
    if (World == nullptr || !IsValid(Manager.SoccerBall))
    {
        return false;
    }

    // Configuration decides the corner-specific actors and geometry only.
    // Ball placement, restrictions and tactical movement belong to Preparation.
    Manager.CancelBallOutOfPlayDelay();
    Manager.CancelThrowInRestart();
    Manager.CancelGoalLineRestart();
    Manager.DestroyActiveRestartHumanRestrictionIndicator();
    Manager.ClearPendingOffsideSnapshot();
    Manager.ClearNoRetouchRestriction();
    Manager.ClearAttackRunRelease();
    Manager.ClearAssignedAI();
    Manager.ReleaseAllAIBallPossessions();
    Manager.ReleaseAllHumanBallPossessions();

    const float NormalizedGoalLineSign = GoalLineSign >= 0.0f ? 1.0f : -1.0f;
    const FVector CornerBallLocation = Manager.BuildCornerKickBallLocation(
        CrossingLocation,
        NormalizedGoalLineSign
    );

    Manager.GoalLineRestart.ConfigureCorner(
        RestartTeam,
        Manager.GetOppositeTeam(RestartTeam),
        NormalizedGoalLineSign,
        CrossingLocation,
        CornerBallLocation,
        World->GetTimeSeconds()
    );

    ASoccerAICharacter* CornerTakerAI = Manager.FindBestGoalLineRestartTakerForTeam(
        RestartTeam,
        ESoccerGoalLineRestartType::CornerKick,
        CornerBallLocation
    );
    ASoccerAICharacter* CornerReceiverAI = Manager.FindBestGoalLineRestartReceiverForTeam(
        RestartTeam,
        ESoccerGoalLineRestartType::CornerKick,
        CornerTakerAI,
        CornerBallLocation
    );

    Manager.GoalLineRestart.SetCornerParticipants(CornerTakerAI, CornerReceiverAI);

    if (!Manager.GoalLineRestart.IsCornerConfigured())
    {
        Manager.GoalLineRestart.ResetRuntime();
        return false;
    }

    Manager.RecalculateGoalLineRestartGeometry();
    bConfigured = true;

    ASoccerDebugManager::Message(
        &Manager,
        ESoccerDebugCategory::Restarts,
        TEXT("CORNER CONFIGURATION: ejecutor, receptor y geometria listos"),
        FColor(255, 165, 0)
    );
    return true;
}

void FSoccerCornerConfigurationState::Tick(ASoccerMatchManager& Manager, float DeltaTime)
{
    (void)DeltaTime;
    if (bConfigured)
    {
        Manager.RequestMatchStateTransition(ESoccerMatchStateTransition::CornerPreparation);
    }
}
