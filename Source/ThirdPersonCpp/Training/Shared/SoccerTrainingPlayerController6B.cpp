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

void ASoccerTrainingPlayerController6B::StartExercise6H()
{
    ASoccerTrainingMovementGameMode6B* Mode = GetWorld()
        ? Cast<ASoccerTrainingMovementGameMode6B>(GetWorld()->GetAuthGameMode()) : nullptr;
    if (!Mode) return;
    bInitialBallCollectionClickUsed6I = false;
    Mode->BeginExercise6H();
    if (IsValid(ExerciseWidget6H)) ExerciseWidget6H->SetVisibility(ESlateVisibility::Collapsed);
    bShowMouseCursor = false;
    FInputModeGameOnly InputMode;
    SetInputMode(InputMode);
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
    // Training 6I: this lesson teaches only ball possession and I/J/K/L dribbling.
    // Intercept mouse actions BEFORE pawn input bindings; avoid changing match code.
    // UI clicks continue to work because the instruction/result screens use UIOnly.
    const ASoccerTrainingMovementGameMode6B* TrainingMode = GetWorld()
        ? Cast<ASoccerTrainingMovementGameMode6B>(GetWorld()->GetAuthGameMode()) : nullptr;
    if (TrainingMode && TrainingMode->IsExerciseRunning6H() && !bTrainingPaused6B)
    {
        // No charge, shot or release from the right button in this lesson.
        if (Key == EKeys::RightMouseButton)
            return true;

        if (Key == EKeys::LeftMouseButton)
        {
            if (bInitialBallCollectionClickUsed6I)
                return true; // blocks autopass on subsequent presses and releases

            if (EventType == IE_Pressed)
                bInitialBallCollectionClickUsed6I = true;
            // Pass the first press through to HandleLeftClickTarget() for collection.
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
    bShowMouseCursor = false;
    FInputModeGameOnly Mode;
    SetInputMode(Mode);
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
            UGameplayStatics::OpenLevel(this,
                FName(*(Folder + TEXT("/") + ShortName)), true);
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
