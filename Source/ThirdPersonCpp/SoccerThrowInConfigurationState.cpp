#include "SoccerThrowInConfigurationState.h"

#include "SoccerMatchManager.h"
#include "SoccerAICharacter.h"
#include "SoccerBall.h"
#include "SoccerDebugManager.h"
#include "Engine/World.h"

bool FSoccerThrowInConfigurationState::Enter(ASoccerMatchManager& Manager)
{
    UWorld* World = Manager.GetWorld();
    if (World == nullptr || !IsValid(Manager.SoccerBall))
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

    Manager.ThrowInTeam = RestartTeam;
    Manager.ThrowInLocation = TouchlineLocation;
    Manager.ThrowInInwardDirection = InwardDirection.GetSafeNormal();
    if (Manager.ThrowInInwardDirection.IsNearlyZero())
    {
        return false;
    }
    Manager.ThrowInSetupStartTime = World->GetTimeSeconds();

    Manager.ThrowInTakerAI =
        Manager.FindBestThrowInTakerForTeam(RestartTeam, TouchlineLocation);
    Manager.ThrowInReceiverAI =
        Manager.FindBestThrowInReceiverForTeam(
            RestartTeam,
            Manager.ThrowInTakerAI,
            TouchlineLocation
        );

    if (!IsValid(Manager.ThrowInTakerAI) || !IsValid(Manager.ThrowInReceiverAI))
    {
        Manager.ThrowInTakerAI = nullptr;
        Manager.ThrowInReceiverAI = nullptr;
        return false;
    }

    Manager.RecalculateThrowInGeometry();
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
