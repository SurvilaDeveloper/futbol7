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

	UTextBlock* Header = NewText(WidgetTree, TEXT("LOCAL                            ESTADÍSTICA                           RIVAL"),
		13, Muted);
	AddVertical(Content, Header, FMargin(0.0f, 0.0f, 0.0f, 8.0f));

	USizeBox* ScrollHeight = WidgetTree->ConstructWidget<USizeBox>();
	ScrollHeight->SetHeightOverride(335.0f);
	StatisticsScroll = WidgetTree->ConstructWidget<UScrollBox>();
	StatisticsScroll->SetScrollBarVisibility(ESlateVisibility::Visible);
	ScrollHeight->SetContent(StatisticsScroll);
	AddVertical(Content, ScrollHeight, FMargin(0.0f, 0.0f, 0.0f, 9.0f));

	StatusText = NewText(WidgetTree, TEXT(""), 14, Muted);
	AddVertical(Content, StatusText, FMargin(0.0f, 0.0f, 0.0f, 8.0f));

	ContinueButton = WidgetTree->ConstructWidget<UButton>();
	ContinueButton->SetBackgroundColor(Green);
	ContinueLabel = NewText(WidgetTree, TEXT("CONTINUAR AL SEGUNDO TIEMPO"), 20, White);
	ContinueButton->AddChild(ContinueLabel);
	ContinueButton->OnClicked.AddDynamic(this, &USoccerPeriodSummaryWidget::HandleContinueClicked5F);
	AddVertical(Content, ContinueButton, FMargin(95.0f, 0.0f, 95.0f, 0.0f));
}

void USoccerPeriodSummaryWidget::InitializeForMatch(ASoccerMatchManager* InMatchManager)
{
	MatchManager = InMatchManager;
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
