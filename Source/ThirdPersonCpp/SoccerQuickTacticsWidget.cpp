#include "SoccerQuickTacticsWidget.h"
#include "SoccerUITheme.h"

#include "GameHUD.h"
#include "SoccerTacticalPresetManager.h"
#include "SoccerTacticalPresetTypes.h"
#include "SoccerUIStyleEditorController.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/ScrollBox.h"
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

void USoccerQuickTacticsWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    UIAnimations.Tick(InDeltaTime);
    const FVector2D LogicalViewport = FSoccerUITheme::GetLogicalViewportSize(this);
    if (LogicalViewport.X > 1.f && !LastResponsiveViewport.Equals(LogicalViewport, 1.f))
    {
        LastResponsiveViewport = LogicalViewport;
        ApplyLiveStyle();
    }
}

void USoccerQuickTacticsWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
	ApplyLiveStyle();
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

    UIAnimations.StartEntrance(LiveMainPanel);
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
	// En modo UIOnly el viewport no recibe F11. Atenderlo aqui permite
	// alternar pantalla completa sin habilitar controles de gameplay.
	if (Key == EKeys::F11)
	{
		if (APlayerController* PlayerController = GetOwningPlayer())
		{
			PlayerController->ConsoleCommand(TEXT("fullscreen"), true);
		}
		return FReply::Handled();
	}
    if ((StyleEditor && StyleEditor->HasFocusedInput()) ||
        FSoccerUINavigation::IsEditingText(WidgetTree))
        return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);


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

	if (FSoccerUINavigation::IsAccept(Key))
	{
        if (IsValid(CloseButton) && CloseButton->HasKeyboardFocus())
            CloseMenu();
        else
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

FReply USoccerQuickTacticsWidget::NativeOnAnalogValueChanged(
    const FGeometry& InGeometry, const FAnalogInputEvent& InAnalogInputEvent)
{
    if ((StyleEditor && StyleEditor->HasFocusedInput()) ||
        FSoccerUINavigation::IsEditingText(WidgetTree))
        return Super::NativeOnAnalogValueChanged(InGeometry, InAnalogInputEvent);
    int32 X = 0, Y = 0;
    if (FSoccerUINavigation::ReadStick(InAnalogInputEvent, AnalogNavigation, X, Y))
    {
        if (X) MoveSelection(X);
        if (Y) MoveSelection(Y);
        return FReply::Handled();
    }
    return Super::NativeOnAnalogValueChanged(InGeometry, InAnalogInputEvent);
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
	LiveMainPanel = Panel;
	Panel->SetBrushColor(QuickPanelBackground);
	Panel->SetPadding(FMargin(16.0f));

	UCanvasPanelSlot* PanelSlot = RootCanvas->AddChildToCanvas(Panel);
	LivePanelCanvasSlot = PanelSlot;
	PanelSlot->SetAnchors(FAnchors(1.0f, 0.5f));
	PanelSlot->SetAlignment(FVector2D(1.0f, 0.5f));
	PanelSlot->SetPosition(FVector2D(-34.0f, 0.0f));
	PanelSlot->SetSize(FVector2D(430.0f, 390.0f));

	// Stage 4E: a short viewport or large text must never obscure the
	// final Apply/Cancel actions. Keep the entire panel content scrollable.
	UScrollBox* QuickMenuViewport = WidgetTree->ConstructWidget<UScrollBox>();
	Panel->AddChild(QuickMenuViewport);
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	QuickMenuViewport->AddChild(Column);

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
	ActivePresetText->SetAutoWrapText(true);
	AddQuickVerticalChild(Column, ActivePresetText, FMargin(0.0f, 0.0f, 0.0f, 10.0f));

	QuickSlotButtons.Empty();
	QuickSlotTexts.Empty();

	for (int32 QuickSlotIndex = 0; QuickSlotIndex < 4; ++QuickSlotIndex)
	{
		USizeBox* ButtonSize = WidgetTree->ConstructWidget<USizeBox>();
		ButtonSize->SetHeightOverride(48.0f);
		LiveQuickRows.Add(ButtonSize);

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
		TEXT("Flechas/Cruceta/Stick: elegir   Enter/A: aplicar   Esc/B: cerrar"),
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

	StyleEditor = NewObject<USoccerUIStyleEditorController>(this);
	TArray<FSoccerEditableStyleField> Fields;
	Fields.Add(FSoccerEditableStyleField::Number(TEXT("PanelWidth"), TEXT("Ancho del panel"), 430.f, 320.f, 780.f));
	Fields.Add(FSoccerEditableStyleField::Number(TEXT("PanelHeight"), TEXT("Alto del panel"), 390.f, 300.f, 730.f));
	Fields.Add(FSoccerEditableStyleField::Number(TEXT("RightOffset"), TEXT("Separación al borde"), 34.f, 0.f, 220.f));
	Fields.Add(FSoccerEditableStyleField::Number(TEXT("PanelPadding"), TEXT("Margen interior"), 16.f, 2.f, 60.f));
	Fields.Add(FSoccerEditableStyleField::Number(TEXT("RowHeight"), TEXT("Alto de botón táctico"), 48.f, 28.f, 110.f));
	Fields.Add(FSoccerEditableStyleField::Number(TEXT("FontScale"), TEXT("Escala de letra"), 1.f, .65f, 1.6f));
	Fields.Add(FSoccerEditableStyleField::Color(TEXT("PanelColor"), TEXT("Color panel"), TEXT("#05070AF0")));
	Fields.Add(FSoccerEditableStyleField::Color(TEXT("NormalColor"), TEXT("Botón normal"), TEXT("#121A24")));
	Fields.Add(FSoccerEditableStyleField::Color(TEXT("SelectedColor"), TEXT("Botón seleccionado"), TEXT("#215E8A")));
	Fields.Add(FSoccerEditableStyleField::Color(TEXT("ActiveColor"), TEXT("Táctica activa"), TEXT("#125738")));
	StyleEditor->Setup(WidgetTree, RootCanvas, TEXT("QuickTacticsStyle"), Fields);
	StyleEditor->OnApplied.AddDynamic(this, &USoccerQuickTacticsWidget::HandleStyleApplied);
}

void USoccerQuickTacticsWidget::HandleStyleApplied()
{
	ApplyLiveStyle();
}

void USoccerQuickTacticsWidget::ApplyLiveStyle()
{
	if (!StyleEditor || !LiveMainPanel || !LivePanelCanvasSlot) return;
	LiveMainPanel->SetBrushColor(StyleEditor->GetColor(TEXT("PanelColor"), QuickPanelBackground));
    const FVector2D ScreenUnits = FSoccerUITheme::GetLogicalViewportSize(this);
    const float PanelPadding = StyleEditor->GetNumber(TEXT("PanelPadding"), 16.f);
    LiveMainPanel->SetPadding(FMargin(ScreenUnits.X > 1.f
        ? FMath::Min(PanelPadding, FMath::Max(8.f, ScreenUnits.X * .025f))
        : PanelPadding));
    const float DesiredRightOffset = StyleEditor->GetNumber(TEXT("RightOffset"), 34.f);
    const FVector2D LogicalViewport = FSoccerUITheme::GetLogicalViewportSize(this);
    const float RightOffset = LogicalViewport.X > 1.f
        ? FMath::Min(DesiredRightOffset, FMath::Max(12.f, LogicalViewport.X * .04f))
        : DesiredRightOffset;
    const FVector2D FittedSize = FSoccerUITheme::FitPanelToViewport(this, FVector2D(
        StyleEditor->GetNumber(TEXT("PanelWidth"), 430.f),
        StyleEditor->GetNumber(TEXT("PanelHeight"), 390.f)), RightOffset + 8.f);
    LivePanelCanvasSlot->SetSize(FittedSize);
    LivePanelCanvasSlot->SetPosition(FVector2D(-RightOffset, 0.f));
    const float DesiredRowHeight = StyleEditor->GetNumber(TEXT("RowHeight"), 48.f);
    const float AvailableRowHeight = FMath::Max(30.f, (FittedSize.Y - 165.f) / 4.f);
    for (USizeBox* RowBox : LiveQuickRows)
    {
        if (RowBox) RowBox->SetHeightOverride(FMath::Min(DesiredRowHeight, AvailableRowHeight));
    }
    StyleEditor->ApplyTextScale(LiveMainPanel, FSoccerUITheme::GetAdaptiveTextScale(
        this, StyleEditor->GetNumber(TEXT("FontScale"), 1.f)));
    FSoccerUITheme::ApplyButtonStates(LiveMainPanel);
	RefreshFromPresetManager();
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
				? (StyleEditor ? StyleEditor->GetColor(TEXT("SelectedColor"), QuickSelectedButton) : QuickSelectedButton)
				: bActive
				? (StyleEditor ? StyleEditor->GetColor(TEXT("ActiveColor"), QuickActiveButton) : QuickActiveButton)
				: (StyleEditor ? StyleEditor->GetColor(TEXT("NormalColor"), QuickNormalButton) : QuickNormalButton)
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

	const int32 OldQuickSlotIndex = SelectedQuickSlotIndex;
	SelectedQuickSlotIndex = FMath::Clamp(NewQuickSlotIndex, 0, SlotCount - 1);
	RefreshFromPresetManager();
    if (OldQuickSlotIndex != SelectedQuickSlotIndex &&
        QuickSlotButtons.IsValidIndex(SelectedQuickSlotIndex))
    {
        UIAnimations.StartSelection(QuickSlotButtons[SelectedQuickSlotIndex]);
    }

	APlayerController* PlayerController = GetOwningPlayer();
	if (
		PlayerController != nullptr &&
		QuickSlotButtons.IsValidIndex(SelectedQuickSlotIndex)
	)
	{
        FSoccerUINavigation::FocusAndReveal(this, QuickSlotButtons[SelectedQuickSlotIndex]);
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
