#include "SoccerClubSelectionWidget.h"

#include "SoccerClubProfile.h"
#include "SoccerClubIdButton.h"
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
    if (ConfirmButton != nullptr)
    {
        InputMode.SetWidgetToFocus(ConfirmButton->TakeWidget());
    }
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
    ClubGrid->SetSlotPadding(FMargin(8.0f));
    const int32 SafeColumnCount = FMath::Max(1, ClubGridColumnCount);

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
        CardSize->SetWidthOverride(FMath::Max(120.0f, ClubCardWidth));
        CardSize->SetHeightOverride(FMath::Max(140.0f, ClubCardHeight));

        UBorder* CardBorder = WidgetTree->ConstructWidget<UBorder>();
        CardBorder->SetPadding(FMargin(10.0f));
        CardBorder->SetBrushColor(FLinearColor(0.07f, 0.10f, 0.15f, 1.0f));
        CardSize->AddChild(CardBorder);

        UVerticalBox* CardColumn = WidgetTree->ConstructWidget<UVerticalBox>();
        CardBorder->AddChild(CardColumn);

        USizeBox* CrestSize = WidgetTree->ConstructWidget<USizeBox>();
        CrestSize->SetWidthOverride(FMath::Max(64.0f, ClubCrestSize));
        CrestSize->SetHeightOverride(FMath::Max(64.0f, ClubCrestSize));

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
            ? FLinearColor(0.10f, 0.52f, 0.27f, 1.0f)
            : FLinearColor(0.07f, 0.10f, 0.15f, 1.0f)
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
                ? TEXT("Elegí el rival para este encuentro.")
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

    USizeBox* PanelSize = WidgetTree->ConstructWidget<USizeBox>();
    PanelSize->SetWidthOverride(1080.0f);
    PanelSize->SetHeightOverride(650.0f);
    UCanvasPanelSlot* CardSlot = Root->AddChildToCanvas(PanelSize);
    CardSlot->SetAnchors(FAnchors(0.5f, 0.5f));
    CardSlot->SetAlignment(FVector2D(0.5f, 0.5f));
    CardSlot->SetPosition(FVector2D::ZeroVector);
    CardSlot->SetSize(FVector2D(1080.0f, 650.0f));

    UBorder* Card = WidgetTree->ConstructWidget<UBorder>();
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

    ConfirmButton = MakeClubSelectionButton(WidgetTree, TEXT("JUGAR PARTIDO"));
    ConfirmButton->OnClicked.AddDynamic(this, &USoccerClubSelectionWidget::HandleConfirmClicked);
    Column->AddChildToVerticalBox(ConfirmButton);
}
