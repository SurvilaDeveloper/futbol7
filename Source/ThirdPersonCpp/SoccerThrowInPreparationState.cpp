#include "SoccerThrowInPreparationState.h"

#include "SoccerMatchManager.h"
#include "SoccerBall.h"
#include "SoccerAICharacter.h"
#include "SoccerDebugManager.h"

bool FSoccerThrowInPreparationState::Enter(ASoccerMatchManager& Manager)
{
    if (
        !IsValid(Manager.SoccerBall) ||
        !IsValid(Manager.ThrowInTakerAI) ||
        !IsValid(Manager.ThrowInReceiverAI)
    )
    {
        Manager.CancelThrowInRestart();
        return false;
    }

    Manager.PossessingCharacter = nullptr;
    Manager.PossessionTeam = ESoccerPossessionTeam::None;

    const bool bUseStagedRestartContext =
        Manager.IsRestartContextActive() &&
        Manager.ActiveRestartType == ESoccerRestartType::ThrowIn &&
        Manager.ActiveRestartTeam == Manager.ThrowInTeam &&
        Manager.ActiveRestartLocation.Equals(Manager.ThrowInLocation, 1.0f);

    if (!bUseStagedRestartContext)
    {
        Manager.BeginRestartContext(
            ESoccerRestartType::ThrowIn,
            Manager.ThrowInTeam,
            Manager.ThrowInLocation
        );
    }

    Manager.SoccerBall->SetActorEnableCollision(true);
    Manager.SoccerBall->StopBallKeepingPhysics();
    Manager.SoccerBall->SetPossessed(true);
    Manager.SoccerBall->SetActorLocation(
        Manager.ThrowInLocation,
        false,
        nullptr,
        ETeleportType::TeleportPhysics
    );

    Manager.MatchPlayState = ESoccerMatchPlayState::ThrowInSetup;

    if (
        !bUseStagedRestartContext ||
        Manager.ActiveRestartAITargetLocations.Num() == 0
    )
    {
        Manager.CaptureActiveRestartAITargetLocations(false);
    }

    ASoccerDebugManager::Message(
        &Manager,
        ESoccerDebugCategory::Restarts,
        TEXT("THROW IN PREPARATION: posicionando equipos"),
        FColor::Yellow
    );
    return true;
}

void FSoccerThrowInPreparationState::Tick(ASoccerMatchManager& Manager, float DeltaTime)
{
    if (
        !IsValid(Manager.SoccerBall) ||
        !IsValid(Manager.ThrowInTakerAI) ||
        !IsValid(Manager.ThrowInReceiverAI)
    )
    {
        Manager.CancelThrowInRestart();
        Manager.RequestMatchStateTransition(ESoccerMatchStateTransition::Playing);
        return;
    }

    if (Manager.MatchPlayState == ESoccerMatchPlayState::ThrowInSetup)
    {
        if (!Manager.UpdateActiveRestartReadiness(
            Manager.ThrowInSetupStartTime,
            Manager.ThrowInMinSetupTime
        ))
        {
            return;
        }

        Manager.RecalculateThrowInGeometry();

        // El ejecutor ya llego al punto de recogida. Toma la pelota antes
        // de alejarse hacia el inicio exterior de throw_in_in_place.
        FVector FaceBallDirection =
            Manager.ThrowInLocation - Manager.ThrowInTakerAI->GetActorLocation();
        FaceBallDirection.Z = 0.0f;

        if (FaceBallDirection.Normalize())
        {
            Manager.ThrowInTakerAI->SetActorRotation(FaceBallDirection.Rotation());
        }

        if (!Manager.ThrowInTakerAI->HoldThrowInBall(Manager.SoccerBall))
        {
            Manager.CancelThrowInRestart();
            Manager.RequestMatchStateTransition(ESoccerMatchStateTransition::Playing);
            return;
        }

        Manager.PossessingCharacter = Manager.ThrowInTakerAI;
        Manager.PossessionTeam =
            Manager.ConvertTeamToPossessionTeam(Manager.ThrowInTeam);

        Manager.MatchPlayState = ESoccerMatchPlayState::ThrowInPositioning;

        Manager.ThrowInTakerAI->ClearScriptedLocomotionVelocity();

        FVector MoveDirection =
            Manager.ThrowInOutsideStartLocation -
            Manager.ThrowInTakerAI->GetActorLocation();
        MoveDirection.Z = 0.0f;

        if (!MoveDirection.Normalize())
        {
            MoveDirection = -Manager.ThrowInInwardDirection;
        }

        Manager.ThrowInTakerAI->SetActorRotation(MoveDirection.Rotation());
        Manager.ThrowInTakerAI->RequestAIMovementMode(
            ESoccerAIMovementMode::Jog,
            ESoccerAIMovementReason::NearbyReposition,
            true
        );
        return;
    }

    if (Manager.MatchPlayState != ESoccerMatchPlayState::ThrowInPositioning)
    {
        return;
    }

    const FVector CurrentLocation = Manager.ThrowInTakerAI->GetActorLocation();
    const FVector NewLocation = FMath::VInterpConstantTo(
        CurrentLocation,
        Manager.ThrowInOutsideStartLocation,
        DeltaTime,
        FMath::Max(1.0f, Manager.ThrowInOutsidePositioningSpeed)
    );

    FVector ScriptedVelocity =
        DeltaTime > KINDA_SMALL_NUMBER
        ? (NewLocation - CurrentLocation) / DeltaTime
        : FVector::ZeroVector;
    ScriptedVelocity.Z = 0.0f;

    if (!ScriptedVelocity.IsNearlyZero())
    {
        Manager.ThrowInTakerAI->SetActorRotation(
            ScriptedVelocity.GetSafeNormal().Rotation()
        );
    }

    Manager.ThrowInTakerAI->SetActorLocation(
        NewLocation,
        true,
        nullptr,
        ETeleportType::None
    );

    Manager.ThrowInTakerAI->SetScriptedLocomotionVelocity(
        ScriptedVelocity,
        ESoccerAIMovementMode::Jog,
        ESoccerAIMovementReason::NearbyReposition
    );

    if (FVector::Dist2D(NewLocation, Manager.ThrowInOutsideStartLocation) > 3.0f)
    {
        return;
    }

    Manager.ThrowInTakerAI->SetActorLocation(
        Manager.ThrowInOutsideStartLocation,
        false,
        nullptr,
        ETeleportType::TeleportPhysics
    );

    Manager.ThrowInTakerAI->ClearScriptedLocomotionVelocity();
    Manager.ThrowInTakerAI->SetActorRotation(Manager.ThrowInDirection.Rotation());

    // Los rivales deben conservar la distancia hasta que comienza
    // efectivamente la ejecucion del lateral.
    if (!Manager.AreActiveRestartOpponentsLegal())
    {
        return;
    }

    Manager.RequestMatchStateTransition(
        ESoccerMatchStateTransition::ThrowInExecution
    );
}
