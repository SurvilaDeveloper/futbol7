#include "SoccerCornerPreparationState.h"

#include "SoccerMatchManager.h"
#include "SoccerGoalLineRestart.h"
#include "SoccerAICharacter.h"
#include "SoccerBall.h"
#include "SoccerDebugManager.h"

bool FSoccerCornerPreparationState::Enter(ASoccerMatchManager& Manager)
{
    if (
        !Manager.GoalLineRestart.IsCornerConfigured() ||
        !IsValid(Manager.SoccerBall)
    )
    {
        Manager.CancelGoalLineRestart();
        return false;
    }

    Manager.PossessingCharacter = nullptr;
    Manager.PossessionTeam = ESoccerPossessionTeam::None;

    const bool bUseStagedRestartContext =
        Manager.IsRestartContextActive() &&
        Manager.ActiveRestartType == ESoccerRestartType::CornerKick &&
        Manager.ActiveRestartTeam ==
            Manager.GoalLineRestart.GetRestartTeam() &&
        Manager.ActiveRestartLocation.Equals(
            Manager.GoalLineRestart.GetBallLocation(),
            1.0f
        );

    if (!bUseStagedRestartContext)
    {
        Manager.BeginRestartContext(
            ESoccerRestartType::CornerKick,
            Manager.GoalLineRestart.GetRestartTeam(),
            Manager.GoalLineRestart.GetBallLocation()
        );
    }

    Manager.SoccerBall->SetActorEnableCollision(true);
    Manager.SoccerBall->SetPossessed(false);
    Manager.SoccerBall->StopBallKeepingPhysics();
    Manager.SoccerBall->SetActorLocation(
        Manager.GoalLineRestart.GetBallLocation(),
        false,
        nullptr,
        ETeleportType::TeleportPhysics
    );

    Manager.MatchPlayState = ESoccerMatchPlayState::GoalLineRestartSetup;
    const bool bHumanTakerClaimChanged =
        Manager.UpdateNonFreeKickHumanTakerClaimDuringPreparation(
            ESoccerRestartType::CornerKick
        );

    if (
        !bUseStagedRestartContext ||
        bHumanTakerClaimChanged ||
        Manager.ActiveRestartAITargetLocations.Num() == 0
    )
    {
        Manager.CaptureActiveRestartAITargetLocations(false);
    }

    ASoccerDebugManager::Message(
        &Manager,
        ESoccerDebugCategory::Restarts,
        TEXT("CORNER PREPARATION: posicionando equipos"),
        FColor(255, 165, 0)
    );
    return true;
}

void FSoccerCornerPreparationState::Tick(ASoccerMatchManager& Manager, float DeltaTime)
{
    if (
        !Manager.GoalLineRestart.IsCornerConfigured() ||
        !IsValid(Manager.SoccerBall)
    )
    {
        Manager.CancelGoalLineRestart();
        Manager.RequestMatchStateTransition(ESoccerMatchStateTransition::Playing);
        return;
    }

    Manager.SoccerBall->StopBallKeepingPhysics();
    Manager.SoccerBall->SetActorLocation(
        Manager.GoalLineRestart.GetBallLocation(),
        false,
        nullptr,
        ETeleportType::TeleportPhysics
    );

    if (Manager.UpdateNonFreeKickHumanTakerClaimDuringPreparation(
        ESoccerRestartType::CornerKick
    ))
    {
        Manager.ResetActiveRestartLivePositioning();
        if (IsValid(Manager.GoalLineRestart.GetTaker()))
        {
            Manager.GoalLineRestart.GetTaker()->ClearScriptedLocomotionVelocity();
        }
        Manager.MatchPlayState = ESoccerMatchPlayState::GoalLineRestartSetup;
        Manager.RecalculateGoalLineRestartGeometry();
        Manager.CaptureActiveRestartAITargetLocations(
            Manager.AreActiveRestartOpponentsLegal()
        );
        return;
    }

    if (Manager.MatchPlayState == ESoccerMatchPlayState::GoalLineRestartSetup)
    {
        // As in the validated throw-in flow, first finish the fixed legal setup.
        // Dynamic offers and marking begin only after everybody is settled, so
        // their movement cannot keep resetting the common readiness hold.
        if (!Manager.IsActiveRestartLivePositioningActive())
        {
            if (!Manager.UpdateActiveRestartReadiness(
                Manager.GoalLineRestart.GetSetupStartTime(),
                Manager.GoalLineRestartMinSetupTime
            ))
            {
                return;
            }

            Manager.RecalculateGoalLineRestartGeometry();
            Manager.GoalLineRestart.SetCornerFinalRunActive(false);
            Manager.BeginActiveRestartLivePositioning();
        }

        if (Manager.IsNonFreeKickHumanTakerClaimedFor(ESoccerRestartType::CornerKick))
        {
            Manager.GoalLineRestart.GetTaker()->ClearScriptedLocomotionVelocity();
            Manager.RequestMatchStateTransition(
                ESoccerMatchStateTransition::CornerExecution
            );
            return;
        }

        Manager.MatchPlayState = ESoccerMatchPlayState::GoalLineRestartPositioning;

        Manager.GoalLineRestart.GetTaker()->ClearScriptedLocomotionVelocity();

        FVector MoveDirection =
            Manager.GoalLineRestart.GetCornerOutsideStartLocation() -
            Manager.GoalLineRestart.GetTaker()->GetActorLocation();
        MoveDirection.Z = 0.0f;

        if (!MoveDirection.Normalize())
        {
            MoveDirection = -Manager.GoalLineRestart.GetKickDirection();
        }

        Manager.GoalLineRestart.GetTaker()->SetActorRotation(MoveDirection.Rotation());
        Manager.GoalLineRestart.GetTaker()->RequestAIMovementMode(
            ESoccerAIMovementMode::Jog,
            ESoccerAIMovementReason::NearbyReposition,
            true
        );
        return;
    }

    if (Manager.MatchPlayState != ESoccerMatchPlayState::GoalLineRestartPositioning)
    {
        return;
    }

    if (!IsValid(Manager.GoalLineRestart.GetTaker()))
    {
        Manager.CancelGoalLineRestart();
        Manager.RequestMatchStateTransition(ESoccerMatchStateTransition::Playing);
        return;
    }

    // The AI taker may already walk toward the exterior run-up point while the
    // off-ball contest develops. When the bounded decision window ends, freeze
    // the receiver and all destinations, then recompute the exact run direction.
    if (
        Manager.IsActiveRestartLivePositioningActive() &&
        Manager.IsActiveRestartAILivePositioningWaitComplete() &&
        !Manager.bActiveRestartLivePositioningLocked
    )
    {
        Manager.CommitBestActiveRestartLiveReceiver();
        Manager.LockActiveRestartLivePositioning();
        Manager.RecalculateGoalLineRestartGeometry();
    }

    const FVector CurrentLocation =
        Manager.GoalLineRestart.GetTaker()->GetActorLocation();
    const FVector NewLocation = FMath::VInterpConstantTo(
        CurrentLocation,
        Manager.GoalLineRestart.GetCornerOutsideStartLocation(),
        DeltaTime,
        FMath::Max(1.0f, Manager.CornerKickOutsidePositioningSpeed)
    );

    FVector ScriptedVelocity =
        DeltaTime > KINDA_SMALL_NUMBER
        ? (NewLocation - CurrentLocation) / DeltaTime
        : FVector::ZeroVector;
    ScriptedVelocity.Z = 0.0f;

    if (!ScriptedVelocity.IsNearlyZero())
    {
        Manager.GoalLineRestart.GetTaker()->SetActorRotation(
            ScriptedVelocity.GetSafeNormal().Rotation()
        );
    }

    Manager.GoalLineRestart.GetTaker()->SetActorLocation(
        NewLocation,
        true,
        nullptr,
        ETeleportType::None
    );

    Manager.GoalLineRestart.GetTaker()->SetScriptedLocomotionVelocity(
        ScriptedVelocity,
        ESoccerAIMovementMode::Jog,
        ESoccerAIMovementReason::NearbyReposition
    );

    if (FVector::Dist2D(
        NewLocation,
        Manager.GoalLineRestart.GetCornerOutsideStartLocation()
    ) > 4.0f)
    {
        return;
    }

    Manager.GoalLineRestart.GetTaker()->SetActorLocation(
        Manager.GoalLineRestart.GetCornerOutsideStartLocation(),
        false,
        nullptr,
        ETeleportType::TeleportPhysics
    );

    Manager.GoalLineRestart.GetTaker()->ClearScriptedLocomotionVelocity();
    Manager.GoalLineRestart.GetTaker()->SetActorRotation(
        Manager.GoalLineRestart.GetKickDirection().Rotation()
    );

    if (!Manager.IsActiveRestartAILivePositioningWaitComplete())
    {
        return;
    }

    // El corner no inicia la carrera si un rival vuelve a entrar
    // en el radio mientras el ejecutor se coloca fuera del campo.
    if (!Manager.AreActiveRestartOpponentsLegal())
    {
        return;
    }

    Manager.RequestMatchStateTransition(
        ESoccerMatchStateTransition::CornerExecution
    );
}
