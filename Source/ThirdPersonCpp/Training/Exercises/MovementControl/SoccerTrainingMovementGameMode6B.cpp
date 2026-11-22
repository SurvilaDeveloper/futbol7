#include "Training/Exercises/MovementControl/SoccerTrainingMovementGameMode6B.h"
#include "Training/Exercises/MovementControl/SoccerTrainingMovementHUD6B.h"
#include "Training/Shared/SoccerTrainingPlayerController6B.h"

#include "SoccerBall.h"
#include "SoccerField.h"
#include "SoccerFieldDimensions.h"
#include "ThirdPersonCppCharacter.h"
#include "SoccerPlayerProfile.h"
#include "EngineUtils.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "DrawDebugHelpers.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/GameplayStatics.h"

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
    bChargedShotLesson6M = Lesson.Equals(TEXT("ChargedShot"), ESearchCase::IgnoreCase);
    UE_LOG(LogTemp, Display, TEXT("[Training6J] Lesson: %s"),
        bAutoPassLesson6J ? TEXT("AutoPass") : TEXT("Dribble"));
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
    if (bChargedShotLesson6M)
    {
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

void ASoccerTrainingMovementGameMode6B::RegisterChargedShotRelease6M(float ChargeFraction)
{
    if (!bChargedShotLesson6M || !IsExerciseRunning6H() || bShotReleased6M || bBetweenShotAttempts6N || !IsValid(PracticeBall6B)) return;
    bShotReleased6M = true;
    bGoalScored6M = false;
    bShotInFlight6M = false;
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
            ResolveShotAttempt6N(Outcome);
            return;
        }
    }
    PreviousBallLocal6M = Current;
    bHavePreviousBall6M = true;
    // Allow time for the character to run up to the ball and play the kick.
    // After ball movement, a settled ball can terminate a missed attempt early.
    if (SinceRelease > 15.f ||
        (bShotInFlight6M && SinceRelease > 3.f && BallVelocity.Size() < 35.f))
    {
        // No definitive goal-line crossing. A barrier collision is evidence;
        // otherwise keep an unresolved result instead of guessing the cause.
        ResolveShotAttempt6N(bBarrierContactThisShot6P
            ? ETrainingShotOutcome6P::Barrier : ETrainingShotOutcome6P::Unresolved);
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

    // Cancel delayed kick/approach actions before repositioning, to avoid a
    // previous attempt striking a freshly reset ball.
    Trainee6B->ReleaseBallForMatchRestart(false);
    if (UCharacterMovementComponent* Movement = Trainee6B->GetCharacterMovement())
        Movement->StopMovementImmediately();
    PracticeBall6B->SetPossessed(false);
    PracticeBall6B->StopBallKeepingPhysics();

    const FVector BallLocal = GetShotBallLocal6N(ShotAttemptIndex6N);
    // 6O: update the obstacle layout before the player takes the next kick.
    SetupPracticeBarriers6O(BallLocal);
    const FVector PlayerLocal(BallLocal.X - 300.f, BallLocal.Y, 130.f);
    Trainee6B->SetActorLocationAndRotation(Field->PitchLocalToWorld(PlayerLocal),
        Field->GetActorRotation(), false, nullptr, ETeleportType::TeleportPhysics);
    PracticeBall6B->SetActorLocationAndRotation(Field->PitchLocalToWorld(BallLocal),
        Field->GetActorRotation(), false, nullptr, ETeleportType::TeleportPhysics);
    PracticeBall6B->StopBallKeepingPhysics();
    bShotReleased6M = false;
    bShotInFlight6M = false;
    bHavePreviousBall6M = false;
    bGoalScored6M = false;
    ShotCharge6M = 0.f;
    bBarrierContactThisShot6P = false;
    UE_LOG(LogTemp, Display, TEXT("[Training6N] Ready: attempt %d/10"), ShotAttemptIndex6N + 1);
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
    return Message;
}

FString ASoccerTrainingMovementGameMode6B::GetShotResultsReport6P() const
{
    int32 Wide = 0, Over = 0, Blocked = 0, Unresolved = 0, BarrierTouches = 0;
    for (int32 Index = 0; Index < ShotOutcomes6P.Num(); ++Index)
    {
        switch (ShotOutcomes6P[Index])
        {
        case ETrainingShotOutcome6P::Wide: ++Wide; break;
        case ETrainingShotOutcome6P::OverBar: ++Over; break;
        case ETrainingShotOutcome6P::Barrier: ++Blocked; break;
        case ETrainingShotOutcome6P::Unresolved: ++Unresolved; break;
        default: break;
        }
        if (ShotBarrierContacts6P.IsValidIndex(Index) && ShotBarrierContacts6P[Index])
            ++BarrierTouches;
    }
    FString Report = FString::Printf(
        TEXT("Desviados: %d  |  Sobre travesano: %d\nBarrera: %d  |  Sin resolucion: %d  |  Contactos barrera: %d\n"),
        Wide, Over, Blocked, Unresolved, BarrierTouches);
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
    ++ShotsCompleted6N;
    if (bGoal) ++GoalsScored6N;
    UE_LOG(LogTemp, Display, TEXT("[Training6P] Attempt %d/10: %s, charge %.0f%%, barrier contact %s"),
        ShotsCompleted6N, *ShotOutcomeLabel6P(Outcome), ShotCharge6M * 100.f,
        bBarrierContactThisShot6P ? TEXT("YES") : TEXT("NO"));
    if (ShotsCompleted6N >= 10)
    {
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
        NextAttemptAt6N = GetWorld()->GetTimeSeconds() + 1.5f;
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
        return TEXT("Apunta con la cruz verde, mant\u00e9n clic derecho para cargar y suelta. No necesitas posesi\u00f3n.");
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
