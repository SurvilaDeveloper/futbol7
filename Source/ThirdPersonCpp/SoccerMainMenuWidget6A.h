#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SoccerMainMenuWidget6A.generated.h"

class UButton;
class USoccerMatchHistoryWidget;
class USoccerTrainingSelectorWidget6B;

/** Front door for all game modes; independent from the soccer match map. */
UCLASS()
class THIRDPERSONCPP_API USoccerMainMenuWidget6A : public UUserWidget
{
    GENERATED_BODY()

public:
    void FocusFirstAction6A();
    void OpenTrainingSelector6B();

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual FReply NativeOnPreviewKeyDown(
        const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
    virtual FReply NativeOnAnalogValueChanged(
        const FGeometry& InGeometry, const FAnalogInputEvent& InAnalogInputEvent) override;

private:
    void BuildMenu6A();
    void Navigate6A(int32 Step);
    void UpdateFocusColors6A();

    UFUNCTION() void HandlePlay6A();
    UFUNCTION() void HandleTraining6B();
    UFUNCTION() void HandleTrainingClosed6B();
    UFUNCTION() void HandleHistory6A();
    UFUNCTION() void HandleHistoryClosed6A();
    UFUNCTION() void HandleQuit6A();

    UPROPERTY(Transient) UButton* PlayButton6A = nullptr;
    UPROPERTY(Transient) UButton* TrainingButton6B = nullptr;
    UPROPERTY(Transient) UButton* HistoryButton6A = nullptr;
    UPROPERTY(Transient) UButton* QuitButton6A = nullptr;
    UPROPERTY(Transient) USoccerMatchHistoryWidget* HistoryWidget6A = nullptr;
    UPROPERTY(Transient) USoccerTrainingSelectorWidget6B* TrainingWidget6B = nullptr;

    double LastAnalogNavigation6A = -1.0;
};
