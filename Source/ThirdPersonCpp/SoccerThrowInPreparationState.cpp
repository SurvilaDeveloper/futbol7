#include "SoccerThrowInPreparationState.h"

#include "SoccerMatchManager.h"
#include "SoccerBall.h"
#include "SoccerAICharacter.h"
#include "ThirdPersonCppCharacter.h"
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
    const bool bHumanClaimChanged =
        Manager.UpdateHumanThrowInTakerClaimDuringPreparation();

    if (
        !bUseStagedRestartContext ||
        bHumanClaimChanged ||
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
        // Until somebody physically picks the ball up, keep it pinned to the
        // legal touchline location. Claiming the throw-in never moves the ball.
        if (Manager.SoccerBall->GetAttachParentActor() == nullptr)
        {
            Manager.SoccerBall->StopBallKeepingPhysics();
            Manager.SoccerBall->SetActorLocation(
                Manager.ThrowInLocation,
                false,
                nullptr,
                ETeleportType::TeleportPhysics
            );
        }

        if (Manager.UpdateHumanThrowInTakerClaimDuringPreparation())
        {
            // Changing the taker also changes whether the configured AI is an
            // off-ball option. Rebuild from the fixed legal setup before a new
            // live-positioning window is allowed to start.
            Manager.ResetActiveRestartLivePositioning();
            Manager.RecalculateThrowInGeometry();
            Manager.CaptureActiveRestartAITargetLocations(
                Manager.AreActiveRestartOpponentsLegal()
            );
            return;
        }

        // First complete the existing fixed/legal placement. Dynamic off-ball
        // targets begin only afterward, so their movement cannot keep resetting
        // UpdateActiveRestartReadiness() and block the throw forever.
        if (!Manager.IsActiveRestartLivePositioningActive())
        {
            if (!Manager.UpdateActiveRestartReadiness(
                Manager.ThrowInSetupStartTime,
                Manager.ThrowInMinSetupTime
            ))
            {
                return;
            }

            Manager.BeginActiveRestartLivePositioning();
        }

        // A human thrower receives the ball as soon as the legal setup is ready
        // and the off-ball contest continues until their manual click. An AI
        // thrower observes it for a short, deterministic bounded interval.
        if (
            !Manager.bThrowInHumanTakerClaimed &&
            !Manager.IsActiveRestartAILivePositioningWaitComplete()
        )
        {
            return;
        }

        if (!Manager.bThrowInHumanTakerClaimed)
        {
            // Choose who the AI intends to serve before it starts carrying the
            // ball, but keep every live destination open during that approach.
            Manager.CommitBestActiveRestartLiveReceiver();
        }

        Manager.RecalculateThrowInGeometry();

        if (Manager.bThrowInHumanTakerClaimed)
        {
            if (!IsValid(Manager.ThrowInHumanTaker))
            {
                Manager.bThrowInHumanTakerClaimed = false;
                Manager.ResetActiveRestartLivePositioning();
                Manager.CaptureActiveRestartAITargetLocations(
                    Manager.AreActiveRestartOpponentsLegal()
                );
                return;
            }

            // The claim radius decides who wants the restart. Actual pickup is
            // stricter: the human must walk up to the same staging point used by
            // the AI, so the ball never teleports into the player's hands.
            if (FVector::Dist2D(
                Manager.ThrowInHumanTaker->GetActorLocation(),
                Manager.ThrowInStagingLocation
            ) > FMath::Max(10.0f, Manager.ThrowInHumanPickupReadyDistance))
            {
                return;
            }

            FVector FaceBallDirection =
                Manager.ThrowInLocation -
                Manager.ThrowInHumanTaker->GetActorLocation();
            FaceBallDirection.Z = 0.0f;

            if (FaceBallDirection.Normalize())
            {
                Manager.ThrowInHumanTaker->SetActorRotation(
                    FaceBallDirection.Rotation()
                );
            }

            if (!Manager.ThrowInHumanTaker->HoldThrowInBall(Manager.SoccerBall))
            {
                Manager.CancelThrowInRestart();
                Manager.RequestMatchStateTransition(
                    ESoccerMatchStateTransition::Playing
                );
                return;
            }

            Manager.PossessingCharacter = Manager.ThrowInHumanTaker;
            Manager.PossessionTeam =
                Manager.ConvertTeamToPossessionTeam(Manager.ThrowInTeam);

            // Once the ball is physically in the human's hands, the restart is
            // committed. From here walking away no longer hands it back to AI.
            Manager.bThrowInHumanTakerCommitted = true;
            Manager.bThrowInHumanExecutionAuthorized = false;
            Manager.bThrowInHumanTargetSelected = false;
            Manager.bThrowInHumanRepositioningForTarget = false;
            Manager.bThrowInHumanMontageStarted = false;
            Manager.ThrowInHumanTargetLocation = FVector::ZeroVector;

            Manager.ThrowInTakerAI->ClearScriptedLocomotionVelocity();
            Manager.ThrowInHumanTaker->ClearThrowInScriptedMovementVelocity();

            Manager.RequestMatchStateTransition(
                ESoccerMatchStateTransition::ThrowInExecution
            );
            return;
        }

        // The AI picks the ball up and walks toward the provisional outside
        // start while receivers and markers are still allowed to reposition.
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

    if (!Manager.AreActiveRestartOpponentsLegal())
    {
        return;
    }

    if (!Manager.bThrowInAICommittedTargetSelected)
    {
        // This is the last safe instant to choose the real receiver (the human
        // may be the best option) and one physical target for animation/release.
        Manager.CommitBestActiveRestartLiveReceiver();
        Manager.SelectActiveRestartExecutionReceiver(
            Manager.ThrowInTeam,
            Manager.ThrowInTakerAI,
            Manager.ThrowInReceiverAI,
            true
        );

        Manager.ThrowInAICommittedTargetLocation =
            Manager.GetActiveRestartExecutionTargetLocation(
                Manager.ThrowInReceiverMoveLocation
            );
        Manager.ThrowInAICommittedTargetLocation.Z =
            Manager.SoccerBall->GetActorLocation().Z;
        Manager.bThrowInAICommittedTargetSelected = true;
        Manager.RecalculateThrowInGeometry();

        // A new final direction also changes the curve-derived exterior start.
        // Reach that correction through the existing physical movement path.
        FVector CorrectedMoveDirection =
            Manager.ThrowInOutsideStartLocation -
            Manager.ThrowInTakerAI->GetActorLocation();
        CorrectedMoveDirection.Z = 0.0f;

        if (CorrectedMoveDirection.Size() > 3.0f)
        {
            Manager.ThrowInTakerAI->SetActorRotation(
                CorrectedMoveDirection.GetSafeNormal().Rotation()
            );
            Manager.ThrowInTakerAI->RequestAIMovementMode(
                ESoccerAIMovementMode::Jog,
                ESoccerAIMovementReason::NearbyReposition,
                true
            );
            return;
        }
    }

    Manager.ThrowInTakerAI->SetActorLocation(
        Manager.ThrowInOutsideStartLocation,
        false,
        nullptr,
        ETeleportType::TeleportPhysics
    );
    Manager.ThrowInTakerAI->SetActorRotation(Manager.ThrowInDirection.Rotation());

    // Keep all offers and marks alive even during a possible final exterior
    // correction. Lock only on the frame that starts the throw animation.
    Manager.CommitActiveRestartLivePositioningForAIAction();

    Manager.RequestMatchStateTransition(
        ESoccerMatchStateTransition::ThrowInExecution
    );
}
