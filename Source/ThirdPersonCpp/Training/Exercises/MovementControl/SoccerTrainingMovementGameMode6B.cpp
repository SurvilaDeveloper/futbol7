#include "Training/Exercises/MovementControl/SoccerTrainingMovementGameMode6B.h"
#include "SoccerGoalkeeperPositioningShared6AA.h"
#include "Training/Exercises/MovementControl/SoccerTrainingMovementHUD6B.h"
#include "Training/Shared/SoccerTrainingPlayerController6B.h"

#include "SoccerBall.h"
#include "SoccerAICharacter.h"
#include "SoccerAIController.h"
#include "SoccerTeamTypes.h"
#include "SoccerField.h"
#include "SoccerFieldDimensions.h"
#include "ThirdPersonCppCharacter.h"
#include "SoccerPlayerProfile.h"
#include "EngineUtils.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/Controller.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "DrawDebugHelpers.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"

// 6AB.3: the goalkeeper exercise must keep the practice ball collidable while
// waiting for the next kick. Other ball/character restart paths may toggle
// collision after the teleport, so apply this AFTER their reset functions.
static bool RestorePracticeBallGroundCollision6AB3(ASoccerBall* Ball)
{
    if (!IsValid(Ball)) return false;
    UPrimitiveComponent* Body = Cast<UPrimitiveComponent>(Ball->GetRootComponent());
    if (!Body) return false;
    bool bChanged = false;
    if (!Ball->GetActorEnableCollision())
    {
        Ball->SetActorEnableCollision(true);
        bChanged = true;
    }
    if (Body->GetCollisionEnabled() != ECollisionEnabled::QueryAndPhysics)
    {
        Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        bChanged = true;
    }
    if (Body->GetCollisionResponseToChannel(ECC_WorldStatic) != ECR_Block)
    {
        Body->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
        bChanged = true;
    }
    return bChanged;
}

void ASoccerTrainingMovementGameMode6B::OnPracticeBallHit6AF(
    UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
    FVector NormalImpulse, const FHitResult& Hit)
{
    (void)HitComp;
    (void)OtherActor;
    (void)NormalImpulse;
    (void)Hit;
    if (!bTrackFrameImpact6AF || !bGoalkeeperShotLesson6T || !bShotInFlight6M ||
        bBetweenShotAttempts6N || !IsValid(OtherComp)) return;
    // The match's actual goal frame components are named in SoccerField.cpp.
    // A proximity-only check is NOT proof that a shot touched the frame.
    const FString ComponentName = OtherComp->GetName();
    if (ComponentName.StartsWith(TEXT("Goal_Right_")))
    {
        if (ComponentName.Contains(TEXT("Crossbar")))
            bCrossbarTouchedThisShot6AF = true;
        else if (ComponentName.Contains(TEXT("Post")))
            bPostTouchedThisShot6AF = true;
        else
            return;
        UE_LOG(LogTemp, Display,
            TEXT("[Training6AF] Frame contact attempt=%d component=%s post=%s crossbar=%s"),
            ShotAttemptIndex6N + 1, *ComponentName,
            bPostTouchedThisShot6AF ? TEXT("YES") : TEXT("NO"),
            bCrossbarTouchedThisShot6AF ? TEXT("YES") : TEXT("NO"));
    }
}

ASoccerTrainingMovementGameMode6B::ASoccerTrainingMovementGameMode6B()
{
    PrimaryActorTick.bCanEverTick = true;
    PlayerControllerClass = ASoccerTrainingPlayerController6B::StaticClass();
    HUDClass = ASoccerTrainingMovementHUD6B::StaticClass();
    DefaultPawnClass = AThirdPersonCppCharacter::StaticClass();
    static ConstructorHelpers::FClassFinder<APawn> CharacterBP6B(
        TEXT("/Game/ThirdPersonCPP/Blueprints/ThirdPersonCharacter"));
    if (CharacterBP6B.Succeeded()) DefaultPawnClass = CharacterBP6B.Class;
}

void ASoccerTrainingMovementGameMode6B::StartPlay()
{
    const FString Lesson = UGameplayStatics::ParseOption(OptionsString, TEXT("TrainingLesson"));
    bAutoPassLesson6J = Lesson.Equals(TEXT("AutoPass"), ESearchCase::IgnoreCase);
    bGoalkeeperShotLesson6T = Lesson.Equals(TEXT("GoalkeeperShot"), ESearchCase::IgnoreCase);
    bChargedShotLesson6M = bGoalkeeperShotLesson6T || Lesson.Equals(TEXT("ChargedShot"), ESearchCase::IgnoreCase);
    UE_LOG(LogTemp, Display, TEXT("[Training6J] Lesson: %s"),
        bGoalkeeperShotLesson6T ? TEXT("GoalkeeperShot") : bChargedShotLesson6M ? TEXT("ChargedShot") : bAutoPassLesson6J ? TEXT("AutoPass") : TEXT("Dribble"));
    // 6L: ASoccerField can live in a shared, Always Loaded streaming level.
    // Never create the temporary cube when the real field is present.
    if (FindSharedField6L())
    {
        UE_LOG(LogTemp, Display, TEXT("[Training6L] Shared ASoccerField detected; using real pitch geometry."));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[Training6L] No ASoccerField found. Add the shared field sublevel to this training map."));
    }
    if (bGenerateTestGround6B && !FindSharedField6L()) EnsureGround6B();
    EnsurePlayerStart6B();
    if (bSpawnPracticeBall6B) EnsureBall6B();
    Super::StartPlay();
    FindExerciseActors6B();
    EnsureTrainingProfile6D();
    EnsureTrainingBallParity6G();

    // 6U: the goalkeeper is placed in the persistent training level, shared by
    // all lessons. Keep it entirely inactive outside the goalkeeper lesson.
    // This affects only the training world; it does not change the Blueprint
    // or the goalkeeper used in regular matches.
    for (TActorIterator<ASoccerAICharacter> It(GetWorld()); It; ++It)
    {
        ASoccerAICharacter* Candidate = *It;
        if (!IsValid(Candidate) || Candidate->GetPlayerRole() != ESoccerPlayerRole::Goalkeeper)
            continue;

        if (!bGoalkeeperShotLesson6T)
        {
            Candidate->SetActorHiddenInGame(true);
            Candidate->SetActorEnableCollision(false);
            Candidate->SetActorTickEnabled(false);
            if (AController* Controller = Candidate->GetController())
                Controller->SetActorTickEnabled(false);
            UE_LOG(LogTemp, Display, TEXT("[Training6U] Goalkeeper inactive for this lesson: %s"),
                *Candidate->GetName());
        }
        else
        {
            Candidate->SetActorHiddenInGame(false);
            UE_LOG(LogTemp, Display, TEXT("[Training6U] Goalkeeper available for shot lesson: %s"),
                *Candidate->GetName());
        }
    }
    if (bGoalkeeperShotLesson6T) FindTrainingGoalkeeper6T();
    UE_LOG(LogTemp, Display, TEXT("[Training6B] Movement lesson started; map owns lighting."));
}

void ASoccerTrainingMovementGameMode6B::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!IsValid(PracticeBall6B) || !IsValid(Trainee6B)) FindExerciseActors6B();
    EnsureTrainingProfile6D();
    EnsureTrainingBallParity6G();
    if (IsValid(Trainee6B))
    {
        const int32 PossessionState = Trainee6B->IsPossessingBall() ? 1 : 0;
        if (PossessionState != LastPossessionState6D)
        {
            LastPossessionState6D = PossessionState;
            UE_LOG(LogTemp, Display, TEXT("[Training6D] Physical dribble state: %s; player profile: %s"),
                PossessionState ? TEXT("PossessingBall") : TEXT("NotPossessingBall"),
                Trainee6B->HasPlayerProfile() ? *Trainee6B->GetPlayerProfileId().ToString() : TEXT("None"));
        }
    }
    if (!IsExerciseRunning6H() || !IsValid(PracticeBall6B) || !IsValid(Trainee6B)) return;
    ElapsedTime6B += DeltaSeconds;
    if (bGoalkeeperShotLesson6T) UpdateTrainingGoalkeeper6T(DeltaSeconds);
    if (bChargedShotLesson6M)
    {
        VerifyShotBallReset6AB2();
        TickChargedShot6M(DeltaSeconds);
        return;
    }

    const FVector BallPos = PracticeBall6B->GetActorLocation();
    const FVector PlayerPos = Trainee6B->GetActorLocation();
    const float Separation = FVector::Dist2D(BallPos, PlayerPos);
    const bool bClose = Separation <= MaximumControlDistance6B;
    // Report an interruption once after a sustained separation, not every frame.
    if (!bClose)
    {
        OutOfControlSeconds6B += DeltaSeconds;
        if (bHadControl6B && OutOfControlSeconds6B >= 1.f)
        {
            ++LostControlCount6B;
            bHadControl6B = false;
        }
    }
    else
    {
        bHadControl6B = true;
        OutOfControlSeconds6B = 0.f;
    }

    DrawCheckpoint6B();
    const FVector Checkpoint = GetCurrentCheckpoint6B();
    // Autopass: the ball must reach the target AFTER a real autopass click.
    // The player may still be following the ball, so do not require bClose.
    const bool bMayAdvance = bAutoPassLesson6J ? bAutoPassArmed6J : bClose;
    if (bMayAdvance && FVector::Dist2D(BallPos, Checkpoint) <= CheckpointRadius6B &&
        FMath::Abs(BallPos.Z - Checkpoint.Z) <= 180.f)
    {
        ++CheckpointIndex6B;
        bAutoPassArmed6J = false;
        if (CheckpointIndex6B >= GetCheckpointCount6B())
        {
            bComplete6B = true;
            if (ASoccerTrainingPlayerController6B* PC = Cast<ASoccerTrainingPlayerController6B>(GetWorld()->GetFirstPlayerController()))
                PC->ShowExerciseResults6H();
            UE_LOG(LogTemp, Display, TEXT("[Training6B] Completed in %.1fs, control interruptions: %d"),
                ElapsedTime6B, LostControlCount6B);
        }
    }
}

void ASoccerTrainingMovementGameMode6B::FindTrainingGoalkeeper6T()
{
    if (IsValid(TrainingGoalkeeper6T)) return;
    for (TActorIterator<ASoccerAICharacter> It(GetWorld()); It; ++It)
    {
        ASoccerAICharacter* Candidate = *It;
        if (IsValid(Candidate) && Candidate->GetPlayerRole() == ESoccerPlayerRole::Goalkeeper)
        {
            TrainingGoalkeeper6T = Candidate;
            EnsureTrainingGoalkeeperProfile6Z3();
            // 6X.1: Diagnose the ACTUAL possessing controller, not merely the
            // AIControllerClass configured on the goalkeeper Blueprint.
            AController* Controller = Candidate->GetController();
            const ASoccerAIController* SoccerController = Cast<ASoccerAIController>(Controller);
            UE_LOG(LogTemp, Warning,
                TEXT("[Training6X1] Keeper=%s | Controller=%s | Class=%s | IsSoccerAIController=%s | ControllerTickBefore=%s | KeeperTick=%s"),
                *GetNameSafe(Candidate),
                *GetNameSafe(Controller),
                Controller ? *Controller->GetClass()->GetPathName() : TEXT("None"),
                SoccerController ? TEXT("YES") : TEXT("NO"),
                Controller && Controller->IsActorTickEnabled() ? TEXT("ON") : TEXT("OFF"),
                Candidate->IsActorTickEnabled() ? TEXT("ON") : TEXT("OFF"));

            // Without SoccerMatchManager the normal AI controller would return
            // to its match home. The exercise owns only this keeper's positioning.
            if (Controller)
            {
                Controller->SetActorTickEnabled(false);
                UE_LOG(LogTemp, Warning,
                    TEXT("[Training6X1] ControllerTickAfter=%s | RegularAI=%s | MatchSaveSelector=%s"),
                    Controller->IsActorTickEnabled() ? TEXT("ON") : TEXT("OFF"),
                    Controller->IsActorTickEnabled() ? TEXT("MAY RUN") : TEXT("DISABLED BY TRAINING"),
                    SoccerController ? TEXT("AVAILABLE FOR EXPLICIT CALL") : TEXT("UNAVAILABLE"));
            }
            else
            {
                UE_LOG(LogTemp, Warning,
                    TEXT("[Training6X1] No controller possesses the goalkeeper."));
            }
            UE_LOG(LogTemp, Display, TEXT("[Training6T] Goalkeeper found: %s"), *Candidate->GetName());
            return;
        }
    }
    UE_LOG(LogTemp, Warning, TEXT("[Training6T] No goalkeeper in training map. Copy a goalkeeper actor from the match to the TRAINING persistent level."));
}

// 6Z.3: Assign a profile only when explicitly configured, then report the
// effective goalkeeper abilities. Never overwrite the human training profile.
void ASoccerTrainingMovementGameMode6B::EnsureTrainingGoalkeeperProfile6Z3()
{
    if (!IsValid(TrainingGoalkeeper6T)) return;

    if (!TrainingGoalkeeperProfile6Z3.IsNull())
    {
        USoccerPlayerProfile* RequestedProfile = TrainingGoalkeeperProfile6Z3.LoadSynchronous();
        if (IsValid(RequestedProfile))
        {
            TrainingGoalkeeper6T->SetPlayerProfileForMatch(RequestedProfile);
            UE_LOG(LogTemp, Display, TEXT("[Training6Z3] Assigned goalkeeper profile asset=%s"),
                *RequestedProfile->GetPathName());
        }
        else
        {
            UE_LOG(LogTemp, Warning,
                TEXT("[Training6Z3] Could not load configured goalkeeper profile asset=%s"),
                *TrainingGoalkeeperProfile6Z3.ToSoftObjectPath().ToString());
        }
    }

    const USoccerPlayerProfile* EffectiveProfile = TrainingGoalkeeper6T->GetPlayerProfile();
    if (!IsValid(EffectiveProfile))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[Training6Z3] Keeper=%s HasProfile=NO ProfileId=NINGUNO. Configure TrainingGoalkeeperProfile6Z3 in the training GameMode."),
            *GetNameSafe(TrainingGoalkeeper6T));
        return;
    }

    const FSoccerPlayerAttributes& A = EffectiveProfile->Attributes;
    UE_LOG(LogTemp, Display,
        TEXT("[Training6Z3] Keeper=%s HasProfile=YES ProfileId=%s ProfileAsset=%s | Reflexes=%.0f Positioning=%.0f Handling=%.0f Diving=%.0f Distribution=%.0f | Anticipation=%.0f DefensiveReaction=%.0f Composure=%.0f"),
        *GetNameSafe(TrainingGoalkeeper6T),
        *TrainingGoalkeeper6T->GetPlayerProfileId().ToString(),
        *EffectiveProfile->GetPathName(),
        static_cast<double>(A.Goalkeeper.Reflexes),
        static_cast<double>(A.Goalkeeper.Positioning),
        static_cast<double>(A.Goalkeeper.Handling),
        static_cast<double>(A.Goalkeeper.Diving),
        static_cast<double>(A.Goalkeeper.Distribution),
        static_cast<double>(A.Tactical.Anticipation),
        static_cast<double>(A.Tactical.DefensiveReaction),
        static_cast<double>(A.Tactical.Composure));
}

void ASoccerTrainingMovementGameMode6B::UpdateTrainingGoalkeeper6T(float DeltaSeconds)
{
    if (!IsValid(TrainingGoalkeeper6T))
    {
        if (!bGoalkeeperPrepared6T)
        {
            bGoalkeeperPrepared6T = true;
            FindTrainingGoalkeeper6T();
        }
        return;
    }

    ASoccerField* Field = FindSharedField6L();
    if (!Field || !IsValid(PracticeBall6B) || DeltaSeconds <= 0.f) return;

    // Hold the goalkeeper's pose briefly after a save so the animation can play.
    // The next attempt restores normal positioning in PrepareShotAttempt6N().
    if (bBetweenShotAttempts6N && bKeeperSaveTriggered6T)
    {
        TrainingGoalkeeper6T->SetScriptedLocomotionVelocity(
            FVector::ZeroVector, ESoccerAIMovementMode::Run, ESoccerAIMovementReason::NormalPlay);
        return;
    }

    // 6Y: stabilize the goalkeeper's pose immediately before the selected
    // animation. Earlier in the shot we continue using the 6V positioning.
    if (!bGoalkeeperShotLesson6T && bShotInFlight6M && bKeeperHasPendingPlan6Y &&
        KeeperPlannedDelay6Y <= KeeperPreMontageFreezeSeconds6Y)
    {
        TrainingGoalkeeper6T->SetScriptedLocomotionVelocity(
            FVector::ZeroVector, ESoccerAIMovementMode::Run,
            ESoccerAIMovementReason::NormalPlay);
        return;
    }

    // 6X: do not teleport/reorient the goalkeeper while a selected montage
    // is playing; the montage owns the pose during that interval.
    if (bShotInFlight6M && TrainingGoalkeeper6T->IsGoalkeeperActionActive())
    {
        TrainingGoalkeeper6T->SetScriptedLocomotionVelocity(
            FVector::ZeroVector, ESoccerAIMovementMode::Run,
            ESoccerAIMovementReason::NormalPlay);
        return;
    }

    // 6AA: the exact same geometry implementation called by SoccerMatchManager.
    const FVector GoalCenter = Field->GetGoalCenterWorldLocation(+1.f);
    const FVector Inward = Field->GetActorRotation().RotateVector(FVector(-1.f, 0.f, 0.f));
    const FVector Lateral = Field->GetActorRotation().RotateVector(FVector(0.f, 1.f, 0.f));
    FSoccerGoalkeeperPositioningConfig6AA Config;
    Config.MaximumDepth = KeeperMaxDepth6V;
    Config.FullDepthDistance = KeeperFullDepthDistance6V;
    Config.LateralRange = KeeperLateralRange6V;
    Config.BallFollowAlpha = KeeperFarFollowAlpha6V;
    Config.PostSafetyMargin = KeeperPostMargin6V;
    Config.FullCoverageAngle = KeeperFullCoverageAngle6V;
    Config.ResponseExponent = KeeperAngleResponseExponent6V;
    Config.NearFollowDepth = KeeperNearFollowDepth6V;
    Config.FarFollowDepth = KeeperFarFollowDepth6V;
    const FVector Desired = FSoccerGoalkeeperPositioningShared6AA::Calculate(
        GoalCenter, Inward, Lateral, KeeperHomeDepth6V,
        TrainingGoalkeeper6T->GetActorLocation().Z, PracticeBall6B,
        Config, true);

    if (ASoccerAIController* KeeperController =
        Cast<ASoccerAIController>(TrainingGoalkeeper6T->GetController()))
    {
        // 6AA.2: a MoveTo request every frame can continuously replace the
        // previous path. Keep it alive until the target changes appreciably,
        // the path goes idle, or a periodic refresh is due.
        const float Now = GetWorld()->GetTimeSeconds();
        const float ErrorCm = FVector::Dist2D(TrainingGoalkeeper6T->GetActorLocation(), Desired);
        const bool bTargetChanged = !bHasIssuedKeeperTarget6AA2 ||
            FVector::Dist2D(LastIssuedKeeperTarget6AA2, Desired) > 35.f;
        const bool bMovementIdle = KeeperController->GetMoveStatus() == EPathFollowingStatus::Idle;
        if (ErrorCm > 30.f &&
            (bTargetChanged || (bMovementIdle && Now - LastKeeperMoveRequestAt6AA2 >= 0.35f) ||
             Now - LastKeeperMoveRequestAt6AA2 >= 1.0f))
        {
            // Retain the shared controller movement routine and match geometry.
            KeeperController->MoveTrainingGoalkeeperTo6AA(Desired);
            LastIssuedKeeperTarget6AA2 = Desired;
            LastKeeperMoveRequestAt6AA2 = Now;
            bHasIssuedKeeperTarget6AA2 = true;
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[Training6AA] Keeper has no SoccerAIController; cannot issue match-style movement."));
    }
    if (GetWorld() && GetWorld()->GetTimeSeconds() - LastPositionDebug6AA >= 1.f)
    {
        LastPositionDebug6AA = GetWorld()->GetTimeSeconds();
        ASoccerAIController* DebugController =
            Cast<ASoccerAIController>(TrainingGoalkeeper6T->GetController());
        UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
        FNavLocation NavKeeper, NavTarget;
        const FVector NavExtent(120.f, 120.f, 250.f);
        const bool bKeeperOnNav = NavSys && NavSys->ProjectPointToNavigation(
            TrainingGoalkeeper6T->GetActorLocation(), NavKeeper, NavExtent);
        const bool bTargetOnNav = NavSys && NavSys->ProjectPointToNavigation(
            Desired, NavTarget, NavExtent);
        UE_LOG(LogTemp, Display,
            TEXT("[Training6AA2] Keeper=%s actual=%s target=%s errorCm=%.1f moveStatus=%d controllerTick=%s navKeeper=%s navTarget=%s"),
            *GetNameSafe(TrainingGoalkeeper6T),
            *TrainingGoalkeeper6T->GetActorLocation().ToCompactString(),
            *Desired.ToCompactString(),
            FVector::Dist2D(TrainingGoalkeeper6T->GetActorLocation(), Desired),
            DebugController ? static_cast<int32>(DebugController->GetMoveStatus()) : -1,
            DebugController && DebugController->IsActorTickEnabled() ? TEXT("ON") : TEXT("OFF"),
            bKeeperOnNav ? TEXT("YES") : TEXT("NO"),
            bTargetOnNav ? TEXT("YES") : TEXT("NO"));
    }
    const FVector Face = PracticeBall6B->GetActorLocation() - TrainingGoalkeeper6T->GetActorLocation();
    const FVector FlatFace(Face.X, Face.Y, 0.f);
    if (!FlatFace.IsNearlyZero())
        TrainingGoalkeeper6T->SetActorRotation(FlatFace.Rotation());

}

void ASoccerTrainingMovementGameMode6B::RegisterChargedShotRelease6M(float ChargeFraction)
{
    if (!bChargedShotLesson6M || !IsExerciseRunning6H() || bShotReleased6M || bBetweenShotAttempts6N || !IsValid(PracticeBall6B)) return;
    bShotReleased6M = true;
    bGoalScored6M = false;
    bShotInFlight6M = false;
    bKeeperHasPendingPlan6Y = false;
    ShotCharge6M = FMath::Clamp(ChargeFraction, 0.f, 1.f);
    ShotReleaseTime6M = GetWorld()->GetTimeSeconds();
    if (ASoccerField* Field = FindSharedField6L())
    {
        PreviousBallLocal6M = Field->WorldToPitchLocal(PracticeBall6B->GetActorLocation());
        bHavePreviousBall6M = true;
    }
    UE_LOG(LogTemp, Display, TEXT("[Training6M] Charged shot released. Charge=%.0f%%"), ShotCharge6M * 100.f);
}

void ASoccerTrainingMovementGameMode6B::TickChargedShot6M(float DeltaSeconds)
{
    (void)DeltaSeconds;
    // The shot is delivered by the existing character code, possibly after a montage.
    // Allow the player to see the result briefly, then reset ball and trainee.
    if (bBetweenShotAttempts6N)
    {
        if (GetWorld()->GetTimeSeconds() >= NextAttemptAt6N)
        {
            bBetweenShotAttempts6N = false;
            PrepareShotAttempt6N();
        }
        return;
    }
    if (!bShotReleased6M || !IsValid(PracticeBall6B)) return;
    ASoccerField* Field = FindSharedField6L();
    if (!Field) return;
    const float SinceRelease = GetWorld()->GetTimeSeconds() - ShotReleaseTime6M;
    const FVector Current = Field->WorldToPitchLocal(PracticeBall6B->GetActorLocation());
    const FVector BallVelocity = PracticeBall6B->GetBallPhysicsVelocity();
    if (BallVelocity.Size() > 140.f) bShotInFlight6M = true;
    // 6Z: only the match animated-contact solver can validate a save.
    // The AnimNotifyState_GKContactWindow drives hand/body checks on the
    // existing goalkeeper controller, even with its regular Tick disabled.
    if (bGoalkeeperShotLesson6T && bShotInFlight6M &&
        !bKeeperSaveTriggered6T && IsValid(TrainingGoalkeeper6T))
    {
        ASoccerAIController* KeeperController =
            Cast<ASoccerAIController>(TrainingGoalkeeper6T->GetController());
        if (IsValid(KeeperController) && KeeperController->ConsumeTrainingGoalkeeperContact6Z())
        {
            bKeeperSaveTriggered6T = true;
            bKeeperTouchedThisShot6AF = true;
            KeeperTouchTime6AF = GetWorld()->GetTimeSeconds();
            UE_LOG(LogTemp, Display,
                TEXT("[Training6AF] Keeper contact attempt=%d; awaiting final trajectory, not yet a save"),
                ShotAttemptIndex6N + 1);
            // Do not resolve here. The ball might still cross the goal line.
            // The match planner is left alone; only training scoring changes.
        }
    }

    // 6AB: The SAME ShotPending -> UpdatePending -> montage commit cycle
    // used by the match. The regular tactical AI Tick remains disabled.
    if (bGoalkeeperShotLesson6T && bShotInFlight6M &&
        !bKeeperSaveTriggered6T && IsValid(TrainingGoalkeeper6T))
    {
        if (ASoccerAIController* KeeperController =
            Cast<ASoccerAIController>(TrainingGoalkeeper6T->GetController()))
        {
            KeeperController->TickTrainingGoalkeeperPlanner6AB(
                TrainingGoalkeeper6T, PracticeBall6B, DeltaSeconds);
        }
    }
    // 6Z: removed the old 6W radius-based automatic deflection.
    // It could report a save without the hands/body intersecting the ball,
    // and interfere with the actual montage's contact windows.
    if (bShotInFlight6M && bHavePreviousBall6M)
    {
        const float GoalX = SoccerFieldDimensions::RightGoalLineX;
        if (PreviousBallLocal6M.X < GoalX && Current.X >= GoalX && Current.X > PreviousBallLocal6M.X)
        {
            const float Fraction = FMath::Clamp((GoalX - PreviousBallLocal6M.X) /
                FMath::Max(0.001f, Current.X - PreviousBallLocal6M.X), 0.f, 1.f);
            const FVector Crossing = FMath::Lerp(PreviousBallLocal6M, Current, Fraction);
            const float Radius = PracticeBall6B->GetBallRadiusCm();
            const bool bInsidePosts = FMath::Abs(Crossing.Y) + Radius < SoccerFieldDimensions::GoalHalfWidthCm;
            const bool bUnderCrossbar = Crossing.Z + Radius < SoccerFieldDimensions::GoalHeightCm && Crossing.Z >= 0.f;
            const ETrainingShotOutcome6P Outcome = !bInsidePosts ? ETrainingShotOutcome6P::Wide
                : !bUnderCrossbar ? ETrainingShotOutcome6P::OverBar
                : ETrainingShotOutcome6P::Goal;
            // The crossing is authoritative: a touched shot that enters is a GOAL.
            // A confirmed frame bounce can still produce a goal afterwards.
            if (Outcome == ETrainingShotOutcome6P::Goal)
            {
                ShotEndDetail6AE = bKeeperTouchedThisShot6AF
                    ? TEXT("gol despues de contacto del arquero")
                    : (bPostTouchedThisShot6AF || bCrossbarTouchedThisShot6AF)
                        ? TEXT("gol despues de rebote en el marco")
                        : TEXT("cruce valido de la linea de gol");
            }
            else if (bKeeperTouchedThisShot6AF)
                ShotEndDetail6AE = TEXT("arquero desvio el remate fuera de la boca del arco");
            else if (bCrossbarTouchedThisShot6AF || bPostTouchedThisShot6AF)
                ShotEndDetail6AE = TEXT("rebote en el marco sin gol");
            else
                ShotEndDetail6AE = Outcome == ETrainingShotOutcome6P::Wide
                    ? TEXT("cruce de linea de fondo fuera de postes")
                    : TEXT("cruce sobre travesano o altura no valida");
            const ETrainingShotOutcome6P FinalOutcome6AF = Outcome == ETrainingShotOutcome6P::Goal
                ? ETrainingShotOutcome6P::Goal
                : bKeeperTouchedThisShot6AF ? ETrainingShotOutcome6P::Saved
                : bCrossbarTouchedThisShot6AF ? ETrainingShotOutcome6P::Crossbar
                : bPostTouchedThisShot6AF ? ETrainingShotOutcome6P::Post
                : Outcome;
            ResolveShotAttempt6N(FinalOutcome6AF);
            return;
        }
    }
    // 6AE: a ball crossing either touchline is observably out of play even
    // when it never reaches the goal line. Only classify after actual flight;
    // a nearly stationary or pre-kick ball is not evidence of a wide shot.
    if (bShotInFlight6M && bHavePreviousBall6M)
    {
        const float HalfWidth = SoccerFieldDimensions::HalfPitchWidthCm;
        const float BallRadius = PracticeBall6B->GetBallRadiusCm();
        if (FMath::Abs(PreviousBallLocal6M.Y) <= HalfWidth + BallRadius &&
            FMath::Abs(Current.Y) > HalfWidth + BallRadius)
        {
            ShotEndDetail6AE = bKeeperTouchedThisShot6AF
                ? TEXT("desvio del arquero hacia linea lateral")
                : bCrossbarTouchedThisShot6AF ? TEXT("rebote de travesano hacia linea lateral")
                : bPostTouchedThisShot6AF ? TEXT("rebote de poste hacia linea lateral")
                : TEXT("salio por linea lateral");
            ResolveShotAttempt6N(bKeeperTouchedThisShot6AF ? ETrainingShotOutcome6P::Saved
                : bCrossbarTouchedThisShot6AF ? ETrainingShotOutcome6P::Crossbar
                : bPostTouchedThisShot6AF ? ETrainingShotOutcome6P::Post
                : ETrainingShotOutcome6P::Wide);
            return;
        }
    }
    PreviousBallLocal6M = Current;
    bHavePreviousBall6M = true;
    // A keeper who has securely caught the ball has made a save, provided the
    // goal-line crossing check above has had priority. Do not wait 15 seconds.
    if (bGoalkeeperShotLesson6T && bShotInFlight6M &&
        IsValid(TrainingGoalkeeper6T) && TrainingGoalkeeper6T->IsAIPossessingBall())
    {
        ShotEndDetail6AE = TEXT("pelota controlada por el arquero");
        ResolveShotAttempt6N(ETrainingShotOutcome6P::Saved);
        return;
    }
    // Allow time for the character to run up to the ball and play the kick.
    // After ball movement, a settled ball can terminate a missed attempt early.
    if (SinceRelease > 15.f ||
        (bShotInFlight6M && SinceRelease > 3.f && BallVelocity.Size() < 35.f))
    {
        // 6AE: explain WHY there is no definitive result instead of using
        // 'SIN RESOLUCION' as an opaque bucket. No inferred goalkeeper save.
        if (bBarrierContactThisShot6P)
            ShotEndDetail6AE = TEXT("contacto con barrera confirmado");
        else if (!bShotInFlight6M)
            ShotEndDetail6AE = TEXT("no se detecto remate en vuelo");
        else if (BallVelocity.Size() < 35.f)
            ShotEndDetail6AE = TEXT("pelota detenida sin cruce confirmado");
        else
            ShotEndDetail6AE = TEXT("tiempo agotado con pelota en movimiento");
        UE_LOG(LogTemp, Display,
            TEXT("[Training6AE] Closing attempt=%d reason=%s elapsed=%.2f speed=%.1f ballLocal=%s keeperPossession=%s"),
            ShotAttemptIndex6N + 1, *ShotEndDetail6AE, SinceRelease,
            BallVelocity.Size(), *Current.ToCompactString(),
            IsValid(TrainingGoalkeeper6T) && TrainingGoalkeeper6T->IsAIPossessingBall()
                ? TEXT("YES") : TEXT("NO"));
        const ETrainingShotOutcome6P FinalOutcome6AF = bKeeperTouchedThisShot6AF
            ? ETrainingShotOutcome6P::Saved
            : bCrossbarTouchedThisShot6AF ? ETrainingShotOutcome6P::Crossbar
            : bPostTouchedThisShot6AF ? ETrainingShotOutcome6P::Post
            : bBarrierContactThisShot6P ? ETrainingShotOutcome6P::Barrier
            : ETrainingShotOutcome6P::Unresolved;
        if (bKeeperTouchedThisShot6AF)
            ShotEndDetail6AE = TEXT("contacto del arquero, sin gol tras seguimiento");
        else if (bCrossbarTouchedThisShot6AF)
            ShotEndDetail6AE = TEXT("rebote confirmado de travesano sin gol");
        else if (bPostTouchedThisShot6AF)
            ShotEndDetail6AE = TEXT("rebote confirmado de poste sin gol");
        ResolveShotAttempt6N(FinalOutcome6AF);
    }
}

FVector ASoccerTrainingMovementGameMode6B::GetShotBallLocal6N(int32 AttemptIndex) const
{
    // Repeatable positions, inside the playable pitch, scaled to field dimensions.
    const float Distances[10] = {0.30f, 0.36f, 0.31f, 0.37f, 0.26f,
                                 0.40f, 0.33f, 0.38f, 0.28f, 0.42f};
    const float Laterals[10]  = {0.00f, -0.22f, 0.25f, -0.40f, 0.38f,
                                -0.10f, 0.15f, -0.32f, 0.30f, 0.00f};
    const int32 Index = FMath::Clamp(AttemptIndex, 0, 9);
    const float Distance = FMath::Min(2400.f, SoccerFieldDimensions::PitchLengthCm * Distances[Index]);
    return FVector(SoccerFieldDimensions::RightGoalLineX - Distance,
        SoccerFieldDimensions::HalfPitchWidthCm * Laterals[Index], 70.f);
}

void ASoccerTrainingMovementGameMode6B::PrepareShotAttempt6N()
{
    if (!IsValid(PracticeBall6B) || !IsValid(Trainee6B)) return;
    ASoccerField* Field = FindSharedField6L();
    if (!Field)
    {
        UE_LOG(LogTemp, Error, TEXT("[Training6N] Shared field is required for shooting exercises."));
        return;
    }

    // 6AC: finish the PREVIOUS goalkeeper save before touching the practice
    // ball. Finishing a montage/contact test after a ball teleport can change
    // the ball's possession/collision state again. This is training-only;
    // regular match controllers and their Tick are unchanged.
    if (bGoalkeeperShotLesson6T && IsValid(TrainingGoalkeeper6T))
    {
        if (ASoccerAIController* KeeperController =
            Cast<ASoccerAIController>(TrainingGoalkeeper6T->GetController()))
        {
            KeeperController->EndTrainingGoalkeeperPlanner6AB(TrainingGoalkeeper6T);
            KeeperController->ResetTrainingGoalkeeperSave6Z(TrainingGoalkeeper6T);
            KeeperController->StopMovement();
        }
        // 6AC.1: a successful catch may leave logical possession active. The
        // regular match controller releases this during its normal AI cycle,
        // but the training controller deliberately has its Tick disabled.
        const bool bHadPossession6AC1 = TrainingGoalkeeper6T->IsAIPossessingBall();
        if (bHadPossession6AC1)
        {
            TrainingGoalkeeper6T->ReleaseAIBall(false);
        }

        // Stop the authored save montage while the action enum still identifies
        // it. A timed action can already have cleared the enum although its
        // montage remains in the animation slot, leaving the keeper sliding in
        // a catch pose instead of returning to locomotion.
        const bool bWasActionActive6AC1 = TrainingGoalkeeper6T->IsGoalkeeperActionActive();
        TrainingGoalkeeper6T->StopGoalkeeperActionMontage(0.10f);
        if (USkeletalMeshComponent* KeeperMesh6AC1 = TrainingGoalkeeper6T->GetMesh())
        {
            if (UAnimInstance* KeeperAnim6AC1 = KeeperMesh6AC1->GetAnimInstance())
            {
                // Training reset only: clear a leftover action-slot montage
                // even if the timed goalkeeper action has already ended.
                if (UAnimMontage* LeftoverMontage6AC1 = KeeperAnim6AC1->GetCurrentActiveMontage())
                {
                    UE_LOG(LogTemp, Warning,
                        TEXT("[Training6AC1] Stopping leftover keeper montage=%s attempt=%d"),
                        *GetNameSafe(LeftoverMontage6AC1), ShotAttemptIndex6N + 1);
                    KeeperAnim6AC1->Montage_Stop(0.10f);
                }
            }
        }
        TrainingGoalkeeper6T->ClearScriptedLocomotionVelocity();
        UE_LOG(LogTemp, Display,
            TEXT("[Training6AC1] Reset keeper attempt=%d possessionBefore=%s actionBefore=%s possessionAfter=%s"),
            ShotAttemptIndex6N + 1,
            bHadPossession6AC1 ? TEXT("YES") : TEXT("NO"),
            bWasActionActive6AC1 ? TEXT("YES") : TEXT("NO"),
            TrainingGoalkeeper6T->IsAIPossessingBall() ? TEXT("YES") : TEXT("NO"));
        bHasIssuedKeeperTarget6AA2 = false;
        LastKeeperMoveRequestAt6AA2 = -100.f;
        UE_LOG(LogTemp, Display,
            TEXT("[Training6AC] Previous goalkeeper action cleared before ball reset; attempt=%d"),
            ShotAttemptIndex6N + 1);
    }

    // Cancel delayed kick/approach actions before repositioning, to avoid a
    // previous attempt striking a freshly reset ball.
    Trainee6B->ReleaseBallForMatchRestart(false);
    if (UCharacterMovementComponent* Movement = Trainee6B->GetCharacterMovement())
        Movement->StopMovementImmediately();
    // 6AB.2: restore a fresh training ball physics state before teleporting.
    // 6Z.2: freeze the root physics body BEFORE teleporting. Moving a
    // simulating rigid body and then resetting velocity can leave its physics
    // pose out of sync with the actor transform on the next physics step.
    PracticeBall6B->SetPossessed(true);

    const FVector BallLocal = GetShotBallLocal6N(ShotAttemptIndex6N);
    // 6O: update the obstacle layout before the player takes the next kick.
    if (bGoalkeeperShotLesson6T) ClearPracticeBarriers6O();
    else SetupPracticeBarriers6O(BallLocal);
    const FVector PlayerLocal(BallLocal.X - 300.f, BallLocal.Y, 130.f);
    Trainee6B->SetActorLocationAndRotation(Field->PitchLocalToWorld(PlayerLocal),
        Field->GetActorRotation(), false, nullptr, ETeleportType::TeleportPhysics);
    const FVector BallWorld = FindSafeShotBallSpawn6AB2(Field->PitchLocalToWorld(BallLocal));
    UPrimitiveComponent* BallBody = Cast<UPrimitiveComponent>(PracticeBall6B->GetRootComponent());
    if (BallBody)
    {
        BallBody->SetSimulatePhysics(false);
        BallBody->SetPhysicsLinearVelocity(FVector::ZeroVector);
        BallBody->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
        BallBody->SetEnableGravity(true);
        BallBody->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        BallBody->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
    }
    const bool bBallTeleported = PracticeBall6B->SetActorLocationAndRotation(
        BallWorld, Field->GetActorRotation(), false, nullptr, ETeleportType::TeleportPhysics);
    // Resume only after both the actor and its root rigid body have been moved.
    PracticeBall6B->SetPossessed(false);
    PracticeBall6B->StopBallKeepingPhysics();
    const bool bCollisionRestored6AB3 = RestorePracticeBallGroundCollision6AB3(PracticeBall6B);
    if (bCollisionRestored6AB3)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Training6AB3] Collision restored after ball reset attempt=%d"),
            ShotAttemptIndex6N + 1);
    }
    ShotBallResetWorld6AB2 = BallWorld;
    ShotBallResetUntil6AB2 = GetWorld()->GetTimeSeconds() + 1.25f;
    ShotBallNextCheck6AB2 = GetWorld()->GetTimeSeconds() + 0.10f;
    ShotBallResetCorrections6AB2 = 0;
    UE_LOG(LogTemp, Display,
        TEXT("[Training6AB2] Ball reset attempt=%d teleported=%s target=%s actual=%s radius=%.1f sim=%s collision=%d"),
        ShotAttemptIndex6N + 1,
        bBallTeleported ? TEXT("yes") : TEXT("no"),
        *BallWorld.ToString(), *PracticeBall6B->GetActorLocation().ToString(),
        PracticeBall6B->GetBallRadiusCm(),
        BallBody && BallBody->IsSimulatingPhysics() ? TEXT("YES") : TEXT("NO"),
        BallBody ? static_cast<int32>(BallBody->GetCollisionEnabled()) : -1);
    bShotReleased6M = false;
    bShotInFlight6M = false;
    bHavePreviousBall6M = false;
    bGoalScored6M = false;
    ShotCharge6M = 0.f;
    bBarrierContactThisShot6P = false;
    ShotEndDetail6AE.Empty();
    bKeeperTouchedThisShot6AF = false;
    KeeperTouchTime6AF = -1.f;
    bPostTouchedThisShot6AF = false;
    bCrossbarTouchedThisShot6AF = false;
    bKeeperSaveTriggered6T = false;
    bKeeperMontageStarted6X = false;
    bKeeperSelectorLogged6X = false;
    bKeeperHasPendingPlan6Y = false;
    KeeperPlannedDelay6Y = 0.f;
    KeeperNextPlanLogAt6Y = 0.f;
    // 6AC: arm the NEXT shot only after the ball's final collision state
    // has been restored. Do not clear a montage or finish a previous save here.
    if (bGoalkeeperShotLesson6T && IsValid(TrainingGoalkeeper6T))
    {
        if (ASoccerAIController* KeeperController =
            Cast<ASoccerAIController>(TrainingGoalkeeper6T->GetController()))
        {
            const FVector GoalCenter = Field->GetGoalCenterWorldLocation(+1.f);
            const FVector Outfield = Field->GetPitchLengthWorldDirection() * -1.f;
            KeeperController->BeginTrainingGoalkeeperPlanner6AB(
                TrainingGoalkeeper6T, GoalCenter, Outfield);
        }
        // Preserve 6AB.3's proven collision restoration. The first 1.25 s
        // and the entire pre-kick waiting period remain monitored.
        if (RestorePracticeBallGroundCollision6AB3(PracticeBall6B))
        {
            UE_LOG(LogTemp, Warning,
                TEXT("[Training6AC] Practice ball collision restored after planner initialization; attempt=%d"),
                ShotAttemptIndex6N + 1);
        }
    }
    if (bGoalkeeperShotLesson6T)
    {
        UPrimitiveComponent* ReadyBody6AC = Cast<UPrimitiveComponent>(PracticeBall6B->GetRootComponent());
        UE_LOG(LogTemp, Display,
            TEXT("[Training6AC] Ready attempt=%d collision=%d actorCollision=%s physics=%s keeperAction=%s"),
            ShotAttemptIndex6N + 1,
            ReadyBody6AC ? static_cast<int32>(ReadyBody6AC->GetCollisionEnabled()) : -1,
            PracticeBall6B->GetActorEnableCollision() ? TEXT("YES") : TEXT("NO"),
            ReadyBody6AC && ReadyBody6AC->IsSimulatingPhysics() ? TEXT("YES") : TEXT("NO"),
            IsValid(TrainingGoalkeeper6T) && TrainingGoalkeeper6T->IsGoalkeeperActionActive()
                ? TEXT("ACTIVE") : TEXT("CLEARED"));
    }
    UE_LOG(LogTemp, Display, TEXT("[Training6N] Ready: attempt %d/10"), ShotAttemptIndex6N + 1);
}

// Training-only: find the field's actual collision surface rather than
// assuming that local Z=0 is the physical ground in every shared level.
FVector ASoccerTrainingMovementGameMode6B::FindSafeShotBallSpawn6AB2(const FVector& Requested) const
{
    FVector Result = Requested;
    if (!GetWorld() || !IsValid(PracticeBall6B)) return Result;
    const float Radius = FMath::Max(1.f, PracticeBall6B->GetBallRadiusCm());
    FHitResult GroundHit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(TrainingBallGround6AB2), false);
    Params.AddIgnoredActor(PracticeBall6B);
    if (IsValid(Trainee6B)) Params.AddIgnoredActor(Trainee6B);
    if (IsValid(TrainingGoalkeeper6T)) Params.AddIgnoredActor(TrainingGoalkeeper6T);
    const bool bGround = GetWorld()->LineTraceSingleByChannel(
        GroundHit, Requested + FVector(0.f, 0.f, 350.f),
        Requested - FVector(0.f, 0.f, 800.f), ECC_WorldStatic, Params);
    if (bGround && GroundHit.ImpactNormal.Z >= 0.65f)
    {
        // Place the bottom of the ball just above the collision surface.
        Result.Z = GroundHit.ImpactPoint.Z + Radius + 5.f;
    }
    UE_LOG(LogTemp, Display,
        TEXT("[Training6AB2] Ground trace hit=%s groundZ=%.1f normalZ=%.2f requestedZ=%.1f spawnZ=%.1f"),
        bGround ? TEXT("YES") : TEXT("NO"),
        bGround ? GroundHit.ImpactPoint.Z : 0.f,
        bGround ? GroundHit.ImpactNormal.Z : 0.f, Requested.Z, Result.Z);
    return Result;
}

// The first moments after a reset are the only time when we correct a
// missed floor collision. Never touch the ball after a kick/release.
void ASoccerTrainingMovementGameMode6B::VerifyShotBallReset6AB2()
{
    if (!bGoalkeeperShotLesson6T || bShotReleased6M || bBetweenShotAttempts6N ||
        !IsValid(PracticeBall6B) || !GetWorld()) return;
    const float Now = GetWorld()->GetTimeSeconds();
    // Maintain collision throughout the waiting period, not only for the first
    // 1.25 seconds. Never interfere once the player has launched the shot.
    UPrimitiveComponent* Body = Cast<UPrimitiveComponent>(PracticeBall6B->GetRootComponent());
    const bool bCollisionMissing6AB3 = Body &&
        (!PracticeBall6B->GetActorEnableCollision() ||
         Body->GetCollisionEnabled() != ECollisionEnabled::QueryAndPhysics ||
         Body->GetCollisionResponseToChannel(ECC_WorldStatic) != ECR_Block);
    if (bCollisionMissing6AB3)
    {
        const int32 OldCollision = Body ? static_cast<int32>(Body->GetCollisionEnabled()) : -1;
        const bool bActorCollision = PracticeBall6B->GetActorEnableCollision();
        RestorePracticeBallGroundCollision6AB3(PracticeBall6B);
        UE_LOG(LogTemp, Warning,
            TEXT("[Training6AB3] Ground collision was disabled while waiting: attempt=%d bodyBefore=%d actorBefore=%s bodyAfter=%d actorAfter=%s"),
            ShotAttemptIndex6N + 1, OldCollision, bActorCollision ? TEXT("YES") : TEXT("NO"),
            Body ? static_cast<int32>(Body->GetCollisionEnabled()) : -1,
            PracticeBall6B->GetActorEnableCollision() ? TEXT("YES") : TEXT("NO"));
    }
    if (Now < ShotBallNextCheck6AB2) return;
    if (Now > ShotBallResetUntil6AB2 && !bCollisionMissing6AB3) return;
    ShotBallNextCheck6AB2 = Now + 0.20f;
    const FVector Current = PracticeBall6B->GetActorLocation();
    const FVector Velocity = PracticeBall6B->GetBallPhysicsVelocity();
    const float Radius = FMath::Max(1.f, PracticeBall6B->GetBallRadiusCm());
    const float FloorZ = ShotBallResetWorld6AB2.Z - Radius - 5.f;
    // The mesh's visual bounds radius is not necessarily its physics contact
    // radius. A resting center at Z=11 on a floor at Z=0 is valid: only regard
    // the center crossing BELOW the ground as genuine penetration.
    const bool bBelowSurface = Current.Z < FloorZ - 5.f;
    UE_LOG(LogTemp, Display,
        TEXT("[Training6AB2] Settle attempt=%d z=%.1f floorZ=%.1f vz=%.1f sim=%s collision=%d below=%s"),
        ShotAttemptIndex6N + 1, Current.Z, FloorZ, Velocity.Z,
        Body && Body->IsSimulatingPhysics() ? TEXT("YES") : TEXT("NO"),
        Body ? static_cast<int32>(Body->GetCollisionEnabled()) : -1,
        bBelowSurface ? TEXT("YES") : TEXT("NO"));
    if (!bBelowSurface) return;
    if (ShotBallResetCorrections6AB2 >= 4)
    {
        UE_LOG(LogTemp, Error,
            TEXT("[Training6AB3] Repeated ground penetration despite collision restoration; attempt=%d z=%.1f floorZ=%.1f"),
            ShotAttemptIndex6N + 1, Current.Z, FloorZ);
        return;
    }
    ++ShotBallResetCorrections6AB2;
    PracticeBall6B->SetPossessed(true);
    if (Body)
    {
        Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        Body->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
        Body->SetEnableGravity(true);
    }
    const FVector SafeWorld = ShotBallResetWorld6AB2;
    PracticeBall6B->SetActorLocation(SafeWorld, false, nullptr, ETeleportType::TeleportPhysics);
    PracticeBall6B->SetPossessed(false);
    PracticeBall6B->StopBallKeepingPhysics();
    RestorePracticeBallGroundCollision6AB3(PracticeBall6B);
    UE_LOG(LogTemp, Warning,
        TEXT("[Training6AB2] Corrected sinking practice ball, attempt=%d correction=%d"),
        ShotAttemptIndex6N + 1, ShotBallResetCorrections6AB2);
}

void ASoccerTrainingMovementGameMode6B::ClearPracticeBarriers6O()
{
    for (AStaticMeshActor* Barrier : PracticeBarriers6O)
    {
        if (IsValid(Barrier)) Barrier->Destroy();
    }
    PracticeBarriers6O.Empty();
}

void ASoccerTrainingMovementGameMode6B::SetupPracticeBarriers6O(const FVector& BallLocal)
{
    ClearPracticeBarriers6O();
    // The first four shots teach basic accuracy. The final six introduce
    // gradually wider walls, requiring players to select a lofted trajectory.
    if (ShotAttemptIndex6N < 4 || !GetWorld()) return;
    ASoccerField* Field = FindSharedField6L();
    if (!Field) return;

    UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (!CubeMesh)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Training6O] Cannot load the obstacle mesh."));
        return;
    }

    // Position the barrier on the straight line from the ball to the goal.
    // Coordinates follow the field, not absolute world X/Y axes.
    const float TravelX = SoccerFieldDimensions::RightGoalLineX - BallLocal.X;
    const float ForwardOffset = FMath::Min(650.f, TravelX * 0.40f);
    const float FractionToGoal = ForwardOffset / FMath::Max(1.f, TravelX);
    const float CenterY = FMath::Lerp(BallLocal.Y, 0.f, FractionToGoal);
    const float WallWidth = BarrierWidthCm6O * (ShotAttemptIndex6N >= 7 ? 1.25f : 1.f);
    const float WallHeight = BarrierHeightCm6O * (ShotAttemptIndex6N >= 8 ? 1.12f : 1.f);
    const float WallThickness = 45.f;
    const int32 ObstacleCount = ShotAttemptIndex6N >= 8 ? 3 : 2;
    const float ObstacleWidth = WallWidth / ObstacleCount;

    for (int32 Index = 0; Index < ObstacleCount; ++Index)
    {
        const float OffsetY = (Index - 0.5f * (ObstacleCount - 1)) * ObstacleWidth;
        const FVector LocalPosition(BallLocal.X + ForwardOffset,
            CenterY + OffsetY, 0.5f * WallHeight);
        AStaticMeshActor* Barrier = GetWorld()->SpawnActor<AStaticMeshActor>(
            Field->PitchLocalToWorld(LocalPosition), Field->GetActorRotation());
        if (!IsValid(Barrier)) continue;
        UStaticMeshComponent* Mesh = Barrier->GetStaticMeshComponent();
        if (!Mesh) { Barrier->Destroy(); continue; }
        Mesh->SetMobility(EComponentMobility::Movable);
        Mesh->SetStaticMesh(CubeMesh);
        Mesh->SetSimulatePhysics(false);
        Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        Mesh->SetCollisionProfileName(TEXT("BlockAll"));
        Mesh->SetWorldScale3D(FVector(WallThickness / 100.f,
            ObstacleWidth / 100.f, WallHeight / 100.f));
        // Physics contact is reported by the actor hit delegate. The ball is
        // checked in the handler so touching the barrier with the player
        // does not count as a blocked shot.
        Mesh->SetNotifyRigidBodyCollision(true);
        Barrier->OnActorHit.AddDynamic(this, &ASoccerTrainingMovementGameMode6B::OnPracticeBarrierHit6P);
        PracticeBarriers6O.Add(Barrier);
    }
    UE_LOG(LogTemp, Display, TEXT("[Training6O] Shot %d: %d obstacles, width %.0f cm, height %.0f cm"),
        ShotAttemptIndex6N + 1, PracticeBarriers6O.Num(), WallWidth, WallHeight);
}

void ASoccerTrainingMovementGameMode6B::OnPracticeBarrierHit6P(
    AActor* SelfActor, AActor* OtherActor, FVector NormalImpulse, const FHitResult& Hit)
{
    (void)SelfActor;
    (void)NormalImpulse;
    (void)Hit;
    if (bChargedShotLesson6M && IsExerciseRunning6H() &&
        bShotReleased6M && !bBetweenShotAttempts6N &&
        IsValid(PracticeBall6B) && OtherActor == PracticeBall6B)
    {
        bBarrierContactThisShot6P = true;
        bShotInFlight6M = true;
        UE_LOG(LogTemp, Display, TEXT("[Training6P] Ball touched practice barrier in attempt %d"),
            ShotAttemptIndex6N + 1);
    }
}

FString ASoccerTrainingMovementGameMode6B::ShotOutcomeLabel6P(ETrainingShotOutcome6P Outcome)
{
    switch (Outcome)
    {
    case ETrainingShotOutcome6P::Goal: return TEXT("GOL");
    case ETrainingShotOutcome6P::Saved: return TEXT("ATAJADA");
    case ETrainingShotOutcome6P::Post: return TEXT("POSTE SIN GOL");
    case ETrainingShotOutcome6P::Crossbar: return TEXT("TRAVESANO SIN GOL");
    case ETrainingShotOutcome6P::Wide: return TEXT("DESVIADO");
    case ETrainingShotOutcome6P::OverBar: return TEXT("SOBRE EL TRAVESANO");
    case ETrainingShotOutcome6P::Barrier: return TEXT("BARRERA");
    default: return TEXT("SIN RESOLUCION");
    }
}

FString ASoccerTrainingMovementGameMode6B::GetLastShotFeedback6P() const
{
    if (ShotOutcomes6P.Num() == 0) return TEXT("");
    const int32 Index = ShotOutcomes6P.Num() - 1;
    FString Message = FString::Printf(TEXT("Intento %d: %s"),
        Index + 1, *ShotOutcomeLabel6P(ShotOutcomes6P[Index]));
    if (ShotBarrierContacts6P.IsValidIndex(Index) && ShotBarrierContacts6P[Index] &&
        ShotOutcomes6P[Index] != ETrainingShotOutcome6P::Barrier)
    {
        Message += TEXT(" (toco la barrera)");
    }
    if (ShotGoalsAfterKeeperTouch6AF.IsValidIndex(Index) && ShotGoalsAfterKeeperTouch6AF[Index])
        Message += TEXT(" (toco el arquero y entro)");
    return Message;
}

FString ASoccerTrainingMovementGameMode6B::GetShotResultsReport6P() const
{
    int32 Wide = 0, Over = 0, Blocked = 0, Saved = 0, Unresolved = 0, BarrierTouches = 0;
    int32 PostsWithoutGoal = 0, CrossbarsWithoutGoal = 0;
    int32 KeeperTouches = 0, GoalsAfterKeeperTouch = 0;
    int32 PostContacts = 0, CrossbarContacts = 0, GoalsAfterFrameContact = 0;

    for (int32 Index = 0; Index < ShotOutcomes6P.Num(); ++Index)
    {
        const ETrainingShotOutcome6P Outcome = ShotOutcomes6P[Index];
        switch (Outcome)
        {
        case ETrainingShotOutcome6P::Saved: ++Saved; break;
        case ETrainingShotOutcome6P::Post: ++PostsWithoutGoal; break;
        case ETrainingShotOutcome6P::Crossbar: ++CrossbarsWithoutGoal; break;
        case ETrainingShotOutcome6P::Wide: ++Wide; break;
        case ETrainingShotOutcome6P::OverBar: ++Over; break;
        case ETrainingShotOutcome6P::Barrier: ++Blocked; break;
        case ETrainingShotOutcome6P::Unresolved: ++Unresolved; break;
        default: break;
        }
        const bool bKeeperContact = ShotGoalkeeperTouches6AF.IsValidIndex(Index)
            && ShotGoalkeeperTouches6AF[Index];
        const bool bGoalAfterKeeper = ShotGoalsAfterKeeperTouch6AF.IsValidIndex(Index)
            && ShotGoalsAfterKeeperTouch6AF[Index];
        const bool bPostContact = ShotPostTouches6AG.IsValidIndex(Index)
            && ShotPostTouches6AG[Index];
        const bool bCrossbarContact = ShotCrossbarTouches6AG.IsValidIndex(Index)
            && ShotCrossbarTouches6AG[Index];
        if (bKeeperContact) ++KeeperTouches;
        if (bGoalAfterKeeper) ++GoalsAfterKeeperTouch;
        if (bPostContact) ++PostContacts;
        if (bCrossbarContact) ++CrossbarContacts;
        if (Outcome == ETrainingShotOutcome6P::Goal && (bPostContact || bCrossbarContact))
            ++GoalsAfterFrameContact;
        if (ShotBarrierContacts6P.IsValidIndex(Index) && ShotBarrierContacts6P[Index])
            ++BarrierTouches;
    }

    FString Report;
    if (bGoalkeeperShotLesson6T)
    {
        const int32 Total = ShotOutcomes6P.Num();
        const int32 OnTarget = GoalsScored6N + Saved;
        const int32 DirectGoals = FMath::Max(0, GoalsScored6N - GoalsAfterKeeperTouch);
        const float Accuracy = Total > 0 ? (100.f * OnTarget / Total) : 0.f;
        const float Conversion = OnTarget > 0 ? (100.f * GoalsScored6N / OnTarget) : 0.f;
        const float SavePct = OnTarget > 0 ? (100.f * Saved / OnTarget) : 0.f;

        Report = FString::Printf(
            TEXT("REMATES CONTRA ARQUERO\n"
                 "================================\n"
                 "RESUMEN  (%d intentos)\n"
                 "Goles: %d  |  Atajadas: %d\n"
                 "Al arco: %d/%d (%.1f%%)\n"
                 "Conversion de remates al arco: %.1f%%\n"
                 "Porcentaje de atajadas: %.1f%%\n\n"
                 "INTERVENCIONES DEL ARQUERO\n"
                 "Goles sin toque del arquero: %d\n"
                 "Goles tras toque del arquero: %d\n"
                 "Intentos con contacto del arquero: %d\n\n"
                 "POSTES Y TRAVESANO\n"
                 "Postes sin gol: %d  |  Travesanos sin gol: %d\n"
                 "Goles tras rebote en el marco: %d\n"
                 "Intentos con impacto en poste: %d\n"
                 "Intentos con impacto en travesano: %d\n\n"
                 "OTROS RESULTADOS\n"
                 "Desviados: %d  |  Sobre travesano: %d\n"
                 "Barrera: %d  |  Sin resolucion: %d\n"),
            Total, GoalsScored6N, Saved, OnTarget, Total, Accuracy,
            Conversion, SavePct, DirectGoals, GoalsAfterKeeperTouch, KeeperTouches,
            PostsWithoutGoal, CrossbarsWithoutGoal, GoalsAfterFrameContact,
            PostContacts, CrossbarContacts, Wide, Over, Blocked, Unresolved);

        // A goal with a keeper touch or frame impact is included in Goals
        // exactly once. The other counters describe events, not extra shots.
        Report += TEXT("\n================================\nDETALLE DE CADA REMATE\n");
        for (int32 Index = 0; Index < Total; ++Index)
        {
            const float Charge = ShotCharges6P.IsValidIndex(Index)
                ? ShotCharges6P[Index] * 100.f : 0.f;
            Report += FString::Printf(TEXT("\n%02d. %s  |  Potencia %.0f%%\n"),
                Index + 1, *ShotOutcomeLabel6P(ShotOutcomes6P[Index]), Charge);
            const bool bKeeperTouch = ShotGoalkeeperTouches6AF.IsValidIndex(Index)
                && ShotGoalkeeperTouches6AF[Index];
            const bool bPost = ShotPostTouches6AG.IsValidIndex(Index)
                && ShotPostTouches6AG[Index];
            const bool bCrossbar = ShotCrossbarTouches6AG.IsValidIndex(Index)
                && ShotCrossbarTouches6AG[Index];
            if (bKeeperTouch || bPost || bCrossbar)
            {
                Report += TEXT("   Contactos:");
                if (bKeeperTouch) Report += TEXT(" arquero");
                if (bPost) Report += TEXT(" poste");
                if (bCrossbar) Report += TEXT(" travesano");
                Report += TEXT("\n");
            }
            if (ShotGoalsAfterKeeperTouch6AF.IsValidIndex(Index)
                && ShotGoalsAfterKeeperTouch6AF[Index])
            {
                Report += TEXT("   GOL TRAS INTERVENCION DEL ARQUERO\n");
            }
            else if (ShotOutcomes6P[Index] == ETrainingShotOutcome6P::Goal
                && (bPost || bCrossbar))
            {
                Report += TEXT("   GOL TRAS REBOTE EN EL MARCO\n");
            }
            const FString Reason = ShotEndDetails6AE.IsValidIndex(Index)
                ? ShotEndDetails6AE[Index] : TEXT("motivo no registrado");
            Report += FString::Printf(TEXT("   Motivo: %s\n"), *Reason);
        }
        if (Unresolved > 0)
            Report += TEXT("\nNota: los intentos sin resolucion no cuentan como goles ni atajadas.\n");
        Report += TEXT("\nLos goles con toque del arquero o rebote en el marco ya estan incluidos en Goles.\n");
        return Report;
    }

    // Keep the original report layout for REMATES without a goalkeeper.
    Report = FString::Printf(
        TEXT("Desviados: %d  |  Sobre travesano: %d\n"
             "Atajadas: %d  |  Barrera: %d  |  Sin resolucion: %d  |  Contactos barrera: %d\n"),
        Wide, Over, Saved, Blocked, Unresolved, BarrierTouches);
    Report += TEXT("\nDetalle por intento:\n");
    for (int32 Index = 0; Index < ShotOutcomes6P.Num(); ++Index)
    {
        if (Index % 2 == 0 && Index > 0) Report += TEXT("\n");
        else if (Index % 2 == 1) Report += TEXT("   |   ");
        const float Charge = ShotCharges6P.IsValidIndex(Index) ? ShotCharges6P[Index] * 100.f : 0.f;
        Report += FString::Printf(TEXT("%02d: %s (%.0f%%)"),
            Index + 1, *ShotOutcomeLabel6P(ShotOutcomes6P[Index]), Charge);
    }
    return Report;
}

void ASoccerTrainingMovementGameMode6B::ResolveShotAttempt6N(ETrainingShotOutcome6P Outcome)
{
    if (!bShotReleased6M || bBetweenShotAttempts6N || bComplete6B) return;
    const bool bGoal = Outcome == ETrainingShotOutcome6P::Goal;
    bGoalScored6M = bGoal;
    ShotOutcomes6P.Add(Outcome);
    ShotCharges6P.Add(ShotCharge6M);
    ShotBarrierContacts6P.Add(bBarrierContactThisShot6P);
    // 6AE: record supporting evidence, never infer a save from a timeout.
    const FString Reason6AE = ShotEndDetail6AE.IsEmpty()
        ? TEXT("resultado detectado sin detalle adicional") : ShotEndDetail6AE;
    ShotEndDetails6AE.Add(Reason6AE);
    ShotGoalkeeperTouches6AF.Add(bKeeperTouchedThisShot6AF);
    ShotGoalsAfterKeeperTouch6AF.Add(bGoal && bKeeperTouchedThisShot6AF);
    ShotPostTouches6AG.Add(bPostTouchedThisShot6AF);
    ShotCrossbarTouches6AG.Add(bCrossbarTouchedThisShot6AF);
    if (bGoalkeeperShotLesson6T)
    {
        UE_LOG(LogTemp, Display,
            TEXT("[Training6AF] Attempt=%d outcome=%s keeperTouch=%s goalAfterKeeperTouch=%s post=%s crossbar=%s reason=%s"),
            ShotsCompleted6N + 1, *ShotOutcomeLabel6P(Outcome),
            bKeeperTouchedThisShot6AF ? TEXT("YES") : TEXT("NO"),
            bGoal && bKeeperTouchedThisShot6AF ? TEXT("YES") : TEXT("NO"),
            bPostTouchedThisShot6AF ? TEXT("YES") : TEXT("NO"),
            bCrossbarTouchedThisShot6AF ? TEXT("YES") : TEXT("NO"), *Reason6AE);
    }
    if (bGoalkeeperShotLesson6T)
    {
        UE_LOG(LogTemp, Display, TEXT("[Training6AE] Attempt=%d reason=%s outcome=%s"),
            ShotsCompleted6N + 1, *Reason6AE, *ShotOutcomeLabel6P(Outcome));
    }
    ++ShotsCompleted6N;
    if (bGoal) ++GoalsScored6N;
    if (bGoalkeeperShotLesson6T)
    {
        const bool bConfirmedOnTarget6AD =
            Outcome == ETrainingShotOutcome6P::Goal || Outcome == ETrainingShotOutcome6P::Saved;
        UE_LOG(LogTemp, Display,
            TEXT("[Training6AD] Attempt=%d/10 outcome=%s onTargetConfirmed=%s savedByAnimatedContact=%s"),
            ShotsCompleted6N, *ShotOutcomeLabel6P(Outcome),
            bConfirmedOnTarget6AD ? TEXT("YES") : TEXT("NO"),
            Outcome == ETrainingShotOutcome6P::Saved ? TEXT("YES") : TEXT("NO"));
    }
    UE_LOG(LogTemp, Display, TEXT("[Training6P] Attempt %d/10: %s, charge %.0f%%, barrier contact %s"),
        ShotsCompleted6N, *ShotOutcomeLabel6P(Outcome), ShotCharge6M * 100.f,
        bBarrierContactThisShot6P ? TEXT("YES") : TEXT("NO"));
    if (ShotsCompleted6N >= 10)
    {
        if (bGoalkeeperShotLesson6T)
        {
            UE_LOG(LogTemp, Display, TEXT("[Training6AD] Final report:\n%s"),
                *GetShotResultsReport6P());
        }
        ClearPracticeBarriers6O();
        bComplete6B = true;
        if (IsValid(Trainee6B)) Trainee6B->ReleaseBallForMatchRestart(false);
        if (ASoccerTrainingPlayerController6B* PC = Cast<ASoccerTrainingPlayerController6B>(GetWorld()->GetFirstPlayerController()))
            PC->ShowExerciseResults6H();
    }
    else
    {
        ShotAttemptIndex6N = ShotsCompleted6N;
        bBetweenShotAttempts6N = true;
        const float Delay = (bGoalkeeperShotLesson6T && Outcome == ETrainingShotOutcome6P::Saved)
            ? FMath::Max(1.5f, KeeperSaveRecoverySeconds6W) : 1.5f;
        NextAttemptAt6N = GetWorld()->GetTimeSeconds() + Delay;
    }
}

void ASoccerTrainingMovementGameMode6B::RegisterAutoPassClick6J()
{
    if (!bAutoPassLesson6J || !IsExerciseRunning6H()) return;
    bAutoPassArmed6J = true;
    ++AutoPassCount6J;
    UE_LOG(LogTemp, Display, TEXT("[Training6J] Autopass click #%d, target %d/4"),
        AutoPassCount6J, CheckpointIndex6B + 1);
}

void ASoccerTrainingMovementGameMode6B::BeginExercise6H()
{
    if (bExerciseStarted6H) return;
    bExerciseStarted6H = true;
    bAutoPassArmed6J = false;
    AutoPassCount6J = 0;
    bShotReleased6M = false;
    bGoalScored6M = false;
    bShotInFlight6M = false;
    bHavePreviousBall6M = false;
    ShotCharge6M = 0.f;
    ShotAttemptIndex6N = 0;
    ShotsCompleted6N = 0;
    GoalsScored6N = 0;
    ShotOutcomes6P.Empty();
    ShotCharges6P.Empty();
    ShotBarrierContacts6P.Empty();
    ShotEndDetails6AE.Empty();
    ShotGoalkeeperTouches6AF.Empty();
    ShotGoalsAfterKeeperTouch6AF.Empty();
    ShotPostTouches6AG.Empty();
    ShotCrossbarTouches6AG.Empty();
    bTrackFrameImpact6AF = bGoalkeeperShotLesson6T;
    if (bTrackFrameImpact6AF && IsValid(PracticeBall6B))
    {
        if (UPrimitiveComponent* BallBody6AF = Cast<UPrimitiveComponent>(PracticeBall6B->GetRootComponent()))
        {
            BallBody6AF->OnComponentHit.RemoveDynamic(this, &ASoccerTrainingMovementGameMode6B::OnPracticeBallHit6AF);
            BallBody6AF->SetNotifyRigidBodyCollision(true);
            BallBody6AF->OnComponentHit.AddDynamic(this, &ASoccerTrainingMovementGameMode6B::OnPracticeBallHit6AF);
        }
    }
    ShotEndDetail6AE.Empty();
    bBarrierContactThisShot6P = false;
    bBetweenShotAttempts6N = false;
    bComplete6B = false;
    ElapsedTime6B = 0.f;
    CheckpointIndex6B = 0;
    LostControlCount6B = 0;
    OutOfControlSeconds6B = 0.f;
    bHadControl6B = false;
    ClearPracticeBarriers6O();
    if (bChargedShotLesson6M) PrepareShotAttempt6N();
    UE_LOG(LogTemp, Display, TEXT("[Training6H] Exercise started, timer reset."));
}

void ASoccerTrainingMovementGameMode6B::EnsureTrainingProfile6D()
{
    if (bTrainingProfileApplied6D || !IsValid(Trainee6B)) return;
    bTrainingProfileApplied6D = true;
    USoccerPlayerProfile* Profile = nullptr;
    if (!TrainingPlayerProfile6D.IsNull())
    {
        Profile = TrainingPlayerProfile6D.LoadSynchronous();
    }
    else if (bUseRecordedMatchProfile6G)
    {
        // Exact Data Asset path recorded in Match_20261121_101121_properties.txt.
        Profile = LoadObject<USoccerPlayerProfile>(nullptr,
            TEXT("/Game/DataAssets/Players/PlayerSquad/Player_4.Player_4"));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[Training6G] Profile parity disabled; training uses no match profile."));
        return;
    }
    if (!IsValid(Profile))
    {
        UE_LOG(LogTemp, Warning, TEXT("[Training6D] Unable to load the selected player profile."));
        return;
    }
    Trainee6B->SetPlayerProfileForMatch(Profile);
    UE_LOG(LogTemp, Display, TEXT("[Training6D] Applied match player profile: %s"),
        *Trainee6B->GetPlayerProfileId().ToString());
}

void ASoccerTrainingMovementGameMode6B::EnsureTrainingBallParity6G()
{
    if (bTrainingBallParityApplied6G || !IsValid(PracticeBall6B)) return;
    bTrainingBallParityApplied6G = true;
    PracticeBall6B->ApplyTrainingBallParity6G(
        TrainingHorizontalLaunchScale6G, TrainingAirLinearDamping6G);
}

FString ASoccerTrainingMovementGameMode6B::GetControlDiagnostics6D() const
{
    if (!IsValid(Trainee6B) || !IsValid(PracticeBall6B)) return TEXT("Control: esperando actores");
    return FString::Printf(TEXT("Posesion fisica: %s | Perfil: %s | Distancia: %.0f cm"),
        Trainee6B->IsPossessingBall() ? TEXT("SI") : TEXT("NO"),
        Trainee6B->HasPlayerProfile() ? *Trainee6B->GetPlayerProfileId().ToString() : TEXT("NINGUNO"),
        FVector::Dist2D(Trainee6B->GetActorLocation(), PracticeBall6B->GetActorLocation()));
}

FString ASoccerTrainingMovementGameMode6B::GetSpeedDiagnostics6E() const
{
    return IsValid(Trainee6B)
        ? Trainee6B->GetPhysicalDribbleDiagnostics6E()
        : TEXT("Jugador: esperando...");
}

FString ASoccerTrainingMovementGameMode6B::GetBallDiagnostics6E() const
{
    if (!IsValid(PracticeBall6B) || !IsValid(Trainee6B))
        return TEXT("Pelota: esperando...");
    const FVector BallVelocity = PracticeBall6B->GetBallPhysicsVelocity();
    const FVector PlayerVelocity = Trainee6B->GetVelocity();
    const FVector PlayerForward = PlayerVelocity.GetSafeNormal2D();
    const FVector RelativePosition = PracticeBall6B->GetActorLocation() - Trainee6B->GetActorLocation();
    const float ForwardDistance = FVector::DotProduct(RelativePosition, PlayerForward);
    const FString ControlValue = Trainee6B->HasPlayerProfile()
        ? FString::Printf(TEXT("%.0f/100"), Trainee6B->GetPlayerProfileBallControlAlpha() * 100.0f)
        : TEXT("NINGUNO");
    return FString::Printf(
        TEXT("Pelota %.0f cm/s | Separacion adelante %.0f cm | Perfil BallControl %s"),
        BallVelocity.Size2D(), ForwardDistance, *ControlValue);
}

ASoccerField* ASoccerTrainingMovementGameMode6B::FindSharedField6L() const
{
    UWorld* World = GetWorld();
    if (!World) return nullptr;
    for (TActorIterator<ASoccerField> It(World); It; ++It)
    {
        if (IsValid(*It)) return *It;
    }
    return nullptr;
}

FVector ASoccerTrainingMovementGameMode6B::GetCurrentCheckpoint6B() const
{
    if (bChargedShotLesson6M)
    {
        if (ASoccerField* Field = FindSharedField6L())
            return Field->GetGoalCenterWorldLocation(+1.f, SoccerFieldDimensions::GoalHeightCm * 0.5f);
    }
    // 6L: place checkpoints in field-local space, so translated/rotated fields
    // and future dimension changes do not require changing the exercise.
    if (ASoccerField* Field = FindSharedField6L())
    {
        const float HalfLength = SoccerFieldDimensions::HalfPitchLengthCm;
        const float HalfWidth = SoccerFieldDimensions::HalfPitchWidthCm;
        const FVector LocalCheckpoints[] = {
            FVector(-0.35f * HalfLength, -0.24f * HalfWidth, 25.f),
            FVector(-0.10f * HalfLength,  0.24f * HalfWidth, 25.f),
            FVector( 0.17f * HalfLength, -0.24f * HalfWidth, 25.f),
            FVector( 0.43f * HalfLength,  0.00f * HalfWidth, 25.f)
        };
        return Field->PitchLocalToWorld(LocalCheckpoints[FMath::Clamp(CheckpointIndex6B, 0, 3)]);
    }

    // Keep the old exercise functional while the streaming level is set up.
    static const FVector LegacyCheckpoints[] = {
        FVector(0.f, -400.f, 25.f),
        FVector(500.f, 400.f, 25.f),
        FVector(1050.f, -400.f, 25.f),
        FVector(1500.f, 0.f, 25.f)
    };
    return LegacyCheckpoints[FMath::Clamp(CheckpointIndex6B, 0, 3)];
}

FString ASoccerTrainingMovementGameMode6B::GetExerciseMessage6B() const
{
    if (bChargedShotLesson6M)
    {
        if (bComplete6B) return TEXT("Diez remates completados. Consulta el resultado.");
        if (!bExerciseStarted6H) return TEXT("Lee las instrucciones y comienza el ejercicio");
        if (bBetweenShotAttempts6N)
            return GetLastShotFeedback6P() + TEXT(". Preparando el siguiente intento...");
        if (bShotReleased6M) return TEXT("El jugador va hacia la pelota y remata. Espera el resultado...");
        if (ShotAttemptIndex6N >= 4)
            return TEXT("Supera la barrera: apunta con la cruz verde y ajusta la carga para elevar la pelota.");
        return TEXT("Apunta con la cruz verde, mantén clic derecho para cargar y suelta. No necesitas posesión.");
    }
    if (bComplete6B) return TEXT("RECORRIDO TERMINADO");
    if (!bExerciseStarted6H) return TEXT("Lee las instrucciones y pulsa Empezar ejercicio");
    if (!IsValid(PracticeBall6B) || !IsValid(Trainee6B))
        return TEXT("Esperando personaje y pelota...");
    if (bAutoPassLesson6J)
    {
        if (!Trainee6B->IsPossessingBall())
            return TEXT("Haz clic izquierdo para buscar y poseer la pelota");
        return bAutoPassArmed6J
            ? TEXT("Autopase enviado: observa si la pelota alcanza la marca")
            : TEXT("Apunta a la marca con el cursor y haz clic izquierdo");
    }
    if (FVector::Dist2D(PracticeBall6B->GetActorLocation(), Trainee6B->GetActorLocation()) > MaximumControlDistance6B)
        return TEXT("Acercate a la pelota y conducila hasta el aro amarillo");
    return TEXT("Conduce la pelota al aro amarillo sin alejarte de ella");
}

void ASoccerTrainingMovementGameMode6B::FindExerciseActors6B()
{
    UWorld* World = GetWorld();
    if (!World) return;
    if (!IsValid(PracticeBall6B))
        for (TActorIterator<ASoccerBall> It(World); It; ++It) { PracticeBall6B = *It; break; }
    if (!IsValid(Trainee6B))
        for (TActorIterator<AThirdPersonCppCharacter> It(World); It; ++It)
            if (It->IsPlayerControlled()) { Trainee6B = *It; break; }
}

void ASoccerTrainingMovementGameMode6B::DrawCheckpoint6B() const
{
    UWorld* World = GetWorld();
    if (!World) return;
    if (bChargedShotLesson6M) return; // The real goal is the shooting target.
    const FVector Center = GetCurrentCheckpoint6B();
    const FColor Yellow = FColor::Yellow;
    DrawDebugCircle(World, Center, CheckpointRadius6B, 48, Yellow, false, 0.f, 0, 5.f,
        FVector(1.f, 0.f, 0.f), FVector(0.f, 1.f, 0.f), false);
    DrawDebugLine(World, Center + FVector(0.f, 0.f, 15.f),
        Center + FVector(0.f, 0.f, 170.f), Yellow, false, 0.f, 0, 4.f);
    DrawDebugSphere(World, Center + FVector(0.f, 0.f, 170.f), 22.f, 12, Yellow, false, 0.f, 0, 3.f);
}

void ASoccerTrainingMovementGameMode6B::EnsureGround6B()
{
    UWorld* World = GetWorld();
    if (!World) return;
    for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
        if (It->ActorHasTag(TEXT("MovementTrainingFloor6B"))) return;
    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (!Cube) return;
    AStaticMeshActor* Ground = World->SpawnActor<AStaticMeshActor>(FVector(0.f,0.f,-50.f), FRotator::ZeroRotator);
    if (!Ground) return;
    Ground->Tags.Add(TEXT("MovementTrainingFloor6B"));
    UStaticMeshComponent* Mesh = Ground->GetStaticMeshComponent();
    Mesh->SetStaticMesh(Cube);
    Mesh->SetMobility(EComponentMobility::Static);
    Mesh->SetWorldScale3D(FVector(40.f,26.f,1.f));
    Mesh->SetCollisionProfileName(TEXT("BlockAll"));
}

void ASoccerTrainingMovementGameMode6B::EnsurePlayerStart6B()
{
    UWorld* World = GetWorld();
    if (!World) return;
    for (TActorIterator<APlayerStart> It(World); It; ++It) return;
    if (ASoccerField* Field = FindSharedField6L())
    {
        if (bChargedShotLesson6M)
        {
            const FVector BallLocal = GetShotBallLocal6N(0);
            World->SpawnActor<APlayerStart>(Field->PitchLocalToWorld(
                FVector(BallLocal.X - 300.f, BallLocal.Y, 130.f)), Field->GetActorRotation());
            return;
        }
        const FVector LocalStart(-0.78f * SoccerFieldDimensions::HalfPitchLengthCm, 0.f, 130.f);
        World->SpawnActor<APlayerStart>(Field->PitchLocalToWorld(LocalStart), Field->GetActorRotation());
        return;
    }
    World->SpawnActor<APlayerStart>(FVector(-850.f,0.f,130.f), FRotator::ZeroRotator);
}

void ASoccerTrainingMovementGameMode6B::EnsureBall6B()
{
    UWorld* World = GetWorld();
    if (!World) return;
    for (TActorIterator<ASoccerBall> It(World); It; ++It) { PracticeBall6B = *It; return; }
    UClass* BallClass = LoadClass<ASoccerBall>(nullptr, TEXT("/Game/Blueprints/BP_SoccerBall.BP_SoccerBall_C"));
    if (!BallClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Training6D] BP_SoccerBall not found; using ASoccerBall C++ fallback."));
        BallClass = ASoccerBall::StaticClass();
    }
    else
    {
        UE_LOG(LogTemp, Display, TEXT("[Training6D] Practice ball class: %s"), *BallClass->GetName());
    }
    FActorSpawnParameters Spawn;
    Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    FVector BallPosition(-350.f, 0.f, 70.f);
    FRotator BallRotation = FRotator::ZeroRotator;
    if (ASoccerField* Field = FindSharedField6L())
    {
        const FVector LocalBall = bChargedShotLesson6M ? GetShotBallLocal6N(0)
            : FVector(-0.63f * SoccerFieldDimensions::HalfPitchLengthCm, 0.f, 70.f);
        BallPosition = Field->PitchLocalToWorld(LocalBall);
        BallRotation = Field->GetActorRotation();
    }
    PracticeBall6B = World->SpawnActor<ASoccerBall>(BallClass,
        BallPosition, BallRotation, Spawn);
}
