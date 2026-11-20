#include "SoccerPeriodSummaryWidget.h"

#include "SoccerMatchManager.h"
#include "SoccerClubProfile.h"
#include "SoccerTeamTypes.h"
#include "GameHUD.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"

// Unique namespace prevents unity-build collisions with existing menu helpers.
namespace SoccerPeriodSummary5F
{
	const FLinearColor White(0.96f, 0.98f, 0.99f, 1.0f);
	const FLinearColor Muted(0.67f, 0.78f, 0.84f, 1.0f);
	const FLinearColor Green(0.16f, 0.73f, 0.53f, 1.0f);
	const FLinearColor Cyan(0.34f, 0.75f, 0.91f, 1.0f);

	// The player table header and data rows MUST use identical column weights
	// and horizontal padding. Never position the headings with spaces in text.
	constexpr float PlayerNameWidth5G = 3.8f;
	constexpr float PlayerStatWidth5G = 0.8f;
    // Shared by the player rows and BOTH attack/defense headings.
    constexpr float PlayerRatingWidth5J = 1.02f;
	constexpr float PlayerRowHorizontalPadding5G = 10.0f;

	UTextBlock* NewText(UWidgetTree* Tree, const FString& Value, int32 FontSize,
		const FLinearColor& Color, ETextJustify::Type Justification = ETextJustify::Center)
	{
		UTextBlock* Label = Tree->ConstructWidget<UTextBlock>();
		Label->SetText(FText::FromString(Value));
		FSlateFontInfo FontInfo = Label->Font;
		FontInfo.Size = FontSize;
		Label->SetFont(FontInfo);
		Label->SetColorAndOpacity(FSlateColor(Color));
		Label->SetJustification(Justification);
		Label->SetAutoWrapText(true);
		return Label;
	}

	void AddVertical(UVerticalBox* Parent, UWidget* Child, const FMargin& Padding)
	{
		UVerticalBoxSlot* ChildSlot = Parent->AddChildToVerticalBox(Child);
		ChildSlot->SetPadding(Padding);
		ChildSlot->SetHorizontalAlignment(HAlign_Fill);
	}

	void AddColumn(UHorizontalBox* Parent, UWidget* Child, float RelativeWidth)
	{
		UHorizontalBoxSlot* ChildSlot = Parent->AddChildToHorizontalBox(Child);
		FSlateChildSize ColumnSize(ESlateSizeRule::Fill);
		ColumnSize.Value = RelativeWidth;
		ChildSlot->SetSize(ColumnSize);
		ChildSlot->SetHorizontalAlignment(HAlign_Fill);
		ChildSlot->SetVerticalAlignment(VAlign_Center);
	}

	FString GetClubName(const ASoccerMatchManager* Match, ESoccerTeam Team)
	{
		const USoccerClubProfile* Profile = Match ? Match->GetClubProfileForTeam(Team) : nullptr;
		if (IsValid(Profile))
		{
			if (!Profile->ShortName.IsEmpty()) return Profile->ShortName.ToString();
			if (!Profile->DisplayName.IsEmpty()) return Profile->DisplayName.ToString();
		}
		return Team == ESoccerTeam::PlayerTeam ? TEXT("CLUB LOCAL") : TEXT("CLUB RIVAL");
	}
}

void USoccerPeriodSummaryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	bIsFocusable = true;
	BuildScreen5F();
}

void USoccerPeriodSummaryWidget::BuildScreen5F()
{
	if (!WidgetTree) return;
	using namespace SoccerPeriodSummary5F;

	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget = RootCanvas;

	UBorder* Backdrop = WidgetTree->ConstructWidget<UBorder>();
	Backdrop->SetBrushColor(FLinearColor(0.008f, 0.018f, 0.025f, 0.94f));
	UCanvasPanelSlot* BackdropSlot = RootCanvas->AddChildToCanvas(Backdrop);
	BackdropSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	BackdropSlot->SetOffsets(FMargin(0.0f));

	UScaleBox* ResponsiveScale = WidgetTree->ConstructWidget<UScaleBox>();
	ResponsiveScale->SetStretch(EStretch::ScaleToFit);
	ResponsiveScale->SetStretchDirection(EStretchDirection::DownOnly);
	Backdrop->SetContent(ResponsiveScale);

	USizeBox* CardSize = WidgetTree->ConstructWidget<USizeBox>();
	CardSize->SetWidthOverride(760.0f);
	CardSize->SetHeightOverride(685.0f);
	ResponsiveScale->AddChild(CardSize);
	if (UScaleBoxSlot* ResponsiveSlot = Cast<UScaleBoxSlot>(CardSize->Slot))
	{
		ResponsiveSlot->SetHorizontalAlignment(HAlign_Center);
		ResponsiveSlot->SetVerticalAlignment(VAlign_Center);
	}

	UBorder* MainCard = WidgetTree->ConstructWidget<UBorder>();
	MainCard->SetBrushColor(FLinearColor(0.030f, 0.064f, 0.085f, 1.0f));
	MainCard->SetPadding(FMargin(30.0f, 20.0f));
	CardSize->SetContent(MainCard);

	UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>();
	MainCard->SetContent(Content);

	HeadingText = NewText(WidgetTree, TEXT("ENTRETIEMPO"), 32, White);
	AddVertical(Content, HeadingText, FMargin(0.0f, 2.0f, 0.0f, 10.0f));
	ClubsText = NewText(WidgetTree, TEXT("CLUB LOCAL         CLUB RIVAL"), 18, Muted);
	AddVertical(Content, ClubsText, FMargin(0.0f, 0.0f, 0.0f, 2.0f));
	ScoreText = NewText(WidgetTree, TEXT("0  -  0"), 42, White);
	AddVertical(Content, ScoreText, FMargin(0.0f, 0.0f, 0.0f, 6.0f));

	PossessionText = NewText(WidgetTree, TEXT("POSESIÓN CONTROLADA"), 17, Muted);
	AddVertical(Content, PossessionText, FMargin(0.0f, 2.0f, 0.0f, 14.0f));

    // The two views use the same scroll region: adding individual players
    // cannot push the continue button off-screen at 1280x720.
    UHorizontalBox* SummaryTabs5G = WidgetTree->ConstructWidget<UHorizontalBox>();
    TeamTabButton5G = WidgetTree->ConstructWidget<UButton>();
    PlayerTabButton5G = WidgetTree->ConstructWidget<UButton>();
    TimelineTabButton5I = WidgetTree->ConstructWidget<UButton>();
    ReportTabButton5K = WidgetTree->ConstructWidget<UButton>();
    TeamTabButton5G->AddChild(NewText(WidgetTree, TEXT("EQUIPOS"), 17, White));
    PlayerTabButton5G->AddChild(NewText(WidgetTree, TEXT("FUTBOLISTAS"), 16, White));
    TimelineTabButton5I->AddChild(NewText(WidgetTree, TEXT("CRONOLOGÍA"), 16, White));
    ReportTabButton5K->AddChild(NewText(WidgetTree, TEXT("INFORME"), 16, White));
    TeamTabButton5G->OnClicked.AddDynamic(this, &USoccerPeriodSummaryWidget::ShowTeamStatistics5G);
    PlayerTabButton5G->OnClicked.AddDynamic(this, &USoccerPeriodSummaryWidget::ShowPlayerStatistics5G);
    TimelineTabButton5I->OnClicked.AddDynamic(this, &USoccerPeriodSummaryWidget::ShowTimeline5I);
    ReportTabButton5K->OnClicked.AddDynamic(this, &USoccerPeriodSummaryWidget::ShowFinalReport5K);
    AddColumn(SummaryTabs5G, TeamTabButton5G, 1.0f);
    AddColumn(SummaryTabs5G, PlayerTabButton5G, 1.0f);
    AddColumn(SummaryTabs5G, TimelineTabButton5I, 1.0f);
    AddColumn(SummaryTabs5G, ReportTabButton5K, 1.0f);
    ReportTabButton5K->SetVisibility(ESlateVisibility::Collapsed);
    AddVertical(Content, SummaryTabs5G, FMargin(10.0f, 0.0f, 10.0f, 8.0f));

    // Second level of navigation keeps both individual tables readable at 720p.
    // The attack table retains the exact aligned six-column design from 5G.
    IndividualPages5H = WidgetTree->ConstructWidget<UHorizontalBox>();
    AttackingPageButton5H = WidgetTree->ConstructWidget<UButton>();
    DefensivePageButton5H = WidgetTree->ConstructWidget<UButton>();
    AttackingPageButton5H->AddChild(NewText(WidgetTree, TEXT("ATAQUE"), 14, White));
    DefensivePageButton5H->AddChild(NewText(WidgetTree, TEXT("PASES Y DEFENSA"), 14, White));
    AttackingPageButton5H->OnClicked.AddDynamic(this,
        &USoccerPeriodSummaryWidget::ShowAttackIndividual5H);
    DefensivePageButton5H->OnClicked.AddDynamic(this,
        &USoccerPeriodSummaryWidget::ShowDefenseIndividual5H);
    AddColumn(IndividualPages5H, AttackingPageButton5H, 1.0f);
    AddColumn(IndividualPages5H, DefensivePageButton5H, 1.0f);
    AddVertical(Content, IndividualPages5H, FMargin(10.0f, 0.0f, 10.0f, 5.0f));
    IndividualPages5H->SetVisibility(ESlateVisibility::Collapsed);

    StatisticsHeightBox5H = WidgetTree->ConstructWidget<USizeBox>();
    StatisticsHeightBox5H->SetHeightOverride(315.0f);
	StatisticsScroll = WidgetTree->ConstructWidget<UScrollBox>();
	StatisticsScroll->SetScrollBarVisibility(ESlateVisibility::Visible);
    StatisticsHeightBox5H->SetContent(StatisticsScroll);
    AddVertical(Content, StatisticsHeightBox5H, FMargin(0.0f, 0.0f, 0.0f, 9.0f));

	StatusText = NewText(WidgetTree, TEXT(""), 14, Muted);
	AddVertical(Content, StatusText, FMargin(0.0f, 0.0f, 0.0f, 8.0f));

	ContinueButton = WidgetTree->ConstructWidget<UButton>();
    ContinueButton->SetBackgroundColor(FLinearColor(0.32f, 0.82f, 0.52f, 1.0f));
    USizeBox* ContinueContentSize5M = WidgetTree->ConstructWidget<USizeBox>();
    ContinueContentSize5M->SetMinDesiredHeight(55.0f);
    ContinueLabel = NewText(WidgetTree, TEXT("CONTINUAR AL SEGUNDO TIEMPO"), 21,
        FLinearColor(0.018f, 0.13f, 0.085f, 1.0f));
    ContinueContentSize5M->SetContent(ContinueLabel);
    ContinueButton->AddChild(ContinueContentSize5M);
	ContinueButton->OnClicked.AddDynamic(this, &USoccerPeriodSummaryWidget::HandleContinueClicked5F);

    // Both actions share the existing bottom row: no extra height is needed at 720p.
    UHorizontalBox* BottomActions5M = WidgetTree->ConstructWidget<UHorizontalBox>();
    AddColumn(BottomActions5M, ContinueButton, 1.0f);
    PlayAgainButton5M = WidgetTree->ConstructWidget<UButton>();
    PlayAgainButton5M->SetBackgroundColor(FLinearColor(0.32f, 0.82f, 0.52f, 1.0f));
    USizeBox* PlayAgainContentSize5M = WidgetTree->ConstructWidget<USizeBox>();
    PlayAgainContentSize5M->SetMinDesiredHeight(55.0f);
    PlayAgainContentSize5M->SetContent(NewText(WidgetTree, TEXT("JUGAR OTRO PARTIDO"), 21,
        FLinearColor(0.018f, 0.13f, 0.085f, 1.0f)));
    PlayAgainButton5M->AddChild(PlayAgainContentSize5M);
    PlayAgainButton5M->OnClicked.AddDynamic(this,
        &USoccerPeriodSummaryWidget::HandlePlayAgain5M);
    AddColumn(BottomActions5M, PlayAgainButton5M, 1.0f);
    PlayAgainButton5M->SetVisibility(ESlateVisibility::Collapsed);
    AddVertical(Content, BottomActions5M, FMargin(20.0f, 0.0f, 20.0f, 0.0f));
}

void USoccerPeriodSummaryWidget::InitializeForMatch(ASoccerMatchManager* InMatchManager)
{
	MatchManager = InMatchManager;
    // The same widget instance can survive between periods: the full-time
    // panel must start on the final report, not the half-time tab last viewed.
    bShowingReport5K = IsValid(MatchManager) &&
        MatchManager->GetCurrentMatchPeriod() == ESoccerMatchPeriod::FullTime;
    bShowingTimeline5I = false;
    bShowingPlayers5G = false;
	RefreshStatistics5F();
	RefreshContinueHint5F();
}

void USoccerPeriodSummaryWidget::RefreshStatistics5F()
{
	if (!IsValid(MatchManager) || !IsValid(StatisticsScroll)) return;
	using namespace SoccerPeriodSummary5F;

	const bool bFullTime = MatchManager->GetCurrentMatchPeriod() == ESoccerMatchPeriod::FullTime;
	HeadingText->SetText(FText::FromString(bFullTime
		? TEXT("FINAL DEL PARTIDO") : TEXT("ENTRETIEMPO")));
	ClubsText->SetText(FText::FromString(FString::Printf(TEXT("%s        |        %s"),
		*GetClubName(MatchManager, ESoccerTeam::PlayerTeam),
		*GetClubName(MatchManager, ESoccerTeam::OpponentTeam))));
	ScoreText->SetText(FText::FromString(FString::Printf(TEXT("%d  -  %d"),
		MatchManager->GetPlayerTeamScore(), MatchManager->GetOpponentTeamScore())));

	const FSoccerMatchStatistics5D& Stats = MatchManager->GetMatchStatistics5D();
	const FSoccerMatchTeamStatistics5D& LocalStats = Stats.PlayerTeam;
	const FSoccerMatchTeamStatistics5D& RivalStats = Stats.OpponentTeam;
	const float Total = LocalStats.ControlledPossessionSeconds + RivalStats.ControlledPossessionSeconds;
	const int32 LocalPercent = Total > KINDA_SMALL_NUMBER
		? FMath::Clamp(FMath::RoundToInt(100.0f * LocalStats.ControlledPossessionSeconds / Total), 0, 100)
		: 50;
	PossessionText->SetText(FText::FromString(Total > KINDA_SMALL_NUMBER
		? FString::Printf(TEXT("POSESIÓN CONTROLADA        %d%%  -  %d%%"), LocalPercent, 100 - LocalPercent)
		: TEXT("POSESIÓN CONTROLADA        SIN DATOS")));

    if (IsValid(TeamTabButton5G) && IsValid(PlayerTabButton5G))
    {
        TeamTabButton5G->SetBackgroundColor(!bShowingPlayers5G && !bShowingTimeline5I && !bShowingReport5K ?
            Green : FLinearColor(0.12f, 0.25f, 0.30f, 1.0f));
        PlayerTabButton5G->SetBackgroundColor(bShowingPlayers5G ?
            Green : FLinearColor(0.12f, 0.25f, 0.30f, 1.0f));
    }
    if (IsValid(TimelineTabButton5I))
    {
        TimelineTabButton5I->SetBackgroundColor(bShowingTimeline5I ?
            Green : FLinearColor(0.12f, 0.25f, 0.30f, 1.0f));
    }
    if (IsValid(ReportTabButton5K))
    {
        ReportTabButton5K->SetVisibility(bFullTime
            ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
        ReportTabButton5K->SetBackgroundColor(bShowingReport5K ?
            Green : FLinearColor(0.12f, 0.25f, 0.30f, 1.0f));
    }
    if (IsValid(IndividualPages5H))
    {
        IndividualPages5H->SetVisibility(bShowingPlayers5G
            ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }
    if (IsValid(StatisticsHeightBox5H))
    {
        StatisticsHeightBox5H->SetHeightOverride(bShowingPlayers5G ? 278.0f : 315.0f);
    }
    if (IsValid(AttackingPageButton5H) && IsValid(DefensivePageButton5H))
    {
        AttackingPageButton5H->SetBackgroundColor(!bShowingDefense5H ?
            Green : FLinearColor(0.12f, 0.25f, 0.30f, 1.0f));
        DefensivePageButton5H->SetBackgroundColor(bShowingDefense5H ?
            Green : FLinearColor(0.12f, 0.25f, 0.30f, 1.0f));
    }
    if (bShowingReport5K && bFullTime)
    {
        RefreshFinalReport5K();
        return;
    }
    if (bShowingPlayers5G)
    {
        RefreshPlayerStatistics5G();
        return;
    }
    if (bShowingTimeline5I)
    {
        RefreshTimeline5I();
        return;
    }

	StatisticsScroll->ClearChildren();
	int32 RowIndex = 0;
	const auto AddNumbers = [this, &RowIndex](const FString& Label, int32 Left, int32 Right)
	{
		AddStatisticRow5F(Label, FString::FromInt(Left), FString::FromInt(Right), RowIndex++);
	};
	AddNumbers(TEXT("REMATES"), LocalStats.Shots, RivalStats.Shots);
	AddNumbers(TEXT("REMATES AL ARCO"), LocalStats.ShotsOnTarget, RivalStats.ShotsOnTarget);
	AddNumbers(TEXT("ATAJADAS"), LocalStats.GoalkeeperSaves, RivalStats.GoalkeeperSaves);
	AddNumbers(TEXT("CÓRNERS"), LocalStats.CornersWon, RivalStats.CornersWon);
	AddNumbers(TEXT("TIROS LIBRES"), LocalStats.FreeKicksWon, RivalStats.FreeKicksWon);
	AddNumbers(TEXT("FUERAS DE JUEGO"), LocalStats.OffsidesCommitted, RivalStats.OffsidesCommitted);
	AddNumbers(TEXT("PENALES"), LocalStats.PenaltiesWon, RivalStats.PenaltiesWon);
	AddNumbers(TEXT("SUSTITUCIONES"),
		MatchManager->GetMatchSquadState(ESoccerTeam::PlayerTeam).SubstitutionHistory.Num(),
		MatchManager->GetMatchSquadState(ESoccerTeam::OpponentTeam).SubstitutionHistory.Num());

    // Team totals are the sums of confirmed individual events, not estimates.
    int32 PassAttempts5H[2] = {0, 0};
    int32 PassCompleted5H[2] = {0, 0};
    int32 Recoveries5H[2] = {0, 0};
    int32 Interceptions5H[2] = {0, 0};
    int32 Tackles5H[2] = {0, 0};
    for (const FSoccerPlayerMatchStatistics5G& Player : MatchManager->GetPlayerStatistics5G())
    {
        const int32 Index = Player.Team == ESoccerTeam::PlayerTeam ? 0 : 1;
        PassAttempts5H[Index] += Player.PassesAttempted;
        PassCompleted5H[Index] += Player.PassesCompleted;
        Recoveries5H[Index] += Player.Recoveries;
        Interceptions5H[Index] += Player.Interceptions;
        Tackles5H[Index] += Player.CleanTackles;
    }
    AddNumbers(TEXT("PASES INTENTADOS"), PassAttempts5H[0], PassAttempts5H[1]);
    AddNumbers(TEXT("PASES COMPLETADOS"), PassCompleted5H[0], PassCompleted5H[1]);
    AddNumbers(TEXT("RECUPERACIONES"), Recoveries5H[0], Recoveries5H[1]);
    AddNumbers(TEXT("INTERCEPCIONES"), Interceptions5H[0], Interceptions5H[1]);
    AddNumbers(TEXT("ENTRADAS LIMPIAS"), Tackles5H[0], Tackles5H[1]);
	StatisticsScroll->ScrollToStart();
}

void USoccerPeriodSummaryWidget::ChangeSummaryTab5G(bool bPlayers)
{
    if (bShowingPlayers5G == bPlayers && !bShowingTimeline5I && !bShowingReport5K) return;
    bShowingTimeline5I = false;
    bShowingReport5K = false;
    bShowingPlayers5G = bPlayers;
    RefreshStatistics5F();
}

void USoccerPeriodSummaryWidget::ShowTimeline5I()
{
    if (bShowingTimeline5I) return;
    bShowingPlayers5G = false;
    bShowingReport5K = false;
    bShowingTimeline5I = true;
    RefreshStatistics5F();
}

void USoccerPeriodSummaryWidget::ShowFinalReport5K()
{
    if (!IsValid(MatchManager) ||
        MatchManager->GetCurrentMatchPeriod() != ESoccerMatchPeriod::FullTime ||
        bShowingReport5K)
    {
        return;
    }
    bShowingReport5K = true;
    bShowingTimeline5I = false;
    bShowingPlayers5G = false;
    RefreshStatistics5F();
}

void USoccerPeriodSummaryWidget::RefreshTimeline5I()
{
    if (!IsValid(MatchManager) || !IsValid(StatisticsScroll) || !WidgetTree) return;
    using namespace SoccerPeriodSummary5F;
    StatisticsScroll->ClearChildren();

    const TArray<FSoccerMatchTimelineEvent5I>& Events = MatchManager->GetMatchTimeline5I();
    if (Events.Num() == 0)
    {
        StatisticsScroll->AddChild(NewText(WidgetTree,
            TEXT("Todavía no hay acontecimientos registrados."), 16, Muted));
        return;
    }

    StatisticsScroll->AddChild(NewText(WidgetTree,
        TEXT("ACONTECIMIENTOS EN ORDEN CRONOLÓGICO"), 13, Muted));

    for (int32 EventIndex5I = 0; EventIndex5I < Events.Num(); ++EventIndex5I)
    {
        const FSoccerMatchTimelineEvent5I& Event5I = Events[EventIndex5I];
        const int32 Seconds5I = FMath::Max(0, FMath::FloorToInt(Event5I.MatchSeconds));
        const TCHAR* PeriodLabel5I = TEXT("1T");
        if (Event5I.Period == ESoccerMatchPeriod::SecondHalf) PeriodLabel5I = TEXT("2T");
        else if (Event5I.Period == ESoccerMatchPeriod::HalfTime) PeriodLabel5I = TEXT("ET");
        else if (Event5I.Period == ESoccerMatchPeriod::FullTime) PeriodLabel5I = TEXT("FT");
        const FString Clock5I = FString::Printf(TEXT("%s %02d:%02d"),
            PeriodLabel5I, Seconds5I / 60, Seconds5I % 60);
        const FString TeamName5I = Event5I.bHasTeam
            ? GetClubName(MatchManager, Event5I.Team) : TEXT("PARTIDO");
        const FLinearColor TeamColor5I = !Event5I.bHasTeam ? Muted
            : Event5I.Team == ESoccerTeam::PlayerTeam ? Green : Cyan;

        UBorder* Background5I = WidgetTree->ConstructWidget<UBorder>();
        Background5I->SetBrushColor(EventIndex5I % 2 == 0
            ? FLinearColor(0.065f, 0.115f, 0.145f, 1.0f)
            : FLinearColor(0.042f, 0.085f, 0.110f, 1.0f));
        Background5I->SetPadding(FMargin(10.0f, 7.0f));
        UHorizontalBox* Row5I = WidgetTree->ConstructWidget<UHorizontalBox>();
        Background5I->SetContent(Row5I);
        AddColumn(Row5I, NewText(WidgetTree, Clock5I, 14, Muted), 1.25f);
        AddColumn(Row5I, NewText(WidgetTree, TeamName5I, 14, TeamColor5I), 1.8f);

        UVerticalBox* DescriptionBox5I = WidgetTree->ConstructWidget<UVerticalBox>();
        AddVertical(DescriptionBox5I, NewText(WidgetTree,
            Event5I.Title, 16, White, ETextJustify::Left), FMargin(0.0f));
        if (!Event5I.Detail.IsEmpty())
        {
            AddVertical(DescriptionBox5I, NewText(WidgetTree,
                Event5I.Detail, 12, Muted, ETextJustify::Left), FMargin(0.0f));
        }
        AddColumn(Row5I, DescriptionBox5I, 4.3f);
        StatisticsScroll->AddChild(Background5I);
    }
    StatisticsScroll->ScrollToStart();
}

void USoccerPeriodSummaryWidget::RefreshFinalReport5K()
{
    if (!WidgetTree || !StatisticsScroll || !IsValid(MatchManager)) return;
    using namespace SoccerPeriodSummary5F;

    FSoccerFinalMatchReport5K Report5K;
    if (!MatchManager->BuildFinalMatchReport5K(Report5K)) return;
    StatisticsScroll->ClearChildren();

    const auto AddSection5K = [this](const FString& Text5K)
    {
        UBorder* Header5K = WidgetTree->ConstructWidget<UBorder>();
        Header5K->SetBrushColor(FLinearColor(0.045f, 0.16f, 0.16f, 1.0f));
        Header5K->SetPadding(FMargin(10.0f, 7.0f));
        Header5K->SetContent(SoccerPeriodSummary5F::NewText(
            WidgetTree, Text5K, 17, SoccerPeriodSummary5F::Green,
            ETextJustify::Left));
        StatisticsScroll->AddChild(Header5K);
    };
    const auto AddDescription5K = [this](const FString& Text5K,
        const FLinearColor& Color5K, int32 Size5K)
    {
        UBorder* Line5K = WidgetTree->ConstructWidget<UBorder>();
        // UBorder's default brush is WHITE in native UMG. Without an explicit
        // brush the bright report text becomes almost unreadable (stage 5K).
        Line5K->SetBrushColor(FLinearColor(0.045f, 0.105f, 0.135f, 1.0f));
        Line5K->SetPadding(FMargin(12.0f, 7.0f));
        Line5K->SetContent(SoccerPeriodSummary5F::NewText(
            WidgetTree, Text5K, Size5K, Color5K, ETextJustify::Left));
        StatisticsScroll->AddChild(Line5K);
    };
    const auto FormatTime5K = [](const FSoccerMatchTimelineEvent5I& Event5K)
    {
        const int32 Seconds5K = FMath::Max(0, FMath::FloorToInt(Event5K.MatchSeconds));
        const TCHAR* Half5K = Event5K.Period == ESoccerMatchPeriod::SecondHalf
            ? TEXT("2T") : TEXT("1T");
        return FString::Printf(TEXT("%s %02d:%02d"),
            Half5K, Seconds5K / 60, Seconds5K % 60);
    };

    // The result stays visible in the permanent header; this section is an
    // additional self-contained summary for a future saved match record.
    AddSection5K(TEXT("INFORME FINAL DEL ENCUENTRO"));
    const FString Outcome5K = Report5K.LocalScore > Report5K.RivalScore
        ? FString::Printf(TEXT("GANÓ %s"), *Report5K.LocalClubName)
        : Report5K.RivalScore > Report5K.LocalScore
            ? FString::Printf(TEXT("GANÓ %s"), *Report5K.RivalClubName)
            : TEXT("EMPATE");
    AddDescription5K(FString::Printf(TEXT("%s  %d - %d  %s  |  %s"),
        *Report5K.LocalClubName, Report5K.LocalScore,
        Report5K.RivalScore, *Report5K.RivalClubName, *Outcome5K),
        White, 18);
    if (Report5K.bHasBestPlayer)
    {
        const FString& BestClub5K =
            Report5K.BestPlayerTeam == ESoccerTeam::PlayerTeam
            ? Report5K.LocalClubName : Report5K.RivalClubName;
        AddDescription5K(FString::Printf(
            TEXT("JUGADOR DEL PARTIDO: %s (%s)  |  NOTA %.1f"),
            *Report5K.BestPlayerName, *BestClub5K,
            Report5K.BestPlayerRating), Cyan, 16);
    }
    else
    {
        AddDescription5K(TEXT("JUGADOR DEL PARTIDO: sin acciones individuales suficientes"),
            Muted, 15);
    }

    AddSection5K(TEXT("GOLES DEL PARTIDO"));
    int32 GoalCount5K = 0;
    for (const FSoccerMatchTimelineEvent5I& Event5K : Report5K.Events)
    {
        if (Event5K.Title != TEXT("¡GOL!")) continue;
        const FString& Club5K = Event5K.Team == ESoccerTeam::PlayerTeam
            ? Report5K.LocalClubName : Report5K.RivalClubName;
        AddDescription5K(FString::Printf(TEXT("%s  |  %s  |  %s"),
            *FormatTime5K(Event5K), *Club5K, *Event5K.Detail), White, 14);
        ++GoalCount5K;
    }
    if (GoalCount5K == 0)
    {
        AddDescription5K(TEXT("No hubo goles."), Muted, 14);
    }

    AddSection5K(TEXT("ESTADÍSTICAS PRINCIPALES"));
    const FSoccerMatchTeamStatistics5D& Home5K =
        Report5K.TeamStatistics.PlayerTeam;
    const FSoccerMatchTeamStatistics5D& Away5K =
        Report5K.TeamStatistics.OpponentTeam;
    const float Controlled5K = Home5K.ControlledPossessionSeconds +
        Away5K.ControlledPossessionSeconds;
    if (Controlled5K > KINDA_SMALL_NUMBER)
    {
        const int32 HomePercent5K = FMath::Clamp(FMath::RoundToInt(
            100.0f * Home5K.ControlledPossessionSeconds / Controlled5K), 0, 100);
        AddStatisticRow5F(TEXT("POSESIÓN CONTROLADA"),
            FString::Printf(TEXT("%d%%"), HomePercent5K),
            FString::Printf(TEXT("%d%%"), 100 - HomePercent5K), 0);
    }
    else
    {
        AddStatisticRow5F(TEXT("POSESIÓN CONTROLADA"),
            TEXT("--"), TEXT("--"), 0);
    }
    AddStatisticRow5F(TEXT("REMATES"), FString::FromInt(Home5K.Shots),
        FString::FromInt(Away5K.Shots), 1);
    AddStatisticRow5F(TEXT("AL ARCO"), FString::FromInt(Home5K.ShotsOnTarget),
        FString::FromInt(Away5K.ShotsOnTarget), 2);
    AddStatisticRow5F(TEXT("ATAJADAS"), FString::FromInt(Home5K.GoalkeeperSaves),
        FString::FromInt(Away5K.GoalkeeperSaves), 3);
    AddStatisticRow5F(TEXT("CÓRNERS"), FString::FromInt(Home5K.CornersWon),
        FString::FromInt(Away5K.CornersWon), 4);
    int32 PassesHome5K = 0;
    int32 PassesAway5K = 0;
    for (const FSoccerPlayerMatchStatistics5G& Player5K : Report5K.Players)
    {
        if (Player5K.Team == ESoccerTeam::PlayerTeam)
            PassesHome5K += Player5K.PassesCompleted;
        else
            PassesAway5K += Player5K.PassesCompleted;
    }
    AddStatisticRow5F(TEXT("PASES COMPLETADOS"),
        FString::FromInt(PassesHome5K), FString::FromInt(PassesAway5K), 5);

    AddSection5K(TEXT("MOMENTOS RELEVANTES"));
    int32 HighlightCount5K = 0;
    for (const FSoccerMatchTimelineEvent5I& Event5K : Report5K.Events)
    {
        // All goals are shown above. Keep a brief selection of referee and
        // substitution events here; the CRONOLOGÍA tab keeps the full list.
        if (Event5K.Title != TEXT("PENAL") &&
            Event5K.Title != TEXT("SUSTITUCIÓN") &&
            Event5K.Title != TEXT("FUERA DE JUEGO"))
            continue;
        if (HighlightCount5K >= 7) break;
        const FString Club5K = Event5K.bHasTeam
            ? (Event5K.Team == ESoccerTeam::PlayerTeam
                ? Report5K.LocalClubName : Report5K.RivalClubName)
            : TEXT("PARTIDO");
        AddDescription5K(FString::Printf(TEXT("%s  |  %s  |  %s: %s"),
            *FormatTime5K(Event5K), *Club5K, *Event5K.Title,
            *Event5K.Detail), Muted, 13);
        ++HighlightCount5K;
    }
    if (HighlightCount5K == 0)
    {
        AddDescription5K(TEXT("Sin otros acontecimientos destacados."), Muted, 14);
    }
    AddDescription5K(TEXT("La cronología completa y todas las estadísticas están en las otras pestañas."),
        Muted, 12);
    AddDescription5K(TEXT("Informe calculado a partir de eventos confirmados. La nota es estimada. El encuentro se guarda en el historial al finalizar."),
        Muted, 12);
    StatisticsScroll->ScrollToStart();
}

void USoccerPeriodSummaryWidget::ChangeIndividualPage5H(bool bDefensive)
{
    if (bShowingDefense5H == bDefensive || !bShowingPlayers5G) return;
    bShowingDefense5H = bDefensive;
    RefreshStatistics5F();
}

void USoccerPeriodSummaryWidget::ShowAttackIndividual5H()
{
    ChangeIndividualPage5H(false);
}

void USoccerPeriodSummaryWidget::ShowDefenseIndividual5H()
{
    ChangeIndividualPage5H(true);
}

void USoccerPeriodSummaryWidget::ShowTeamStatistics5G()
{
    ChangeSummaryTab5G(false);
}

void USoccerPeriodSummaryWidget::ShowPlayerStatistics5G()
{
    ChangeSummaryTab5G(true);
}

void USoccerPeriodSummaryWidget::AddPlayerStatisticsRow5G(
    const FSoccerPlayerMatchStatistics5G& PlayerStats, int32 RowIndex,
    bool bBestPlayer5J)
{
    if (!WidgetTree || !StatisticsScroll) return;
    using namespace SoccerPeriodSummary5F;

    UBorder* RowBackground = WidgetTree->ConstructWidget<UBorder>();
    RowBackground->SetBrushColor(bBestPlayer5J
        ? FLinearColor(0.06f, 0.20f, 0.17f, 1.0f)
        : RowIndex % 2 == 0
            ? FLinearColor(0.065f, 0.115f, 0.145f, 1.0f)
            : FLinearColor(0.042f, 0.085f, 0.110f, 1.0f));
    RowBackground->SetPadding(FMargin(PlayerRowHorizontalPadding5G, 7.0f));
    UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
    RowBackground->SetContent(Row);
    FString PlayerLabel = PlayerStats.ShirtNumber > 0
        ? FString::Printf(TEXT("%d · %s"), PlayerStats.ShirtNumber,
            *PlayerStats.DisplayName)
        : PlayerStats.DisplayName;
    AddColumn(Row, NewText(WidgetTree, PlayerLabel, 16, White,
        ETextJustify::Left), PlayerNameWidth5G);
    const int32 PassingPercent5H = PlayerStats.PassesAttempted > 0
        ? FMath::Clamp(FMath::RoundToInt(100.0f * PlayerStats.PassesCompleted /
            PlayerStats.PassesAttempted), 0, 100)
        : 0;
    const int32 OffensiveValues5H[] = {
        PlayerStats.Goals, PlayerStats.Shots, PlayerStats.ShotsOnTarget,
        PlayerStats.Assists, PlayerStats.Saves, PlayerStats.OwnGoals
    };
    const int32 DefensiveValues5H[] = {
        PlayerStats.PassesAttempted, PlayerStats.PassesCompleted, PassingPercent5H,
        PlayerStats.Recoveries, PlayerStats.Interceptions, PlayerStats.CleanTackles
    };
    const int32* NumericValues = bShowingDefense5H
        ? DefensiveValues5H : OffensiveValues5H;
    for (int32 NumericIndex5H = 0; NumericIndex5H < 6; ++NumericIndex5H)
    {
        const int32 Value = NumericValues[NumericIndex5H];
        AddColumn(Row, NewText(WidgetTree, FString::FromInt(Value), 16,
            Value > 0 ? Green : Muted), PlayerStatWidth5G);
    }
    const float Rating5J = ASoccerMatchManager::CalculatePlayerRating5J(PlayerStats);
    const FLinearColor RatingColor5J = bBestPlayer5J || Rating5J >= 7.5f
        ? Green : Rating5J < 5.5f ? Muted : White;
    AddColumn(Row, NewText(WidgetTree,
        FString::Printf(TEXT("%.1f"), Rating5J), 17, RatingColor5J),
        PlayerRatingWidth5J);
    StatisticsScroll->AddChild(RowBackground);
}

void USoccerPeriodSummaryWidget::RefreshPlayerStatistics5G()
{
    if (!IsValid(MatchManager) || !IsValid(StatisticsScroll)) return;
    using namespace SoccerPeriodSummary5F;
    StatisticsScroll->ClearChildren();

    TArray<FSoccerPlayerMatchStatistics5G> Players = MatchManager->GetPlayerStatistics5G();
    Players.Sort([](const FSoccerPlayerMatchStatistics5G& A,
                    const FSoccerPlayerMatchStatistics5G& B)
    {
        if (A.Team != B.Team)
            return A.Team == ESoccerTeam::PlayerTeam;
        const float RatingA5J = ASoccerMatchManager::CalculatePlayerRating5J(A);
        const float RatingB5J = ASoccerMatchManager::CalculatePlayerRating5J(B);
        if (RatingA5J != RatingB5J) return RatingA5J > RatingB5J;
        if (A.Goals != B.Goals) return A.Goals > B.Goals;
        if (A.Assists != B.Assists) return A.Assists > B.Assists;
        if (A.ShotsOnTarget != B.ShotsOnTarget)
            return A.ShotsOnTarget > B.ShotsOnTarget;
        if (A.Interceptions != B.Interceptions)
            return A.Interceptions > B.Interceptions;
        if (A.Recoveries != B.Recoveries)
            return A.Recoveries > B.Recoveries;
        return A.DisplayName < B.DisplayName;
    });

    if (Players.Num() == 0)
    {
        UTextBlock* EmptyLabel = NewText(WidgetTree,
            TEXT("Todavía no hay acciones individuales registradas."), 18, Muted);
        StatisticsScroll->AddChild(EmptyLabel);
        return;
    }

    const FSoccerPlayerMatchStatistics5G* TopScorer = nullptr;
    const FSoccerPlayerMatchStatistics5G* BestPlayer5J = nullptr;
    float BestRating5J = -1.0f;
    int32 BestImpact5J = MIN_int32;
    for (const FSoccerPlayerMatchStatistics5G& Player : Players)
    {
        if (Player.Goals > 0 && (!TopScorer || Player.Goals > TopScorer->Goals))
            TopScorer = &Player;
        const float Rating5J = ASoccerMatchManager::CalculatePlayerRating5J(Player);
        // Break equal tenths by confirmed impact, independently of team.
        // The final identity comparison keeps the result deterministic when
        // two players have exactly the same rating AND contribution.
        const int32 Impact5J = Player.Goals * 8 + Player.Assists * 5 +
            Player.Saves * 4 + Player.Interceptions * 3 +
            Player.CleanTackles * 3 + Player.Recoveries * 2 +
            Player.ShotsOnTarget + Player.PassesCompleted / 3 -
            Player.OwnGoals * 7;
        if (Rating5J > BestRating5J ||
            (Rating5J == BestRating5J && Impact5J > BestImpact5J) ||
            (Rating5J == BestRating5J && Impact5J == BestImpact5J &&
                (!BestPlayer5J || Player.PlayerKey < BestPlayer5J->PlayerKey)))
        {
            BestRating5J = Rating5J;
            BestImpact5J = Impact5J;
            BestPlayer5J = &Player;
        }
    }
    const bool bFullTime5J =
        MatchManager->GetCurrentMatchPeriod() == ESoccerMatchPeriod::FullTime;
    const TCHAR* BestLabel5J = bFullTime5J
        ? TEXT("JUGADOR DEL PARTIDO") : TEXT("DESTACADO DEL PRIMER TIEMPO");
    UTextBlock* Highlights = NewText(WidgetTree,
        FString::Printf(TEXT("GOLEADOR: %s    |    %s: %s (%.1f)"),
            TopScorer ? *TopScorer->DisplayName : TEXT("Sin datos"),
            BestLabel5J,
            BestPlayer5J ? *BestPlayer5J->DisplayName : TEXT("Sin datos"),
            BestPlayer5J ? BestRating5J : 0.0f), 15, Cyan);
    StatisticsScroll->AddChild(Highlights);
    StatisticsScroll->AddChild(NewText(WidgetTree,
        TEXT("NOTA: estimación de 1,0 a 10,0 según acciones registradas; no incluye minutos ni acciones sin seguimiento."),
        12, Muted));

    UTextBlock* Legend = NewText(WidgetTree,
        bShowingDefense5H
            ? TEXT("P: pases intentados  ·  PC: completados  ·  %: precisión  ·  REC: recuperaciones  ·  INT: intercepciones  ·  ENT: entradas limpias")
            : TEXT("G: goles  ·  R: remates  ·  AL: al arco  ·  AS: asistencias  ·  AT: atajadas  ·  AG: autogoles"),
        13, Muted);
    StatisticsScroll->AddChild(Legend);

    for (int32 TeamIndex = 0; TeamIndex < 2; ++TeamIndex)
    {
        const ESoccerTeam Team = TeamIndex == 0
            ? ESoccerTeam::PlayerTeam : ESoccerTeam::OpponentTeam;
        // Use the very same horizontal-box grid as each player row.
        // The previous single text string with spaces could never line up
        // with the proportional columns of the numeric values.
        UBorder* ClubHeaderBackground = WidgetTree->ConstructWidget<UBorder>();
        ClubHeaderBackground->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
        ClubHeaderBackground->SetPadding(FMargin(PlayerRowHorizontalPadding5G, 5.0f));
        UHorizontalBox* ClubHeaderRow = WidgetTree->ConstructWidget<UHorizontalBox>();
        ClubHeaderBackground->SetContent(ClubHeaderRow);
        AddColumn(ClubHeaderRow, NewText(WidgetTree,
            GetClubName(MatchManager, Team), 17, Green, ETextJustify::Left),
            PlayerNameWidth5G);
        const TCHAR* AttackCaptions5H[] = {
            TEXT("G"), TEXT("R"), TEXT("AL"), TEXT("AS"), TEXT("AT"), TEXT("AG")
        };
        const TCHAR* DefenseCaptions5H[] = {
            TEXT("P"), TEXT("PC"), TEXT("%"), TEXT("REC"), TEXT("INT"), TEXT("ENT")
        };
        const TCHAR* const* ColumnCaptions = bShowingDefense5H
            ? DefenseCaptions5H : AttackCaptions5H;
        for (int32 ColumnIndex5H = 0; ColumnIndex5H < 6; ++ColumnIndex5H)
        {
            const TCHAR* Caption = ColumnCaptions[ColumnIndex5H];
            AddColumn(ClubHeaderRow, NewText(WidgetTree, Caption, 16, Green),
                PlayerStatWidth5G);
        }
        // Exact same width in heading and row: keeps NOTA aligned at 720p.
        AddColumn(ClubHeaderRow, NewText(WidgetTree, TEXT("NOTA"), 15, Cyan),
            PlayerRatingWidth5J);
        StatisticsScroll->AddChild(ClubHeaderBackground);
        int32 RowIndex = 0;
        for (const FSoccerPlayerMatchStatistics5G& Player : Players)
        {
            if (Player.Team != Team) continue;
            const bool bBestPlayer5J = BestPlayer5J &&
                Player.PlayerKey == BestPlayer5J->PlayerKey;
            AddPlayerStatisticsRow5G(Player, RowIndex++, bBestPlayer5J);
        }
        if (RowIndex == 0)
        {
            StatisticsScroll->AddChild(NewText(WidgetTree,
                TEXT("Sin acciones registradas para este club."), 14, Muted));
        }
    }
    StatisticsScroll->ScrollToStart();
}

void USoccerPeriodSummaryWidget::AddStatisticRow5F(
	const FString& Label, const FString& LocalValue,
	const FString& RivalValue, int32 RowIndex)
{
	if (!WidgetTree || !StatisticsScroll) return;
	using namespace SoccerPeriodSummary5F;

	UBorder* RowBackground = WidgetTree->ConstructWidget<UBorder>();
	RowBackground->SetBrushColor(RowIndex % 2 == 0
		? FLinearColor(0.065f, 0.115f, 0.145f, 1.0f)
		: FLinearColor(0.042f, 0.085f, 0.110f, 1.0f));
	RowBackground->SetPadding(FMargin(14.0f, 7.0f));
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	RowBackground->SetContent(Row);
	AddColumn(Row, NewText(WidgetTree, LocalValue, 20, White), 1.0f);
	AddColumn(Row, NewText(WidgetTree, Label, 17, Muted), 3.0f);
	AddColumn(Row, NewText(WidgetTree, RivalValue, 20, White), 1.0f);
	StatisticsScroll->AddChild(RowBackground);
}

void USoccerPeriodSummaryWidget::RefreshContinueHint5F()
{
	if (!IsValid(MatchManager) || !IsValid(StatusText)) return;
	const bool bFinal = MatchManager->GetCurrentMatchPeriod() == ESoccerMatchPeriod::FullTime;
    if (IsValid(PlayAgainButton5M))
        PlayAgainButton5M->SetVisibility(bFinal ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    // At full time this is a secondary CLOSE action, not a PLAY action.
    ContinueButton->SetBackgroundColor(bFinal
        ? FLinearColor(0.15f, 0.31f, 0.39f, 1.0f)
        : FLinearColor(0.32f, 0.82f, 0.52f, 1.0f));
    ContinueLabel->SetColorAndOpacity(FSlateColor(bFinal
        ? FLinearColor::White
        : FLinearColor(0.018f, 0.13f, 0.085f, 1.0f)));
	if (bFinal)
	{
		ContinueLabel->SetText(FText::FromString(TEXT("CERRAR RESUMEN")));
		StatusText->SetText(FText::FromString(TEXT("Enter / A para cerrar  ·  El partido ya terminó")));
		return;
	}
	if (MatchManager->IsHalfTimeContinueRequested5F())
	{
		ContinueLabel->SetText(FText::FromString(TEXT("PREPARANDO SEGUNDO TIEMPO...")));
		StatusText->SetText(FText::FromString(TEXT("Esperando a que termine el cambio de campo")));
	}
	else
	{
		ContinueLabel->SetText(FText::FromString(TEXT("CONTINUAR AL SEGUNDO TIEMPO")));
		StatusText->SetText(FText::FromString(TEXT("Enter / A para continuar cuando estés listo")));
	}
}

void USoccerPeriodSummaryWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshContinueHint5F();
}

void USoccerPeriodSummaryWidget::HandlePlayAgain5M()
{
    if (!IsValid(MatchManager) ||
        MatchManager->GetCurrentMatchPeriod() != ESoccerMatchPeriod::FullTime)
        return;
    if (APlayerController* PlayerController = GetOwningPlayer())
    {
        if (AGameHUD* Hud5M = Cast<AGameHUD>(PlayerController->GetHUD()))
            Hud5M->StartNewMatch5M();
    }
}

void USoccerPeriodSummaryWidget::HandleContinueClicked5F()
{
	if (!IsValid(MatchManager)) return;
	if (MatchManager->GetCurrentMatchPeriod() == ESoccerMatchPeriod::HalfTime)
	{
		MatchManager->RequestHalfTimeContinue5F();
		RefreshContinueHint5F();
	}
	else if (MatchManager->GetCurrentMatchPeriod() == ESoccerMatchPeriod::FullTime)
	{
		APlayerController* PlayerController = GetOwningPlayer();
		if (PlayerController)
		{
			if (AGameHUD* GameHUD = Cast<AGameHUD>(PlayerController->GetHUD()))
			{
				GameHUD->DismissFinalSummary5F();
			}
		}
	}
}

FReply USoccerPeriodSummaryWidget::NativeOnPreviewKeyDown(
	const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
    if (Key == EKeys::One || Key == EKeys::Gamepad_LeftShoulder)
    {
        ChangeSummaryTab5G(false);
        return FReply::Handled();
    }
    if (Key == EKeys::Two || Key == EKeys::Gamepad_RightShoulder)
    {
        ChangeSummaryTab5G(true);
        return FReply::Handled();
    }
    if (Key == EKeys::Five || Key == EKeys::Gamepad_FaceButton_Top)
    {
        ShowTimeline5I();
        return FReply::Handled();
    }
    if (Key == EKeys::Six || Key == EKeys::Gamepad_FaceButton_Left)
    {
        ShowFinalReport5K();
        return FReply::Handled();
    }
    if (bShowingPlayers5G &&
        (Key == EKeys::Three || Key == EKeys::Gamepad_DPad_Left))
    {
        ChangeIndividualPage5H(false);
        return FReply::Handled();
    }
    if (bShowingPlayers5G &&
        (Key == EKeys::Four || Key == EKeys::Gamepad_DPad_Right))
    {
        ChangeIndividualPage5H(true);
        return FReply::Handled();
    }
    if (Key == EKeys::J && IsValid(MatchManager) &&
        MatchManager->GetCurrentMatchPeriod() == ESoccerMatchPeriod::FullTime)
    {
        HandlePlayAgain5M();
        return FReply::Handled();
    }
	if (Key == EKeys::Enter || Key == EKeys::SpaceBar ||
		Key == EKeys::Gamepad_FaceButton_Bottom)
	{
		HandleContinueClicked5F();
		return FReply::Handled();
	}
	if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right)
	{
		// Never allow B/Escape to bypass the halftime field transition.
		if (IsValid(MatchManager) &&
			MatchManager->GetCurrentMatchPeriod() == ESoccerMatchPeriod::FullTime)
		{
			HandleContinueClicked5F();
		}
		return FReply::Handled();
	}
	if (Key == EKeys::F11)
	{
		if (APlayerController* PlayerController = GetOwningPlayer())
		{
			PlayerController->ConsoleCommand(TEXT("fullscreen"), true);
		}
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}
