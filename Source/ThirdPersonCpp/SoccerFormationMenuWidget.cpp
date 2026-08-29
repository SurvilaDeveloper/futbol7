#include "SoccerFormationMenuWidget.h"

#include "SoccerFormationLibrary.h"
#include "SoccerMatchManager.h"
#include "SoccerTacticalPresetManager.h"
#include "SoccerCharacterBase.h"
#include "SoccerTeamTypes.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScaleBox.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	const ESoccerFormationSystem FormationSystems[] =
	{
		ESoccerFormationSystem::OneThreeTwoOne,
		ESoccerFormationSystem::OneTwoThreeOne,
		ESoccerFormationSystem::OneThreeThree,
		ESoccerFormationSystem::OneTwoTwoTwo,
		ESoccerFormationSystem::OneThreeOneTwo,
		ESoccerFormationSystem::OneTwoOneTwoOne,
		ESoccerFormationSystem::OneTwoOneThree,
		ESoccerFormationSystem::OneFourOneOne,
		ESoccerFormationSystem::OneFiveOne
	};

	template <typename TEnum>
	struct TTacticDisplayOption
	{
		TEnum Value;
		const TCHAR* Label;
	};

	const TTacticDisplayOption<ESoccerBuildUpStyle> BuildUpOptions[] =
	{
		{ ESoccerBuildUpStyle::ShortPossession, TEXT("Salida corta") },
		{ ESoccerBuildUpStyle::Balanced, TEXT("Equilibrada") },
		{ ESoccerBuildUpStyle::Direct, TEXT("Directa") }
	};

	const TTacticDisplayOption<ESoccerAttackChannel> AttackChannelOptions[] =
	{
		{ ESoccerAttackChannel::Balanced, TEXT("Equilibrado") },
		{ ESoccerAttackChannel::Left, TEXT("Izquierda") },
		{ ESoccerAttackChannel::Center, TEXT("Centro") },
		{ ESoccerAttackChannel::Right, TEXT("Derecha") }
	};

	const TTacticDisplayOption<ESoccerAttackingWidth> AttackingWidthOptions[] =
	{
		{ ESoccerAttackingWidth::Narrow, TEXT("Estrecha") },
		{ ESoccerAttackingWidth::Balanced, TEXT("Equilibrada") },
		{ ESoccerAttackingWidth::Wide, TEXT("Amplia") }
	};

	const TTacticDisplayOption<ESoccerAttackingTempo> AttackingTempoOptions[] =
	{
		{ ESoccerAttackingTempo::Patient, TEXT("Paciente") },
		{ ESoccerAttackingTempo::Balanced, TEXT("Equilibrado") },
		{ ESoccerAttackingTempo::Fast, TEXT("Rápido") }
	};

	const TTacticDisplayOption<ESoccerAttackingTransition> AttackingTransitionOptions[] =
	{
		{ ESoccerAttackingTransition::RetainPossession, TEXT("Conservar posesión") },
		{ ESoccerAttackingTransition::Balanced, TEXT("Equilibrada") },
		{ ESoccerAttackingTransition::CounterAttack, TEXT("Contraataque") }
	};

	const TTacticDisplayOption<ESoccerDefensiveBlock> DefensiveBlockOptions[] =
	{
		{ ESoccerDefensiveBlock::Low, TEXT("Bajo") },
		{ ESoccerDefensiveBlock::Medium, TEXT("Medio") },
		{ ESoccerDefensiveBlock::High, TEXT("Alto") }
	};

	const TTacticDisplayOption<ESoccerPressingIntensity> PressingOptions[] =
	{
		{ ESoccerPressingIntensity::Low, TEXT("Baja") },
		{ ESoccerPressingIntensity::Medium, TEXT("Media") },
		{ ESoccerPressingIntensity::High, TEXT("Alta") }
	};

	const TTacticDisplayOption<ESoccerMarkingStyle> MarkingOptions[] =
	{
		{ ESoccerMarkingStyle::Zonal, TEXT("Zonal") },
		{ ESoccerMarkingStyle::Mixed, TEXT("Mixto") },
		{ ESoccerMarkingStyle::ManToMan, TEXT("Hombre a hombre") }
	};

	const TTacticDisplayOption<ESoccerDefensiveTransition> DefensiveTransitionOptions[] =
	{
		{ ESoccerDefensiveTransition::Regroup, TEXT("Replegar") },
		{ ESoccerDefensiveTransition::Balanced, TEXT("Equilibrada") },
		{ ESoccerDefensiveTransition::CounterPress, TEXT("Contrapresión") }
	};

	const TTacticDisplayOption<ESoccerIndividualAttackInstruction>
	IndividualAttackInstructionOptions[] =
	{
		{ ESoccerIndividualAttackInstruction::Balanced, TEXT("Equilibrada") },
		{ ESoccerIndividualAttackInstruction::HoldPosition, TEXT("Mantener posición") },
		{ ESoccerIndividualAttackInstruction::LinkPlay, TEXT("Enlazar juego") },
		{ ESoccerIndividualAttackInstruction::MakeForwardRuns, TEXT("Picar al espacio") },
		{ ESoccerIndividualAttackInstruction::StayWide, TEXT("Dar amplitud") },
		{ ESoccerIndividualAttackInstruction::ComeShort, TEXT("Acercarse a recibir") },
		{ ESoccerIndividualAttackInstruction::TargetPlayer, TEXT("Referencia arriba") }
	};

	const TTacticDisplayOption<ESoccerIndividualDefensiveInstruction>
	IndividualDefensiveInstructionOptions[] =
	{
		{ ESoccerIndividualDefensiveInstruction::Balanced, TEXT("Equilibrada") },
		{ ESoccerIndividualDefensiveInstruction::HoldPosition, TEXT("Mantener posición") },
		{ ESoccerIndividualDefensiveInstruction::PressBall, TEXT("Presionar") },
		{ ESoccerIndividualDefensiveInstruction::Cover, TEXT("Cubrir") },
		{ ESoccerIndividualDefensiveInstruction::ProtectCenter, TEXT("Proteger centro") },
		{ ESoccerIndividualDefensiveInstruction::MarkTightly, TEXT("Marcar de cerca") }
	};

	// The coach menu deliberately leaves the paused match visible behind it.
	// Controls remain more opaque than the structural surfaces so text and focus
	// feedback stay readable even over bright stadiums.
	const FLinearColor MenuBackground(0.020f, 0.030f, 0.045f, 0.76f);
	const FLinearColor CardBackground(0.040f, 0.058f, 0.080f, 0.52f);
	const FLinearColor SelectorBackground(0.055f, 0.075f, 0.100f, 0.86f);
	const FLinearColor SelectorFocused(0.075f, 0.34f, 0.48f, 0.96f);
	const FLinearColor Accent(0.20f, 0.72f, 0.95f, 1.0f);
	const FLinearColor WarmAccent(0.95f, 0.74f, 0.22f, 1.0f);
	const FLinearColor MutedText(0.62f, 0.68f, 0.74f, 1.0f);
	const float PresetPreviewWidth = 570.0f;
	const float PresetPreviewHeight = 180.0f;

	UTextBlock* MakeTextBlock(
		UWidgetTree* WidgetTree,
		const FString& Text,
		const FLinearColor& Color = FLinearColor::White,
		int32 FontSize = 18
	)
	{
		if (WidgetTree == nullptr)
		{
			return nullptr;
		}

		UTextBlock* TextBlock = WidgetTree->ConstructWidget<UTextBlock>();
		if (TextBlock != nullptr)
		{
			TextBlock->SetText(FText::FromString(Text));
			TextBlock->SetColorAndOpacity(FSlateColor(Color));
			FSlateFontInfo FontInfo = TextBlock->Font;
			FontInfo.Size = FontSize;
			TextBlock->SetFont(FontInfo);
		}

		return TextBlock;
	}

	void AddVerticalChild(
		UVerticalBox* VerticalBox,
		UWidget* Child,
		const FMargin& Padding = FMargin(0.0f),
		EHorizontalAlignment HorizontalAlignment = HAlign_Fill,
		ESlateSizeRule::Type SizeRule = ESlateSizeRule::Automatic
	)
	{
		if (VerticalBox == nullptr || Child == nullptr)
		{
			return;
		}

		UVerticalBoxSlot* Slot = VerticalBox->AddChildToVerticalBox(Child);
		if (Slot != nullptr)
		{
			Slot->SetPadding(Padding);
			Slot->SetHorizontalAlignment(HorizontalAlignment);
			Slot->SetSize(FSlateChildSize(SizeRule));
		}
	}

	void AddHorizontalChild(
		UHorizontalBox* HorizontalBox,
		UWidget* Child,
		const FMargin& Padding = FMargin(0.0f),
		ESlateSizeRule::Type SizeRule = ESlateSizeRule::Automatic,
		EVerticalAlignment VerticalAlignment = VAlign_Center
	)
	{
		if (HorizontalBox == nullptr || Child == nullptr)
		{
			return;
		}

		UHorizontalBoxSlot* Slot = HorizontalBox->AddChildToHorizontalBox(Child);
		if (Slot != nullptr)
		{
			Slot->SetPadding(Padding);
			Slot->SetVerticalAlignment(VerticalAlignment);
			Slot->SetSize(FSlateChildSize(SizeRule));
		}
	}

	UButton* MakeButton(
		UWidgetTree* WidgetTree,
		const FString& Label,
		int32 FontSize = 17,
		const FLinearColor& TextColor = FLinearColor::White
	)
	{
		if (WidgetTree == nullptr)
		{
			return nullptr;
		}

		UButton* Button = WidgetTree->ConstructWidget<UButton>();
		if (Button == nullptr)
		{
			return nullptr;
		}

		Button->SetBackgroundColor(FLinearColor(0.08f, 0.11f, 0.15f, 0.90f));
		UTextBlock* Text = MakeTextBlock(WidgetTree, Label, TextColor, FontSize);
		if (Text != nullptr)
		{
			Text->SetJustification(ETextJustify::Center);
			Button->AddChild(Text);
		}

		return Button;
	}

	UBorder* MakeCard(UWidgetTree* WidgetTree, const FLinearColor& Color = CardBackground)
	{
		if (WidgetTree == nullptr)
		{
			return nullptr;
		}

		UBorder* Border = WidgetTree->ConstructWidget<UBorder>();
		if (Border != nullptr)
		{
			Border->SetBrushColor(Color);
			Border->SetPadding(FMargin(22.0f));
		}
		return Border;
	}

	template <typename TEnum, SIZE_T OptionCount>
	FString GetTacticDisplayName(
		TEnum Value,
		const TTacticDisplayOption<TEnum>(&Options)[OptionCount]
	)
	{
		for (const TTacticDisplayOption<TEnum>& Option : Options)
		{
			if (Option.Value == Value)
			{
				return FString(Option.Label);
			}
		}
		return TEXT("--");
	}

	template <typename TEnum, SIZE_T OptionCount>
	TEnum CycleTacticValue(
		TEnum CurrentValue,
		int32 Direction,
		const TTacticDisplayOption<TEnum>(&Options)[OptionCount]
	)
	{
		int32 CurrentIndex = 0;
		for (int32 Index = 0; Index < static_cast<int32>(OptionCount); ++Index)
		{
			if (Options[Index].Value == CurrentValue)
			{
				CurrentIndex = Index;
				break;
			}
		}

		const int32 Count = static_cast<int32>(OptionCount);
		const int32 Step = Direction >= 0 ? 1 : -1;
		const int32 NewIndex = (CurrentIndex + Step + Count) % Count;
		return Options[NewIndex].Value;
	}

	void AddCanvasRect(
		UWidgetTree* WidgetTree,
		UCanvasPanel* Canvas,
		const FVector2D& Position,
		const FVector2D& Size,
		const FLinearColor& Color
	)
	{
		if (WidgetTree == nullptr || Canvas == nullptr)
		{
			return;
		}

		UBorder* Rect = WidgetTree->ConstructWidget<UBorder>();
		Rect->SetBrushColor(Color);
		UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Rect);
		Slot->SetPosition(Position);
		Slot->SetSize(Size);
	}

	void AddCanvasOutline(
		UWidgetTree* WidgetTree,
		UCanvasPanel* Canvas,
		const FVector2D& Position,
		const FVector2D& Size,
		const FLinearColor& Color,
		float Thickness = 2.0f
	)
	{
		AddCanvasRect(WidgetTree, Canvas, Position, FVector2D(Size.X, Thickness), Color);
		AddCanvasRect(
			WidgetTree,
			Canvas,
			FVector2D(Position.X, Position.Y + Size.Y - Thickness),
			FVector2D(Size.X, Thickness),
			Color
		);
		AddCanvasRect(WidgetTree, Canvas, Position, FVector2D(Thickness, Size.Y), Color);
		AddCanvasRect(
			WidgetTree,
			Canvas,
			FVector2D(Position.X + Size.X - Thickness, Position.Y),
			FVector2D(Thickness, Size.Y),
			Color
		);
	}
}

void USoccerFormationMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
}

void USoccerFormationMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshFromMatchManager();
}

FReply USoccerFormationMenuWidget::NativeOnPreviewKeyDown(
	const FGeometry& InGeometry,
	const FKeyEvent& InKeyEvent
)
{
	const FKey Key = InKeyEvent.GetKey();

	const bool bGamepadKey =
		Key == EKeys::Gamepad_Special_Right ||
		Key == EKeys::Gamepad_LeftShoulder ||
		Key == EKeys::Gamepad_RightShoulder ||
		Key == EKeys::Gamepad_DPad_Up ||
		Key == EKeys::Gamepad_DPad_Down ||
		Key == EKeys::Gamepad_DPad_Left ||
		Key == EKeys::Gamepad_DPad_Right ||
		Key == EKeys::Gamepad_FaceButton_Bottom ||
		Key == EKeys::Gamepad_FaceButton_Right;

	SetLastInputDevice(
		bGamepadKey
		? ECoachMenuInputDevice::Gamepad
		: ECoachMenuInputDevice::KeyboardMouse
	);

	// Stage 16D2: deletion confirmation is modal. While it is open, no coach
	// navigation or preset action can leak through to the menu underneath.
	if (bPresetDeleteConfirmationOpen)
	{
		if (
			Key == EKeys::Escape ||
			Key == EKeys::M ||
			Key == EKeys::Gamepad_Special_Right ||
			Key == EKeys::Gamepad_FaceButton_Right
		)
		{
			ClosePresetDeleteConfirmation();
			return FReply::Handled();
		}

		if (
			Key == EKeys::Enter ||
			Key == EKeys::SpaceBar ||
			Key == EKeys::Gamepad_FaceButton_Bottom
		)
		{
			ConfirmPendingPresetDeletion();
			return FReply::Handled();
		}

		return FReply::Handled();
	}

	// Once a text field exists, ordinary letters, arrows, space and Enter must
	// belong to the editor instead of being interpreted as coach shortcuts.
	// The dedicated gamepad Menu/Options key remains available to close the UI.
	if (
		PresetNameTextBox != nullptr &&
		PresetNameTextBox->HasKeyboardFocus() &&
		Key != EKeys::Gamepad_Special_Right
	)
	{
		return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
	}

	// The same dedicated toggle that opens the menu from gameplay closes it
	// here. UIOnly prevents the character binding from also receiving the key.
	if (
		Key == EKeys::M ||
		Key == EKeys::Gamepad_Special_Right ||
		Key == EKeys::Escape
	)
	{
		CloseMenu();
		return FReply::Handled();
	}

	if (Key == EKeys::Gamepad_LeftShoulder || Key == EKeys::PageUp)
	{
		const int32 CurrentTabIndex = static_cast<int32>(ActiveTab);
		const int32 NewTabIndex = (CurrentTabIndex + 3) % 4;
		SetActiveTab(static_cast<ECoachMenuTab>(NewTabIndex));
		return FReply::Handled();
	}

	if (Key == EKeys::Gamepad_RightShoulder || Key == EKeys::PageDown)
	{
		const int32 CurrentTabIndex = static_cast<int32>(ActiveTab);
		const int32 NewTabIndex = (CurrentTabIndex + 1) % 4;
		SetActiveTab(static_cast<ECoachMenuTab>(NewTabIndex));
		return FReply::Handled();
	}

	if (Key == EKeys::Gamepad_DPad_Up || Key == EKeys::Up)
	{
		MoveFocus(-1);
		return FReply::Handled();
	}

	if (Key == EKeys::Gamepad_DPad_Down || Key == EKeys::Down)
	{
		MoveFocus(1);
		return FReply::Handled();
	}

	if (Key == EKeys::Gamepad_DPad_Left || Key == EKeys::Left)
	{
		CycleFocusedValue(-1);
		return FReply::Handled();
	}

	if (Key == EKeys::Gamepad_DPad_Right || Key == EKeys::Right)
	{
		CycleFocusedValue(1);
		return FReply::Handled();
	}

	// A / Enter is an alternate one-button selection path. Values are still
	// applied immediately, so selecting a focused row advances to its next
	// valid option without requiring a ComboBox or a second modal screen.
	if (
		Key == EKeys::Gamepad_FaceButton_Bottom ||
		Key == EKeys::Enter ||
		Key == EKeys::SpaceBar
	)
	{
		if (ActiveTab == ECoachMenuTab::Presets)
		{
			if (FocusedControlIndex == 2)
			{
				HandlePresetJsonImportClicked();
			}
			else if (FocusedControlIndex == 0)
			{
				if (SelectedBuiltInPresetId.IsValid())
				{
					SelectedPresetId = SelectedBuiltInPresetId;
					HandlePresetApplyClicked();
				}
			}
			else if (SelectedUserPresetId.IsValid())
			{
				SelectedPresetId = SelectedUserPresetId;
				HandlePresetApplyClicked();
			}
			else
			{
				SetStatusMessage(TEXT("Todavía no hay presets en MIS PRESETS."));
			}
		}
		else
		{
			CycleFocusedValue(1);
		}
		return FReply::Handled();
	}

	// B is intentionally consumed but does not close the whole coach screen.
	// Menu/Options remains the single dedicated open/close button. This also
	// leaves B free to become a contextual gameplay action whenever UI is closed.
	if (Key == EKeys::Gamepad_FaceButton_Right)
	{
		return FReply::Handled();
	}

	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

FReply USoccerFormationMenuWidget::NativeOnAnalogValueChanged(
	const FGeometry& InGeometry,
	const FAnalogInputEvent& InAnalogInputEvent
)
{
	SetLastInputDevice(ECoachMenuInputDevice::Gamepad);

	const FKey Key = InAnalogInputEvent.GetKey();
	const float AnalogValue = InAnalogInputEvent.GetAnalogValue();
	const double CurrentRealTime = FPlatformTime::Seconds();
	const double RepeatSeconds = FMath::Max(0.05f, AnalogNavigationRepeatSeconds);

	if (Key == EKeys::Gamepad_LeftY)
	{
		int32 Direction = 0;
		if (AnalogValue >= AnalogNavigationThreshold)
		{
			Direction = -1;
		}
		else if (AnalogValue <= -AnalogNavigationThreshold)
		{
			Direction = 1;
		}

		if (Direction == 0)
		{
			LastAnalogVerticalDirection = 0;
		}
		else if (
			Direction != LastAnalogVerticalDirection ||
			CurrentRealTime - LastAnalogVerticalActionTime >= RepeatSeconds
		)
		{
			MoveFocus(Direction);
			LastAnalogVerticalDirection = Direction;
			LastAnalogVerticalActionTime = CurrentRealTime;
		}

		return FReply::Handled();
	}

	if (Key == EKeys::Gamepad_LeftX)
	{
		int32 Direction = 0;
		if (AnalogValue >= AnalogNavigationThreshold)
		{
			Direction = 1;
		}
		else if (AnalogValue <= -AnalogNavigationThreshold)
		{
			Direction = -1;
		}

		if (Direction == 0)
		{
			LastAnalogHorizontalDirection = 0;
		}
		else if (
			Direction != LastAnalogHorizontalDirection ||
			CurrentRealTime - LastAnalogHorizontalActionTime >= RepeatSeconds
		)
		{
			CycleFocusedValue(Direction);
			LastAnalogHorizontalDirection = Direction;
			LastAnalogHorizontalActionTime = CurrentRealTime;
		}

		return FReply::Handled();
	}

	return Super::NativeOnAnalogValueChanged(InGeometry, InAnalogInputEvent);
}

FReply USoccerFormationMenuWidget::NativeOnMouseMove(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent
)
{
	SetLastInputDevice(ECoachMenuInputDevice::KeyboardMouse);
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

void USoccerFormationMenuWidget::InitializeForMatchManager(
	ASoccerMatchManager* InMatchManager,
	USoccerTacticalPresetManager* InTacticalPresetManager
)
{
	MatchManager = InMatchManager;

	// Stage 16C: GameHUD owns the shared preset service so the full coach menu
	// and the compact in-match selector share ActivePresetId and quick slots.
	// The fallback keeps this widget usable in isolation.
	if (IsValid(InTacticalPresetManager))
	{
		TacticalPresetManager = InTacticalPresetManager;
	}
	else if (!IsValid(TacticalPresetManager))
	{
		TacticalPresetManager = NewObject<USoccerTacticalPresetManager>(this);
		if (IsValid(TacticalPresetManager))
		{
			TacticalPresetManager->Initialize(MatchManager);
		}
	}

	RefreshFromMatchManager();
	RefreshPresetsPage(true);
}

void USoccerFormationMenuWidget::ActivateMenu(bool bPauseGame)
{
	APlayerController* PlayerController = GetOwningPlayer();
	if (PlayerController == nullptr)
	{
		return;
	}

	bPreviousMouseCursorVisible = PlayerController->bShowMouseCursor;
	bAppliedGamePause = false;

	if (
		bPauseGame &&
		GetWorld() != nullptr &&
		!UGameplayStatics::IsGamePaused(GetWorld())
	)
	{
		UGameplayStatics::SetGamePaused(GetWorld(), true);
		bAppliedGamePause = true;
	}

	PlayerController->bShowMouseCursor = true;
	PlayerController->bEnableClickEvents = true;

	// Stage 12B: the menu owns input exclusively while it is visible.
	// Gameplay bindings (pass, tackle, jump, shoulders, D-Pad, etc.) are therefore
	// free to reuse the same physical buttons without firing behind the UI.
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PlayerController->SetInputMode(InputMode);

	LastAnalogHorizontalDirection = 0;
	LastAnalogVerticalDirection = 0;
	LastAnalogHorizontalActionTime = -1000.0;
	LastAnalogVerticalActionTime = -1000.0;

	UButton* ActiveTabButton = FormationTabButton;
	switch (ActiveTab)
	{
	case ECoachMenuTab::Presets: ActiveTabButton = PresetsTabButton; break;
	case ECoachMenuTab::Formation: ActiveTabButton = FormationTabButton; break;
	case ECoachMenuTab::Tactics: ActiveTabButton = TacticsTabButton; break;
	case ECoachMenuTab::Instructions: ActiveTabButton = InstructionsTabButton; break;
	}

	if (ActiveTabButton != nullptr)
	{
		ActiveTabButton->SetUserFocus(PlayerController);
	}

	SetActiveTab(ActiveTab, false);
	RefreshFocusVisuals();
	RefreshControlHints();
	SetTabGuidanceMessage();
}

void USoccerFormationMenuWidget::CloseMenu()
{
	ClosePresetDeleteConfirmation();

	APlayerController* PlayerController = GetOwningPlayer();

	if (
		bAppliedGamePause &&
		GetWorld() != nullptr &&
		UGameplayStatics::IsGamePaused(GetWorld())
	)
	{
		UGameplayStatics::SetGamePaused(GetWorld(), false);
	}

	bAppliedGamePause = false;

	if (PlayerController != nullptr)
	{
		PlayerController->bShowMouseCursor = bPreviousMouseCursorVisible;
		FInputModeGameOnly InputMode;
		PlayerController->SetInputMode(InputMode);
	}

	RemoveFromParent();
}

void USoccerFormationMenuWidget::BuildWidgetTree()
{
	if (WidgetTree == nullptr || WidgetTree->RootWidget != nullptr)
	{
		return;
	}

	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget = RootCanvas;

	UBorder* ScreenDim = WidgetTree->ConstructWidget<UBorder>();
	ScreenDim->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.22f));
	UCanvasPanelSlot* DimSlot = RootCanvas->AddChildToCanvas(ScreenDim);
	DimSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	DimSlot->SetOffsets(FMargin(0.0f));

	UBorder* MenuBorder = WidgetTree->ConstructWidget<UBorder>();
	MenuBorder->SetBrushColor(MenuBackground);
	MenuBorder->SetPadding(FMargin(30.0f, 24.0f));

	UCanvasPanelSlot* MenuCanvasSlot = RootCanvas->AddChildToCanvas(MenuBorder);
	// Percentage anchors make the menu occupy almost all of the usable viewport
	// at every DPI scale without relying on a resolution-specific fixed size.
	MenuCanvasSlot->SetAnchors(FAnchors(0.025f, 0.035f, 0.975f, 0.965f));
	MenuCanvasSlot->SetOffsets(FMargin(0.0f));

	UVerticalBox* MainColumn = WidgetTree->ConstructWidget<UVerticalBox>();
	MenuBorder->AddChild(MainColumn);

	// Header
	UHorizontalBox* HeaderRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	AddVerticalChild(MainColumn, HeaderRow, FMargin(0.0f, 0.0f, 0.0f, 16.0f));

	UVerticalBox* HeaderLeft = WidgetTree->ConstructWidget<UVerticalBox>();
	AddHorizontalChild(HeaderRow, HeaderLeft, FMargin(0.0f, 0.0f, 24.0f, 0.0f), ESlateSizeRule::Fill);

	UTextBlock* TitleText = MakeTextBlock(
		WidgetTree,
		TEXT("SISTEMA Y TÁCTICA"),
		FLinearColor(0.97f, 0.98f, 1.0f, 1.0f),
		31
	);
	AddVerticalChild(HeaderLeft, TitleText);

	HeaderContextText = MakeTextBlock(
		WidgetTree,
		TEXT("PlayerTeam  |  --"),
		MutedText,
		18
	);
	AddVerticalChild(HeaderLeft, HeaderContextText, FMargin(0.0f, 3.0f, 0.0f, 0.0f));

	HeaderOpponentText = MakeTextBlock(
		WidgetTree,
		TEXT("Rival: --"),
		FLinearColor(0.78f, 0.82f, 0.86f, 1.0f),
		18
	);
	HeaderOpponentText->SetJustification(ETextJustify::Right);
	AddHorizontalChild(
		HeaderRow,
		HeaderOpponentText,
		FMargin(0.0f, 0.0f, 22.0f, 0.0f),
		ESlateSizeRule::Automatic,
		VAlign_Center
	);

	USizeBox* CloseSizeBox = WidgetTree->ConstructWidget<USizeBox>();
	CloseSizeBox->SetWidthOverride(230.0f);
	CloseSizeBox->SetHeightOverride(46.0f);
	CloseButton = MakeButton(WidgetTree, TEXT("VOLVER AL PARTIDO"), 16);
	CloseButton->SetBackgroundColor(FLinearColor(0.11f, 0.16f, 0.21f, 0.92f));
	CloseButton->OnClicked.AddDynamic(this, &USoccerFormationMenuWidget::HandleCloseClicked);
	CloseSizeBox->AddChild(CloseButton);
	AddHorizontalChild(HeaderRow, CloseSizeBox, FMargin(0.0f), ESlateSizeRule::Automatic, VAlign_Center);

	// Tabs
	UHorizontalBox* TabRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	AddVerticalChild(MainColumn, TabRow, FMargin(0.0f, 0.0f, 0.0f, 18.0f));

	PresetsTabButton = MakeButton(WidgetTree, TEXT("PRESETS"), 19);
	FormationTabButton = MakeButton(WidgetTree, TEXT("FORMACIÓN"), 19);
	TacticsTabButton = MakeButton(WidgetTree, TEXT("TÁCTICA"), 19);
	InstructionsTabButton = MakeButton(WidgetTree, TEXT("INSTRUCCIONES"), 19);

	PresetsTabButton->OnClicked.AddDynamic(this, &USoccerFormationMenuWidget::HandlePresetsTabClicked);
	FormationTabButton->OnClicked.AddDynamic(this, &USoccerFormationMenuWidget::HandleFormationTabClicked);
	TacticsTabButton->OnClicked.AddDynamic(this, &USoccerFormationMenuWidget::HandleTacticsTabClicked);
	InstructionsTabButton->OnClicked.AddDynamic(this, &USoccerFormationMenuWidget::HandleInstructionsTabClicked);

	AddHorizontalChild(TabRow, PresetsTabButton, FMargin(0.0f, 0.0f, 8.0f, 0.0f), ESlateSizeRule::Fill);
	AddHorizontalChild(TabRow, FormationTabButton, FMargin(8.0f), ESlateSizeRule::Fill);
	AddHorizontalChild(TabRow, TacticsTabButton, FMargin(8.0f), ESlateSizeRule::Fill);
	AddHorizontalChild(TabRow, InstructionsTabButton, FMargin(8.0f, 0.0f, 0.0f, 0.0f), ESlateSizeRule::Fill);

	// Page area
	UBorder* PageBorder = MakeCard(WidgetTree, FLinearColor(0.028f, 0.043f, 0.060f, 0.34f));
	PageBorder->SetPadding(FMargin(26.0f));
	AddVerticalChild(
		MainColumn,
		PageBorder,
		FMargin(0.0f, 0.0f, 0.0f, 16.0f),
		HAlign_Fill,
		ESlateSizeRule::Fill
	);

	PageSwitcher = WidgetTree->ConstructWidget<UWidgetSwitcher>();
	PageBorder->AddChild(PageSwitcher);

	// Presets can contain substantially more information than the other pages.
	// Keep it inside its own scroll viewport so its desired height can never paint
	// over the fixed footer when the game runs at a shorter resolution / DPI.
	UScrollBox* PresetsScrollPage = WidgetTree->ConstructWidget<UScrollBox>();
	UVerticalBox* PresetsPage = WidgetTree->ConstructWidget<UVerticalBox>();
	PresetsScrollPage->AddChild(PresetsPage);

	UVerticalBox* FormationPage = WidgetTree->ConstructWidget<UVerticalBox>();
	UVerticalBox* TacticsPage = WidgetTree->ConstructWidget<UVerticalBox>();
	UVerticalBox* InstructionsPage = WidgetTree->ConstructWidget<UVerticalBox>();

	PageSwitcher->AddChild(PresetsScrollPage);
	PageSwitcher->AddChild(FormationPage);
	PageSwitcher->AddChild(TacticsPage);
	PageSwitcher->AddChild(InstructionsPage);

	BuildPresetsPage(PresetsPage);
	BuildFormationPage(FormationPage);
	BuildTacticsPage(TacticsPage);
	BuildInstructionsPage(InstructionsPage);

	// Quiet, single-line contextual footer. The close action lives in the header.
	UHorizontalBox* FooterRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	AddVerticalChild(MainColumn, FooterRow, FMargin(0.0f, 2.0f, 0.0f, 0.0f));

	StatusText = MakeTextBlock(
		WidgetTree,
		TEXT("Listo."),
		FLinearColor(0.72f, 0.86f, 0.96f, 1.0f),
		16
	);
	AddHorizontalChild(
		FooterRow,
		StatusText,
		FMargin(0.0f, 0.0f, 30.0f, 0.0f),
		ESlateSizeRule::Fill,
		VAlign_Center
	);

	ControlHintText = MakeTextBlock(
		WidgetTree,
		TEXT("Mouse: cambiar   Flechas: navegar/cambiar   PgUp/PgDn: sección   Enter: cambiar   M/Esc: cerrar"),
		MutedText,
		15
	);
	ControlHintText->SetJustification(ETextJustify::Right);
	AddHorizontalChild(FooterRow, ControlHintText, FMargin(0.0f), ESlateSizeRule::Automatic, VAlign_Center);

	// Stage 16D2: destructive preset deletion gets its own modal layer.
	// The full-screen border blocks mouse interaction with the coach menu below.
	PresetDeleteConfirmationOverlay = WidgetTree->ConstructWidget<UBorder>();
	PresetDeleteConfirmationOverlay->SetBrushColor(
		FLinearColor(0.0f, 0.0f, 0.0f, 0.72f)
	);
	PresetDeleteConfirmationOverlay->SetHorizontalAlignment(HAlign_Center);
	PresetDeleteConfirmationOverlay->SetVerticalAlignment(VAlign_Center);
	PresetDeleteConfirmationOverlay->SetVisibility(ESlateVisibility::Collapsed);

	UCanvasPanelSlot* DeleteOverlaySlot =
		RootCanvas->AddChildToCanvas(PresetDeleteConfirmationOverlay);
	DeleteOverlaySlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	DeleteOverlaySlot->SetOffsets(FMargin(0.0f));
	DeleteOverlaySlot->SetZOrder(100);

	USizeBox* DeleteDialogSize = WidgetTree->ConstructWidget<USizeBox>();
	DeleteDialogSize->SetWidthOverride(620.0f);
	DeleteDialogSize->SetHeightOverride(280.0f);
	PresetDeleteConfirmationOverlay->AddChild(DeleteDialogSize);

	UBorder* DeleteDialogCard = MakeCard(
		WidgetTree,
		FLinearColor(0.035f, 0.050f, 0.068f, 1.0f)
	);
	DeleteDialogCard->SetPadding(FMargin(30.0f, 26.0f));
	DeleteDialogSize->AddChild(DeleteDialogCard);

	UVerticalBox* DeleteDialogColumn = WidgetTree->ConstructWidget<UVerticalBox>();
	DeleteDialogCard->AddChild(DeleteDialogColumn);

	UTextBlock* DeleteDialogTitle = MakeTextBlock(
		WidgetTree,
		TEXT("ELIMINAR PRESET"),
		FLinearColor(0.95f, 0.37f, 0.37f, 1.0f),
		23
	);
	DeleteDialogTitle->SetJustification(ETextJustify::Center);
	AddVerticalChild(
		DeleteDialogColumn,
		DeleteDialogTitle,
		FMargin(0.0f, 0.0f, 0.0f, 14.0f)
	);

	PresetDeleteConfirmationText = MakeTextBlock(
		WidgetTree,
		TEXT("¿Eliminar este preset?"),
		FLinearColor(0.92f, 0.94f, 0.97f, 1.0f),
		18
	);
	PresetDeleteConfirmationText->SetJustification(ETextJustify::Center);
	PresetDeleteConfirmationText->SetAutoWrapText(true);
	AddVerticalChild(
		DeleteDialogColumn,
		PresetDeleteConfirmationText,
		FMargin(0.0f, 0.0f, 0.0f, 20.0f),
		HAlign_Fill,
		ESlateSizeRule::Fill
	);

	UHorizontalBox* DeleteDialogButtons =
		WidgetTree->ConstructWidget<UHorizontalBox>();
	AddVerticalChild(DeleteDialogColumn, DeleteDialogButtons);

	PresetDeleteCancelButton = MakeButton(WidgetTree, TEXT("CANCELAR"), 17);
	PresetDeleteCancelButton->SetBackgroundColor(
		FLinearColor(0.11f, 0.16f, 0.21f, 1.0f)
	);
	PresetDeleteCancelButton->OnClicked.AddDynamic(
		this,
		&USoccerFormationMenuWidget::HandlePresetDeleteCancelClicked
	);
	AddHorizontalChild(
		DeleteDialogButtons,
		PresetDeleteCancelButton,
		FMargin(0.0f, 0.0f, 6.0f, 0.0f),
		ESlateSizeRule::Fill
	);

	PresetDeleteConfirmButton = MakeButton(WidgetTree, TEXT("ELIMINAR"), 17);
	PresetDeleteConfirmButton->SetBackgroundColor(
		FLinearColor(0.42f, 0.08f, 0.09f, 1.0f)
	);
	PresetDeleteConfirmButton->OnClicked.AddDynamic(
		this,
		&USoccerFormationMenuWidget::HandlePresetDeleteConfirmClicked
	);
	AddHorizontalChild(
		DeleteDialogButtons,
		PresetDeleteConfirmButton,
		FMargin(6.0f, 0.0f, 0.0f, 0.0f),
		ESlateSizeRule::Fill
	);

	RefreshTabVisuals();
	RefreshFocusVisuals();
	RefreshControlHints();
}

void USoccerFormationMenuWidget::BuildPresetsPage(UVerticalBox* PageRoot)
{
	if (PageRoot == nullptr)
	{
		return;
	}

	UHorizontalBox* ContentRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	AddVerticalChild(PageRoot, ContentRow, FMargin(0.0f), HAlign_Fill, ESlateSizeRule::Fill);

	UBorder* LibraryCard = MakeCard(WidgetTree);
	USizeBox* LibrarySize = WidgetTree->ConstructWidget<USizeBox>();
	LibrarySize->SetWidthOverride(620.0f);
	LibrarySize->AddChild(LibraryCard);
	AddHorizontalChild(
		ContentRow,
		LibrarySize,
		FMargin(0.0f, 0.0f, 28.0f, 0.0f),
		ESlateSizeRule::Automatic,
		VAlign_Fill
	);

	UVerticalBox* LibraryColumn = WidgetTree->ConstructWidget<UVerticalBox>();
	LibraryCard->AddChild(LibraryColumn);

	UTextBlock* LibraryTitle = MakeTextBlock(
		WidgetTree,
		TEXT("BIBLIOTECA DE ESTRATEGIAS"),
		Accent,
		23
	);
	AddVerticalChild(LibraryColumn, LibraryTitle, FMargin(0.0f, 0.0f, 0.0f, 18.0f));

	BuiltInPresetSelector = BuildSelectorRow(
		LibraryColumn,
		TEXT("DEL JUEGO"),
		145.0f,
		285.0f
	);
	BuiltInPresetSelector.PreviousButton->OnClicked.AddDynamic(
		this,
		&USoccerFormationMenuWidget::HandleBuiltInPresetPrevious
	);
	BuiltInPresetSelector.NextButton->OnClicked.AddDynamic(
		this,
		&USoccerFormationMenuWidget::HandleBuiltInPresetNext
	);

	PresetSelector = BuildSelectorRow(
		LibraryColumn,
		TEXT("MIS PRESETS"),
		145.0f,
		285.0f
	);
	PresetSelector.PreviousButton->OnClicked.AddDynamic(
		this,
		&USoccerFormationMenuWidget::HandlePresetPrevious
	);
	PresetSelector.NextButton->OnClicked.AddDynamic(
		this,
		&USoccerFormationMenuWidget::HandlePresetNext
	);

	PresetSelectionOriginText = MakeTextBlock(
		WidgetTree,
		TEXT("TIPO: PRESET DEL JUEGO · SOLO LECTURA"),
		FLinearColor(0.93f, 0.78f, 0.36f, 1.0f),
		14
	);
	AddVerticalChild(
		LibraryColumn,
		PresetSelectionOriginText,
		FMargin(2.0f, 8.0f, 2.0f, 4.0f)
	);

	ActivePresetText = MakeTextBlock(
		WidgetTree,
		TEXT("ACTIVO: configuración manual"),
		FLinearColor(0.82f, 0.93f, 1.0f, 1.0f),
		18
	);
	ActivePresetText->SetAutoWrapText(true);
	AddVerticalChild(
		LibraryColumn,
		ActivePresetText,
		FMargin(2.0f, 18.0f, 2.0f, 7.0f)
	);

	UHorizontalBox* ActivePresetStateRow =
		WidgetTree->ConstructWidget<UHorizontalBox>();
	AddVerticalChild(
		LibraryColumn,
		ActivePresetStateRow,
		FMargin(2.0f, 0.0f, 2.0f, 14.0f)
	);

	ActivePresetStateText = MakeTextBlock(
		WidgetTree,
		TEXT("ESTADO: SIN PRESET ACTIVO"),
		FLinearColor(0.70f, 0.75f, 0.80f, 1.0f),
		14
	);
	AddHorizontalChild(
		ActivePresetStateRow,
		ActivePresetStateText,
		FMargin(0.0f, 0.0f, 8.0f, 0.0f),
		ESlateSizeRule::Fill,
		VAlign_Center
	);

	PresetRevertButton = MakeButton(
		WidgetTree,
		TEXT("REVERTIR CAMBIOS"),
		13
	);
	PresetRevertButton->SetBackgroundColor(
		FLinearColor(0.36f, 0.22f, 0.06f, 1.0f)
	);
	PresetRevertButton->OnClicked.AddDynamic(
		this,
		&USoccerFormationMenuWidget::HandlePresetRevertClicked
	);
	PresetRevertButton->SetVisibility(ESlateVisibility::Collapsed);

	USizeBox* PresetRevertSize = WidgetTree->ConstructWidget<USizeBox>();
	PresetRevertSize->SetWidthOverride(185.0f);
	PresetRevertSize->SetHeightOverride(36.0f);
	PresetRevertSize->AddChild(PresetRevertButton);
	AddHorizontalChild(
		ActivePresetStateRow,
		PresetRevertSize,
		FMargin(0.0f),
		ESlateSizeRule::Automatic,
		VAlign_Center
	);

	UTextBlock* PresetPreviewTitle = MakeTextBlock(
		WidgetTree,
		TEXT("VISTA PREVIA · NO APLICA CAMBIOS"),
		Accent,
		16
	);
	AddVerticalChild(
		LibraryColumn,
		PresetPreviewTitle,
		FMargin(2.0f, 10.0f, 2.0f, 9.0f)
	);

	PresetFormationPreviewSizeBox = WidgetTree->ConstructWidget<USizeBox>();
	PresetFormationPreviewSizeBox->SetWidthOverride(PresetPreviewWidth);
	PresetFormationPreviewSizeBox->SetHeightOverride(PresetPreviewHeight);
	PresetFormationPreviewCanvas = WidgetTree->ConstructWidget<UCanvasPanel>();
	PresetFormationPreviewSizeBox->AddChild(PresetFormationPreviewCanvas);
	AddVerticalChild(
		LibraryColumn,
		PresetFormationPreviewSizeBox,
		FMargin(0.0f, 0.0f, 0.0f, 10.0f),
		HAlign_Center
	);

	PresetFormationSummaryText = MakeTextBlock(
		WidgetTree,
		TEXT("Todavía no hay presets guardados."),
		FLinearColor(0.86f, 0.90f, 0.94f, 1.0f),
		14
	);
	AddVerticalChild(
		LibraryColumn,
		PresetFormationSummaryText,
		FMargin(2.0f, 0.0f, 2.0f, 10.0f)
	);

	USizeBox* PresetTacticalSummarySize =
		WidgetTree->ConstructWidget<USizeBox>();
	PresetTacticalSummarySize->SetHeightOverride(112.0f);
	AddVerticalChild(
		LibraryColumn,
		PresetTacticalSummarySize,
		FMargin(0.0f, 0.0f, 0.0f, 10.0f)
	);

	UHorizontalBox* PresetTacticalSummaryRow =
		WidgetTree->ConstructWidget<UHorizontalBox>();
	PresetTacticalSummarySize->AddChild(PresetTacticalSummaryRow);

	UBorder* PresetAttackCard = MakeCard(
		WidgetTree,
		FLinearColor(0.035f, 0.080f, 0.065f, 0.62f)
	);
	PresetAttackCard->SetPadding(FMargin(14.0f, 11.0f));
	AddHorizontalChild(
		PresetTacticalSummaryRow,
		PresetAttackCard,
		FMargin(0.0f, 0.0f, 7.0f, 0.0f),
		ESlateSizeRule::Fill,
		VAlign_Fill
	);

	UVerticalBox* PresetAttackColumn =
		WidgetTree->ConstructWidget<UVerticalBox>();
	PresetAttackCard->AddChild(PresetAttackColumn);

	UTextBlock* PresetAttackTitle = MakeTextBlock(
		WidgetTree,
		TEXT("ATAQUE"),
		FLinearColor(0.34f, 0.88f, 0.50f, 1.0f),
		14
	);
	AddVerticalChild(
		PresetAttackColumn,
		PresetAttackTitle,
		FMargin(0.0f, 0.0f, 0.0f, 5.0f)
	);

	PresetAttackSummaryText = MakeTextBlock(
		WidgetTree,
		TEXT("--"),
		FLinearColor(0.76f, 0.82f, 0.87f, 1.0f),
		13
	);
	AddVerticalChild(PresetAttackColumn, PresetAttackSummaryText);

	UBorder* PresetDefenseCard = MakeCard(
		WidgetTree,
		FLinearColor(0.085f, 0.050f, 0.055f, 0.62f)
	);
	PresetDefenseCard->SetPadding(FMargin(14.0f, 11.0f));
	AddHorizontalChild(
		PresetTacticalSummaryRow,
		PresetDefenseCard,
		FMargin(7.0f, 0.0f, 0.0f, 0.0f),
		ESlateSizeRule::Fill,
		VAlign_Fill
	);

	UVerticalBox* PresetDefenseColumn =
		WidgetTree->ConstructWidget<UVerticalBox>();
	PresetDefenseCard->AddChild(PresetDefenseColumn);

	UTextBlock* PresetDefenseTitle = MakeTextBlock(
		WidgetTree,
		TEXT("DEFENSA"),
		FLinearColor(0.95f, 0.48f, 0.38f, 1.0f),
		14
	);
	AddVerticalChild(
		PresetDefenseColumn,
		PresetDefenseTitle,
		FMargin(0.0f, 0.0f, 0.0f, 5.0f)
	);

	PresetDefenseSummaryText = MakeTextBlock(
		WidgetTree,
		TEXT("--"),
		FLinearColor(0.76f, 0.82f, 0.87f, 1.0f),
		13
	);
	AddVerticalChild(PresetDefenseColumn, PresetDefenseSummaryText);

	PresetInstructionSummaryText = MakeTextBlock(
		WidgetTree,
		TEXT("Instrucciones personalizadas: --"),
		FLinearColor(0.66f, 0.73f, 0.79f, 1.0f),
		13
	);
	AddVerticalChild(
		LibraryColumn,
		PresetInstructionSummaryText,
		FMargin(2.0f, 0.0f, 2.0f, 0.0f)
	);

	UBorder* ActionsCard = MakeCard(WidgetTree, FLinearColor(0.045f, 0.065f, 0.085f, 0.58f));
	AddHorizontalChild(ContentRow, ActionsCard, FMargin(0.0f), ESlateSizeRule::Fill, VAlign_Fill);

	UVerticalBox* ActionsColumn = WidgetTree->ConstructWidget<UVerticalBox>();
	ActionsCard->AddChild(ActionsColumn);

	UTextBlock* ActionsTitle = MakeTextBlock(
		WidgetTree,
		TEXT("GESTIÓN DEL PRESET"),
		WarmAccent,
		23
	);
	AddVerticalChild(ActionsColumn, ActionsTitle, FMargin(0.0f, 0.0f, 0.0f, 18.0f));

	UHorizontalBox* ActionsBodyRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	AddVerticalChild(
		ActionsColumn,
		ActionsBodyRow,
		FMargin(0.0f),
		HAlign_Fill,
		ESlateSizeRule::Fill
	);

	UVerticalBox* ManagementColumn = WidgetTree->ConstructWidget<UVerticalBox>();
	AddHorizontalChild(
		ActionsBodyRow,
		ManagementColumn,
		FMargin(0.0f, 0.0f, 18.0f, 0.0f),
		ESlateSizeRule::Fill,
		VAlign_Fill
	);

	UVerticalBox* ToolsColumn = WidgetTree->ConstructWidget<UVerticalBox>();
	AddHorizontalChild(
		ActionsBodyRow,
		ToolsColumn,
		FMargin(18.0f, 0.0f, 0.0f, 0.0f),
		ESlateSizeRule::Fill,
		VAlign_Fill
	);

	UTextBlock* NameLabel = MakeTextBlock(
		WidgetTree,
		TEXT("Nombre para guardar o renombrar"),
		FLinearColor(0.78f, 0.82f, 0.86f, 1.0f),
		16
	);
	AddVerticalChild(ManagementColumn, NameLabel, FMargin(0.0f, 0.0f, 0.0f, 8.0f));

	USizeBox* NameSize = WidgetTree->ConstructWidget<USizeBox>();
	NameSize->SetHeightOverride(46.0f);
	PresetNameTextBox = WidgetTree->ConstructWidget<UEditableTextBox>();
	PresetNameTextBox->SetHintText(FText::FromString(TEXT("Ej. Presión alta")));
	NameSize->AddChild(PresetNameTextBox);
	AddVerticalChild(ManagementColumn, NameSize, FMargin(0.0f, 0.0f, 0.0f, 20.0f));

	PresetApplyButton = MakeButton(WidgetTree, TEXT("APLICAR SELECCIONADO"), 17);
	PresetApplyButton->SetBackgroundColor(FLinearColor(0.08f, 0.42f, 0.28f, 0.96f));
	PresetApplyButton->OnClicked.AddDynamic(
		this,
		&USoccerFormationMenuWidget::HandlePresetApplyClicked
	);
	USizeBox* ApplySize = WidgetTree->ConstructWidget<USizeBox>();
	ApplySize->SetHeightOverride(50.0f);
	ApplySize->AddChild(PresetApplyButton);
	AddVerticalChild(ManagementColumn, ApplySize, FMargin(0.0f, 0.0f, 0.0f, 22.0f));

	UTextBlock* SaveTitle = MakeTextBlock(
		WidgetTree,
		TEXT("GUARDAR CAMBIOS"),
		FLinearColor(0.72f, 0.78f, 0.83f, 1.0f),
		15
	);
	AddVerticalChild(ManagementColumn, SaveTitle, FMargin(0.0f, 0.0f, 0.0f, 9.0f));

	UHorizontalBox* SaveRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	AddVerticalChild(ManagementColumn, SaveRow, FMargin(0.0f, 0.0f, 0.0f, 20.0f));

	PresetSaveCurrentButton = MakeButton(WidgetTree, TEXT("GUARDAR ACTUAL"), 16);
	PresetSaveCurrentButton->OnClicked.AddDynamic(
		this,
		&USoccerFormationMenuWidget::HandlePresetSaveCurrentClicked
	);
	AddHorizontalChild(
		SaveRow,
		PresetSaveCurrentButton,
		FMargin(0.0f, 0.0f, 7.0f, 0.0f),
		ESlateSizeRule::Fill
	);

	PresetOverwriteButton = MakeButton(WidgetTree, TEXT("SOBRESCRIBIR"), 16);
	PresetOverwriteButton->OnClicked.AddDynamic(
		this,
		&USoccerFormationMenuWidget::HandlePresetOverwriteClicked
	);
	AddHorizontalChild(
		SaveRow,
		PresetOverwriteButton,
		FMargin(7.0f, 0.0f, 0.0f, 0.0f),
		ESlateSizeRule::Fill
	);

	UTextBlock* OrganizeTitle = MakeTextBlock(
		WidgetTree,
		TEXT("ORGANIZAR BIBLIOTECA"),
		FLinearColor(0.72f, 0.78f, 0.83f, 1.0f),
		15
	);
	AddVerticalChild(ManagementColumn, OrganizeTitle, FMargin(0.0f, 0.0f, 0.0f, 9.0f));

	UHorizontalBox* ManageRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	AddVerticalChild(ManagementColumn, ManageRow, FMargin(0.0f, 0.0f, 0.0f, 12.0f));

	PresetRenameButton = MakeButton(WidgetTree, TEXT("RENOMBRAR"), 16);
	PresetRenameButton->OnClicked.AddDynamic(
		this,
		&USoccerFormationMenuWidget::HandlePresetRenameClicked
	);
	AddHorizontalChild(
		ManageRow,
		PresetRenameButton,
		FMargin(0.0f, 0.0f, 7.0f, 0.0f),
		ESlateSizeRule::Fill
	);

	PresetDuplicateButton = MakeButton(WidgetTree, TEXT("DUPLICAR"), 16);
	PresetDuplicateButton->OnClicked.AddDynamic(
		this,
		&USoccerFormationMenuWidget::HandlePresetDuplicateClicked
	);
	AddHorizontalChild(
		ManageRow,
		PresetDuplicateButton,
		FMargin(7.0f, 0.0f, 0.0f, 0.0f),
		ESlateSizeRule::Fill
	);

	// Destructive actions are kept away from the everyday organization controls.
	PresetDeleteButton = MakeButton(WidgetTree, TEXT("ELIMINAR PRESET..."), 15);
	PresetDeleteButton->SetBackgroundColor(FLinearColor(0.34f, 0.10f, 0.11f, 0.86f));
	PresetDeleteButton->OnClicked.AddDynamic(
		this,
		&USoccerFormationMenuWidget::HandlePresetDeleteClicked
	);
	UHorizontalBox* OrderRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	AddVerticalChild(ManagementColumn, OrderRow, FMargin(0.0f, 0.0f, 0.0f, 24.0f));

	UTextBlock* OrderLabel = MakeTextBlock(
		WidgetTree,
		TEXT("ORDEN EN LA BIBLIOTECA"),
		FLinearColor(0.66f, 0.72f, 0.78f, 1.0f),
		14
	);
	AddHorizontalChild(
		OrderRow,
		OrderLabel,
		FMargin(0.0f, 0.0f, 10.0f, 0.0f),
		ESlateSizeRule::Fill,
		VAlign_Center
	);

	PresetMoveUpButton = MakeButton(WidgetTree, TEXT("SUBIR"), 14);
	PresetMoveUpButton->OnClicked.AddDynamic(
		this,
		&USoccerFormationMenuWidget::HandlePresetMoveUpClicked
	);
	USizeBox* MoveUpSize = WidgetTree->ConstructWidget<USizeBox>();
	MoveUpSize->SetWidthOverride(112.0f);
	MoveUpSize->SetHeightOverride(38.0f);
	MoveUpSize->AddChild(PresetMoveUpButton);
	AddHorizontalChild(
		OrderRow,
		MoveUpSize,
		FMargin(0.0f, 0.0f, 6.0f, 0.0f),
		ESlateSizeRule::Automatic,
		VAlign_Center
	);

	PresetMoveDownButton = MakeButton(WidgetTree, TEXT("BAJAR"), 14);
	PresetMoveDownButton->OnClicked.AddDynamic(
		this,
		&USoccerFormationMenuWidget::HandlePresetMoveDownClicked
	);
	USizeBox* MoveDownSize = WidgetTree->ConstructWidget<USizeBox>();
	MoveDownSize->SetWidthOverride(112.0f);
	MoveDownSize->SetHeightOverride(38.0f);
	MoveDownSize->AddChild(PresetMoveDownButton);
	AddHorizontalChild(
		OrderRow,
		MoveDownSize,
		FMargin(6.0f, 0.0f, 0.0f, 0.0f),
		ESlateSizeRule::Automatic,
		VAlign_Center
	);

	UTextBlock* DangerTitle = MakeTextBlock(
		WidgetTree,
		TEXT("ZONA DE ELIMINACIÓN"),
		FLinearColor(0.84f, 0.56f, 0.56f, 1.0f),
		14
	);
	AddVerticalChild(ManagementColumn, DangerTitle, FMargin(0.0f, 8.0f, 0.0f, 9.0f));

	USizeBox* DeletePresetSize = WidgetTree->ConstructWidget<USizeBox>();
	DeletePresetSize->SetWidthOverride(230.0f);
	DeletePresetSize->SetHeightOverride(42.0f);
	DeletePresetSize->AddChild(PresetDeleteButton);
	AddVerticalChild(ManagementColumn, DeletePresetSize, FMargin(0.0f), HAlign_Left);

	UTextBlock* JsonTitle = MakeTextBlock(
		WidgetTree,
		TEXT("IMPORTAR / EXPORTAR JSON"),
		WarmAccent,
		18
	);
	AddVerticalChild(
		ToolsColumn,
		JsonTitle,
		FMargin(0.0f, 2.0f, 0.0f, 10.0f)
	);

	UTextBlock* JsonHelp = MakeTextBlock(
		WidgetTree,
		TEXT("Los archivos se intercambian en Saved/TacticalPresets/Exchange. Exportar no cambia el equipo; importar crea un preset local nuevo y tampoco lo aplica."),
		FLinearColor(0.66f, 0.72f, 0.78f, 1.0f),
		14
	);
	JsonHelp->SetAutoWrapText(true);
	AddVerticalChild(
		ToolsColumn,
		JsonHelp,
		FMargin(0.0f, 0.0f, 0.0f, 10.0f)
	);

	JsonFileSelector = BuildSelectorRow(
		ToolsColumn,
		TEXT("JSON DETECTADO"),
		130.0f,
		220.0f
	);
	JsonFileSelector.PreviousButton->OnClicked.AddDynamic(
		this,
		&USoccerFormationMenuWidget::HandlePresetJsonPreviousClicked
	);
	JsonFileSelector.NextButton->OnClicked.AddDynamic(
		this,
		&USoccerFormationMenuWidget::HandlePresetJsonNextClicked
	);

	UHorizontalBox* JsonActionRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	AddVerticalChild(
		ToolsColumn,
		JsonActionRow,
		FMargin(0.0f, 8.0f, 0.0f, 24.0f)
	);

	PresetJsonExportButton = MakeButton(WidgetTree, TEXT("EXPORTAR JSON"), 14);
	PresetJsonExportButton->OnClicked.AddDynamic(
		this,
		&USoccerFormationMenuWidget::HandlePresetJsonExportClicked
	);
	AddHorizontalChild(
		JsonActionRow,
		PresetJsonExportButton,
		FMargin(0.0f, 0.0f, 6.0f, 0.0f),
		ESlateSizeRule::Fill
	);

	PresetJsonImportButton = MakeButton(WidgetTree, TEXT("IMPORTAR JSON"), 14);
	PresetJsonImportButton->SetBackgroundColor(
		FLinearColor(0.08f, 0.32f, 0.43f, 0.94f)
	);
	PresetJsonImportButton->OnClicked.AddDynamic(
		this,
		&USoccerFormationMenuWidget::HandlePresetJsonImportClicked
	);
	AddHorizontalChild(
		JsonActionRow,
		PresetJsonImportButton,
		FMargin(6.0f, 0.0f, 6.0f, 0.0f),
		ESlateSizeRule::Fill
	);

	PresetJsonRefreshButton = MakeButton(WidgetTree, TEXT("ACTUALIZAR"), 14);
	PresetJsonRefreshButton->OnClicked.AddDynamic(
		this,
		&USoccerFormationMenuWidget::HandlePresetJsonRefreshClicked
	);
	AddHorizontalChild(
		JsonActionRow,
		PresetJsonRefreshButton,
		FMargin(6.0f, 0.0f, 0.0f, 0.0f),
		ESlateSizeRule::Fill
	);

	UTextBlock* QuickTitle = MakeTextBlock(
		WidgetTree,
		TEXT("ACCESOS RÁPIDOS EN PARTIDO"),
		Accent,
		18
	);
	AddVerticalChild(
		ToolsColumn,
		QuickTitle,
		FMargin(0.0f, 2.0f, 0.0f, 10.0f)
	);

	UTextBlock* QuickHelp = MakeTextBlock(
		WidgetTree,
		TEXT("Seleccioná un preset y asignalo a 1-4. Si pulsás otra vez el mismo slot, se libera."),
		FLinearColor(0.66f, 0.72f, 0.78f, 1.0f),
		15
	);
	QuickHelp->SetAutoWrapText(true);
	AddVerticalChild(
		ToolsColumn,
		QuickHelp,
		FMargin(0.0f, 0.0f, 0.0f, 10.0f)
	);

	QuickPresetSlotButtons.Empty();
	QuickPresetSlotTexts.Empty();

	USizeBox* QuickRowSize = WidgetTree->ConstructWidget<USizeBox>();
	QuickRowSize->SetHeightOverride(42.0f);
	UHorizontalBox* QuickRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	QuickRowSize->AddChild(QuickRow);
	AddVerticalChild(ToolsColumn, QuickRowSize, FMargin(0.0f, 0.0f, 0.0f, 14.0f));

	for (int32 QuickSlotIndex = 0; QuickSlotIndex < 4; ++QuickSlotIndex)
	{
		UButton* QuickButton = WidgetTree->ConstructWidget<UButton>();
		QuickButton->SetBackgroundColor(FLinearColor(0.08f, 0.11f, 0.15f, 0.90f));

		UTextBlock* QuickText = MakeTextBlock(
			WidgetTree,
			FString::Printf(TEXT("%d: VACÍO"), QuickSlotIndex + 1),
			FLinearColor::White,
			15
		);
		QuickText->SetJustification(ETextJustify::Center);
		QuickButton->AddChild(QuickText);

		QuickPresetSlotButtons.Add(QuickButton);
		QuickPresetSlotTexts.Add(QuickText);

		AddHorizontalChild(
			QuickRow,
			QuickButton,
			FMargin(
				QuickSlotIndex == 0 ? 0.0f : 3.0f,
				0.0f,
				QuickSlotIndex == 3 ? 0.0f : 3.0f,
				0.0f
			),
			ESlateSizeRule::Fill
		);
	}

	if (QuickPresetSlotButtons.Num() == 4)
	{
		QuickPresetSlotButtons[0]->OnClicked.AddDynamic(
			this,
			&USoccerFormationMenuWidget::HandleQuickPresetSlot1Clicked
		);
		QuickPresetSlotButtons[1]->OnClicked.AddDynamic(
			this,
			&USoccerFormationMenuWidget::HandleQuickPresetSlot2Clicked
		);
		QuickPresetSlotButtons[2]->OnClicked.AddDynamic(
			this,
			&USoccerFormationMenuWidget::HandleQuickPresetSlot3Clicked
		);
		QuickPresetSlotButtons[3]->OnClicked.AddDynamic(
			this,
			&USoccerFormationMenuWidget::HandleQuickPresetSlot4Clicked
		);
	}

	UTextBlock* StorageHelp = MakeTextBlock(
		WidgetTree,
		TEXT("Cada preset guarda formación, táctica colectiva e instrucciones por puesto. Los accesos 1-4 también quedan guardados. Durante el partido, Tab (teclado) o View/Back (gamepad) abre el selector rápido."),
		FLinearColor(0.58f, 0.66f, 0.72f, 1.0f),
		15
	);
	StorageHelp->SetAutoWrapText(true);
	AddVerticalChild(ToolsColumn, StorageHelp, FMargin(2.0f, 4.0f, 2.0f, 0.0f));
}

void USoccerFormationMenuWidget::BuildFormationPage(UVerticalBox* PageRoot)
{
	if (PageRoot == nullptr)
	{
		return;
	}

	UHorizontalBox* ContentRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	AddVerticalChild(PageRoot, ContentRow, FMargin(0.0f), HAlign_Fill, ESlateSizeRule::Fill);

	UBorder* LeftCard = MakeCard(WidgetTree);
	USizeBox* LeftSize = WidgetTree->ConstructWidget<USizeBox>();
	LeftSize->SetWidthOverride(440.0f);
	LeftSize->AddChild(LeftCard);
	AddHorizontalChild(ContentRow, LeftSize, FMargin(0.0f, 0.0f, 28.0f, 0.0f));

	UVerticalBox* LeftColumn = WidgetTree->ConstructWidget<UVerticalBox>();
	LeftCard->AddChild(LeftColumn);

	UTextBlock* SectionTitle = MakeTextBlock(WidgetTree, TEXT("SISTEMA DE JUEGO"), Accent, 23);
	AddVerticalChild(LeftColumn, SectionTitle, FMargin(0.0f, 0.0f, 0.0f, 18.0f));

	FormationSelector = BuildSelectorRow(LeftColumn, TEXT("Formación"), 120.0f, 165.0f);
	FormationSelector.PreviousButton->OnClicked.AddDynamic(this, &USoccerFormationMenuWidget::HandleFormationPrevious);
	FormationSelector.NextButton->OnClicked.AddDynamic(this, &USoccerFormationMenuWidget::HandleFormationNext);

	CurrentFormationText = MakeTextBlock(
		WidgetTree,
		TEXT("Tu sistema\n--"),
		FLinearColor::White,
		21
	);
	CurrentFormationText->SetAutoWrapText(true);
	AddVerticalChild(LeftColumn, CurrentFormationText, FMargin(2.0f, 26.0f, 2.0f, 12.0f));

	OpponentFormationText = MakeTextBlock(
		WidgetTree,
		TEXT("Sistema rival\n--"),
		MutedText,
		19
	);
	OpponentFormationText->SetAutoWrapText(true);
	AddVerticalChild(LeftColumn, OpponentFormationText, FMargin(2.0f, 6.0f));

	UTextBlock* FormationHelp = MakeTextBlock(
		WidgetTree,
		TEXT("La estructura cambia de forma progresiva. Los roles, la táctica y las decisiones locales siguen siendo independientes de la formación."),
		FLinearColor(0.58f, 0.66f, 0.72f, 1.0f),
		16
	);
	FormationHelp->SetAutoWrapText(true);
	AddVerticalChild(LeftColumn, FormationHelp, FMargin(2.0f, 28.0f, 2.0f, 0.0f));

	UBorder* PreviewCard = MakeCard(WidgetTree, FLinearColor(0.030f, 0.070f, 0.050f, 0.46f));
	AddHorizontalChild(ContentRow, PreviewCard, FMargin(0.0f), ESlateSizeRule::Fill);

	UVerticalBox* PreviewColumn = WidgetTree->ConstructWidget<UVerticalBox>();
	PreviewCard->AddChild(PreviewColumn);

	UTextBlock* PreviewTitle = MakeTextBlock(WidgetTree, TEXT("VISTA DE LA FORMACIÓN"), FLinearColor(0.82f, 0.92f, 0.85f, 1.0f), 20);
	PreviewTitle->SetJustification(ETextJustify::Center);
	AddVerticalChild(PreviewColumn, PreviewTitle, FMargin(0.0f, 0.0f, 0.0f, 14.0f));

	USizeBox* PreviewSizeBox = WidgetTree->ConstructWidget<USizeBox>();
	PreviewSizeBox->SetWidthOverride(PreviewWidth);
	PreviewSizeBox->SetHeightOverride(PreviewHeight);
	FormationPreviewCanvas = WidgetTree->ConstructWidget<UCanvasPanel>();
	PreviewSizeBox->AddChild(FormationPreviewCanvas);

	UScaleBox* PreviewScaleBox = WidgetTree->ConstructWidget<UScaleBox>();
	PreviewScaleBox->SetStretch(EStretch::ScaleToFit);
	PreviewScaleBox->SetStretchDirection(EStretchDirection::DownOnly);
	PreviewScaleBox->AddChild(PreviewSizeBox);
	AddVerticalChild(
		PreviewColumn,
		PreviewScaleBox,
		FMargin(0.0f),
		HAlign_Fill,
		ESlateSizeRule::Fill
	);
}

void USoccerFormationMenuWidget::BuildTacticsPage(UVerticalBox* PageRoot)
{
	if (PageRoot == nullptr)
	{
		return;
	}

	UHorizontalBox* Columns = WidgetTree->ConstructWidget<UHorizontalBox>();
	AddVerticalChild(PageRoot, Columns, FMargin(0.0f, 0.0f, 0.0f, 22.0f), HAlign_Fill, ESlateSizeRule::Fill);

	UBorder* AttackCard = MakeCard(WidgetTree);
	UBorder* DefenseCard = MakeCard(WidgetTree);
	AddHorizontalChild(Columns, AttackCard, FMargin(0.0f, 0.0f, 14.0f, 0.0f), ESlateSizeRule::Fill, VAlign_Fill);
	AddHorizontalChild(Columns, DefenseCard, FMargin(14.0f, 0.0f, 0.0f, 0.0f), ESlateSizeRule::Fill, VAlign_Fill);

	UVerticalBox* AttackColumn = WidgetTree->ConstructWidget<UVerticalBox>();
	AttackCard->AddChild(AttackColumn);
	UTextBlock* AttackTitle = MakeTextBlock(WidgetTree, TEXT("ATAQUE"), FLinearColor(0.34f, 0.88f, 0.50f, 1.0f), 23);
	AddVerticalChild(AttackColumn, AttackTitle, FMargin(0.0f, 0.0f, 0.0f, 16.0f));

	BuildUpSelector = BuildSelectorRow(AttackColumn, TEXT("Salida"));
	AttackChannelSelector = BuildSelectorRow(AttackColumn, TEXT("Canal"));
	AttackingWidthSelector = BuildSelectorRow(AttackColumn, TEXT("Amplitud"));
	AttackingTempoSelector = BuildSelectorRow(AttackColumn, TEXT("Ritmo"));
	AttackingTransitionSelector = BuildSelectorRow(AttackColumn, TEXT("Al recuperar"));

	BuildUpSelector.PreviousButton->OnClicked.AddDynamic(this, &USoccerFormationMenuWidget::HandleBuildUpPrevious);
	BuildUpSelector.NextButton->OnClicked.AddDynamic(this, &USoccerFormationMenuWidget::HandleBuildUpNext);
	AttackChannelSelector.PreviousButton->OnClicked.AddDynamic(this, &USoccerFormationMenuWidget::HandleAttackChannelPrevious);
	AttackChannelSelector.NextButton->OnClicked.AddDynamic(this, &USoccerFormationMenuWidget::HandleAttackChannelNext);
	AttackingWidthSelector.PreviousButton->OnClicked.AddDynamic(this, &USoccerFormationMenuWidget::HandleAttackingWidthPrevious);
	AttackingWidthSelector.NextButton->OnClicked.AddDynamic(this, &USoccerFormationMenuWidget::HandleAttackingWidthNext);
	AttackingTempoSelector.PreviousButton->OnClicked.AddDynamic(this, &USoccerFormationMenuWidget::HandleAttackingTempoPrevious);
	AttackingTempoSelector.NextButton->OnClicked.AddDynamic(this, &USoccerFormationMenuWidget::HandleAttackingTempoNext);
	AttackingTransitionSelector.PreviousButton->OnClicked.AddDynamic(this, &USoccerFormationMenuWidget::HandleAttackingTransitionPrevious);
	AttackingTransitionSelector.NextButton->OnClicked.AddDynamic(this, &USoccerFormationMenuWidget::HandleAttackingTransitionNext);

	UVerticalBox* DefenseColumn = WidgetTree->ConstructWidget<UVerticalBox>();
	DefenseCard->AddChild(DefenseColumn);
	UTextBlock* DefenseTitle = MakeTextBlock(WidgetTree, TEXT("DEFENSA"), FLinearColor(0.95f, 0.48f, 0.38f, 1.0f), 23);
	AddVerticalChild(DefenseColumn, DefenseTitle, FMargin(0.0f, 0.0f, 0.0f, 16.0f));

	DefensiveBlockSelector = BuildSelectorRow(DefenseColumn, TEXT("Bloque"));
	PressingIntensitySelector = BuildSelectorRow(DefenseColumn, TEXT("Presión"));
	MarkingStyleSelector = BuildSelectorRow(DefenseColumn, TEXT("Marcaje"));
	DefensiveTransitionSelector = BuildSelectorRow(DefenseColumn, TEXT("Al perder"));

	DefensiveBlockSelector.PreviousButton->OnClicked.AddDynamic(this, &USoccerFormationMenuWidget::HandleDefensiveBlockPrevious);
	DefensiveBlockSelector.NextButton->OnClicked.AddDynamic(this, &USoccerFormationMenuWidget::HandleDefensiveBlockNext);
	PressingIntensitySelector.PreviousButton->OnClicked.AddDynamic(this, &USoccerFormationMenuWidget::HandlePressingIntensityPrevious);
	PressingIntensitySelector.NextButton->OnClicked.AddDynamic(this, &USoccerFormationMenuWidget::HandlePressingIntensityNext);
	MarkingStyleSelector.PreviousButton->OnClicked.AddDynamic(this, &USoccerFormationMenuWidget::HandleMarkingStylePrevious);
	MarkingStyleSelector.NextButton->OnClicked.AddDynamic(this, &USoccerFormationMenuWidget::HandleMarkingStyleNext);
	DefensiveTransitionSelector.PreviousButton->OnClicked.AddDynamic(this, &USoccerFormationMenuWidget::HandleDefensiveTransitionPrevious);
	DefensiveTransitionSelector.NextButton->OnClicked.AddDynamic(this, &USoccerFormationMenuWidget::HandleDefensiveTransitionNext);

	UHorizontalBox* SummaryRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	AddVerticalChild(PageRoot, SummaryRow);

	UBorder* PlayerSummaryCard = MakeCard(WidgetTree, FLinearColor(0.045f, 0.090f, 0.120f, 0.60f));
	UBorder* RivalSummaryCard = MakeCard(WidgetTree, FLinearColor(0.070f, 0.070f, 0.078f, 0.60f));
	AddHorizontalChild(SummaryRow, PlayerSummaryCard, FMargin(0.0f, 0.0f, 14.0f, 0.0f), ESlateSizeRule::Fill, VAlign_Fill);
	AddHorizontalChild(SummaryRow, RivalSummaryCard, FMargin(14.0f, 0.0f, 0.0f, 0.0f), ESlateSizeRule::Fill, VAlign_Fill);

	TacticSummaryText = MakeTextBlock(WidgetTree, TEXT("TU PLAN\n--"), FLinearColor(0.82f, 0.93f, 1.0f, 1.0f), 17);
	TacticSummaryText->SetAutoWrapText(true);
	PlayerSummaryCard->AddChild(TacticSummaryText);

	OpponentTacticText = MakeTextBlock(WidgetTree, TEXT("RIVAL\n--"), FLinearColor(0.72f, 0.74f, 0.78f, 1.0f), 17);
	OpponentTacticText->SetAutoWrapText(true);
	RivalSummaryCard->AddChild(OpponentTacticText);
}

void USoccerFormationMenuWidget::BuildInstructionsPage(UVerticalBox* PageRoot)
{
	if (PageRoot == nullptr)
	{
		return;
	}

	UHorizontalBox* ContentRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	AddVerticalChild(PageRoot, ContentRow, FMargin(0.0f), HAlign_Fill, ESlateSizeRule::Fill);

	UBorder* SlotCard = MakeCard(WidgetTree);
	USizeBox* SlotCardSize = WidgetTree->ConstructWidget<USizeBox>();
	SlotCardSize->SetWidthOverride(450.0f);
	SlotCardSize->AddChild(SlotCard);
	AddHorizontalChild(ContentRow, SlotCardSize, FMargin(0.0f, 0.0f, 28.0f, 0.0f), ESlateSizeRule::Automatic, VAlign_Fill);

	UVerticalBox* SlotColumn = WidgetTree->ConstructWidget<UVerticalBox>();
	SlotCard->AddChild(SlotColumn);
	UTextBlock* SlotTitle = MakeTextBlock(WidgetTree, TEXT("PUESTO"), WarmAccent, 23);
	AddVerticalChild(SlotColumn, SlotTitle, FMargin(0.0f, 0.0f, 0.0f, 18.0f));

	InstructionSlotSelector = BuildSelectorRow(SlotColumn, TEXT("Slot"), 95.0f, 180.0f);
	InstructionSlotSelector.PreviousButton->OnClicked.AddDynamic(this, &USoccerFormationMenuWidget::HandleInstructionSlotPrevious);
	InstructionSlotSelector.NextButton->OnClicked.AddDynamic(this, &USoccerFormationMenuWidget::HandleInstructionSlotNext);

	IndividualInstructionPlayerText = MakeTextBlock(
		WidgetTree,
		TEXT("JUGADOR ASIGNADO\n--"),
		FLinearColor(0.88f, 0.90f, 0.92f, 1.0f),
		19
	);
	IndividualInstructionPlayerText->SetAutoWrapText(true);
	AddVerticalChild(SlotColumn, IndividualInstructionPlayerText, FMargin(2.0f, 28.0f, 2.0f, 10.0f));

	UTextBlock* SlotHelp = MakeTextBlock(
		WidgetTree,
		TEXT("Las instrucciones pertenecen al puesto de la formación. Si cambia el jugador asignado, el nuevo ocupante hereda las prioridades del slot."),
		FLinearColor(0.58f, 0.66f, 0.72f, 1.0f),
		16
	);
	SlotHelp->SetAutoWrapText(true);
	AddVerticalChild(SlotColumn, SlotHelp, FMargin(2.0f, 26.0f, 2.0f, 0.0f));

	UBorder* InstructionCard = MakeCard(WidgetTree);
	AddHorizontalChild(ContentRow, InstructionCard, FMargin(0.0f), ESlateSizeRule::Fill, VAlign_Fill);

	UVerticalBox* InstructionColumn = WidgetTree->ConstructWidget<UVerticalBox>();
	InstructionCard->AddChild(InstructionColumn);
	UTextBlock* InstructionTitle = MakeTextBlock(WidgetTree, TEXT("PRIORIDADES DEL PUESTO"), Accent, 23);
	AddVerticalChild(InstructionColumn, InstructionTitle, FMargin(0.0f, 0.0f, 0.0f, 18.0f));

	IndividualAttackSelector = BuildSelectorRow(InstructionColumn, TEXT("Ataque"), 200.0f, 330.0f);
	IndividualDefenseSelector = BuildSelectorRow(InstructionColumn, TEXT("Defensa"), 200.0f, 330.0f);
	MarkingTargetSelector = BuildSelectorRow(InstructionColumn, TEXT("Marca individual"), 200.0f, 330.0f);

	IndividualAttackSelector.PreviousButton->OnClicked.AddDynamic(this, &USoccerFormationMenuWidget::HandleIndividualAttackPrevious);
	IndividualAttackSelector.NextButton->OnClicked.AddDynamic(this, &USoccerFormationMenuWidget::HandleIndividualAttackNext);
	IndividualDefenseSelector.PreviousButton->OnClicked.AddDynamic(this, &USoccerFormationMenuWidget::HandleIndividualDefensePrevious);
	IndividualDefenseSelector.NextButton->OnClicked.AddDynamic(this, &USoccerFormationMenuWidget::HandleIndividualDefenseNext);
	MarkingTargetSelector.PreviousButton->OnClicked.AddDynamic(this, &USoccerFormationMenuWidget::HandleMarkingTargetPrevious);
	MarkingTargetSelector.NextButton->OnClicked.AddDynamic(this, &USoccerFormationMenuWidget::HandleMarkingTargetNext);

	UBorder* SummaryCard = MakeCard(WidgetTree, FLinearColor(0.075f, 0.070f, 0.045f, 0.62f));
	AddVerticalChild(InstructionColumn, SummaryCard, FMargin(0.0f, 26.0f, 0.0f, 0.0f));
	IndividualInstructionSummaryText = MakeTextBlock(
		WidgetTree,
		TEXT("RESUMEN\n--"),
		FLinearColor(0.93f, 0.86f, 0.62f, 1.0f),
		17
	);
	IndividualInstructionSummaryText->SetAutoWrapText(true);
	SummaryCard->AddChild(IndividualInstructionSummaryText);
}

USoccerFormationMenuWidget::FSelectorRow
USoccerFormationMenuWidget::BuildSelectorRow(
	UVerticalBox* Parent,
	const FString& Label,
	float LabelWidth,
	float ValueWidth
)
{
	FSelectorRow Result;
	if (WidgetTree == nullptr || Parent == nullptr)
	{
		return Result;
	}

	Result.Border = WidgetTree->ConstructWidget<UBorder>();
	Result.Border->SetBrushColor(SelectorBackground);
	Result.Border->SetPadding(FMargin(12.0f, 9.0f));
	AddVerticalChild(Parent, Result.Border, FMargin(0.0f, 0.0f, 0.0f, 11.0f));

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	Result.Border->AddChild(Row);

	USizeBox* LabelSize = WidgetTree->ConstructWidget<USizeBox>();
	LabelSize->SetWidthOverride(LabelWidth);
	Result.LabelText = MakeTextBlock(WidgetTree, Label, FLinearColor(0.80f, 0.84f, 0.88f, 1.0f), 17);
	LabelSize->AddChild(Result.LabelText);
	AddHorizontalChild(Row, LabelSize, FMargin(0.0f, 0.0f, 12.0f, 0.0f));

	USizeBox* PreviousSize = WidgetTree->ConstructWidget<USizeBox>();
	PreviousSize->SetWidthOverride(46.0f);
	PreviousSize->SetHeightOverride(38.0f);
	Result.PreviousButton = MakeButton(WidgetTree, TEXT("<"), 20, Accent);
	PreviousSize->AddChild(Result.PreviousButton);
	AddHorizontalChild(Row, PreviousSize, FMargin(0.0f, 0.0f, 8.0f, 0.0f));

	USizeBox* ValueSize = WidgetTree->ConstructWidget<USizeBox>();
	ValueSize->SetWidthOverride(ValueWidth);
	Result.ValueText = MakeTextBlock(WidgetTree, TEXT("--"), FLinearColor::White, 18);
	Result.ValueText->SetJustification(ETextJustify::Center);
	ValueSize->AddChild(Result.ValueText);
	AddHorizontalChild(Row, ValueSize, FMargin(0.0f, 0.0f, 8.0f, 0.0f), ESlateSizeRule::Fill);

	USizeBox* NextSize = WidgetTree->ConstructWidget<USizeBox>();
	NextSize->SetWidthOverride(46.0f);
	NextSize->SetHeightOverride(38.0f);
	Result.NextButton = MakeButton(WidgetTree, TEXT(">"), 20, Accent);
	NextSize->AddChild(Result.NextButton);
	AddHorizontalChild(Row, NextSize);

	return Result;
}

void USoccerFormationMenuWidget::RefreshFromMatchManager()
{
	if (!IsValid(MatchManager))
	{
		return;
	}

	EnsureSelectedInstructionSlotValid();
	RefreshHeader();
	RefreshPresetsPage(false);
	RefreshFormationPage();
	RefreshTacticsPage();
	RefreshInstructionsPage();
	RefreshTabVisuals();
	RefreshFocusVisuals();
}

void USoccerFormationMenuWidget::RefreshHeader()
{
	if (!IsValid(MatchManager))
	{
		return;
	}

	const FString PlayerFormation = GetFormationDisplayName(
		MatchManager->GetFormationSystemForTeam(ESoccerTeam::PlayerTeam)
	);

	if (HeaderContextText != nullptr)
	{
		HeaderContextText->SetText(
			FText::FromString(
				FString::Printf(TEXT("PlayerTeam  |  %s"), *PlayerFormation)
			)
		);
	}

	if (HeaderOpponentText != nullptr)
	{
		HeaderOpponentText->SetText(
			FText::FromString(
				FString::Printf(
					TEXT("Rival: %s  |  %s"),
					*GetFormationDisplayName(
						MatchManager->GetFormationSystemForTeam(ESoccerTeam::OpponentTeam)
					),
					*MatchManager->GetOpponentCoachModeDisplayName()
				)
			)
		);
	}
}

void USoccerFormationMenuWidget::RefreshPresetsPage(
	bool bCopySelectedNameToEditor
)
{
	if (!IsValid(TacticalPresetManager))
	{
		if (BuiltInPresetSelector.ValueText != nullptr)
		{
			BuiltInPresetSelector.ValueText->SetText(FText::FromString(TEXT("--")));
		}
		if (PresetSelector.ValueText != nullptr)
		{
			PresetSelector.ValueText->SetText(FText::FromString(TEXT("--")));
		}
		return;
	}

	EnsureSelectedPresetValid();

	const TArray<FSoccerTacticalPreset>& BuiltInPresets =
		TacticalPresetManager->GetBuiltInPresets();
	const TArray<FSoccerTacticalPreset>& UserPresets =
		TacticalPresetManager->GetPresets();

	const FSoccerTacticalPreset* SelectedBuiltInPreset =
		TacticalPresetManager->FindBuiltInPreset(SelectedBuiltInPresetId);
	const FSoccerTacticalPreset* SelectedUserPreset =
		TacticalPresetManager->FindPreset(SelectedUserPresetId);
	const FSoccerTacticalPreset* SelectedPreset =
		TacticalPresetManager->FindAnyPreset(SelectedPresetId);

	const bool bHasSelectedPreset = SelectedPreset != nullptr;
	const bool bSelectedIsBuiltIn =
		bHasSelectedPreset && TacticalPresetManager->IsBuiltInPreset(SelectedPresetId);
	const bool bSelectedIsUserPreset =
		bHasSelectedPreset && !bSelectedIsBuiltIn;

	if (BuiltInPresetSelector.ValueText != nullptr)
	{
		BuiltInPresetSelector.ValueText->SetText(
			FText::FromString(
				SelectedBuiltInPreset != nullptr
				? SelectedBuiltInPreset->PresetName
				: TEXT("--")
			)
		);
	}
	if (BuiltInPresetSelector.PreviousButton != nullptr)
	{
		BuiltInPresetSelector.PreviousButton->SetIsEnabled(BuiltInPresets.Num() > 1);
	}
	if (BuiltInPresetSelector.NextButton != nullptr)
	{
		BuiltInPresetSelector.NextButton->SetIsEnabled(BuiltInPresets.Num() > 1);
	}

	if (PresetSelector.ValueText != nullptr)
	{
		PresetSelector.ValueText->SetText(
			FText::FromString(
				SelectedUserPreset != nullptr
				? SelectedUserPreset->PresetName
				: TEXT("-- SIN PRESETS --")
			)
		);
	}
	if (PresetSelector.PreviousButton != nullptr)
	{
		PresetSelector.PreviousButton->SetIsEnabled(UserPresets.Num() > 1);
	}
	if (PresetSelector.NextButton != nullptr)
	{
		PresetSelector.NextButton->SetIsEnabled(UserPresets.Num() > 1);
	}

	if (PresetSelectionOriginText != nullptr)
	{
		PresetSelectionOriginText->SetText(
			FText::FromString(
				!bHasSelectedPreset
				? TEXT("TIPO: SIN PRESET SELECCIONADO")
				: bSelectedIsBuiltIn
				? TEXT("TIPO: PRESET DEL JUEGO · SOLO LECTURA")
				: TEXT("TIPO: PRESET DEL USUARIO · EDITABLE")
			)
		);
		PresetSelectionOriginText->SetColorAndOpacity(
			FSlateColor(
				bSelectedIsBuiltIn
				? FLinearColor(0.93f, 0.78f, 0.36f, 1.0f)
				: bSelectedIsUserPreset
				? FLinearColor(0.48f, 0.82f, 1.0f, 1.0f)
				: FLinearColor(0.70f, 0.75f, 0.80f, 1.0f)
			)
		);
	}

	if (bCopySelectedNameToEditor && PresetNameTextBox != nullptr)
	{
		PresetNameTextBox->SetText(
			FText::FromString(
				bHasSelectedPreset
				? SelectedPreset->PresetName
				: TEXT("")
			)
		);
	}

	if (PresetFormationPreviewSizeBox != nullptr)
	{
		PresetFormationPreviewSizeBox->SetVisibility(
			bHasSelectedPreset
			? ESlateVisibility::Visible
			: ESlateVisibility::Collapsed
		);
	}

	if (PresetFormationPreviewCanvas != nullptr)
	{
		if (bHasSelectedPreset)
		{
			RefreshFormationPreviewCanvas(
				PresetFormationPreviewCanvas,
				SelectedPreset->FormationSystem,
				PresetPreviewWidth,
				PresetPreviewHeight,
				true
			);
		}
		else
		{
			PresetFormationPreviewCanvas->ClearChildren();
		}
	}

	if (bHasSelectedPreset)
	{
		int32 CustomizedInstructionCount = 0;
		for (const FSoccerSlotTacticalInstruction& Instruction :
			SelectedPreset->SlotInstructions)
		{
			const bool bHasCustomAttack =
				Instruction.AttackInstruction !=
				ESoccerIndividualAttackInstruction::Balanced;
			const bool bHasCustomDefense =
				Instruction.DefensiveInstruction !=
				ESoccerIndividualDefensiveInstruction::Balanced;
			const bool bHasExplicitMark =
				!Instruction.MarkingTargetSlotId.IsNone();

			if (bHasCustomAttack || bHasCustomDefense || bHasExplicitMark)
			{
				++CustomizedInstructionCount;
			}
		}

		const FSoccerTeamTacticalPlan& Plan = SelectedPreset->TacticalPlan;

		if (PresetFormationSummaryText != nullptr)
		{
			PresetFormationSummaryText->SetText(
				FText::FromString(
					FString::Printf(
						TEXT("Formación: %s"),
						*GetFormationDisplayName(SelectedPreset->FormationSystem)
					)
				)
			);
		}

		if (PresetAttackSummaryText != nullptr)
		{
			PresetAttackSummaryText->SetText(
				FText::FromString(
					FString::Printf(
						TEXT("Salida: %s\nCanal: %s\nAncho: %s\nRitmo: %s\nTransición: %s"),
						*GetTacticDisplayName(Plan.BuildUpStyle, BuildUpOptions),
						*GetTacticDisplayName(Plan.AttackChannel, AttackChannelOptions),
						*GetTacticDisplayName(Plan.AttackingWidth, AttackingWidthOptions),
						*GetTacticDisplayName(Plan.AttackingTempo, AttackingTempoOptions),
						*GetTacticDisplayName(Plan.AttackingTransition, AttackingTransitionOptions)
					)
				)
			);
		}

		if (PresetDefenseSummaryText != nullptr)
		{
			PresetDefenseSummaryText->SetText(
				FText::FromString(
					FString::Printf(
						TEXT("Bloque: %s\nPresión: %s\nMarcaje: %s\nTransición: %s"),
						*GetTacticDisplayName(Plan.DefensiveBlock, DefensiveBlockOptions),
						*GetTacticDisplayName(Plan.PressingIntensity, PressingOptions),
						*GetTacticDisplayName(Plan.MarkingStyle, MarkingOptions),
						*GetTacticDisplayName(Plan.DefensiveTransition, DefensiveTransitionOptions)
					)
				)
			);
		}

		if (PresetInstructionSummaryText != nullptr)
		{
			PresetInstructionSummaryText->SetText(
				FText::FromString(
					FString::Printf(
						TEXT("Instrucciones personalizadas: %d de %d puestos"),
						CustomizedInstructionCount,
						SelectedPreset->SlotInstructions.Num()
					)
				)
			);
		}
	}
	else
	{
		if (PresetFormationSummaryText != nullptr)
		{
			PresetFormationSummaryText->SetText(
				FText::FromString(TEXT("Sin preset seleccionado."))
			);
		}
		if (PresetAttackSummaryText != nullptr)
		{
			PresetAttackSummaryText->SetText(FText::FromString(TEXT("--")));
		}
		if (PresetDefenseSummaryText != nullptr)
		{
			PresetDefenseSummaryText->SetText(FText::FromString(TEXT("--")));
		}
		if (PresetInstructionSummaryText != nullptr)
		{
			PresetInstructionSummaryText->SetText(
				FText::FromString(TEXT("Instrucciones personalizadas: --"))
			);
		}
	}

	if (ActivePresetText != nullptr)
	{
		const FGuid ActivePresetId = TacticalPresetManager->GetActivePresetId();
		const FSoccerTacticalPreset* ActivePreset =
			TacticalPresetManager->FindAnyPreset(ActivePresetId);

		if (ActivePreset != nullptr)
		{
			const bool bModified = TacticalPresetManager->IsActivePresetModified();
			const bool bSelectedIsActive =
				bHasSelectedPreset && ActivePresetId == SelectedPresetId;
			const bool bActiveIsBuiltIn =
				TacticalPresetManager->IsBuiltInPreset(ActivePresetId);

			const FString ActivePresetLabel = bSelectedIsActive
				? FString::Printf(
					TEXT("ACTIVO: %s  ·  ES EL SELECCIONADO"),
					*ActivePreset->PresetName
				)
				: FString::Printf(
					TEXT("ACTIVO: %s"),
					*ActivePreset->PresetName
				);

			ActivePresetText->SetText(FText::FromString(ActivePresetLabel));
			ActivePresetText->SetColorAndOpacity(
				FSlateColor(
					bSelectedIsActive
					? FLinearColor(0.38f, 0.92f, 0.62f, 1.0f)
					: FLinearColor(0.82f, 0.93f, 1.0f, 1.0f)
				)
			);

			if (ActivePresetStateText != nullptr)
			{
				if (bModified)
				{
					ActivePresetStateText->SetText(
						FText::FromString(TEXT("ESTADO: MODIFICADO"))
					);
					ActivePresetStateText->SetColorAndOpacity(
						FSlateColor(FLinearColor(1.0f, 0.72f, 0.30f, 1.0f))
					);
				}
				else if (bActiveIsBuiltIn)
				{
					ActivePresetStateText->SetText(
						FText::FromString(TEXT("ESTADO: PRESET DEL JUEGO"))
					);
					ActivePresetStateText->SetColorAndOpacity(
						FSlateColor(FLinearColor(0.93f, 0.78f, 0.36f, 1.0f))
					);
				}
				else
				{
					ActivePresetStateText->SetText(
						FText::FromString(TEXT("ESTADO: GUARDADO"))
					);
					ActivePresetStateText->SetColorAndOpacity(
						FSlateColor(FLinearColor(0.38f, 0.92f, 0.62f, 1.0f))
					);
				}
			}

			if (PresetRevertButton != nullptr)
			{
				PresetRevertButton->SetIsEnabled(bModified);
				PresetRevertButton->SetVisibility(
					bModified
					? ESlateVisibility::Visible
					: ESlateVisibility::Collapsed
				);
			}
		}
		else
		{
			ActivePresetText->SetText(
				FText::FromString(TEXT("ACTIVO: configuración manual"))
			);
			ActivePresetText->SetColorAndOpacity(
				FSlateColor(FLinearColor(0.70f, 0.75f, 0.80f, 1.0f))
			);

			if (ActivePresetStateText != nullptr)
			{
				ActivePresetStateText->SetText(
					FText::FromString(TEXT("ESTADO: SIN PRESET ACTIVO"))
				);
				ActivePresetStateText->SetColorAndOpacity(
					FSlateColor(FLinearColor(0.70f, 0.75f, 0.80f, 1.0f))
				);
			}

			if (PresetRevertButton != nullptr)
			{
				PresetRevertButton->SetIsEnabled(false);
				PresetRevertButton->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
	}

	// Built-in presets are immutable. APPLY and DUPLICAR are the only actions
	// that operate directly on them; DUPLICAR creates a normal user preset.
	if (PresetApplyButton != nullptr)
	{
		PresetApplyButton->SetIsEnabled(bHasSelectedPreset);
	}
	if (PresetOverwriteButton != nullptr)
	{
		PresetOverwriteButton->SetIsEnabled(bSelectedIsUserPreset);
	}
	if (PresetRenameButton != nullptr)
	{
		PresetRenameButton->SetIsEnabled(bSelectedIsUserPreset);
	}
	if (PresetDuplicateButton != nullptr)
	{
		PresetDuplicateButton->SetIsEnabled(bHasSelectedPreset);
	}

	int32 SelectedUserPresetIndex = INDEX_NONE;
	if (SelectedUserPreset != nullptr)
	{
		SelectedUserPresetIndex = UserPresets.IndexOfByPredicate(
			[this](const FSoccerTacticalPreset& Candidate)
			{
				return Candidate.PresetId == SelectedPresetId;
			}
		);
	}

	if (PresetMoveUpButton != nullptr)
	{
		PresetMoveUpButton->SetIsEnabled(
			bSelectedIsUserPreset && SelectedUserPresetIndex > 0
		);
	}
	if (PresetMoveDownButton != nullptr)
	{
		PresetMoveDownButton->SetIsEnabled(
			bSelectedIsUserPreset &&
			SelectedUserPresetIndex != INDEX_NONE &&
			SelectedUserPresetIndex < UserPresets.Num() - 1
		);
	}

	if (PresetDeleteButton != nullptr)
	{
		PresetDeleteButton->SetIsEnabled(bSelectedIsUserPreset);
	}
	if (PresetJsonExportButton != nullptr)
	{
		PresetJsonExportButton->SetIsEnabled(bSelectedIsUserPreset);
	}

	RefreshJsonExchangeFiles(true);
	RefreshQuickPresetAssignments();
}

void USoccerFormationMenuWidget::RefreshJsonExchangeFiles(
	bool bPreserveSelection
)
{
	FString PreviouslySelectedFile;
	if (
		bPreserveSelection &&
		AvailablePresetJsonFiles.IsValidIndex(SelectedPresetJsonFileIndex)
	)
	{
		PreviouslySelectedFile =
			AvailablePresetJsonFiles[SelectedPresetJsonFileIndex];
	}

	AvailablePresetJsonFiles.Empty();
	if (IsValid(TacticalPresetManager))
	{
		TacticalPresetManager->GetAvailableJsonExchangeFiles(
			AvailablePresetJsonFiles
		);
	}

	SelectedPresetJsonFileIndex = INDEX_NONE;
	if (!PreviouslySelectedFile.IsEmpty())
	{
		SelectedPresetJsonFileIndex =
			AvailablePresetJsonFiles.IndexOfByKey(PreviouslySelectedFile);
	}
	if (
		SelectedPresetJsonFileIndex == INDEX_NONE &&
		AvailablePresetJsonFiles.Num() > 0
	)
	{
		SelectedPresetJsonFileIndex = 0;
	}

	if (JsonFileSelector.ValueText != nullptr)
	{
		const FString DisplayText =
			AvailablePresetJsonFiles.IsValidIndex(SelectedPresetJsonFileIndex)
			? AvailablePresetJsonFiles[SelectedPresetJsonFileIndex]
			: TEXT("-- SIN ARCHIVOS JSON --");
		JsonFileSelector.ValueText->SetText(FText::FromString(DisplayText));
	}

	const bool bHasJsonFile =
		AvailablePresetJsonFiles.IsValidIndex(SelectedPresetJsonFileIndex);
	if (PresetJsonImportButton != nullptr)
	{
		PresetJsonImportButton->SetIsEnabled(bHasJsonFile);
	}
	if (JsonFileSelector.PreviousButton != nullptr)
	{
		JsonFileSelector.PreviousButton->SetIsEnabled(
			AvailablePresetJsonFiles.Num() > 1
		);
	}
	if (JsonFileSelector.NextButton != nullptr)
	{
		JsonFileSelector.NextButton->SetIsEnabled(
			AvailablePresetJsonFiles.Num() > 1
		);
	}
}

void USoccerFormationMenuWidget::RefreshQuickPresetAssignments()
{
	const int32 SlotCount = FMath::Min(
		QuickPresetSlotTexts.Num(),
		QuickPresetSlotButtons.Num()
	);

	for (int32 QuickSlotIndex = 0; QuickSlotIndex < SlotCount; ++QuickSlotIndex)
	{
		UTextBlock* QuickText = QuickPresetSlotTexts[QuickSlotIndex];
		UButton* QuickButton = QuickPresetSlotButtons[QuickSlotIndex];
		if (QuickText == nullptr || QuickButton == nullptr)
		{
			continue;
		}

		const FSoccerTacticalPreset* QuickPreset =
			IsValid(TacticalPresetManager)
			? TacticalPresetManager->GetQuickPresetForSlot(QuickSlotIndex)
			: nullptr;

		QuickText->SetText(
			FText::FromString(
				QuickPreset != nullptr
				? FString::Printf(
					TEXT("%d: %s"),
					QuickSlotIndex + 1,
					*QuickPreset->PresetName
				)
				: FString::Printf(
					TEXT("%d: VACÍO"),
					QuickSlotIndex + 1
				)
			)
		);

		const bool bContainsSelectedPreset =
			QuickPreset != nullptr &&
			QuickPreset->PresetId == SelectedPresetId;

		const bool bContainsActivePreset =
			QuickPreset != nullptr &&
			IsValid(TacticalPresetManager) &&
			QuickPreset->PresetId == TacticalPresetManager->GetActivePresetId();

		QuickButton->SetBackgroundColor(
			bContainsSelectedPreset
			? FLinearColor(0.16f, 0.38f, 0.52f, 0.96f)
			: bContainsActivePreset
			? FLinearColor(0.08f, 0.38f, 0.24f, 0.96f)
			: FLinearColor(0.08f, 0.11f, 0.15f, 0.90f)
		);

		const bool bSelectedCanUseQuickSlots =
			IsValid(TacticalPresetManager) &&
			TacticalPresetManager->FindPreset(SelectedPresetId) != nullptr;
		QuickButton->SetIsEnabled(bSelectedCanUseQuickSlots);
	}
}

void USoccerFormationMenuWidget::RefreshFormationPage()
{
	if (!IsValid(MatchManager))
	{
		return;
	}

	const ESoccerFormationSystem PlayerFormation =
		MatchManager->GetFormationSystemForTeam(ESoccerTeam::PlayerTeam);
	const ESoccerFormationSystem OpponentFormation =
		MatchManager->GetFormationSystemForTeam(ESoccerTeam::OpponentTeam);

	if (FormationSelector.ValueText != nullptr)
	{
		FormationSelector.ValueText->SetText(
			FText::FromString(GetFormationDisplayName(PlayerFormation))
		);
	}

	if (CurrentFormationText != nullptr)
	{
		CurrentFormationText->SetText(
			FText::FromString(
				FString::Printf(
					TEXT("TU SISTEMA\n%s"),
					*GetFormationDisplayName(PlayerFormation)
				)
			)
		);
	}

	if (OpponentFormationText != nullptr)
	{
		OpponentFormationText->SetText(
			FText::FromString(
				FString::Printf(
					TEXT("SISTEMA RIVAL\n%s"),
					*GetFormationDisplayName(OpponentFormation)
				)
			)
		);
	}

	RefreshFormationPreview(PlayerFormation);
}

void USoccerFormationMenuWidget::RefreshFormationPreview(
	ESoccerFormationSystem FormationSystem
)
{
	RefreshFormationPreviewCanvas(
		FormationPreviewCanvas,
		FormationSystem,
		PreviewWidth,
		PreviewHeight,
		false
	);
}

void USoccerFormationMenuWidget::RefreshFormationPreviewCanvas(
	UCanvasPanel* PreviewCanvas,
	ESoccerFormationSystem FormationSystem,
	float CanvasWidth,
	float CanvasHeight,
	bool bCompact
)
{
	if (
		PreviewCanvas == nullptr ||
		WidgetTree == nullptr ||
		CanvasWidth <= 1.0f ||
		CanvasHeight <= 1.0f
	)
	{
		return;
	}

	PreviewCanvas->ClearChildren();

	UBorder* FieldBackground = WidgetTree->ConstructWidget<UBorder>();
	FieldBackground->SetBrushColor(FLinearColor(0.025f, 0.19f, 0.095f, 1.0f));
	UCanvasPanelSlot* FieldSlot = PreviewCanvas->AddChildToCanvas(FieldBackground);
	FieldSlot->SetPosition(FVector2D::ZeroVector);
	FieldSlot->SetSize(FVector2D(CanvasWidth, CanvasHeight));

	const FLinearColor LineColor(0.82f, 0.90f, 0.84f, 0.72f);
	const float OuterInset = bCompact ? 6.0f : 8.0f;
	AddCanvasOutline(
		WidgetTree,
		PreviewCanvas,
		FVector2D(OuterInset, OuterInset),
		FVector2D(CanvasWidth - OuterInset * 2.0f, CanvasHeight - OuterInset * 2.0f),
		LineColor,
		2.0f
	);

	AddCanvasRect(
		WidgetTree,
		PreviewCanvas,
		FVector2D(CanvasWidth * 0.5f - 1.0f, OuterInset),
		FVector2D(2.0f, CanvasHeight - OuterInset * 2.0f),
		LineColor
	);

	const FVector2D PenaltyBoxSize(
		CanvasWidth * (105.0f / 640.0f),
		CanvasHeight * (205.0f / 330.0f)
	);
	AddCanvasOutline(
		WidgetTree,
		PreviewCanvas,
		FVector2D(
			OuterInset,
			CanvasHeight * 0.5f - PenaltyBoxSize.Y * 0.5f
		),
		PenaltyBoxSize,
		LineColor,
		2.0f
	);
	AddCanvasOutline(
		WidgetTree,
		PreviewCanvas,
		FVector2D(
			CanvasWidth - OuterInset - PenaltyBoxSize.X,
			CanvasHeight * 0.5f - PenaltyBoxSize.Y * 0.5f
		),
		PenaltyBoxSize,
		LineColor,
		2.0f
	);

	const FSoccerFormationDefinition& Definition =
		SoccerFormationLibrary::GetDefinition(FormationSystem);

	const float HorizontalMargin =
		CanvasWidth * (46.0f / 640.0f);
	const float VerticalMargin =
		CanvasHeight * (42.0f / 330.0f);
	const float UsableWidth = CanvasWidth - HorizontalMargin * 2.0f;
	const float UsableHeight = CanvasHeight - VerticalMargin * 2.0f;
	const FVector2D SlotSize = bCompact
		? FVector2D(54.0f, 23.0f)
		: FVector2D(74.0f, 30.0f);
	const int32 SlotFontSize = bCompact ? 10 : 13;

	for (const FSoccerFormationSlot& FormationSlot : Definition.Slots)
	{
		const float NormalizedDepth =
			FMath::Clamp(FormationSlot.DepthAlpha, 0.0f, 1.0f);
		const float NormalizedLateral = FMath::Clamp(
			FormationSlot.LateralAlpha * 0.5f + 0.5f,
			0.0f,
			1.0f
		);

		const float SlotCenterX =
			HorizontalMargin + NormalizedDepth * UsableWidth;
		const float SlotCenterY =
			VerticalMargin + NormalizedLateral * UsableHeight;

		UBorder* SlotBorder = WidgetTree->ConstructWidget<UBorder>();
		FLinearColor SlotColor(0.16f, 0.56f, 0.94f, 0.98f);
		if (FormationSlot.PlayerRole == ESoccerPlayerRole::Goalkeeper)
		{
			SlotColor = FLinearColor(0.95f, 0.70f, 0.16f, 0.98f);
		}
		else if (FormationSlot.PlayerRole == ESoccerPlayerRole::Forward)
		{
			SlotColor = FLinearColor(0.90f, 0.27f, 0.22f, 0.98f);
		}
		else if (FormationSlot.PlayerRole == ESoccerPlayerRole::Midfielder)
		{
			SlotColor = FLinearColor(0.18f, 0.72f, 0.38f, 0.98f);
		}

		SlotBorder->SetBrushColor(SlotColor);
		SlotBorder->SetPadding(
			bCompact ? FMargin(2.0f, 1.0f) : FMargin(4.0f, 2.0f)
		);
		UTextBlock* SlotText = MakeTextBlock(
			WidgetTree,
			FormationSlot.SlotId.ToString(),
			FLinearColor::White,
			SlotFontSize
		);
		SlotText->SetJustification(ETextJustify::Center);
		SlotBorder->AddChild(SlotText);

		UCanvasPanelSlot* SlotCanvasSlot =
			PreviewCanvas->AddChildToCanvas(SlotBorder);
		SlotCanvasSlot->SetPosition(
			FVector2D(
				SlotCenterX - SlotSize.X * 0.5f,
				SlotCenterY - SlotSize.Y * 0.5f
			)
		);
		SlotCanvasSlot->SetSize(SlotSize);
	}

	const int32 GoalLabelFontSize = bCompact ? 9 : 11;
	const float GoalLabelY = CanvasHeight - (bCompact ? 23.0f : 30.0f);
	const float GoalLabelWidth = bCompact ? 82.0f : 100.0f;
	const float GoalLabelHeight = bCompact ? 16.0f : 20.0f;
	const float GoalLabelMargin = bCompact ? 12.0f : 18.0f;

	UTextBlock* OwnGoalText = MakeTextBlock(
		WidgetTree,
		TEXT("ARCO PROPIO"),
		FLinearColor(0.82f, 0.90f, 0.84f, 0.75f),
		GoalLabelFontSize
	);
	UCanvasPanelSlot* OwnGoalSlot = PreviewCanvas->AddChildToCanvas(OwnGoalText);
	OwnGoalSlot->SetPosition(FVector2D(GoalLabelMargin, GoalLabelY));
	OwnGoalSlot->SetSize(FVector2D(GoalLabelWidth, GoalLabelHeight));

	UTextBlock* OppGoalText = MakeTextBlock(
		WidgetTree,
		TEXT("ARCO RIVAL"),
		FLinearColor(0.82f, 0.90f, 0.84f, 0.75f),
		GoalLabelFontSize
	);
	OppGoalText->SetJustification(ETextJustify::Right);
	UCanvasPanelSlot* OppGoalSlot = PreviewCanvas->AddChildToCanvas(OppGoalText);
	OppGoalSlot->SetPosition(
		FVector2D(CanvasWidth - GoalLabelMargin - GoalLabelWidth, GoalLabelY)
	);
	OppGoalSlot->SetSize(FVector2D(GoalLabelWidth, GoalLabelHeight));
}

void USoccerFormationMenuWidget::RefreshTacticsPage()
{
	if (!IsValid(MatchManager))
	{
		return;
	}

	const FSoccerTeamTacticalPlan PlayerPlan =
		MatchManager->GetTacticalPlanForTeam(ESoccerTeam::PlayerTeam);
	const FSoccerTeamTacticalPlan OpponentPlan =
		MatchManager->GetTacticalPlanForTeam(ESoccerTeam::OpponentTeam);

	if (BuildUpSelector.ValueText != nullptr)
	{
		BuildUpSelector.ValueText->SetText(FText::FromString(GetTacticDisplayName(PlayerPlan.BuildUpStyle, BuildUpOptions)));
	}
	if (AttackChannelSelector.ValueText != nullptr)
	{
		AttackChannelSelector.ValueText->SetText(FText::FromString(GetTacticDisplayName(PlayerPlan.AttackChannel, AttackChannelOptions)));
	}
	if (AttackingWidthSelector.ValueText != nullptr)
	{
		AttackingWidthSelector.ValueText->SetText(FText::FromString(GetTacticDisplayName(PlayerPlan.AttackingWidth, AttackingWidthOptions)));
	}
	if (AttackingTempoSelector.ValueText != nullptr)
	{
		AttackingTempoSelector.ValueText->SetText(FText::FromString(GetTacticDisplayName(PlayerPlan.AttackingTempo, AttackingTempoOptions)));
	}
	if (AttackingTransitionSelector.ValueText != nullptr)
	{
		AttackingTransitionSelector.ValueText->SetText(FText::FromString(GetTacticDisplayName(PlayerPlan.AttackingTransition, AttackingTransitionOptions)));
	}
	if (DefensiveBlockSelector.ValueText != nullptr)
	{
		DefensiveBlockSelector.ValueText->SetText(FText::FromString(GetTacticDisplayName(PlayerPlan.DefensiveBlock, DefensiveBlockOptions)));
	}
	if (PressingIntensitySelector.ValueText != nullptr)
	{
		PressingIntensitySelector.ValueText->SetText(FText::FromString(GetTacticDisplayName(PlayerPlan.PressingIntensity, PressingOptions)));
	}
	if (MarkingStyleSelector.ValueText != nullptr)
	{
		MarkingStyleSelector.ValueText->SetText(FText::FromString(GetTacticDisplayName(PlayerPlan.MarkingStyle, MarkingOptions)));
	}
	if (DefensiveTransitionSelector.ValueText != nullptr)
	{
		DefensiveTransitionSelector.ValueText->SetText(FText::FromString(GetTacticDisplayName(PlayerPlan.DefensiveTransition, DefensiveTransitionOptions)));
	}

	if (TacticSummaryText != nullptr)
	{
		TacticSummaryText->SetText(
			FText::FromString(
				FString::Printf(
					TEXT("TU PLAN\nCanal %s  |  Bloque %s  |  Presión %s"),
					*GetTacticDisplayName(PlayerPlan.AttackChannel, AttackChannelOptions),
					*GetTacticDisplayName(PlayerPlan.DefensiveBlock, DefensiveBlockOptions),
					*GetTacticDisplayName(PlayerPlan.PressingIntensity, PressingOptions)
				)
			)
		);
	}

	if (OpponentTacticText != nullptr)
	{
		OpponentTacticText->SetText(
			FText::FromString(
				FString::Printf(
					TEXT("RIVAL - %s\nCanal %s  |  Bloque %s  |  Presión %s"),
					*MatchManager->GetOpponentCoachModeDisplayName(),
					*GetTacticDisplayName(OpponentPlan.AttackChannel, AttackChannelOptions),
					*GetTacticDisplayName(OpponentPlan.DefensiveBlock, DefensiveBlockOptions),
					*GetTacticDisplayName(OpponentPlan.PressingIntensity, PressingOptions)
				)
			)
		);
	}
}

void USoccerFormationMenuWidget::RefreshInstructionsPage()
{
	if (!IsValid(MatchManager))
	{
		return;
	}

	EnsureSelectedInstructionSlotValid();
	if (SelectedInstructionSlotId.IsNone())
	{
		return;
	}

	const FSoccerSlotTacticalInstruction Instruction =
		MatchManager->GetSlotTacticalInstructionForTeam(
			ESoccerTeam::PlayerTeam,
			SelectedInstructionSlotId
		);

	if (InstructionSlotSelector.ValueText != nullptr)
	{
		InstructionSlotSelector.ValueText->SetText(
			FText::FromName(SelectedInstructionSlotId)
		);
	}
	if (IndividualAttackSelector.ValueText != nullptr)
	{
		IndividualAttackSelector.ValueText->SetText(
			FText::FromString(
				GetTacticDisplayName(
					Instruction.AttackInstruction,
					IndividualAttackInstructionOptions
				)
			)
		);
	}
	if (IndividualDefenseSelector.ValueText != nullptr)
	{
		IndividualDefenseSelector.ValueText->SetText(
			FText::FromString(
				GetTacticDisplayName(
					Instruction.DefensiveInstruction,
					IndividualDefensiveInstructionOptions
				)
			)
		);
	}
	if (MarkingTargetSelector.ValueText != nullptr)
	{
		MarkingTargetSelector.ValueText->SetText(
			FText::FromString(
				Instruction.MarkingTargetSlotId.IsNone()
				? TEXT("Automático")
				: Instruction.MarkingTargetSlotId.ToString()
			)
		);
	}

	if (IndividualInstructionPlayerText != nullptr)
	{
		IndividualInstructionPlayerText->SetText(
			FText::FromString(
				FString::Printf(
					TEXT("JUGADOR ASIGNADO\n%s"),
					*GetSelectedInstructionPlayerName()
				)
			)
		);
	}

	if (IndividualInstructionSummaryText != nullptr)
	{
		IndividualInstructionSummaryText->SetText(
			FText::FromString(
				FString::Printf(
					TEXT("RESUMEN - %s\nAtaque: %s  |  Defensa: %s  |  Marca: %s"),
					*SelectedInstructionSlotId.ToString(),
					*GetTacticDisplayName(
						Instruction.AttackInstruction,
						IndividualAttackInstructionOptions
					),
					*GetTacticDisplayName(
						Instruction.DefensiveInstruction,
						IndividualDefensiveInstructionOptions
					),
					Instruction.MarkingTargetSlotId.IsNone()
					? TEXT("Automático")
					: *Instruction.MarkingTargetSlotId.ToString()
				)
			)
		);
	}
}

void USoccerFormationMenuWidget::RefreshTabVisuals()
{
	if (PresetsTabButton != nullptr)
	{
		PresetsTabButton->SetBackgroundColor(
			ActiveTab == ECoachMenuTab::Presets
			? Accent
			: FLinearColor(0.08f, 0.11f, 0.15f, 0.82f)
		);
	}
	if (FormationTabButton != nullptr)
	{
		FormationTabButton->SetBackgroundColor(
			ActiveTab == ECoachMenuTab::Formation
			? Accent
			: FLinearColor(0.08f, 0.11f, 0.15f, 0.82f)
		);
	}
	if (TacticsTabButton != nullptr)
	{
		TacticsTabButton->SetBackgroundColor(
			ActiveTab == ECoachMenuTab::Tactics
			? Accent
			: FLinearColor(0.08f, 0.11f, 0.15f, 0.82f)
		);
	}
	if (InstructionsTabButton != nullptr)
	{
		InstructionsTabButton->SetBackgroundColor(
			ActiveTab == ECoachMenuTab::Instructions
			? Accent
			: FLinearColor(0.08f, 0.11f, 0.15f, 0.82f)
		);
	}
}

void USoccerFormationMenuWidget::RefreshFocusVisuals()
{
	FSelectorRow* AllRows[] =
	{
		&BuiltInPresetSelector,
		&PresetSelector,
		&JsonFileSelector,
		&FormationSelector,
		&BuildUpSelector,
		&AttackChannelSelector,
		&AttackingWidthSelector,
		&AttackingTempoSelector,
		&AttackingTransitionSelector,
		&DefensiveBlockSelector,
		&PressingIntensitySelector,
		&MarkingStyleSelector,
		&DefensiveTransitionSelector,
		&InstructionSlotSelector,
		&IndividualAttackSelector,
		&IndividualDefenseSelector,
		&MarkingTargetSelector
	};

	for (FSelectorRow* Row : AllRows)
	{
		if (Row == nullptr)
		{
			continue;
		}

		if (Row->Border != nullptr)
		{
			Row->Border->SetBrushColor(SelectorBackground);
		}

		if (Row->LabelText != nullptr)
		{
			Row->LabelText->SetColorAndOpacity(
				FSlateColor(FLinearColor(0.80f, 0.84f, 0.88f, 1.0f))
			);
		}

		if (Row->ValueText != nullptr)
		{
			Row->ValueText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		}

		if (Row->PreviousButton != nullptr)
		{
			Row->PreviousButton->SetBackgroundColor(
				FLinearColor(0.08f, 0.11f, 0.15f, 0.90f)
			);
		}

		if (Row->NextButton != nullptr)
		{
			Row->NextButton->SetBackgroundColor(
				FLinearColor(0.08f, 0.11f, 0.15f, 0.90f)
			);
		}
	}

	TArray<FSelectorRow*> Rows = GetActiveSelectorRows();
	if (Rows.IsValidIndex(FocusedControlIndex))
	{
		FSelectorRow* FocusedRow = Rows[FocusedControlIndex];
		if (FocusedRow != nullptr)
		{
			if (FocusedRow->Border != nullptr)
			{
				FocusedRow->Border->SetBrushColor(SelectorFocused);
			}

			if (FocusedRow->LabelText != nullptr)
			{
				FocusedRow->LabelText->SetColorAndOpacity(FSlateColor(Accent));
			}

			if (FocusedRow->ValueText != nullptr)
			{
				FocusedRow->ValueText->SetColorAndOpacity(
					FSlateColor(FLinearColor(1.0f, 0.95f, 0.72f, 1.0f))
				);
			}

			const FLinearColor FocusedButtonColor(0.08f, 0.46f, 0.62f, 1.0f);
			if (FocusedRow->PreviousButton != nullptr)
			{
				FocusedRow->PreviousButton->SetBackgroundColor(FocusedButtonColor);
			}
			if (FocusedRow->NextButton != nullptr)
			{
				FocusedRow->NextButton->SetBackgroundColor(FocusedButtonColor);
			}
		}
	}
}


void USoccerFormationMenuWidget::RefreshControlHints()
{
	if (ControlHintText == nullptr)
	{
		return;
	}

	FString Hint;
	if (ActiveTab == ECoachMenuTab::Presets)
	{
		Hint =
			LastInputDevice == ECoachMenuInputDevice::Gamepad
			? TEXT("LB/RB: sección   Arr/Ab: juego/usuario/JSON   Izq/Der: elegir   A: aplicar/importar   Menu/Options: cerrar")
			: TEXT("Mouse: gestionar   Arr/Ab: juego/usuario/JSON   Izq/Der: elegir   Enter: aplicar/importar   PgUp/PgDn: sección   M/Esc: cerrar");
	}
	else
	{
		Hint =
			LastInputDevice == ECoachMenuInputDevice::Gamepad
			? TEXT("LB/RB: sección   D-Pad/Stick: navegar/cambiar   A: cambiar   Menu/Options: cerrar")
			: TEXT("Mouse: cambiar   Flechas: navegar/cambiar   PgUp/PgDn: sección   Enter: cambiar   M/Esc: cerrar");
	}

	ControlHintText->SetText(FText::FromString(Hint));
}

void USoccerFormationMenuWidget::SetLastInputDevice(
	ECoachMenuInputDevice NewInputDevice
)
{
	if (LastInputDevice == NewInputDevice)
	{
		return;
	}

	LastInputDevice = NewInputDevice;
	RefreshControlHints();
}

void USoccerFormationMenuWidget::SetTabGuidanceMessage()
{
	switch (ActiveTab)
	{
	case ECoachMenuTab::Presets:
		SetStatusMessage(
			TEXT("Presets: aplicá o duplicá plantillas DEL JUEGO; administrá MIS PRESETS y copias portables en JSON.")
		);
		break;

	case ECoachMenuTab::Formation:
		SetStatusMessage(
			TEXT("Formación: elegí la estructura base del equipo; el cambio se aplica de forma progresiva.")
		);
		break;

	case ECoachMenuTab::Tactics:
		SetStatusMessage(
			TEXT("Táctica: ajustá el plan colectivo. Izquierda/derecha cambia el valor seleccionado.")
		);
		break;

	case ECoachMenuTab::Instructions:
		SetStatusMessage(
			TEXT("Instrucciones: elegí un puesto y definí las prioridades que heredará su ocupante.")
		);
		break;
	}
}

void USoccerFormationMenuWidget::SetActiveTab(
	ECoachMenuTab NewTab,
	bool bResetFocus
)
{
	ActiveTab = NewTab;
	if (bResetFocus)
	{
		FocusedControlIndex = 0;
		if (
			NewTab == ECoachMenuTab::Presets &&
			IsValid(TacticalPresetManager) &&
			TacticalPresetManager->FindPreset(SelectedPresetId) != nullptr
		)
		{
			FocusedControlIndex = 1;
		}
	}

	if (PageSwitcher != nullptr)
	{
		PageSwitcher->SetActiveWidgetIndex(static_cast<int32>(ActiveTab));
	}

	RefreshTabVisuals();
	RefreshFocusVisuals();
	RefreshControlHints();
	SetTabGuidanceMessage();
}

void USoccerFormationMenuWidget::MoveFocus(int32 Direction)
{
	TArray<FSelectorRow*> Rows = GetActiveSelectorRows();
	if (Rows.Num() <= 0)
	{
		FocusedControlIndex = 0;
		return;
	}

	const int32 Step = Direction >= 0 ? 1 : -1;
	FocusedControlIndex =
		(FocusedControlIndex + Step + Rows.Num()) % Rows.Num();

	// In PRESETS, moving focus between DEL JUEGO and MIS PRESETS also changes
	// which preset feeds the preview, but never applies it to the team.
	if (ActiveTab == ECoachMenuTab::Presets)
	{
		if (FocusedControlIndex == 0 && SelectedBuiltInPresetId.IsValid())
		{
			SelectedPresetId = SelectedBuiltInPresetId;
			RefreshPresetsPage(false);
		}
		else if (FocusedControlIndex == 1 && SelectedUserPresetId.IsValid())
		{
			SelectedPresetId = SelectedUserPresetId;
			RefreshPresetsPage(false);
		}
	}

	RefreshFocusVisuals();
}

void USoccerFormationMenuWidget::CycleFocusedValue(int32 Direction)
{
	switch (ActiveTab)
	{
	case ECoachMenuTab::Presets:
		if (FocusedControlIndex == 0)
		{
			CycleBuiltInPreset(Direction);
		}
		else if (FocusedControlIndex == 1)
		{
			CyclePreset(Direction);
		}
		else
		{
			CycleJsonExchangeFile(Direction);
		}
		break;

	case ECoachMenuTab::Formation:
		CycleFormation(Direction);
		break;

	case ECoachMenuTab::Tactics:
		switch (FocusedControlIndex)
		{
		case 0: CycleBuildUp(Direction); break;
		case 1: CycleAttackChannel(Direction); break;
		case 2: CycleAttackingWidth(Direction); break;
		case 3: CycleAttackingTempo(Direction); break;
		case 4: CycleAttackingTransition(Direction); break;
		case 5: CycleDefensiveBlock(Direction); break;
		case 6: CyclePressingIntensity(Direction); break;
		case 7: CycleMarkingStyle(Direction); break;
		case 8: CycleDefensiveTransition(Direction); break;
		default: break;
		}
		break;

	case ECoachMenuTab::Instructions:
		switch (FocusedControlIndex)
		{
		case 0: CycleInstructionSlot(Direction); break;
		case 1: CycleIndividualAttack(Direction); break;
		case 2: CycleIndividualDefense(Direction); break;
		case 3: CycleMarkingTarget(Direction); break;
		default: break;
		}
		break;
	}
}

TArray<USoccerFormationMenuWidget::FSelectorRow*>
USoccerFormationMenuWidget::GetActiveSelectorRows()
{
	TArray<FSelectorRow*> Result;

	switch (ActiveTab)
	{
	case ECoachMenuTab::Presets:
		Result.Add(&BuiltInPresetSelector);
		Result.Add(&PresetSelector);
		Result.Add(&JsonFileSelector);
		break;

	case ECoachMenuTab::Formation:
		Result.Add(&FormationSelector);
		break;

	case ECoachMenuTab::Tactics:
		Result.Add(&BuildUpSelector);
		Result.Add(&AttackChannelSelector);
		Result.Add(&AttackingWidthSelector);
		Result.Add(&AttackingTempoSelector);
		Result.Add(&AttackingTransitionSelector);
		Result.Add(&DefensiveBlockSelector);
		Result.Add(&PressingIntensitySelector);
		Result.Add(&MarkingStyleSelector);
		Result.Add(&DefensiveTransitionSelector);
		break;

	case ECoachMenuTab::Instructions:
		Result.Add(&InstructionSlotSelector);
		Result.Add(&IndividualAttackSelector);
		Result.Add(&IndividualDefenseSelector);
		Result.Add(&MarkingTargetSelector);
		break;
	}

	return Result;
}

void USoccerFormationMenuWidget::CycleBuiltInPreset(int32 Direction)
{
	if (!IsValid(TacticalPresetManager))
	{
		return;
	}

	const TArray<FSoccerTacticalPreset>& Presets =
		TacticalPresetManager->GetBuiltInPresets();
	if (Presets.Num() <= 0)
	{
		SelectedBuiltInPresetId.Invalidate();
		RefreshPresetsPage(false);
		return;
	}

	int32 CurrentIndex = Presets.IndexOfByPredicate(
		[this](const FSoccerTacticalPreset& Candidate)
		{
			return Candidate.PresetId == SelectedBuiltInPresetId;
		}
	);
	if (CurrentIndex == INDEX_NONE)
	{
		CurrentIndex = 0;
	}

	const int32 Step = Direction >= 0 ? 1 : -1;
	const int32 NewIndex = (CurrentIndex + Step + Presets.Num()) % Presets.Num();
	SelectedBuiltInPresetId = Presets[NewIndex].PresetId;
	SelectedPresetId = SelectedBuiltInPresetId;
	RefreshPresetsPage(true);
	SetStatusMessage(
		FString::Printf(
			TEXT("Preset del juego seleccionado: %s. Podés APLICARLO o DUPLICARLO en MIS PRESETS."),
			*Presets[NewIndex].PresetName
		)
	);
}

void USoccerFormationMenuWidget::CyclePreset(int32 Direction)
{
	if (!IsValid(TacticalPresetManager))
	{
		return;
	}

	const TArray<FSoccerTacticalPreset>& Presets = TacticalPresetManager->GetPresets();
	if (Presets.Num() <= 0)
	{
		SelectedUserPresetId.Invalidate();
		RefreshPresetsPage(false);
		SetStatusMessage(TEXT("Todavía no hay presets del usuario. Podés duplicar uno DEL JUEGO o guardar la estrategia actual."));
		return;
	}

	int32 CurrentIndex = Presets.IndexOfByPredicate(
		[this](const FSoccerTacticalPreset& Candidate)
		{
			return Candidate.PresetId == SelectedUserPresetId;
		}
	);
	if (CurrentIndex == INDEX_NONE)
	{
		CurrentIndex = 0;
	}

	const int32 Step = Direction >= 0 ? 1 : -1;
	const int32 NewIndex = (CurrentIndex + Step + Presets.Num()) % Presets.Num();
	SelectedUserPresetId = Presets[NewIndex].PresetId;
	SelectedPresetId = SelectedUserPresetId;
	RefreshPresetsPage(true);
	SetStatusMessage(
		FString::Printf(
			TEXT("Preset del usuario seleccionado: %s. Enter/A lo aplica; cambiar la selección no modifica el equipo."),
			*Presets[NewIndex].PresetName
		)
	);
}

void USoccerFormationMenuWidget::CycleJsonExchangeFile(int32 Direction)
{
	if (AvailablePresetJsonFiles.Num() <= 0)
	{
		SelectedPresetJsonFileIndex = INDEX_NONE;
		RefreshJsonExchangeFiles(false);
		return;
	}

	if (!AvailablePresetJsonFiles.IsValidIndex(SelectedPresetJsonFileIndex))
	{
		SelectedPresetJsonFileIndex = 0;
	}
	else
	{
		const int32 Step = Direction >= 0 ? 1 : -1;
		SelectedPresetJsonFileIndex =
			(SelectedPresetJsonFileIndex + Step + AvailablePresetJsonFiles.Num()) %
			AvailablePresetJsonFiles.Num();
	}

	if (JsonFileSelector.ValueText != nullptr)
	{
		JsonFileSelector.ValueText->SetText(
			FText::FromString(
				AvailablePresetJsonFiles[SelectedPresetJsonFileIndex]
			)
		);
	}

	SetStatusMessage(
		FString::Printf(
			TEXT("JSON seleccionado para importar: %s"),
			*AvailablePresetJsonFiles[SelectedPresetJsonFileIndex]
		)
	);
}

void USoccerFormationMenuWidget::CycleFormation(int32 Direction)
{
	if (!IsValid(MatchManager))
	{
		return;
	}

	const ESoccerFormationSystem Current =
		MatchManager->GetFormationSystemForTeam(ESoccerTeam::PlayerTeam);
	int32 CurrentIndex = 0;
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(FormationSystems); ++Index)
	{
		if (FormationSystems[Index] == Current)
		{
			CurrentIndex = Index;
			break;
		}
	}

	const int32 Count = UE_ARRAY_COUNT(FormationSystems);
	const int32 Step = Direction >= 0 ? 1 : -1;
	const int32 NewIndex = (CurrentIndex + Step + Count) % Count;
	const ESoccerFormationSystem NewFormation = FormationSystems[NewIndex];

	MatchManager->SetFormationSystemForTeam(
		ESoccerTeam::PlayerTeam,
		NewFormation
	);

	EnsureSelectedInstructionSlotValid();
	RefreshHeader();
	RefreshFormationPage();
	RefreshInstructionsPage();
	RefreshPresetsPage(false);
	SetStatusMessage(
		FString::Printf(
			TEXT("Sistema aplicado: %s"),
			*GetFormationDisplayName(NewFormation)
		)
	);
}

void USoccerFormationMenuWidget::CycleBuildUp(int32 Direction)
{
	if (!IsValid(MatchManager)) return;
	FSoccerTeamTacticalPlan Plan = MatchManager->GetTacticalPlanForTeam(ESoccerTeam::PlayerTeam);
	Plan.BuildUpStyle = CycleTacticValue(Plan.BuildUpStyle, Direction, BuildUpOptions);
	ApplyPlayerTacticalPlan(Plan, TEXT("Táctica actualizada: salida."));
}

void USoccerFormationMenuWidget::CycleAttackChannel(int32 Direction)
{
	if (!IsValid(MatchManager)) return;
	FSoccerTeamTacticalPlan Plan = MatchManager->GetTacticalPlanForTeam(ESoccerTeam::PlayerTeam);
	Plan.AttackChannel = CycleTacticValue(Plan.AttackChannel, Direction, AttackChannelOptions);
	ApplyPlayerTacticalPlan(Plan, TEXT("Táctica actualizada: canal de ataque."));
}

void USoccerFormationMenuWidget::CycleAttackingWidth(int32 Direction)
{
	if (!IsValid(MatchManager)) return;
	FSoccerTeamTacticalPlan Plan = MatchManager->GetTacticalPlanForTeam(ESoccerTeam::PlayerTeam);
	Plan.AttackingWidth = CycleTacticValue(Plan.AttackingWidth, Direction, AttackingWidthOptions);
	ApplyPlayerTacticalPlan(Plan, TEXT("Táctica actualizada: amplitud."));
}

void USoccerFormationMenuWidget::CycleAttackingTempo(int32 Direction)
{
	if (!IsValid(MatchManager)) return;
	FSoccerTeamTacticalPlan Plan = MatchManager->GetTacticalPlanForTeam(ESoccerTeam::PlayerTeam);
	Plan.AttackingTempo = CycleTacticValue(Plan.AttackingTempo, Direction, AttackingTempoOptions);
	ApplyPlayerTacticalPlan(Plan, TEXT("Táctica actualizada: ritmo."));
}

void USoccerFormationMenuWidget::CycleAttackingTransition(int32 Direction)
{
	if (!IsValid(MatchManager)) return;
	FSoccerTeamTacticalPlan Plan = MatchManager->GetTacticalPlanForTeam(ESoccerTeam::PlayerTeam);
	Plan.AttackingTransition = CycleTacticValue(Plan.AttackingTransition, Direction, AttackingTransitionOptions);
	ApplyPlayerTacticalPlan(Plan, TEXT("Táctica actualizada: transición ofensiva."));
}

void USoccerFormationMenuWidget::CycleDefensiveBlock(int32 Direction)
{
	if (!IsValid(MatchManager)) return;
	FSoccerTeamTacticalPlan Plan = MatchManager->GetTacticalPlanForTeam(ESoccerTeam::PlayerTeam);
	Plan.DefensiveBlock = CycleTacticValue(Plan.DefensiveBlock, Direction, DefensiveBlockOptions);
	ApplyPlayerTacticalPlan(Plan, TEXT("Táctica actualizada: bloque defensivo."));
}

void USoccerFormationMenuWidget::CyclePressingIntensity(int32 Direction)
{
	if (!IsValid(MatchManager)) return;
	FSoccerTeamTacticalPlan Plan = MatchManager->GetTacticalPlanForTeam(ESoccerTeam::PlayerTeam);
	Plan.PressingIntensity = CycleTacticValue(Plan.PressingIntensity, Direction, PressingOptions);
	ApplyPlayerTacticalPlan(Plan, TEXT("Táctica actualizada: presión."));
}

void USoccerFormationMenuWidget::CycleMarkingStyle(int32 Direction)
{
	if (!IsValid(MatchManager)) return;
	FSoccerTeamTacticalPlan Plan = MatchManager->GetTacticalPlanForTeam(ESoccerTeam::PlayerTeam);
	Plan.MarkingStyle = CycleTacticValue(Plan.MarkingStyle, Direction, MarkingOptions);
	ApplyPlayerTacticalPlan(Plan, TEXT("Táctica actualizada: marcaje."));
}

void USoccerFormationMenuWidget::CycleDefensiveTransition(int32 Direction)
{
	if (!IsValid(MatchManager)) return;
	FSoccerTeamTacticalPlan Plan = MatchManager->GetTacticalPlanForTeam(ESoccerTeam::PlayerTeam);
	Plan.DefensiveTransition = CycleTacticValue(Plan.DefensiveTransition, Direction, DefensiveTransitionOptions);
	ApplyPlayerTacticalPlan(Plan, TEXT("Táctica actualizada: transición defensiva."));
}

void USoccerFormationMenuWidget::CycleInstructionSlot(int32 Direction)
{
	TArray<FName> SlotIds = GetCurrentPlayerInstructionSlotIds();
	if (SlotIds.Num() <= 0)
	{
		return;
	}

	int32 CurrentIndex = SlotIds.IndexOfByKey(SelectedInstructionSlotId);
	if (CurrentIndex == INDEX_NONE)
	{
		CurrentIndex = 0;
	}

	const int32 Step = Direction >= 0 ? 1 : -1;
	const int32 NewIndex = (CurrentIndex + Step + SlotIds.Num()) % SlotIds.Num();
	SelectedInstructionSlotId = SlotIds[NewIndex];
	RefreshInstructionsPage();
	SetStatusMessage(
		FString::Printf(
			TEXT("Editando instrucciones de %s."),
			*SelectedInstructionSlotId.ToString()
		)
	);
}

void USoccerFormationMenuWidget::CycleIndividualAttack(int32 Direction)
{
	if (!IsValid(MatchManager) || SelectedInstructionSlotId.IsNone()) return;
	FSoccerSlotTacticalInstruction Instruction = MatchManager->GetSlotTacticalInstructionForTeam(ESoccerTeam::PlayerTeam, SelectedInstructionSlotId);
	Instruction.AttackInstruction = CycleTacticValue(Instruction.AttackInstruction, Direction, IndividualAttackInstructionOptions);
	ApplySelectedSlotInstruction(Instruction, TEXT("Instrucción individual actualizada: ataque."));
}

void USoccerFormationMenuWidget::CycleIndividualDefense(int32 Direction)
{
	if (!IsValid(MatchManager) || SelectedInstructionSlotId.IsNone()) return;
	FSoccerSlotTacticalInstruction Instruction = MatchManager->GetSlotTacticalInstructionForTeam(ESoccerTeam::PlayerTeam, SelectedInstructionSlotId);
	Instruction.DefensiveInstruction = CycleTacticValue(Instruction.DefensiveInstruction, Direction, IndividualDefensiveInstructionOptions);
	ApplySelectedSlotInstruction(Instruction, TEXT("Instrucción individual actualizada: defensa."));
}

void USoccerFormationMenuWidget::CycleMarkingTarget(int32 Direction)
{
	if (!IsValid(MatchManager) || SelectedInstructionSlotId.IsNone())
	{
		return;
	}

	TArray<FName> TargetIds = GetCurrentMarkingTargetIds();
	if (TargetIds.Num() <= 0)
	{
		return;
	}

	FSoccerSlotTacticalInstruction Instruction =
		MatchManager->GetSlotTacticalInstructionForTeam(
			ESoccerTeam::PlayerTeam,
			SelectedInstructionSlotId
		);

	int32 CurrentIndex = TargetIds.IndexOfByKey(Instruction.MarkingTargetSlotId);
	if (CurrentIndex == INDEX_NONE)
	{
		CurrentIndex = 0;
	}

	const int32 Step = Direction >= 0 ? 1 : -1;
	const int32 NewIndex = (CurrentIndex + Step + TargetIds.Num()) % TargetIds.Num();
	Instruction.MarkingTargetSlotId = TargetIds[NewIndex];
	ApplySelectedSlotInstruction(Instruction, TEXT("Instrucción individual actualizada: marca."));
}

void USoccerFormationMenuWidget::ApplyPlayerTacticalPlan(
	const FSoccerTeamTacticalPlan& TacticalPlan,
	const FString& StatusMessage
)
{
	if (!IsValid(MatchManager))
	{
		return;
	}

	MatchManager->SetTacticalPlanForTeam(
		ESoccerTeam::PlayerTeam,
		TacticalPlan
	);
	RefreshTacticsPage();
	RefreshPresetsPage(false);
	SetStatusMessage(StatusMessage);
}

void USoccerFormationMenuWidget::ApplySelectedSlotInstruction(
	const FSoccerSlotTacticalInstruction& Instruction,
	const FString& StatusMessage
)
{
	if (!IsValid(MatchManager))
	{
		return;
	}

	MatchManager->SetSlotTacticalInstructionForTeam(
		ESoccerTeam::PlayerTeam,
		Instruction
	);
	RefreshInstructionsPage();
	RefreshPresetsPage(false);
	SetStatusMessage(StatusMessage);
}

void USoccerFormationMenuWidget::EnsureSelectedInstructionSlotValid()
{
	TArray<FName> SlotIds = GetCurrentPlayerInstructionSlotIds();
	if (SlotIds.Num() <= 0)
	{
		SelectedInstructionSlotId = NAME_None;
		return;
	}

	if (SlotIds.Contains(SelectedInstructionSlotId))
	{
		return;
	}

	// Prefer the first outfield slot. Formation definitions keep GK first.
	SelectedInstructionSlotId = SlotIds.Num() > 1 ? SlotIds[1] : SlotIds[0];
}

TArray<FName> USoccerFormationMenuWidget::GetCurrentPlayerInstructionSlotIds() const
{
	TArray<FName> Result;
	if (!IsValid(MatchManager))
	{
		return Result;
	}

	const FSoccerFormationDefinition Definition =
		MatchManager->GetFormationDefinitionForTeam(ESoccerTeam::PlayerTeam);
	for (const FSoccerFormationSlot& FormationSlot : Definition.Slots)
	{
		Result.Add(FormationSlot.SlotId);
	}
	return Result;
}

TArray<FName> USoccerFormationMenuWidget::GetCurrentMarkingTargetIds() const
{
	TArray<FName> Result;
	Result.Add(NAME_None);

	if (!IsValid(MatchManager))
	{
		return Result;
	}

	const FSoccerFormationDefinition Definition =
		MatchManager->GetFormationDefinitionForTeam(ESoccerTeam::OpponentTeam);
	for (const FSoccerFormationSlot& FormationSlot : Definition.Slots)
	{
		if (FormationSlot.PlayerRole != ESoccerPlayerRole::Goalkeeper)
		{
			Result.Add(FormationSlot.SlotId);
		}
	}
	return Result;
}

FString USoccerFormationMenuWidget::GetFormationDisplayName(
	ESoccerFormationSystem FormationSystem
) const
{
	return SoccerFormationLibrary::GetDefinition(FormationSystem).DisplayName.ToString();
}

FString USoccerFormationMenuWidget::GetSelectedInstructionPlayerName() const
{
	if (!IsValid(MatchManager) || SelectedInstructionSlotId.IsNone())
	{
		return TEXT("--");
	}

	ASoccerCharacterBase* AssignedCharacter =
		MatchManager->GetFormationSlotAssignedCharacter(
			ESoccerTeam::PlayerTeam,
			SelectedInstructionSlotId
		);

	return IsValid(AssignedCharacter)
		? AssignedCharacter->GetName()
		: TEXT("Sin jugador asignado");
}

FString USoccerFormationMenuWidget::GetPresetNameEditorText() const
{
	return
		PresetNameTextBox != nullptr
		? PresetNameTextBox->GetText().ToString()
		: FString();
}

void USoccerFormationMenuWidget::EnsureSelectedPresetValid()
{
	if (!IsValid(TacticalPresetManager))
	{
		SelectedPresetId.Invalidate();
		SelectedBuiltInPresetId.Invalidate();
		SelectedUserPresetId.Invalidate();
		return;
	}

	const TArray<FSoccerTacticalPreset>& BuiltInPresets =
		TacticalPresetManager->GetBuiltInPresets();
	const TArray<FSoccerTacticalPreset>& UserPresets =
		TacticalPresetManager->GetPresets();

	if (
		!SelectedBuiltInPresetId.IsValid() ||
		TacticalPresetManager->FindBuiltInPreset(SelectedBuiltInPresetId) == nullptr
	)
	{
		SelectedBuiltInPresetId = BuiltInPresets.Num() > 0
			? BuiltInPresets[0].PresetId
			: FGuid();
	}

	if (
		!SelectedUserPresetId.IsValid() ||
		TacticalPresetManager->FindPreset(SelectedUserPresetId) == nullptr
	)
	{
		SelectedUserPresetId = UserPresets.Num() > 0
			? UserPresets[0].PresetId
			: FGuid();
	}

	if (
		SelectedPresetId.IsValid() &&
		TacticalPresetManager->FindAnyPreset(SelectedPresetId) != nullptr
	)
	{
		return;
	}

	// Built-ins are always available and make a useful first view for a fresh
	// profile. If none exist for any reason, fall back to the user's library.
	if (SelectedBuiltInPresetId.IsValid())
	{
		SelectedPresetId = SelectedBuiltInPresetId;
	}
	else
	{
		SelectedPresetId = SelectedUserPresetId;
	}
}

void USoccerFormationMenuWidget::SetStatusMessage(const FString& Message)
{
	if (StatusText != nullptr)
	{
		StatusText->SetText(FText::FromString(Message));
	}
}

void USoccerFormationMenuWidget::HandlePresetsTabClicked()
{
	SetActiveTab(ECoachMenuTab::Presets);
	RefreshPresetsPage(false);
}

void USoccerFormationMenuWidget::HandlePresetApplyClicked()
{
	if (!IsValid(TacticalPresetManager))
	{
		return;
	}

	FString Message;
	if (TacticalPresetManager->ApplyPresetToPlayerTeam(SelectedPresetId, Message))
	{
		EnsureSelectedInstructionSlotValid();
		RefreshFromMatchManager();
	}
	SetStatusMessage(Message);
}

void USoccerFormationMenuWidget::HandlePresetSaveCurrentClicked()
{
	if (!IsValid(TacticalPresetManager))
	{
		return;
	}

	FGuid NewPresetId;
	FString Message;
	if (TacticalPresetManager->SaveCurrentPlayerStrategyAsNewPreset(
		GetPresetNameEditorText(),
		NewPresetId,
		Message
	))
	{
		SelectedUserPresetId = NewPresetId;
		SelectedPresetId = NewPresetId;
		RefreshPresetsPage(true);
	}
	SetStatusMessage(Message);
}

void USoccerFormationMenuWidget::HandlePresetOverwriteClicked()
{
	if (!IsValid(TacticalPresetManager))
	{
		return;
	}

	FString Message;
	if (TacticalPresetManager->OverwritePresetWithCurrentPlayerStrategy(
		SelectedPresetId,
		Message
	))
	{
		RefreshPresetsPage(true);
	}
	SetStatusMessage(Message);
}

void USoccerFormationMenuWidget::HandlePresetRenameClicked()
{
	if (!IsValid(TacticalPresetManager))
	{
		return;
	}

	FString Message;
	if (TacticalPresetManager->RenamePreset(
		SelectedPresetId,
		GetPresetNameEditorText(),
		Message
	))
	{
		RefreshPresetsPage(true);
	}
	SetStatusMessage(Message);
}

void USoccerFormationMenuWidget::HandlePresetDuplicateClicked()
{
	if (!IsValid(TacticalPresetManager))
	{
		return;
	}

	FGuid DuplicatePresetId;
	FString Message;
	if (TacticalPresetManager->DuplicatePreset(
		SelectedPresetId,
		DuplicatePresetId,
		Message
	))
	{
		SelectedUserPresetId = DuplicatePresetId;
		SelectedPresetId = DuplicatePresetId;
		RefreshPresetsPage(true);
	}
	SetStatusMessage(Message);
}

void USoccerFormationMenuWidget::HandlePresetMoveUpClicked()
{
	if (!IsValid(TacticalPresetManager))
	{
		return;
	}

	FString Message;
	if (TacticalPresetManager->MovePreset(SelectedPresetId, -1, Message))
	{
		RefreshPresetsPage(false);
	}
	SetStatusMessage(Message);
}

void USoccerFormationMenuWidget::HandlePresetMoveDownClicked()
{
	if (!IsValid(TacticalPresetManager))
	{
		return;
	}

	FString Message;
	if (TacticalPresetManager->MovePreset(SelectedPresetId, 1, Message))
	{
		RefreshPresetsPage(false);
	}
	SetStatusMessage(Message);
}

void USoccerFormationMenuWidget::HandlePresetJsonPreviousClicked()
{
	CycleJsonExchangeFile(-1);
}

void USoccerFormationMenuWidget::HandlePresetJsonNextClicked()
{
	CycleJsonExchangeFile(1);
}

void USoccerFormationMenuWidget::HandlePresetJsonExportClicked()
{
	if (!IsValid(TacticalPresetManager))
	{
		return;
	}

	FString ExportedFileName;
	FString Message;
	if (TacticalPresetManager->ExportPresetToJson(
		SelectedPresetId,
		ExportedFileName,
		Message
	))
	{
		RefreshJsonExchangeFiles(false);
		SelectedPresetJsonFileIndex =
			AvailablePresetJsonFiles.IndexOfByKey(ExportedFileName);
		if (JsonFileSelector.ValueText != nullptr)
		{
			JsonFileSelector.ValueText->SetText(
				FText::FromString(ExportedFileName)
			);
		}
	}

	SetStatusMessage(Message);
}

void USoccerFormationMenuWidget::HandlePresetJsonImportClicked()
{
	if (!IsValid(TacticalPresetManager))
	{
		return;
	}

	if (!AvailablePresetJsonFiles.IsValidIndex(SelectedPresetJsonFileIndex))
	{
		SetStatusMessage(
			TEXT("No hay un archivo JSON detectado para importar.")
		);
		return;
	}

	const FString JsonFileName =
		AvailablePresetJsonFiles[SelectedPresetJsonFileIndex];
	FGuid ImportedPresetId;
	FString Message;
	if (TacticalPresetManager->ImportPresetFromJson(
		JsonFileName,
		ImportedPresetId,
		Message
	))
	{
		SelectedUserPresetId = ImportedPresetId;
		SelectedPresetId = ImportedPresetId;
		RefreshPresetsPage(true);
	}

	SetStatusMessage(Message);
}

void USoccerFormationMenuWidget::HandlePresetJsonRefreshClicked()
{
	RefreshJsonExchangeFiles(true);
	if (AvailablePresetJsonFiles.Num() <= 0)
	{
		SetStatusMessage(
			TEXT("No se encontraron archivos .json en Saved/TacticalPresets/Exchange.")
		);
		return;
	}

	SetStatusMessage(
		FString::Printf(
			TEXT("%d archivo(s) JSON detectado(s) en la carpeta de intercambio."),
			AvailablePresetJsonFiles.Num()
		)
	);
}

void USoccerFormationMenuWidget::HandlePresetRevertClicked()
{
	if (!IsValid(TacticalPresetManager))
	{
		return;
	}

	const FGuid ActivePresetId = TacticalPresetManager->GetActivePresetId();
	const FSoccerTacticalPreset* ActivePreset =
		TacticalPresetManager->FindAnyPreset(ActivePresetId);
	if (ActivePreset == nullptr)
	{
		SetStatusMessage(TEXT("No hay un preset activo para revertir."));
		return;
	}

	if (!TacticalPresetManager->IsActivePresetModified())
	{
		SetStatusMessage(TEXT("El preset activo no tiene cambios para revertir."));
		RefreshPresetsPage(false);
		return;
	}

	const FString ActivePresetName = ActivePreset->PresetName;
	FString Message;
	if (TacticalPresetManager->ApplyPresetToPlayerTeam(ActivePresetId, Message))
	{
		EnsureSelectedInstructionSlotValid();
		RefreshFromMatchManager();
		SetStatusMessage(
			FString::Printf(
				TEXT("Cambios revertidos. Se restauró '%s'."),
				*ActivePresetName
			)
		);
		return;
	}

	SetStatusMessage(Message);
}

void USoccerFormationMenuWidget::HandlePresetDeleteClicked()
{
	OpenPresetDeleteConfirmation();
}

void USoccerFormationMenuWidget::HandlePresetDeleteConfirmClicked()
{
	ConfirmPendingPresetDeletion();
}

void USoccerFormationMenuWidget::HandlePresetDeleteCancelClicked()
{
	ClosePresetDeleteConfirmation();
	SetStatusMessage(TEXT("Eliminación cancelada."));
}

void USoccerFormationMenuWidget::OpenPresetDeleteConfirmation()
{
	if (
		!IsValid(TacticalPresetManager) ||
		!SelectedPresetId.IsValid() ||
		PresetDeleteConfirmationOverlay == nullptr
	)
	{
		return;
	}

	const FSoccerTacticalPreset* SelectedPreset =
		TacticalPresetManager->FindPreset(SelectedPresetId);
	if (SelectedPreset == nullptr)
	{
		return;
	}

	// Freeze the identity now. Confirmation must always delete exactly the
	// preset whose name is displayed, even if some future UI path changes the
	// underlying selection while the modal is visible.
	PendingDeletePresetId = SelectedPreset->PresetId;
	bPresetDeleteConfirmationOpen = true;

	if (PresetDeleteConfirmationText != nullptr)
	{
		PresetDeleteConfirmationText->SetText(
			FText::FromString(
				FString::Printf(
					TEXT("¿Eliminar \"%s\"?\nEsta acción no se puede deshacer."),
					*SelectedPreset->PresetName
				)
			)
		);
	}

	PresetDeleteConfirmationOverlay->SetVisibility(ESlateVisibility::Visible);

	if (PresetDeleteCancelButton != nullptr)
	{
		PresetDeleteCancelButton->SetKeyboardFocus();
	}
}

void USoccerFormationMenuWidget::ClosePresetDeleteConfirmation()
{
	bPresetDeleteConfirmationOpen = false;
	PendingDeletePresetId.Invalidate();

	if (PresetDeleteConfirmationOverlay != nullptr)
	{
		PresetDeleteConfirmationOverlay->SetVisibility(
			ESlateVisibility::Collapsed
		);
	}
}

void USoccerFormationMenuWidget::ConfirmPendingPresetDeletion()
{
	if (
		!bPresetDeleteConfirmationOpen ||
		!PendingDeletePresetId.IsValid() ||
		!IsValid(TacticalPresetManager)
	)
	{
		ClosePresetDeleteConfirmation();
		return;
	}

	const FGuid PresetIdToDelete = PendingDeletePresetId;
	ClosePresetDeleteConfirmation();

	FString Message;
	if (TacticalPresetManager->DeletePreset(PresetIdToDelete, Message))
	{
		if (SelectedPresetId == PresetIdToDelete)
		{
			SelectedPresetId.Invalidate();
		}
		if (SelectedUserPresetId == PresetIdToDelete)
		{
			SelectedUserPresetId.Invalidate();
		}

		EnsureSelectedPresetValid();
		if (SelectedUserPresetId.IsValid())
		{
			SelectedPresetId = SelectedUserPresetId;
		}
		RefreshPresetsPage(true);
	}

	SetStatusMessage(Message);
}

void USoccerFormationMenuWidget::HandleQuickPresetSlotClicked(
	int32 QuickSlotIndex
)
{
	if (!IsValid(TacticalPresetManager))
	{
		return;
	}

	FString Message;
	if (TacticalPresetManager->IsBuiltInPreset(SelectedPresetId))
	{
		SetStatusMessage(
			TEXT("Los presets DEL JUEGO son de solo lectura. DUPLICALO en MIS PRESETS para asignarlo a un acceso rápido.")
		);
		return;
	}

	const FGuid ExistingPresetId =
		TacticalPresetManager->GetQuickPresetIdForSlot(QuickSlotIndex);

	bool bSucceeded = false;
	if (
		SelectedPresetId.IsValid() &&
		ExistingPresetId == SelectedPresetId
	)
	{
		bSucceeded = TacticalPresetManager->ClearQuickPresetSlot(
			QuickSlotIndex,
			Message
		);
	}
	else if (SelectedPresetId.IsValid())
	{
		bSucceeded = TacticalPresetManager->AssignPresetToQuickSlot(
			QuickSlotIndex,
			SelectedPresetId,
			Message
		);
	}
	else
	{
		Message = TEXT("Primero seleccioná un preset para asignarlo al acceso rápido.");
	}

	if (bSucceeded)
	{
		RefreshQuickPresetAssignments();
	}
	SetStatusMessage(Message);
}

void USoccerFormationMenuWidget::HandleQuickPresetSlot1Clicked()
{
	HandleQuickPresetSlotClicked(0);
}

void USoccerFormationMenuWidget::HandleQuickPresetSlot2Clicked()
{
	HandleQuickPresetSlotClicked(1);
}

void USoccerFormationMenuWidget::HandleQuickPresetSlot3Clicked()
{
	HandleQuickPresetSlotClicked(2);
}

void USoccerFormationMenuWidget::HandleQuickPresetSlot4Clicked()
{
	HandleQuickPresetSlotClicked(3);
}

void USoccerFormationMenuWidget::HandleFormationTabClicked()
{
	SetActiveTab(ECoachMenuTab::Formation);
}

void USoccerFormationMenuWidget::HandleTacticsTabClicked()
{
	SetActiveTab(ECoachMenuTab::Tactics);
}

void USoccerFormationMenuWidget::HandleInstructionsTabClicked()
{
	SetActiveTab(ECoachMenuTab::Instructions);
}

#define SOCCER_CYCLE_HANDLER(FuncName, CycleFunc, DirectionValue) \
void USoccerFormationMenuWidget::FuncName() \
{ \
	CycleFunc(DirectionValue); \
}

SOCCER_CYCLE_HANDLER(HandleBuiltInPresetPrevious, CycleBuiltInPreset, -1)
SOCCER_CYCLE_HANDLER(HandleBuiltInPresetNext, CycleBuiltInPreset, 1)
SOCCER_CYCLE_HANDLER(HandlePresetPrevious, CyclePreset, -1)
SOCCER_CYCLE_HANDLER(HandlePresetNext, CyclePreset, 1)
SOCCER_CYCLE_HANDLER(HandleFormationPrevious, CycleFormation, -1)
SOCCER_CYCLE_HANDLER(HandleFormationNext, CycleFormation, 1)
SOCCER_CYCLE_HANDLER(HandleBuildUpPrevious, CycleBuildUp, -1)
SOCCER_CYCLE_HANDLER(HandleBuildUpNext, CycleBuildUp, 1)
SOCCER_CYCLE_HANDLER(HandleAttackChannelPrevious, CycleAttackChannel, -1)
SOCCER_CYCLE_HANDLER(HandleAttackChannelNext, CycleAttackChannel, 1)
SOCCER_CYCLE_HANDLER(HandleAttackingWidthPrevious, CycleAttackingWidth, -1)
SOCCER_CYCLE_HANDLER(HandleAttackingWidthNext, CycleAttackingWidth, 1)
SOCCER_CYCLE_HANDLER(HandleAttackingTempoPrevious, CycleAttackingTempo, -1)
SOCCER_CYCLE_HANDLER(HandleAttackingTempoNext, CycleAttackingTempo, 1)
SOCCER_CYCLE_HANDLER(HandleAttackingTransitionPrevious, CycleAttackingTransition, -1)
SOCCER_CYCLE_HANDLER(HandleAttackingTransitionNext, CycleAttackingTransition, 1)
SOCCER_CYCLE_HANDLER(HandleDefensiveBlockPrevious, CycleDefensiveBlock, -1)
SOCCER_CYCLE_HANDLER(HandleDefensiveBlockNext, CycleDefensiveBlock, 1)
SOCCER_CYCLE_HANDLER(HandlePressingIntensityPrevious, CyclePressingIntensity, -1)
SOCCER_CYCLE_HANDLER(HandlePressingIntensityNext, CyclePressingIntensity, 1)
SOCCER_CYCLE_HANDLER(HandleMarkingStylePrevious, CycleMarkingStyle, -1)
SOCCER_CYCLE_HANDLER(HandleMarkingStyleNext, CycleMarkingStyle, 1)
SOCCER_CYCLE_HANDLER(HandleDefensiveTransitionPrevious, CycleDefensiveTransition, -1)
SOCCER_CYCLE_HANDLER(HandleDefensiveTransitionNext, CycleDefensiveTransition, 1)
SOCCER_CYCLE_HANDLER(HandleInstructionSlotPrevious, CycleInstructionSlot, -1)
SOCCER_CYCLE_HANDLER(HandleInstructionSlotNext, CycleInstructionSlot, 1)
SOCCER_CYCLE_HANDLER(HandleIndividualAttackPrevious, CycleIndividualAttack, -1)
SOCCER_CYCLE_HANDLER(HandleIndividualAttackNext, CycleIndividualAttack, 1)
SOCCER_CYCLE_HANDLER(HandleIndividualDefensePrevious, CycleIndividualDefense, -1)
SOCCER_CYCLE_HANDLER(HandleIndividualDefenseNext, CycleIndividualDefense, 1)
SOCCER_CYCLE_HANDLER(HandleMarkingTargetPrevious, CycleMarkingTarget, -1)
SOCCER_CYCLE_HANDLER(HandleMarkingTargetNext, CycleMarkingTarget, 1)

#undef SOCCER_CYCLE_HANDLER

void USoccerFormationMenuWidget::HandleCloseClicked()
{
	CloseMenu();
}
