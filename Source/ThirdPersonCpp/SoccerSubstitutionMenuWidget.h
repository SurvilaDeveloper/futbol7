#pragma once

#include "CoreMinimal.h"
#include "SoccerUIFeedbackAnimations.h"
#include "SoccerUINavigation.h"
#include "Blueprint/UserWidget.h"
#include "SoccerSubstitutionMenuWidget.generated.h"

class ASoccerMatchManager;
class UButton;
class UTextBlock;
class UBorder;
class UCanvasPanelSlot;
class USoccerUIStyleEditorController;

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
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnPreviewKeyDown(
		const FGeometry& InGeometry,
		const FKeyEvent& InKeyEvent
	) override;
    virtual FReply NativeOnAnalogValueChanged(const FGeometry& InGeometry,
        const FAnalogInputEvent& InAnalogInputEvent) override;

private:
	void BuildWidgetTree();
	void RefreshPlayerLists(bool bPreserveSelection);
	void RefreshDisplay();
	void CycleOutgoing(int32 Direction);
	void CycleIncoming(int32 Direction);
    void UpdateSelectorFocus(bool bSetKeyboardFocus);
	void ConfirmSubstitution();
	void CancelPendingSubstitution();
	FString DescribePlayer(FName PlayerId) const;
	FString DescribeOutgoingSelection() const;
	FString DescribeIncomingSelection() const;
	float GetSelectedOutgoingEnergyPercent() const;
	int32 GetSelectedIncomingSuitability() const;
	void SetStatus(const FString& Message, bool bError = false);
	void ApplyLiveStyle();
	UFUNCTION() void HandleStyleApplied();

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

	UPROPERTY(Transient) USoccerUIStyleEditorController* StyleEditor = nullptr;
	UPROPERTY(Transient) UBorder* LiveMainPanel = nullptr;
	UPROPERTY(Transient) UCanvasPanelSlot* LiveMainCanvasSlot = nullptr;
	UPROPERTY(Transient) TArray<UBorder*> LiveSelectorCards;
	UPROPERTY() ASoccerMatchManager* MatchManager = nullptr;

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
	UButton* OutgoingNextButton = nullptr;
	UPROPERTY()
	UButton* IncomingNextButton = nullptr;
	UPROPERTY()
	UButton* ConfirmButton = nullptr;
	UPROPERTY()
	UButton* CancelButton = nullptr;

    UPROPERTY()
    UButton* CloseMenuButton = nullptr;

	TArray<FName> ActivePlayerIds;
	TArray<FName> ActiveSlotIds;
	TArray<FName> BenchPlayerIds;
	int32 SelectedOutgoingIndex = INDEX_NONE;
	int32 SelectedIncomingIndex = INDEX_NONE;
	int32 FocusedSelectorIndex = 0;
    FSoccerMenuAnalogRepeat AnalogNavigation;

	bool bAppliedGamePause = false;
	bool bPreviousMouseCursorVisible = false;
    FVector2D LastResponsiveViewport = FVector2D::ZeroVector;
    FSoccerUIFeedbackAnimations UIAnimations;
};
