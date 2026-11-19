#include "SoccerSubstitutionMenuWidget.h"

#include "SoccerAICharacter.h"
#include "SoccerCharacterBase.h"
#include "SoccerFormationLibrary.h"
#include "SoccerGameInstance.h"
#include "SoccerLineupEvaluationLibrary.h"
#include "SoccerMatchManager.h"
#include "SoccerPlayerProfile.h"
#include "ThirdPersonCppCharacter.h"
#include "SoccerUIStyleEditorController.h"
#include "SoccerUITheme.h"

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
#include "Kismet/GameplayStatics.h"

namespace
{
	const FLinearColor MenuBackground(0.015f, 0.025f, 0.040f, 0.97f);
	const FLinearColor CardBackground(0.035f, 0.055f, 0.075f, 0.96f);
	const FLinearColor Accent(0.08f, 0.62f, 0.78f, 1.0f);
	const FLinearColor MutedText(0.68f, 0.73f, 0.78f, 1.0f);

	UTextBlock* MakeText(
		UWidgetTree* WidgetTree,
		const FString& Text,
		int32 FontSize,
		const FLinearColor& Color = FLinearColor::White
	)
	{
		UTextBlock* Result = WidgetTree->ConstructWidget<UTextBlock>();
		Result->SetText(FText::FromString(Text));
		FSlateFontInfo Font = Result->Font;
		Font.Size = FontSize;
		Result->SetFont(Font);
		Result->SetColorAndOpacity(FSlateColor(Color));
		return Result;
	}

	UButton* MakeButton(
		UWidgetTree* WidgetTree,
		const FString& Label,
		int32 FontSize,
		const FLinearColor& Color = Accent
	)
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>();
		Button->SetBackgroundColor(Color);
		Button->AddChild(MakeText(WidgetTree, Label, FontSize));
		return Button;
	}

	void AddVertical(
		UVerticalBox* Parent,
		UWidget* Child,
		const FMargin& Padding = FMargin(0.0f),
		ESlateSizeRule::Type SizeRule = ESlateSizeRule::Automatic
	)
	{
		UVerticalBoxSlot* Slot = Parent->AddChildToVerticalBox(Child);
		Slot->SetPadding(Padding);
		Slot->SetSize(FSlateChildSize(SizeRule));
		Slot->SetHorizontalAlignment(HAlign_Fill);
	}

	void AddHorizontal(
		UHorizontalBox* Parent,
		UWidget* Child,
		const FMargin& Padding = FMargin(0.0f),
		ESlateSizeRule::Type SizeRule = ESlateSizeRule::Automatic
	)
	{
		UHorizontalBoxSlot* Slot = Parent->AddChildToHorizontalBox(Child);
		Slot->SetPadding(Padding);
		Slot->SetSize(FSlateChildSize(SizeRule));
		Slot->SetVerticalAlignment(VAlign_Center);
	}
}

void USoccerSubstitutionMenuWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    UIAnimations.Tick(InDeltaTime);
    const FVector2D ViewportUnits = FSoccerUITheme::GetLogicalViewportSize(this);
    if (ViewportUnits.X > 1.f && !LastResponsiveViewport.Equals(ViewportUnits, 1.f))
    {
        LastResponsiveViewport = ViewportUnits;
        ApplyLiveStyle();
    }
}

void USoccerSubstitutionMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
	ApplyLiveStyle();
}

void USoccerSubstitutionMenuWidget::InitializeForMatchManager(
	ASoccerMatchManager* InMatchManager
)
{
	MatchManager = InMatchManager;
	RefreshPlayerLists(false);
}

void USoccerSubstitutionMenuWidget::ActivateMenu(bool bPauseGame)
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
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PlayerController->SetInputMode(InputMode);
	RefreshPlayerLists(true);
    UpdateSelectorFocus(true);
    UIAnimations.StartEntrance(LiveMainPanel);
}

void USoccerSubstitutionMenuWidget::CloseMenu()
{
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

FReply USoccerSubstitutionMenuWidget::NativeOnPreviewKeyDown(
	const FGeometry& InGeometry,
	const FKeyEvent& InKeyEvent
)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::F11)
	{
		if (APlayerController* PC = GetOwningPlayer()) PC->ConsoleCommand(TEXT("fullscreen"), true);
		return FReply::Handled();
	}
    if ((StyleEditor && StyleEditor->HasFocusedInput()) ||
        FSoccerUINavigation::IsEditingText(WidgetTree))
        return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
	if (Key == EKeys::N || Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right)
	{
		CloseMenu();
		return FReply::Handled();
	}
	if (Key == EKeys::Up || Key == EKeys::Gamepad_DPad_Up)
	{
		FocusedSelectorIndex = 0;
        UpdateSelectorFocus(true);
		SetStatus(TEXT("Seleccionando al jugador que sale."));
		return FReply::Handled();
	}
	if (Key == EKeys::Down || Key == EKeys::Gamepad_DPad_Down)
	{
		FocusedSelectorIndex = 1;
        UpdateSelectorFocus(true);
		SetStatus(TEXT("Seleccionando al suplente que entra."));
		return FReply::Handled();
	}
	if (Key == EKeys::Left || Key == EKeys::Gamepad_DPad_Left)
	{
		if (FocusedSelectorIndex == 0) CycleOutgoing(-1); else CycleIncoming(-1);
		return FReply::Handled();
	}
	if (Key == EKeys::Right || Key == EKeys::Gamepad_DPad_Right)
	{
		if (FocusedSelectorIndex == 0) CycleOutgoing(1); else CycleIncoming(1);
		return FReply::Handled();
	}
	if (FSoccerUINavigation::IsAccept(Key))
	{
        if (IsValid(CloseMenuButton) && CloseMenuButton->HasKeyboardFocus())
            CloseMenu();
        else if (IsValid(CancelButton) && CancelButton->HasKeyboardFocus())
            CancelPendingSubstitution();
        else
            ConfirmSubstitution();
		return FReply::Handled();
	}
	if (Key == EKeys::BackSpace || Key == EKeys::Delete)
	{
		CancelPendingSubstitution();
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

FReply USoccerSubstitutionMenuWidget::NativeOnAnalogValueChanged(
    const FGeometry& InGeometry, const FAnalogInputEvent& InAnalogInputEvent)
{
    if ((StyleEditor && StyleEditor->HasFocusedInput()) ||
        FSoccerUINavigation::IsEditingText(WidgetTree))
        return Super::NativeOnAnalogValueChanged(InGeometry, InAnalogInputEvent);
    int32 X = 0, Y = 0;
    if (FSoccerUINavigation::ReadStick(InAnalogInputEvent, AnalogNavigation, X, Y))
    {
        if (Y) { FocusedSelectorIndex = Y < 0 ? 0 : 1; UpdateSelectorFocus(true); }
        if (X) { if (FocusedSelectorIndex == 0) CycleOutgoing(X); else CycleIncoming(X); }
        return FReply::Handled();
    }
    return Super::NativeOnAnalogValueChanged(InGeometry, InAnalogInputEvent);
}

void USoccerSubstitutionMenuWidget::UpdateSelectorFocus(bool bSetKeyboardFocus)
{
    const FLinearColor NormalCard = StyleEditor
        ? StyleEditor->GetColor(TEXT("CardColor"), FLinearColor(.035f, .055f, .075f, .96f))
        : FLinearColor(.035f, .055f, .075f, .96f);
    const FLinearColor FocusedCard = FLinearColor::LerpUsingHSV(
        NormalCard, FLinearColor(.05f, .23f, .30f, .96f), .72f);
    for (int32 Index = 0; Index < LiveSelectorCards.Num(); ++Index)
    {
        if (IsValid(LiveSelectorCards[Index]))
            LiveSelectorCards[Index]->SetBrushColor(
                Index == FocusedSelectorIndex ? FocusedCard : NormalCard);
    }
    if (bSetKeyboardFocus)
    {
        UButton* FocusTarget = FocusedSelectorIndex == 0 ? OutgoingNextButton : IncomingNextButton;
        FSoccerUINavigation::FocusAndReveal(this, FocusTarget);
    }
}

void USoccerSubstitutionMenuWidget::BuildWidgetTree()
{
	if (WidgetTree == nullptr || WidgetTree->RootWidget != nullptr)
	{
		return;
	}

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget = Root;
	UBorder* Dim = WidgetTree->ConstructWidget<UBorder>();
	Dim->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.45f));
	UCanvasPanelSlot* DimSlot = Root->AddChildToCanvas(Dim);
	DimSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	DimSlot->SetOffsets(FMargin(0.0f));

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>();
	LiveMainPanel = Panel;
	Panel->SetBrushColor(MenuBackground);
	Panel->SetPadding(FMargin(28.0f));
	UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel);
	LiveMainCanvasSlot = PanelSlot;
	PanelSlot->SetAnchors(FAnchors(0.14f, 0.10f, 0.86f, 0.90f));
	PanelSlot->SetOffsets(FMargin(0.0f));

	UVerticalBox* Main = WidgetTree->ConstructWidget<UVerticalBox>();
	Panel->AddChild(Main);
	UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>();
	AddVertical(Main, Header, FMargin(0.0f, 0.0f, 0.0f, 18.0f));
	AddHorizontal(Header, MakeText(WidgetTree, TEXT("CAMBIOS"), 30), FMargin(0.0f, 0.0f, 20.0f, 0.0f), ESlateSizeRule::Fill);
	CloseMenuButton = MakeButton(WidgetTree, TEXT("VOLVER AL PARTIDO"), 16, FLinearColor(0.11f, 0.16f, 0.21f, 1.0f));
	CloseMenuButton->OnClicked.AddDynamic(this, &USoccerSubstitutionMenuWidget::HandleCloseClicked);
	AddHorizontal(Header, CloseMenuButton);

	SummaryText = MakeText(WidgetTree, TEXT("--"), 18, MutedText);
	AddVertical(Main, SummaryText, FMargin(0.0f, 0.0f, 0.0f, 18.0f));

	auto BuildSelector = [this, Main](
		const FString& Label,
		UTextBlock*& ValueText,
		UButton*& PreviousButton,
		UButton*& NextButton
	)
	{
		UBorder* Card = WidgetTree->ConstructWidget<UBorder>();
		LiveSelectorCards.Add(Card);
		Card->SetBrushColor(CardBackground);
		Card->SetPadding(FMargin(16.0f, 13.0f));
		AddVertical(Main, Card, FMargin(0.0f, 0.0f, 0.0f, 12.0f));
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
		Card->AddChild(Row);
		USizeBox* LabelSize = WidgetTree->ConstructWidget<USizeBox>();
		LabelSize->SetWidthOverride(175.0f);
		LabelSize->AddChild(MakeText(WidgetTree, Label, 18, MutedText));
		AddHorizontal(Row, LabelSize, FMargin(0.0f, 0.0f, 12.0f, 0.0f));
		PreviousButton = MakeButton(WidgetTree, TEXT("<"), 22);
		AddHorizontal(Row, PreviousButton, FMargin(0.0f, 0.0f, 10.0f, 0.0f));
		ValueText = MakeText(WidgetTree, TEXT("--"), 19);
		ValueText->SetJustification(ETextJustify::Center);
		AddHorizontal(Row, ValueText, FMargin(0.0f, 0.0f, 10.0f, 0.0f), ESlateSizeRule::Fill);
		NextButton = MakeButton(WidgetTree, TEXT(">"), 22);
		AddHorizontal(Row, NextButton);
	};

	UButton* OutPrevious = nullptr;
	UButton* OutNext = nullptr;
	BuildSelector(TEXT("SALE"), OutgoingValueText, OutPrevious, OutNext);
	OutPrevious->OnClicked.AddDynamic(this, &USoccerSubstitutionMenuWidget::HandleOutgoingPrevious);
	OutNext->OnClicked.AddDynamic(this, &USoccerSubstitutionMenuWidget::HandleOutgoingNext);
    OutgoingNextButton = OutNext;

	UButton* InPrevious = nullptr;
	UButton* InNext = nullptr;
	BuildSelector(TEXT("ENTRA"), IncomingValueText, InPrevious, InNext);
	InPrevious->OnClicked.AddDynamic(this, &USoccerSubstitutionMenuWidget::HandleIncomingPrevious);
	InNext->OnClicked.AddDynamic(this, &USoccerSubstitutionMenuWidget::HandleIncomingNext);
    IncomingNextButton = InNext;

	ComparisonText = MakeText(WidgetTree, TEXT("--"), 18, FLinearColor(0.82f, 0.90f, 0.96f, 1.0f));
	AddVertical(Main, ComparisonText, FMargin(0.0f, 8.0f, 0.0f, 14.0f));
	PendingText = MakeText(WidgetTree, TEXT("--"), 17, FLinearColor(1.0f, 0.82f, 0.38f, 1.0f));
	AddVertical(Main, PendingText, FMargin(0.0f, 0.0f, 0.0f, 16.0f));

	UHorizontalBox* Actions = WidgetTree->ConstructWidget<UHorizontalBox>();
	AddVertical(Main, Actions, FMargin(0.0f, 0.0f, 0.0f, 16.0f));
	ConfirmButton = MakeButton(WidgetTree, TEXT("CONFIRMAR CAMBIO"), 18, FLinearColor(0.05f, 0.55f, 0.28f, 1.0f));
	ConfirmButton->OnClicked.AddDynamic(this, &USoccerSubstitutionMenuWidget::HandleConfirmClicked);
	AddHorizontal(Actions, ConfirmButton, FMargin(0.0f, 0.0f, 12.0f, 0.0f), ESlateSizeRule::Fill);
	CancelButton = MakeButton(WidgetTree, TEXT("CANCELAR SOLICITUD"), 18, FLinearColor(0.56f, 0.20f, 0.12f, 1.0f));
	CancelButton->OnClicked.AddDynamic(this, &USoccerSubstitutionMenuWidget::HandleCancelClicked);
	AddHorizontal(Actions, CancelButton, FMargin(12.0f, 0.0f, 0.0f, 0.0f), ESlateSizeRule::Fill);

	StatusText = MakeText(WidgetTree, TEXT("Listo."), 16, MutedText);
	AddVertical(Main, StatusText, FMargin(0.0f, 0.0f, 0.0f, 8.0f));
	UTextBlock* Help = MakeText(
		WidgetTree,
		TEXT("Flechas / cruceta / stick: elegir   Arriba/Abajo: selector   Enter/A: confirmar   Retroceso: cancelar   N/B/Esc: cerrar"),
		15,
		MutedText
	);
    Help->SetAutoWrapText(true);
	AddVertical(Main, Help);

	StyleEditor = NewObject<USoccerUIStyleEditorController>(this);
	TArray<FSoccerEditableStyleField> Fields;
	Fields.Add(FSoccerEditableStyleField::Number(TEXT("LeftAnchor"), TEXT("Borde izquierdo (0 a 0.4)"), .14f, 0.f, .4f));
	Fields.Add(FSoccerEditableStyleField::Number(TEXT("TopAnchor"), TEXT("Borde superior (0 a 0.3)"), .10f, 0.f, .3f));
	Fields.Add(FSoccerEditableStyleField::Number(TEXT("PanelPadding"), TEXT("Margen panel"), 28.f, 4.f, 65.f));
	Fields.Add(FSoccerEditableStyleField::Number(TEXT("CardPadding"), TEXT("Margen de fichas"), 16.f, 2.f, 40.f));
	Fields.Add(FSoccerEditableStyleField::Number(TEXT("FontScale"), TEXT("Escala de letra"), 1.f, .65f, 1.5f));
	Fields.Add(FSoccerEditableStyleField::Color(TEXT("PanelColor"), TEXT("Fondo del panel"), TEXT("#04060AF7")));
	Fields.Add(FSoccerEditableStyleField::Color(TEXT("CardColor"), TEXT("Color de tarjetas"), TEXT("#090E13F5")));
	StyleEditor->Setup(WidgetTree, Root, TEXT("SubstitutionStyle"), Fields);
	StyleEditor->OnApplied.AddDynamic(this, &USoccerSubstitutionMenuWidget::HandleStyleApplied);
}

void USoccerSubstitutionMenuWidget::HandleStyleApplied()
{
	ApplyLiveStyle();
}

void USoccerSubstitutionMenuWidget::ApplyLiveStyle()
{
	if (!StyleEditor || !LiveMainPanel || !LiveMainCanvasSlot) return;
    const FVector2D ViewportUnits = FSoccerUITheme::GetLogicalViewportSize(this);
    const float RequestedLeft = StyleEditor->GetNumber(TEXT("LeftAnchor"), .14f);
    const float RequestedTop = StyleEditor->GetNumber(TEXT("TopAnchor"), .10f);
    const float Left = ViewportUnits.X > 1.f && ViewportUnits.X < 1100.f
        ? FMath::Min(RequestedLeft, .06f) : RequestedLeft;
    const float Top = ViewportUnits.Y > 1.f && ViewportUnits.Y < 750.f
        ? FMath::Min(RequestedTop, .06f) : RequestedTop;
    LiveMainCanvasSlot->SetAnchors(FAnchors(Left, Top, 1.f - Left, 1.f - Top));
    const float RequestedPadding = StyleEditor->GetNumber(TEXT("PanelPadding"), 28.f);
    LiveMainPanel->SetPadding(FMargin(ViewportUnits.X > 1.f
        ? FMath::Min(RequestedPadding, FMath::Max(8.f, ViewportUnits.X * .025f))
        : RequestedPadding));
	LiveMainPanel->SetBrushColor(StyleEditor->GetColor(TEXT("PanelColor"), MenuBackground));
	for (UBorder* Card : LiveSelectorCards)
	{
		if (!Card) continue;
		Card->SetBrushColor(StyleEditor->GetColor(TEXT("CardColor"), CardBackground));
		const float Pad = StyleEditor->GetNumber(TEXT("CardPadding"), 16.f);
		Card->SetPadding(FMargin(Pad, 13.f));
	}
    StyleEditor->ApplyTextScale(LiveMainPanel, FSoccerUITheme::GetAdaptiveTextScale(
        this, StyleEditor->GetNumber(TEXT("FontScale"), 1.f)));
    FSoccerUITheme::ApplyButtonStates(LiveMainPanel);
    UpdateSelectorFocus(false);
}

void USoccerSubstitutionMenuWidget::RefreshPlayerLists(bool bPreserveSelection)
{
	const FName PreviousOutgoing = ActivePlayerIds.IsValidIndex(SelectedOutgoingIndex)
		? ActivePlayerIds[SelectedOutgoingIndex]
		: NAME_None;
	const FName PreviousIncoming = BenchPlayerIds.IsValidIndex(SelectedIncomingIndex)
		? BenchPlayerIds[SelectedIncomingIndex]
		: NAME_None;
	ActivePlayerIds.Reset();
	ActiveSlotIds.Reset();
	BenchPlayerIds.Reset();
	if (!IsValid(MatchManager))
	{
		RefreshDisplay();
		return;
	}

	const ESoccerFormationSystem FormationSystem =
		MatchManager->GetFormationSystemForTeam(ESoccerTeam::PlayerTeam);
	const FSoccerFormationDefinition& Formation =
		SoccerFormationLibrary::GetDefinition(FormationSystem);
	for (const FSoccerFormationSlot& FormationSlot : Formation.Slots)
	{
		ASoccerCharacterBase* Character =
			MatchManager->GetFormationSlotAssignedCharacter(
				ESoccerTeam::PlayerTeam,
				FormationSlot.SlotId
			);
		if (IsValid(Character) && !Character->GetPlayerProfileId().IsNone())
		{
			ActivePlayerIds.Add(Character->GetPlayerProfileId());
			ActiveSlotIds.Add(FormationSlot.SlotId);
		}
	}
	BenchPlayerIds = MatchManager->GetMatchSquadState(
		ESoccerTeam::PlayerTeam
	).AvailableBenchPlayerIds;

	SelectedOutgoingIndex = bPreserveSelection
		? ActivePlayerIds.IndexOfByKey(PreviousOutgoing)
		: INDEX_NONE;
	SelectedIncomingIndex = bPreserveSelection
		? BenchPlayerIds.IndexOfByKey(PreviousIncoming)
		: INDEX_NONE;
	if (!ActivePlayerIds.IsValidIndex(SelectedOutgoingIndex) && ActivePlayerIds.Num() > 0)
	{
		SelectedOutgoingIndex = 0;
	}
	if (!BenchPlayerIds.IsValidIndex(SelectedIncomingIndex) && BenchPlayerIds.Num() > 0)
	{
		SelectedIncomingIndex = 0;
	}
	RefreshDisplay();
}

FString USoccerSubstitutionMenuWidget::DescribePlayer(FName PlayerId) const
{
	if (PlayerId.IsNone())
	{
		return TEXT("--");
	}
	const USoccerGameInstance* GameInstance = GetWorld() != nullptr
		? Cast<USoccerGameInstance>(GetWorld()->GetGameInstance())
		: nullptr;
	const USoccerPlayerProfile* Profile = IsValid(GameInstance)
		? GameInstance->FindPlayerProfileById(PlayerId)
		: nullptr;
	if (IsValid(Profile) && !Profile->Identity.DisplayName.IsEmpty())
	{
		return FString::Printf(
			TEXT("%s  #%d  [%s]"),
			*Profile->Identity.DisplayName.ToString(),
			Profile->Identity.ShirtNumber,
			*PlayerId.ToString()
		);
	}
	return PlayerId.ToString();
}

FString USoccerSubstitutionMenuWidget::DescribeOutgoingSelection() const
{
	if (!ActivePlayerIds.IsValidIndex(SelectedOutgoingIndex) || !ActiveSlotIds.IsValidIndex(SelectedOutgoingIndex))
	{
		return TEXT("No hay jugador seleccionable");
	}
	return FString::Printf(
		TEXT("%s  |  Puesto: %s  |  Energía: %.0f%%"),
		*DescribePlayer(ActivePlayerIds[SelectedOutgoingIndex]),
		*ActiveSlotIds[SelectedOutgoingIndex].ToString(),
		GetSelectedOutgoingEnergyPercent() * 100.0f
	);
}

FString USoccerSubstitutionMenuWidget::DescribeIncomingSelection() const
{
	if (!BenchPlayerIds.IsValidIndex(SelectedIncomingIndex))
	{
		return TEXT("No hay suplentes disponibles");
	}
	return FString::Printf(
		TEXT("%s  |  Compatibilidad: %d/100"),
		*DescribePlayer(BenchPlayerIds[SelectedIncomingIndex]),
		GetSelectedIncomingSuitability()
	);
}

float USoccerSubstitutionMenuWidget::GetSelectedOutgoingEnergyPercent() const
{
	if (!IsValid(MatchManager) || !ActiveSlotIds.IsValidIndex(SelectedOutgoingIndex))
	{
		return 0.0f;
	}
	ASoccerCharacterBase* Character = MatchManager->GetFormationSlotAssignedCharacter(
		ESoccerTeam::PlayerTeam,
		ActiveSlotIds[SelectedOutgoingIndex]
	);
	if (const AThirdPersonCppCharacter* Human = Cast<AThirdPersonCppCharacter>(Character))
	{
		return Human->GetPlayerEnergyPercent();
	}
	if (const ASoccerAICharacter* AICharacter = Cast<ASoccerAICharacter>(Character))
	{
		return AICharacter->GetAIPlayerEnergyPercent();
	}
	return 0.0f;
}

int32 USoccerSubstitutionMenuWidget::GetSelectedIncomingSuitability() const
{
	if (
		!IsValid(MatchManager) ||
		!ActiveSlotIds.IsValidIndex(SelectedOutgoingIndex) ||
		!BenchPlayerIds.IsValidIndex(SelectedIncomingIndex)
	)
	{
		return 0;
	}
	const USoccerGameInstance* GameInstance = GetWorld() != nullptr
		? Cast<USoccerGameInstance>(GetWorld()->GetGameInstance())
		: nullptr;
	const USoccerPlayerProfile* Profile = IsValid(GameInstance)
		? GameInstance->FindPlayerProfileById(BenchPlayerIds[SelectedIncomingIndex])
		: nullptr;
	return USoccerLineupEvaluationLibrary::EvaluatePlayerForFormationSlot(
		Profile,
		MatchManager->GetFormationSystemForTeam(ESoccerTeam::PlayerTeam),
		ActiveSlotIds[SelectedOutgoingIndex]
	).OverallScore;
}

void USoccerSubstitutionMenuWidget::RefreshDisplay()
{
	if (!IsValid(MatchManager))
	{
		return;
	}
	const FSoccerMatchSquadState State = MatchManager->GetMatchSquadState(
		ESoccerTeam::PlayerTeam
	);
	if (SummaryText != nullptr)
	{
		SummaryText->SetText(FText::FromString(FString::Printf(
			TEXT("Cambios utilizados: %d/%d  |  Restantes: %d  |  Suplentes disponibles: %d"),
			State.SubstitutionHistory.Num(),
			State.MaximumSubstitutions,
			FMath::Max(0, State.MaximumSubstitutions - State.SubstitutionHistory.Num()),
			State.AvailableBenchPlayerIds.Num()
		)));
	}
	if (OutgoingValueText != nullptr)
	{
		OutgoingValueText->SetText(FText::FromString(DescribeOutgoingSelection()));
	}
	if (IncomingValueText != nullptr)
	{
		IncomingValueText->SetText(FText::FromString(DescribeIncomingSelection()));
	}
	if (ComparisonText != nullptr)
	{
		const int32 Fit = GetSelectedIncomingSuitability();
		const FString Warning = Fit < 35
			? TEXT("  |  ADVERTENCIA: jugador improvisado en este puesto")
			: TEXT("");
		ComparisonText->SetText(FText::FromString(FString::Printf(
			TEXT("El cambio conserva el puesto táctico. Compatibilidad del reemplazante: %d/100%s"),
			Fit,
			*Warning
		)));
	}

	FSoccerMatchSubstitutionRequest PendingRequest;
	const bool bHasPending = MatchManager->GetPendingMatchSubstitution(
		ESoccerTeam::PlayerTeam,
		PendingRequest
	);
	if (PendingText != nullptr)
	{
		PendingText->SetText(FText::FromString(
			bHasPending
				? FString::Printf(
					TEXT("CAMBIO PENDIENTE — Sale: %s  |  Entra: %s  |  Se ejecutará en una pausa segura."),
					*DescribePlayer(PendingRequest.OutgoingPlayerId),
					*DescribePlayer(PendingRequest.IncomingPlayerId)
				)
				: TEXT("No hay un cambio pendiente.")
		));
	}
	const bool bCanConfirm =
		!bHasPending &&
		State.SubstitutionHistory.Num() < State.MaximumSubstitutions &&
		ActivePlayerIds.IsValidIndex(SelectedOutgoingIndex) &&
		BenchPlayerIds.IsValidIndex(SelectedIncomingIndex);
	if (ConfirmButton != nullptr) ConfirmButton->SetIsEnabled(bCanConfirm);
	if (CancelButton != nullptr) CancelButton->SetIsEnabled(bHasPending);
}

void USoccerSubstitutionMenuWidget::CycleOutgoing(int32 Direction)
{
    FocusedSelectorIndex = 0;
    UpdateSelectorFocus(false);
	if (ActivePlayerIds.Num() <= 0) return;
	const int32 Step = Direction >= 0 ? 1 : -1;
	SelectedOutgoingIndex =
		(SelectedOutgoingIndex + Step + ActivePlayerIds.Num()) % ActivePlayerIds.Num();
	RefreshDisplay();
    if (LiveSelectorCards.IsValidIndex(0))
    {
        UIAnimations.StartSelection(LiveSelectorCards[0]);
    }
}

void USoccerSubstitutionMenuWidget::CycleIncoming(int32 Direction)
{
    FocusedSelectorIndex = 1;
    UpdateSelectorFocus(false);
	if (BenchPlayerIds.Num() <= 0) return;
	const int32 Step = Direction >= 0 ? 1 : -1;
	SelectedIncomingIndex =
		(SelectedIncomingIndex + Step + BenchPlayerIds.Num()) % BenchPlayerIds.Num();
	RefreshDisplay();
    if (LiveSelectorCards.IsValidIndex(1))
    {
        UIAnimations.StartSelection(LiveSelectorCards[1]);
    }
}

void USoccerSubstitutionMenuWidget::ConfirmSubstitution()
{
	if (
		!IsValid(MatchManager) ||
		!ActivePlayerIds.IsValidIndex(SelectedOutgoingIndex) ||
		!BenchPlayerIds.IsValidIndex(SelectedIncomingIndex)
	)
	{
		SetStatus(TEXT("No hay una pareja válida para confirmar."), true);
		return;
	}
	if (MatchManager->RequestMatchSubstitution(
		ESoccerTeam::PlayerTeam,
		ActivePlayerIds[SelectedOutgoingIndex],
		BenchPlayerIds[SelectedIncomingIndex]
	))
	{
		SetStatus(
			MatchManager->HasPendingMatchSubstitution(ESoccerTeam::PlayerTeam)
				? TEXT("Cambio solicitado. Se ejecutará en la próxima pausa segura.")
				: TEXT("Cambio ejecutado."),
			false
		);
		RefreshPlayerLists(true);
	}
	else
	{
		// The match manager is authoritative, but show the player *why* it
		// rejected this request rather than always blaming the bench/limit.
		const FSoccerMatchSquadState State = MatchManager->GetMatchSquadState(
			ESoccerTeam::PlayerTeam
		);
		const FName OutgoingId = ActivePlayerIds[SelectedOutgoingIndex];
		const FName IncomingId = BenchPlayerIds[SelectedIncomingIndex];
		bool bOutgoingTrackedAsActive = false;
		for (const TPair<FName, FName>& ActiveEntry : State.ActivePlayerByFormationSlot)
		{
			if (ActiveEntry.Value == OutgoingId)
			{
				bOutgoingTrackedAsActive = true;
				break;
			}
		}

		FString RejectionReason;
		if (!State.bInitialized)
		{
			RejectionReason = TEXT("El plantel del partido no está inicializado: verificá los 7 titulares en Director Técnico.");
		}
		else if (State.SubstitutionHistory.Num() >= State.MaximumSubstitutions)
		{
			RejectionReason = TEXT("Ya se alcanzó el límite de sustituciones.");
		}
		else if (MatchManager->HasPendingMatchSubstitution(ESoccerTeam::PlayerTeam))
		{
			RejectionReason = TEXT("Ya existe un cambio pendiente. Esperá una interrupción o cancelalo.");
		}
		else if (!State.AvailableBenchPlayerIds.Contains(IncomingId))
		{
			RejectionReason = TEXT("El jugador elegido para entrar ya no está en el banco disponible.");
		}
		else if (State.WithdrawnPlayerIds.Contains(IncomingId))
		{
			RejectionReason = TEXT("Este jugador ya salió del partido y no puede volver a ingresar.");
		}
		else if (!bOutgoingTrackedAsActive)
		{
			RejectionReason = TEXT("El titular no figura en la alineación registrada del partido. Revisá perfiles y asignaciones de formación.");
		}
		else
		{
			RejectionReason = TEXT("Cambio rechazado. Consultá [Substitution] en Output Log para identificar la condición.");
		}
		SetStatus(RejectionReason, true);
		RefreshDisplay();
	}
}

void USoccerSubstitutionMenuWidget::CancelPendingSubstitution()
{
	if (!IsValid(MatchManager)) return;
	if (MatchManager->CancelPendingMatchSubstitution(ESoccerTeam::PlayerTeam))
	{
		SetStatus(TEXT("Solicitud pendiente cancelada."));
	}
	else
	{
		SetStatus(TEXT("No había una solicitud pendiente."), true);
	}
	RefreshPlayerLists(true);
}

void USoccerSubstitutionMenuWidget::SetStatus(const FString& Message, bool bError)
{
	if (StatusText != nullptr)
	{
		StatusText->SetText(FText::FromString(Message));
		StatusText->SetColorAndOpacity(FSlateColor(
			bError
				? FLinearColor(1.0f, 0.40f, 0.34f, 1.0f)
				: FLinearColor(0.55f, 0.92f, 0.72f, 1.0f)
		));
	}
}

void USoccerSubstitutionMenuWidget::HandleOutgoingPrevious() { CycleOutgoing(-1); }
void USoccerSubstitutionMenuWidget::HandleOutgoingNext() { CycleOutgoing(1); }
void USoccerSubstitutionMenuWidget::HandleIncomingPrevious() { CycleIncoming(-1); }
void USoccerSubstitutionMenuWidget::HandleIncomingNext() { CycleIncoming(1); }
void USoccerSubstitutionMenuWidget::HandleConfirmClicked() { ConfirmSubstitution(); }
void USoccerSubstitutionMenuWidget::HandleCancelClicked() { CancelPendingSubstitution(); }
void USoccerSubstitutionMenuWidget::HandleCloseClicked() { CloseMenu(); }
