#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SoccerFormationTypes.h"
#include "SoccerTacticTypes.h"
#include "SoccerPlayerInstructionTypes.h"
#include "SoccerFormationMenuWidget.generated.h"

class ASoccerMatchManager;
class USoccerTacticalPresetManager;
class UBorder;
class UButton;
class UCanvasPanel;
class UEditableTextBox;
class USizeBox;
class UTextBlock;
class UVerticalBox;
class UWidgetSwitcher;

/*
 * Native coach menu for the human-controlled team.
 *
 * Stage 12A replaced the old dense form/ComboBox layout with a tabbed,
 * gamepad-oriented presentation. Stage 12B gives this widget exclusive input
 * ownership while it is open: gameplay uses GameOnly, this menu uses UIOnly,
 * and the same M / Gamepad Menu-Options toggle closes it from inside the UI.
 * Stage 12C adds device-aware help, clearer focus feedback and contextual page
 * guidance without changing the tactics/runtime APIs. Formation, collective
 * tactics and slot instructions remain owned by ASoccerMatchManager.
 */
UCLASS()
class THIRDPERSONCPP_API USoccerFormationMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeForMatchManager(
		ASoccerMatchManager* InMatchManager,
		USoccerTacticalPresetManager* InTacticalPresetManager = nullptr
	);

	void ActivateMenu(bool bPauseGame);

	void CloseMenu();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual FReply NativeOnPreviewKeyDown(
		const FGeometry& InGeometry,
		const FKeyEvent& InKeyEvent
	) override;
	virtual FReply NativeOnAnalogValueChanged(
		const FGeometry& InGeometry,
		const FAnalogInputEvent& InAnalogInputEvent
	) override;
	virtual FReply NativeOnMouseMove(
		const FGeometry& InGeometry,
		const FPointerEvent& InMouseEvent
	) override;

private:
	enum class ECoachMenuTab : uint8
	{
		Presets,
		Formation,
		Tactics,
		Instructions
	};

	enum class ECoachMenuInputDevice : uint8
	{
		KeyboardMouse,
		Gamepad
	};

	struct FSelectorRow
	{
		UBorder* Border = nullptr;
		UTextBlock* LabelText = nullptr;
		UButton* PreviousButton = nullptr;
		UTextBlock* ValueText = nullptr;
		UButton* NextButton = nullptr;
	};

	void BuildWidgetTree();
	void BuildPresetsPage(UVerticalBox* PageRoot);
	void BuildFormationPage(UVerticalBox* PageRoot);
	void BuildTacticsPage(UVerticalBox* PageRoot);
	void BuildInstructionsPage(UVerticalBox* PageRoot);
	FSelectorRow BuildSelectorRow(
		UVerticalBox* Parent,
		const FString& Label,
		float LabelWidth = 190.0f,
		float ValueWidth = 210.0f
	);

	void RefreshFromMatchManager();
	void RefreshHeader();
	void RefreshPresetsPage(bool bCopySelectedNameToEditor = false);
	void RefreshQuickPresetAssignments();
	void RefreshFormationPage();
	void RefreshFormationPreview(ESoccerFormationSystem FormationSystem);
	void RefreshFormationPreviewCanvas(
		UCanvasPanel* PreviewCanvas,
		ESoccerFormationSystem FormationSystem,
		float CanvasWidth,
		float CanvasHeight,
		bool bCompact
	);
	void RefreshTacticsPage();
	void RefreshInstructionsPage();
	void RefreshTabVisuals();
	void RefreshFocusVisuals();
	void RefreshControlHints();
	void SetLastInputDevice(ECoachMenuInputDevice NewInputDevice);
	void SetTabGuidanceMessage();

	void SetActiveTab(ECoachMenuTab NewTab, bool bResetFocus = true);
	void MoveFocus(int32 Direction);
	void CycleFocusedValue(int32 Direction);
	TArray<FSelectorRow*> GetActiveSelectorRows();

	void CycleBuiltInPreset(int32 Direction);
	void CyclePreset(int32 Direction);
	void CycleJsonExchangeFile(int32 Direction);
	void RefreshJsonExchangeFiles(bool bPreserveSelection = true);
	void HandleQuickPresetSlotClicked(int32 QuickSlotIndex);
	void CycleFormation(int32 Direction);
	void CycleBuildUp(int32 Direction);
	void CycleAttackChannel(int32 Direction);
	void CycleAttackingWidth(int32 Direction);
	void CycleAttackingTempo(int32 Direction);
	void CycleAttackingTransition(int32 Direction);
	void CycleDefensiveBlock(int32 Direction);
	void CyclePressingIntensity(int32 Direction);
	void CycleMarkingStyle(int32 Direction);
	void CycleDefensiveTransition(int32 Direction);
	void CycleInstructionSlot(int32 Direction);
	void CycleIndividualAttack(int32 Direction);
	void CycleIndividualDefense(int32 Direction);
	void CycleMarkingTarget(int32 Direction);

	void ApplyPlayerTacticalPlan(
		const FSoccerTeamTacticalPlan& TacticalPlan,
		const FString& StatusMessage
	);
	void ApplySelectedSlotInstruction(
		const FSoccerSlotTacticalInstruction& Instruction,
		const FString& StatusMessage
	);

	void EnsureSelectedInstructionSlotValid();
	TArray<FName> GetCurrentPlayerInstructionSlotIds() const;
	TArray<FName> GetCurrentMarkingTargetIds() const;

	FString GetFormationDisplayName(ESoccerFormationSystem FormationSystem) const;
	FString GetSelectedInstructionPlayerName() const;
	FString GetPresetNameEditorText() const;
	void EnsureSelectedPresetValid();
	void SetStatusMessage(const FString& Message);
	void OpenPresetDeleteConfirmation();
	void ClosePresetDeleteConfirmation();
	void ConfirmPendingPresetDeletion();

	UFUNCTION()
	void HandlePresetsTabClicked();
	UFUNCTION()
	void HandleFormationTabClicked();
	UFUNCTION()
	void HandleTacticsTabClicked();
	UFUNCTION()
	void HandleInstructionsTabClicked();

	UFUNCTION()
	void HandleBuiltInPresetPrevious();
	UFUNCTION()
	void HandleBuiltInPresetNext();
	UFUNCTION()
	void HandlePresetPrevious();
	UFUNCTION()
	void HandlePresetNext();
	UFUNCTION()
	void HandlePresetApplyClicked();
	UFUNCTION()
	void HandlePresetSaveCurrentClicked();
	UFUNCTION()
	void HandlePresetOverwriteClicked();
	UFUNCTION()
	void HandlePresetRenameClicked();
	UFUNCTION()
	void HandlePresetDuplicateClicked();
	UFUNCTION()
	void HandlePresetMoveUpClicked();
	UFUNCTION()
	void HandlePresetMoveDownClicked();
	UFUNCTION()
	void HandlePresetJsonPreviousClicked();
	UFUNCTION()
	void HandlePresetJsonNextClicked();
	UFUNCTION()
	void HandlePresetJsonExportClicked();
	UFUNCTION()
	void HandlePresetJsonImportClicked();
	UFUNCTION()
	void HandlePresetJsonRefreshClicked();
	UFUNCTION()
	void HandlePresetRevertClicked();
	UFUNCTION()
	void HandlePresetDeleteClicked();
	UFUNCTION()
	void HandlePresetDeleteConfirmClicked();
	UFUNCTION()
	void HandlePresetDeleteCancelClicked();
	UFUNCTION()
	void HandleQuickPresetSlot1Clicked();
	UFUNCTION()
	void HandleQuickPresetSlot2Clicked();
	UFUNCTION()
	void HandleQuickPresetSlot3Clicked();
	UFUNCTION()
	void HandleQuickPresetSlot4Clicked();

	UFUNCTION()
	void HandleFormationPrevious();
	UFUNCTION()
	void HandleFormationNext();
	UFUNCTION()
	void HandleBuildUpPrevious();
	UFUNCTION()
	void HandleBuildUpNext();
	UFUNCTION()
	void HandleAttackChannelPrevious();
	UFUNCTION()
	void HandleAttackChannelNext();
	UFUNCTION()
	void HandleAttackingWidthPrevious();
	UFUNCTION()
	void HandleAttackingWidthNext();
	UFUNCTION()
	void HandleAttackingTempoPrevious();
	UFUNCTION()
	void HandleAttackingTempoNext();
	UFUNCTION()
	void HandleAttackingTransitionPrevious();
	UFUNCTION()
	void HandleAttackingTransitionNext();
	UFUNCTION()
	void HandleDefensiveBlockPrevious();
	UFUNCTION()
	void HandleDefensiveBlockNext();
	UFUNCTION()
	void HandlePressingIntensityPrevious();
	UFUNCTION()
	void HandlePressingIntensityNext();
	UFUNCTION()
	void HandleMarkingStylePrevious();
	UFUNCTION()
	void HandleMarkingStyleNext();
	UFUNCTION()
	void HandleDefensiveTransitionPrevious();
	UFUNCTION()
	void HandleDefensiveTransitionNext();
	UFUNCTION()
	void HandleInstructionSlotPrevious();
	UFUNCTION()
	void HandleInstructionSlotNext();
	UFUNCTION()
	void HandleIndividualAttackPrevious();
	UFUNCTION()
	void HandleIndividualAttackNext();
	UFUNCTION()
	void HandleIndividualDefensePrevious();
	UFUNCTION()
	void HandleIndividualDefenseNext();
	UFUNCTION()
	void HandleMarkingTargetPrevious();
	UFUNCTION()
	void HandleMarkingTargetNext();

	UFUNCTION()
	void HandleCloseClicked();

	UPROPERTY()
	ASoccerMatchManager* MatchManager = nullptr;

	UPROPERTY()
	USoccerTacticalPresetManager* TacticalPresetManager = nullptr;

	UPROPERTY()
	UWidgetSwitcher* PageSwitcher = nullptr;

	UPROPERTY()
	UButton* PresetsTabButton = nullptr;
	UPROPERTY()
	UButton* FormationTabButton = nullptr;
	UPROPERTY()
	UButton* TacticsTabButton = nullptr;
	UPROPERTY()
	UButton* InstructionsTabButton = nullptr;

	UPROPERTY()
	UTextBlock* HeaderContextText = nullptr;
	UPROPERTY()
	UTextBlock* HeaderOpponentText = nullptr;

	UPROPERTY()
	UEditableTextBox* PresetNameTextBox = nullptr;
	UPROPERTY()
	UTextBlock* PresetFormationSummaryText = nullptr;
	UPROPERTY()
	UTextBlock* PresetAttackSummaryText = nullptr;
	UPROPERTY()
	UTextBlock* PresetDefenseSummaryText = nullptr;
	UPROPERTY()
	UTextBlock* PresetInstructionSummaryText = nullptr;
	UPROPERTY()
	USizeBox* PresetFormationPreviewSizeBox = nullptr;
	UPROPERTY()
	UCanvasPanel* PresetFormationPreviewCanvas = nullptr;
	UPROPERTY()
	UTextBlock* PresetSelectionOriginText = nullptr;
	UPROPERTY()
	UTextBlock* ActivePresetText = nullptr;
	UPROPERTY()
	UTextBlock* ActivePresetStateText = nullptr;
	UPROPERTY()
	UButton* PresetRevertButton = nullptr;
	UPROPERTY()
	UButton* PresetApplyButton = nullptr;
	UPROPERTY()
	UButton* PresetSaveCurrentButton = nullptr;
	UPROPERTY()
	UButton* PresetOverwriteButton = nullptr;
	UPROPERTY()
	UButton* PresetRenameButton = nullptr;
	UPROPERTY()
	UButton* PresetDuplicateButton = nullptr;
	UPROPERTY()
	UButton* PresetMoveUpButton = nullptr;
	UPROPERTY()
	UButton* PresetMoveDownButton = nullptr;
	UPROPERTY()
	UButton* PresetJsonExportButton = nullptr;
	UPROPERTY()
	UButton* PresetJsonImportButton = nullptr;
	UPROPERTY()
	UButton* PresetJsonRefreshButton = nullptr;
	UPROPERTY()
	UButton* PresetDeleteButton = nullptr;

	UPROPERTY()
	UBorder* PresetDeleteConfirmationOverlay = nullptr;
	UPROPERTY()
	UTextBlock* PresetDeleteConfirmationText = nullptr;
	UPROPERTY()
	UButton* PresetDeleteConfirmButton = nullptr;
	UPROPERTY()
	UButton* PresetDeleteCancelButton = nullptr;

	UPROPERTY()
	TArray<UButton*> QuickPresetSlotButtons;
	UPROPERTY()
	TArray<UTextBlock*> QuickPresetSlotTexts;

	UPROPERTY()
	UTextBlock* CurrentFormationText = nullptr;
	UPROPERTY()
	UTextBlock* OpponentFormationText = nullptr;
	UPROPERTY()
	UCanvasPanel* FormationPreviewCanvas = nullptr;

	UPROPERTY()
	UTextBlock* TacticSummaryText = nullptr;
	UPROPERTY()
	UTextBlock* OpponentTacticText = nullptr;

	UPROPERTY()
	UTextBlock* IndividualInstructionPlayerText = nullptr;
	UPROPERTY()
	UTextBlock* IndividualInstructionSummaryText = nullptr;

	UPROPERTY()
	UTextBlock* StatusText = nullptr;
	UPROPERTY()
	UTextBlock* ControlHintText = nullptr;
	UPROPERTY()
	UButton* CloseButton = nullptr;

	FSelectorRow BuiltInPresetSelector;
	FSelectorRow PresetSelector;
	FSelectorRow JsonFileSelector;
	FSelectorRow FormationSelector;
	FSelectorRow BuildUpSelector;
	FSelectorRow AttackChannelSelector;
	FSelectorRow AttackingWidthSelector;
	FSelectorRow AttackingTempoSelector;
	FSelectorRow AttackingTransitionSelector;
	FSelectorRow DefensiveBlockSelector;
	FSelectorRow PressingIntensitySelector;
	FSelectorRow MarkingStyleSelector;
	FSelectorRow DefensiveTransitionSelector;
	FSelectorRow InstructionSlotSelector;
	FSelectorRow IndividualAttackSelector;
	FSelectorRow IndividualDefenseSelector;
	FSelectorRow MarkingTargetSelector;

	ECoachMenuTab ActiveTab = ECoachMenuTab::Formation;
	FGuid SelectedPresetId;
	FGuid SelectedBuiltInPresetId;
	FGuid SelectedUserPresetId;
	TArray<FString> AvailablePresetJsonFiles;
	int32 SelectedPresetJsonFileIndex = INDEX_NONE;
	FGuid PendingDeletePresetId;
	bool bPresetDeleteConfirmationOpen = false;
	ECoachMenuInputDevice LastInputDevice = ECoachMenuInputDevice::KeyboardMouse;
	int32 FocusedControlIndex = 0;
	FName SelectedInstructionSlotId = NAME_None;

	bool bAppliedGamePause = false;
	bool bPreviousMouseCursorVisible = false;

	// Left-stick navigation is deliberately owned by the widget only while the
	// menu is open. Repeating is based on real time so it still works while the
	// match world is paused.
	int32 LastAnalogHorizontalDirection = 0;
	int32 LastAnalogVerticalDirection = 0;
	double LastAnalogHorizontalActionTime = -1000.0;
	double LastAnalogVerticalActionTime = -1000.0;
	float AnalogNavigationThreshold = 0.65f;
	float AnalogNavigationRepeatSeconds = 0.20f;

	float PreviewWidth = 640.0f;
	float PreviewHeight = 330.0f;
};
