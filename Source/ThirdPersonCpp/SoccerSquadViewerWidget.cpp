#include "SoccerSquadViewerWidget.h"

#include "SoccerClubProfile.h"
#include "SoccerGameInstance.h"
#include "SoccerMatchManager.h"
#include "SoccerPlayerIdButton.h"
#include "SoccerPlayerProfile.h"
#include "SoccerSquadCatalog.h"
#include "SoccerSquadPreviewActor.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/ScaleBox.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/TextureRenderTarget2D.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"

// Dedicated namespace prevents anonymous-namespace helper collisions in UE4 Unity Builds.
namespace SoccerSquadViewerUI
{
    constexpr float PanelWidth = 1180.0f;
    constexpr float PanelHeight = 685.0f;

    UTextBlock* MakeText(UWidgetTree* Tree, const FString& Value, int32 Size,
                         bool bCentered = false)
    {
        UTextBlock* Text = Tree->ConstructWidget<UTextBlock>();
        Text->SetText(FText::FromString(Value));
        FSlateFontInfo Font = Text->Font;
        Font.Size = Size;
        Text->SetFont(Font);
        Text->SetColorAndOpacity(FSlateColor(FLinearColor::White));
        Text->SetJustification(bCentered ? ETextJustify::Center : ETextJustify::Left);
        Text->SetAutoWrapText(true);
        return Text;
    }

    UButton* MakeButton(UWidgetTree* Tree, const FString& Label, int32 FontSize = 17)
    {
        UButton* Button = Tree->ConstructWidget<UButton>();
        Button->AddChild(MakeText(Tree, Label, FontSize, true));
        return Button;
    }

    UHorizontalBoxSlot* AddButton(UHorizontalBox* Row, UButton* Button, float Space = 5.0f)
    {
        UHorizontalBoxSlot* Slot = Row->AddChildToHorizontalBox(Button);
        Slot->SetPadding(FMargin(Space, 0.0f));
        Slot->SetVerticalAlignment(VAlign_Center);
        return Slot;
    }

    UVerticalBoxSlot* AddVertical(UVerticalBox* Column, UWidget* Child, float Bottom = 5.0f)
    {
        UVerticalBoxSlot* Slot = Column->AddChildToVerticalBox(Child);
        Slot->SetPadding(FMargin(0, 0, 0, Bottom));
        return Slot;
    }
}

void USoccerSquadViewerWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    BuildWidgetTree();
}

void USoccerSquadViewerWidget::NativeDestruct()
{
    if (IsValid(PreviewActor))
    {
        PreviewActor->Destroy();
    }
    PreviewActor = nullptr;
    Super::NativeDestruct();
}

void USoccerSquadViewerWidget::InitializeForClub(
    USoccerGameInstance* InGameInstance, ASoccerMatchManager* InMatchManager,
    FName InitialClubId)
{
    SoccerGameInstance = InGameInstance;
    MatchManager = InMatchManager;
    AvailableClubIds.Reset();
    if (IsValid(SoccerGameInstance))
    {
        AvailableClubIds = SoccerGameInstance->GetAvailableClubIds();
    }
    AvailableClubIds.Sort([](const FName& Left, const FName& Right)
    {
        return Left.ToString() < Right.ToString();
    });
    SelectedClubIndex = AvailableClubIds.IndexOfByKey(InitialClubId);
    if (SelectedClubIndex == INDEX_NONE && AvailableClubIds.Num() > 0)
    {
        SelectedClubIndex = 0;
    }
    CreatePreviewIfNeeded();
    RebuildClub();
}

void USoccerSquadViewerWidget::ActivateMenu()
{
    APlayerController* PC = GetOwningPlayer();
    if (!PC)
    {
        return;
    }
    // The underlying pre-match club selection already owns the game pause.
    // Do not unpause it or modify its opponent selection from this viewer.
    PC->bShowMouseCursor = true;
    PC->bEnableClickEvents = true;
    FInputModeUIOnly InputMode;
    if (CloseButton)
    {
        InputMode.SetWidgetToFocus(CloseButton->TakeWidget());
    }
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    PC->SetInputMode(InputMode);
    if (CloseButton)
    {
        CloseButton->SetUserFocus(PC);
    }
    if (IsValid(PreviewActor))
    {
        PreviewActor->RefreshCapture();
    }
}

FReply USoccerSquadViewerWidget::NativeOnPreviewKeyDown(
    const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    const FKey Key = InKeyEvent.GetKey();
    if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right)
    {
        HandleClose();
        return FReply::Handled();
    }
    if (Key == EKeys::Left || Key == EKeys::Gamepad_DPad_Left)
    {
        CyclePlayer(-1);
        return FReply::Handled();
    }
    if (Key == EKeys::Right || Key == EKeys::Gamepad_DPad_Right)
    {
        CyclePlayer(1);
        return FReply::Handled();
    }
    if (Key == EKeys::PageUp)
    {
        CycleClub(-1);
        return FReply::Handled();
    }
    if (Key == EKeys::PageDown)
    {
        CycleClub(1);
        return FReply::Handled();
    }
    if (Key == EKeys::Q) { HandleRotateLeft(); return FReply::Handled(); }
    if (Key == EKeys::E) { HandleRotateRight(); return FReply::Handled(); }
    return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

void USoccerSquadViewerWidget::CreatePreviewIfNeeded()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }
    if (!PreviewTexture)
    {
        PreviewTexture = NewObject<UTextureRenderTarget2D>(this);
        PreviewTexture->ClearColor = FLinearColor(0.055f, 0.08f, 0.105f, 1.0f);
        PreviewTexture->InitAutoFormat(512, 768);
        PreviewTexture->UpdateResourceImmediate(true);
    }
    if (PreviewImage)
    {
        FSlateBrush ImageBrush;
        ImageBrush.SetResourceObject(PreviewTexture);
        ImageBrush.ImageSize = FVector2D(512.0f, 768.0f);
        PreviewImage->SetBrush(ImageBrush);
    }
    if (!IsValid(PreviewActor))
    {
        FActorSpawnParameters SpawnParams;
        SpawnParams.SpawnCollisionHandlingOverride =
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        // Deliberately isolated from the playable pitch and its collisions.
        PreviewActor = World->SpawnActor<ASoccerSquadPreviewActor>(
            ASoccerSquadPreviewActor::StaticClass(),
            FVector(250000.0f, 250000.0f, 15000.0f),
            FRotator::ZeroRotator, SpawnParams);
        if (IsValid(PreviewActor))
        {
            PreviewActor->SetRenderTarget(PreviewTexture);
        }
    }
}

FString USoccerSquadViewerWidget::GetClubDisplayName(FName ClubId) const
{
    USoccerSquadCatalog* Squad = IsValid(SoccerGameInstance)
        ? SoccerGameInstance->FindSquadCatalogByClubId(ClubId) : nullptr;
    if (IsValid(Squad) && IsValid(Squad->ClubProfile) &&
        !Squad->ClubProfile->DisplayName.IsEmpty())
    {
        return Squad->ClubProfile->DisplayName.ToString();
    }
    return ClubId.IsNone() ? TEXT("Sin club") : ClubId.ToString();
}

FString USoccerSquadViewerWidget::GetPlayerDisplayName(
    const USoccerPlayerProfile* Profile) const
{
    if (!IsValid(Profile))
    {
        return TEXT("Jugador desconocido");
    }
    return Profile->Identity.DisplayName.IsEmpty()
        ? Profile->Identity.PlayerId.ToString()
        : Profile->Identity.DisplayName.ToString();
}

FString USoccerSquadViewerWidget::GetNaturalPositionName(uint8 Position) const
{
    switch (static_cast<ESoccerPlayerNaturalPosition>(Position))
    {
    case ESoccerPlayerNaturalPosition::Goalkeeper: return TEXT("Arquero");
    case ESoccerPlayerNaturalPosition::LeftDefender: return TEXT("Defensor izquierdo");
    case ESoccerPlayerNaturalPosition::CentralDefender: return TEXT("Defensor central");
    case ESoccerPlayerNaturalPosition::RightDefender: return TEXT("Defensor derecho");
    case ESoccerPlayerNaturalPosition::LeftMidfielder: return TEXT("Volante izquierdo");
    case ESoccerPlayerNaturalPosition::CentralMidfielder: return TEXT("Volante central");
    case ESoccerPlayerNaturalPosition::RightMidfielder: return TEXT("Volante derecho");
    case ESoccerPlayerNaturalPosition::LeftForward: return TEXT("Delantero izquierdo");
    case ESoccerPlayerNaturalPosition::CenterForward: return TEXT("Delantero centro");
    case ESoccerPlayerNaturalPosition::RightForward: return TEXT("Delantero derecho");
    default: return TEXT("Sin posición");
    }
}

void USoccerSquadViewerWidget::RebuildClub()
{
    CurrentPlayers.Reset();
    PlayerBorders.Reset();
    SelectedPlayerIndex = INDEX_NONE;
    if (PlayerButtonColumn)
    {
        PlayerButtonColumn->ClearChildren();
    }
    if (!AvailableClubIds.IsValidIndex(SelectedClubIndex) ||
        !IsValid(SoccerGameInstance))
    {
        if (ClubNameText) ClubNameText->SetText(FText::FromString(TEXT("Sin planteles")));
        if (PreviewStatusText) PreviewStatusText->SetText(FText::FromString(TEXT("No se encontraron clubes.")));
        if (AttributeColumn) AttributeColumn->ClearChildren();
        return;
    }

    const FName ClubId = AvailableClubIds[SelectedClubIndex];
    USoccerSquadCatalog* Squad = SoccerGameInstance->FindSquadCatalogByClubId(ClubId);
    TSet<FName> UniqueIds;
    auto AddProfile = [this, &UniqueIds](USoccerPlayerProfile* Profile)
    {
        if (IsValid(Profile) && !Profile->Identity.PlayerId.IsNone() &&
            !UniqueIds.Contains(Profile->Identity.PlayerId))
        {
            UniqueIds.Add(Profile->Identity.PlayerId);
            CurrentPlayers.Add(Profile);
        }
    };

    // The human club may have a customized persistent squad; honor its IDs.
    if (ClubId == SoccerGameInstance->GetDefaultHumanClubId())
    {
        for (const FName PlayerId : SoccerGameInstance->GetSquadPlayerIds())
        {
            AddProfile(SoccerGameInstance->FindPlayerProfileById(PlayerId));
        }
    }
    // Other clubs come from their content catalogs. Also fall back to the
    // catalog for the human club when there is no saved/resolved squad.
    if ( (ClubId != SoccerGameInstance->GetDefaultHumanClubId() || CurrentPlayers.Num() == 0)
        && IsValid(Squad))
    {
        for (USoccerPlayerProfile* Profile : Squad->PlayerProfiles)
        {
            AddProfile(Profile);
        }
    }

    const FString ClubName = GetClubDisplayName(ClubId);
    const bool bOwnClub = ClubId == SoccerGameInstance->GetDefaultHumanClubId();
    if (ClubNameText)
    {
        ClubNameText->SetText(FText::FromString(FString::Printf(
            TEXT("%s%s"), *ClubName, bOwnClub ? TEXT("  ·  TU CLUB") : TEXT(""))));
    }
    if (ClubCounterText)
    {
        ClubCounterText->SetText(FText::FromString(FString::Printf(
            TEXT("Club %d / %d"), SelectedClubIndex + 1, AvailableClubIds.Num())));
    }
    RebuildPlayerButtons();
    if (CurrentPlayers.Num() > 0)
    {
        SelectPlayerIndex(0);
    }
    else
    {
        if (PlayerNameText) PlayerNameText->SetText(FText::FromString(TEXT("Sin jugadores")));
        if (PlayerCounterText) PlayerCounterText->SetText(FText::GetEmpty());
        if (IsValid(PreviewActor)) PreviewActor->ClearPlayer();
        if (AttributeColumn) AttributeColumn->ClearChildren();
        if (PreviewStatusText) PreviewStatusText->SetText(FText::FromString(TEXT("El plantel no tiene perfiles.")));
    }
}

void USoccerSquadViewerWidget::RebuildPlayerButtons()
{
    if (!PlayerButtonColumn || !WidgetTree)
    {
        return;
    }
    PlayerButtonColumn->ClearChildren();
    PlayerBorders.Reset();
    for (USoccerPlayerProfile* Profile : CurrentPlayers)
    {
        USoccerPlayerIdButton* Button = WidgetTree->ConstructWidget<USoccerPlayerIdButton>();
        Button->SetPlayerId(Profile->Identity.PlayerId);
        Button->OnPlayerIdClicked.AddDynamic(this, &USoccerSquadViewerWidget::HandlePlayerClicked);
        UBorder* Card = WidgetTree->ConstructWidget<UBorder>();
        Card->SetPadding(FMargin(8.0f, 7.0f));
        Card->SetBrushColor(FLinearColor(0.09f, 0.13f, 0.17f, 1.0f));
        Card->AddChild(SoccerSquadViewerUI::MakeText(WidgetTree, FString::Printf(TEXT("%02d  %s"),
            Profile->Identity.ShirtNumber, *GetPlayerDisplayName(Profile)), 15));
        Button->AddChild(Card);
        SoccerSquadViewerUI::AddVertical(PlayerButtonColumn, Button, 5.0f);
        PlayerBorders.Add(Profile->Identity.PlayerId, Card);
    }
}

void USoccerSquadViewerWidget::SelectPlayerIndex(int32 Index)
{
    if (!CurrentPlayers.IsValidIndex(Index))
    {
        return;
    }
    SelectedPlayerIndex = Index;
    for (const TPair<FName, UBorder*>& Pair : PlayerBorders)
    {
        if (IsValid(Pair.Value))
        {
            Pair.Value->SetBrushColor(
                CurrentPlayers[Index]->Identity.PlayerId == Pair.Key
                    ? FLinearColor(0.07f, 0.45f, 0.29f, 1.0f)
                    : FLinearColor(0.09f, 0.13f, 0.17f, 1.0f));
        }
    }
    RefreshSelectedPlayer();
}

void USoccerSquadViewerWidget::RefreshSelectedPlayer()
{
    if (!CurrentPlayers.IsValidIndex(SelectedPlayerIndex))
    {
        return;
    }
    USoccerPlayerProfile* Profile = CurrentPlayers[SelectedPlayerIndex];
    const FName ClubId = AvailableClubIds[SelectedClubIndex];
    USoccerSquadCatalog* Squad = SoccerGameInstance->FindSquadCatalogByClubId(ClubId);
    USoccerClubProfile* Club = IsValid(Squad) ? Squad->ClubProfile : nullptr;
    if (PlayerNameText)
    {
        PlayerNameText->SetText(FText::FromString(FString::Printf(
            TEXT("%02d · %s"), Profile->Identity.ShirtNumber,
            *GetPlayerDisplayName(Profile))));
    }
    if (PlayerCounterText)
    {
        PlayerCounterText->SetText(FText::FromString(FString::Printf(
            TEXT("Jugador %d / %d"), SelectedPlayerIndex + 1, CurrentPlayers.Num())));
    }
    CreatePreviewIfNeeded();
    const bool bHasPreview = IsValid(PreviewActor) &&
        PreviewActor->ShowPlayer(Profile, Club, MatchManager);
    if (PreviewStatusText)
    {
        PreviewStatusText->SetText(FText::FromString(
            bHasPreview ? TEXT("Girar y acercar con los botones de abajo; Q/E también gira")
                        : TEXT("No se pudo cargar la malla de este jugador.")));
    }

    if (!AttributeColumn)
    {
        return;
    }
    AttributeColumn->ClearChildren();
    FString Foot = TEXT("Derecho");
    if (Profile->Identity.PreferredFoot == ESoccerPreferredFoot::Left) Foot = TEXT("Izquierdo");
    else if (Profile->Identity.PreferredFoot == ESoccerPreferredFoot::Both) Foot = TEXT("Ambos");

    AddDetail(FString::Printf(TEXT("Pie hábil: %s  |  Altura: %.0f cm  |  Peso: %.0f kg"),
        *Foot, Profile->Identity.HeightCm, Profile->Identity.WeightKg));
    TArray<FSoccerPlayerPositionPreference> Positions = Profile->PositionPreferences;
    Positions.Sort([](const FSoccerPlayerPositionPreference& A,
                       const FSoccerPlayerPositionPreference& B)
    {
        return A.Familiarity > B.Familiarity;
    });
    bool bGoalkeeper = false;
    for (const FSoccerPlayerPositionPreference& Position : Positions)
    {
        AddDetail(FString::Printf(TEXT("%s (%d)"),
            *GetNaturalPositionName(static_cast<uint8>(Position.Position)),
            Position.Familiarity));
        bGoalkeeper |= Position.Position == ESoccerPlayerNaturalPosition::Goalkeeper &&
            Position.Familiarity >= 50;
    }
    if (Positions.Num() == 0)
    {
        AddDetail(TEXT("Posiciones sin definir"));
    }
    const FSoccerPlayerAttributes& Attributes = Profile->Attributes;
    const FSoccerPlayerPhysicalAttributes& Physical = Attributes.Physical;
    AddStatGroup(TEXT("FÍSICO"));
    AddStat(TEXT("Velocidad"), Physical.Pace);
    AddStat(TEXT("Aceleración"), Physical.Acceleration);
    AddStat(TEXT("Resistencia"), Physical.Stamina);
    AddStat(TEXT("Recuperación"), Physical.StaminaRecovery);
    AddStat(TEXT("Fuerza"), Physical.Strength);
    AddStat(TEXT("Agilidad"), Physical.Agility);
    AddStat(TEXT("Equilibrio"), Physical.Balance);

    const FSoccerPlayerTechnicalAttributes& Technical = Attributes.Technical;
    AddStatGroup(TEXT("TÉCNICO"));
    AddStat(TEXT("Control"), Technical.BallControl);
    AddStat(TEXT("Regate"), Technical.Dribbling);
    AddStat(TEXT("Precisión de pase"), Technical.PassingAccuracy);
    AddStat(TEXT("Precisión de remate"), Technical.ShootingAccuracy);
    AddStat(TEXT("Potencia de remate"), Technical.ShotPower);
    AddStat(TEXT("Entrada"), Technical.Tackling);
    AddStat(TEXT("Juego aéreo"), Technical.AerialAbility);

    const FSoccerPlayerTacticalAttributes& Tactical = Attributes.Tactical;
    AddStatGroup(TEXT("TÁCTICO"));
    AddStat(TEXT("Reacción defensiva"), Tactical.DefensiveReaction);
    AddStat(TEXT("Anticipación"), Tactical.Anticipation);
    AddStat(TEXT("Desmarque"), Tactical.OffBallPositioning);
    AddStat(TEXT("Posicionamiento defensivo"), Tactical.DefensivePositioning);
    AddStat(TEXT("Marcaje"), Tactical.Marking);
    AddStat(TEXT("Decisiones"), Tactical.DecisionMaking);
    AddStat(TEXT("Temple"), Tactical.Composure);
    if (bGoalkeeper)
    {
        const FSoccerGoalkeeperAttributes& GK = Attributes.Goalkeeper;
        AddStatGroup(TEXT("ARQUERO"));
        AddStat(TEXT("Reflejos"), GK.Reflexes);
        AddStat(TEXT("Posición"), GK.Positioning);
        AddStat(TEXT("Manos"), GK.Handling);
        AddStat(TEXT("Estirada"), GK.Diving);
        AddStat(TEXT("Distribución"), GK.Distribution);
    }
}

void USoccerSquadViewerWidget::AddDetail(const FString& Text)
{
    if (!AttributeColumn) return;
    UTextBlock* Line = SoccerSquadViewerUI::MakeText(WidgetTree, Text, 14);
    Line->SetColorAndOpacity(FSlateColor(FLinearColor(0.73f, 0.82f, 0.85f)));
    SoccerSquadViewerUI::AddVertical(AttributeColumn, Line, 4.0f);
}

void USoccerSquadViewerWidget::AddStatGroup(const FString& GroupName)
{
    if (!AttributeColumn) return;
    UTextBlock* Heading = SoccerSquadViewerUI::MakeText(WidgetTree, GroupName, 18);
    Heading->SetColorAndOpacity(FSlateColor(FLinearColor(0.44f, 0.88f, 0.67f)));
    UVerticalBoxSlot* HeadingSlot = SoccerSquadViewerUI::AddVertical(AttributeColumn, Heading, 6.0f);
    HeadingSlot->SetPadding(FMargin(0, 14.0f, 0, 6.0f));
}

void USoccerSquadViewerWidget::AddStat(const FString& Label, int32 Rating)
{
    if (!AttributeColumn) return;
    UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
    USizeBox* LabelSize = WidgetTree->ConstructWidget<USizeBox>();
    LabelSize->SetWidthOverride(181.0f);
    LabelSize->AddChild(SoccerSquadViewerUI::MakeText(WidgetTree, Label, 14));
    Row->AddChildToHorizontalBox(LabelSize);

    USizeBox* MeterSize = WidgetTree->ConstructWidget<USizeBox>();
    MeterSize->SetWidthOverride(105.0f);
    UProgressBar* Meter = WidgetTree->ConstructWidget<UProgressBar>();
    Meter->SetPercent(FMath::Clamp(Rating, 0, 100) / 100.0f);
    Meter->SetFillColorAndOpacity(FLinearColor(0.20f, 0.70f, 0.43f));
    MeterSize->AddChild(Meter);
    UHorizontalBoxSlot* MeterSlot = Row->AddChildToHorizontalBox(MeterSize);
    MeterSlot->SetPadding(FMargin(6.0f, 2.0f));
    MeterSlot->SetVerticalAlignment(VAlign_Center);

    UTextBlock* Number = SoccerSquadViewerUI::MakeText(WidgetTree, FString::FromInt(Rating), 14);
    UHorizontalBoxSlot* NumberSlot = Row->AddChildToHorizontalBox(Number);
    NumberSlot->SetPadding(FMargin(8.0f, 0.0f));
    SoccerSquadViewerUI::AddVertical(AttributeColumn, Row, 4.0f);
}

void USoccerSquadViewerWidget::CycleClub(int32 Direction)
{
    if (AvailableClubIds.Num() == 0) return;
    SelectedClubIndex = (SelectedClubIndex + Direction + AvailableClubIds.Num()) %
        AvailableClubIds.Num();
    RebuildClub();
}

void USoccerSquadViewerWidget::CyclePlayer(int32 Direction)
{
    if (CurrentPlayers.Num() == 0) return;
    SelectPlayerIndex((SelectedPlayerIndex + Direction + CurrentPlayers.Num()) %
        CurrentPlayers.Num());
}

void USoccerSquadViewerWidget::HandlePreviousClub() { CycleClub(-1); }
void USoccerSquadViewerWidget::HandleNextClub() { CycleClub(1); }
void USoccerSquadViewerWidget::HandlePreviousPlayer() { CyclePlayer(-1); }
void USoccerSquadViewerWidget::HandleNextPlayer() { CyclePlayer(1); }
void USoccerSquadViewerWidget::HandlePlayerClicked(FName PlayerId)
{
    const int32 Index = CurrentPlayers.IndexOfByPredicate(
        [PlayerId](const USoccerPlayerProfile* Profile)
        {
            return IsValid(Profile) && Profile->Identity.PlayerId == PlayerId;
        });
    SelectPlayerIndex(Index);
}
void USoccerSquadViewerWidget::HandleRotateLeft()
{
    if (IsValid(PreviewActor)) PreviewActor->RotateBy(-25.0f);
}
void USoccerSquadViewerWidget::HandleRotateRight()
{
    if (IsValid(PreviewActor)) PreviewActor->RotateBy(25.0f);
}
void USoccerSquadViewerWidget::HandleZoomIn()
{
    if (IsValid(PreviewActor)) PreviewActor->ZoomBy(-35.0f);
}
void USoccerSquadViewerWidget::HandleZoomOut()
{
    if (IsValid(PreviewActor)) PreviewActor->ZoomBy(35.0f);
}
void USoccerSquadViewerWidget::HandleClose()
{
    RemoveFromParent();
    OnViewerClosed.Broadcast();
}

void USoccerSquadViewerWidget::BuildWidgetTree()
{
    if (!WidgetTree || WidgetTree->RootWidget) return;
    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>();
    WidgetTree->RootWidget = Root;
    UBorder* Dim = WidgetTree->ConstructWidget<UBorder>();
    Dim->SetBrushColor(FLinearColor(0.015f, 0.02f, 0.03f, 0.97f));
    UCanvasPanelSlot* DimSlot = Root->AddChildToCanvas(Dim);
    DimSlot->SetAnchors(FAnchors(0, 0, 1, 1));
    DimSlot->SetOffsets(FMargin(0));

    UScaleBox* Scale = WidgetTree->ConstructWidget<UScaleBox>();
    Scale->SetStretch(EStretch::ScaleToFit);
    UCanvasPanelSlot* ScaleSlot = Root->AddChildToCanvas(Scale);
    ScaleSlot->SetAnchors(FAnchors(0, 0, 1, 1));
    ScaleSlot->SetOffsets(FMargin(12.0f));

    USizeBox* PanelSize = WidgetTree->ConstructWidget<USizeBox>();
    PanelSize->SetWidthOverride(SoccerSquadViewerUI::PanelWidth);
    PanelSize->SetHeightOverride(SoccerSquadViewerUI::PanelHeight);
    Scale->AddChild(PanelSize);
    UBorder* Panel = WidgetTree->ConstructWidget<UBorder>();
    Panel->SetPadding(FMargin(22));
    Panel->SetBrushColor(FLinearColor(0.05f, 0.075f, 0.11f));
    PanelSize->AddChild(Panel);
    UVerticalBox* Page = WidgetTree->ConstructWidget<UVerticalBox>();
    Panel->AddChild(Page);

    UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>();
    SoccerSquadViewerUI::AddVertical(Page, Header, 14.0f);
    UTextBlock* Title = SoccerSquadViewerUI::MakeText(WidgetTree, TEXT("PLANTELES Y JUGADORES"), 27);
    UHorizontalBoxSlot* TitleSlot = Header->AddChildToHorizontalBox(Title);
    TitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    CloseButton = SoccerSquadViewerUI::MakeButton(WidgetTree, TEXT("VOLVER"));
    CloseButton->OnClicked.AddDynamic(this, &USoccerSquadViewerWidget::HandleClose);
    SoccerSquadViewerUI::AddButton(Header, CloseButton);

    UHorizontalBox* ClubRow = WidgetTree->ConstructWidget<UHorizontalBox>();
    SoccerSquadViewerUI::AddVertical(Page, ClubRow, 12.0f);
    UButton* PrevClub = SoccerSquadViewerUI::MakeButton(WidgetTree, TEXT("< CLUB"));
    PrevClub->OnClicked.AddDynamic(this, &USoccerSquadViewerWidget::HandlePreviousClub);
    SoccerSquadViewerUI::AddButton(ClubRow, PrevClub);
    ClubNameText = SoccerSquadViewerUI::MakeText(WidgetTree, TEXT("Club"), 22, true);
    UHorizontalBoxSlot* NameSlot = ClubRow->AddChildToHorizontalBox(ClubNameText);
    NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    NameSlot->SetVerticalAlignment(VAlign_Center);
    ClubCounterText = SoccerSquadViewerUI::MakeText(WidgetTree, TEXT(""), 14, true);
    UButton* NextClub = SoccerSquadViewerUI::MakeButton(WidgetTree, TEXT("CLUB >"));
    NextClub->OnClicked.AddDynamic(this, &USoccerSquadViewerWidget::HandleNextClub);
    SoccerSquadViewerUI::AddButton(ClubRow, NextClub);
    // Counter below the row keeps the club name readable on narrow screens.
    SoccerSquadViewerUI::AddVertical(Page, ClubCounterText, 7.0f);

    UHorizontalBox* Columns = WidgetTree->ConstructWidget<UHorizontalBox>();
    UVerticalBoxSlot* ColumnsSlot = SoccerSquadViewerUI::AddVertical(Page, Columns, 6.0f);
    ColumnsSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

    USizeBox* ListSize = WidgetTree->ConstructWidget<USizeBox>();
    ListSize->SetWidthOverride(265.0f);
    UHorizontalBoxSlot* ListSlot = Columns->AddChildToHorizontalBox(ListSize);
    ListSlot->SetPadding(FMargin(0, 0, 14.0f, 0));
    UBorder* ListPanel = WidgetTree->ConstructWidget<UBorder>();
    ListPanel->SetPadding(FMargin(10));
    ListPanel->SetBrushColor(FLinearColor(0.07f, 0.11f, 0.15f));
    ListSize->AddChild(ListPanel);
    UVerticalBox* LeftColumn = WidgetTree->ConstructWidget<UVerticalBox>();
    ListPanel->AddChild(LeftColumn);
    SoccerSquadViewerUI::AddVertical(LeftColumn, SoccerSquadViewerUI::MakeText(WidgetTree, TEXT("JUGADORES"), 19), 8.0f);
    UScrollBox* PlayersScroll = WidgetTree->ConstructWidget<UScrollBox>();
    UVerticalBoxSlot* PlayersScrollSlot = LeftColumn->AddChildToVerticalBox(PlayersScroll);
    PlayersScrollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    PlayerButtonColumn = WidgetTree->ConstructWidget<UVerticalBox>();
    PlayersScroll->AddChild(PlayerButtonColumn);

    USizeBox* CenterSize = WidgetTree->ConstructWidget<USizeBox>();
    CenterSize->SetWidthOverride(408.0f);
    UHorizontalBoxSlot* CenterSlot = Columns->AddChildToHorizontalBox(CenterSize);
    CenterSlot->SetPadding(FMargin(0, 0, 14, 0));
    UBorder* CenterPanel = WidgetTree->ConstructWidget<UBorder>();
    CenterPanel->SetPadding(FMargin(8));
    CenterPanel->SetBrushColor(FLinearColor(0.045f, 0.08f, 0.11f));
    CenterSize->AddChild(CenterPanel);
    UVerticalBox* CenterColumn = WidgetTree->ConstructWidget<UVerticalBox>();
    CenterPanel->AddChild(CenterColumn);
    PlayerNameText = SoccerSquadViewerUI::MakeText(WidgetTree, TEXT("Jugador"), 18, true);
    SoccerSquadViewerUI::AddVertical(CenterColumn, PlayerNameText, 3.0f);
    PlayerCounterText = SoccerSquadViewerUI::MakeText(WidgetTree, TEXT(""), 13, true);
    SoccerSquadViewerUI::AddVertical(CenterColumn, PlayerCounterText, 4.0f);
    USizeBox* ImageSize = WidgetTree->ConstructWidget<USizeBox>();
    ImageSize->SetHeightOverride(370.0f);
    UVerticalBoxSlot* ImageSlot = CenterColumn->AddChildToVerticalBox(ImageSize);
    ImageSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    UScaleBox* ImageScale = WidgetTree->ConstructWidget<UScaleBox>();
    ImageScale->SetStretch(EStretch::ScaleToFit);
    ImageSize->AddChild(ImageScale);
    PreviewImage = WidgetTree->ConstructWidget<UImage>();
    ImageScale->AddChild(PreviewImage);
    PreviewStatusText = SoccerSquadViewerUI::MakeText(WidgetTree, TEXT("Vista tridimensional"), 12, true);
    SoccerSquadViewerUI::AddVertical(CenterColumn, PreviewStatusText, 7.0f);
    UHorizontalBox* ControlRow = WidgetTree->ConstructWidget<UHorizontalBox>();
    SoccerSquadViewerUI::AddVertical(CenterColumn, ControlRow, 6.0f);
    UButton* RotateL = SoccerSquadViewerUI::MakeButton(WidgetTree, TEXT("< GIRO"), 22);
    UButton* RotateR = SoccerSquadViewerUI::MakeButton(WidgetTree, TEXT("GIRO >"), 22);
    UButton* ZoomIn = SoccerSquadViewerUI::MakeButton(WidgetTree, TEXT("+"), 22);
    UButton* ZoomOut = SoccerSquadViewerUI::MakeButton(WidgetTree, TEXT("-"), 22);
    RotateL->OnClicked.AddDynamic(this, &USoccerSquadViewerWidget::HandleRotateLeft);
    RotateR->OnClicked.AddDynamic(this, &USoccerSquadViewerWidget::HandleRotateRight);
    ZoomIn->OnClicked.AddDynamic(this, &USoccerSquadViewerWidget::HandleZoomIn);
    ZoomOut->OnClicked.AddDynamic(this, &USoccerSquadViewerWidget::HandleZoomOut);
    SoccerSquadViewerUI::AddButton(ControlRow, RotateL);
    SoccerSquadViewerUI::AddButton(ControlRow, RotateR);
    SoccerSquadViewerUI::AddButton(ControlRow, ZoomIn);
    SoccerSquadViewerUI::AddButton(ControlRow, ZoomOut);
    UHorizontalBox* PlayerNav = WidgetTree->ConstructWidget<UHorizontalBox>();
    SoccerSquadViewerUI::AddVertical(CenterColumn, PlayerNav, 0.0f);
    UButton* PrevPlayer = SoccerSquadViewerUI::MakeButton(WidgetTree, TEXT("< ANTERIOR"), 14);
    UButton* NextPlayer = SoccerSquadViewerUI::MakeButton(WidgetTree, TEXT("SIGUIENTE >"), 14);
    PrevPlayer->OnClicked.AddDynamic(this, &USoccerSquadViewerWidget::HandlePreviousPlayer);
    NextPlayer->OnClicked.AddDynamic(this, &USoccerSquadViewerWidget::HandleNextPlayer);
    SoccerSquadViewerUI::AddButton(PlayerNav, PrevPlayer);
    SoccerSquadViewerUI::AddButton(PlayerNav, NextPlayer);

    USizeBox* StatsSize = WidgetTree->ConstructWidget<USizeBox>();
    StatsSize->SetWidthOverride(400.0f);
    UHorizontalBoxSlot* StatsSlot = Columns->AddChildToHorizontalBox(StatsSize);
    StatsSlot->SetPadding(FMargin(0));
    UBorder* StatsPanel = WidgetTree->ConstructWidget<UBorder>();
    StatsPanel->SetPadding(FMargin(12));
    StatsPanel->SetBrushColor(FLinearColor(0.07f, 0.11f, 0.15f));
    StatsSize->AddChild(StatsPanel);
    UVerticalBox* StatsContainer = WidgetTree->ConstructWidget<UVerticalBox>();
    StatsPanel->AddChild(StatsContainer);
    SoccerSquadViewerUI::AddVertical(StatsContainer, SoccerSquadViewerUI::MakeText(WidgetTree, TEXT("CARACTERÍSTICAS"), 19), 8.0f);
    UScrollBox* StatsScroll = WidgetTree->ConstructWidget<UScrollBox>();
    UVerticalBoxSlot* ScrollSlot = StatsContainer->AddChildToVerticalBox(StatsScroll);
    ScrollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    AttributeColumn = WidgetTree->ConstructWidget<UVerticalBox>();
    StatsScroll->AddChild(AttributeColumn);

    SoccerSquadViewerUI::AddVertical(Page, SoccerSquadViewerUI::MakeText(WidgetTree,
        TEXT("Consulta de planteles · No modifica la alineación ni el rival seleccionado"),
        13, true), 0.0f);
}
