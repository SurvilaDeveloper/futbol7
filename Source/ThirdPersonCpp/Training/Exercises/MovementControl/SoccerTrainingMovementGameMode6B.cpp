#include "Training/Exercises/MovementControl/SoccerTrainingMovementGameMode6B.h"
#include "Training/Exercises/MovementControl/SoccerTrainingMovementHUD6B.h"
#include "Training/Shared/SoccerTrainingPlayerController6B.h"

#include "SoccerBall.h"
#include "ThirdPersonCppCharacter.h"
#include "SoccerPlayerProfile.h"
#include "EngineUtils.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/PlayerStart.h"
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
    bAutoPassLesson6J = UGameplayStatics::ParseOption(OptionsString, TEXT("TrainingLesson"))
        .Equals(TEXT("AutoPass"), ESearchCase::IgnoreCase);
    UE_LOG(LogTemp, Display, TEXT("[Training6J] Lesson: %s"),
        bAutoPassLesson6J ? TEXT("AutoPass") : TEXT("Dribble"));
    if (bGenerateTestGround6B) EnsureGround6B();
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
    bComplete6B = false;
    ElapsedTime6B = 0.f;
    CheckpointIndex6B = 0;
    LostControlCount6B = 0;
    OutOfControlSeconds6B = 0.f;
    bHadControl6B = false;
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

FVector ASoccerTrainingMovementGameMode6B::GetCurrentCheckpoint6B() const
{
    // Ordered slalom; the ground is 4000 x 2600 cm, all points fit inside.
    static const FVector Checkpoints[] = {
        FVector(0.f, -400.f, 25.f),
        FVector(500.f, 400.f, 25.f),
        FVector(1050.f, -400.f, 25.f),
        FVector(1500.f, 0.f, 25.f)
    };
    return Checkpoints[FMath::Clamp(CheckpointIndex6B, 0, 3)];
}

FString ASoccerTrainingMovementGameMode6B::GetExerciseMessage6B() const
{
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
    PracticeBall6B = World->SpawnActor<ASoccerBall>(BallClass,
        FVector(-350.f,0.f,70.f), FRotator::ZeroRotator, Spawn);
}
