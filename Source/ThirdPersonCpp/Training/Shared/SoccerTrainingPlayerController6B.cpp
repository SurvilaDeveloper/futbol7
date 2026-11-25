#include "Training/Shared/SoccerTrainingPlayerController6B.h"
#include "Training/Shared/SoccerTrainingPauseWidget6B.h"
#include "Training/Exercises/MovementControl/SoccerTrainingExerciseWidget6H.h"
#include "Training/Exercises/MovementControl/SoccerTrainingMovementGameMode6B.h"

#include "Blueprint/UserWidget.h"
#include "TimerManager.h"
#include "InputCoreTypes.h"
#include "Misc/PackageName.h"
#include "Engine/World.h"
#include "UObject/Package.h"
#include "Kismet/GameplayStatics.h"
#include "ThirdPersonCppCharacter.h"

void ASoccerTrainingPlayerController6B::BeginPlay()
{
    Super::BeginPlay();
    PauseWidget6B = CreateWidget<USoccerTrainingPauseWidget6B>(
        this, USoccerTrainingPauseWidget6B::StaticClass());
    if (IsValid(PauseWidget6B))
    {
        PauseWidget6B->AddToViewport(100);
        PauseWidget6B->ShowPause6B(false);
    }
    ExerciseWidget6H = CreateWidget<USoccerTrainingExerciseWidget6H>(
        this, USoccerTrainingExerciseWidget6H::StaticClass());
    if (IsValid(ExerciseWidget6H))
    {
        ExerciseWidget6H->AddToViewport(110);
        ExerciseWidget6H->ShowInstructions6H();
        // ThirdPersonCharacter::BeginPlay() also sets GameOnly and hides the cursor.
        // Apply UI mode on the next game tick, after the pawn has initialized.
        GetWorldTimerManager().SetTimerForNextTick(
            this, &ASoccerTrainingPlayerController6B::ApplyExerciseUIMode6H);
    }
}

void ASoccerTrainingPlayerController6B::ApplyExerciseUIMode6H()
{
    if (!IsLocalController() || !IsValid(ExerciseWidget6H) ||
        ExerciseWidget6H->GetVisibility() != ESlateVisibility::Visible)
        return;

    bShowMouseCursor = true;
    bEnableClickEvents = true;
    bEnableMouseOverEvents = true;
    FInputModeUIOnly InputMode;
    InputMode.SetWidgetToFocus(ExerciseWidget6H->TakeWidget());
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(InputMode);
    ExerciseWidget6H->FocusPrimary6H();
    UE_LOG(LogTemp, Display, TEXT("[Training6H2] Instructions UI input enabled: cursor=%d"),
        bShowMouseCursor ? 1 : 0);
}

void ASoccerTrainingPlayerController6B::ApplyExerciseGameInput6J()
{
    // In gameplay, use the same captured mouse input as the match.
    // The green reticle is drawn by the training HUD; never display the OS pointer.
    bShowMouseCursor = false;
    bEnableClickEvents = true;
    bEnableMouseOverEvents = false;
    FInputModeGameOnly InputMode;
    SetInputMode(InputMode);
}

bool ASoccerTrainingPlayerController6B::IsAutoPassAimEnabled6J() const
{
    const ASoccerTrainingMovementGameMode6B* Mode = GetWorld()
        ? Cast<ASoccerTrainingMovementGameMode6B>(GetWorld()->GetAuthGameMode()) : nullptr;
    return Mode && Mode->IsAutoPassLesson6J() && Mode->IsExerciseRunning6H()
        && !bTrainingPaused6B;
}

void ASoccerTrainingPlayerController6B::StartExercise6H()
{
    ASoccerTrainingMovementGameMode6B* Mode = GetWorld()
        ? Cast<ASoccerTrainingMovementGameMode6B>(GetWorld()->GetAuthGameMode()) : nullptr;
    if (!Mode) return;
    Mode->BeginExercise6H();
    if (IsValid(ExerciseWidget6H)) ExerciseWidget6H->SetVisibility(ESlateVisibility::Collapsed);
    ApplyExerciseGameInput6J();
}

void ASoccerTrainingPlayerController6B::ShowExerciseResults6H()
{
    if (!IsValid(ExerciseWidget6H)) return;
    ExerciseWidget6H->ShowResults6H();
    ApplyExerciseUIMode6H();
}

bool ASoccerTrainingPlayerController6B::InputKey(
    FKey Key, EInputEvent EventType, float AmountDepressed, bool bGamepad)
{
    if (IsValid(ExerciseWidget6H) && ExerciseWidget6H->GetVisibility() == ESlateVisibility::Visible)
        return true;
    // Training 6I remains possession + keys only. Training 6J reuses the
    // actual character autopass binding and the match-style virtual aim reticle.
    ASoccerTrainingMovementGameMode6B* TrainingMode = GetWorld()
        ? Cast<ASoccerTrainingMovementGameMode6B>(GetWorld()->GetAuthGameMode()) : nullptr;
    if (TrainingMode && TrainingMode->IsExerciseRunning6H() && !bTrainingPaused6B)
    {
        if (TrainingMode->IsPassLesson6AO() &&
            (Key == EKeys::Q || Key == EKeys::RightMouseButton || Key == EKeys::Gamepad_RightTriggerAxis ||
             Key == EKeys::LeftMouseButton || Key == EKeys::Gamepad_RightTrigger))
        {
            if (!TrainingMode->CanCollectGroundPass6AN()) return true;
            return Super::InputKey(Key, EventType, AmountDepressed, bGamepad);
        }
        if (Key == EKeys::RightMouseButton)
        {
            if (!TrainingMode->IsChargedShotLesson6M() || TrainingMode->HasShotBeenReleased6M() || TrainingMode->IsBetweenShotAttempts6N())
                return true;
            AThirdPersonCppCharacter* Shooter = Cast<AThirdPersonCppCharacter>(GetPawn());
            if (!Shooter) return true; // Existing match kick chases the ball on release.
            // 6AI.3: never block aiming or the right-button charge because
            // the moving ball is outside the zone. Validate the actual shot
            // at launch in the training GameMode instead.
            if (EventType == IE_Released)
            {
                float Charge = 0.f;
                if (Shooter->GetKickChargePercent(Charge))
                    TrainingMode->RegisterChargedShotRelease6M(Charge);
            }
            return Super::InputKey(Key, EventType, AmountDepressed, bGamepad);
        }

        if (Key == EKeys::LeftMouseButton)
        {
            if (!TrainingMode->IsAutoPassLesson6J() && !TrainingMode->IsChargedShotLesson6M())
            {
                // This lesson allows repeated attempts to collect the ball.
                // Ignore clicks during possession (no autopass) and while the
                // initial chase is still active (no queued KickAndFollow).
                // If movement keys cancel the chase, the pawn returns to
                // Manual and the next left click can initiate a new chase.
                const AThirdPersonCppCharacter* Trainee =
                    Cast<AThirdPersonCppCharacter>(GetPawn());
                if (!Trainee || Trainee->IsPossessingBall() ||
                    Trainee->HasHumanLogicalBallControl() ||
                    Trainee->IsChasingBall() || Trainee->IsKicking())
                {
                    return true;
                }
            }
            else if (TrainingMode->IsChargedShotLesson6M() &&
                     !TrainingMode->IsZoneFinishLesson6AH())
            {
                // Shooting lesson uses right click only. No collection or autopass.
                return true;
            }
            else if (EventType == IE_Pressed)
            {
                // The first click collects; subsequent clicks with possession
                // are ordinary autopasses, handled by ThirdPersonCppCharacter.
                if (AThirdPersonCppCharacter* Trainee =
                    Cast<AThirdPersonCppCharacter>(GetPawn()))
                {
                    if (Trainee->IsPossessingBall())
                        TrainingMode->RegisterAutoPassClick6J();
                }
            }
        }
    }

    // Intercept before the human pawn sees P/Escape. Its regular soccer-match
    // bindings remain unchanged in every other map.
    if (EventType == IE_Pressed &&
        (Key == EKeys::P || Key == EKeys::Escape ||
         Key == EKeys::Gamepad_LeftThumbstick))
    {
        ToggleTrainingPause6B();
        return true;
    }
    return Super::InputKey(Key, EventType, AmountDepressed, bGamepad);
}

void ASoccerTrainingPlayerController6B::ToggleTrainingPause6B()
{
    if (bTrainingPaused6B)
    {
        ResumeTraining6B();
        return;
    }
    if (!IsValid(PauseWidget6B)) return;
    bTrainingPaused6B = true;
    UGameplayStatics::SetGamePaused(this, true);
    PauseWidget6B->ShowPause6B(true);
    bShowMouseCursor = true;
    bEnableClickEvents = true;
    bEnableMouseOverEvents = true;
    FInputModeUIOnly Mode;
    Mode.SetWidgetToFocus(PauseWidget6B->TakeWidget());
    Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(Mode);
    PauseWidget6B->FocusResume6B();
}

void ASoccerTrainingPlayerController6B::ResumeTraining6B()
{
    bTrainingPaused6B = false;
    UGameplayStatics::SetGamePaused(this, false);
    if (IsValid(PauseWidget6B)) PauseWidget6B->ShowPause6B(false);
    ASoccerTrainingMovementGameMode6B* TrainingMode = GetWorld()
        ? Cast<ASoccerTrainingMovementGameMode6B>(GetWorld()->GetAuthGameMode()) : nullptr;
    if (TrainingMode && TrainingMode->IsExerciseRunning6H())
        ApplyExerciseGameInput6J();
    else
        ApplyExerciseUIMode6H();
}

void ASoccerTrainingPlayerController6B::RestartTraining6B()
{
    ResumeTraining6B();
    // Reopen the ACTIVE map, not any other exercise or SoccerMainMap.
    // Resolve the full package path, including each exercise's own folder.
    // GetCurrentLevelName(..., true) strips the PIE instance prefix.
    if (const UWorld* TrainingWorld6B = GetWorld())
    {
        const FString ShortName = UGameplayStatics::GetCurrentLevelName(this, true);
        const FString Folder = FPackageName::GetLongPackagePath(
            TrainingWorld6B->GetOutermost()->GetName());
        if (!ShortName.IsEmpty() && !Folder.IsEmpty())
        {
            const ASoccerTrainingMovementGameMode6B* TrainingMode =
                Cast<ASoccerTrainingMovementGameMode6B>(TrainingWorld6B->GetAuthGameMode());
            const bool bAutoPass = TrainingMode && TrainingMode->IsAutoPassLesson6J();
            const bool bShot = TrainingMode && TrainingMode->IsChargedShotLesson6M();
            UGameplayStatics::OpenLevel(this,
                FName(*(Folder + TEXT("/") + ShortName)), true,
                (TrainingMode && TrainingMode->IsAerialPassLesson6AO()) ? TEXT("TrainingLesson=AerialPass") : (TrainingMode && TrainingMode->IsPassLesson6AO()) ? TEXT("TrainingLesson=GroundPass") : (TrainingMode && TrainingMode->IsAvoidTackleLesson6AK()) ? TEXT("TrainingLesson=AvoidTackle") : (TrainingMode && TrainingMode->IsWinTackleLesson6AK()) ? TEXT("TrainingLesson=WinTackle") : (TrainingMode && TrainingMode->IsZoneFinishLesson6AH()) ? TEXT("TrainingLesson=ZoneFinish") : (TrainingMode && TrainingMode->IsGoalkeeperShotLesson6T()) ? TEXT("TrainingLesson=GoalkeeperShot") : bShot ? TEXT("TrainingLesson=ChargedShot") : bAutoPass ? TEXT("TrainingLesson=AutoPass") : TEXT(""));
        }
    }
}

void ASoccerTrainingPlayerController6B::ReturnToExerciseSelector6B()
{
    ResumeTraining6B();
    UGameplayStatics::OpenLevel(this,
        FName(TEXT("/Game/ThirdPersonCPP/Maps/SoccerMainMenuMap")),
        true, TEXT("TrainingSelector=1"));
}

void ASoccerTrainingPlayerController6B::ReturnToMainMenu6B()
{
    ResumeTraining6B();
    UGameplayStatics::OpenLevel(this,
        FName(TEXT("/Game/ThirdPersonCPP/Maps/SoccerMainMenuMap")), true);
}
