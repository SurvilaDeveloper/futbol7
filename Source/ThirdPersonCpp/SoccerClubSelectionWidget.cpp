#include "SoccerClubSelectionWidget.h"
#include "SoccerUITheme.h"

#include "SoccerClubProfile.h"
#include "SoccerClubIdButton.h"
#include "SoccerGameInstance.h"
#include "SoccerMatchManager.h"
#include "SoccerSquadCatalog.h"
#include "SoccerSquadViewerWidget.h"
#include "SoccerMatchHistoryWidget.h"
#include "SoccerUIStyleEditorController.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Texture2D.h"

namespace
{
    UTextBlock* MakeClubSelectionText(
        UWidgetTree* WidgetTree,
        const FString& Text,
        int32 FontSize
    )
    {
        UTextBlock* TextBlock = WidgetTree->ConstructWidget<UTextBlock>();
        TextBlock->SetText(FText::FromString(Text));
        FSlateFontInfo Font = TextBlock->Font;
        Font.Size = FontSize;
        TextBlock->SetFont(Font);
        TextBlock->SetColorAndOpacity(FSlateColor(FLinearColor::White));
        TextBlock->SetJustification(ETextJustify::Center);
        return TextBlock;
    }

    UButton* MakeClubSelectionButton(
        UWidgetTree* WidgetTree,
        const FString& Label
    )
    {
        UButton* Button = WidgetTree->ConstructWidget<UButton>();
        Button->AddChild(MakeClubSelectionText(WidgetTree, Label, 18));
        return Button;
    }
}

void USoccerClubSelectionWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
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

void USoccerClubSelectionWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    BuildWidgetTree();
    ApplyLiveStyle();
}

void USoccerClubSelectionWidget::InitializeForMatchManager(
    ASoccerMatchManager* InMatchManager
)
{
    MatchManager = InMatchManager;
    UWorld* World = GetWorld();
    SoccerGameInstance = World != nullptr
        ? Cast<USoccerGameInstance>(World->GetGameInstance())
        : nullptr;
    RefreshAvailableClubs();
    RebuildClubGrid();
    RefreshTexts();
}

bool USoccerClubSelectionWidget::HasSelectableOpponent() const
{
    return SelectableOpponentClubIds.Num() > 0;
}

void USoccerClubSelectionWidget::ActivateMenu(bool bPauseGame)
{
    APlayerController* PlayerController = GetOwningPlayer();
    if (PlayerController == nullptr)
    {
        return;
    }

    bPreviousMouseCursorVisible = PlayerController->bShowMouseCursor;
    bAppliedGamePause = false;
    if (
        bPauseGame && GetWorld() != nullptr &&
        !UGameplayStatics::IsGamePaused(GetWorld())
    )
    {
        UGameplayStatics::SetGamePaused(GetWorld(), true);
        bAppliedGamePause = true;
    }

    PlayerController->bShowMouseCursor = true;
    PlayerController->bEnableClickEvents = true;
    FInputModeUIOnly InputMode;
    const FName InitialClubId = SelectableOpponentClubIds.IsValidIndex(SelectedOpponentIndex)
        ? SelectableOpponentClubIds[SelectedOpponentIndex] : NAME_None;
    UWidget* InitialFocus = ClubButtonsById.FindRef(InitialClubId);
    if (UWidget* FocusTarget = InitialFocus ? InitialFocus : static_cast<UWidget*>(ConfirmButton))
        InputMode.SetWidgetToFocus(FocusTarget->TakeWidget());
    else
        InputMode.SetWidgetToFocus(TakeWidget());
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    PlayerController->SetInputMode(InputMode);

    FocusSelectedClub();
    UIAnimations.StartEntrance(LiveMainPanel);
}

FReply USoccerClubSelectionWidget::NativeOnPreviewKeyDown(
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

    if (Key == EKeys::H)
    {
        HandleHistoryClicked();
        return FReply::Handled();
    }
    if (Key == EKeys::Gamepad_FaceButton_Top || Key == EKeys::V)
    {
        HandleViewSquadsClicked();
        return FReply::Handled();
    }
    int32 Horizontal = FSoccerUINavigation::Horizontal(Key);
    if (Key == EKeys::A) Horizontal = -1;
    if (Key == EKeys::D) Horizontal = 1;
    if (Horizontal != 0) { CycleOpponent(Horizontal); return FReply::Handled(); }
    const int32 Vertical = FSoccerUINavigation::Vertical(Key);
    if (Vertical != 0) { NavigateOpponentGrid(Vertical); return FReply::Handled(); }
    if (FSoccerUINavigation::IsAccept(Key))
    {
        if (IsValid(HistoryButton) && HistoryButton->HasKeyboardFocus())
            HandleHistoryClicked();
        else if (IsValid(ViewSquadsButton) && ViewSquadsButton->HasKeyboardFocus())
            HandleViewSquadsClicked();
        else
            HandleConfirmClicked();
        return FReply::Handled();
    }
    return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

FReply USoccerClubSelectionWidget::NativeOnAnalogValueChanged(
    const FGeometry& InGeometry, const FAnalogInputEvent& InAnalogInputEvent)
{
    if ((StyleEditor && StyleEditor->HasFocusedInput()) ||
        FSoccerUINavigation::IsEditingText(WidgetTree))
        return Super::NativeOnAnalogValueChanged(InGeometry, InAnalogInputEvent);
    int32 X = 0, Y = 0;
    if (FSoccerUINavigation::ReadStick(InAnalogInputEvent, AnalogNavigation, X, Y))
    {
        if (X) CycleOpponent(X);
        if (Y) NavigateOpponentGrid(Y);
        return FReply::Handled();
    }
    return Super::NativeOnAnalogValueChanged(InGeometry, InAnalogInputEvent);
}

void USoccerClubSelectionWidget::RefreshAvailableClubs()
{
    SelectableOpponentClubIds.Reset();
    SelectedOpponentIndex = INDEX_NONE;
    if (!IsValid(SoccerGameInstance))
    {
        return;
    }

    const FName HumanClubId = SoccerGameInstance->GetDefaultHumanClubId();
    const FName PreviousOpponentId =
        SoccerGameInstance->GetSelectedOpponentClubId();

    for (const FName ClubId : SoccerGameInstance->GetAvailableClubIds())
    {
        if (ClubId != HumanClubId)
        {
            SelectableOpponentClubIds.Add(ClubId);
        }
    }

    SelectedOpponentIndex =
        SelectableOpponentClubIds.IndexOfByKey(PreviousOpponentId);
    if (SelectedOpponentIndex == INDEX_NONE && SelectableOpponentClubIds.Num() > 0)
    {
        SelectedOpponentIndex = 0;
    }
}

FString USoccerClubSelectionWidget::GetClubDisplayName(FName ClubId) const
{
    if (!IsValid(SoccerGameInstance))
    {
        return ClubId.ToString();
    }

    USoccerSquadCatalog* SquadCatalog =
        SoccerGameInstance->FindSquadCatalogByClubId(ClubId);
    if (
        IsValid(SquadCatalog) && IsValid(SquadCatalog->ClubProfile) &&
        !SquadCatalog->ClubProfile->DisplayName.IsEmpty()
    )
    {
        return SquadCatalog->ClubProfile->DisplayName.ToString();
    }
    return ClubId.ToString();
}

USoccerClubProfile* USoccerClubSelectionWidget::GetClubProfile(
    FName ClubId
) const
{
    if (!IsValid(SoccerGameInstance))
    {
        return nullptr;
    }

    USoccerSquadCatalog* SquadCatalog =
        SoccerGameInstance->FindSquadCatalogByClubId(ClubId);

    return IsValid(SquadCatalog) ? SquadCatalog->ClubProfile : nullptr;
}

void USoccerClubSelectionWidget::RebuildClubGrid()
{
    ClubButtonsById.Reset();
    ClubCardBordersById.Reset();

    if (ClubGrid == nullptr || WidgetTree == nullptr)
    {
        return;
    }

    ClubGrid->ClearChildren();
    ClubGrid->SetSlotPadding(FMargin(StyleEditor ? StyleEditor->GetNumber(TEXT("GridGap"), 8.f) : 8.f));
    const int32 RequestedColumnCount = FMath::Clamp(FMath::RoundToInt(
        StyleEditor ? StyleEditor->GetNumber(TEXT("Columns"), float(ClubGridColumnCount))
                    : float(ClubGridColumnCount)), 1, 8);
    const float CardItemWidth = StyleEditor ? StyleEditor->GetNumber(TEXT("CardWidth"), ClubCardWidth) : ClubCardWidth;
    const float CellGap = StyleEditor ? StyleEditor->GetNumber(TEXT("GridGap"), 8.f) : 8.f;
    const float InnerPadding = StyleEditor ? StyleEditor->GetNumber(TEXT("PanelPadding"), 38.f) : 38.f;
    const float RequestedPanelWidth = StyleEditor ? StyleEditor->GetNumber(TEXT("PanelWidth"), 1080.f) : 1080.f;
    const float EffectivePanelWidth = FSoccerUITheme::FitPanelToViewport(this,
        FVector2D(RequestedPanelWidth, 650.f)).X;
    const float UsableWidth = FMath::Max(120.f, EffectivePanelWidth - 2.f * InnerPadding);
    const float VisibleCardWidth = FMath::Min(CardItemWidth, FMath::Max(120.f, UsableWidth - 2.f * CellGap));
    const int32 ColumnsThatFit = FMath::Max(1, FMath::FloorToInt(
        UsableWidth / FMath::Max(1.f, VisibleCardWidth + CellGap * 2.f)));
    const int32 SafeColumnCount = FMath::Min(RequestedColumnCount, ColumnsThatFit);
    RenderedGridColumnCount = SafeColumnCount;

    for (int32 ClubIndex = 0; ClubIndex < SelectableOpponentClubIds.Num(); ++ClubIndex)
    {
        const FName ClubId = SelectableOpponentClubIds[ClubIndex];
        USoccerClubProfile* ClubProfile = GetClubProfile(ClubId);

        USoccerClubIdButton* ClubButton =
            WidgetTree->ConstructWidget<USoccerClubIdButton>();
        ClubButton->SetClubId(ClubId);
        ClubButton->OnClubIdClicked.AddDynamic(
            this,
            &USoccerClubSelectionWidget::HandleClubCardClicked
        );

        USizeBox* CardSize = WidgetTree->ConstructWidget<USizeBox>();
        CardSize->SetWidthOverride(VisibleCardWidth);
        CardSize->SetHeightOverride(StyleEditor ? StyleEditor->GetNumber(TEXT("CardHeight"), ClubCardHeight) : ClubCardHeight);

        UBorder* CardBorder = WidgetTree->ConstructWidget<UBorder>();
        CardBorder->SetPadding(FMargin(StyleEditor ? StyleEditor->GetNumber(TEXT("CardPadding"), 10.f) : 10.f));
        CardBorder->SetBrushColor(StyleEditor ? StyleEditor->GetColor(TEXT("CardColor"), FLinearColor(.07f, .10f, .15f, 1.f)) : FLinearColor(.07f, .10f, .15f, 1.f));
        CardSize->AddChild(CardBorder);

        UVerticalBox* CardColumn = WidgetTree->ConstructWidget<UVerticalBox>();
        CardBorder->AddChild(CardColumn);

        USizeBox* CrestSize = WidgetTree->ConstructWidget<USizeBox>();
        const float RequestedCrest = StyleEditor ? StyleEditor->GetNumber(TEXT("CrestSize"), ClubCrestSize) : ClubCrestSize;
        const float FittedCrest = FMath::Min(RequestedCrest, FMath::Max(48.f, VisibleCardWidth - 36.f));
        CrestSize->SetWidthOverride(FittedCrest);
        CrestSize->SetHeightOverride(FittedCrest);

        UOverlay* CrestOverlay = WidgetTree->ConstructWidget<UOverlay>();
        CrestSize->AddChild(CrestOverlay);

        UBorder* CrestFallback = WidgetTree->ConstructWidget<UBorder>();
        const FLinearColor PrimaryColor = IsValid(ClubProfile)
            ? ClubProfile->PrimaryColor
            : FLinearColor(0.18f, 0.25f, 0.34f, 1.0f);
        CrestFallback->SetBrushColor(FLinearColor(
            PrimaryColor.R,
            PrimaryColor.G,
            PrimaryColor.B,
            1.0f
        ));
        CrestOverlay->AddChildToOverlay(CrestFallback);

        UTexture2D* CrestTexture = IsValid(ClubProfile)
            ? ClubProfile->Crest.LoadSynchronous()
            : nullptr;

        if (IsValid(CrestTexture))
        {
            UImage* CrestImage = WidgetTree->ConstructWidget<UImage>();
            CrestImage->SetBrushFromTexture(CrestTexture, true);
            CrestImage->SetColorAndOpacity(FLinearColor::White);
            CrestOverlay->AddChildToOverlay(CrestImage);
        }
        else
        {
            FString FallbackLabel = IsValid(ClubProfile) &&
                !ClubProfile->ShortName.IsEmpty()
                ? ClubProfile->ShortName.ToString()
                : GetClubDisplayName(ClubId).Left(3).ToUpper();
            UTextBlock* FallbackText = MakeClubSelectionText(
                WidgetTree,
                FallbackLabel,
                28
            );
            UOverlaySlot* FallbackTextSlot =
                CrestOverlay->AddChildToOverlay(FallbackText);
            FallbackTextSlot->SetHorizontalAlignment(HAlign_Center);
            FallbackTextSlot->SetVerticalAlignment(VAlign_Center);
        }

        UVerticalBoxSlot* CrestSlot =
            CardColumn->AddChildToVerticalBox(CrestSize);
        CrestSlot->SetHorizontalAlignment(HAlign_Center);
        CrestSlot->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 10.0f));

        UTextBlock* ClubNameText = MakeClubSelectionText(
            WidgetTree,
            GetClubDisplayName(ClubId),
            17
        );
        ClubNameText->SetAutoWrapText(true);
        UVerticalBoxSlot* ClubNameSlot =
            CardColumn->AddChildToVerticalBox(ClubNameText);
        ClubNameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        ClubNameSlot->SetVerticalAlignment(VAlign_Center);

        ClubButton->AddChild(CardSize);

        UUniformGridSlot* GridSlot = ClubGrid->AddChildToUniformGrid(
            ClubButton,
            ClubIndex / SafeColumnCount,
            ClubIndex % SafeColumnCount
        );
        GridSlot->SetHorizontalAlignment(HAlign_Center);
        GridSlot->SetVerticalAlignment(VAlign_Center);

        ClubButtonsById.Add(ClubId, ClubButton);
        ClubCardBordersById.Add(ClubId, CardBorder);
    }
    if (StyleEditor)
    {
        StyleEditor->ApplyTextScale(ClubGrid, FSoccerUITheme::GetAdaptiveTextScale(
            this, StyleEditor->GetNumber(TEXT("FontScale"), 1.f)));
    }
    // Rebuilding the grid destroys the previously focused card.
    if (IsInViewport() && (!StyleEditor || !StyleEditor->HasFocusedInput()))
        FocusSelectedClub();
}

void USoccerClubSelectionWidget::RefreshCardSelectionStates()
{
    const FName SelectedClubId =
        SelectableOpponentClubIds.IsValidIndex(SelectedOpponentIndex)
        ? SelectableOpponentClubIds[SelectedOpponentIndex]
        : NAME_None;

    for (const TPair<FName, UBorder*>& CardPair : ClubCardBordersById)
    {
        if (CardPair.Value == nullptr)
        {
            continue;
        }

        CardPair.Value->SetBrushColor(
            CardPair.Key == SelectedClubId
            ? (StyleEditor ? StyleEditor->GetColor(TEXT("SelectedColor"), FLinearColor(.10f, .52f, .27f, 1.f)) : FLinearColor(.10f, .52f, .27f, 1.f))
            : (StyleEditor ? StyleEditor->GetColor(TEXT("CardColor"), FLinearColor(.07f, .10f, .15f, 1.f)) : FLinearColor(.07f, .10f, .15f, 1.f))
        );
    }
}

void USoccerClubSelectionWidget::RefreshTexts()
{
    const FName HumanClubId = IsValid(SoccerGameInstance)
        ? SoccerGameInstance->GetDefaultHumanClubId()
        : NAME_None;
    if (HumanClubText != nullptr)
    {
        HumanClubText->SetText(FText::FromString(
            FString::Printf(TEXT("Tu club: %s"), *GetClubDisplayName(HumanClubId))
        ));
    }

    const bool bHasOpponent =
        SelectableOpponentClubIds.IsValidIndex(SelectedOpponentIndex);
    if (OpponentClubText != nullptr)
    {
        OpponentClubText->SetText(FText::FromString(
            bHasOpponent
                ? GetClubDisplayName(SelectableOpponentClubIds[SelectedOpponentIndex])
                : TEXT("No hay otro club disponible")
        ));
    }
    if (ConfirmButton != nullptr)
    {
        ConfirmButton->SetIsEnabled(bHasOpponent);
    }
    if (StatusText != nullptr)
    {
        StatusText->SetText(FText::FromString(
            bHasOpponent
                ? TEXT("Flechas/cruceta/stick: elegir  |  Enter/A: jugar  |  V/Y: ver planteles")
                : TEXT("Creá y vinculá al menos dos planteles de clubes.")
        ));
    }
    RefreshCardSelectionStates();
}

void USoccerClubSelectionWidget::CycleOpponent(int32 Direction)
{
    if (SelectableOpponentClubIds.Num() <= 0)
    {
        return;
    }
    SelectedOpponentIndex =
        (SelectedOpponentIndex + Direction + SelectableOpponentClubIds.Num()) %
        SelectableOpponentClubIds.Num();
    RefreshTexts();
    const FName ActiveClubId = SelectableOpponentClubIds.IsValidIndex(SelectedOpponentIndex)
        ? SelectableOpponentClubIds[SelectedOpponentIndex] : NAME_None;
    UIAnimations.StartSelection(ClubCardBordersById.FindRef(ActiveClubId));
    FocusSelectedClub();
}

void USoccerClubSelectionWidget::NavigateOpponentGrid(int32 Direction)
{
    if (SelectableOpponentClubIds.Num() == 0) return;
    const int32 Delta = Direction * FMath::Max(1, RenderedGridColumnCount);
    SelectedOpponentIndex = FMath::Clamp(SelectedOpponentIndex + Delta,
                                       0, SelectableOpponentClubIds.Num() - 1);
    RefreshTexts();
    FocusSelectedClub();
}

void USoccerClubSelectionWidget::FocusSelectedClub()
{
    if (!SelectableOpponentClubIds.IsValidIndex(SelectedOpponentIndex)) return;
    FSoccerUINavigation::FocusAndReveal(this,
        ClubButtonsById.FindRef(SelectableOpponentClubIds[SelectedOpponentIndex]));
}

void USoccerClubSelectionWidget::HandlePreviousClicked()
{
    CycleOpponent(-1);
}

void USoccerClubSelectionWidget::HandleNextClicked()
{
    CycleOpponent(1);
}

void USoccerClubSelectionWidget::HandleClubCardClicked(FName ClubId)
{
    const int32 ClubIndex = SelectableOpponentClubIds.IndexOfByKey(ClubId);
    if (ClubIndex == INDEX_NONE)
    {
        return;
    }

    SelectedOpponentIndex = ClubIndex;
    RefreshTexts();
    const FName ActiveClubId = SelectableOpponentClubIds.IsValidIndex(SelectedOpponentIndex)
        ? SelectableOpponentClubIds[SelectedOpponentIndex] : NAME_None;
    UIAnimations.StartSelection(ClubCardBordersById.FindRef(ActiveClubId));
}

void USoccerClubSelectionWidget::HandleConfirmClicked()
{
    if (
        !IsValid(SoccerGameInstance) || !IsValid(MatchManager) ||
        !SelectableOpponentClubIds.IsValidIndex(SelectedOpponentIndex)
    )
    {
        return;
    }

    const FName HumanClubId = SoccerGameInstance->GetDefaultHumanClubId();
    const FName OpponentClubId =
        SelectableOpponentClubIds[SelectedOpponentIndex];
    if (!SoccerGameInstance->ConfigureStandaloneMatch(HumanClubId, OpponentClubId))
    {
        return;
    }

    USoccerSquadCatalog* HumanSquad =
        SoccerGameInstance->FindSquadCatalogByClubId(HumanClubId);
    USoccerSquadCatalog* OpponentSquad =
        SoccerGameInstance->FindSquadCatalogByClubId(OpponentClubId);
    MatchManager->SetClubProfileForTeam(
        ESoccerTeam::PlayerTeam,
        IsValid(HumanSquad) ? HumanSquad->ClubProfile : nullptr
    );
    MatchManager->SetClubProfileForTeam(
        ESoccerTeam::OpponentTeam,
        IsValid(OpponentSquad) ? OpponentSquad->ClubProfile : nullptr
    );

	MatchManager->MaterializeConfiguredMatchTeams();

    CloseMenu();
    OnSelectionConfirmed.Broadcast();
}

void USoccerClubSelectionWidget::HandleViewSquadsClicked()
{
    OpenSquadViewer();
}

void USoccerClubSelectionWidget::HandleHistoryClicked()
{
    APlayerController* PC = GetOwningPlayer();
    if (!PC || !IsInViewport()) return;
    if (!IsValid(HistoryWidget))
        HistoryWidget = CreateWidget<USoccerMatchHistoryWidget>(
            PC, USoccerMatchHistoryWidget::StaticClass());
    if (!IsValid(HistoryWidget)) return;
    HistoryWidget->OnHistoryClosed.RemoveDynamic(
        this, &USoccerClubSelectionWidget::HandleHistoryClosed);
    HistoryWidget->OnHistoryClosed.AddDynamic(
        this, &USoccerClubSelectionWidget::HandleHistoryClosed);
    if (!HistoryWidget->IsInViewport()) HistoryWidget->AddToViewport(160);
    HistoryWidget->OpenHistory();
}

void USoccerClubSelectionWidget::HandleHistoryClosed()
{
    // Exactly the same input-mode restoration as when leaving VER PLANTELES.
    HandleSquadViewerClosed();
    if (IsValid(HistoryButton)) HistoryButton->SetKeyboardFocus();
}

void USoccerClubSelectionWidget::OpenSquadViewer()
{
    APlayerController* PlayerController = GetOwningPlayer();
    if (!IsValid(SoccerGameInstance) || !PlayerController || !IsInViewport())
    {
        return;
    }
    if (!IsValid(SquadViewerWidget))
    {
        SquadViewerWidget = CreateWidget<USoccerSquadViewerWidget>(
            PlayerController, USoccerSquadViewerWidget::StaticClass());
    }
    if (!IsValid(SquadViewerWidget))
    {
        return;
    }
    SquadViewerWidget->OnViewerClosed.RemoveDynamic(
        this, &USoccerClubSelectionWidget::HandleSquadViewerClosed);
    SquadViewerWidget->OnViewerClosed.AddDynamic(
        this, &USoccerClubSelectionWidget::HandleSquadViewerClosed);
    SquadViewerWidget->InitializeForClub(
        SoccerGameInstance, MatchManager, SoccerGameInstance->GetDefaultHumanClubId());
    if (!SquadViewerWidget->IsInViewport())
    {
        SquadViewerWidget->AddToViewport(150);
    }
    SquadViewerWidget->ActivateMenu();
}

void USoccerClubSelectionWidget::HandleSquadViewerClosed()
{
    APlayerController* PlayerController = GetOwningPlayer();
    if (!PlayerController || !IsInViewport())
    {
        return;
    }
    // The pre-match screen remains visible and continues to own the pause.
    FInputModeUIOnly InputMode;
    const FName SelectedClubId = SelectableOpponentClubIds.IsValidIndex(SelectedOpponentIndex)
        ? SelectableOpponentClubIds[SelectedOpponentIndex] : NAME_None;
    if (UWidget* SelectedButton = ClubButtonsById.FindRef(SelectedClubId))
        InputMode.SetWidgetToFocus(SelectedButton->TakeWidget());
    else if (ConfirmButton)
        InputMode.SetWidgetToFocus(ConfirmButton->TakeWidget());
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    PlayerController->SetInputMode(InputMode);
    FocusSelectedClub();
}

void USoccerClubSelectionWidget::CloseMenu()
{
    APlayerController* PlayerController = GetOwningPlayer();
    if (
        bAppliedGamePause && GetWorld() != nullptr &&
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

void USoccerClubSelectionWidget::BuildWidgetTree()
{
    if (WidgetTree == nullptr || WidgetTree->RootWidget != nullptr)
    {
        return;
    }

    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>();
    WidgetTree->RootWidget = Root;

    UBorder* DimBackground = WidgetTree->ConstructWidget<UBorder>();
    LiveBackdrop = DimBackground;
    DimBackground->SetBrushColor(FLinearColor(0.01f, 0.015f, 0.025f, 0.94f));
    UCanvasPanelSlot* BackgroundSlot = Root->AddChildToCanvas(DimBackground);
    BackgroundSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
    BackgroundSlot->SetOffsets(FMargin(0.0f));

    USizeBox* PanelSize = WidgetTree->ConstructWidget<USizeBox>();
    LiveMainSize = PanelSize;
    PanelSize->SetWidthOverride(1080.0f);
    PanelSize->SetHeightOverride(650.0f);
    UCanvasPanelSlot* CardSlot = Root->AddChildToCanvas(PanelSize);
    LivePanelCanvasSlot = CardSlot;
    CardSlot->SetAnchors(FAnchors(0.5f, 0.5f));
    CardSlot->SetAlignment(FVector2D(0.5f, 0.5f));
    CardSlot->SetPosition(FVector2D::ZeroVector);
    CardSlot->SetSize(FVector2D(1080.0f, 650.0f));

    UBorder* Card = WidgetTree->ConstructWidget<UBorder>();
    LiveMainPanel = Card;
    Card->SetPadding(FMargin(38.0f));
    Card->SetBrushColor(FLinearColor(0.05f, 0.075f, 0.11f, 1.0f));
    PanelSize->AddChild(Card);

    UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
    Card->AddChild(Column);

    Column->AddChildToVerticalBox(
        MakeClubSelectionText(WidgetTree, TEXT("SELECCIONAR CLUB RIVAL"), 30)
    );
    HumanClubText = MakeClubSelectionText(WidgetTree, TEXT("Tu club"), 20);
    UVerticalBoxSlot* HumanSlot = Column->AddChildToVerticalBox(HumanClubText);
    HumanSlot->SetPadding(FMargin(0.0f, 14.0f, 0.0f, 10.0f));

    USizeBox* GridViewportSize = WidgetTree->ConstructWidget<USizeBox>();
    LiveGridViewportSize = GridViewportSize;
    GridViewportSize->SetHeightOverride(430.0f);
    UScrollBox* GridScrollBox = WidgetTree->ConstructWidget<UScrollBox>();
    GridScrollBox->SetOrientation(Orient_Vertical);
    GridViewportSize->AddChild(GridScrollBox);
    ClubGrid = WidgetTree->ConstructWidget<UUniformGridPanel>();
    GridScrollBox->AddChild(ClubGrid);
    UVerticalBoxSlot* GridViewportSlot =
        Column->AddChildToVerticalBox(GridViewportSize);
    GridViewportSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

    OpponentClubText = MakeClubSelectionText(WidgetTree, TEXT("Rival"), 20);
    UVerticalBoxSlot* OpponentSlot =
        Column->AddChildToVerticalBox(OpponentClubText);
    OpponentSlot->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 2.0f));

    StatusText = MakeClubSelectionText(WidgetTree, TEXT("Elegí el rival."), 15);
    UVerticalBoxSlot* StatusSlot = Column->AddChildToVerticalBox(StatusText);
    StatusSlot->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 10.0f));

    // Both actions share the same pre-match footer; viewing squads never
    // starts a match or changes which opposing club is selected.
    UHorizontalBox* Footer = WidgetTree->ConstructWidget<UHorizontalBox>();
    ViewSquadsButton = MakeClubSelectionButton(
        WidgetTree, TEXT("VER PLANTELES"));
    ViewSquadsButton->OnClicked.AddDynamic(
        this, &USoccerClubSelectionWidget::HandleViewSquadsClicked);
    UHorizontalBoxSlot* ViewerSlot = Footer->AddChildToHorizontalBox(ViewSquadsButton);
    ViewerSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    ViewerSlot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));

    HistoryButton = MakeClubSelectionButton(WidgetTree, TEXT("HISTORIAL"));
    HistoryButton->OnClicked.AddDynamic(
        this, &USoccerClubSelectionWidget::HandleHistoryClicked);
    UHorizontalBoxSlot* HistorySlot = Footer->AddChildToHorizontalBox(HistoryButton);
    HistorySlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    HistorySlot->SetPadding(FMargin(6.0f, 0.0f, 6.0f, 0.0f));

    ConfirmButton = MakeClubSelectionButton(WidgetTree, TEXT("JUGAR PARTIDO"));
    ConfirmButton->OnClicked.AddDynamic(this, &USoccerClubSelectionWidget::HandleConfirmClicked);
    UHorizontalBoxSlot* PlaySlot = Footer->AddChildToHorizontalBox(ConfirmButton);
    PlaySlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    PlaySlot->SetPadding(FMargin(8.0f, 0.0f, 0.0f, 0.0f));
    Column->AddChildToVerticalBox(Footer);

    StyleEditor = NewObject<USoccerUIStyleEditorController>(this);
    TArray<FSoccerEditableStyleField> Fields;
    Fields.Add(FSoccerEditableStyleField::Number(TEXT("PanelWidth"), TEXT("Ancho del panel"), 1080.f, 700.f, 1700.f));
    Fields.Add(FSoccerEditableStyleField::Number(TEXT("PanelHeight"), TEXT("Alto del panel"), 650.f, 420.f, 950.f));
    Fields.Add(FSoccerEditableStyleField::Number(TEXT("PanelPadding"), TEXT("Margen interior"), 38.f, 5.f, 90.f));
    Fields.Add(FSoccerEditableStyleField::Number(TEXT("Columns"), TEXT("Columnas de clubes"), float(ClubGridColumnCount), 1.f, 8.f));
    Fields.Add(FSoccerEditableStyleField::Number(TEXT("GridGap"), TEXT("Separación de tarjetas"), 8.f, 0.f, 30.f));
    Fields.Add(FSoccerEditableStyleField::Number(TEXT("CardWidth"), TEXT("Ancho tarjeta"), ClubCardWidth, 120.f, 400.f));
    Fields.Add(FSoccerEditableStyleField::Number(TEXT("CardHeight"), TEXT("Alto tarjeta"), ClubCardHeight, 140.f, 420.f));
    Fields.Add(FSoccerEditableStyleField::Number(TEXT("CrestSize"), TEXT("Tamaño escudo"), ClubCrestSize, 48.f, 230.f));
    Fields.Add(FSoccerEditableStyleField::Number(TEXT("CardPadding"), TEXT("Margen tarjeta"), 10.f, 0.f, 30.f));
    Fields.Add(FSoccerEditableStyleField::Number(TEXT("FontScale"), TEXT("Escala de letra"), 1.f, .65f, 1.6f));
    Fields.Add(FSoccerEditableStyleField::Color(TEXT("Backdrop"), TEXT("Color de fondo"), TEXT("#030406F0")));
    Fields.Add(FSoccerEditableStyleField::Color(TEXT("PanelColor"), TEXT("Color de panel"), TEXT("#0D131C")));
    Fields.Add(FSoccerEditableStyleField::Color(TEXT("CardColor"), TEXT("Tarjeta normal"), TEXT("#121A26")));
    Fields.Add(FSoccerEditableStyleField::Color(TEXT("SelectedColor"), TEXT("Tarjeta seleccionada"), TEXT("#1A8545")));
    StyleEditor->Setup(WidgetTree, Root, TEXT("ClubSelectionStyle"), Fields);
    StyleEditor->OnApplied.AddDynamic(this, &USoccerClubSelectionWidget::HandleStyleApplied);
}

void USoccerClubSelectionWidget::HandleStyleApplied()
{
    ApplyLiveStyle();
}

void USoccerClubSelectionWidget::ApplyLiveStyle()
{
    if (!StyleEditor || !LiveMainPanel || !LiveMainSize) return;
    const FVector2D FitSize = FSoccerUITheme::FitPanelToViewport(this, FVector2D(
        StyleEditor->GetNumber(TEXT("PanelWidth"), 1080.f),
        StyleEditor->GetNumber(TEXT("PanelHeight"), 650.f)));
    LiveMainSize->SetWidthOverride(FitSize.X);
    LiveMainSize->SetHeightOverride(FitSize.Y);
    if (LivePanelCanvasSlot) LivePanelCanvasSlot->SetSize(FitSize);
    LiveMainPanel->SetPadding(FMargin(FMath::Min(
        StyleEditor->GetNumber(TEXT("PanelPadding"), 38.f), FitSize.Y * .065f)));
    if (LiveGridViewportSize)
        LiveGridViewportSize->SetHeightOverride(FMath::Clamp(FitSize.Y - 225.f, 130.f, 430.f));
    LiveMainPanel->SetBrushColor(StyleEditor->GetColor(TEXT("PanelColor"), FLinearColor(.05f, .075f, .11f)));
    if (LiveBackdrop) LiveBackdrop->SetBrushColor(StyleEditor->GetColor(TEXT("Backdrop"), FLinearColor(.01f, .015f, .025f, .94f)));
    RebuildClubGrid();
    RefreshTexts();
    StyleEditor->ApplyTextScale(LiveMainPanel, FSoccerUITheme::GetAdaptiveTextScale(
        this, StyleEditor->GetNumber(TEXT("FontScale"), 1.f)));
    FSoccerUITheme::ApplyButtonStates(LiveMainPanel);
}
