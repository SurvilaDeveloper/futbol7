#include "SoccerClubSelectionWidget.h"

#include "SoccerClubProfile.h"
#include "SoccerGameInstance.h"
#include "SoccerMatchManager.h"
#include "SoccerSquadCatalog.h"

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

void USoccerClubSelectionWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    BuildWidgetTree();
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
    InputMode.SetWidgetToFocus(TakeWidget());
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    PlayerController->SetInputMode(InputMode);

    if (ConfirmButton != nullptr)
    {
        ConfirmButton->SetUserFocus(PlayerController);
    }
}

FReply USoccerClubSelectionWidget::NativeOnPreviewKeyDown(
    const FGeometry& InGeometry,
    const FKeyEvent& InKeyEvent
)
{
    const FKey Key = InKeyEvent.GetKey();
    if (Key == EKeys::Left || Key == EKeys::A || Key == EKeys::Gamepad_DPad_Left)
    {
        CycleOpponent(-1);
        return FReply::Handled();
    }
    if (Key == EKeys::Right || Key == EKeys::D || Key == EKeys::Gamepad_DPad_Right)
    {
        CycleOpponent(1);
        return FReply::Handled();
    }
    if (Key == EKeys::Enter || Key == EKeys::Gamepad_FaceButton_Bottom)
    {
        HandleConfirmClicked();
        return FReply::Handled();
    }
    return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
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
                ? TEXT("Elegí el rival para este encuentro.")
                : TEXT("Creá y vinculá al menos dos planteles de clubes.")
        ));
    }
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
}

void USoccerClubSelectionWidget::HandlePreviousClicked()
{
    CycleOpponent(-1);
}

void USoccerClubSelectionWidget::HandleNextClicked()
{
    CycleOpponent(1);
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

    CloseMenu();
    OnSelectionConfirmed.Broadcast();
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
    DimBackground->SetBrushColor(FLinearColor(0.01f, 0.015f, 0.025f, 0.94f));
    UCanvasPanelSlot* BackgroundSlot = Root->AddChildToCanvas(DimBackground);
    BackgroundSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
    BackgroundSlot->SetOffsets(FMargin(0.0f));

    USizeBox* CardSize = WidgetTree->ConstructWidget<USizeBox>();
    CardSize->SetWidthOverride(720.0f);
    CardSize->SetHeightOverride(420.0f);
    UCanvasPanelSlot* CardSlot = Root->AddChildToCanvas(CardSize);
    CardSlot->SetAnchors(FAnchors(0.5f, 0.5f));
    CardSlot->SetAlignment(FVector2D(0.5f, 0.5f));
    CardSlot->SetPosition(FVector2D::ZeroVector);
    CardSlot->SetSize(FVector2D(720.0f, 420.0f));

    UBorder* Card = WidgetTree->ConstructWidget<UBorder>();
    Card->SetPadding(FMargin(38.0f));
    Card->SetBrushColor(FLinearColor(0.05f, 0.075f, 0.11f, 1.0f));
    CardSize->AddChild(Card);

    UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
    Card->AddChild(Column);

    Column->AddChildToVerticalBox(
        MakeClubSelectionText(WidgetTree, TEXT("CONFIGURAR ENCUENTRO"), 30)
    );
    HumanClubText = MakeClubSelectionText(WidgetTree, TEXT("Tu club"), 20);
    UVerticalBoxSlot* HumanSlot = Column->AddChildToVerticalBox(HumanClubText);
    HumanSlot->SetPadding(FMargin(0.0f, 28.0f, 0.0f, 22.0f));

    UHorizontalBox* Selector = WidgetTree->ConstructWidget<UHorizontalBox>();
    Column->AddChildToVerticalBox(Selector);
    UButton* PreviousButton = MakeClubSelectionButton(WidgetTree, TEXT("<"));
    PreviousButton->OnClicked.AddDynamic(this, &USoccerClubSelectionWidget::HandlePreviousClicked);
    Selector->AddChildToHorizontalBox(PreviousButton);

    USizeBox* OpponentSize = WidgetTree->ConstructWidget<USizeBox>();
    OpponentSize->SetWidthOverride(430.0f);
    OpponentClubText = MakeClubSelectionText(WidgetTree, TEXT("Rival"), 25);
    OpponentSize->AddChild(OpponentClubText);
    UHorizontalBoxSlot* OpponentSlot = Selector->AddChildToHorizontalBox(OpponentSize);
    OpponentSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

    UButton* NextButton = MakeClubSelectionButton(WidgetTree, TEXT(">"));
    NextButton->OnClicked.AddDynamic(this, &USoccerClubSelectionWidget::HandleNextClicked);
    Selector->AddChildToHorizontalBox(NextButton);

    StatusText = MakeClubSelectionText(WidgetTree, TEXT("Elegí el rival."), 15);
    UVerticalBoxSlot* StatusSlot = Column->AddChildToVerticalBox(StatusText);
    StatusSlot->SetPadding(FMargin(0.0f, 24.0f, 0.0f, 24.0f));

    ConfirmButton = MakeClubSelectionButton(WidgetTree, TEXT("CONTINUAR"));
    ConfirmButton->OnClicked.AddDynamic(this, &USoccerClubSelectionWidget::HandleConfirmClicked);
    Column->AddChildToVerticalBox(ConfirmButton);
}
