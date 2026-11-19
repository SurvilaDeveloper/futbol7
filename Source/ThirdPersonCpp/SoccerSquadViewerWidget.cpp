#include "SoccerSquadViewerWidget.h"
#include "SoccerUITheme.h"
#include "SoccerUIStyleEditorController.h"

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
#include "Components/ComboBoxString.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/ScaleBox.h"
#include "Components/ScrollBox.h"
#include "Components/Slider.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/PanelWidget.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
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
    LoadStyle();
    StyleSnapshots.Reset();
    StyleSnapshotIndex = INDEX_NONE;
    PushStyleSnapshot();
    BuildWidgetTree();
    ApplyViewerStyle();
}

void USoccerSquadViewerWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    UIAnimations.Tick(InDeltaTime);
    const FVector2D Viewport = FSoccerUITheme::GetLogicalViewportSize(this);
    if (Viewport.X > 1.f && !LastResponsiveViewport.Equals(Viewport, 1.f))
    {
        LastResponsiveViewport = Viewport;
        ApplyViewerStyle();
    }
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
    UButton* FirstPlayerButton = CurrentPlayers.IsValidIndex(SelectedPlayerIndex)
        ? PlayerButtonsById.FindRef(CurrentPlayers[SelectedPlayerIndex]->Identity.PlayerId) : nullptr;
    if (UWidget* InitialFocus = FirstPlayerButton ? static_cast<UWidget*>(FirstPlayerButton) : static_cast<UWidget*>(CloseButton))
        InputMode.SetWidgetToFocus(InitialFocus->TakeWidget());
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    PC->SetInputMode(InputMode);
    if (FirstPlayerButton)
        FSoccerUINavigation::FocusAndReveal(this, FirstPlayerButton);
    else if (CloseButton)
        FSoccerUINavigation::FocusAndReveal(this, CloseButton);
    if (IsValid(PreviewActor))
    {
        PreviewActor->RefreshCapture();
    }

    UIAnimations.StartEntrance(MainPanel);
}

FReply USoccerSquadViewerWidget::NativeOnPreviewKeyDown(
    const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
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

    if (StyleDrawer && StyleDrawer->GetVisibility() == ESlateVisibility::Visible &&
        FSoccerUINavigation::HasFocusedDescendant(StyleDrawer))
        return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
    if (StyleDrawer && StyleDrawer->GetVisibility() == ESlateVisibility::Visible)
    {
        if (GlobalThemeChoice && GlobalThemeChoice->HasKeyboardFocus())
            return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
        for (const TPair<FName, UEditableTextBox*>& StyleEntry : StyleEditors)
        {
            if (StyleEntry.Value && StyleEntry.Value->HasKeyboardFocus())
                return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
        }
    }
    if (FSoccerUINavigation::IsEditingText(WidgetTree))
        return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);

    if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right)
    {
        HandleClose();
        return FReply::Handled();
    }
    if (FSoccerUINavigation::Horizontal(Key) != 0 ||
        FSoccerUINavigation::Vertical(Key) != 0)
    {
        const int32 Delta = FSoccerUINavigation::Horizontal(Key) != 0
            ? FSoccerUINavigation::Horizontal(Key) : FSoccerUINavigation::Vertical(Key);
        CyclePlayer(Delta);
        return FReply::Handled();
    }
    if (Key == EKeys::PageUp || Key == EKeys::Gamepad_LeftShoulder)
    {
        CycleClub(-1);
        return FReply::Handled();
    }
    if (Key == EKeys::PageDown || Key == EKeys::Gamepad_RightShoulder)
    {
        CycleClub(1);
        return FReply::Handled();
    }
    if (Key == EKeys::Q) { HandleRotateLeft(); return FReply::Handled(); }
    if (Key == EKeys::E) { HandleRotateRight(); return FReply::Handled(); }
    return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

FReply USoccerSquadViewerWidget::NativeOnAnalogValueChanged(
    const FGeometry& InGeometry, const FAnalogInputEvent& InAnalogInputEvent)
{
    if (FSoccerUINavigation::IsEditingText(WidgetTree) ||
        (StyleDrawer && StyleDrawer->GetVisibility() == ESlateVisibility::Visible &&
         FSoccerUINavigation::HasFocusedDescendant(StyleDrawer)))
        return Super::NativeOnAnalogValueChanged(InGeometry, InAnalogInputEvent);
    int32 X = 0, Y = 0;
    if (FSoccerUINavigation::ReadStick(InAnalogInputEvent, AnalogNavigation, X, Y))
    {
        if (X) CyclePlayer(X);
        if (Y) CyclePlayer(Y);
        return FReply::Handled();
    }
    return Super::NativeOnAnalogValueChanged(InGeometry, InAnalogInputEvent);
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
    PlayerButtonsById.Reset();
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
    PlayerButtonsById.Reset();
    for (USoccerPlayerProfile* Profile : CurrentPlayers)
    {
        USoccerPlayerIdButton* Button = WidgetTree->ConstructWidget<USoccerPlayerIdButton>();
        Button->SetPlayerId(Profile->Identity.PlayerId);
        Button->OnPlayerIdClicked.AddDynamic(this, &USoccerSquadViewerWidget::HandlePlayerClicked);
        UBorder* Card = WidgetTree->ConstructWidget<UBorder>();
        Card->SetPadding(FMargin(LiveStyle.CardPadding, 7.0f));
        Card->SetBrushColor(EffectiveStyle.PlayerCard);
        Card->AddChild(SoccerSquadViewerUI::MakeText(WidgetTree, FString::Printf(TEXT("%02d  %s"),
            Profile->Identity.ShirtNumber, *GetPlayerDisplayName(Profile)), 15));
        Button->AddChild(Card);
        SoccerSquadViewerUI::AddVertical(PlayerButtonColumn, Button, 5.0f);
        PlayerBorders.Add(Profile->Identity.PlayerId, Card);
        PlayerButtonsById.Add(Profile->Identity.PlayerId, Button);
    }
    ApplyViewerStyle();
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
                    ? EffectiveStyle.SelectedCard
                    : EffectiveStyle.PlayerCard);
        }
    }
    RefreshSelectedPlayer();
    UIAnimations.StartSelection(PlayerBorders.FindRef(CurrentPlayers[Index]->Identity.PlayerId));
    UIAnimations.StartPage(StatsPanel);
    // Scroll the selected row into view even with very large club squads.
    FSoccerUINavigation::FocusAndReveal(this,
        PlayerButtonsById.FindRef(CurrentPlayers[Index]->Identity.PlayerId), IsInViewport());
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
    StatLabelBoxes.Reset();
    StatMeterBoxes.Reset();
    AccentTextNodes.Reset();
    SecondaryTextNodes.Reset();
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
    ApplyViewerStyle();
}

void USoccerSquadViewerWidget::AddDetail(const FString& Text)
{
    if (!AttributeColumn) return;
    UTextBlock* Line = SoccerSquadViewerUI::MakeText(WidgetTree, Text, 14);
    Line->SetColorAndOpacity(FSlateColor(EffectiveStyle.SecondaryText));
    SecondaryTextNodes.Add(Line);
    SoccerSquadViewerUI::AddVertical(AttributeColumn, Line, 4.0f);
}

void USoccerSquadViewerWidget::AddStatGroup(const FString& GroupName)
{
    if (!AttributeColumn) return;
    UTextBlock* Heading = SoccerSquadViewerUI::MakeText(WidgetTree, GroupName, 18);
    Heading->SetColorAndOpacity(FSlateColor(EffectiveStyle.Accent));
    AccentTextNodes.Add(Heading);
    UVerticalBoxSlot* HeadingSlot = SoccerSquadViewerUI::AddVertical(AttributeColumn, Heading, 6.0f);
    HeadingSlot->SetPadding(FMargin(0, 14.0f, 0, 6.0f));
}

void USoccerSquadViewerWidget::AddStat(const FString& Label, int32 Rating)
{
    if (!AttributeColumn) return;
    UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
    USizeBox* LabelSize = WidgetTree->ConstructWidget<USizeBox>();
    LabelSize->SetWidthOverride(LiveStyle.StatLabelWidth);
    StatLabelBoxes.Add(LabelSize);
    LabelSize->AddChild(SoccerSquadViewerUI::MakeText(WidgetTree, Label, 14));
    Row->AddChildToHorizontalBox(LabelSize);

    USizeBox* MeterSize = WidgetTree->ConstructWidget<USizeBox>();
    MeterSize->SetWidthOverride(LiveStyle.StatMeterWidth);
    StatMeterBoxes.Add(MeterSize);
    UProgressBar* Meter = WidgetTree->ConstructWidget<UProgressBar>();
    Meter->SetPercent(FMath::Clamp(Rating, 0, 100) / 100.0f);
    Meter->SetFillColorAndOpacity(EffectiveStyle.Meter);
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
    MainDim = Dim;
    Dim->SetBrushColor(LiveStyle.Backdrop);
    UCanvasPanelSlot* DimSlot = Root->AddChildToCanvas(Dim);
    DimSlot->SetAnchors(FAnchors(0, 0, 1, 1));
    DimSlot->SetOffsets(FMargin(0));

    UScaleBox* Scale = WidgetTree->ConstructWidget<UScaleBox>();
    Scale->SetStretch(EStretch::ScaleToFit);
    UCanvasPanelSlot* ScaleSlot = Root->AddChildToCanvas(Scale);
    ScaleSlot->SetAnchors(FAnchors(0, 0, 1, 1));
    ScaleSlot->SetOffsets(FMargin(12.0f));

    USizeBox* PanelSize = WidgetTree->ConstructWidget<USizeBox>();
    MainSizeBox = PanelSize;
    PanelSize->SetWidthOverride(LiveStyle.PanelWidth);
    PanelSize->SetHeightOverride(LiveStyle.PanelHeight);
    Scale->AddChild(PanelSize);
    UBorder* Panel = WidgetTree->ConstructWidget<UBorder>();
    MainPanel = Panel;
    Panel->SetPadding(FMargin(LiveStyle.PanelPadding));
    Panel->SetBrushColor(LiveStyle.Panel);
    PanelSize->AddChild(Panel);
    UVerticalBox* Page = WidgetTree->ConstructWidget<UVerticalBox>();
    Panel->AddChild(Page);

    UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>();
    SoccerSquadViewerUI::AddVertical(Page, Header, 14.0f);
    UTextBlock* Title = SoccerSquadViewerUI::MakeText(WidgetTree, TEXT("PLANTELES Y JUGADORES"), 27);
    UHorizontalBoxSlot* TitleSlot = Header->AddChildToHorizontalBox(Title);
    TitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    UButton* StyleButton = SoccerSquadViewerUI::MakeButton(WidgetTree, TEXT("ESTILOS"), 15);
    StyleButton->OnClicked.AddDynamic(this, &USoccerSquadViewerWidget::HandleToggleStyleEditor);
    SoccerSquadViewerUI::AddButton(Header, StyleButton);
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
    ListSizeBox = ListSize;
    ListSize->SetWidthOverride(LiveStyle.ListWidth);
    UHorizontalBoxSlot* ListSlot = Columns->AddChildToHorizontalBox(ListSize);
    ListColumnSlot = ListSlot;
    ListSlot->SetPadding(FMargin(0, 0, LiveStyle.ColumnGap, 0));
    ListPanel = WidgetTree->ConstructWidget<UBorder>();
    ListPanel->SetPadding(FMargin(LiveStyle.ListPadding));
    ListPanel->SetBrushColor(LiveStyle.ListPanel);
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
    CenterSizeBox = CenterSize;
    CenterSize->SetWidthOverride(LiveStyle.CenterWidth);
    UHorizontalBoxSlot* CenterSlot = Columns->AddChildToHorizontalBox(CenterSize);
    CenterColumnSlot = CenterSlot;
    CenterSlot->SetPadding(FMargin(0, 0, LiveStyle.ColumnGap, 0));
    CenterPanel = WidgetTree->ConstructWidget<UBorder>();
    CenterPanel->SetPadding(FMargin(8));
    CenterPanel->SetBrushColor(LiveStyle.CenterPanel);
    CenterSize->AddChild(CenterPanel);
    UVerticalBox* CenterColumn = WidgetTree->ConstructWidget<UVerticalBox>();
    CenterPanel->AddChild(CenterColumn);
    PlayerNameText = SoccerSquadViewerUI::MakeText(WidgetTree, TEXT("Jugador"), 18, true);
    SoccerSquadViewerUI::AddVertical(CenterColumn, PlayerNameText, 3.0f);
    PlayerCounterText = SoccerSquadViewerUI::MakeText(WidgetTree, TEXT(""), 13, true);
    SoccerSquadViewerUI::AddVertical(CenterColumn, PlayerCounterText, 4.0f);
    USizeBox* ImageSize = WidgetTree->ConstructWidget<USizeBox>();
    PreviewSizeBox = ImageSize;
    ImageSize->SetHeightOverride(LiveStyle.PreviewHeight);
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
    StatsSizeBox = StatsSize;
    StatsSize->SetWidthOverride(LiveStyle.StatsWidth);
    UHorizontalBoxSlot* StatsSlot = Columns->AddChildToHorizontalBox(StatsSize);
    StatsSlot->SetPadding(FMargin(0));
    StatsPanel = WidgetTree->ConstructWidget<UBorder>();
    StatsPanel->SetPadding(FMargin(LiveStyle.StatsPadding));
    StatsPanel->SetBrushColor(LiveStyle.StatsPanel);
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

    BuildStyleEditor(Root);
}

// These are the variables exposed in the temporary in-game CSS-like drawer.
// Each entry is reused for reading/writing the INI and building the controls.
#define SOCCER_SQUAD_STYLE_NUMBERS(X) \
    X(PanelWidth,       TEXT("Ancho total"),      650.f, 2000.f) \
    X(PanelHeight,      TEXT("Alto total"),       400.f, 1300.f) \
    X(ListWidth,        TEXT("Ancho lista"),      130.f, 700.f) \
    X(CenterWidth,      TEXT("Ancho visor 3D"),   200.f, 900.f) \
    X(StatsWidth,       TEXT("Ancho atributos"),  200.f, 800.f) \
    X(PreviewHeight,    TEXT("Alto malla 3D"),    160.f, 800.f) \
    X(PanelPadding,     TEXT("Padding general"),    0.f, 65.f) \
    X(ColumnGap,        TEXT("Separar columnas"),  0.f, 55.f) \
    X(ListPadding,      TEXT("Padding lista"),     0.f, 45.f) \
    X(StatsPadding,     TEXT("Padding atributos"), 0.f, 45.f) \
    X(CardPadding,      TEXT("Padding jugadores"), 0.f, 35.f) \
    X(FontScale,        TEXT("Escala fuentes"),    .65f, 1.65f) \
    X(StatLabelWidth,   TEXT("Ancho etiquetas"),  90.f, 350.f) \
    X(StatMeterWidth,   TEXT("Ancho barras"),     40.f, 280.f)

#define SOCCER_SQUAD_STYLE_COLORS(X) \
    X(Backdrop,       TEXT("Fondo externo")) \
    X(Panel,          TEXT("Panel principal")) \
    X(ListPanel,      TEXT("Panel lista")) \
    X(CenterPanel,    TEXT("Panel malla")) \
    X(StatsPanel,     TEXT("Panel atributos")) \
    X(PlayerCard,     TEXT("Jugador normal")) \
    X(SelectedCard,   TEXT("Jugador elegido")) \
    X(Accent,         TEXT("Titulos seccion")) \
    X(Text,           TEXT("Texto general")) \
    X(SecondaryText,  TEXT("Texto secundario")) \
    X(Meter,          TEXT("Barras atributos")) \
    X(ButtonTint,     TEXT("Tint botones"))

namespace SoccerSquadViewerStyleIO
{
    bool TryParseHexColor(const FString& Input, FLinearColor& Output)
    {
        FString Hex = Input.TrimStartAndEnd();
        Hex.RemoveFromStart(TEXT("#"));
        if (Hex.Len() != 6 && Hex.Len() != 8)
        {
            return false;
        }
        const FString Digits(TEXT("0123456789abcdefABCDEF"));
        for (int32 Index = 0; Index < Hex.Len(); ++Index)
        {
            if (!Digits.Contains(FString::Chr(Hex[Index])))
            {
                return false;
            }
        }
        Output = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("#") + Hex));
        return true;
    }

    FString ToHex(const FLinearColor& Color)
    {
        return TEXT("#") + Color.ToFColor(true).ToHex();
    }
}

FString USoccerSquadViewerWidget::StyleConfigPath() const
{
    return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SoccerUI"),
                           TEXT("SquadViewerStyle.ini"));
}

void USoccerSquadViewerWidget::LoadStyle()
{
    LiveStyle = FSoccerSquadViewerStyleSettings();
    LoadStyleAtPath(StyleConfigPath());
}

bool USoccerSquadViewerWidget::LoadStyleAtPath(const FString& Path)
{
    FString Ini;
    if (!FFileHelper::LoadFileToString(Ini, *Path))
    {
        return false; // Missing optional style/preset file.
    }
    TArray<FString> Lines;
    Ini.ParseIntoArrayLines(Lines, true);
    for (const FString& Line : Lines)
    {
        FString Key, Value;
        if (!Line.Split(TEXT("="), &Key, &Value))
        {
            continue;
        }
        Key.TrimStartAndEndInline();
        Value.TrimStartAndEndInline();
#define LOAD_STYLE_NUMBER(Name, Caption, MinVal, MaxVal) \
        if (Key == TEXT(#Name) && Value.IsNumeric()) \
        { \
            const float Parsed = FCString::Atof(*Value); \
            if (FMath::IsFinite(Parsed)) LiveStyle.Name = FMath::Clamp(Parsed, MinVal, MaxVal); \
        }
        SOCCER_SQUAD_STYLE_NUMBERS(LOAD_STYLE_NUMBER)
#undef LOAD_STYLE_NUMBER
#define LOAD_STYLE_COLOR(Name, Caption) \
        if (Key == TEXT(#Name)) \
        { \
            FLinearColor Parsed; \
            if (SoccerSquadViewerStyleIO::TryParseHexColor(Value, Parsed)) LiveStyle.Name = Parsed; \
        }
        SOCCER_SQUAD_STYLE_COLORS(LOAD_STYLE_COLOR)
#undef LOAD_STYLE_COLOR
    }
    return true;
}

bool USoccerSquadViewerWidget::SaveStyle() const
{
    return SaveStyleAtPath(StyleConfigPath());
}

bool USoccerSquadViewerWidget::SaveStyleAtPath(const FString& Path) const
{
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
    FString Ini = TEXT("; Temporary style configuration - Planteles y jugadores\n");
    Ini += TEXT("; Editable from the ESTILOS drawer in Unreal Engine 4.27\n");
    Ini += TEXT("[SquadViewerStyle]\n");
#define SAVE_STYLE_NUMBER(Name, Caption, MinVal, MaxVal) \
    Ini += FString::Printf(TEXT("%s=%s\n"), TEXT(#Name), *FString::SanitizeFloat(LiveStyle.Name));
    SOCCER_SQUAD_STYLE_NUMBERS(SAVE_STYLE_NUMBER)
#undef SAVE_STYLE_NUMBER
#define SAVE_STYLE_COLOR(Name, Caption) \
    Ini += FString::Printf(TEXT("%s=%s\n"), TEXT(#Name), \
                           *SoccerSquadViewerStyleIO::ToHex(LiveStyle.Name));
    SOCCER_SQUAD_STYLE_COLORS(SAVE_STYLE_COLOR)
#undef SAVE_STYLE_COLOR
    return FFileHelper::SaveStringToFile(Ini, *Path);
}

FString USoccerSquadViewerWidget::StylePresetPath() const
{
    const FString Selected = StylePresetChoice ? StylePresetChoice->GetSelectedOption() : TEXT("A");
    const FString SafeName = Selected == TEXT("B") ? TEXT("B") : Selected == TEXT("C") ? TEXT("C") : TEXT("A");
    return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SoccerUI"), TEXT("Presets"),
        TEXT("SquadViewerStyle_Preset_") + SafeName + TEXT(".ini"));
}

void USoccerSquadViewerWidget::PushStyleSnapshot()
{
    // Compare serialised numeric and color values, not struct padding bytes.
    if (StyleSnapshots.IsValidIndex(StyleSnapshotIndex))
    {
        const FSoccerSquadViewerStyleSettings& Previous = StyleSnapshots[StyleSnapshotIndex];
        bool bDifferent = false;
#define TEST_STYLE_NUMBER(Name, Caption, MinVal, MaxVal) \
        bDifferent |= !FMath::IsNearlyEqual(Previous.Name, LiveStyle.Name, .0001f);
        SOCCER_SQUAD_STYLE_NUMBERS(TEST_STYLE_NUMBER)
#undef TEST_STYLE_NUMBER
#define TEST_STYLE_COLOR(Name, Caption) \
        bDifferent |= !Previous.Name.Equals(LiveStyle.Name, .0001f);
        SOCCER_SQUAD_STYLE_COLORS(TEST_STYLE_COLOR)
#undef TEST_STYLE_COLOR
        if (!bDifferent) return;
    }
    if (StyleSnapshotIndex + 1 < StyleSnapshots.Num())
        StyleSnapshots.RemoveAt(StyleSnapshotIndex + 1, StyleSnapshots.Num() - StyleSnapshotIndex - 1);
    StyleSnapshots.Add(LiveStyle);
    if (StyleSnapshots.Num() > 30) StyleSnapshots.RemoveAt(0);
    StyleSnapshotIndex = StyleSnapshots.Num() - 1;
}

void USoccerSquadViewerWidget::ApplyStyleSnapshot(int32 Index)
{
    if (!StyleSnapshots.IsValidIndex(Index)) return;
    StyleSnapshotIndex = Index;
    LiveStyle = StyleSnapshots[Index];
    SyncStyleEditors();
    ApplyViewerStyle();
}

void USoccerSquadViewerWidget::HandleStyleSliderInput(FName Key, float Value)
{
    if (UEditableTextBox* const* Input = StyleEditors.Find(Key))
    {
#define VIEWER_SLIDER_NUMBER(Name, Caption, MinVal, MaxVal) \
        if (Key == FName(TEXT(#Name))) \
        { \
            (*Input)->SetText(FText::FromString(FString::SanitizeFloat( \
                FMath::Clamp(Value, MinVal, MaxVal)))); \
            return; \
        }
        SOCCER_SQUAD_STYLE_NUMBERS(VIEWER_SLIDER_NUMBER)
#undef VIEWER_SLIDER_NUMBER
    }
}

void USoccerSquadViewerWidget::HandleStylePaletteInput(FName Key, const FString& Preset)
{
    const FString Hex = Preset == TEXT("Blanco") ? TEXT("#FFFFFFFF")
        : Preset == TEXT("Negro") ? TEXT("#000000FF")
        : Preset == TEXT("Verde estadio") ? TEXT("#21A669FF")
        : Preset == TEXT("Azul profesional") ? TEXT("#237FCAFF")
        : Preset == TEXT("Grafito") ? TEXT("#333C45FF")
        : Preset == TEXT("Transparente") ? TEXT("#00000000") : FString();
    if (!Hex.IsEmpty())
        if (UEditableTextBox* const* Input = StyleEditors.Find(Key))
            if (IsValid(*Input)) (*Input)->SetText(FText::FromString(Hex));
}

void USoccerSquadViewerWidget::HandleStyleHexInput(FName Key, const FText& Value)
{
    FLinearColor Parsed;
    if (!SoccerSquadViewerStyleIO::TryParseHexColor(Value.ToString(), Parsed)) return;
    if (UBorder* const* Sample = StyleSwatches.Find(Key))
        if (IsValid(*Sample)) (*Sample)->SetBrushColor(Parsed);
}

void USoccerSquadViewerWidget::AddStyleField(
    UVerticalBox* Parent, const TCHAR* Key, const TCHAR* Caption,
    const FString& CurrentValue)
{
    if (!Parent || !WidgetTree) return;
    UVerticalBox* Entry = WidgetTree->ConstructWidget<UVerticalBox>();
    SoccerSquadViewerUI::AddVertical(Parent, Entry, 6.f);
    SoccerSquadViewerUI::AddVertical(Entry,
        SoccerSquadViewerUI::MakeText(WidgetTree, Caption, 13), 2.f);
    UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
    SoccerSquadViewerUI::AddVertical(Entry, Row, 1.f);
    UEditableTextBox* Edit = WidgetTree->ConstructWidget<UEditableTextBox>();
    Edit->SetText(FText::FromString(CurrentValue));
    Row->AddChildToHorizontalBox(Edit);
    StyleEditors.Add(FName(Key), Edit);

    USoccerUIStyleFieldInputBridge* Binding = NewObject<USoccerUIStyleFieldInputBridge>(this);
    Binding->InitializeViewer(this, FName(Key));
    StyleInputBindings.Add(Binding);
    const FString Name(Key);
    bool bIsNumber = false;
#define ADD_VIEWER_SLIDER(NameField, CaptionText, MinVal, MaxVal) \
    if (Name == TEXT(#NameField)) \
    { \
        USlider* RangeSlider = WidgetTree->ConstructWidget<USlider>(); \
        RangeSlider->SetMinValue(MinVal); \
        RangeSlider->SetMaxValue(MaxVal); \
        RangeSlider->SetValue(FCString::Atof(*CurrentValue)); \
        RangeSlider->OnValueChanged.AddDynamic(Binding, \
            &USoccerUIStyleFieldInputBridge::HandleSliderChanged); \
        Entry->AddChildToVerticalBox(RangeSlider); \
        StyleSliders.Add(FName(Key), RangeSlider); \
        bIsNumber = true; \
    }
    SOCCER_SQUAD_STYLE_NUMBERS(ADD_VIEWER_SLIDER)
#undef ADD_VIEWER_SLIDER
    if (!bIsNumber)
    {
        FLinearColor Parsed;
        UBorder* Swatch = WidgetTree->ConstructWidget<UBorder>();
        Swatch->SetPadding(FMargin(12.f, 9.f));
        if (SoccerSquadViewerStyleIO::TryParseHexColor(CurrentValue, Parsed))
            Swatch->SetBrushColor(Parsed);
        Row->AddChildToHorizontalBox(Swatch);
        StyleSwatches.Add(FName(Key), Swatch);
        Edit->OnTextChanged.AddDynamic(Binding, &USoccerUIStyleFieldInputBridge::HandleHexChanged);
        UComboBoxString* Palette = WidgetTree->ConstructWidget<UComboBoxString>();
        for (const TCHAR* Option : { TEXT("Personalizado"), TEXT("Blanco"), TEXT("Negro"),
            TEXT("Verde estadio"), TEXT("Azul profesional"), TEXT("Grafito"), TEXT("Transparente") })
            Palette->AddOption(Option);
        Palette->SetSelectedOption(TEXT("Personalizado"));
        Palette->OnSelectionChanged.AddDynamic(Binding,
            &USoccerUIStyleFieldInputBridge::HandlePaletteChanged);
        SoccerSquadViewerUI::AddVertical(Entry, Palette, 2.f);
    }
}

void USoccerSquadViewerWidget::BuildStyleEditor(UCanvasPanel* Root)
{
    if (!Root || !WidgetTree) return;
    StyleDrawer = WidgetTree->ConstructWidget<UBorder>();
    StyleDrawer->SetPadding(FMargin(14.f));
    StyleDrawer->SetBrushColor(FLinearColor(.07f, .09f, .13f, .99f));
    StyleDrawer->SetVisibility(ESlateVisibility::Collapsed);
    UCanvasPanelSlot* DrawerSlot = Root->AddChildToCanvas(StyleDrawer);
    DrawerSlot->SetAnchors(FAnchors(1.f, 0.f, 1.f, 1.f));
    DrawerSlot->SetAlignment(FVector2D(1.f, 0.f));
    DrawerSlot->SetOffsets(FMargin(-10.f, 10.f, 390.f, 10.f));
    DrawerSlot->SetZOrder(100);

    UVerticalBox* DrawerBody = WidgetTree->ConstructWidget<UVerticalBox>();
    StyleDrawer->AddChild(DrawerBody);
    SoccerSquadViewerUI::AddVertical(DrawerBody,
        SoccerSquadViewerUI::MakeText(WidgetTree, TEXT("EDITOR DE ESTILOS (TEMPORAL)"), 18), 7.f);
    SoccerSquadViewerUI::AddVertical(DrawerBody,
        SoccerSquadViewerUI::MakeText(WidgetTree,
            TEXT("Editar valores y pulsar APLICAR. No es necesario recompilar."), 12), 6.f);

    SoccerSquadViewerUI::AddVertical(DrawerBody,
        SoccerSquadViewerUI::MakeText(WidgetTree, TEXT("TEMA GLOBAL"), 13), 4.f);
    GlobalThemeChoice = WidgetTree->ConstructWidget<UComboBoxString>();
    GlobalThemeChoice->AddOption(TEXT("Original"));
    GlobalThemeChoice->AddOption(TEXT("Verde estadio"));
    GlobalThemeChoice->AddOption(TEXT("Azul profesional"));
    GlobalThemeChoice->AddOption(TEXT("Grafito"));
    GlobalThemeChoice->SetSelectedOption(FSoccerUITheme::GetActiveTheme());
    SoccerSquadViewerUI::AddVertical(DrawerBody, GlobalThemeChoice, 5.f);
    UButton* ApplyGlobalThemeButton = SoccerSquadViewerUI::MakeButton(WidgetTree, TEXT("APLICAR TEMA GLOBAL"), 13);
    ApplyGlobalThemeButton->OnClicked.AddDynamic(this, &USoccerSquadViewerWidget::HandleApplyGlobalTheme);
    SoccerSquadViewerUI::AddVertical(DrawerBody, ApplyGlobalThemeButton, 7.f);

    UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>();
    UVerticalBoxSlot* ScrollSlot = DrawerBody->AddChildToVerticalBox(Scroll);
    ScrollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    UVerticalBox* Fields = WidgetTree->ConstructWidget<UVerticalBox>();
    Scroll->AddChild(Fields);
    SoccerSquadViewerUI::AddVertical(Fields,
        SoccerSquadViewerUI::MakeText(WidgetTree, TEXT("DIMENSIONES Y ESPACIOS"), 15), 8.f);
#define EDIT_STYLE_NUMBER(Name, Caption, MinVal, MaxVal) \
    AddStyleField(Fields, TEXT(#Name), Caption, FString::SanitizeFloat(LiveStyle.Name));
    SOCCER_SQUAD_STYLE_NUMBERS(EDIT_STYLE_NUMBER)
#undef EDIT_STYLE_NUMBER
    SoccerSquadViewerUI::AddVertical(Fields,
        SoccerSquadViewerUI::MakeText(WidgetTree, TEXT("COLORES HEX (#RRGGBBAA)"), 15), 8.f);
#define EDIT_STYLE_COLOR(Name, Caption) \
    AddStyleField(Fields, TEXT(#Name), Caption, SoccerSquadViewerStyleIO::ToHex(LiveStyle.Name));
    SOCCER_SQUAD_STYLE_COLORS(EDIT_STYLE_COLOR)
#undef EDIT_STYLE_COLOR

    SoccerSquadViewerUI::AddVertical(DrawerBody,
        SoccerSquadViewerUI::MakeText(WidgetTree, TEXT("PRESETS A / B / C"), 12), 3.f);
    StylePresetChoice = WidgetTree->ConstructWidget<UComboBoxString>();
    StylePresetChoice->AddOption(TEXT("A"));
    StylePresetChoice->AddOption(TEXT("B"));
    StylePresetChoice->AddOption(TEXT("C"));
    StylePresetChoice->SetSelectedOption(TEXT("A"));
    SoccerSquadViewerUI::AddVertical(DrawerBody, StylePresetChoice, 3.f);
    UHorizontalBox* PresetButtons = WidgetTree->ConstructWidget<UHorizontalBox>();
    SoccerSquadViewerUI::AddVertical(DrawerBody, PresetButtons, 5.f);
    UButton* SavePresetButton = SoccerSquadViewerUI::MakeButton(WidgetTree, TEXT("GUARDAR PRESET"), 12);
    UButton* LoadPresetButton = SoccerSquadViewerUI::MakeButton(WidgetTree, TEXT("CARGAR PRESET"), 12);
    SavePresetButton->OnClicked.AddDynamic(this, &USoccerSquadViewerWidget::HandleStyleSavePreset);
    LoadPresetButton->OnClicked.AddDynamic(this, &USoccerSquadViewerWidget::HandleStyleLoadPreset);
    SoccerSquadViewerUI::AddButton(PresetButtons, SavePresetButton, 3.f);
    SoccerSquadViewerUI::AddButton(PresetButtons, LoadPresetButton, 3.f);

    StyleStatus = SoccerSquadViewerUI::MakeText(WidgetTree,
        TEXT("Configuracion solo para este visor"), 12, true);
    SoccerSquadViewerUI::AddVertical(DrawerBody, StyleStatus, 9.f);
    UHorizontalBox* Actions = WidgetTree->ConstructWidget<UHorizontalBox>();
    SoccerSquadViewerUI::AddVertical(DrawerBody, Actions, 6.f);
    UButton* Apply = SoccerSquadViewerUI::MakeButton(WidgetTree, TEXT("APLICAR"), 14);
    UButton* Save = SoccerSquadViewerUI::MakeButton(WidgetTree, TEXT("GUARDAR"), 14);
    Apply->OnClicked.AddDynamic(this, &USoccerSquadViewerWidget::HandleApplyStyle);
    Save->OnClicked.AddDynamic(this, &USoccerSquadViewerWidget::HandleSaveStyle);
    SoccerSquadViewerUI::AddButton(Actions, Apply, 5.f);
    SoccerSquadViewerUI::AddButton(Actions, Save, 5.f);
    UHorizontalBox* Actions2 = WidgetTree->ConstructWidget<UHorizontalBox>();
    SoccerSquadViewerUI::AddVertical(DrawerBody, Actions2, 0.f);
    UButton* Reset = SoccerSquadViewerUI::MakeButton(WidgetTree, TEXT("RESTAURAR"), 14);
    UButton* Close = SoccerSquadViewerUI::MakeButton(WidgetTree, TEXT("CERRAR"), 14);
    Reset->OnClicked.AddDynamic(this, &USoccerSquadViewerWidget::HandleResetStyle);
    Close->OnClicked.AddDynamic(this, &USoccerSquadViewerWidget::HandleToggleStyleEditor);
    SoccerSquadViewerUI::AddButton(Actions2, Reset, 5.f);
    SoccerSquadViewerUI::AddButton(Actions2, Close, 5.f);
    UHorizontalBox* Advanced = WidgetTree->ConstructWidget<UHorizontalBox>();
    SoccerSquadViewerUI::AddVertical(DrawerBody, Advanced, 4.f);
    UButton* Undo = SoccerSquadViewerUI::MakeButton(WidgetTree, TEXT("DESHACER"), 12);
    UButton* Redo = SoccerSquadViewerUI::MakeButton(WidgetTree, TEXT("REHACER"), 12);
    UButton* Export = SoccerSquadViewerUI::MakeButton(WidgetTree, TEXT("EXPORTAR TODO"), 12);
    Undo->OnClicked.AddDynamic(this, &USoccerSquadViewerWidget::HandleStyleUndo);
    Redo->OnClicked.AddDynamic(this, &USoccerSquadViewerWidget::HandleStyleRedo);
    Export->OnClicked.AddDynamic(this, &USoccerSquadViewerWidget::HandleStyleExport);
    SoccerSquadViewerUI::AddButton(Advanced, Undo, 2.f);
    SoccerSquadViewerUI::AddButton(Advanced, Redo, 2.f);
    SoccerSquadViewerUI::AddButton(Advanced, Export, 2.f);
}

void USoccerSquadViewerWidget::SyncStyleEditors()
{
#define SYNC_STYLE_NUMBER(Name, Caption, MinVal, MaxVal) \
    if (UEditableTextBox* const* Edit = StyleEditors.Find(FName(TEXT(#Name)))) \
        if (IsValid(*Edit)) (*Edit)->SetText(FText::FromString(FString::SanitizeFloat(LiveStyle.Name)));
    SOCCER_SQUAD_STYLE_NUMBERS(SYNC_STYLE_NUMBER)
#undef SYNC_STYLE_NUMBER
#define SYNC_VIEWER_SLIDER(Name, Caption, MinVal, MaxVal) \
    if (USlider* const* Range = StyleSliders.Find(FName(TEXT(#Name)))) \
        if (IsValid(*Range)) (*Range)->SetValue(LiveStyle.Name);
    SOCCER_SQUAD_STYLE_NUMBERS(SYNC_VIEWER_SLIDER)
#undef SYNC_VIEWER_SLIDER
#define SYNC_STYLE_COLOR(Name, Caption) \
    if (UEditableTextBox* const* Edit = StyleEditors.Find(FName(TEXT(#Name)))) \
        if (IsValid(*Edit)) (*Edit)->SetText(FText::FromString(SoccerSquadViewerStyleIO::ToHex(LiveStyle.Name)));
    SOCCER_SQUAD_STYLE_COLORS(SYNC_STYLE_COLOR)
#undef SYNC_STYLE_COLOR
#define SYNC_VIEWER_SWATCH(Name, Caption) \
    if (UBorder* const* Sample = StyleSwatches.Find(FName(TEXT(#Name)))) \
        if (IsValid(*Sample)) (*Sample)->SetBrushColor(LiveStyle.Name);
    SOCCER_SQUAD_STYLE_COLORS(SYNC_VIEWER_SWATCH)
#undef SYNC_VIEWER_SWATCH
}

void USoccerSquadViewerWidget::HandleToggleStyleEditor()
{
    if (!StyleDrawer) return;
    const bool bShow = StyleDrawer->GetVisibility() != ESlateVisibility::Visible;
    StyleDrawer->SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (bShow)
    {
        SyncStyleEditors();
        if (GlobalThemeChoice) GlobalThemeChoice->SetSelectedOption(FSoccerUITheme::GetActiveTheme());
    }
}

void USoccerSquadViewerWidget::HandleApplyGlobalTheme()
{
    if (!GlobalThemeChoice) return;
    const bool bSaved = FSoccerUITheme::SetActiveTheme(GlobalThemeChoice->GetSelectedOption());
    if (bSaved) ApplyViewerStyle();
    if (StyleStatus)
    {
        StyleStatus->SetText(FText::FromString(bSaved
            ? TEXT("Tema global guardado; el resto de menús lo tomará al abrirse.")
            : TEXT("No se pudo guardar el tema global.")));
    }
}

void USoccerSquadViewerWidget::HandleApplyStyle()
{
    bool bInvalid = false;
#define APPLY_STYLE_NUMBER(Name, Caption, MinVal, MaxVal) \
    if (UEditableTextBox* const* Edit = StyleEditors.Find(FName(TEXT(#Name)))) \
    { \
        const FString Value = (*Edit)->GetText().ToString().TrimStartAndEnd(); \
        if (Value.IsNumeric()) \
        { \
            const float Parsed = FCString::Atof(*Value); \
            if (FMath::IsFinite(Parsed)) LiveStyle.Name = FMath::Clamp(Parsed, MinVal, MaxVal); \
            else bInvalid = true; \
        } \
        else bInvalid = true; \
    }
    SOCCER_SQUAD_STYLE_NUMBERS(APPLY_STYLE_NUMBER)
#undef APPLY_STYLE_NUMBER
#define APPLY_STYLE_COLOR(Name, Caption) \
    if (UEditableTextBox* const* Edit = StyleEditors.Find(FName(TEXT(#Name)))) \
    { \
        FLinearColor Parsed; \
        if (SoccerSquadViewerStyleIO::TryParseHexColor((*Edit)->GetText().ToString(), Parsed)) \
            LiveStyle.Name = Parsed; \
        else bInvalid = true; \
    }
    SOCCER_SQUAD_STYLE_COLORS(APPLY_STYLE_COLOR)
#undef APPLY_STYLE_COLOR
    PushStyleSnapshot();
    ApplyViewerStyle();
    if (StyleStatus)
    {
        StyleStatus->SetText(FText::FromString(bInvalid
            ? TEXT("Algunos valores no son validos; se conservaron los anteriores")
            : TEXT("Aplicado en vivo. GUARDAR mantiene los cambios")));
    }
}

void USoccerSquadViewerWidget::HandleSaveStyle()
{
    HandleApplyStyle();
    const bool bSaved = SaveStyle();
    if (StyleStatus)
    {
        StyleStatus->SetText(FText::FromString(bSaved
            ? TEXT("Guardado en Saved/SoccerUI/SquadViewerStyle.ini")
            : TEXT("No se pudo escribir el archivo de estilos")));
    }
}

void USoccerSquadViewerWidget::HandleResetStyle()
{
    LiveStyle = FSoccerSquadViewerStyleSettings();
    PushStyleSnapshot();
    SyncStyleEditors();
    ApplyViewerStyle();
    if (StyleStatus)
    {
        StyleStatus->SetText(FText::FromString(
            TEXT("Valores originales. Pulsar GUARDAR para conservarlos")));
    }
}

void USoccerSquadViewerWidget::HandleStyleUndo()
{
    if (StyleSnapshotIndex > 0)
    {
        ApplyStyleSnapshot(StyleSnapshotIndex - 1);
        if (StyleStatus) StyleStatus->SetText(FText::FromString(TEXT("Deshecho (sin guardar).")));
    }
}

void USoccerSquadViewerWidget::HandleStyleRedo()
{
    if (StyleSnapshotIndex + 1 < StyleSnapshots.Num())
    {
        ApplyStyleSnapshot(StyleSnapshotIndex + 1);
        if (StyleStatus) StyleStatus->SetText(FText::FromString(TEXT("Rehecho (sin guardar).")));
    }
}

void USoccerSquadViewerWidget::HandleStyleSavePreset()
{
    HandleApplyStyle();
    const bool bSaved = SaveStyleAtPath(StylePresetPath());
    if (StyleStatus) StyleStatus->SetText(FText::FromString(bSaved
        ? TEXT("Preset guardado, sin cambiar el estilo activo.")
        : TEXT("No se pudo guardar el preset.")));
}

void USoccerSquadViewerWidget::HandleStyleLoadPreset()
{
    const FSoccerSquadViewerStyleSettings Previous = LiveStyle;
    if (!LoadStyleAtPath(StylePresetPath()))
    {
        LiveStyle = Previous;
        if (StyleStatus) StyleStatus->SetText(FText::FromString(TEXT("Preset inexistente.")));
        return;
    }
    PushStyleSnapshot();
    SyncStyleEditors();
    ApplyViewerStyle();
    if (StyleStatus) StyleStatus->SetText(FText::FromString(TEXT("Preset cargado (sin guardar).")));
}

void USoccerSquadViewerWidget::HandleStyleExport()
{
    FString OutputPath;
    const bool bExported = USoccerUIStyleEditorController::ExportAllStyles(OutputPath);
    if (StyleStatus) StyleStatus->SetText(FText::FromString(bExported
        ? TEXT("Exportado en Saved/SoccerUI/SoccerUIStyles_Export.txt")
        : TEXT("Guardá algún estilo antes de exportar.")));
}

void USoccerSquadViewerWidget::StyleMainWidget(UWidget* Widget)
{
    if (!Widget) return;
    if (UTextBlock* TextNode = Cast<UTextBlock>(Widget))
    {
        const TWeakObjectPtr<UTextBlock> Key(TextNode);
        int32* OriginalSize = DefaultTextSizes.Find(Key);
        if (!OriginalSize)
        {
            DefaultTextSizes.Add(Key, TextNode->Font.Size);
            OriginalSize = DefaultTextSizes.Find(Key);
        }
        if (OriginalSize)
        {
            FSlateFontInfo Font = TextNode->Font;
            Font.Size = FMath::Max(9, FMath::RoundToInt((*OriginalSize) *
                FSoccerUITheme::GetAdaptiveTextScale(this, LiveStyle.FontScale)));
            TextNode->SetFont(Font);
        }
        FLinearColor Color = EffectiveStyle.Text;
        if (AccentTextNodes.Contains(Key)) Color = EffectiveStyle.Accent;
        else if (SecondaryTextNodes.Contains(Key)) Color = EffectiveStyle.SecondaryText;
        TextNode->SetColorAndOpacity(FSlateColor(Color));
    }
    if (UProgressBar* MeterNode = Cast<UProgressBar>(Widget))
    {
        MeterNode->SetFillColorAndOpacity(EffectiveStyle.Meter);
    }
    if (UButton* ButtonNode = Cast<UButton>(Widget))
    {
        ButtonNode->SetBackgroundColor(EffectiveStyle.ButtonTint);
    }
    if (UPanelWidget* Container = Cast<UPanelWidget>(Widget))
    {
        for (int32 I = 0; I < Container->GetChildrenCount(); ++I)
        {
            StyleMainWidget(Container->GetChildAt(I));
        }
    }
}

void USoccerSquadViewerWidget::ApplyViewerStyle()
{
    EffectiveStyle = LiveStyle;
    const FSoccerSquadViewerStyleSettings DefaultVisualStyle;
#define RESOLVE_SQUAD_THEME_COLOR(Name, Caption) \
    EffectiveStyle.Name = FSoccerUITheme::ResolveColor(TEXT(#Name), LiveStyle.Name, \
        !LiveStyle.Name.Equals(DefaultVisualStyle.Name, .006f));
    SOCCER_SQUAD_STYLE_COLORS(RESOLVE_SQUAD_THEME_COLOR)
#undef RESOLVE_SQUAD_THEME_COLOR
    // On a compact viewport, compress the three columns instead of scaling
    // the entire fixed-width panel into illegibly small text. Do not modify
    // LiveStyle: the values saved by the player are preserved for large screens.
    const FVector2D ViewportUnits = FSoccerUITheme::GetLogicalViewportSize(this);
    const float VisiblePanelWidth = ViewportUnits.X > 1.f
        ? FMath::Min(LiveStyle.PanelWidth, FMath::Max(420.f, ViewportUnits.X - 24.f))
        : LiveStyle.PanelWidth;
    const float VisiblePanelHeight = ViewportUnits.Y > 1.f
        ? FMath::Min(LiveStyle.PanelHeight, FMath::Max(360.f, ViewportUnits.Y - 24.f))
        : LiveStyle.PanelHeight;
    const float VisiblePadding = FMath::Min(LiveStyle.PanelPadding,
        FMath::Max(8.f, VisiblePanelWidth * .028f));
    const float VisibleGap = FMath::Min(LiveStyle.ColumnGap,
        FMath::Max(3.f, VisiblePanelWidth * .013f));
    const float AvailableColumnWidth = FMath::Max(260.f,
        VisiblePanelWidth - 2.f * VisiblePadding - 2.f * VisibleGap);
    const float RequestedColumnWidth = FMath::Max(1.f,
        LiveStyle.ListWidth + LiveStyle.CenterWidth + LiveStyle.StatsWidth);
    const float ColumnScale = FMath::Min(1.f, AvailableColumnWidth / RequestedColumnWidth);
    const float VisibleListWidth = LiveStyle.ListWidth * ColumnScale;
    const float VisibleCenterWidth = LiveStyle.CenterWidth * ColumnScale;
    const float VisibleStatsWidth = LiveStyle.StatsWidth * ColumnScale;
    const float VisibleStatsPadding = FMath::Min(LiveStyle.StatsPadding,
        FMath::Max(4.f, VisibleStatsWidth * .045f));
    // Stat label and meter may shrink together, so the rating value remains
    // visible even when the statistics column is reduced.
    const float StatAvailable = FMath::Max(80.f,
        VisibleStatsWidth - 2.f * VisibleStatsPadding - 32.f);
    const float StatDesired = FMath::Max(1.f,
        LiveStyle.StatLabelWidth + LiveStyle.StatMeterWidth);
    const float StatScale = FMath::Min(1.f, StatAvailable / StatDesired);
    if (MainSizeBox)
    {
        MainSizeBox->SetWidthOverride(VisiblePanelWidth);
        MainSizeBox->SetHeightOverride(VisiblePanelHeight);
    }
    if (ListSizeBox) ListSizeBox->SetWidthOverride(VisibleListWidth);
    if (CenterSizeBox) CenterSizeBox->SetWidthOverride(VisibleCenterWidth);
    if (StatsSizeBox) StatsSizeBox->SetWidthOverride(VisibleStatsWidth);
    if (PreviewSizeBox)
        PreviewSizeBox->SetHeightOverride(FMath::Min(LiveStyle.PreviewHeight,
            FMath::Max(165.f, VisiblePanelHeight * .52f)));
    if (MainDim) MainDim->SetBrushColor(EffectiveStyle.Backdrop);
    if (MainPanel)
    {
        MainPanel->SetBrushColor(EffectiveStyle.Panel);
        MainPanel->SetPadding(FMargin(VisiblePadding));
    }
    if (ListPanel)
    {
        ListPanel->SetBrushColor(EffectiveStyle.ListPanel);
        ListPanel->SetPadding(FMargin(LiveStyle.ListPadding));
    }
    if (CenterPanel) CenterPanel->SetBrushColor(EffectiveStyle.CenterPanel);
    if (StatsPanel)
    {
        StatsPanel->SetBrushColor(EffectiveStyle.StatsPanel);
        StatsPanel->SetPadding(FMargin(VisibleStatsPadding));
    }
    if (ListColumnSlot) ListColumnSlot->SetPadding(FMargin(0, 0, VisibleGap, 0));
    if (CenterColumnSlot) CenterColumnSlot->SetPadding(FMargin(0, 0, VisibleGap, 0));
    for (USizeBox* LabelBox : StatLabelBoxes)
    {
        if (IsValid(LabelBox)) LabelBox->SetWidthOverride(LiveStyle.StatLabelWidth * StatScale);
    }
    for (USizeBox* MeterBox : StatMeterBoxes)
    {
        if (IsValid(MeterBox)) MeterBox->SetWidthOverride(LiveStyle.StatMeterWidth * StatScale);
    }
    for (const TPair<FName, UBorder*>& Pair : PlayerBorders)
    {
        if (!IsValid(Pair.Value)) continue;
        const bool bSelected = CurrentPlayers.IsValidIndex(SelectedPlayerIndex) &&
            CurrentPlayers[SelectedPlayerIndex]->Identity.PlayerId == Pair.Key;
        Pair.Value->SetPadding(FMargin(LiveStyle.CardPadding, 7.f));
        Pair.Value->SetBrushColor(bSelected ? EffectiveStyle.SelectedCard : EffectiveStyle.PlayerCard);
    }
    StyleMainWidget(MainPanel);
    FSoccerUITheme::ApplyButtonStates(MainPanel);
}

#undef SOCCER_SQUAD_STYLE_NUMBERS
#undef SOCCER_SQUAD_STYLE_COLORS
