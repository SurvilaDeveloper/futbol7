#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SoccerPeriodSummaryWidget.generated.h"

class ASoccerMatchManager;
class UButton;
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
	UButton* ContinueButton = nullptr;
};
