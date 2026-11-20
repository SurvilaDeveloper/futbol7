#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SoccerPeriodSummaryWidget.generated.h"

class ASoccerMatchManager;
struct FSoccerPlayerMatchStatistics5G;
class UButton;
class UHorizontalBox;
class USizeBox;
class UScrollBox;
class UTextBlock;

/** Stage 5F: fully native, interactive half-time / full-time statistics screen. */
UCLASS()
class THIRDPERSONCPP_API USoccerPeriodSummaryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeForMatch(ASoccerMatchManager* InMatchManager);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnPreviewKeyDown(
		const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	void BuildScreen5F();
	void RefreshStatistics5F();
	void RefreshContinueHint5F();
	void AddStatisticRow5F(const FString& Label, const FString& LocalValue,
		const FString& RivalValue, int32 RowIndex);
    void RefreshPlayerStatistics5G();
    void AddPlayerStatisticsRow5G(const FSoccerPlayerMatchStatistics5G& Row,
        int32 RowIndex, bool bBestPlayer5J);
    void ChangeSummaryTab5G(bool bPlayers);
    void RefreshTimeline5I();
    void RefreshFinalReport5K();
    UFUNCTION()
    void ShowFinalReport5K();
    UFUNCTION()
    void ShowTimeline5I();
    void ChangeIndividualPage5H(bool bDefensive);
    UFUNCTION()
    void ShowAttackIndividual5H();
    UFUNCTION()
    void ShowDefenseIndividual5H();
    UFUNCTION()
    void ShowTeamStatistics5G();
    UFUNCTION()
    void ShowPlayerStatistics5G();

	UFUNCTION()
	void HandleContinueClicked5F();

	UPROPERTY(Transient)
	ASoccerMatchManager* MatchManager = nullptr;

	UPROPERTY(Transient)
	UTextBlock* HeadingText = nullptr;
	UPROPERTY(Transient)
	UTextBlock* ClubsText = nullptr;
	UPROPERTY(Transient)
	UTextBlock* ScoreText = nullptr;
	UPROPERTY(Transient)
	UTextBlock* PossessionText = nullptr;
	UPROPERTY(Transient)
	UTextBlock* ContinueLabel = nullptr;
	UPROPERTY(Transient)
	UTextBlock* StatusText = nullptr;
	UPROPERTY(Transient)
	UScrollBox* StatisticsScroll = nullptr;
    UPROPERTY(Transient)
    UButton* TeamTabButton5G = nullptr;
    UPROPERTY(Transient)
    UButton* PlayerTabButton5G = nullptr;
    UPROPERTY(Transient)
    UButton* TimelineTabButton5I = nullptr;
    UPROPERTY(Transient)
    UButton* ReportTabButton5K = nullptr;
    bool bShowingReport5K = false;
    bool bShowingTimeline5I = false;
    bool bShowingPlayers5G = false;
    bool bShowingDefense5H = false;
    UPROPERTY(Transient)
    UHorizontalBox* IndividualPages5H = nullptr;
    UPROPERTY(Transient)
    UButton* AttackingPageButton5H = nullptr;
    UPROPERTY(Transient)
    UButton* DefensivePageButton5H = nullptr;
    UPROPERTY(Transient)
    USizeBox* StatisticsHeightBox5H = nullptr;

	UPROPERTY(Transient)
	UButton* ContinueButton = nullptr;
};
