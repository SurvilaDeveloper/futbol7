#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/ComboBoxString.h"
#include "SoccerMatchHistory5L.h"
#include "SoccerHistoricalStats5M.h"
#include "SoccerMatchHistoryWidget.generated.h"

class UButton;
class UScrollBox;
class UTextBlock;
class UWidgetTree;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSoccerHistoryClosed5L);

/** Read-only match archive opened from the pre-match club selection. */
UCLASS()
class THIRDPERSONCPP_API USoccerMatchHistoryWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable)
    FSoccerHistoryClosed5L OnHistoryClosed;

    void OpenHistory();

protected:
    virtual void NativeOnInitialized() override;
    virtual FReply NativeOnPreviewKeyDown(
        const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
    void BuildHistoryScreen();
    void SwitchMatch(int32 Delta);
    void SwitchPage(int32 NewPage);
    void RefreshContents();
    void AddDarkLine(const FString& Text, int32 FontSize = 15,
        bool bAccent = false);

    UFUNCTION() void HandlePrevious();
    UFUNCTION() void HandleNext();
    UFUNCTION() void HandleClose();
    UFUNCTION() void HandleReport();
    UFUNCTION() void HandlePlayers();
    UFUNCTION() void HandleEvents();
    UFUNCTION() void HandleTotals5M();
    UFUNCTION() void HandleSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

    UPROPERTY(Transient) UComboBoxString* MatchSelector = nullptr;
    UPROPERTY(Transient) UScrollBox* Details = nullptr;
    UPROPERTY(Transient) UTextBlock* SummaryHeading = nullptr;
    UPROPERTY(Transient) UTextBlock* RecordCount = nullptr;
    UPROPERTY(Transient) UButton* ReportButton = nullptr;
    UPROPERTY(Transient) UButton* PlayersButton = nullptr;
    UPROPERTY(Transient) UButton* EventsButton = nullptr;
    UPROPERTY(Transient) UButton* TotalsButton5M = nullptr;
    TArray<FSoccerClubTotals5M> TotalClubs5M;
    TArray<FSoccerPlayerTotals5M> TotalPlayers5M;
    int32 TotalMatches5M = 0;
    TArray<FSoccerArchivedMatch5L> Matches;
    int32 SelectedIndex = 0;
    int32 ActivePage = 0;
    bool bChangingOptions = false;
};
