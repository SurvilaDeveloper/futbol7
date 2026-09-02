#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SoccerClubSelectionWidget.generated.h"

class ASoccerMatchManager;
class USoccerGameInstance;
class UButton;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSoccerClubSelectionConfirmed);

/** Standalone pre-match screen that creates an independent match setup. */
UCLASS()
class THIRDPERSONCPP_API USoccerClubSelectionWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable)
    FSoccerClubSelectionConfirmed OnSelectionConfirmed;

    void InitializeForMatchManager(ASoccerMatchManager* InMatchManager);
    bool HasSelectableOpponent() const;
    void ActivateMenu(bool bPauseGame);

protected:
    virtual void NativeOnInitialized() override;
    virtual FReply NativeOnPreviewKeyDown(
        const FGeometry& InGeometry,
        const FKeyEvent& InKeyEvent
    ) override;

private:
    void BuildWidgetTree();
    void RefreshAvailableClubs();
    void RefreshTexts();
    void CycleOpponent(int32 Direction);
    FString GetClubDisplayName(FName ClubId) const;
    void CloseMenu();

    UFUNCTION()
    void HandlePreviousClicked();

    UFUNCTION()
    void HandleNextClicked();

    UFUNCTION()
    void HandleConfirmClicked();

    UPROPERTY(Transient)
    ASoccerMatchManager* MatchManager = nullptr;

    UPROPERTY(Transient)
    USoccerGameInstance* SoccerGameInstance = nullptr;

    UPROPERTY(Transient)
    UTextBlock* HumanClubText = nullptr;

    UPROPERTY(Transient)
    UTextBlock* OpponentClubText = nullptr;

    UPROPERTY(Transient)
    UTextBlock* StatusText = nullptr;

    UPROPERTY(Transient)
    UButton* ConfirmButton = nullptr;

    TArray<FName> SelectableOpponentClubIds;
    int32 SelectedOpponentIndex = INDEX_NONE;
    bool bAppliedGamePause = false;
    bool bPreviousMouseCursorVisible = false;
};
