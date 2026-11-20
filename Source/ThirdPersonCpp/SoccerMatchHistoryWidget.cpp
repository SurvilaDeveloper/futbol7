#include "SoccerMatchHistoryWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"

namespace SoccerHistoryUI5L
{
    const FLinearColor White(0.95f, 0.97f, 0.98f, 1.0f);
    const FLinearColor Muted(0.72f, 0.82f, 0.87f, 1.0f);
    const FLinearColor Green(0.38f, 0.86f, 0.68f, 1.0f);
    const FLinearColor RowDark(0.048f, 0.109f, 0.139f, 1.0f);
    const FLinearColor RowAlt(0.068f, 0.135f, 0.170f, 1.0f);
    const FLinearColor ButtonDark(0.11f, 0.27f, 0.29f, 1.0f);

    UTextBlock* Text(UWidgetTree* Tree, const FString& Label, int32 Size,
        const FLinearColor& Color, ETextJustify::Type Align = ETextJustify::Center)
    {
        UTextBlock* T = Tree->ConstructWidget<UTextBlock>();
        T->SetText(FText::FromString(Label));
        FSlateFontInfo Font = T->Font;
        Font.Size = Size;
        T->SetFont(Font);
        T->SetColorAndOpacity(FSlateColor(Color));
        T->SetAutoWrapText(true);
        T->SetJustification(Align);
        return T;
    }
    UButton* Button(UWidgetTree* Tree, const FString& Label)
    {
        UButton* B = Tree->ConstructWidget<UButton>();
        B->SetBackgroundColor(ButtonDark);
        B->AddChild(Text(Tree, Label, 16, White));
        return B;
    }
    void Vertical(UVerticalBox* Parent, UWidget* Child, float Bottom = 7.0f)
    {
        UVerticalBoxSlot* Slot = Parent->AddChildToVerticalBox(Child);
        Slot->SetPadding(FMargin(0, 0, 0, Bottom));
        Slot->SetHorizontalAlignment(HAlign_Fill);
    }
    void Column(UHorizontalBox* Parent, UWidget* Child, float Size = 1.0f)
    {
        UHorizontalBoxSlot* Slot = Parent->AddChildToHorizontalBox(Child);
        FSlateChildSize Width(ESlateSizeRule::Fill);
        Width.Value = Size;
        Slot->SetSize(Width);
        Slot->SetPadding(FMargin(4, 2));
        Slot->SetVerticalAlignment(VAlign_Center);
    }
    FString EventClock(const FSoccerMatchTimelineEvent5I& Event)
    {
        const int32 Seconds = FMath::Max(0, FMath::FloorToInt(Event.MatchSeconds));
        const TCHAR* Period = TEXT("1T");
        if (Event.Period == ESoccerMatchPeriod::SecondHalf) Period = TEXT("2T");
        else if (Event.Period == ESoccerMatchPeriod::HalfTime) Period = TEXT("ET");
        else if (Event.Period == ESoccerMatchPeriod::FullTime) Period = TEXT("FT");
        return FString::Printf(TEXT("%s %02d:%02d"), Period, Seconds / 60, Seconds % 60);
    }
}

void USoccerMatchHistoryWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    bIsFocusable = true;
    BuildHistoryScreen();
}

void USoccerMatchHistoryWidget::BuildHistoryScreen()
{
    using namespace SoccerHistoryUI5L;
    if (!WidgetTree || WidgetTree->RootWidget) return;

    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>();
    WidgetTree->RootWidget = Root;
    UBorder* Background = WidgetTree->ConstructWidget<UBorder>();
    Background->SetBrushColor(FLinearColor(0.013f, 0.028f, 0.036f, 0.98f));
    UCanvasPanelSlot* RootSlot = Root->AddChildToCanvas(Background);
    RootSlot->SetAnchors(FAnchors(0, 0, 1, 1));
    RootSlot->SetOffsets(FMargin(0));

    UScaleBox* Scaler = WidgetTree->ConstructWidget<UScaleBox>();
    Scaler->SetStretch(EStretch::ScaleToFit);
    Scaler->SetStretchDirection(EStretchDirection::DownOnly);
    Background->SetContent(Scaler);
    USizeBox* Frame = WidgetTree->ConstructWidget<USizeBox>();
    Frame->SetWidthOverride(900.0f);
    Frame->SetHeightOverride(680.0f);
    Scaler->AddChild(Frame);
    if (UScaleBoxSlot* FrameScaleSlot = Cast<UScaleBoxSlot>(Frame->Slot))
    {
        FrameScaleSlot->SetHorizontalAlignment(HAlign_Center);
        FrameScaleSlot->SetVerticalAlignment(VAlign_Center);
    }
    UBorder* Card = WidgetTree->ConstructWidget<UBorder>();
    Card->SetBrushColor(FLinearColor(0.035f, 0.075f, 0.095f, 1.0f));
    Card->SetPadding(FMargin(30, 22));
    Frame->SetContent(Card);
    UVerticalBox* Layout = WidgetTree->ConstructWidget<UVerticalBox>();
    Card->SetContent(Layout);
    Vertical(Layout, Text(WidgetTree, TEXT("HISTORIAL DE PARTIDOS"), 29, White), 7.0f);
    RecordCount = Text(WidgetTree, TEXT("Cargando..."), 14, Muted);
    Vertical(Layout, RecordCount, 12.0f);

    UHorizontalBox* Picker = WidgetTree->ConstructWidget<UHorizontalBox>();
    UButton* Prev = Button(WidgetTree, TEXT("ANTERIOR"));
    UButton* Next = Button(WidgetTree, TEXT("SIGUIENTE"));
    Prev->OnClicked.AddDynamic(this, &USoccerMatchHistoryWidget::HandlePrevious);
    Next->OnClicked.AddDynamic(this, &USoccerMatchHistoryWidget::HandleNext);
    MatchSelector = WidgetTree->ConstructWidget<UComboBoxString>();
    MatchSelector->OnSelectionChanged.AddDynamic(this,
        &USoccerMatchHistoryWidget::HandleSelectionChanged);
    Column(Picker, Prev, 1.05f);
    Column(Picker, MatchSelector, 4.0f);
    Column(Picker, Next, 1.05f);
    Vertical(Layout, Picker, 12.0f);

    SummaryHeading = Text(WidgetTree, TEXT("Seleccioná un partido"), 19, Green);
    Vertical(Layout, SummaryHeading, 12.0f);

    UHorizontalBox* Pages = WidgetTree->ConstructWidget<UHorizontalBox>();
    ReportButton = Button(WidgetTree, TEXT("INFORME"));
    PlayersButton = Button(WidgetTree, TEXT("FUTBOLISTAS"));
    EventsButton = Button(WidgetTree, TEXT("CRONOLOGÍA"));
    TotalsButton5M = Button(WidgetTree, TEXT("ACUMULADOS"));
    ReportButton->OnClicked.AddDynamic(this, &USoccerMatchHistoryWidget::HandleReport);
    PlayersButton->OnClicked.AddDynamic(this, &USoccerMatchHistoryWidget::HandlePlayers);
    EventsButton->OnClicked.AddDynamic(this, &USoccerMatchHistoryWidget::HandleEvents);
    TotalsButton5M->OnClicked.AddDynamic(this, &USoccerMatchHistoryWidget::HandleTotals5M);
    Column(Pages, ReportButton);
    Column(Pages, PlayersButton);
    Column(Pages, EventsButton);
    Column(Pages, TotalsButton5M);
    Vertical(Layout, Pages, 9.0f);

    USizeBox* DetailSize = WidgetTree->ConstructWidget<USizeBox>();
    DetailSize->SetHeightOverride(395.0f);
    Details = WidgetTree->ConstructWidget<UScrollBox>();
    Details->SetScrollBarVisibility(ESlateVisibility::Visible);
    DetailSize->SetContent(Details);
    Vertical(Layout, DetailSize, 8.0f);

    Vertical(Layout, Text(WidgetTree,
        TEXT("Izq/Der: partido  |  1/2/3/4: pestañas  |  Esc/B: volver"),
        13, Muted), 6.0f);
    UButton* Close = WidgetTree->ConstructWidget<UButton>();
    ReturnButtonLabel = Text(WidgetTree, TEXT("VOLVER A SELECCIÓN DE CLUB"), 16, White);
    Close->AddChild(ReturnButtonLabel);
    Close->SetBackgroundColor(FLinearColor(0.14f, 0.47f, 0.37f, 1.0f));
    Close->OnClicked.AddDynamic(this, &USoccerMatchHistoryWidget::HandleClose);
    Vertical(Layout, Close, 0);
}

void USoccerMatchHistoryWidget::OpenHistory(bool bFromMainMenu)
{
    // The same history widget can be opened from two different screens.
    // Update the button each time, including when a widget instance is reused.
    if (IsValid(ReturnButtonLabel))
    {
        ReturnButtonLabel->SetText(FText::FromString(bFromMainMenu
            ? TEXT("VOLVER AL MENÚ PRINCIPAL")
            : TEXT("VOLVER A SELECCIÓN DE CLUB")));
    }
    FSoccerMatchHistory5L::LoadRecent(Matches, 100);
    TArray<FSoccerArchivedMatch5L> AllSavedMatches5M;
    FSoccerMatchHistory5L::LoadRecent(AllSavedMatches5M, 10000);
    TotalMatches5M = AllSavedMatches5M.Num();
    FSoccerHistoricalStats5M::Aggregate(AllSavedMatches5M, TotalClubs5M, TotalPlayers5M);
    SelectedIndex = 0;
    ActivePage = 0;
    bChangingOptions = true;
    if (MatchSelector)
    {
        MatchSelector->ClearOptions();
        for (const FSoccerArchivedMatch5L& M : Matches)
        {
            MatchSelector->AddOption(FString::Printf(TEXT("%s  |  %s %d - %d %s"),
                *M.PlayedAt, *M.Report.LocalClubName, M.Report.LocalScore,
                M.Report.RivalScore, *M.Report.RivalClubName));
        }
        if (Matches.Num() > 0) MatchSelector->SetSelectedIndex(0);
    }
    bChangingOptions = false;
    if (RecordCount)
    {
        RecordCount->SetText(FText::FromString(FString::Printf(
            TEXT("%d encuentros guardados  |  Archivos JSON locales"), Matches.Num())));
    }
    RefreshContents();
    if (APlayerController* PC = GetOwningPlayer())
    {
        PC->bShowMouseCursor = true;
        FInputModeUIOnly Mode;
        Mode.SetWidgetToFocus(TakeWidget());
        Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        PC->SetInputMode(Mode);
        SetKeyboardFocus();
    }
}

void USoccerMatchHistoryWidget::AddDarkLine(
    const FString& Value, int32 FontSize, bool bAccent)
{
    if (!WidgetTree || !Details) return;
    using namespace SoccerHistoryUI5L;
    UBorder* Line = WidgetTree->ConstructWidget<UBorder>();
    Line->SetBrushColor(bAccent ? RowAlt : RowDark);
    Line->SetPadding(FMargin(12, 8));
    Line->SetContent(Text(WidgetTree, Value, FontSize,
        bAccent ? Green : White, ETextJustify::Left));
    Details->AddChild(Line);
}

void USoccerMatchHistoryWidget::RefreshContents()
{
    using namespace SoccerHistoryUI5L;
    if (!Details || !SummaryHeading) return;
    Details->ClearChildren();
    if (ReportButton) ReportButton->SetBackgroundColor(ActivePage == 0 ? Green : ButtonDark);
    if (PlayersButton) PlayersButton->SetBackgroundColor(ActivePage == 1 ? Green : ButtonDark);
    if (EventsButton) EventsButton->SetBackgroundColor(ActivePage == 2 ? Green : ButtonDark);
    if (TotalsButton5M) TotalsButton5M->SetBackgroundColor(ActivePage == 3 ? Green : ButtonDark);
    if (ActivePage == 3)
    {
        SummaryHeading->SetText(FText::FromString(TEXT("ESTADÍSTICAS HISTÓRICAS ACUMULADAS")));
        AddDarkLine(FString::Printf(TEXT("PARTIDOS FINALIZADOS REGISTRADOS: %d"), TotalMatches5M), 17, true);
        if (TotalClubs5M.Num() == 0)
        {
            AddDarkLine(TEXT("Todavía no hay partidos completos guardados."));
        }
        for (const FSoccerClubTotals5M& Club5M : TotalClubs5M)
        {
            AddDarkLine(Club5M.Name.ToUpper(), 18, true);
            AddDarkLine(FString::Printf(TEXT("PJ %d   PG %d   PE %d   PP %d   GF %d   GC %d"),
                Club5M.Played, Club5M.Wins, Club5M.Draws, Club5M.Losses,
                Club5M.GoalsFor, Club5M.GoalsAgainst));
            AddDarkLine(FString::Printf(TEXT("REMATES %d   AL ARCO %d   ATAJADAS %d   CÓRNERS %d   TIROS LIBRES %d"),
                Club5M.Shots, Club5M.ShotsOnTarget, Club5M.Saves,
                Club5M.Corners, Club5M.FreeKicks), 13);
        }
        AddDarkLine(TEXT("FUTBOLISTAS: REGISTROS CON ACCIONES"), 17, true);
        for (const FSoccerPlayerTotals5M& Player5M : TotalPlayers5M)
        {
            const float Average5M = Player5M.Records > 0
                ? Player5M.RatingSum / Player5M.Records : 0.0f;
            AddDarkLine(FString::Printf(TEXT("%s  |  %s  |  REG %d  G %d  AS %d  AT %d  NOTA MEDIA %.1f"),
                *Player5M.Club, *Player5M.Name, Player5M.Records,
                Player5M.Goals, Player5M.Assists, Player5M.Saves, Average5M));
            AddDarkLine(FString::Printf(TEXT("REM %d  AL %d  PASES %d/%d  REC %d  INT %d  ENT %d"),
                Player5M.Shots, Player5M.ShotsOnTarget,
                Player5M.PassesCompleted, Player5M.PassesAttempted,
                Player5M.Recoveries, Player5M.Interceptions, Player5M.CleanTackles), 13);
        }
        Details->ScrollToStart();
        return;
    }
    if (!Matches.IsValidIndex(SelectedIndex))
    {
        SummaryHeading->SetText(FText::FromString(TEXT("TODAVÍA NO HAY ENCUENTROS GUARDADOS")));
        AddDarkLine(TEXT("Al finalizar un partido nuevo, se guardará automáticamente su informe aquí."), 16);
        AddDarkLine(TEXT("Los encuentros anteriores a la etapa 5L no se pueden reconstruir automáticamente."), 14);
        return;
    }

    const FSoccerArchivedMatch5L& Saved = Matches[SelectedIndex];
    const FSoccerFinalMatchReport5K& R = Saved.Report;
    SummaryHeading->SetText(FText::FromString(FString::Printf(
        TEXT("%s   %d - %d   %s"), *R.LocalClubName, R.LocalScore,
        R.RivalScore, *R.RivalClubName)));
    if (ActivePage == 0)
    {
        AddDarkLine(FString::Printf(TEXT("FECHA: %s"), *Saved.PlayedAt), 15, true);
        if (R.bHasBestPlayer)
            AddDarkLine(FString::Printf(TEXT("JUGADOR DEL PARTIDO: %s  |  NOTA %.1f"),
                *R.BestPlayerName, R.BestPlayerRating), 17, true);
        const FSoccerMatchTeamStatistics5D& A = R.TeamStatistics.PlayerTeam;
        const FSoccerMatchTeamStatistics5D& B = R.TeamStatistics.OpponentTeam;
        const float Total = A.ControlledPossessionSeconds + B.ControlledPossessionSeconds;
        if (Total > KINDA_SMALL_NUMBER)
        {
            const int32 Pct = FMath::Clamp(FMath::RoundToInt(
                100.0f * A.ControlledPossessionSeconds / Total), 0, 100);
            AddDarkLine(FString::Printf(TEXT("POSESIÓN CONTROLADA: %d%% - %d%%"), Pct, 100-Pct));
        }
        AddDarkLine(FString::Printf(TEXT("REMATES: %d - %d   |   AL ARCO: %d - %d"),
            A.Shots, B.Shots, A.ShotsOnTarget, B.ShotsOnTarget));
        AddDarkLine(FString::Printf(TEXT("ATAJADAS: %d - %d   |   CÓRNERS: %d - %d"),
            A.GoalkeeperSaves, B.GoalkeeperSaves, A.CornersWon, B.CornersWon));
        AddDarkLine(FString::Printf(TEXT("TIROS LIBRES: %d - %d   |   OFFSIDES: %d - %d"),
            A.FreeKicksWon, B.FreeKicksWon, A.OffsidesCommitted, B.OffsidesCommitted));
        AddDarkLine(FString::Printf(TEXT("PENALES: %d - %d   |   LATERALES: %d - %d"),
            A.PenaltiesWon, B.PenaltiesWon, A.ThrowInsWon, B.ThrowInsWon));
        int32 Goals = 0;
        for (const FSoccerMatchTimelineEvent5I& E : R.Events)
        {
            if (E.Title != TEXT("¡GOL!")) continue;
            if (Goals++ == 0) AddDarkLine(TEXT("GOLES"), 17, true);
            AddDarkLine(FString::Printf(TEXT("%s  |  %s  |  %s"),
                *EventClock(E), *(E.Team == ESoccerTeam::PlayerTeam
                ? R.LocalClubName : R.RivalClubName), *E.Detail));
        }
        if (Goals == 0) AddDarkLine(TEXT("Sin goles en este encuentro."));
    }
    else if (ActivePage == 1)
    {
        if (R.Players.Num() == 0) AddDarkLine(TEXT("No hay acciones individuales registradas."));
        for (int32 TeamIndex = 0; TeamIndex < 2; ++TeamIndex)
        {
            const ESoccerTeam Team = TeamIndex == 0
                ? ESoccerTeam::PlayerTeam : ESoccerTeam::OpponentTeam;
            AddDarkLine(TeamIndex == 0 ? R.LocalClubName : R.RivalClubName, 18, true);
            for (const FSoccerPlayerMatchStatistics5G& P : R.Players)
            {
                if (P.Team != Team) continue;
                AddDarkLine(FString::Printf(
                    TEXT("#%d %s  |  NOTA %.1f  |  G %d  AS %d  R %d  AL %d  AT %d"),
                    P.ShirtNumber, *P.DisplayName,
                    ASoccerMatchManager::CalculatePlayerRating5J(P),
                    P.Goals, P.Assists, P.Shots, P.ShotsOnTarget, P.Saves));
                AddDarkLine(FString::Printf(
                    TEXT("PASES %d/%d  |  REC %d  INT %d  ENT %d  AG %d"),
                    P.PassesCompleted, P.PassesAttempted, P.Recoveries,
                    P.Interceptions, P.CleanTackles, P.OwnGoals), 13);
            }
        }
    }
    else
    {
        if (R.Events.Num() == 0) AddDarkLine(TEXT("Sin acontecimientos registrados."));
        for (const FSoccerMatchTimelineEvent5I& E : R.Events)
        {
            const FString TeamName = E.bHasTeam
                ? (E.Team == ESoccerTeam::PlayerTeam ? R.LocalClubName : R.RivalClubName)
                : TEXT("PARTIDO");
            AddDarkLine(FString::Printf(TEXT("%s  |  %s  |  %s: %s"),
                *EventClock(E), *TeamName, *E.Title, *E.Detail), 14,
                E.Title == TEXT("¡GOL!"));
        }
    }
    Details->ScrollToStart();
}

void USoccerMatchHistoryWidget::SwitchMatch(int32 Delta)
{
    if (Matches.Num() == 0 || !MatchSelector) return;
    SelectedIndex = FMath::Clamp(SelectedIndex + Delta, 0, Matches.Num() - 1);
    bChangingOptions = true;
    MatchSelector->SetSelectedIndex(SelectedIndex);
    bChangingOptions = false;
    RefreshContents();
}
void USoccerMatchHistoryWidget::SwitchPage(int32 NewPage)
{
    ActivePage = FMath::Clamp(NewPage, 0, 3);
    RefreshContents();
}
void USoccerMatchHistoryWidget::HandlePrevious() { SwitchMatch(-1); }
void USoccerMatchHistoryWidget::HandleNext() { SwitchMatch(1); }
void USoccerMatchHistoryWidget::HandleReport() { SwitchPage(0); }
void USoccerMatchHistoryWidget::HandlePlayers() { SwitchPage(1); }
void USoccerMatchHistoryWidget::HandleEvents() { SwitchPage(2); }
void USoccerMatchHistoryWidget::HandleTotals5M() { SwitchPage(3); }
void USoccerMatchHistoryWidget::HandleSelectionChanged(FString Option, ESelectInfo::Type SelectionType)
{
    if (bChangingOptions || !MatchSelector) return;
    const int32 Index = MatchSelector->FindOptionIndex(Option);
    if (Matches.IsValidIndex(Index))
    {
        SelectedIndex = Index;
        RefreshContents();
    }
}
void USoccerMatchHistoryWidget::HandleClose()
{
    RemoveFromParent();
    OnHistoryClosed.Broadcast();
}
FReply USoccerMatchHistoryWidget::NativeOnPreviewKeyDown(
    const FGeometry& Geometry, const FKeyEvent& KeyEvent)
{
    const FKey Key = KeyEvent.GetKey();
    if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right)
    {
        HandleClose();
        return FReply::Handled();
    }
    if (Key == EKeys::One) { SwitchPage(0); return FReply::Handled(); }
    if (Key == EKeys::Two) { SwitchPage(1); return FReply::Handled(); }
    if (Key == EKeys::Three) { SwitchPage(2); return FReply::Handled(); }
    if (Key == EKeys::Four) { SwitchPage(3); return FReply::Handled(); }
    if (Key == EKeys::Left || Key == EKeys::Gamepad_DPad_Left)
    {
        SwitchMatch(-1); return FReply::Handled();
    }
    if (Key == EKeys::Right || Key == EKeys::Gamepad_DPad_Right)
    {
        SwitchMatch(1); return FReply::Handled();
    }
    if (Key == EKeys::F11)
    {
        if (APlayerController* PC = GetOwningPlayer()) PC->ConsoleCommand(TEXT("fullscreen"), true);
        return FReply::Handled();
    }
    return Super::NativeOnPreviewKeyDown(Geometry, KeyEvent);
}
