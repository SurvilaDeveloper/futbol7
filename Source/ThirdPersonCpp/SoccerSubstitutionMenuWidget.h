#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SoccerSubstitutionMenuWidget.generated.h"

class ASoccerMatchManager;
class UButton;
class UTextBlock;

/** Native in-match substitution screen for the human-controlled team. */
UCLASS()
class THIRDPERSONCPP_API USoccerSubstitutionMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeForMatchManager(ASoccerMatchManager* InMatchManager);
	void ActivateMenu(bool bPauseGame = true);
	void CloseMenu();

protected:
	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnPreviewKeyDown(
		const FGeometry& InGeometry,
		const FKeyEvent& InKeyEvent
	) override;

private:
	void BuildWidgetTree();
	void RefreshPlayerLists(bool bPreserveSelection);
	void RefreshDisplay();
	void CycleOutgoing(int32 Direction);
	void CycleIncoming(int32 Direction);
	void ConfirmSubstitution();
	void CancelPendingSubstitution();
	FString DescribePlayer(FName PlayerId) const;
	FString DescribeOutgoingSelection() const;
	FString DescribeIncomingSelection() const;
	float GetSelectedOutgoingEnergyPercent() const;
	int32 GetSelectedIncomingSuitability() const;
	void SetStatus(const FString& Message, bool bError = false);

	UFUNCTION()
	void HandleOutgoingPrevious();
	UFUNCTION()
	void HandleOutgoingNext();
	UFUNCTION()
	void HandleIncomingPrevious();
	UFUNCTION()
	void HandleIncomingNext();
	UFUNCTION()
	void HandleConfirmClicked();
	UFUNCTION()
	void HandleCancelClicked();
	UFUNCTION()
	void HandleCloseClicked();

	UPROPERTY()
	ASoccerMatchManager* MatchManager = nullptr;

	UPROPERTY()
	UTextBlock* SummaryText = nullptr;
	UPROPERTY()
	UTextBlock* OutgoingValueText = nullptr;
	UPROPERTY()
	UTextBlock* IncomingValueText = nullptr;
	UPROPERTY()
	UTextBlock* ComparisonText = nullptr;
	UPROPERTY()
	UTextBlock* PendingText = nullptr;
	UPROPERTY()
	UTextBlock* StatusText = nullptr;
	UPROPERTY()
	UButton* ConfirmButton = nullptr;
	UPROPERTY()
	UButton* CancelButton = nullptr;

	TArray<FName> ActivePlayerIds;
	TArray<FName> ActiveSlotIds;
	TArray<FName> BenchPlayerIds;
	int32 SelectedOutgoingIndex = INDEX_NONE;
	int32 SelectedIncomingIndex = INDEX_NONE;
	int32 FocusedSelectorIndex = 0;

	bool bAppliedGamePause = false;
	bool bPreviousMouseCursorVisible = false;
};
