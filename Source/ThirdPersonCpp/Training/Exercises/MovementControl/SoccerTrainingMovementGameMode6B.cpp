#include "Training/Exercises/MovementControl/SoccerTrainingMovementGameMode6B.h"
#include "SoccerGoalkeeperPositioningShared6AA.h"
#include "Training/Exercises/MovementControl/SoccerTrainingMovementHUD6B.h"
#include "Training/Shared/SoccerTrainingPlayerController6B.h"

#include "SoccerBall.h"
#include "SoccerAICharacter.h"
#include "SoccerAIController.h"
#include "SoccerTacklePredictionShared.h"
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
#include "Components/CapsuleComponent.h"
#include "Animation/AnimInstance.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/Controller.h"
#include "AIController.h"
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
    bZoneFinishLesson6AH = Lesson.Equals(TEXT("ZoneFinish"), ESearchCase::IgnoreCase);
    bAvoidTackleLesson6AK = Lesson.Equals(TEXT("AvoidTackle"), ESearchCase::IgnoreCase);
    bWinTackleLesson6AK = Lesson.Equals(TEXT("WinTackle"), ESearchCase::IgnoreCase);
    bGoalkeeperShotLesson6T = bZoneFinishLesson6AH || Lesson.Equals(TEXT("GoalkeeperShot"), ESearchCase::IgnoreCase);
    bChargedShotLesson6M = bGoalkeeperShotLesson6T || Lesson.Equals(TEXT("ChargedShot"), ESearchCase::IgnoreCase);
    UE_LOG(LogTemp, Display, TEXT("[Training6J] Lesson: %s"),
        bAvoidTackleLesson6AK ? TEXT("AvoidTackle") : bWinTackleLesson6AK ? TEXT("WinTackle") : bZoneFinishLesson6AH ? TEXT("ZoneFinish") : bGoalkeeperShotLesson6T ? TEXT("GoalkeeperShot") : bChargedShotLesson6M ? TEXT("ChargedShot") : bAutoPassLesson6J ? TEXT("AutoPass") : TEXT("Dribble"));
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
    if (IsTackleLesson6AK()) EnsureTackleOpponent6AK();
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
    // 6AL owns only the evasion exercise; WinTackle remains the 6AK scenario.
    if (bAvoidTackleLesson6AK)
    {
        TickAvoidTackle6AL(DeltaSeconds);
        return;
    }
    if (bWinTackleLesson6AK) return;
    // 6AJ: a zone is reached when the BALL (including its physical radius)
    // enters the target, not when the player crosses the marker.
    if (bZoneFinishLesson6AH && !bBetweenShotAttempts6N && !bShotReleased6M &&
        IsBallInsideShotZone6AH())
    {
        bReachedShotZoneThisAttempt6AJ = true;
    }
    // 6AI: only the new zone lesson permits a proactive goalkeeper rush.
    // Keep the older GoalkeeperShot lesson's exact 6AG behavior.
    bool bSweeperOwnsMovement6AI = false;
    if (bZoneFinishLesson6AH && !bShotReleased6M && !bBetweenShotAttempts6N &&
        IsValid(TrainingGoalkeeper6T))
    {
        if (ASoccerAIController* KeeperController =
            Cast<ASoccerAIController>(TrainingGoalkeeper6T->GetController()))
        {
            const int32 Result6AI = KeeperController->TickTrainingGoalkeeperSweeper6AI(
                TrainingGoalkeeper6T, PracticeBall6B, Trainee6B);
            bSweeperOwnsMovement6AI = Result6AI != 0;
            if (Result6AI == 2)
            {
                // An interception BEFORE a valid zone kick is a lost attempt,
                // never an animated save of a shot. Reuse the existing 10-attempt
                // bookkeeping without creating a new physical shot.
                bKeeperInterceptionThisAttempt6AI = true;
                bShotReleased6M = true;
                ShotCharge6M = 0.f;
                ShotEndDetail6AE = TEXT("arquero anticipo y recupero/rechazo la pelota antes del remate");
                ResolveShotAttempt6N(ETrainingShotOutcome6P::Intercepted);
                UE_LOG(LogTemp, Display, TEXT("[Training6AI] Attempt=%d INTERCEPTED before shot"),
                    ShotsCompleted6N);
                return;
            }
        }
    }
    if (bGoalkeeperShotLesson6T && !bSweeperOwnsMovement6AI)
        UpdateTrainingGoalkeeper6T(DeltaSeconds);
    if (bZoneFinishLesson6AH && !bBetweenShotAttempts6N && !bShotReleased6M)
        DrawZoneFinishTarget6AH();
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
    // 6AI.3: a charged kick may begin outside the circle while the ball is
    // moving toward it. Only the ball's position at the actual kick matters.
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
    if (BallVelocity.Size() > 140.f)
    {
        if (bZoneFinishLesson6AH && !bShotInFlight6M)
        {
            // 6AI.3: assess the last pre-flight position, rather than the
            // ball's current position AFTER the kick advanced it this frame.
            // This lets the player aim/charge while approaching the circle,
            // yet never awards a goal to a kick launched from outside it.
            const FVector ZoneLocal = GetZoneCenterLocal6AH(ShotAttemptIndex6N);
            const float EffectiveRadius = ZoneRadius6AH - PracticeBall6B->GetBallRadiusCm();
            const bool bValidKickOrigin = bHavePreviousBall6M && EffectiveRadius > 0.f &&
                FVector::DistSquared2D(PreviousBallLocal6M, ZoneLocal) <= FMath::Square(EffectiveRadius);
            // 6AI.4: legality is recorded at the kick, but NEVER interrupts
            // gameplay. The ball, goal and goalkeeper finish the attempt using
            // exactly the same physical resolution as a kick from inside.
            bZoneShotOriginChecked6AI4 = true;
            bZoneShotOriginValid6AI4 = bValidKickOrigin;
            bReachedShotZoneThisAttempt6AJ |= bValidKickOrigin;
            if (!bValidKickOrigin)
            {
                UE_LOG(LogTemp, Warning,
                    TEXT("[Training6AI4] KICK OUTSIDE ZONE: attempt=%d origin=%s center=%s radius=%.1f; allowing play to finish"),
                    ShotAttemptIndex6N + 1, *PreviousBallLocal6M.ToCompactString(),
                    *ZoneLocal.ToCompactString(), EffectiveRadius);
            }
            else
            {
                UE_LOG(LogTemp, Display,
                    TEXT("[Training6AI4] VALID ORIGIN: attempt=%d origin=%s; awaiting physical result"),
                    ShotAttemptIndex6N + 1, *PreviousBallLocal6M.ToCompactString());
            }
        }
        bShotInFlight6M = true;
    }
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

// 6AH: all coordinates are local to ASoccerField; no TargetPoints or hardcoded world positions.
FVector ASoccerTrainingMovementGameMode6B::GetZoneCenterLocal6AH(int32 AttemptIndex) const
{
    const float XFraction[10] = {0.47f, 0.57f, 0.53f, 0.43f, 0.66f,
                                0.63f, 0.72f, 0.69f, 0.75f, 0.77f};
    const float YFraction[10] = {0.f, -0.25f, 0.25f, 0.f, -0.38f,
                                0.38f, -0.22f, 0.22f, -0.46f, 0.f};
    const int32 Index = FMath::Clamp(AttemptIndex, 0, 9);
    return FVector(SoccerFieldDimensions::HalfPitchLengthCm * XFraction[Index],
        SoccerFieldDimensions::HalfPitchWidthCm * YFraction[Index], 8.f);
}

FVector ASoccerTrainingMovementGameMode6B::GetZoneCenterWorld6AH() const
{
    if (const ASoccerField* Field = FindSharedField6L())
        return Field->PitchLocalToWorld(GetZoneCenterLocal6AH(ShotAttemptIndex6N));
    return FVector::ZeroVector;
}

bool ASoccerTrainingMovementGameMode6B::IsBallInsideShotZone6AH() const
{
    if (!bZoneFinishLesson6AH || !IsValid(PracticeBall6B)) return false;
    const ASoccerField* Field = FindSharedField6L();
    if (!Field) return false;
    const FVector BallLocal = Field->WorldToPitchLocal(PracticeBall6B->GetActorLocation());
    const FVector ZoneLocal = GetZoneCenterLocal6AH(ShotAttemptIndex6N);
    const float EffectiveRadius = ZoneRadius6AH - PracticeBall6B->GetBallRadiusCm();
    return EffectiveRadius > 0.f && FVector::Dist2D(BallLocal, ZoneLocal) <= EffectiveRadius;
}

bool ASoccerTrainingMovementGameMode6B::CanStartZoneShot6AH() const
{
    return bZoneFinishLesson6AH && IsExerciseRunning6H() &&
        !bShotReleased6M && !bBetweenShotAttempts6N && IsBallInsideShotZone6AH();
}

void ASoccerTrainingMovementGameMode6B::ReportInvalidZoneShot6AH()
{
    if (!GetWorld() || GetWorld()->GetTimeSeconds() - LastZoneWarningAt6AH < 0.75f) return;
    LastZoneWarningAt6AH = GetWorld()->GetTimeSeconds();
    UE_LOG(LogTemp, Warning,
        TEXT("[Training6AH] Shot BLOCKED: ball is outside the target circle, attempt=%d"),
        ShotAttemptIndex6N + 1);
}

void ASoccerTrainingMovementGameMode6B::DrawZoneFinishTarget6AH() const
{
    if (!GetWorld() || !bZoneFinishLesson6AH) return;
    const ASoccerField* Field = FindSharedField6L();
    if (!Field) return;
    const FVector Center = Field->PitchLocalToWorld(GetZoneCenterLocal6AH(ShotAttemptIndex6N));
    const FVector XAxis = Field->GetActorRotation().RotateVector(FVector(1.f, 0.f, 0.f));
    const FVector YAxis = Field->GetActorRotation().RotateVector(FVector(0.f, 1.f, 0.f));
    const FColor Color = IsBallInsideShotZone6AH() ? FColor::Green : FColor::Yellow;
    DrawDebugCircle(GetWorld(), Center, ZoneRadius6AH, 64, Color, false, 0.f, 0, 8.f,
        XAxis, YAxis, false);
    DrawDebugLine(GetWorld(), Center, Center + FVector(0.f, 0.f, 120.f), Color,
        false, 0.f, 0, 2.5f);
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

    const FVector BallLocal = bZoneFinishLesson6AH
        ? FVector(260.f, 0.f, 70.f)
        : GetShotBallLocal6N(ShotAttemptIndex6N);
    // 6O: update the obstacle layout before the player takes the next kick.
    if (bGoalkeeperShotLesson6T) ClearPracticeBarriers6O();
    else SetupPracticeBarriers6O(BallLocal);
    const FVector PlayerLocal = bZoneFinishLesson6AH
        ? FVector(0.f, 0.f, 130.f)
        : FVector(BallLocal.X - 300.f, BallLocal.Y, 130.f);
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
    bZoneShotOriginChecked6AI4 = false;
    bZoneShotOriginValid6AI4 = false;
    if (bZoneFinishLesson6AH)
    {
        bReachedShotZoneThisAttempt6AJ = false;
        ZoneAttemptStartSeconds6AJ = GetWorld()->GetTimeSeconds();
    }
    bGoalScored6M = false;
    ShotCharge6M = 0.f;
    bBarrierContactThisShot6P = false;
    ShotEndDetail6AE.Empty();
    bKeeperInterceptionThisAttempt6AI = false;
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
    case ETrainingShotOutcome6P::Intercepted: return TEXT("INTERCEPCION DEL ARQUERO");
    case ETrainingShotOutcome6P::InvalidZone: return TEXT("NO VALIDO: FUERA DE ZONA");
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
    int32 Wide = 0, Over = 0, Blocked = 0, Saved = 0, Intercepted = 0, Unresolved = 0, BarrierTouches = 0;
    int32 InvalidZone = 0;
    int32 PostsWithoutGoal = 0, CrossbarsWithoutGoal = 0;
    int32 KeeperTouches = 0, GoalsAfterKeeperTouch = 0;
    int32 PostContacts = 0, CrossbarContacts = 0, GoalsAfterFrameContact = 0;

    for (int32 Index = 0; Index < ShotOutcomes6P.Num(); ++Index)
    {
        const ETrainingShotOutcome6P Outcome = ShotOutcomes6P[Index];
        switch (Outcome)
        {
        case ETrainingShotOutcome6P::Saved: ++Saved; break;
        case ETrainingShotOutcome6P::Intercepted: ++Intercepted; break;
        case ETrainingShotOutcome6P::Post: ++PostsWithoutGoal; break;
        case ETrainingShotOutcome6P::Crossbar: ++CrossbarsWithoutGoal; break;
        case ETrainingShotOutcome6P::Wide: ++Wide; break;
        case ETrainingShotOutcome6P::OverBar: ++Over; break;
        case ETrainingShotOutcome6P::Barrier: ++Blocked; break;
        case ETrainingShotOutcome6P::Unresolved: ++Unresolved; break;
        case ETrainingShotOutcome6P::InvalidZone: ++InvalidZone; break;
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

    // 6AJ: This exercise judges two independent facts: did the kick start in
    // the zone, and what happened physically? Never count an invalid physical
    // goal as a valid goal, nor count keeper contacts as extra attempts.
    if (bZoneFinishLesson6AH)
    {
        const int32 Total = ShotOutcomes6P.Num();
        int32 ZonesReached = 0, KicksInside = 0, KicksOutside = 0;
        int32 NoKickInterceptions = 0, ValidGoals = 0, ValidSaves = 0;
        int32 ValidWide = 0, ValidOver = 0, ValidPost = 0, ValidCrossbar = 0;
        int32 ValidUnresolved = 0, InvalidPhysicalGoals = 0;
        int32 GoalsAfterKeeper = 0, GoalsAfterFrame = 0;
        int32 KeeperContactEvents = 0, PostEvents = 0, CrossbarEvents = 0;
        float TotalAttemptSeconds = 0.f;
        for (int32 I = 0; I < Total; ++I)
        {
            const ETrainingShotOutcome6P Scored = ShotOutcomes6P[I];
            const ETrainingShotOutcome6P Physical = ShotPhysicalOutcomes6AI4.IsValidIndex(I)
                ? ShotPhysicalOutcomes6AI4[I] : Scored;
            const bool bInZone = ShotOriginInside6AJ.IsValidIndex(I) && ShotOriginInside6AJ[I];
            const bool bKick = ShotHadKick6AJ.IsValidIndex(I) && ShotHadKick6AJ[I];
            const bool bPost = ShotPostTouches6AG.IsValidIndex(I) && ShotPostTouches6AG[I];
            const bool bCrossbar = ShotCrossbarTouches6AG.IsValidIndex(I) && ShotCrossbarTouches6AG[I];
            const bool bKeeperTouch = ShotGoalkeeperTouches6AF.IsValidIndex(I) && ShotGoalkeeperTouches6AF[I];
            if (ShotZoneReached6AJ.IsValidIndex(I) && ShotZoneReached6AJ[I]) ++ZonesReached;
            if (bKick && bInZone) ++KicksInside;
            if (bKick && !bInZone) ++KicksOutside;
            if (!bKick && Physical == ETrainingShotOutcome6P::Intercepted) ++NoKickInterceptions;
            if (ShotZoneAttemptSeconds6AJ.IsValidIndex(I)) TotalAttemptSeconds += ShotZoneAttemptSeconds6AJ[I];
            if (bKeeperTouch) ++KeeperContactEvents;
            if (bPost) ++PostEvents;
            if (bCrossbar) ++CrossbarEvents;
            if (bKick && !bInZone && Physical == ETrainingShotOutcome6P::Goal) ++InvalidPhysicalGoals;
            if (!bInZone) continue;
            switch (Physical)
            {
            case ETrainingShotOutcome6P::Goal:
                ++ValidGoals;
                if (bKeeperTouch) ++GoalsAfterKeeper;
                if (bPost || bCrossbar) ++GoalsAfterFrame;
                break;
            case ETrainingShotOutcome6P::Saved: ++ValidSaves; break;
            case ETrainingShotOutcome6P::Wide: ++ValidWide; break;
            case ETrainingShotOutcome6P::OverBar: ++ValidOver; break;
            case ETrainingShotOutcome6P::Post: ++ValidPost; break;
            case ETrainingShotOutcome6P::Crossbar: ++ValidCrossbar; break;
            default: ++ValidUnresolved; break;
            }
        }
        const int32 OnTarget = ValidGoals + ValidSaves;
        const float ZonePct = Total ? 100.f * ZonesReached / Total : 0.f;
        const float GoalPct = Total ? 100.f * ValidGoals / Total : 0.f;
        const float Accuracy = KicksInside ? 100.f * OnTarget / KicksInside : 0.f;
        const float Conversion = OnTarget ? 100.f * ValidGoals / OnTarget : 0.f;
        const float AverageSeconds = Total ? TotalAttemptSeconds / Total : 0.f;
        FString ZoneReport = FString::Printf(
            TEXT("CONDUCCION Y DEFINICION POR ZONAS\n"
                 "================================\n"
                 "RESUMEN GENERAL (%d/10 intentos)\n"
                 "Zonas alcanzadas con pelota: %d/%d (%.1f%%)\n"
                 "Remates validos desde zona: %d\n"
                 "Remates desde fuera: %d\n"
                 "Intercepciones antes de patear: %d\n"
                 "Goles validos: %d/%d (%.1f%%)\n"
                 "Tiempo promedio por intento: %.1f s\n\n"
                 "DEFINICION DESDE LA ZONA\n"
                 "Goles: %d | Atajadas: %d\n"
                 "Al arco confirmados: %d/%d (%.1f%%)\n"
                 "Conversion de remates al arco: %.1f%%\n"
                 "Desviados: %d | Sobre travesano: %d\n"
                 "Poste sin gol: %d | Travesano sin gol: %d\n"
                 "Otros resultados: %d\n\n"
                 "INTERVENCIONES Y REBOTES\n"
                 "Goles validos tras toque del arquero: %d\n"
                 "Goles validos tras rebote en marco: %d\n"
                 "Intentos con contacto de arquero: %d\n"
                 "Intentos con impacto en poste: %d\n"
                 "Intentos con impacto en travesano: %d\n\n"
                 "FUERA DE ZONA\n"
                 "Goles fisicos anulados por zona: %d\n"),
            Total, ZonesReached, Total, ZonePct, KicksInside, KicksOutside,
            NoKickInterceptions, ValidGoals, Total, GoalPct, AverageSeconds,
            ValidGoals, ValidSaves, OnTarget, KicksInside, Accuracy, Conversion,
            ValidWide, ValidOver, ValidPost, ValidCrossbar, ValidUnresolved,
            GoalsAfterKeeper, GoalsAfterFrame, KeeperContactEvents, PostEvents,
            CrossbarEvents, InvalidPhysicalGoals);

        ZoneReport += TEXT("\n================================\nDETALLE DE LOS DIEZ INTENTOS\n");
        for (int32 I = 0; I < Total; ++I)
        {
            const ETrainingShotOutcome6P Scored = ShotOutcomes6P[I];
            const ETrainingShotOutcome6P Physical = ShotPhysicalOutcomes6AI4.IsValidIndex(I)
                ? ShotPhysicalOutcomes6AI4[I] : Scored;
            const bool bInZone = ShotOriginInside6AJ.IsValidIndex(I) && ShotOriginInside6AJ[I];
            const bool bKick = ShotHadKick6AJ.IsValidIndex(I) && ShotHadKick6AJ[I];
            const bool bReached = ShotZoneReached6AJ.IsValidIndex(I) && ShotZoneReached6AJ[I];
            const float Seconds = ShotZoneAttemptSeconds6AJ.IsValidIndex(I)
                ? ShotZoneAttemptSeconds6AJ[I] : 0.f;
            const float Charge = ShotCharges6P.IsValidIndex(I) ? 100.f * ShotCharges6P[I] : 0.f;
            const bool bKeeperTouch = ShotGoalkeeperTouches6AF.IsValidIndex(I) && ShotGoalkeeperTouches6AF[I];
            const bool bPost = ShotPostTouches6AG.IsValidIndex(I) && ShotPostTouches6AG[I];
            const bool bCrossbar = ShotCrossbarTouches6AG.IsValidIndex(I) && ShotCrossbarTouches6AG[I];
            const TCHAR* Origin = !bKick ? TEXT("SIN REMATE") : (bInZone ? TEXT("DENTRO") : TEXT("FUERA"));
            ZoneReport += FString::Printf(
                TEXT("\n%02d. %s | Origen: %s | Zona alcanzada: %s\n"
                     "    Tiempo: %.1f s | Potencia: %.0f%%\n"),
                I + 1, *ShotOutcomeLabel6P(Scored), Origin,
                bReached ? TEXT("SI") : TEXT("NO"), Seconds, Charge);
            if (bKick && !bInZone)
                ZoneReport += FString::Printf(TEXT("    Resultado fisico: %s (no cuenta)\n"),
                    *ShotOutcomeLabel6P(Physical));
            if (bKeeperTouch || bPost || bCrossbar)
            {
                ZoneReport += TEXT("    Contactos:");
                if (bKeeperTouch) ZoneReport += TEXT(" arquero");
                if (bPost) ZoneReport += TEXT(" poste");
                if (bCrossbar) ZoneReport += TEXT(" travesano");
                ZoneReport += TEXT("\n");
            }
            if (bInZone && Physical == ETrainingShotOutcome6P::Goal && bKeeperTouch)
                ZoneReport += TEXT("    GOL VALIDO TRAS TOQUE DEL ARQUERO\n");
            if (bInZone && Physical == ETrainingShotOutcome6P::Goal && (bPost || bCrossbar))
                ZoneReport += TEXT("    GOL VALIDO TRAS REBOTE EN MARCO\n");
            if (ShotEndDetails6AE.IsValidIndex(I))
                ZoneReport += FString::Printf(TEXT("    Motivo: %s\n"), *ShotEndDetails6AE[I]);
        }
        ZoneReport += TEXT("\nSolo los goles originados dentro del circulo suman puntos. "
                           "Los contactos y rebotes son eventos complementarios.\n");
        return ZoneReport;
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
            TEXT("%s\n"
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
                 "Barrera: %d  |  Sin resolucion: %d\n"
                 "Intercepciones antes del remate: %d\n"),
            bZoneFinishLesson6AH ? TEXT("CONDUCCION Y DEFINICION POR ZONAS") : TEXT("REMATES CONTRA ARQUERO"),
            Total, GoalsScored6N, Saved, OnTarget, Total, Accuracy,
            Conversion, SavePct, DirectGoals, GoalsAfterKeeperTouch, KeeperTouches,
            PostsWithoutGoal, CrossbarsWithoutGoal, GoalsAfterFrameContact,
            PostContacts, CrossbarContacts, Wide, Over, Blocked, Unresolved, Intercepted);

        // 6AI.4: every physical shot consumes exactly one zone attempt.
        // Invalid-origin shots are not goals even if they crossed the goal line.
        if (bZoneFinishLesson6AH)
        {
            Report += FString::Printf(
                TEXT("\nVALIDACION DE ZONA\nGoles validos: %d/10\n"
                     "Remates desde fuera del circulo: %d\n"
                     "Intentos completados: %d/10\n"),
                GoalsScored6N, InvalidZone, Total);
        }
        // A goal with a keeper touch or frame impact is included in Goals
        // exactly once. The other counters describe events, not extra shots.
        Report += TEXT("\n================================\nDETALLE DE CADA REMATE\n");
        for (int32 Index = 0; Index < Total; ++Index)
        {
            const float Charge = ShotCharges6P.IsValidIndex(Index)
                ? ShotCharges6P[Index] * 100.f : 0.f;
            Report += FString::Printf(TEXT("\n%02d. %s  |  Potencia %.0f%%\n"),
                Index + 1, *ShotOutcomeLabel6P(ShotOutcomes6P[Index]), Charge);
            if (bZoneFinishLesson6AH && ShotOutcomes6P[Index] == ETrainingShotOutcome6P::InvalidZone
                && ShotPhysicalOutcomes6AI4.IsValidIndex(Index))
            {
                Report += FString::Printf(TEXT("   Resultado en cancha: %s (no cuenta)\n"),
                    *ShotOutcomeLabel6P(ShotPhysicalOutcomes6AI4[Index]));
            }
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
    // 6AI.4: keep the true physical result for the report, but never count
    // an out-of-zone kick as a valid goal. The keeper/ball were NOT interrupted.
    const ETrainingShotOutcome6P PhysicalOutcome6AI4 = Outcome;
    if (bZoneFinishLesson6AH)
    {
        const bool bHadKick6AJ = bZoneShotOriginChecked6AI4;
        ShotHadKick6AJ.Add(bHadKick6AJ);
        ShotOriginInside6AJ.Add(bHadKick6AJ && bZoneShotOriginValid6AI4);
        ShotZoneReached6AJ.Add(bReachedShotZoneThisAttempt6AJ ||
            (bHadKick6AJ && bZoneShotOriginValid6AI4));
        ShotZoneAttemptSeconds6AJ.Add(FMath::Max(0.f,
            GetWorld()->GetTimeSeconds() - ZoneAttemptStartSeconds6AJ));
    }
    const bool bInvalidOrigin6AI4 = bZoneFinishLesson6AH &&
        bZoneShotOriginChecked6AI4 && !bZoneShotOriginValid6AI4;
    if (bZoneFinishLesson6AH)
        ShotPhysicalOutcomes6AI4.Add(PhysicalOutcome6AI4);
    if (bInvalidOrigin6AI4)
    {
        ShotEndDetail6AE = FString::Printf(
            TEXT("remate desde fuera del circulo; resultado en cancha: %s; no cuenta"),
            *ShotOutcomeLabel6P(PhysicalOutcome6AI4));
        Outcome = ETrainingShotOutcome6P::InvalidZone;
    }
    const bool bGoal = Outcome == ETrainingShotOutcome6P::Goal;
    bGoalScored6M = bGoal;
    ShotOutcomes6P.Add(Outcome);
    if (bZoneFinishLesson6AH)
    {
        UE_LOG(LogTemp, Display,
            TEXT("[Training6AI4] Attempt=%d/10 origin=%s physical=%s scored=%s; proceeding normally"),
            ShotsCompleted6N + 1, bZoneShotOriginChecked6AI4
                ? (bZoneShotOriginValid6AI4 ? TEXT("INSIDE") : TEXT("OUTSIDE"))
                : TEXT("NO KICK"),
            *ShotOutcomeLabel6P(PhysicalOutcome6AI4), *ShotOutcomeLabel6P(Outcome));
    }
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
    if (bZoneFinishLesson6AH)
    {
        const int32 Index6AJ = ShotOutcomes6P.Num() - 1;
        UE_LOG(LogTemp, Display,
            TEXT("[Training6AJ] Attempt=%d reachedZone=%s origin=%s result=%s elapsed=%.1fs"),
            Index6AJ + 1,
            ShotZoneReached6AJ.IsValidIndex(Index6AJ) && ShotZoneReached6AJ[Index6AJ] ? TEXT("YES") : TEXT("NO"),
            ShotHadKick6AJ.IsValidIndex(Index6AJ) && ShotHadKick6AJ[Index6AJ]
                ? (ShotOriginInside6AJ.IsValidIndex(Index6AJ) && ShotOriginInside6AJ[Index6AJ]
                    ? TEXT("INSIDE") : TEXT("OUTSIDE")) : TEXT("NO KICK"),
            *ShotOutcomeLabel6P(Outcome),
            ShotZoneAttemptSeconds6AJ.IsValidIndex(Index6AJ) ? ShotZoneAttemptSeconds6AJ[Index6AJ] : 0.f);
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
    ShotPhysicalOutcomes6AI4.Empty();
    ShotZoneReached6AJ.Empty();
    ShotZoneAttemptSeconds6AJ.Empty();
    ShotOriginInside6AJ.Empty();
    ShotHadKick6AJ.Empty();
    bReachedShotZoneThisAttempt6AJ = false;
    ZoneAttemptStartSeconds6AJ = 0.f;
    bZoneShotOriginChecked6AI4 = false;
    bZoneShotOriginValid6AI4 = false;
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
    if (IsTackleLesson6AK())
    {
        EnsureTackleOpponent6AK();
        PositionTackleScenario6AK();
        if (bAvoidTackleLesson6AK)
        {
            AvoidTackleAttempt6AL = 1;
            AvoidTackleSuccesses6AL = 0;
            AvoidDefenderTackles6AL2 = 0;
            AvoidDefenderJumps6AL2 = 0;
            AvoidDefenderFouls6AL2 = 0;
            AvoidDefenderRecoveries6AL2 = 0;
            AvoidDefenderOuts6AL2 = 0;
            AvoidDefenderAttemptsFinished6AL2 = 0;
            AvoidDefenderAttemptResults6AL2.Empty();
            AvoidTackleFeedback6AL = TEXT("Zona 0: busca la pelota. Las zonas 1, 2 y 3 tienen un defensor cada una.");
            ResetAvoidTackle6AL();
        }
    }
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

void ASoccerTrainingMovementGameMode6B::ResetAvoidTackle6AL()
{
    if (!bAvoidTackleLesson6AK) return;
    bAvoidTackleLaunched6AL = false;
    bAvoidTackleJumpSeen6AL = false;
    bAvoidTackleFallSeen6AL = false;
    bAvoidTackleWaitingReset6AL = false;
    AvoidTackleLaunchedAt6AL = -1.f;
    AvoidTackleResetAt6AL = -1.f;
    AvoidDefenderLastMoveAt6AL2 = -100.f;
    AvoidDefenderLastTackleAt6AL2 = -100.f;
    AvoidDefenderLastClearanceAt6AL2 = -100.f;
    bAvoidDefenderWasTackling6AL2 = false;
    bAvoidDefenderBallDislodgedThisTackle6AL3 = false;
    bAvoidDefenderClearing6AL2 = false;
    bAvoidDefenderKickImpactLogged6AL4 = false;
    AvoidDefenderKickMisses6AL5 = 0;
    AvoidDefenderLastKickApproachLog6AL51 = -100.f;
    bAvoidDefenderFoulSeen6AL2 = false;
    // Cancel every defender's pending foot impact before the ball is moved.
    // The active pointer is only an alias, so it cannot clean up all three.
    for (ASoccerAICharacter* Rival6AL6 : AvoidZoneDefenders6AL6)
    {
        if (!IsValid(Rival6AL6)) continue;
        Rival6AL6->CancelAIKickMontageBeforeImpact();
        if (Rival6AL6->IsTackleActive()) Rival6AL6->CancelTackle();
        Rival6AL6->ReleaseAIBall(false);
        if (AAIController* AI = Cast<AAIController>(Rival6AL6->GetController()))
            AI->StopMovement();
        Rival6AL6->SetAIChasingBall(false);
    }
    ActiveAvoidZone6AL6 = -1;
    HighestAvoidZone6AL6 = 0;
    TackleOpponent6AK = nullptr; // Zone 0 has no active defender.
    LastInactiveDefenderHomeAt6AL6 = -100.f;
    LastZoneLinesAt6AL6 = -100.f;
    AvoidCoverLastTarget6AL62.Init(FVector::ZeroVector, 3);
    AvoidCoverLastOrderAt6AL62.Init(-100.f, 3);
    if (IsValid(Trainee6B))
    {
        // 6AL.4: clear BOTH logical and physical human possession before
        // teleporting the ball; this must happen on every attempt reset.
        Trainee6B->ReleaseBallForAISteal();
        if (IsValid(PracticeBall6B)) PracticeBall6B->SetPossessed(false);
        UE_LOG(LogTemp, Display, TEXT("[Training6AL4] RESET POSSESSION attempt=%d human=%s logical=%s"),
            AvoidTackleAttempt6AL,
            Trainee6B->IsPossessingBall() ? TEXT("YES") : TEXT("NO"),
            Trainee6B->HasHumanLogicalBallControl() ? TEXT("YES") : TEXT("NO"));
        if (Trainee6B->IsTackleEvasionActive()) Trainee6B->CancelTackleEvasion();
        if (Trainee6B->IsTackleActive()) Trainee6B->CancelTackle();
        if (UCharacterMovementComponent* Move = Trainee6B->GetCharacterMovement())
            Move->StopMovementImmediately();
    }
    PositionTackleScenario6AK();
    UE_LOG(LogTemp, Display, TEXT("[Training6AL] READY attempt=%d"), AvoidTackleAttempt6AL);
}

FString ASoccerTrainingMovementGameMode6B::GetAvoidTackleReport6AL2() const
{
    FString Report = FString::Printf(
        TEXT("Duelos: %d / 10\nLlegadas al arco rival con pelota: %d\nTackles del defensor: %d\nSaltos registrados: %d\nFaltas del defensor: %d\nRecuperaciones + despeje: %d\nPelota fuera: %d\n\n"),
        AvoidDefenderAttemptsFinished6AL2, AvoidTackleSuccesses6AL,
        AvoidDefenderTackles6AL2, AvoidDefenderJumps6AL2,
        AvoidDefenderFouls6AL2, AvoidDefenderRecoveries6AL2, AvoidDefenderOuts6AL2);
    for (int32 i = 0; i < AvoidDefenderAttemptResults6AL2.Num(); ++i)
        Report += FString::Printf(TEXT("%02d. %s\n"), i + 1, *AvoidDefenderAttemptResults6AL2[i]);
    return Report;
}

void ASoccerTrainingMovementGameMode6B::FinishAvoidTackleAttempt6AL2(
    const FString& Outcome, bool bHumanSuccess)
{
    if (bAvoidTackleWaitingReset6AL || bComplete6B || !GetWorld()) return;
    for (ASoccerAICharacter* Rival6AL6 : AvoidZoneDefenders6AL6)
        if (IsValid(Rival6AL6))
        {
            if (AAIController* AI = Cast<AAIController>(Rival6AL6->GetController()))
                AI->StopMovement();
            Rival6AL6->SetAIChasingBall(false);
        }
    if (bHumanSuccess) ++AvoidTackleSuccesses6AL;
    AvoidDefenderJumps6AL2 += bAvoidTackleJumpSeen6AL ? 1 : 0;
    ++AvoidDefenderAttemptsFinished6AL2;
    AvoidDefenderAttemptResults6AL2.Add(FString::Printf(
        TEXT("%s | zona maxima %d"), *Outcome, HighestAvoidZone6AL6));
    AvoidTackleFeedback6AL = Outcome;
    UE_LOG(LogTemp, Display,
        TEXT("[Training6AL2] END attempt=%d outcome=%s jumped=%s fell=%s successes=%d"),
        AvoidTackleAttempt6AL, *Outcome,
        bAvoidTackleJumpSeen6AL ? TEXT("YES") : TEXT("NO"),
        bAvoidTackleFallSeen6AL ? TEXT("YES") : TEXT("NO"), AvoidTackleSuccesses6AL);
    bAvoidTackleWaitingReset6AL = true;
    AvoidTackleResetAt6AL = GetWorld()->GetTimeSeconds() + AvoidTackleResetDelay6AL;
    if (AvoidDefenderAttemptsFinished6AL2 >= 10)
    {
        bComplete6B = true;
        UE_LOG(LogTemp, Display, TEXT("[Training6AL2] FINAL\n%s"), *GetAvoidTackleReport6AL2());
        if (ASoccerTrainingPlayerController6B* PC =
            Cast<ASoccerTrainingPlayerController6B>(GetWorld()->GetFirstPlayerController()))
            PC->ShowExerciseResults6H();
    }
}

void ASoccerTrainingMovementGameMode6B::TickAvoidTackle6AL(float DeltaSeconds)
{
    (void)DeltaSeconds;
    if (!bAvoidTackleLesson6AK ||
        !IsValid(Trainee6B) || !IsValid(PracticeBall6B) || !GetWorld()) return;
    const float Now = GetWorld()->GetTimeSeconds();
    if (bAvoidTackleWaitingReset6AL)
    {
        if (Now >= AvoidTackleResetAt6AL && !bComplete6B)
        {
            ++AvoidTackleAttempt6AL;
            ResetAvoidTackle6AL();
        }
        return;
    }
    ASoccerField* Field = FindSharedField6L();
    if (!Field) return;

    // 6AL.6: use the BALL (not the player) for zone ownership. Zone 0
    // is a free start; each later zone has exactly one active pursuer.
    // Do not cancel a foot montage already launched for a legal clearance
    // when the kicked ball momentarily crosses a zone boundary.
    DrawAvoidZones6AL6(Now);
    if (!bAvoidDefenderClearing6AL2)
        SetActiveAvoidZone6AL6(GetBallZone6AL6(PracticeBall6B->GetActorLocation()));
    UpdateInactiveAvoidDefenders6AL6(Now);

    if (!IsValid(TackleOpponent6AK))
    {
        bAvoidTackleJumpSeen6AL |= Trainee6B->IsTackleEvasionActive();
        bAvoidTackleFallSeen6AL |= Trainee6B->IsTackleFallReactionActive();
        const FVector LocalBall6AL6 = Field->WorldToPitchLocal(PracticeBall6B->GetActorLocation());
        if (FMath::Abs(LocalBall6AL6.X) > SoccerFieldDimensions::HalfPitchLengthCm + 10.f ||
            FMath::Abs(LocalBall6AL6.Y) > SoccerFieldDimensions::HalfPitchWidthCm + 10.f)
        {
            ++AvoidDefenderOuts6AL2;
            FinishAvoidTackleAttempt6AL2(TEXT("PELOTA FUERA DEL CAMPO"), false);
        }
        return; // No opponent may press or tackle while the ball is in zone 0.
    }

    const FVector Human = Trainee6B->GetActorLocation();
    const FVector Defender = TackleOpponent6AK->GetActorLocation();
    const FVector Ball = PracticeBall6B->GetActorLocation();
    const FVector LocalBall = Field->WorldToPitchLocal(Ball);
    const FVector LocalHuman = Field->WorldToPitchLocal(Human);
    const float DefenderBallDistance = FVector::Dist2D(Defender, Ball);
    const float HumanBallDistance = FVector::Dist2D(Human, Ball);
    bool bHumanControl = Trainee6B->IsPossessingBall();
    // 6AL.5.2: the match represents field possession logically. If the rival
    // already owns this ball, the human cannot keep a simultaneous claim.
    // ReleaseBallForAISteal returns the human to Manual: a NEW left-click is
    // required to chase/claim the ball again.
    if (TackleOpponent6AK->IsAIPossessingBall() &&
        (bHumanControl || Trainee6B->HasHumanLogicalBallControl()))
    {
        Trainee6B->ReleaseBallForAISteal();
        bHumanControl = false;
        UE_LOG(LogTemp, Display,
            TEXT("[Training6AL52] HUMAN CLAIM RELEASED: rival already possesses ball attempt=%d"),
            AvoidTackleAttempt6AL);
    }
    bAvoidTackleJumpSeen6AL |= Trainee6B->IsTackleEvasionActive();
    bAvoidTackleFallSeen6AL |= Trainee6B->IsTackleFallReactionActive();

    // 6AL.6.3: the objective is INSIDE the rival (right) goal, not the
    // penalty area. The whole ball and human capsule must cross the goal
    // line inside the posts, while the human still owns the ball.
    // The goal mouth is a special case of "outside pitch": do not finish
    // as PELOTA FUERA as soon as the ball crosses the line. Allow the human
    // to follow it into the net. The net depth and posts are from the field.
    const float GoalLineX = SoccerFieldDimensions::RightGoalLineX;
    const float GoalDepth = SoccerFieldDimensions::GoalDepthCm;
    const float BallRadius = PracticeBall6B->GetBallRadiusCm();
    const UCapsuleComponent* HumanCapsule = Trainee6B->GetCapsuleComponent();
    const float HumanRadius = HumanCapsule ? HumanCapsule->GetScaledCapsuleRadius() : 42.f;
    const bool bBallInRivalGoalMouth =
        LocalBall.X >= GoalLineX &&
        LocalBall.X <= GoalLineX + GoalDepth - BallRadius &&
        FMath::Abs(LocalBall.Y) <= SoccerFieldDimensions::GoalHalfWidthCm - BallRadius &&
        LocalBall.Z >= 0.f &&
        LocalBall.Z <= SoccerFieldDimensions::GoalHeightCm - BallRadius;
    const bool bBallFullyAcrossGoalLine =
        bBallInRivalGoalMouth && LocalBall.X >= GoalLineX + BallRadius;
    const bool bHumanInsideRivalGoal =
        LocalHuman.X >= GoalLineX + HumanRadius &&
        LocalHuman.X <= GoalLineX + GoalDepth - HumanRadius &&
        FMath::Abs(LocalHuman.Y) <= SoccerFieldDimensions::GoalHalfWidthCm - HumanRadius;
    const bool bHumanStillControlsBall =
        bHumanControl || Trainee6B->HasHumanLogicalBallControl();
    if (!bAvoidDefenderClearing6AL2 && bBallFullyAcrossGoalLine &&
        bHumanInsideRivalGoal && bHumanStillControlsBall &&
        !TackleOpponent6AK->IsAIPossessingBall())
    {
        UE_LOG(LogTemp, Display,
            TEXT("[Training6AL63] GOAL ARRIVAL attempt=%d humanX=%.1f ballX=%.1f ballY=%.1f"),
            AvoidTackleAttempt6AL, LocalHuman.X, LocalBall.X, LocalBall.Y);
        FinishAvoidTackleAttempt6AL2(TEXT("EXITO: HUMANO Y PELOTA DENTRO DEL ARCO RIVAL"), true);
        return;
    }

    const bool bBallOut = FMath::Abs(LocalBall.X) > SoccerFieldDimensions::HalfPitchLengthCm + 10.f ||
        FMath::Abs(LocalBall.Y) > SoccerFieldDimensions::HalfPitchWidthCm + 10.f;
    // Inside the rival goal the ball is not a conventional out-of-bounds
    // until the human can also cross the line. A defender clearance still
    // resolves normally when it goes outside the playing field.
    if (bBallOut && (!bBallInRivalGoalMouth || bAvoidDefenderClearing6AL2))
    {
        if (bAvoidDefenderClearing6AL2)
        {
            ++AvoidDefenderRecoveries6AL2;
            FinishAvoidTackleAttempt6AL2(TEXT("DEFENSOR RECUPERO Y DESPEJO FUERA"), false);
        }
        else
        {
            ++AvoidDefenderOuts6AL2;
            FinishAvoidTackleAttempt6AL2(TEXT("PELOTA FUERA DEL CAMPO"), false);
        }
        return;
    }

    // Once the defender has recovered possession, immediately clear sideways.
    // Do not mark the attempt finished until the ball REALLY leaves the pitch.
    if (bAvoidDefenderClearing6AL2)
    {
        // 6AL.4: the montage owns the foot impact. NEVER kick a distant
        // or stopped ball from GameMode as a "retry".
        if (TackleOpponent6AK->HasAIKickMontageImpactedBall() &&
            !bAvoidDefenderKickImpactLogged6AL4)
        {
            bAvoidDefenderKickImpactLogged6AL4 = true;
            UE_LOG(LogTemp, Display,
                TEXT("[Training6AL5] FOOT IMPACT attempt=%d defenderDist=%.1f speed=%.1f"),
                AvoidTackleAttempt6AL,
                FVector::Dist2D(TackleOpponent6AK->GetActorLocation(), PracticeBall6B->GetActorLocation()),
                PracticeBall6B->GetBallPhysicsVelocity().Size2D());
        }
        if (!TackleOpponent6AK->IsAIKickMontageActive() &&
            Now - AvoidDefenderLastClearanceAt6AL2 > 1.0f)
        {
            const bool bActualImpact = bAvoidDefenderKickImpactLogged6AL4 ||
                TackleOpponent6AK->HasAIKickMontageImpactedBall();
            const float Speed = PracticeBall6B->GetBallPhysicsVelocity().Size2D();
            // Open-play kicks can miss if the moving ball escapes the foot.
            // Never credit a miss as a clearance or apply a remote impulse.
            if (!bActualImpact || Speed < 110.f)
            {
                if (!bActualImpact)
                {
                    ++AvoidDefenderKickMisses6AL5;
                    UE_LOG(LogTemp, Display,
                        TEXT("[Training6AL5] FOOT MISS attempt=%d defenderDist=%.1f ballSpeed=%.1f"),
                        AvoidTackleAttempt6AL,
                        FVector::Dist2D(TackleOpponent6AK->GetActorLocation(), PracticeBall6B->GetActorLocation()),
                        Speed);
                }
                else
                {
                    UE_LOG(LogTemp, Display,
                        TEXT("[Training6AL5] CLEARANCE SHORT attempt=%d; return to ball"),
                        AvoidTackleAttempt6AL);
                }
                bAvoidDefenderClearing6AL2 = false;
                bAvoidDefenderKickImpactLogged6AL4 = false;
                TackleOpponent6AK->SetAIChasingBall(true);
            }
        }
        return;
    }

    const bool bTackling = TackleOpponent6AK->IsTackleActive();
    // 6AL.3: a *real, applied* foot-to-ball tackle touch must release
    // the human's logical possession. Otherwise the ball may move physically
    // while the human keeps controlling it and the defender can never steal it.
    // Read the active contact summary; never dislodge on proximity or animation.
    if (bTackling && !bAvoidDefenderBallDislodgedThisTackle6AL3)
    {
        const FSoccerTackleContactSummary ActiveContact =
            TackleOpponent6AK->GetTackleContactSummary();
        if (ActiveContact.bBallContactOccurred && ActiveContact.bBallContactApplied)
        {
            bAvoidDefenderBallDislodgedThisTackle6AL3 = true;
            if (bHumanControl || Trainee6B->HasHumanLogicalBallControl())
            {
                Trainee6B->ReleaseBallForAISteal();
                bHumanControl = false;
                UE_LOG(LogTemp, Display,
                    TEXT("[Training6AL3] BALL DISLODGED attempt=%d defenderDist=%.1f ballSpeed=%.1f"),
                    AvoidTackleAttempt6AL, DefenderBallDistance,
                    PracticeBall6B->GetBallPhysicsVelocity().Size2D());
            }
            else
            {
                UE_LOG(LogTemp, Display,
                    TEXT("[Training6AL3] BALL CONTACT attempt=%d humanAlreadyLoose=YES"),
                    AvoidTackleAttempt6AL);
            }
        }
    }
    if (bAvoidDefenderWasTackling6AL2 && !bTackling)
    {
        const FSoccerTackleContactSummary Contact = TackleOpponent6AK->GetTackleContactSummary();
        // Match-derived foul conditions, restricted to the *training* GameMode.
        // No SoccerMatchManager exists here to adjudicate stoppages.
        if (Contact.bOpponentContactOccurred)
        {
            const bool bValidBall = Contact.bBallContactOccurred && Contact.bBallContactApplied;
            const float Lead = Contact.OpponentContactNormalizedTime - Contact.BallContactNormalizedTime;
            const bool bBallFirst = bValidBall && Lead > 0.035f;
            const bool bSimultaneous = bValidBall && FMath::Abs(Lead) <= 0.035f;
            const bool bCleanBallFirst = bBallFirst &&
                !Contact.bOpponentContactFromBehind && Contact.OpponentContactHeightCm < 30.f &&
                Contact.OpponentContactRelativeSpeedCmPerSec <= 720.f;
            // Mirrors the match's default order/speed/height foul tests; it
            // deliberately does not instantiate or tick SoccerMatchManager.
            const bool bFoul = !bValidBall ||
                (bSimultaneous && (Contact.bOpponentContactFromBehind ||
                    Contact.OpponentContactRelativeSpeedCmPerSec >= 420.f ||
                    Contact.OpponentContactHeightCm >= 30.f)) ||
                (bBallFirst && !bCleanBallFirst) ||
                (!bBallFirst && !bSimultaneous);
            UE_LOG(LogTemp, Display,
                TEXT("[Training6AL2] TACKLE CONTACT ball=%s applied=%s opponent=%s cleanFirst=%s speed=%.1f height=%.1f foul=%s"),
                Contact.bBallContactOccurred ? TEXT("YES") : TEXT("NO"),
                Contact.bBallContactApplied ? TEXT("YES") : TEXT("NO"),
                Contact.bOpponentContactOccurred ? TEXT("YES") : TEXT("NO"),
                bCleanBallFirst ? TEXT("YES") : TEXT("NO"),
                Contact.OpponentContactRelativeSpeedCmPerSec,
                Contact.OpponentContactHeightCm, bFoul ? TEXT("YES") : TEXT("NO"));
            if (bFoul)
            {
                ++AvoidDefenderFouls6AL2;
                FinishAvoidTackleAttempt6AL2(TEXT("FALTA DEL DEFENSOR"), false);
                return;
            }
        }
        // If the tackle ended between Tick samples, the saved contact summary
        // still tells us whether a real ball response was applied.
        if (Contact.bBallContactApplied && !bAvoidDefenderBallDislodgedThisTackle6AL3 &&
            (bHumanControl || Trainee6B->HasHumanLogicalBallControl()))
        {
            Trainee6B->ReleaseBallForAISteal();
            bHumanControl = false;
            bAvoidDefenderBallDislodgedThisTackle6AL3 = true;
            UE_LOG(LogTemp, Display,
                TEXT("[Training6AL3] BALL DISLODGED AT END attempt=%d"), AvoidTackleAttempt6AL);
        }
        bAvoidDefenderWasTackling6AL2 = false;
    }

    // 6AL.5.1: keep the preferred approach from behind, but do not require
    // a nearly perfect angle for a stationary ball. The real open-play
    // montage already validates physical contact when the foot strikes.
    // Previously the defender could arrive beside the ball and wait forever:
    // MoveToLocation(standing point) + capsule avoidance prevented 6AL.5's
    // front/lateral tests from ever becoming true.
    const float Side = LocalBall.Y >= 0.f ? 1.f : -1.f;
    const FVector OutLocal(
        FMath::Clamp(LocalBall.X,
            -SoccerFieldDimensions::HalfPitchLengthCm + 150.f,
             SoccerFieldDimensions::HalfPitchLengthCm - 150.f),
        Side * (SoccerFieldDimensions::HalfPitchWidthCm + 2200.f), LocalBall.Z);
    const FVector TargetWorld = Field->PitchLocalToWorld(OutLocal);
    const FVector KickDirection = (TargetWorld - Ball).GetSafeNormal2D();
    const FVector DesiredStandingPoint = Ball - KickDirection * 66.f;
    const FVector RelativeBall = Ball - Defender;
    const float FrontDot = FVector::DotProduct(RelativeBall.GetSafeNormal2D(), KickDirection);
    const float SideOffset = FMath::Abs(FVector::CrossProduct(KickDirection, RelativeBall).Z);
    const float BallSpeedForClearance = PracticeBall6B->GetBallPhysicsVelocity().Size2D();
    // 6AL.5.2: a clean tackle touch is handled above using the real contact
    // summary. Also handle the defender actually reaching a human-controlled
    // ball with his feet without an active slide (normal match-style steal).
    // A small, ground-level contact window prevents remote steals merely
    // because the defender is within its much larger chase radius.
    const bool bHumanClaimAtFoot = bHumanControl || Trainee6B->HasHumanLogicalBallControl();
    const bool bDefenderFootReach = DefenderBallDistance <= 55.f &&
        FMath::Abs(LocalBall.Z) <= 60.f && !bTackling &&
        !TackleOpponent6AK->IsAIKickMontageActive();
    if (bHumanClaimAtFoot && bDefenderFootReach)
    {
        Trainee6B->ReleaseBallForAISteal();
        bHumanControl = false;
        UE_LOG(LogTemp, Display,
            TEXT("[Training6AL52] FOOT STEAL attempt=%d dist=%.1f human=%s logical=%s; new left-click required"),
            AvoidTackleAttempt6AL, DefenderBallDistance,
            Trainee6B->IsPossessingBall() ? TEXT("YES") : TEXT("NO"),
            Trainee6B->HasHumanLogicalBallControl() ? TEXT("YES") : TEXT("NO"));
    }
    const bool bLooseBall = !bHumanControl && !Trainee6B->HasHumanLogicalBallControl();
    // 85 cm is BELOW the normal 92 cm physical impact range, leaving margin
    // for motion during montage playback. Do not demand perfect alignment:
    // the montage's existing contact check decides whether the kick lands.
    const bool bBallReadyForKick = bLooseBall && !bTackling &&
        !TackleOpponent6AK->IsAIKickMontageActive() &&
        DefenderBallDistance <= 85.f && BallSpeedForClearance < 450.f &&
        Now - AvoidDefenderLastClearanceAt6AL2 > 0.75f;
    if (bLooseBall && !bTackling && DefenderBallDistance < 220.f &&
        Now - AvoidDefenderLastKickApproachLog6AL51 >= 1.5f)
    {
        AvoidDefenderLastKickApproachLog6AL51 = Now;
        UE_LOG(LogTemp, Display,
            TEXT("[Training6AL51] APPROACH attempt=%d dist=%.1f speed=%.1f front=%.2f lateral=%.1f ready=%s"),
            AvoidTackleAttempt6AL, DefenderBallDistance, BallSpeedForClearance,
            FrontDot, SideOffset, bBallReadyForKick ? TEXT("YES") : TEXT("NO"));
    }

    if (bBallReadyForKick)
    {
        if (AAIController* AI = Cast<AAIController>(TackleOpponent6AK->GetController()))
            AI->StopMovement();
        TackleOpponent6AK->SetAIChasingBall(false);
        TackleOpponent6AK->SetActorRotation(KickDirection.Rotation());
        // GameMode does not simulate the strike: possession and montage are
        // owned by ASoccerAICharacter, exactly as in normal play.
        // Final ownership handoff: never start a rival kick with a lingering
        // human claim, even if it was reacquired between Tick samples.
        if (Trainee6B->IsPossessingBall() || Trainee6B->HasHumanLogicalBallControl())
        {
            Trainee6B->ReleaseBallForAISteal();
            UE_LOG(LogTemp, Display,
                TEXT("[Training6AL52] HUMAN CLAIM RELEASED AT KICK attempt=%d"),
                AvoidTackleAttempt6AL);
        }
        if (!TackleOpponent6AK->IsAIPossessingBall())
            TackleOpponent6AK->PossessAIBall(PracticeBall6B);
        // The target is now >20m outside the touchline. The clearance needs
        // sufficient horizontal velocity, independent of older BP defaults.
        const float ClearanceSpeed6AL52 = FMath::Max(1850.f, AvoidDefenderClearanceSpeed6AL2);
        const bool bStarted = TackleOpponent6AK->TryStartAIKickMontageForOpenPlay(
            PracticeBall6B, TargetWorld,
            ClearanceSpeed6AL52, 0.3f, 3.f, false);
        if (bStarted)
        {
            bAvoidDefenderClearing6AL2 = true;
            bAvoidDefenderKickImpactLogged6AL4 = false;
            AvoidDefenderLastClearanceAt6AL2 = Now;
            AvoidTackleFeedback6AL = TEXT("El defensor despeja hacia la banda...");
            UE_LOG(LogTemp, Display,
                TEXT("[Training6AL52] OPEN PLAY KICK START attempt=%d dist=%.1f targetOutside=%.1f kickSpeed=%.1f humanClaim=%s"),
                AvoidTackleAttempt6AL, DefenderBallDistance,
                FMath::Abs(OutLocal.Y) - SoccerFieldDimensions::HalfPitchWidthCm,
                ClearanceSpeed6AL52,
                Trainee6B->HasHumanLogicalBallControl() ? TEXT("YES") : TEXT("NO"));
        }
        else
        {
            // Failure cannot turn into an instantaneous fake kick.
            TackleOpponent6AK->ReleaseAIBall(false);
            // Prevent one warning every Tick if the character is still recovering
            // from a tackle or temporarily cannot play its kick montage.
            AvoidDefenderLastClearanceAt6AL2 = Now;
            UE_LOG(LogTemp, Warning,
                TEXT("[Training6AL51] OPEN PLAY MONTAGE FAILED attempt=%d; check kick montages / animation state"),
                AvoidTackleAttempt6AL);
        }
        return;
    }
    // A loose ball near the defender is NOT sufficient for kicking: approach
    // from the inside of the field, with the target in front of the player's foot.
    // Do not re-tackle an unpossessed ball while arranging a clearance.
    if (!bHumanControl && !Trainee6B->HasHumanLogicalBallControl())
    {
        if (TackleOpponent6AK->IsAIPossessingBall())
            TackleOpponent6AK->ReleaseAIBall(false);
        if (Now - AvoidDefenderLastMoveAt6AL2 >= AvoidDefenderRepathSeconds6AL2)
        {
            if (AAIController* AI = Cast<AAIController>(TackleOpponent6AK->GetController()))
            {
                // Far away: approach from behind the ball. Close up: move
                // toward the ball itself so capsule/path acceptance cannot
                // park the defender 90-140 cm away from an unreachable offset.
                const FVector ApproachTarget = DefenderBallDistance > 170.f
                    ? DesiredStandingPoint : Ball;
                AI->MoveToLocation(ApproachTarget, 12.f, true, true, true, true, nullptr, true);
                TackleOpponent6AK->SetAIChasingBall(true);
                TackleOpponent6AK->RequestAIMovementMode(
                    ESoccerAIMovementMode::Run,
                    ESoccerAIMovementReason::PressBallCarrier, true);
                AvoidDefenderLastMoveAt6AL2 = Now;
            }
        }
        return;
    }

    // Training-driven NavMesh orders: regular match tactical Tick remains OFF.
    if (!bTackling)
    {
        if (Now - AvoidDefenderLastMoveAt6AL2 >= AvoidDefenderRepathSeconds6AL2)
        {
            if (AAIController* AI = Cast<AAIController>(TackleOpponent6AK->GetController()))
            {
                const FVector Prediction = Ball + PracticeBall6B->GetBallPhysicsVelocity() * 0.18f;
                // The previous 110cm acceptance radius could stop the bot
                // outside actual ball-collection range, particularly after a tackle.
                AI->MoveToLocation(Prediction, 40.f, true, true, true, true, nullptr, true);
                TackleOpponent6AK->SetAIChasingBall(true);
                TackleOpponent6AK->RequestAIMovementMode(
                    ESoccerAIMovementMode::FastRun,
                    ESoccerAIMovementReason::PressBallCarrier, true);
                AvoidDefenderLastMoveAt6AL2 = Now;
            }
        }
        // Tackle only in a plausible race to win the ball, not towards human body.
        if (DefenderBallDistance <= AvoidDefenderTackleBallDistance6AL2 &&
            FVector::Dist2D(Defender, Human) < 570.f &&
            HumanBallDistance < 225.f &&
            Now - AvoidDefenderLastTackleAt6AL2 >= AvoidDefenderTackleCooldown6AL2 &&
            !Trainee6B->IsTackleFallReactionActive())
        {
            // 6AL.6.4: the previous tackle always aimed at Ball, its position
            // NOW, even if the human was sprinting. During a 0.3-0.6s slide
            // the ball can travel several metres and the tackle arrives late.
            // Reuse the match's CONTESTED tackle predictor for a carried ball,
            // but estimate lead time from the current defender-to-interception
            // distance rather than blindly using a fixed value.
            const FVector CarrierVelocity = Trainee6B->GetVelocity();
            const float EstimatedSlideSpeed = FMath::Max(
                350.f, AvoidTackleEstimatedSlideSpeed6AL64);
            const float LaunchDelay = FMath::Max(0.f, AvoidTackleLaunchDelay6AL64);
            const float MinLead = FMath::Max(0.02f, AvoidTackleMinLeadSeconds6AL64);
            const float MaxLead = FMath::Max(MinLead, AvoidTackleMaxLeadSeconds6AL64);
            float LeadSeconds = FMath::Clamp(
                SoccerTacklePredictionShared::EstimateSlideArrivalSeconds(
                    Defender, Ball, EstimatedSlideSpeed, LaunchDelay),
                MinLead, MaxLead);

            // Two estimates stabilize the lead for a ball moving toward or
            // away from the tackler. No teleportation of the ball or player.
            FVector PredictedBall = Ball;
            for (int32 EstimateIndex = 0; EstimateIndex < 2; ++EstimateIndex)
            {
                PredictedBall = SoccerTacklePredictionShared::PredictCarriedBallLocation(
                    Ball, CarrierVelocity, LeadSeconds);
                const float Arrival =
                    SoccerTacklePredictionShared::EstimateSlideArrivalSeconds(
                        Defender, PredictedBall, EstimatedSlideSpeed, LaunchDelay);
                LeadSeconds = FMath::Clamp(
                    FMath::Lerp(LeadSeconds, Arrival, 0.65f), MinLead, MaxLead);
            }
            PredictedBall = SoccerTacklePredictionShared::PredictCarriedBallLocation(
                Ball, CarrierVelocity, LeadSeconds);

            // An unreasonable prediction should not launch a blind tackle.
            // Keep pursuing and reassess on a later Tick instead.
            const float AimDistance = FVector::Dist2D(Defender, PredictedBall);
            const float LeadDistance = FVector::Dist2D(Ball, PredictedBall);
            const float MaximumViableSlideDistance =
                EstimatedSlideSpeed * MaxLead + 90.f;
            if (AimDistance <= MaximumViableSlideDistance)
            {
                if (AAIController* AI = Cast<AAIController>(TackleOpponent6AK->GetController()))
                    AI->StopMovement();
                TackleOpponent6AK->SetActorRotation((PredictedBall - Defender).Rotation());
                if (TackleOpponent6AK->TryStartTackleTowardLocation(PredictedBall))
                {
                    bAvoidDefenderBallDislodgedThisTackle6AL3 = false;
                    AvoidDefenderLastTackleAt6AL2 = Now;
                    bAvoidDefenderWasTackling6AL2 = true;
                    ++AvoidDefenderTackles6AL2;
                    AvoidTackleFeedback6AL = TEXT("El defensor anticipo la trayectoria de la pelota. Esquivalo y entra al arco rival.");
                    UE_LOG(LogTemp, Display,
                        TEXT("[Training6AL64] PREDICTIVE TACKLE START attempt=%d ballDist=%.1f humanDist=%.1f carrierSpeed=%.1f lead=%.2fs leadDist=%.1f aimDist=%.1f"),
                        AvoidTackleAttempt6AL, DefenderBallDistance,
                        FVector::Dist2D(Defender, Human), CarrierVelocity.Size2D(),
                        LeadSeconds, LeadDistance, AimDistance);
                }
            }
        }
    }
    else
        bAvoidDefenderWasTackling6AL2 = true;
}

// 6AL.6: coordinate frame always follows ASoccerField/SoccerFieldDimensions.
// Zones increase from the player's own (left) goal towards the right goal.
FVector ASoccerTrainingMovementGameMode6B::GetAvoidDefenderHomeLocal6AL6(int32 Zone) const
{
    const float HalfLength = SoccerFieldDimensions::HalfPitchLengthCm;
    const float HalfWidth = SoccerFieldDimensions::HalfPitchWidthCm;
    const int32 SafeZone = FMath::Clamp(Zone, 1, 3);
    const float ZoneLength = HalfLength * 0.5f;
    return FVector(-HalfLength + (SafeZone + 0.5f) * ZoneLength,
        (SafeZone == 2 ? -0.58f : 0.58f) * HalfWidth, 130.f);
}

// 6AL.6.2: a defender not currently chasing still covers the corridor
// from the ball towards the rival (right) goal. Work exclusively in the
// field-local pitch frame so rotations/stadium dimensions remain supported.
FVector ASoccerTrainingMovementGameMode6B::GetAvoidDefenderCoverLocal6AL62(
    int32 Zone, const FVector& BallWorld) const
{
    const FVector HomeLocal = GetAvoidDefenderHomeLocal6AL6(Zone);
    const ASoccerField* Field = FindSharedField6L();
    if (!Field) return HomeLocal;

    const FVector BallLocal = Field->WorldToPitchLocal(BallWorld);
    const float GoalX = SoccerFieldDimensions::RightGoalLineX;
    // Extend the ball-goal line a little backwards for zones already passed.
    // Limit that extension so a ball near the rival goal cannot drag the
    // waiting defenders outside their own zones or off the touchline.
    const float GoalDistanceX = FMath::Max(200.f, GoalX - BallLocal.X);
    const float AlongLine = FMath::Clamp(
        (HomeLocal.X - BallLocal.X) / GoalDistanceX, -0.45f, 1.f);
    const float LineY = FMath::Lerp(BallLocal.Y, 0.f, AlongLine);

    // Small, fixed asymmetry makes the three defenders look less robotic.
    // The margin is never a reason to leave their assigned playing zone.
    const float Side = (Zone == 2) ? -1.f : 1.f;
    const float SideInset = FMath::Min(230.f, SoccerFieldDimensions::HalfPitchWidthCm * 0.12f);
    const float SafeHalfWidth = SoccerFieldDimensions::HalfPitchWidthCm - SideInset;
    const float DesiredY = FMath::Clamp(
        LineY + Side * AvoidCoverLateralMargin6AL62,
        -SafeHalfWidth, SafeHalfWidth);
    return FVector(HomeLocal.X, DesiredY, HomeLocal.Z);
}

int32 ASoccerTrainingMovementGameMode6B::GetBallZone6AL6(const FVector& BallWorld) const
{
    const ASoccerField* Field = FindSharedField6L();
    if (!Field) return 0;
    const float HalfLength = SoccerFieldDimensions::HalfPitchLengthCm;
    const float LocalX = Field->WorldToPitchLocal(BallWorld).X;
    return FMath::Clamp(FMath::FloorToInt((LocalX + HalfLength) / (0.5f * HalfLength)), 0, 3);
}

void ASoccerTrainingMovementGameMode6B::SetActiveAvoidZone6AL6(int32 Zone)
{
    if (!bAvoidTackleLesson6AK) return;
    const int32 NextZone = FMath::Clamp(Zone, 0, 3);
    if (NextZone == ActiveAvoidZone6AL6) return;

    // The previous defender disengages completely: no residual tackle,
    // possession claim or pending foot impact can affect the new zone.
    if (IsValid(TackleOpponent6AK))
    {
        TackleOpponent6AK->CancelAIKickMontageBeforeImpact();
        if (TackleOpponent6AK->IsTackleActive()) TackleOpponent6AK->CancelTackle();
        TackleOpponent6AK->ReleaseAIBall(false);
        TackleOpponent6AK->SetAIChasingBall(false);
        if (AAIController* AI = Cast<AAIController>(TackleOpponent6AK->GetController()))
            AI->StopMovement();
    }

    // The former active defender must immediately receive a new cover
    // destination on the next update, even if the ball scarcely moved.
    if (ActiveAvoidZone6AL6 > 0 && AvoidCoverLastTarget6AL62.IsValidIndex(ActiveAvoidZone6AL6 - 1))
    {
        AvoidCoverLastTarget6AL62[ActiveAvoidZone6AL6 - 1] = FVector::ZeroVector;
        AvoidCoverLastOrderAt6AL62[ActiveAvoidZone6AL6 - 1] = -100.f;
    }
    ActiveAvoidZone6AL6 = NextZone;
    HighestAvoidZone6AL6 = FMath::Max(HighestAvoidZone6AL6, NextZone);
    TackleOpponent6AK = (NextZone > 0 && AvoidZoneDefenders6AL6.IsValidIndex(NextZone - 1))
        ? AvoidZoneDefenders6AL6[NextZone - 1] : nullptr;

    // Existing 6AL2-6AL5 state describes the CURRENT active defender only.
    AvoidDefenderLastMoveAt6AL2 = -100.f;
    AvoidDefenderLastTackleAt6AL2 = -100.f;
    AvoidDefenderLastClearanceAt6AL2 = -100.f;
    bAvoidDefenderWasTackling6AL2 = false;
    bAvoidDefenderBallDislodgedThisTackle6AL3 = false;
    bAvoidDefenderClearing6AL2 = false;
    bAvoidDefenderKickImpactLogged6AL4 = false;
    AvoidDefenderKickMisses6AL5 = 0;
    AvoidDefenderLastKickApproachLog6AL51 = -100.f;
    LastInactiveDefenderHomeAt6AL6 = -100.f; // home orders immediately

    if (IsValid(TackleOpponent6AK))
    {
        if (AAIController* AI = Cast<AAIController>(TackleOpponent6AK->GetController()))
            AI->StopMovement(); // cancel an unfinished return-home request
        TackleOpponent6AK->SetAIChasingBall(false);
    }
    AvoidTackleFeedback6AL = NextZone == 0
        ? TEXT("Zona 0: busca la pelota y avanza; no hay defensor.")
        : FString::Printf(TEXT("Zona %d: defensor %d activo. Esquiva el tackle y conserva la pelota."),
            NextZone, NextZone);
    UE_LOG(LogTemp, Display,
        TEXT("[Training6AL6] ZONE CHANGE attempt=%d zone=%d active=%s ballDriven=YES"),
        AvoidTackleAttempt6AL, NextZone, *GetNameSafe(TackleOpponent6AK));
}

void ASoccerTrainingMovementGameMode6B::UpdateInactiveAvoidDefenders6AL6(float Now)
{
    if (!bAvoidTackleLesson6AK || !IsValid(PracticeBall6B) ||
        Now - LastInactiveDefenderHomeAt6AL6 < AvoidCoverUpdateSeconds6AL62)
    {
        return;
    }
    const ASoccerField* Field = FindSharedField6L();
    if (!Field) return;
    LastInactiveDefenderHomeAt6AL6 = Now;
    if (AvoidCoverLastTarget6AL62.Num() != 3)
    {
        AvoidCoverLastTarget6AL62.Init(FVector::ZeroVector, 3);
        AvoidCoverLastOrderAt6AL62.Init(-100.f, 3);
    }

    const FVector BallWorld = PracticeBall6B->GetActorLocation();
    for (int32 i = 0; i < AvoidZoneDefenders6AL6.Num(); ++i)
    {
        ASoccerAICharacter* Rival = AvoidZoneDefenders6AL6[i];
        if (!IsValid(Rival) || Rival == TackleOpponent6AK ||
            Rival->IsTackleActive() || Rival->IsAIKickMontageActive())
        {
            continue;
        }
        AAIController* AI = Cast<AAIController>(Rival->GetController());
        if (!AI) continue;

        const FVector CoverLocal = GetAvoidDefenderCoverLocal6AL62(i + 1, BallWorld);
        const FVector CoverWorld = Field->PitchLocalToWorld(CoverLocal);
        const float DistanceToCover = FVector::Dist2D(Rival->GetActorLocation(), CoverWorld);
        const FVector PreviousTarget = AvoidCoverLastTarget6AL62[i];
        const bool bNewTarget = PreviousTarget.IsNearlyZero() ||
            FVector::Dist2D(PreviousTarget, CoverWorld) >= AvoidCoverRepathThreshold6AL62;
        const bool bOldOrder = Now - AvoidCoverLastOrderAt6AL62[i] >= 3.0f;

        if (DistanceToCover <= 85.f)
        {
            // Let the nav controller complete its last few centimetres.
            // Only turn the idle player towards the ball, not while running.
            if (Rival->GetVelocity().Size2D() < 40.f)
            {
                const FVector LookAt = BallWorld - Rival->GetActorLocation();
                if (!LookAt.IsNearlyZero())
                {
                    const FRotator Facing(0.f, LookAt.Rotation().Yaw, 0.f);
                    Rival->SetActorRotation(FMath::RInterpTo(
                        Rival->GetActorRotation(), Facing, AvoidCoverUpdateSeconds6AL62, 2.5f));
                }
            }
            continue;
        }
        // Do not restart MoveTo for tiny ball movements. Refresh an old
        // command occasionally in case navmesh avoidance interrupted it.
        if (!bNewTarget && !bOldOrder) continue;

        Rival->SetAIChasingBall(false); // covering, NOT pursuing / tackling
        AI->MoveToLocation(CoverWorld, 65.f, true, true, true, true, nullptr, true);
        Rival->RequestAIMovementMode(
            ESoccerAIMovementMode::Run,
            ESoccerAIMovementReason::PressBallCarrier, true);
        AvoidCoverLastTarget6AL62[i] = CoverWorld;
        AvoidCoverLastOrderAt6AL62[i] = Now;
        if (bNewTarget)
        {
            UE_LOG(LogTemp, Display,
                TEXT("[Training6AL62] COVER zone=%d ballZone=%d localX=%.0f targetY=%.0f distance=%.0f active=NO"),
                i + 1, ActiveAvoidZone6AL6, CoverLocal.X, CoverLocal.Y,
                DistanceToCover);
        }
    }
}

void ASoccerTrainingMovementGameMode6B::DrawAvoidZones6AL6(float Now)
{
    if (!bAvoidTackleLesson6AK || Now - LastZoneLinesAt6AL6 < 0.12f) return;
    const ASoccerField* Field = FindSharedField6L();
    if (!Field || !GetWorld()) return;
    LastZoneLinesAt6AL6 = Now;
    const float HalfLength = SoccerFieldDimensions::HalfPitchLengthCm;
    const float HalfWidth = SoccerFieldDimensions::HalfPitchWidthCm;
    const FColor LineColor(245, 211, 44);
    for (int32 Boundary = 1; Boundary <= 3; ++Boundary)
    {
        const float X = -HalfLength + Boundary * HalfLength * 0.5f;
        const FVector A = Field->PitchLocalToWorld(FVector(X, -HalfWidth + 12.f, 18.f));
        const FVector B = Field->PitchLocalToWorld(FVector(X, HalfWidth - 12.f, 18.f));
        DrawDebugLine(GetWorld(), A, B, LineColor, false, 0.18f, 0, 6.f);
    }
}

void ASoccerTrainingMovementGameMode6B::EnsureTackleOpponent6AK()
{
    if (!IsTackleLesson6AK() || !GetWorld()) return;
    if (!TackleOpponentClass6AK)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Training6AK] No TackleOpponentClass6AK. Select BP_SoccerAIFieldPlayer in the training GameMode Blueprint."));
        return;
    }
    const ASoccerField* Field = FindSharedField6L();
    if (!Field) return;

    auto PrepareOpponent = [](ASoccerAICharacter* Rival)
    {
        if (!IsValid(Rival)) return;
        if (Rival->Team != ESoccerTeam::OpponentTeam)
        {
            Rival->Team = ESoccerTeam::OpponentTeam;
            Rival->ApplyTeamUniform();
        }
        // Keep the match tactical Tick disabled. The GameMode controls orders.
        if (AController* AI = Rival->GetController())
        {
            if (AAIController* AIC = Cast<AAIController>(AI)) AIC->StopMovement();
            AI->SetActorTickEnabled(false);
        }
        Rival->SetAIChasingBall(false);
    };
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    if (bAvoidTackleLesson6AK)
    {
        if (AvoidZoneDefenders6AL6.Num() != 3) AvoidZoneDefenders6AL6.SetNumZeroed(3);
        for (int32 i = 0; i < 3; ++i)
        {
            if (IsValid(AvoidZoneDefenders6AL6[i])) continue;
            ASoccerAICharacter* Rival = GetWorld()->SpawnActor<ASoccerAICharacter>(
                TackleOpponentClass6AK.Get(),
                Field->PitchLocalToWorld(GetAvoidDefenderHomeLocal6AL6(i + 1)),
                Field->GetActorRotation(), Params);
            AvoidZoneDefenders6AL6[i] = Rival;
            PrepareOpponent(Rival);
            if (IsValid(Rival))
            {
                Rival->SetTrainingRunSpeedMultiplier6AL63(1.125f);
                UE_LOG(LogTemp, Display,
                    TEXT("[Training6AL63] SPEED BOOST zone=%d multiplier=1.125 currentMaxSpeed=%.1f"),
                    i + 1, Rival->GetCharacterMovement() ? Rival->GetCharacterMovement()->MaxWalkSpeed : 0.f);
            }
            UE_LOG(LogTemp, Display,
                TEXT("[Training6AL6] DEFENDER SPAWN zone=%d class=%s team=%d valid=%s"),
                i + 1, *GetNameSafe(TackleOpponentClass6AK.Get()),
                IsValid(Rival) ? static_cast<int32>(Rival->GetTeam()) : -1,
                IsValid(Rival) ? TEXT("YES") : TEXT("NO"));
        }
        return; // Zone 0 intentionally has no active defender.
    }

    // Preserve the separate 6AK/6AM 'Recuperar pelota' scenario unchanged.
    if (IsValid(TackleOpponent6AK)) return;
    const FVector StartLocal(-0.06f * SoccerFieldDimensions::HalfPitchLengthCm, 220.f, 120.f);
    TackleOpponent6AK = GetWorld()->SpawnActor<ASoccerAICharacter>(
        TackleOpponentClass6AK.Get(), Field->PitchLocalToWorld(StartLocal),
        Field->GetActorRotation(), Params);
    PrepareOpponent(TackleOpponent6AK);
    UE_LOG(LogTemp, Display, TEXT("[Training6AK] Opponent spawned: %s class=%s"),
        *GetNameSafe(TackleOpponent6AK), *GetNameSafe(TackleOpponentClass6AK.Get()));
}

void ASoccerTrainingMovementGameMode6B::PositionTackleScenario6AK()
{
    if (!IsTackleLesson6AK()) return;
    ASoccerField* Field = FindSharedField6L();
    if (!Field || !IsValid(Trainee6B) || !IsValid(PracticeBall6B)) return;
    if (Trainee6B->Team != ESoccerTeam::PlayerTeam)
    {
        Trainee6B->Team = ESoccerTeam::PlayerTeam;
        Trainee6B->ApplyTeamUniform();
    }
    const float HalfLength = SoccerFieldDimensions::HalfPitchLengthCm;
    const FVector PlayerLocal = bAvoidTackleLesson6AK
        ? FVector(SoccerFieldDimensions::LeftGoalLineX + FMath::Max(220.f, HalfLength * 0.06f), 0.f, 130.f)
        : FVector(-0.30f * HalfLength, 0.f, 130.f);
    const FVector BallLocal = bAvoidTackleLesson6AK
        ? SoccerFieldDimensions::GetPenaltySpotLocalLocation(-1.f, 70.f)
        : FVector(-0.06f * HalfLength + 115.f, 220.f, 70.f);
    Trainee6B->SetActorLocation(Field->PitchLocalToWorld(PlayerLocal), false,
        nullptr, ETeleportType::TeleportPhysics);
    if (bAvoidTackleLesson6AK)
    {
        // Exactly the three red players in the diagram; inactive players
        // remain visible at home but have no tackle/press orders.
        for (int32 i = 0; i < AvoidZoneDefenders6AL6.Num(); ++i)
        {
            ASoccerAICharacter* Rival = AvoidZoneDefenders6AL6[i];
            if (!IsValid(Rival)) continue;
            if (Rival->Team != ESoccerTeam::OpponentTeam)
            {
                Rival->Team = ESoccerTeam::OpponentTeam;
                Rival->ApplyTeamUniform();
            }
            Rival->CancelAIKickMontageBeforeImpact();
            if (Rival->IsTackleActive()) Rival->CancelTackle();
            Rival->ReleaseAIBall(false);
            if (AAIController* AI = Cast<AAIController>(Rival->GetController())) AI->StopMovement();
            Rival->SetAIChasingBall(false);
            if (UCharacterMovementComponent* Move = Rival->GetCharacterMovement())
                Move->StopMovementImmediately();
            const FVector Home = Field->PitchLocalToWorld(GetAvoidDefenderHomeLocal6AL6(i + 1));
            Rival->SetActorLocation(Home, false, nullptr, ETeleportType::TeleportPhysics);
            Rival->SetActorRotation((Field->PitchLocalToWorld(BallLocal) - Home).Rotation());
        }
    }
    else if (IsValid(TackleOpponent6AK))
    {
        if (TackleOpponent6AK->IsTackleActive()) TackleOpponent6AK->CancelTackle();
        if (UCharacterMovementComponent* Move = TackleOpponent6AK->GetCharacterMovement())
            Move->StopMovementImmediately();
        const FVector RivalWorld = Field->PitchLocalToWorld(FVector(-0.06f * HalfLength, 220.f, 130.f));
        TackleOpponent6AK->SetActorLocation(RivalWorld, false, nullptr, ETeleportType::TeleportPhysics);
    }
    UPrimitiveComponent* BallBody = Cast<UPrimitiveComponent>(PracticeBall6B->GetRootComponent());
    if (BallBody)
    {
        BallBody->SetSimulatePhysics(false);
        BallBody->SetPhysicsLinearVelocity(FVector::ZeroVector);
        BallBody->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
    }
    PracticeBall6B->SetActorLocation(Field->PitchLocalToWorld(BallLocal), false,
        nullptr, ETeleportType::TeleportPhysics);
    RestorePracticeBallGroundCollision6AB3(PracticeBall6B);
    if (BallBody) BallBody->SetSimulatePhysics(true);
    if (bAvoidTackleLesson6AK)
    {
        UE_LOG(LogTemp, Display, TEXT("[Training6AL6] SCENARIO attempt=%d player=%s penaltyBall=%s defenders=%d"),
            AvoidTackleAttempt6AL, *Trainee6B->GetActorLocation().ToString(),
            *PracticeBall6B->GetActorLocation().ToString(), AvoidZoneDefenders6AL6.Num());
    }
    else
    {
        UE_LOG(LogTemp, Display, TEXT("[Training6AK] Scenario=WinTackle player=%s rival=%s ball=%s"),
            *Trainee6B->GetActorLocation().ToString(), *GetNameSafe(TackleOpponent6AK),
            *PracticeBall6B->GetActorLocation().ToString());
    }
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
    if (IsTackleLesson6AK())
    {
        if (!bExerciseStarted6H) return TEXT("Lee las instrucciones y comienza el ejercicio");
        if (bAvoidTackleLesson6AK)
            return FString::Printf(TEXT("6AL.6.3 | Intento %d/10 | Zona pelota %d/3 | Arco %d | Tackles %d | %s"),
                FMath::Min(10, AvoidTackleAttempt6AL), FMath::Max(0, ActiveAvoidZone6AL6),
                AvoidTackleSuccesses6AL, AvoidDefenderTackles6AL2, *AvoidTackleFeedback6AL);
        return IsValid(TackleOpponent6AK)
            ? (bAvoidTackleLesson6AK
                ? TEXT("6AK: escenario de esquivar tackles listo. La IA defensiva se integra en 6AL.")
                : TEXT("6AK: escenario de recuperar pelota listo. La conducción rival se integra en 6AM."))
            : TEXT("6AK: configura Tackle Opponent Class en el GameMode Blueprint.");
    }
    if (bChargedShotLesson6M)
    {
        if (bZoneFinishLesson6AH)
        {
            if (bComplete6B) return TEXT("Diez jugadas completadas. Consulta el resultado.");
            if (!bExerciseStarted6H) return TEXT("Lee las instrucciones y comienza el ejercicio");
            if (bBetweenShotAttempts6N) return GetLastShotFeedback6P() + TEXT(". Preparando la nueva zona...");
            if (bShotReleased6M) return TEXT("Remate en curso. El intento termina normalmente aunque hayas pateado fuera del circulo.");
            return IsBallInsideShotZone6AH()
                ? TEXT("PELOTA EN ZONA: podes rematar con boton derecho. Apunta al arco.")
                : TEXT("Podes apuntar y cargar fuerza en cualquier momento. Solo vale el remate que salga desde el circulo.");
        }
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
    if (bChargedShotLesson6M || IsTackleLesson6AK()) return; // No checkpoints for tackle setup.
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
            const FVector SpawnLocal = bZoneFinishLesson6AH
                ? FVector(0.f, 0.f, 130.f)
                : FVector(BallLocal.X - 300.f, BallLocal.Y, 130.f);
            World->SpawnActor<APlayerStart>(Field->PitchLocalToWorld(SpawnLocal), Field->GetActorRotation());
            return;
        }
        const FVector LocalStart = bAvoidTackleLesson6AK
            ? FVector(SoccerFieldDimensions::LeftGoalLineX +
                FMath::Max(220.f, SoccerFieldDimensions::HalfPitchLengthCm * 0.06f), 0.f, 130.f)
            : bWinTackleLesson6AK
                ? FVector(-0.30f * SoccerFieldDimensions::HalfPitchLengthCm, 0.f, 130.f)
                : FVector(-0.78f * SoccerFieldDimensions::HalfPitchLengthCm, 0.f, 130.f);
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
        const FVector LocalBall = bAvoidTackleLesson6AK
            ? SoccerFieldDimensions::GetPenaltySpotLocalLocation(-1.f, 70.f)
            : bWinTackleLesson6AK
                ? FVector(-0.18f * SoccerFieldDimensions::HalfPitchLengthCm, 0.f, 70.f)
            : bZoneFinishLesson6AH ? FVector(260.f, 0.f, 70.f)
            : bChargedShotLesson6M ? GetShotBallLocal6N(0)
            : FVector(-0.63f * SoccerFieldDimensions::HalfPitchLengthCm, 0.f, 70.f);
        BallPosition = Field->PitchLocalToWorld(LocalBall);
        BallRotation = Field->GetActorRotation();
    }
    PracticeBall6B = World->SpawnActor<ASoccerBall>(BallClass,
        BallPosition, BallRotation, Spawn);
}
