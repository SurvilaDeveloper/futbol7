#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SoccerClubSelectionWidget.generated.h"

class ASoccerMatchManager;
class USoccerGameInstance;
class USoccerClubProfile;
class USoccerClubIdButton;
class UButton;
class UBorder;
class UTextBlock;
class UUniformGridPanel;

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
    void RebuildClubGrid();
    void RefreshTexts();
    void CycleOpponent(int32 Direction);
    FString GetClubDisplayName(FName ClubId) const;
    USoccerClubProfile* GetClubProfile(FName ClubId) const;
    void RefreshCardSelectionStates();
    void CloseMenu();

    UFUNCTION()
    void HandlePreviousClicked();

    UFUNCTION()
    void HandleNextClicked();

    UFUNCTION()
    void HandleClubCardClicked(FName ClubId);

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
    UUniformGridPanel* ClubGrid = nullptr;

    UPROPERTY(Transient)
    UTextBlock* StatusText = nullptr;

    UPROPERTY(Transient)
    UButton* ConfirmButton = nullptr;

    UPROPERTY(Transient)
    TMap<FName, USoccerClubIdButton*> ClubButtonsById;

    UPROPERTY(Transient)
    TMap<FName, UBorder*> ClubCardBordersById;

    UPROPERTY(EditAnywhere, Category = "Soccer|Club Selection|Layout", meta = (ClampMin = "1", ClampMax = "8"))
    int32 ClubGridColumnCount = 4;

    UPROPERTY(EditAnywhere, Category = "Soccer|Club Selection|Layout", meta = (ClampMin = "120.0"))
    float ClubCardWidth = 205.0f;

    UPROPERTY(EditAnywhere, Category = "Soccer|Club Selection|Layout", meta = (ClampMin = "140.0"))
    float ClubCardHeight = 225.0f;

    UPROPERTY(EditAnywhere, Category = "Soccer|Club Selection|Layout", meta = (ClampMin = "64.0"))
    float ClubCrestSize = 132.0f;

    TArray<FName> SelectableOpponentClubIds;
    int32 SelectedOpponentIndex = INDEX_NONE;
    bool bAppliedGamePause = false;
    bool bPreviousMouseCursorVisible = false;
};
