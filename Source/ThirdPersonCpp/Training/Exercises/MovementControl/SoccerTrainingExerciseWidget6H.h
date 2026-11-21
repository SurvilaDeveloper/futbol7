#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SoccerTrainingExerciseWidget6H.generated.h"

class UTextBlock;
class UButton;

/** Start instructions and final result for the movement / ball control lesson. */
UCLASS()
class THIRDPERSONCPP_API USoccerTrainingExerciseWidget6H : public UUserWidget
{
    GENERATED_BODY()
public:
    void ShowInstructions6H();
    void ShowResults6H();
    void FocusPrimary6H();
protected:
    virtual void NativeOnInitialized() override;
    virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry,
        const FKeyEvent& KeyEvent) override;
    virtual FReply NativeOnAnalogValueChanged(const FGeometry& Geometry,
        const FAnalogInputEvent& AnalogEvent) override;
    virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
private:
    void Navigate6H(int32 Offset);
    void ActivateSelected6H();
    UFUNCTION() void PrimaryClicked6H();
    UFUNCTION() void BackClicked6H();
    UPROPERTY(Transient) UTextBlock* Header6H = nullptr;
    UPROPERTY(Transient) UTextBlock* Body6H = nullptr;
    UPROPERTY(Transient) UTextBlock* Feedback6H = nullptr;
    UPROPERTY(Transient) UTextBlock* PrimaryLabel6H = nullptr;
    UPROPERTY(Transient) UButton* PrimaryButton6H = nullptr;
    UPROPERTY(Transient) UButton* BackButton6H = nullptr;
    double LastAnalogTime6H = -1.0;
    bool bResults6H = false;
};
