#include "SoccerQuickTacticsWidget.h"

#include "GameHUD.h"
#include "SoccerTacticalPresetManager.h"
#include "SoccerTacticalPresetTypes.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"

namespace
{
	const FLinearColor QuickPanelBackground(0.018f, 0.028f, 0.040f, 0.94f);
	const FLinearColor QuickNormalButton(0.07f, 0.10f, 0.14f, 1.0f);
	const FLinearColor QuickSelectedButton(0.13f, 0.37f, 0.54f, 1.0f);
	const FLinearColor QuickActiveButton(0.07f, 0.34f, 0.22f, 1.0f);
	const FLinearColor QuickAccent(0.26f, 0.73f, 1.0f, 1.0f);

	UTextBlock* MakeQuickText(
		UWidgetTree* WidgetTree,
		const FString& Text,
		int32 FontSize,
		const FLinearColor& Color = FLinearColor::White
	)
	{
		if (WidgetTree == nullptr)
		{
			return nullptr;
		}

		UTextBlock* TextBlock = WidgetTree->ConstructWidget<UTextBlock>();
		TextBlock->SetText(FText::FromString(Text));
		TextBlock->SetColorAndOpacity(FSlateColor(Color));

		FSlateFontInfo FontInfo = TextBlock->Font;
		FontInfo.Size = FontSize;
		TextBlock->SetFont(FontInfo);
		return TextBlock;
	}

	void AddQuickVerticalChild(
		UVerticalBox* Parent,
		UWidget* Child,
		const FMargin& Padding = FMargin(0.0f)
	)
	{
		if (Parent == nullptr || Child == nullptr)
		{
			return;
		}

		UVerticalBoxSlot* Slot = Parent->AddChildToVerticalBox(Child);
		Slot->SetPadding(Padding);
		Slot->SetHorizontalAlignment(HAlign_Fill);
	}
}

void USoccerQuickTacticsWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
}

void USoccerQuickTacticsWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshFromPresetManager();
}

void USoccerQuickTacticsWidget::InitializeForPresetManager(
	USoccerTacticalPresetManager* InTacticalPresetManager
)
{
	TacticalPresetManager = InTacticalPresetManager;

	if (IsValid(TacticalPresetManager))
	{
		const FGuid ActivePresetId = TacticalPresetManager->GetActivePresetId();
		bool bFoundActiveQuickSlot = false;
		for (
			int32 QuickSlotIndex = 0;
			QuickSlotIndex < USoccerTacticalPresetManager::GetQuickPresetSlotCount();
			++QuickSlotIndex
		)
		{
			if (
				ActivePresetId.IsValid() &&
				TacticalPresetManager->GetQuickPresetIdForSlot(QuickSlotIndex) == ActivePresetId
			)
			{
				SelectedQuickSlotIndex = QuickSlotIndex;
				bFoundActiveQuickSlot = true;
				break;
			}
		}

		if (!bFoundActiveQuickSlot)
		{
			for (
				int32 QuickSlotIndex = 0;
				QuickSlotIndex < USoccerTacticalPresetManager::GetQuickPresetSlotCount();
				++QuickSlotIndex
			)
			{
				if (TacticalPresetManager->GetQuickPresetForSlot(QuickSlotIndex) != nullptr)
				{
					SelectedQuickSlotIndex = QuickSlotIndex;
					break;
				}
			}
		}
	}

	RefreshFromPresetManager();
}

void USoccerQuickTacticsWidget::ActivateMenu()
{
	APlayerController* PlayerController = GetOwningPlayer();
	if (PlayerController == nullptr)
	{
		return;
	}

	bPreviousMouseCursorVisible = PlayerController->bShowMouseCursor;
	PlayerController->bShowMouseCursor = true;
	PlayerController->bEnableClickEvents = true;

	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PlayerController->SetInputMode(InputMode);

	RefreshFromPresetManager();

	if (QuickSlotButtons.IsValidIndex(SelectedQuickSlotIndex))
	{
		QuickSlotButtons[SelectedQuickSlotIndex]->SetUserFocus(PlayerController);
	}
}

void USoccerQuickTacticsWidget::CloseMenu()
{
	APlayerController* PlayerController = GetOwningPlayer();
	if (PlayerController != nullptr)
	{
		PlayerController->bShowMouseCursor = bPreviousMouseCursorVisible;
		PlayerController->SetInputMode(FInputModeGameOnly());
	}

	RemoveFromParent();
}

FReply USoccerQuickTacticsWidget::NativeOnPreviewKeyDown(
	const FGeometry& InGeometry,
	const FKeyEvent& InKeyEvent
)
{
	const FKey Key = InKeyEvent.GetKey();

	if (
		Key == EKeys::Tab ||
		Key == EKeys::Gamepad_Special_Left ||
		Key == EKeys::Escape ||
		Key == EKeys::Gamepad_FaceButton_Right
	)
	{
		CloseMenu();
		return FReply::Handled();
	}

	if (
		Key == EKeys::Left ||
		Key == EKeys::Up ||
		Key == EKeys::Gamepad_DPad_Left ||
		Key == EKeys::Gamepad_DPad_Up
	)
	{
		MoveSelection(-1);
		return FReply::Handled();
	}

	if (
		Key == EKeys::Right ||
		Key == EKeys::Down ||
		Key == EKeys::Gamepad_DPad_Right ||
		Key == EKeys::Gamepad_DPad_Down
	)
	{
		MoveSelection(1);
		return FReply::Handled();
	}

	if (
		Key == EKeys::Enter ||
		Key == EKeys::SpaceBar ||
		Key == EKeys::Gamepad_FaceButton_Bottom
	)
	{
		ApplySelectedQuickSlot();
		return FReply::Handled();
	}

	if (Key == EKeys::One)
	{
		SelectQuickSlot(0);
		ApplySelectedQuickSlot();
		return FReply::Handled();
	}
	if (Key == EKeys::Two)
	{
		SelectQuickSlot(1);
		ApplySelectedQuickSlot();
		return FReply::Handled();
	}
	if (Key == EKeys::Three)
	{
		SelectQuickSlot(2);
		ApplySelectedQuickSlot();
		return FReply::Handled();
	}
	if (Key == EKeys::Four)
	{
		SelectQuickSlot(3);
		ApplySelectedQuickSlot();
		return FReply::Handled();
	}

	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

void USoccerQuickTacticsWidget::BuildWidgetTree()
{
	if (WidgetTree == nullptr)
	{
		return;
	}

	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget = RootCanvas;

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>();
	Panel->SetBrushColor(QuickPanelBackground);
	Panel->SetPadding(FMargin(16.0f));

	UCanvasPanelSlot* PanelSlot = RootCanvas->AddChildToCanvas(Panel);
	PanelSlot->SetAnchors(FAnchors(1.0f, 0.5f));
	PanelSlot->SetAlignment(FVector2D(1.0f, 0.5f));
	PanelSlot->SetPosition(FVector2D(-34.0f, 0.0f));
	PanelSlot->SetSize(FVector2D(430.0f, 390.0f));

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	Panel->AddChild(Column);

	UTextBlock* Title = MakeQuickText(
		WidgetTree,
		TEXT("CAMBIO TÁCTICO RÁPIDO"),
		20,
		QuickAccent
	);
	AddQuickVerticalChild(Column, Title, FMargin(0.0f, 0.0f, 0.0f, 6.0f));

	UTextBlock* Subtitle = MakeQuickText(
		WidgetTree,
		TEXT("Elegí uno de tus cuatro presets favoritos y confirmá."),
		13,
		FLinearColor(0.70f, 0.76f, 0.82f, 1.0f)
	);
	Subtitle->SetAutoWrapText(true);
	AddQuickVerticalChild(Column, Subtitle, FMargin(0.0f, 0.0f, 0.0f, 12.0f));

	ActivePresetText = MakeQuickText(
		WidgetTree,
		TEXT("ACTIVO: configuración manual"),
		14,
		FLinearColor(0.82f, 0.94f, 1.0f, 1.0f)
	);
	AddQuickVerticalChild(Column, ActivePresetText, FMargin(0.0f, 0.0f, 0.0f, 10.0f));

	QuickSlotButtons.Empty();
	QuickSlotTexts.Empty();

	for (int32 QuickSlotIndex = 0; QuickSlotIndex < 4; ++QuickSlotIndex)
	{
		USizeBox* ButtonSize = WidgetTree->ConstructWidget<USizeBox>();
		ButtonSize->SetHeightOverride(48.0f);

		UButton* QuickButton = WidgetTree->ConstructWidget<UButton>();
		QuickButton->SetBackgroundColor(QuickNormalButton);
		ButtonSize->AddChild(QuickButton);

		UTextBlock* QuickText = MakeQuickText(
			WidgetTree,
			FString::Printf(TEXT("%d   — VACÍO —"), QuickSlotIndex + 1),
			15
		);
		QuickText->SetJustification(ETextJustify::Left);
		QuickButton->AddChild(QuickText);

		QuickSlotButtons.Add(QuickButton);
		QuickSlotTexts.Add(QuickText);
		AddQuickVerticalChild(Column, ButtonSize, FMargin(0.0f, 0.0f, 0.0f, 5.0f));
	}

	if (QuickSlotButtons.Num() == 4)
	{
		QuickSlotButtons[0]->OnClicked.AddDynamic(
			this,
			&USoccerQuickTacticsWidget::HandleQuickSlot1Clicked
		);
		QuickSlotButtons[1]->OnClicked.AddDynamic(
			this,
			&USoccerQuickTacticsWidget::HandleQuickSlot2Clicked
		);
		QuickSlotButtons[2]->OnClicked.AddDynamic(
			this,
			&USoccerQuickTacticsWidget::HandleQuickSlot3Clicked
		);
		QuickSlotButtons[3]->OnClicked.AddDynamic(
			this,
			&USoccerQuickTacticsWidget::HandleQuickSlot4Clicked
		);
	}

	StatusText = MakeQuickText(
		WidgetTree,
		TEXT("Flechas/D-Pad: elegir   Enter/A: aplicar"),
		12,
		FLinearColor(0.60f, 0.68f, 0.74f, 1.0f)
	);
	StatusText->SetAutoWrapText(true);
	AddQuickVerticalChild(Column, StatusText, FMargin(0.0f, 5.0f, 0.0f, 8.0f));

	UHorizontalBox* ActionRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	AddQuickVerticalChild(Column, ActionRow);

	ApplyButton = WidgetTree->ConstructWidget<UButton>();
	ApplyButton->SetBackgroundColor(FLinearColor(0.08f, 0.42f, 0.28f, 1.0f));
	UTextBlock* ApplyText = MakeQuickText(WidgetTree, TEXT("APLICAR"), 14);
	ApplyText->SetJustification(ETextJustify::Center);
	ApplyButton->AddChild(ApplyText);
	ApplyButton->OnClicked.AddDynamic(this, &USoccerQuickTacticsWidget::HandleApplyClicked);
	UHorizontalBoxSlot* ApplySlot = ActionRow->AddChildToHorizontalBox(ApplyButton);
	ApplySlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	ApplySlot->SetPadding(FMargin(0.0f, 0.0f, 4.0f, 0.0f));

	CloseButton = WidgetTree->ConstructWidget<UButton>();
	CloseButton->SetBackgroundColor(FLinearColor(0.16f, 0.18f, 0.21f, 1.0f));
	UTextBlock* CloseText = MakeQuickText(WidgetTree, TEXT("CANCELAR"), 14);
	CloseText->SetJustification(ETextJustify::Center);
	CloseButton->AddChild(CloseText);
	CloseButton->OnClicked.AddDynamic(this, &USoccerQuickTacticsWidget::HandleCloseClicked);
	UHorizontalBoxSlot* CloseSlot = ActionRow->AddChildToHorizontalBox(CloseButton);
	CloseSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	CloseSlot->SetPadding(FMargin(4.0f, 0.0f, 0.0f, 0.0f));
}

void USoccerQuickTacticsWidget::RefreshFromPresetManager()
{
	const FGuid ActivePresetId =
		IsValid(TacticalPresetManager)
		? TacticalPresetManager->GetActivePresetId()
		: FGuid();

	const FSoccerTacticalPreset* ActivePreset =
		IsValid(TacticalPresetManager)
		? TacticalPresetManager->FindPreset(ActivePresetId)
		: nullptr;

	if (ActivePresetText != nullptr)
	{
		if (ActivePreset != nullptr)
		{
			const bool bModified = TacticalPresetManager->IsActivePresetModified();
			ActivePresetText->SetText(
				FText::FromString(
					FString::Printf(
						TEXT("ACTIVO: %s%s"),
						*ActivePreset->PresetName,
						bModified ? TEXT("  *") : TEXT("")
					)
				)
			);
		}
		else
		{
			ActivePresetText->SetText(
				FText::FromString(TEXT("ACTIVO: configuración manual"))
			);
		}
	}

	for (int32 QuickSlotIndex = 0; QuickSlotIndex < QuickSlotTexts.Num(); ++QuickSlotIndex)
	{
		if (!QuickSlotButtons.IsValidIndex(QuickSlotIndex))
		{
			continue;
		}

		UTextBlock* QuickText = QuickSlotTexts[QuickSlotIndex];
		UButton* QuickButton = QuickSlotButtons[QuickSlotIndex];
		const FSoccerTacticalPreset* QuickPreset =
			IsValid(TacticalPresetManager)
			? TacticalPresetManager->GetQuickPresetForSlot(QuickSlotIndex)
			: nullptr;

		if (QuickText != nullptr)
		{
			QuickText->SetText(
				FText::FromString(
					QuickPreset != nullptr
					? FString::Printf(
						TEXT("%d   %s"),
						QuickSlotIndex + 1,
						*QuickPreset->PresetName
					)
					: FString::Printf(
						TEXT("%d   — VACÍO —"),
						QuickSlotIndex + 1
					)
				)
			);
		}

		if (QuickButton != nullptr)
		{
			const bool bSelected = QuickSlotIndex == SelectedQuickSlotIndex;
			const bool bActive =
				QuickPreset != nullptr &&
				QuickPreset->PresetId == ActivePresetId;

			QuickButton->SetBackgroundColor(
				bSelected
				? QuickSelectedButton
				: bActive
				? QuickActiveButton
				: QuickNormalButton
			);
		}
	}

	if (ApplyButton != nullptr)
	{
		ApplyButton->SetIsEnabled(
			IsValid(TacticalPresetManager) &&
			TacticalPresetManager->GetQuickPresetForSlot(SelectedQuickSlotIndex) != nullptr
		);
	}
}

void USoccerQuickTacticsWidget::SelectQuickSlot(int32 NewQuickSlotIndex)
{
	const int32 SlotCount = USoccerTacticalPresetManager::GetQuickPresetSlotCount();
	if (SlotCount <= 0)
	{
		SelectedQuickSlotIndex = 0;
		return;
	}

	SelectedQuickSlotIndex = FMath::Clamp(NewQuickSlotIndex, 0, SlotCount - 1);
	RefreshFromPresetManager();

	APlayerController* PlayerController = GetOwningPlayer();
	if (
		PlayerController != nullptr &&
		QuickSlotButtons.IsValidIndex(SelectedQuickSlotIndex)
	)
	{
		QuickSlotButtons[SelectedQuickSlotIndex]->SetUserFocus(PlayerController);
	}
}

void USoccerQuickTacticsWidget::MoveSelection(int32 Direction)
{
	const int32 SlotCount = USoccerTacticalPresetManager::GetQuickPresetSlotCount();
	if (SlotCount <= 0)
	{
		return;
	}

	const int32 Step = Direction >= 0 ? 1 : -1;
	SelectedQuickSlotIndex =
		(SelectedQuickSlotIndex + Step + SlotCount) % SlotCount;
	SelectQuickSlot(SelectedQuickSlotIndex);
}

void USoccerQuickTacticsWidget::ApplySelectedQuickSlot()
{
	if (!IsValid(TacticalPresetManager))
	{
		return;
	}

	FString Message;
	if (TacticalPresetManager->ApplyQuickPresetSlotToPlayerTeam(
		SelectedQuickSlotIndex,
		Message
	))
	{
		NotifyHUD(Message);
		CloseMenu();
		return;
	}

	if (StatusText != nullptr)
	{
		StatusText->SetText(FText::FromString(Message));
	}
	RefreshFromPresetManager();
}

void USoccerQuickTacticsWidget::NotifyHUD(const FString& Message) const
{
	APlayerController* PlayerController = GetOwningPlayer();
	if (PlayerController == nullptr)
	{
		return;
	}

	AGameHUD* GameHUD = Cast<AGameHUD>(PlayerController->GetHUD());
	if (GameHUD != nullptr)
	{
		GameHUD->ShowQuickTacticsFeedback(Message);
	}
}

void USoccerQuickTacticsWidget::HandleQuickSlot1Clicked()
{
	SelectQuickSlot(0);
}

void USoccerQuickTacticsWidget::HandleQuickSlot2Clicked()
{
	SelectQuickSlot(1);
}

void USoccerQuickTacticsWidget::HandleQuickSlot3Clicked()
{
	SelectQuickSlot(2);
}

void USoccerQuickTacticsWidget::HandleQuickSlot4Clicked()
{
	SelectQuickSlot(3);
}

void USoccerQuickTacticsWidget::HandleApplyClicked()
{
	ApplySelectedQuickSlot();
}

void USoccerQuickTacticsWidget::HandleCloseClicked()
{
	CloseMenu();
}
