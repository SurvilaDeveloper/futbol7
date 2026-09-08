//GameHUD.cpp
#include "GameHUD.h"

#include "ThirdPersonCppCharacter.h"
#include "Engine/Canvas.h"
#include "Kismet/GameplayStatics.h"

#include "SoccerMatchManager.h"
#include "SoccerGameInstance.h"
#include "SoccerInstantReplayManager.h"
#include "SoccerPlayerProfile.h"
#include "SoccerTeamTypes.h"

#include "SoccerDebugManager.h"
#include "SoccerClubSelectionWidget.h"
#include "SoccerFormationMenuWidget.h"
#include "SoccerQuickTacticsWidget.h"
#include "SoccerSubstitutionMenuWidget.h"
#include "SoccerTacticalPresetManager.h"

#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "HAL/PlatformTime.h"


void AGameHUD::BeginPlay()
{
	Super::BeginPlay();

	FindMatchManager();
	EnsureTacticalPresetManager();

	FTimerDelegate OpenMenuDelegate;
	OpenMenuDelegate.BindUObject(this, &AGameHUD::OpenInitialMatchMenu);
	GetWorldTimerManager().SetTimerForNextTick(OpenMenuDelegate);
}

void AGameHUD::OpenInitialMatchMenu()
{
	if (!IsValid(MatchManager))
	{
		FindMatchManager();
	}

	if (!IsValid(MatchManager))
	{
		UE_LOG(LogTemp, Warning, TEXT("[ClubSelection] SoccerMatchManager was not found."));
		return;
	}

	if (MatchManager->ShouldShowClubSelectionAtMatchStart())
	{
		ShowClubSelectionMenu();
		return;
	}

	if (MatchManager->ShouldShowFormationMenuAtMatchStart())
	{
		ShowFormationMenu();
	}
}

void AGameHUD::ShowClubSelectionMenu()
{
	if (!IsValid(MatchManager))
	{
		FindMatchManager();
	}

	APlayerController* PlayerController =
		UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!IsValid(MatchManager) || PlayerController == nullptr)
	{
		return;
	}

	if (!IsValid(ClubSelectionWidget))
	{
		ClubSelectionWidget = CreateWidget<USoccerClubSelectionWidget>(
			PlayerController,
			USoccerClubSelectionWidget::StaticClass()
		);
	}

	if (!IsValid(ClubSelectionWidget))
	{
		return;
	}

	ClubSelectionWidget->InitializeForMatchManager(MatchManager);
	if (!ClubSelectionWidget->HasSelectableOpponent())
	{
		UE_LOG(LogTemp, Warning, TEXT("[ClubSelection] No selectable opponent club is available."));
		if (MatchManager->ShouldShowFormationMenuAtMatchStart())
		{
			ShowFormationMenu();
		}
		return;
	}

	ClubSelectionWidget->OnSelectionConfirmed.RemoveDynamic(
		this,
		&AGameHUD::HandleClubSelectionConfirmed
	);
	ClubSelectionWidget->OnSelectionConfirmed.AddDynamic(
		this,
		&AGameHUD::HandleClubSelectionConfirmed
	);

	if (!ClubSelectionWidget->IsInViewport())
	{
		ClubSelectionWidget->AddToViewport(130);
	}
	ClubSelectionWidget->ActivateMenu(true);
	UE_LOG(LogTemp, Display, TEXT("[ClubSelection] Initial opponent selection opened."));
}

bool AGameHUD::IsClubSelectionMenuVisible() const
{
	return IsValid(ClubSelectionWidget) && ClubSelectionWidget->IsInViewport();
}

void AGameHUD::HandleClubSelectionConfirmed()
{
	UE_LOG(LogTemp, Display, TEXT("[ClubSelection] Opponent confirmed; match materialized."));
	if (IsValid(MatchManager) && MatchManager->ShouldShowFormationMenuAtMatchStart())
	{
		FTimerDelegate OpenFormationDelegate;
		OpenFormationDelegate.BindUObject(this, &AGameHUD::ShowFormationMenu);
		GetWorldTimerManager().SetTimerForNextTick(OpenFormationDelegate);
	}
}

void AGameHUD::ShowFormationMenu()
{
	if (IsClubSelectionMenuVisible() || IsSubstitutionMenuVisible())
	{
		return;
	}
	if (!IsValid(MatchManager))
	{
		FindMatchManager();
	}

	if (!IsValid(MatchManager))
	{
		return;
	}

	EnsureTacticalPresetManager();

	APlayerController* PlayerController =
		UGameplayStatics::GetPlayerController(GetWorld(), 0);

	if (PlayerController == nullptr)
	{
		return;
	}

	if (!IsValid(FormationMenuWidget))
	{
		FormationMenuWidget = CreateWidget<USoccerFormationMenuWidget>(
			PlayerController,
			USoccerFormationMenuWidget::StaticClass()
		);
	}

	if (!IsValid(FormationMenuWidget))
	{
		return;
	}

	FormationMenuWidget->InitializeForMatchManager(
		MatchManager,
		TacticalPresetManager
	);

	if (!FormationMenuWidget->IsInViewport())
	{
		FormationMenuWidget->AddToViewport(100);
	}

	FormationMenuWidget->ActivateMenu(
		MatchManager->ShouldPauseGameWhileFormationMenuOpen()
	);
}

void AGameHUD::HideFormationMenu()
{
	if (IsValid(FormationMenuWidget) && FormationMenuWidget->IsInViewport())
	{
		FormationMenuWidget->CloseMenu();
	}
}

void AGameHUD::ToggleFormationMenu()
{
	if (IsFormationMenuVisible())
	{
		HideFormationMenu();
	}
	else
	{
		ShowFormationMenu();
	}
}

bool AGameHUD::IsFormationMenuVisible() const
{
	return
		IsValid(FormationMenuWidget) &&
		FormationMenuWidget->IsInViewport();
}

void AGameHUD::ShowQuickTacticsMenu()
{
	if (IsClubSelectionMenuVisible() || IsFormationMenuVisible() || IsSubstitutionMenuVisible())
	{
		return;
	}

	if (!IsValid(MatchManager))
	{
		FindMatchManager();
	}
	EnsureTacticalPresetManager();

	if (!IsValid(TacticalPresetManager))
	{
		return;
	}

	APlayerController* PlayerController =
		UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (PlayerController == nullptr)
	{
		return;
	}

	if (!IsValid(QuickTacticsWidget))
	{
		QuickTacticsWidget = CreateWidget<USoccerQuickTacticsWidget>(
			PlayerController,
			USoccerQuickTacticsWidget::StaticClass()
		);
	}

	if (!IsValid(QuickTacticsWidget))
	{
		return;
	}

	QuickTacticsWidget->InitializeForPresetManager(TacticalPresetManager);
	if (!QuickTacticsWidget->IsInViewport())
	{
		QuickTacticsWidget->AddToViewport(110);
	}
	QuickTacticsWidget->ActivateMenu();
}

void AGameHUD::HideQuickTacticsMenu()
{
	if (IsValid(QuickTacticsWidget) && QuickTacticsWidget->IsInViewport())
	{
		QuickTacticsWidget->CloseMenu();
	}
}

void AGameHUD::ToggleQuickTacticsMenu()
{
	if (IsQuickTacticsMenuVisible())
	{
		HideQuickTacticsMenu();
	}
	else
	{
		ShowQuickTacticsMenu();
	}
}

bool AGameHUD::IsQuickTacticsMenuVisible() const
{
	return
		IsValid(QuickTacticsWidget) &&
		QuickTacticsWidget->IsInViewport();
}

void AGameHUD::ShowSubstitutionMenu()
{
	if (IsClubSelectionMenuVisible() || IsFormationMenuVisible() || IsQuickTacticsMenuVisible())
	{
		return;
	}
	if (!IsValid(MatchManager))
	{
		FindMatchManager();
	}
	if (!IsValid(MatchManager))
	{
		return;
	}
	APlayerController* PlayerController =
		UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (PlayerController == nullptr)
	{
		return;
	}
	if (!IsValid(SubstitutionMenuWidget))
	{
		SubstitutionMenuWidget = CreateWidget<USoccerSubstitutionMenuWidget>(
			PlayerController,
			USoccerSubstitutionMenuWidget::StaticClass()
		);
	}
	if (!IsValid(SubstitutionMenuWidget))
	{
		return;
	}
	SubstitutionMenuWidget->InitializeForMatchManager(MatchManager);
	if (!SubstitutionMenuWidget->IsInViewport())
	{
		SubstitutionMenuWidget->AddToViewport(120);
	}
	SubstitutionMenuWidget->ActivateMenu(true);
}

void AGameHUD::HideSubstitutionMenu()
{
	if (IsValid(SubstitutionMenuWidget) && SubstitutionMenuWidget->IsInViewport())
	{
		SubstitutionMenuWidget->CloseMenu();
	}
}

void AGameHUD::ToggleSubstitutionMenu()
{
	if (IsSubstitutionMenuVisible())
	{
		HideSubstitutionMenu();
	}
	else
	{
		ShowSubstitutionMenu();
	}
}

bool AGameHUD::IsSubstitutionMenuVisible() const
{
	return IsValid(SubstitutionMenuWidget) && SubstitutionMenuWidget->IsInViewport();
}

void AGameHUD::ShowQuickTacticsFeedback(const FString& Message)
{
	QuickTacticsFeedbackText = Message;
	QuickTacticsFeedbackExpiryRealTime = FPlatformTime::Seconds() + 2.25;
}

void AGameHUD::EnsureTacticalPresetManager()
{
	if (!IsValid(MatchManager))
	{
		FindMatchManager();
	}

	if (!IsValid(MatchManager) || IsValid(TacticalPresetManager))
	{
		return;
	}

	TacticalPresetManager = NewObject<USoccerTacticalPresetManager>(this);
	if (IsValid(TacticalPresetManager))
	{
		TacticalPresetManager->Initialize(MatchManager);
	}
}

void AGameHUD::DrawQuickTacticsFeedback()
{
	if (
		Canvas == nullptr ||
		QuickTacticsFeedbackText.IsEmpty() ||
		FPlatformTime::Seconds() > QuickTacticsFeedbackExpiryRealTime
	)
	{
		return;
	}

	UFont* SmallFont = GEngine != nullptr ? GEngine->GetSmallFont() : nullptr;
	if (SmallFont == nullptr)
	{
		return;
	}

	const float TextScale = 1.15f;
	float TextWidth = 0.0f;
	float TextHeight = 0.0f;
	GetTextSize(
		QuickTacticsFeedbackText,
		TextWidth,
		TextHeight,
		SmallFont,
		TextScale
	);

	const float PaddingX = 16.0f;
	const float PaddingY = 9.0f;
	const float BoxWidth = TextWidth + PaddingX * 2.0f;
	const float BoxHeight = TextHeight + PaddingY * 2.0f;
	const float BoxX = FMath::Max(18.0f, Canvas->SizeX - BoxWidth - 28.0f);
	const float BoxY = FMath::Max(
		18.0f,
		Canvas->SizeY - BoxHeight - 70.0f
	);

	DrawRect(
		FLinearColor(0.02f, 0.12f, 0.08f, 0.86f),
		BoxX,
		BoxY,
		BoxWidth,
		BoxHeight
	);
	DrawText(
		QuickTacticsFeedbackText,
		FLinearColor(0.70f, 1.0f, 0.82f, 1.0f),
		BoxX + PaddingX,
		BoxY + PaddingY,
		SmallFont,
		TextScale
	);
}

void AGameHUD::DrawInstantReplayOverlay()
{
	if (Canvas == nullptr)
	{
		return;
	}

	ASoccerInstantReplayManager* ReplayManager =
		IsValid(MatchManager)
			? MatchManager->GetInstantReplayManager()
			: nullptr;

	if (!IsValid(ReplayManager) || !ReplayManager->IsReplayPlaying())
	{
		return;
	}

	UFont* ReplayFont =
		GEngine != nullptr ? GEngine->GetLargeFont() : nullptr;
	UFont* HintFont =
		GEngine != nullptr ? GEngine->GetSmallFont() : nullptr;

	if (ReplayFont == nullptr || HintFont == nullptr)
	{
		return;
	}

	const FString ReplayText = TEXT("REPLAY");
	const FString SkipHint = ReplayManager->GetSkipReplayInputHintText();

	const float ReplayScale = 1.15f;
	const float HintScale = 1.0f;
	const float PaddingX = 18.0f;
	const float PaddingY = 10.0f;
	const float Gap = 6.0f;

	float ReplayWidth = 0.0f;
	float ReplayHeight = 0.0f;
	GetTextSize(
		ReplayText,
		ReplayWidth,
		ReplayHeight,
		ReplayFont,
		ReplayScale
	);

	float HintWidth = 0.0f;
	float HintHeight = 0.0f;
	GetTextSize(
		SkipHint,
		HintWidth,
		HintHeight,
		HintFont,
		HintScale
	);

	const float BoxWidth =
		FMath::Max(ReplayWidth, HintWidth) + PaddingX * 2.0f;
	const float BoxHeight =
		ReplayHeight + HintHeight + Gap + PaddingY * 2.0f;

	const float BoxX =
		FMath::Max(24.0f, Canvas->SizeX - BoxWidth - 34.0f);
	const float BoxY = 28.0f;

	DrawRect(
		FLinearColor(0.0f, 0.0f, 0.0f, 0.72f),
		BoxX,
		BoxY,
		BoxWidth,
		BoxHeight
	);

	DrawText(
		ReplayText,
		FLinearColor::White,
		BoxX + PaddingX,
		BoxY + PaddingY,
		ReplayFont,
		ReplayScale
	);

	DrawText(
		SkipHint,
		FLinearColor(0.82f, 0.82f, 0.82f, 1.0f),
		BoxX + PaddingX,
		BoxY + PaddingY + ReplayHeight + Gap,
		HintFont,
		HintScale
	);
}

void AGameHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!IsValid(MatchManager))
	{
		FindMatchManager();
	}

	ASoccerInstantReplayManager* ReplayManager =
		IsValid(MatchManager)
			? MatchManager->GetInstantReplayManager()
			: nullptr;

	if (IsValid(ReplayManager) && ReplayManager->IsReplayPlaying())
	{
		DrawInstantReplayOverlay();
		return;
	}

	DrawMatchScoreboard();
	DrawMatchClock();
	DrawHumanPassRequestIndicator();
	DrawHumanJumpHeaderIndicator();
	DrawQuickTacticsFeedback();
	DrawSubstitutionPresentation();
	DrawSoccerDebugTextFeed();
	DrawSoccerDebugPanel();

	AThirdPersonCppCharacter* PlayerCharacter = Cast<AThirdPersonCppCharacter>(
		UGameplayStatics::GetPlayerCharacter(GetWorld(), 0)
	);

	if (PlayerCharacter == nullptr)
	{
		return;
	}

	const int32 Score = PlayerCharacter->GetScore();

	const FString ScoreText = FString::Printf(TEXT("Puntaje: %d"), Score);

	DrawText(
		ScoreText,
		FColor::White,
		50.0f,
		50.0f,
		nullptr,
		2.0f
	);

	float AimScreenX = 0.0f;
	float AimScreenY = 0.0f;

	if (PlayerCharacter->GetAimCursorScreenPosition(AimScreenX, AimScreenY))
	{
		const float CursorSize = 10.0f;

		DrawLine(
			AimScreenX - CursorSize * 6,
			AimScreenY,
			AimScreenX + CursorSize * 6,
			AimScreenY,
			FLinearColor::Green,
			2.0f
		);

		DrawLine(
			AimScreenX,
			AimScreenY - CursorSize,
			AimScreenX,
			AimScreenY + CursorSize,
			FLinearColor::Green,
			2.0f
		);
	}

	const float BarX = 50.0f;
	const float BarWidth = 260.0f;
	const float BarHeight = 14.0f;

	// ============================================================
	// Barra de energia del jugador
	// ============================================================

	const float EnergyPercent = PlayerCharacter->GetPlayerEnergyPercent();

	const float EnergyBarY = 105.0f;

	DrawText(
		TEXT("Energia"),
		FColor::White,
		BarX,
		EnergyBarY - 28.0f,
		nullptr,
		1.2f
	);

	DrawRect(
		FLinearColor(0.05f, 0.05f, 0.05f, 0.85f),
		BarX,
		EnergyBarY,
		BarWidth,
		BarHeight
	);

	DrawRect(
		FLinearColor(0.0f, 0.8f, 0.25f, 1.0f),
		BarX,
		EnergyBarY,
		BarWidth * EnergyPercent,
		BarHeight
	);

	DrawLine(
		BarX,
		EnergyBarY,
		BarX + BarWidth,
		EnergyBarY,
		FLinearColor::White,
		1.0f
	);

	DrawLine(
		BarX,
		EnergyBarY + BarHeight,
		BarX + BarWidth,
		EnergyBarY + BarHeight,
		FLinearColor::White,
		1.0f
	);

	DrawLine(
		BarX,
		EnergyBarY,
		BarX,
		EnergyBarY + BarHeight,
		FLinearColor::White,
		1.0f
	);

	DrawLine(
		BarX + BarWidth,
		EnergyBarY,
		BarX + BarWidth,
		EnergyBarY + BarHeight,
		FLinearColor::White,
		1.0f
	);

	const FString EnergyText = FString::Printf(
		TEXT("%d%%"),
		FMath::RoundToInt(EnergyPercent * 100.0f)
	);

	DrawText(
		EnergyText,
		FColor::White,
		BarX + BarWidth + 12.0f,
		EnergyBarY - 5.0f,
		nullptr,
		1.0f
	);

	// ============================================================
	// Barra de fuerza acumulada de patada release
	// ============================================================

	float KickChargePercent = 0.0f;

	if (PlayerCharacter->GetKickChargePercent(KickChargePercent))
	{
		const float KickBarY = 150.0f;

		DrawText(
			TEXT("Fuerza de disparo"),
			FColor::White,
			BarX,
			KickBarY - 28.0f,
			nullptr,
			1.2f
		);

		DrawRect(
			FLinearColor(0.05f, 0.05f, 0.05f, 0.85f),
			BarX,
			KickBarY,
			BarWidth,
			BarHeight
		);

		DrawRect(
			FLinearColor(1.0f, 0.7f, 0.0f, 1.0f),
			BarX,
			KickBarY,
			BarWidth * KickChargePercent,
			BarHeight
		);

		DrawLine(
			BarX,
			KickBarY,
			BarX + BarWidth,
			KickBarY,
			FLinearColor::White,
			1.0f
		);

		DrawLine(
			BarX,
			KickBarY + BarHeight,
			BarX + BarWidth,
			KickBarY + BarHeight,
			FLinearColor::White,
			1.0f
		);

		DrawLine(
			BarX,
			KickBarY,
			BarX,
			KickBarY + BarHeight,
			FLinearColor::White,
			1.0f
		);

		DrawLine(
			BarX + BarWidth,
			KickBarY,
			BarX + BarWidth,
			KickBarY + BarHeight,
			FLinearColor::White,
			1.0f
		);
	}
}

FString AGameHUD::ResolvePlayerDisplayName(FName PlayerId) const
{
	const USoccerGameInstance* SoccerGameInstance = GetWorld() != nullptr
		? Cast<USoccerGameInstance>(GetWorld()->GetGameInstance())
		: nullptr;
	const USoccerPlayerProfile* PlayerProfile = IsValid(SoccerGameInstance)
		? SoccerGameInstance->FindPlayerProfileById(PlayerId)
		: nullptr;
	if (IsValid(PlayerProfile) && !PlayerProfile->Identity.DisplayName.IsEmpty())
	{
		return PlayerProfile->Identity.DisplayName.ToString();
	}
	return PlayerId.IsNone() ? FString(TEXT("--")) : PlayerId.ToString();
}

void AGameHUD::DrawSubstitutionPresentation()
{
	if (Canvas == nullptr || !IsValid(MatchManager))
	{
		return;
	}

	const FSoccerMatchSquadState PlayerState =
		MatchManager->GetMatchSquadState(ESoccerTeam::PlayerTeam);
	const FSoccerMatchSquadState OpponentState =
		MatchManager->GetMatchSquadState(ESoccerTeam::OpponentTeam);
	if (!bSubstitutionHistoryObserved)
	{
		ObservedPlayerTeamSubstitutionCount = PlayerState.SubstitutionHistory.Num();
		ObservedOpponentTeamSubstitutionCount = OpponentState.SubstitutionHistory.Num();
		bSubstitutionHistoryObserved = true;
	}

	auto CaptureLatestExecution = [this](
		const FSoccerMatchSquadState& SquadState,
		int32& ObservedCount,
		const FString& TeamLabel
	)
	{
		const int32 CurrentCount = SquadState.SubstitutionHistory.Num();
		if (CurrentCount > ObservedCount && CurrentCount > 0)
		{
			const FSoccerMatchSubstitutionRecord& Record =
				SquadState.SubstitutionHistory.Last();
			SubstitutionPresentationText = FString::Printf(
				TEXT("CAMBIO — %s\nSALE  %s     ENTRA  %s"),
				*TeamLabel,
				*ResolvePlayerDisplayName(Record.OutgoingPlayerId),
				*ResolvePlayerDisplayName(Record.IncomingPlayerId)
			);
			SubstitutionPresentationExpiryRealTime = FPlatformTime::Seconds() + 5.0;
		}
		ObservedCount = CurrentCount;
	};

	CaptureLatestExecution(
		PlayerState,
		ObservedPlayerTeamSubstitutionCount,
		TEXT("TU EQUIPO")
	);
	CaptureLatestExecution(
		OpponentState,
		ObservedOpponentTeamSubstitutionCount,
		TEXT("RIVAL")
	);

	UFont* PresentationFont = GEngine != nullptr
		? GEngine->GetMediumFont()
		: nullptr;
	UFont* PendingFont = GEngine != nullptr
		? GEngine->GetSmallFont()
		: nullptr;
	if (PresentationFont == nullptr)
	{
		return;
	}

	const double CurrentRealTime = FPlatformTime::Seconds();
	if (
		!SubstitutionPresentationText.IsEmpty() &&
		CurrentRealTime <= SubstitutionPresentationExpiryRealTime
	)
	{
		const float TextScale = 1.25f;
		float TextWidth = 0.0f;
		float TextHeight = 0.0f;
		GetTextSize(
			SubstitutionPresentationText,
			TextWidth,
			TextHeight,
			PresentationFont,
			TextScale
		);
		const float PaddingX = 28.0f;
		const float PaddingY = 16.0f;
		const float BoxX = Canvas->SizeX * 0.5f - TextWidth * 0.5f - PaddingX;
		const float BoxY = Canvas->SizeY * 0.24f;
		DrawRect(
			FLinearColor(0.015f, 0.055f, 0.075f, 0.90f),
			BoxX,
			BoxY,
			TextWidth + PaddingX * 2.0f,
			TextHeight + PaddingY * 2.0f
		);
		DrawText(
			SubstitutionPresentationText,
			FLinearColor(0.88f, 0.96f, 1.0f, 1.0f),
			Canvas->SizeX * 0.5f - TextWidth * 0.5f,
			BoxY + PaddingY,
			PresentationFont,
			TextScale
		);
	}

	FSoccerMatchSubstitutionRequest PendingRequest;
	if (
		PendingFont != nullptr &&
		MatchManager->GetPendingMatchSubstitution(
			ESoccerTeam::PlayerTeam,
			PendingRequest
		)
	)
	{
		const FString PendingMessage = FString::Printf(
			TEXT("CAMBIO SOLICITADO: %s por %s — esperando una pausa segura"),
			*ResolvePlayerDisplayName(PendingRequest.OutgoingPlayerId),
			*ResolvePlayerDisplayName(PendingRequest.IncomingPlayerId)
		);
		const float PendingScale = 1.05f;
		float PendingWidth = 0.0f;
		float PendingHeight = 0.0f;
		GetTextSize(
			PendingMessage,
			PendingWidth,
			PendingHeight,
			PendingFont,
			PendingScale
		);
		const float PendingX = Canvas->SizeX * 0.5f - PendingWidth * 0.5f;
		const float PendingY = Canvas->SizeY - PendingHeight - 54.0f;
		DrawRect(
			FLinearColor(0.02f, 0.03f, 0.04f, 0.78f),
			PendingX - 16.0f,
			PendingY - 9.0f,
			PendingWidth + 32.0f,
			PendingHeight + 18.0f
		);
		DrawText(
			PendingMessage,
			FLinearColor(1.0f, 0.82f, 0.38f, 1.0f),
			PendingX,
			PendingY,
			PendingFont,
			PendingScale
		);
	}
}

void AGameHUD::DrawHumanJumpHeaderIndicator()
{
	if (Canvas == nullptr)
	{
		return;
	}

	AThirdPersonCppCharacter* PlayerCharacter =
		Cast<AThirdPersonCppCharacter>(
			UGameplayStatics::GetPlayerCharacter(GetWorld(), 0)
		);

	if (!IsValid(PlayerCharacter))
	{
		return;
	}

	FString StatusText;
	FLinearColor StatusColor;

	if (!PlayerCharacter->GetHumanJumpHeaderHUDStatus(
		StatusText,
		StatusColor
	))
	{
		return;
	}

	UFont* SmallFont =
		GEngine != nullptr
		? GEngine->GetSmallFont()
		: nullptr;

	if (SmallFont == nullptr)
	{
		return;
	}

	const float TextScale = 1.10f;
	float TextWidth = 0.0f;
	float TextHeight = 0.0f;

	GetTextSize(
		StatusText,
		TextWidth,
		TextHeight,
		SmallFont,
		TextScale
	);

	const float PaddingX = 14.0f;
	const float PaddingY = 8.0f;
	const float BoxWidth = TextWidth + PaddingX * 2.0f;
	const float BoxHeight = TextHeight + PaddingY * 2.0f;
	const float BoxX = FMath::Max(18.0f, Canvas->SizeX - BoxWidth - 28.0f);
	const float BoxY = 118.0f;

	DrawRect(
		FLinearColor(0.0f, 0.0f, 0.0f, 0.62f),
		BoxX,
		BoxY,
		BoxWidth,
		BoxHeight
	);

	DrawText(
		StatusText,
		StatusColor,
		BoxX + PaddingX,
		BoxY + PaddingY,
		SmallFont,
		TextScale
	);
}

void AGameHUD::DrawMatchClock()
{
	if (Canvas == nullptr)
	{
		return;
	}

	if (!IsValid(MatchManager))
	{
		FindMatchManager();
	}

	if (
		!IsValid(MatchManager) ||
		!MatchManager->ShouldShowMatchClockHUD()
	)
	{
		return;
	}

	UFont* ClockFont =
		GEngine != nullptr
		? GEngine->GetSmallFont()
		: nullptr;

	if (ClockFont == nullptr)
	{
		return;
	}

	const ESoccerMatchPeriod MatchPeriod =
		MatchManager->GetCurrentMatchPeriod();

	FString PeriodText = TEXT("1T");
	if (MatchPeriod == ESoccerMatchPeriod::HalfTime)
	{
		PeriodText = TEXT("ET");
	}
	else if (MatchPeriod == ESoccerMatchPeriod::SecondHalf)
	{
		PeriodText = TEXT("2T");
	}
	else if (MatchPeriod == ESoccerMatchPeriod::FullTime)
	{
		PeriodText = TEXT("FT");
	}

	const int32 TotalSeconds = FMath::Max(
		0,
		FMath::FloorToInt(MatchManager->GetTotalMatchElapsedSeconds())
	);
	const int32 Minutes = TotalSeconds / 60;
	const int32 Seconds = TotalSeconds % 60;

	FString ClockText = FString::Printf(
		TEXT("%s  %02d:%02d"),
		*PeriodText,
		Minutes,
		Seconds
	);

	if (MatchPeriod == ESoccerMatchPeriod::HalfTime)
	{
		ClockText += FString::Printf(
			TEXT("  (%.1fs)"),
			MatchManager->GetHalfTimeRemainingSeconds()
		);
	}

	const float ClockScale = 1.2f;
	float ClockWidth = 0.0f;
	float ClockHeight = 0.0f;
	GetTextSize(
		ClockText,
		ClockWidth,
		ClockHeight,
		ClockFont,
		ClockScale
	);

	const float PaddingX = 14.0f;
	const float PaddingY = 8.0f;
	const float BoxWidth = ClockWidth + PaddingX * 2.0f;
	const float BoxHeight = ClockHeight + PaddingY * 2.0f;

	// El pedido de pase humano ocupa la esquina superior derecha.
	// El reloj se mantiene centrado, debajo del marcador, para que
	// ambos HUD sean independientes y no se superpongan.
	const float BoxX = Canvas->SizeX * 0.5f - BoxWidth * 0.5f;
	const float BoxY = 88.0f;

	DrawRect(
		FLinearColor(0.0f, 0.0f, 0.0f, 0.55f),
		BoxX,
		BoxY,
		BoxWidth,
		BoxHeight
	);

	DrawText(
		ClockText,
		FLinearColor::White,
		BoxX + PaddingX,
		BoxY + PaddingY,
		ClockFont,
		ClockScale
	);
}

void AGameHUD::DrawHumanPassRequestIndicator()
{
	if (Canvas == nullptr)
	{
		return;
	}

	if (!IsValid(MatchManager))
	{
		FindMatchManager();
	}

	if (
		!IsValid(MatchManager) ||
		!MatchManager->ShouldShowHumanPassRequestHUD()
	)
	{
		return;
	}

	UFont* SmallFont =
		GEngine != nullptr
		? GEngine->GetSmallFont()
		: nullptr;

	if (SmallFont == nullptr)
	{
		return;
	}

	const ESoccerHumanPassRequestType RequestType =
		MatchManager->GetActiveHumanPassRequestType();

	FString StatusText = TEXT("PEDIDO: NINGUNO");
	FLinearColor StatusColor(0.68f, 0.68f, 0.68f, 1.0f);

	if (RequestType == ESoccerHumanPassRequestType::Normal)
	{
		StatusText = TEXT("PEDIDO: A LOS PIES");
		StatusColor = FLinearColor(0.25f, 0.85f, 1.0f, 1.0f);
	}
	else if (RequestType == ESoccerHumanPassRequestType::AerialHeader)
	{
		StatusText = TEXT("PEDIDO: AEREO");
		StatusColor = FLinearColor(1.0f, 0.78f, 0.18f, 1.0f);
	}

	if (RequestType != ESoccerHumanPassRequestType::None)
	{
		const float RemainingTime =
			MatchManager->GetActiveHumanPassRequestRemainingTime();

		if (RemainingTime >= 0.0f)
		{
			StatusText += FString::Printf(
				TEXT("  %.1f s"),
				RemainingTime
			);
		}
	}

	const float StatusScale = 1.15f;
	float StatusWidth = 0.0f;
	float StatusHeight = 0.0f;

	GetTextSize(
		StatusText,
		StatusWidth,
		StatusHeight,
		SmallFont,
		StatusScale
	);

	const float PaddingX = 14.0f;
	const float PaddingY = 8.0f;
	const float BoxWidth = StatusWidth + PaddingX * 2.0f;
	const float BoxHeight = StatusHeight + PaddingY * 2.0f;
	const float BoxX = FMath::Max(18.0f, Canvas->SizeX - BoxWidth - 28.0f);
	const float BoxY = 28.0f;

	DrawRect(
		FLinearColor(0.0f, 0.0f, 0.0f, 0.62f),
		BoxX,
		BoxY,
		BoxWidth,
		BoxHeight
	);

	DrawText(
		StatusText,
		StatusColor,
		BoxX + PaddingX,
		BoxY + PaddingY,
		SmallFont,
		StatusScale
	);

	FString BriefText;
	FLinearColor BriefColor;

	if (!MatchManager->GetHumanPassRequestHUDBrief(
		BriefText,
		BriefColor
	))
	{
		return;
	}

	const float BriefScale = 1.0f;
	float BriefWidth = 0.0f;
	float BriefHeight = 0.0f;

	GetTextSize(
		BriefText,
		BriefWidth,
		BriefHeight,
		SmallFont,
		BriefScale
	);

	const float BriefBoxWidth = BriefWidth + PaddingX * 2.0f;
	const float BriefBoxHeight = BriefHeight + PaddingY * 2.0f;
	const float BriefBoxX =
		FMath::Max(18.0f, Canvas->SizeX - BriefBoxWidth - 28.0f);
	const float BriefBoxY = BoxY + BoxHeight + 6.0f;

	DrawRect(
		FLinearColor(0.0f, 0.0f, 0.0f, 0.52f),
		BriefBoxX,
		BriefBoxY,
		BriefBoxWidth,
		BriefBoxHeight
	);

	DrawText(
		BriefText,
		BriefColor,
		BriefBoxX + PaddingX,
		BriefBoxY + PaddingY,
		SmallFont,
		BriefScale
	);
}


void AGameHUD::DrawSoccerDebugTextFeed()
{
	if (Canvas == nullptr)
	{
		return;
	}

	if (!IsValid(DebugManager))
	{
		DebugManager = ASoccerDebugManager::Get(this);
	}

	if (!IsValid(DebugManager) || !DebugManager->ShouldDrawTextFeed())
	{
		return;
	}

	TArray<FSoccerDebugPanelLine> Lines;
	DebugManager->BuildVisibleTextFeedLines(Lines);

	if (Lines.Num() <= 0)
	{
		return;
	}

	UFont* SmallFont = GEngine != nullptr ? GEngine->GetSmallFont() : nullptr;
	if (SmallFont == nullptr)
	{
		return;
	}

	const float LineHeight = DebugManager->GetTextFeedLineHeight();
	const float FeedLeft = DebugManager->GetTextFeedLeft();
	const float BottomMargin = DebugManager->GetTextFeedBottomMargin();
	const float Padding = 8.0f;

	float MaxWidth = 0.0f;
	for (const FSoccerDebugPanelLine& Line : Lines)
	{
		float W = 0.0f;
		float H = 0.0f;
		GetTextSize(Line.Text, W, H, SmallFont, Line.Scale);
		MaxWidth = FMath::Max(MaxWidth, W);
	}

	const float FeedHeight = Padding * 2.0f + LineHeight * Lines.Num();
	const float FeedY = FMath::Max(10.0f, Canvas->SizeY - BottomMargin - FeedHeight);

	DrawRect(
		FLinearColor(0.0f, 0.0f, 0.0f, DebugManager->GetTextFeedBackgroundAlpha()),
		FeedLeft,
		FeedY,
		MaxWidth + Padding * 2.0f,
		FeedHeight
	);

	float TextY = FeedY + Padding;
	for (const FSoccerDebugPanelLine& Line : Lines)
	{
		DrawText(
			Line.Text,
			Line.Color,
			FeedLeft + Padding,
			TextY,
			SmallFont,
			Line.Scale
		);
		TextY += LineHeight;
	}
}

void AGameHUD::DrawSoccerDebugPanel()
{
	if (Canvas == nullptr)
	{
		return;
	}

	if (!IsValid(DebugManager))
	{
		DebugManager =
			ASoccerDebugManager::Get(this);
	}

	if (
		!IsValid(DebugManager) ||
		!DebugManager->ShouldDrawPanel()
		)
	{
		return;
	}

	TArray<FSoccerDebugPanelLine> Lines;

	DebugManager->BuildVisiblePanelLines(
		Lines
	);

	if (Lines.Num() <= 0)
	{
		return;
	}

	UFont* SmallFont =
		GEngine != nullptr
		? GEngine->GetSmallFont()
		: nullptr;

	if (SmallFont == nullptr)
	{
		return;
	}

	const float PanelWidth =
		DebugManager->GetPanelWidth();

	const float Padding =
		DebugManager->GetPanelPadding();

	const float LineHeight =
		DebugManager->GetPanelLineHeight();

	float PanelHeight =
		Padding * 2.0f;

	for (
		const FSoccerDebugPanelLine& Line :
		Lines
		)
	{
		PanelHeight +=
			LineHeight *
			FMath::Max(
				1.0f,
				Line.Scale
			);
	}

	const float PanelX =
		FMath::Max(
			10.0f,
			Canvas->SizeX -
			PanelWidth -
			DebugManager->GetPanelRightMargin()
		);

	const float PanelY =
		DebugManager->GetPanelTop();

	DrawRect(
		DebugManager->GetPanelBackgroundColor(),
		PanelX,
		PanelY,
		PanelWidth,
		PanelHeight
	);

	float TextY =
		PanelY + Padding;

	for (
		const FSoccerDebugPanelLine& Line :
		Lines
		)
	{
		DrawText(
			Line.Text,
			Line.Color,
			PanelX + Padding,
			TextY,
			SmallFont,
			Line.Scale
		);

		TextY +=
			LineHeight *
			FMath::Max(
				1.0f,
				Line.Scale
			);
	}
}

void AGameHUD::FindMatchManager()
{
	MatchManager = nullptr;

	UWorld* World = GetWorld();

	if (World == nullptr)
	{
		return;
	}

	for (TActorIterator<ASoccerMatchManager> It(World); It; ++It)
	{
		MatchManager = *It;
		return;
	}
}

FString AGameHUD::GetMatchStateText() const
{
	if (!IsValid(MatchManager))
	{
		return FString();
	}

	if (MatchManager->GetCurrentMatchPeriod() == ESoccerMatchPeriod::HalfTime)
	{
		return TEXT("ENTRETIEMPO");
	}

	if (MatchManager->GetCurrentMatchPeriod() == ESoccerMatchPeriod::FullTime)
	{
		return TEXT("FINAL");
	}

	switch (MatchManager->GetMatchPlayState())
	{
	case ESoccerMatchPlayState::GoalScored:
		return TEXT("GOOOL");

	case ESoccerMatchPlayState::Resetting:
		return TEXT("Reiniciando");

	case ESoccerMatchPlayState::Playing:
	default:
		return FString();
	}
}

void AGameHUD::DrawMatchScoreboard()
{
	if (Canvas == nullptr)
	{
		return;
	}

	if (!IsValid(MatchManager))
	{
		FindMatchManager();
	}

	if (!IsValid(MatchManager))
	{
		return;
	}

	UFont* ScoreFont =
		GEngine != nullptr
		? GEngine->GetMediumFont()
		: nullptr;

	UFont* SmallFont =
		GEngine != nullptr
		? GEngine->GetSmallFont()
		: nullptr;

	if (ScoreFont == nullptr)
	{
		return;
	}

	const int32 PlayerScore = MatchManager->GetPlayerTeamScore();
	const int32 OpponentScore = MatchManager->GetOpponentTeamScore();

	const FString ScoreText = FString::Printf(
		TEXT("PlayerTeam   %d  -  %d   OpponentTeam"),
		PlayerScore,
		OpponentScore
	);

	const float ScoreScale = 1.45f;

	float ScoreTextWidth = 0.0f;
	float ScoreTextHeight = 0.0f;

	GetTextSize(
		ScoreText,
		ScoreTextWidth,
		ScoreTextHeight,
		ScoreFont,
		ScoreScale
	);

	const float BoxPaddingX = 28.0f;
	const float BoxPaddingY = 12.0f;

	const float BoxWidth = ScoreTextWidth + BoxPaddingX * 2.0f;
	const float BoxHeight = ScoreTextHeight + BoxPaddingY * 2.0f;

	const float BoxX = Canvas->SizeX * 0.5f - BoxWidth * 0.5f;
	const float BoxY = 22.0f;

	DrawRect(
		FLinearColor(0.0f, 0.0f, 0.0f, 0.55f),
		BoxX,
		BoxY,
		BoxWidth,
		BoxHeight
	);

	const float TextX = Canvas->SizeX * 0.5f - ScoreTextWidth * 0.5f;
	const float TextY = BoxY + BoxPaddingY;

	DrawText(
		ScoreText,
		FLinearColor(0.0f, 0.0f, 0.0f, 0.85f),
		TextX + 1.0f,
		TextY + 1.0f,
		ScoreFont,
		ScoreScale
	);

	DrawText(
		ScoreText,
		FLinearColor::White,
		TextX,
		TextY,
		ScoreFont,
		ScoreScale
	);

	const FString StateText = GetMatchStateText();

	if (!StateText.IsEmpty() && SmallFont != nullptr)
	{
		const float StateScale = 1.2f;

		float StateTextWidth = 0.0f;
		float StateTextHeight = 0.0f;

		GetTextSize(
			StateText,
			StateTextWidth,
			StateTextHeight,
			SmallFont,
			StateScale
		);

		const float StateX = Canvas->SizeX * 0.5f - StateTextWidth * 0.5f;

		// El reloj se dibuja centrado debajo del marcador. Reservamos
		// ese espacio antes de mostrar mensajes como GOOOL, ENTRETIEMPO
		// o FINAL.
		const float StateY = BoxY + BoxHeight + 54.0f;

		DrawText(
			StateText,
			FLinearColor::Yellow,
			StateX,
			StateY,
			SmallFont,
			StateScale
		);
	}
}
