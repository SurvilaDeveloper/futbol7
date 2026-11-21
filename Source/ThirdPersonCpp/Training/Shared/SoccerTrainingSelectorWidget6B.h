#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SoccerTrainingSelectorWidget6B.generated.h"

class UButton;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSoccerTrainingSelectorClosed6B);

/** Exercise selection, hosted over the main menu level, never over a match. */
UCLASS()
class THIRDPERSONCPP_API USoccerTrainingSelectorWidget6B : public UUserWidget
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable, Category="Training|6B")
    FSoccerTrainingSelectorClosed6B OnTrainingSelectorClosed6B;

    void FocusFirstExercise6B();

protected:
    virtual void NativeOnInitialized() override;
    virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry,
        const FKeyEvent& KeyEvent) override;
    virtual FReply NativeOnAnalogValueChanged(const FGeometry& Geometry,
        const FAnalogInputEvent& AnalogEvent) override;
    virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;

private:
    void BuildSelector6B();
    void Navigate6B(int32 Offset);

    UFUNCTION() void OpenMovementControl6B();
    UFUNCTION() void Back6B();

    UPROPERTY(Transient) UButton* MovementButton6B = nullptr;
    UPROPERTY(Transient) UButton* BackButton6B = nullptr;
    double LastAnalogTime6B = -1.0;
};
