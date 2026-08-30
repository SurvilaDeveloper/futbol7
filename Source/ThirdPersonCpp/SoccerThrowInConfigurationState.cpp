#include "SoccerThrowInConfigurationState.h"

#include "SoccerMatchManager.h"
#include "SoccerAICharacter.h"
#include "SoccerBall.h"
#include "SoccerDebugManager.h"
#include "Engine/World.h"

bool FSoccerThrowInConfigurationState::Enter(ASoccerMatchManager& Manager)
{
    if (Manager.GetWorld() == nullptr || !IsValid(Manager.SoccerBall))
    {
        return false;
    }

    // Configuration only decides the throw-in-specific participants and geometry.
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

    if (!Manager.ConfigureThrowInRestart(
        RestartTeam,
        TouchlineLocation,
        InwardDirection
    ))
    {
        return false;
    }

    bConfigured = true;

    ASoccerDebugManager::Message(
        &Manager,
        ESoccerDebugCategory::Restarts,
        TEXT("THROW IN CONFIGURATION: ejecutor, receptor y geometria listos"),
        FColor::Yellow
    );
    return true;
}

void FSoccerThrowInConfigurationState::Tick(ASoccerMatchManager& Manager, float DeltaTime)
{
    (void)DeltaTime;
    if (bConfigured)
    {
        Manager.RequestMatchStateTransition(ESoccerMatchStateTransition::ThrowInPreparation);
    }
}
