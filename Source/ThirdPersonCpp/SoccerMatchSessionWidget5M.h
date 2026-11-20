#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SoccerMatchSessionWidget5M.generated.h"

class UButton;
class UTextBlock;

/** Native UI for pause/abort and playing again. Never saves anything itself. */
UCLASS()
class THIRDPERSONCPP_API USoccerMatchSessionWidget5M : public UUserWidget
{
    GENERATED_BODY()
public:
    void ShowPaused5M();
    void ShowAborted5M();
    void ShowFinished5M();

protected:
    virtual void NativeOnInitialized() override;
    virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry,
        const FKeyEvent& KeyEvent) override;

private:
    void BuildScreen5M();
    void SetMode5M(int32 NewMode);
    UFUNCTION() void HandleResume5M();
    UFUNCTION() void HandleAbort5M();
    UFUNCTION() void HandleConfirmAbort5M();
    UFUNCTION() void HandleCancelAbort5M();
    UFUNCTION() void HandlePlay5M();
    UFUNCTION() void HandleSeeField5M();

    UPROPERTY(Transient) UTextBlock* TitleText5M = nullptr;
    UPROPERTY(Transient) UTextBlock* HintText5M = nullptr;
    UPROPERTY(Transient) UButton* ResumeButton5M = nullptr;
    UPROPERTY(Transient) UButton* AbortButton5M = nullptr;
    UPROPERTY(Transient) UButton* ConfirmAbortButton5M = nullptr;
    UPROPERTY(Transient) UButton* CancelAbortButton5M = nullptr;
    UPROPERTY(Transient) UButton* PlayButton5M = nullptr;
    UPROPERTY(Transient) UButton* SeeFieldButton5M = nullptr;
    int32 CurrentMode5M = 0; // 0 paused, 1 confirm, 2 aborted, 3 finished
};
