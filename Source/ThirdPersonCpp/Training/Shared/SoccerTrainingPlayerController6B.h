#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SoccerTrainingPlayerController6B.generated.h"

class USoccerTrainingPauseWidget6B;
class USoccerTrainingExerciseWidget6H;

/** Shared pause/return controls; independent of AGameHUD and ASoccerMatchManager. */
UCLASS()
class THIRDPERSONCPP_API ASoccerTrainingPlayerController6B : public APlayerController
{
    GENERATED_BODY()

public:
    virtual void BeginPlay() override;
    virtual bool InputKey(FKey Key, EInputEvent EventType,
        float AmountDepressed = 1.0f, bool bGamepad = false) override;

    void StartExercise6H();
    void ShowExerciseResults6H();
    void ResumeTraining6B();
    void RestartTraining6B();
    void ReturnToExerciseSelector6B();
    void ReturnToMainMenu6B();

private:
    void ApplyExerciseUIMode6H();
    void ToggleTrainingPause6B();

    UPROPERTY(Transient) USoccerTrainingExerciseWidget6H* ExerciseWidget6H = nullptr;
    UPROPERTY(Transient) USoccerTrainingPauseWidget6B* PauseWidget6B = nullptr;
    bool bTrainingPaused6B = false;
    // Training 6I: only the first left mouse press may start ball collection.
    bool bInitialBallCollectionClickUsed6I = false;
};
