#include "SoccerCornerConfigurationState.h"

#include "SoccerMatchManager.h"
#include "SoccerGoalLineRestart.h"
#include "SoccerAICharacter.h"
#include "SoccerBall.h"
#include "SoccerDebugManager.h"
#include "Engine/World.h"

bool FSoccerCornerConfigurationState::Enter(ASoccerMatchManager& Manager)
{
    if (Manager.GetWorld() == nullptr || !IsValid(Manager.SoccerBall))
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
    Manager.ResetNonFreeKickHumanTakerRuntime();

    if (!Manager.ConfigureGoalLineRestart(
        ESoccerGoalLineRestartType::CornerKick,
        RestartTeam,
        CrossingLocation,
        GoalLineSign
    ))
    {
        return false;
    }

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
