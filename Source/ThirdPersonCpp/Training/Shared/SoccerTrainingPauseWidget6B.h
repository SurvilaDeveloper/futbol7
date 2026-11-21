#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SoccerTrainingPauseWidget6B.generated.h"

class UButton;

/** Pauses one training level. Not tied to the soccer match session or statistics. */
UCLASS()
class THIRDPERSONCPP_API USoccerTrainingPauseWidget6B : public UUserWidget
{
    GENERATED_BODY()

public:
    void ShowPause6B(bool bVisible);
    void FocusResume6B();

protected:
    virtual void NativeOnInitialized() override;
    virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry,
        const FKeyEvent& KeyEvent) override;
    virtual FReply NativeOnAnalogValueChanged(const FGeometry& Geometry,
        const FAnalogInputEvent& AnalogEvent) override;
    virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;

private:
    void BuildPause6B();
    void Navigate6B(int32 Offset);

    UFUNCTION() void Resume6B();
    UFUNCTION() void Restart6B();
    UFUNCTION() void ChangeExercise6B();
    UFUNCTION() void MainMenu6B();

    UPROPERTY(Transient) UButton* ResumeButton6B = nullptr;
    UPROPERTY(Transient) UButton* RestartButton6B = nullptr;
    UPROPERTY(Transient) UButton* ChangeExerciseButton6B = nullptr;
    UPROPERTY(Transient) UButton* MainMenuButton6B = nullptr;
    double LastAnalogTime6B = -1.0;
};
