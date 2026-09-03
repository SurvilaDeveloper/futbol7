#include "SoccerSubstitutionMenuWidget.h"

#include "SoccerAICharacter.h"
#include "SoccerCharacterBase.h"
#include "SoccerFormationLibrary.h"
#include "SoccerGameInstance.h"
#include "SoccerLineupEvaluationLibrary.h"
#include "SoccerMatchManager.h"
#include "SoccerPlayerProfile.h"
#include "ThirdPersonCppCharacter.h"

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

void USoccerSubstitutionMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
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
	if (ConfirmButton != nullptr)
	{
		ConfirmButton->SetUserFocus(PlayerController);
	}
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
	if (Key == EKeys::N || Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right)
	{
		CloseMenu();
		return FReply::Handled();
	}
	if (Key == EKeys::Up || Key == EKeys::Gamepad_DPad_Up)
	{
		FocusedSelectorIndex = 0;
		SetStatus(TEXT("Seleccionando al jugador que sale."));
		return FReply::Handled();
	}
	if (Key == EKeys::Down || Key == EKeys::Gamepad_DPad_Down)
	{
		FocusedSelectorIndex = 1;
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
	if (Key == EKeys::Enter || Key == EKeys::SpaceBar || Key == EKeys::Gamepad_FaceButton_Bottom)
	{
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
	Panel->SetBrushColor(MenuBackground);
	Panel->SetPadding(FMargin(28.0f));
	UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel);
	PanelSlot->SetAnchors(FAnchors(0.14f, 0.10f, 0.86f, 0.90f));
	PanelSlot->SetOffsets(FMargin(0.0f));

	UVerticalBox* Main = WidgetTree->ConstructWidget<UVerticalBox>();
	Panel->AddChild(Main);
	UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>();
	AddVertical(Main, Header, FMargin(0.0f, 0.0f, 0.0f, 18.0f));
	AddHorizontal(Header, MakeText(WidgetTree, TEXT("CAMBIOS"), 30), FMargin(0.0f, 0.0f, 20.0f, 0.0f), ESlateSizeRule::Fill);
	UButton* CloseButton = MakeButton(WidgetTree, TEXT("VOLVER AL PARTIDO"), 16, FLinearColor(0.11f, 0.16f, 0.21f, 1.0f));
	CloseButton->OnClicked.AddDynamic(this, &USoccerSubstitutionMenuWidget::HandleCloseClicked);
	AddHorizontal(Header, CloseButton);

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

	UButton* InPrevious = nullptr;
	UButton* InNext = nullptr;
	BuildSelector(TEXT("ENTRA"), IncomingValueText, InPrevious, InNext);
	InPrevious->OnClicked.AddDynamic(this, &USoccerSubstitutionMenuWidget::HandleIncomingPrevious);
	InNext->OnClicked.AddDynamic(this, &USoccerSubstitutionMenuWidget::HandleIncomingNext);

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
		TEXT("Mouse o flechas: elegir   Arriba/Abajo: selector   Enter: confirmar   Retroceso: cancelar   N/Esc: cerrar"),
		15,
		MutedText
	);
	AddVertical(Main, Help);
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
	if (ActivePlayerIds.Num() <= 0) return;
	const int32 Step = Direction >= 0 ? 1 : -1;
	SelectedOutgoingIndex =
		(SelectedOutgoingIndex + Step + ActivePlayerIds.Num()) % ActivePlayerIds.Num();
	RefreshDisplay();
}

void USoccerSubstitutionMenuWidget::CycleIncoming(int32 Direction)
{
	if (BenchPlayerIds.Num() <= 0) return;
	const int32 Step = Direction >= 0 ? 1 : -1;
	SelectedIncomingIndex =
		(SelectedIncomingIndex + Step + BenchPlayerIds.Num()) % BenchPlayerIds.Num();
	RefreshDisplay();
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
		SetStatus(TEXT("La solicitud fue rechazada. Revisá el banco, el límite y el cambio pendiente."), true);
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
