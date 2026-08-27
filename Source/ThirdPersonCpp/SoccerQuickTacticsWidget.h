#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SoccerQuickTacticsWidget.generated.h"

class UBorder;
class UButton;
class UTextBlock;
class USoccerTacticalPresetManager;

/*
 * Compact in-match selector for the four persistent tactical quick slots.
 *
 * It intentionally does not pause the world: the player opens it, chooses one
 * of four already-prepared strategies and confirms. While visible it owns UI
 * input so gameplay actions do not fire behind D-Pad/Enter/A navigation.
 */
UCLASS()
class THIRDPERSONCPP_API USoccerQuickTacticsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeForPresetManager(
		USoccerTacticalPresetManager* InTacticalPresetManager
	);

	void ActivateMenu();
	void CloseMenu();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual FReply NativeOnPreviewKeyDown(
		const FGeometry& InGeometry,
		const FKeyEvent& InKeyEvent
	) override;

private:
	void BuildWidgetTree();
	void RefreshFromPresetManager();
	void SelectQuickSlot(int32 NewQuickSlotIndex);
	void MoveSelection(int32 Direction);
	void ApplySelectedQuickSlot();
	void NotifyHUD(const FString& Message) const;

	UFUNCTION()
	void HandleQuickSlot1Clicked();
	UFUNCTION()
	void HandleQuickSlot2Clicked();
	UFUNCTION()
	void HandleQuickSlot3Clicked();
	UFUNCTION()
	void HandleQuickSlot4Clicked();
	UFUNCTION()
	void HandleApplyClicked();
	UFUNCTION()
	void HandleCloseClicked();

	UPROPERTY()
	USoccerTacticalPresetManager* TacticalPresetManager = nullptr;

	UPROPERTY()
	TArray<UButton*> QuickSlotButtons;

	UPROPERTY()
	TArray<UTextBlock*> QuickSlotTexts;

	UPROPERTY()
	UTextBlock* ActivePresetText = nullptr;

	UPROPERTY()
	UTextBlock* StatusText = nullptr;

	UPROPERTY()
	UButton* ApplyButton = nullptr;

	UPROPERTY()
	UButton* CloseButton = nullptr;

	int32 SelectedQuickSlotIndex = 0;
	bool bPreviousMouseCursorVisible = false;
};
