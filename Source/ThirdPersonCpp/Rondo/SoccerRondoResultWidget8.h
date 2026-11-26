#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SoccerRondoResultWidget8.generated.h"

class UButton;
class UTextBlock;

/** Interactive final ranking, independent of match and training results. */
UCLASS()
class THIRDPERSONCPP_API USoccerRondoResultWidget8 : public UUserWidget
{
    GENERATED_BODY()
public:
    void FocusFirstAction();

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
    virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry,
        const FKeyEvent& KeyEvent) override;
    virtual FReply NativeOnAnalogValueChanged(const FGeometry& Geometry,
        const FAnalogInputEvent& AnalogEvent) override;

private:
    void BuildResult();
    void ToggleFocus();
    void TravelTo(const FString& MapPackage);
    UFUNCTION() void Replay();
    UFUNCTION() void MainMenu();

    UPROPERTY(Transient) UButton* ReplayButton = nullptr;
    UPROPERTY(Transient) UButton* MainMenuButton = nullptr;
    UPROPERTY(Transient) UTextBlock* ErrorLabel = nullptr;
    bool bTravelRequested = false;
    double LastAnalogNavigation = -1.0;
};
