#pragma once

#include "CoreMinimal.h"
#include "SoccerUIFeedbackAnimations.h"
#include "SoccerUINavigation.h"
#include "Blueprint/UserWidget.h"
#include "SoccerClubSelectionWidget.generated.h"

class ASoccerMatchManager;
class USoccerGameInstance;
class USoccerClubProfile;
class USoccerClubIdButton;
class USoccerSquadViewerWidget;
class USoccerMatchHistoryWidget;
class UButton;
class UBorder;
class UTextBlock;
class UUniformGridPanel;
class USizeBox;
class UCanvasPanelSlot;
class USoccerUIStyleEditorController;

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
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual FReply NativeOnPreviewKeyDown(
        const FGeometry& InGeometry,
        const FKeyEvent& InKeyEvent
    ) override;
    virtual FReply NativeOnAnalogValueChanged(
        const FGeometry& InGeometry, const FAnalogInputEvent& InAnalogInputEvent) override;

private:
    void BuildWidgetTree();
    void RefreshAvailableClubs();
    void RebuildClubGrid();
    void RefreshTexts();
    void CycleOpponent(int32 Direction);
    void NavigateOpponentGrid(int32 Direction);
    void FocusSelectedClub();
    FString GetClubDisplayName(FName ClubId) const;
    USoccerClubProfile* GetClubProfile(FName ClubId) const;
    void RefreshCardSelectionStates();
    void CloseMenu();
    void ApplyLiveStyle();

    UFUNCTION() void HandleStyleApplied();
    void OpenSquadViewer();

    UFUNCTION()
    void HandleViewSquadsClicked();
    UFUNCTION()
    void HandleHistoryClicked();
    UFUNCTION()
    void HandleHistoryClosed();

    UFUNCTION()
    void HandleSquadViewerClosed();

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

    UPROPERTY(Transient) USoccerUIStyleEditorController* StyleEditor = nullptr;
    UPROPERTY(Transient) UBorder* LiveBackdrop = nullptr;
    UPROPERTY(Transient) UBorder* LiveMainPanel = nullptr;
    UPROPERTY(Transient) USizeBox* LiveMainSize = nullptr;
    UPROPERTY(Transient) USizeBox* LiveGridViewportSize = nullptr;
    UPROPERTY(Transient) UCanvasPanelSlot* LivePanelCanvasSlot = nullptr;
    UPROPERTY(Transient) UTextBlock* HumanClubText = nullptr;

    UPROPERTY(Transient)
    UTextBlock* OpponentClubText = nullptr;

    UPROPERTY(Transient)
    UUniformGridPanel* ClubGrid = nullptr;

    UPROPERTY(Transient)
    USoccerSquadViewerWidget* SquadViewerWidget = nullptr;
    UPROPERTY(Transient)
    USoccerMatchHistoryWidget* HistoryWidget = nullptr;

    UPROPERTY(Transient)
    UTextBlock* StatusText = nullptr;

    UPROPERTY(Transient)
    UButton* ViewSquadsButton = nullptr;
    UPROPERTY(Transient)
    UButton* HistoryButton = nullptr;

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
    int32 RenderedGridColumnCount = 1;
    FSoccerMenuAnalogRepeat AnalogNavigation;
    bool bAppliedGamePause = false;
    bool bPreviousMouseCursorVisible = false;
    FVector2D LastResponsiveViewport = FVector2D::ZeroVector;
    FSoccerUIFeedbackAnimations UIAnimations;
};
