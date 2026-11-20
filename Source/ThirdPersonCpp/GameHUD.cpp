//GameHUD.cpp
#include "GameHUD.h"

#include "ThirdPersonCppCharacter.h"
#include "Engine/Canvas.h"
#include "Kismet/GameplayStatics.h"

#include "SoccerMatchManager.h"
#include "SoccerGameInstance.h"
#include "SoccerClubProfile.h"
#include "SoccerInstantReplayManager.h"
#include "SoccerPlayerProfile.h"
#include "SoccerTeamTypes.h"

#include "SoccerDebugManager.h"
#include "SoccerClubSelectionWidget.h"
#include "SoccerFormationMenuWidget.h"
#include "SoccerQuickTacticsWidget.h"
#include "SoccerSubstitutionMenuWidget.h"
#include "SoccerPeriodSummaryWidget.h"
#include "SoccerTacticalPresetManager.h"

#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "HAL/PlatformTime.h"


// All helpers live in a uniquely named namespace to remain safe in UE4 unity builds.
namespace SoccerHudPresentation5B
{
    static float GetScale(const UCanvas* InCanvas)
    {
        if (InCanvas == nullptr)
        {
            return 1.0f;
        }
        return FMath::Clamp(FMath::Min(InCanvas->SizeX / 1600.0f,
            InCanvas->SizeY / 900.0f), 0.70f, 1.15f);
    }

    static const USoccerClubProfile* GetClub(const ASoccerMatchManager* Manager, ESoccerTeam Team)
    {
        return IsValid(Manager) ? Manager->GetClubProfileForTeam(Team) : nullptr;
    }

    static FString GetClubName(const ASoccerMatchManager* Manager, ESoccerTeam Team)
    {
        const USoccerClubProfile* ClubProfile = GetClub(Manager, Team);
        if (IsValid(ClubProfile))
        {
            if (!ClubProfile->ShortName.IsEmpty())
            {
                return ClubProfile->ShortName.ToString();
            }
            if (!ClubProfile->DisplayName.IsEmpty())
            {
                return ClubProfile->DisplayName.ToString();
            }
            if (!ClubProfile->ClubId.IsNone())
            {
                return ClubProfile->ClubId.ToString();
            }
        }
        return Team == ESoccerTeam::PlayerTeam ? TEXT("CLUB LOCAL") : TEXT("CLUB RIVAL");
    }

    static FLinearColor GetClubAccent(const ASoccerMatchManager* Manager, ESoccerTeam Team)
    {
        const USoccerClubProfile* ClubProfile = GetClub(Manager, Team);
        if (IsValid(ClubProfile))
        {
            FLinearColor Accent = ClubProfile->PrimaryColor;
            Accent.A = 1.0f;
            return Accent;
        }
        return Team == ESoccerTeam::PlayerTeam
            ? FLinearColor(0.18f, 0.76f, 0.57f, 1.0f)
            : FLinearColor(0.41f, 0.70f, 1.0f, 1.0f);
    }
}


namespace SoccerHudContext5C
{
    static FString GetPositionName(ESoccerPlayerRole PlayerRole)
    {
        switch (PlayerRole)
        {
        case ESoccerPlayerRole::Goalkeeper: return TEXT("ARQUERO");
        case ESoccerPlayerRole::Defender: return TEXT("DEFENSOR");
        case ESoccerPlayerRole::Midfielder: return TEXT("MEDIOCAMPISTA");
        case ESoccerPlayerRole::Forward: return TEXT("DELANTERO");
        default: return TEXT("JUGADOR");
        }
    }

    static FString GetCurrentAction(const AThirdPersonCppCharacter* Human,
        const ASoccerMatchManager* Match, bool bCharging)
    {
        if (IsValid(Match) &&
            Match->GetMatchPlayState() != ESoccerMatchPlayState::Playing)
        {
            return TEXT("JUEGO DETENIDO");
        }
        if (bCharging) return TEXT("CARGANDO REMATE");
        if (Human->IsHoldingThrowInBall()) return TEXT("PREPARANDO LATERAL");
        if (Human->IsKicking()) return TEXT("PATEANDO");
        if (Human->HasHumanLogicalBallControl() || Human->IsPossessingBall())
        {
            return TEXT("CON LA PELOTA");
        }
        if (Human->IsChasingBall()) return TEXT("BUSCANDO PELOTA");
        if (IsValid(Match))
        {
            switch (Match->GetPossessionTeam())
            {
            case ESoccerPossessionTeam::PlayerTeam: return TEXT("EQUIPO EN ATAQUE");
            case ESoccerPossessionTeam::OpponentTeam: return TEXT("EQUIPO EN DEFENSA");
            default: break;
            }
        }
        return TEXT("PELOTA DIVIDIDA");
    }
}


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
	if (IsPeriodSummaryVisible5F()) return;
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
	if (IsPeriodSummaryVisible5F()) return;
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
	if (IsPeriodSummaryVisible5F()) return;
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
	if (IsPeriodSummaryVisible5F()) return;
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

void AGameHUD::DrawPlayerStatusPanel()
{
    if (Canvas == nullptr)
    {
        return;
    }
    const AThirdPersonCppCharacter* Human = Cast<AThirdPersonCppCharacter>(
        UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
    UFont* StatusFont = GEngine != nullptr ? GEngine->GetSmallFont() : nullptr;
    if (!IsValid(Human) || StatusFont == nullptr)
    {
        return;
    }

    const USoccerPlayerProfile* PlayerProfile = Human->GetPlayerProfile();
    const FName CurrentId = IsValid(PlayerProfile)
        ? PlayerProfile->Identity.PlayerId : NAME_None;
    const FString FullPlayerName = IsValid(PlayerProfile) &&
        !PlayerProfile->Identity.DisplayName.IsEmpty()
        ? PlayerProfile->Identity.DisplayName.ToString()
        : TEXT("JUGADOR CONTROLADO");
    const int32 ShirtNumber = IsValid(PlayerProfile)
        ? PlayerProfile->Identity.ShirtNumber : 0;

    const float Energy = FMath::Clamp(Human->GetPlayerEnergyPercent(), 0.0f, 1.0f);
    float KickCharge = 0.0f;
    const bool bCharging = Human->GetKickChargePercent(KickCharge);
    const double Now = FPlatformTime::Seconds();
    // Notices are edge-triggered: they cannot reappear on every HUD frame.
    if (!CurrentId.IsNone())
    {
        if (!bHudIdentityInitialized)
        {
            ObservedHudPlayerId = CurrentId;
            bHudIdentityInitialized = true;
        }
        else if (CurrentId != ObservedHudPlayerId)
        {
            ObservedHudPlayerId = CurrentId;
            bLowEnergyNoticeArmed = true;
            PlayerContextNoticeText = TEXT("CAMBIO DE JUGADOR: ") + FullPlayerName;
            PlayerContextNoticeAccent = FLinearColor(0.28f, 0.86f, 0.67f, 1.0f);
            PlayerContextNoticeStartedAt = Now;
            PlayerContextNoticeExpiresAt = Now + 3.0;
        }
    }
    if (Energy >= 0.40f)
    {
        bLowEnergyNoticeArmed = true;
    }
    else if (Energy <= 0.25f && bLowEnergyNoticeArmed)
    {
        bLowEnergyNoticeArmed = false;
        PlayerContextNoticeText = TEXT("ENERGÍA BAJA: CONSIDERÁ UN CAMBIO");
        PlayerContextNoticeAccent = FLinearColor(1.0f, 0.71f, 0.25f, 1.0f);
        PlayerContextNoticeStartedAt = Now;
        PlayerContextNoticeExpiresAt = Now + 3.5;
    }

    const float UIScale = SoccerHudPresentation5B::GetScale(Canvas);
    const float CardWidth = FMath::Max(170.0f,
        FMath::Min(312.0f * UIScale, Canvas->SizeX * 0.43f));
    const float CardX = 18.0f * UIScale;
    const float CardHeight = (bCharging ? 203.0f : 159.0f) * UIScale;
    const float CardY = FMath::Max(9.0f,
        Canvas->SizeY - CardHeight - 19.0f * UIScale);
    const float Left = CardX + 12.0f * UIScale;
    const float Right = CardX + CardWidth - 12.0f * UIScale;
    const float BarWidth = FMath::Max(40.0f, Right - Left);
    const float BarHeight = 10.0f * UIScale;
    const FLinearColor TeamAccent = SoccerHudPresentation5B::GetClubAccent(
        MatchManager, ESoccerTeam::PlayerTeam);

    DrawRect(FLinearColor(0.018f, 0.035f, 0.050f, 0.85f),
        CardX, CardY, CardWidth, CardHeight);
    DrawRect(TeamAccent, CardX, CardY, 3.0f * UIScale, CardHeight);

    // Shirt number and player name are the main hierarchy of the contextual HUD.
    const float ShirtWidth = 39.0f * UIScale;
    const float ShirtTop = CardY + 11.0f * UIScale;
    DrawRect(FLinearColor(TeamAccent.R, TeamAccent.G, TeamAccent.B, 0.27f),
        Left, ShirtTop, ShirtWidth, 34.0f * UIScale);
    const FString ShirtText = ShirtNumber > 0
        ? FString::Printf(TEXT("%d"), ShirtNumber) : TEXT("--");
    float ShirtTextW = 0.0f;
    float ShirtTextH = 0.0f;
    const float ShirtScale = 1.22f * UIScale;
    GetTextSize(ShirtText, ShirtTextW, ShirtTextH, StatusFont, ShirtScale);
    DrawText(ShirtText, FLinearColor::White,
        Left + (ShirtWidth - ShirtTextW) * 0.5f,
        ShirtTop + (34.0f * UIScale - ShirtTextH) * 0.5f,
        StatusFont, ShirtScale);

    const float NameX = Left + ShirtWidth + 10.0f * UIScale;
    const float NameMaxWidth = FMath::Max(50.0f, Right - NameX);
    FString VisibleName = FullPlayerName;
    float NameScale = 1.07f * UIScale;
    float NameWidth = 0.0f;
    float NameHeight = 0.0f;
    GetTextSize(VisibleName, NameWidth, NameHeight, StatusFont, NameScale);
    if (NameWidth > NameMaxWidth && NameWidth > 0.0f)
    {
        NameScale = FMath::Max(0.78f * UIScale,
            NameScale * NameMaxWidth / NameWidth);
        GetTextSize(VisibleName, NameWidth, NameHeight, StatusFont, NameScale);
    }
    for (int32 TextLen = FullPlayerName.Len() - 1;
        NameWidth > NameMaxWidth && TextLen >= 3; --TextLen)
    {
        VisibleName = FullPlayerName.Left(TextLen) + TEXT("...");
        GetTextSize(VisibleName, NameWidth, NameHeight, StatusFont, NameScale);
    }
    DrawText(VisibleName, FLinearColor(0.96f, 0.99f, 1.0f, 1.0f),
        NameX, CardY + 12.0f * UIScale, StatusFont, NameScale);

    const FString PositionText = SoccerHudContext5C::GetPositionName(
        Human->GetPlayerRole());
    DrawText(PositionText, FLinearColor(0.70f, 0.84f, 0.88f, 1.0f),
        NameX, CardY + 32.0f * UIScale, StatusFont, 0.86f * UIScale);

    // The current action uses its own line; avoid squeezing the role and
    // action into an unreadable single label on 1280x720 displays.
    const FString ActionText = SoccerHudContext5C::GetCurrentAction(
        Human, MatchManager, bCharging);
    const float ContextY = CardY + 54.0f * UIScale;
    DrawRect(FLinearColor(0.07f, 0.16f, 0.17f, 0.88f),
        Left, ContextY, BarWidth, 20.0f * UIScale);
    DrawRect(TeamAccent, Left, ContextY, 2.0f * UIScale, 20.0f * UIScale);
    float ActionScale = 0.92f * UIScale;
    float ActionWidth = 0.0f;
    float ActionHeight = 0.0f;
    GetTextSize(ActionText, ActionWidth, ActionHeight, StatusFont, ActionScale);
    if (ActionWidth > BarWidth - 17.0f * UIScale && ActionWidth > 0.0f)
    {
        ActionScale *= (BarWidth - 17.0f * UIScale) / ActionWidth;
    }
    DrawText(ActionText, FLinearColor(0.79f, 0.98f, 0.90f, 1.0f),
        Left + 9.0f * UIScale, ContextY + 3.0f * UIScale,
        StatusFont, ActionScale);

    const float EnergyLabelY = CardY + 82.0f * UIScale;
    DrawText(TEXT("ENERGÍA"), FLinearColor::White,
        Left, EnergyLabelY, StatusFont, 0.99f * UIScale);
    const FString EnergyText = FString::Printf(TEXT("%d%%"),
        FMath::RoundToInt(Energy * 100.0f));
    float EnergyTextWidth = 0.0f;
    float EnergyTextHeight = 0.0f;
    GetTextSize(EnergyText, EnergyTextWidth, EnergyTextHeight,
        StatusFont, 0.99f * UIScale);
    DrawText(EnergyText, FLinearColor(0.87f, 0.96f, 0.91f, 1.0f),
        Right - EnergyTextWidth, EnergyLabelY, StatusFont, 0.99f * UIScale);

    const float EnergyBarY = CardY + 107.0f * UIScale;
    DrawRect(FLinearColor(0.08f, 0.13f, 0.16f, 1.0f),
        Left, EnergyBarY, BarWidth, BarHeight);
    const FLinearColor EnergyColor = Energy >= 0.50f
        ? FLinearColor(0.22f, 0.86f, 0.47f, 1.0f)
        : (Energy >= 0.25f ? FLinearColor(1.0f, 0.73f, 0.22f, 1.0f)
            : FLinearColor(1.0f, 0.32f, 0.26f, 1.0f));
    DrawRect(EnergyColor, Left, EnergyBarY,
        BarWidth * Energy, BarHeight);

    DrawText(FString::Printf(TEXT("PUNTOS  %d"), Human->GetScore()),
        FLinearColor(0.79f, 0.84f, 0.88f, 1.0f),
        Left, CardY + 133.0f * UIScale, StatusFont, 0.88f * UIScale);

    if (bCharging)
    {
        DrawText(TEXT("FUERZA DE DISPARO"), FLinearColor::White,
            Left, CardY + 157.0f * UIScale, StatusFont, 0.93f * UIScale);
        const float KickY = CardY + 181.0f * UIScale;
        DrawRect(FLinearColor(0.08f, 0.13f, 0.16f, 1.0f),
            Left, KickY, BarWidth, BarHeight);
        DrawRect(FLinearColor(1.0f, 0.69f, 0.19f, 1.0f),
            Left, KickY, BarWidth * FMath::Clamp(KickCharge, 0.0f, 1.0f), BarHeight);
    }

    // Only one optional toast is visible, and it sits above this card.
    if (!PlayerContextNoticeText.IsEmpty() && Now < PlayerContextNoticeExpiresAt)
    {
        const float NoticeMaxWidth = FMath::Min(
            Canvas->SizeX - 2.0f * Left, CardWidth + 65.0f * UIScale);
        float NoticeScale = 0.96f * UIScale;
        float NoticeWidth = 0.0f;
        float NoticeHeight = 0.0f;
        GetTextSize(PlayerContextNoticeText, NoticeWidth, NoticeHeight,
            StatusFont, NoticeScale);
        if (NoticeWidth > NoticeMaxWidth - 24.0f * UIScale && NoticeWidth > 0.0f)
        {
            NoticeScale *= (NoticeMaxWidth - 24.0f * UIScale) / NoticeWidth;
            GetTextSize(PlayerContextNoticeText, NoticeWidth, NoticeHeight,
                StatusFont, NoticeScale);
        }
        const float NoticeBoxHeight = NoticeHeight + 17.0f * UIScale;
        const float NoticeY = CardY - NoticeBoxHeight - 7.0f * UIScale;
        if (NoticeY >= 8.0f * UIScale)
        {
            const float FadeIn = FMath::Clamp(static_cast<float>(
                (Now - PlayerContextNoticeStartedAt) / 0.15), 0.0f, 1.0f);
            const float FadeOut = FMath::Clamp(static_cast<float>(
                (PlayerContextNoticeExpiresAt - Now) / 0.42), 0.0f, 1.0f);
            const float Fade = FMath::Min(FadeIn, FadeOut);
            const float NoticeBoxWidth = NoticeWidth + 24.0f * UIScale;
            DrawRect(FLinearColor(0.016f, 0.043f, 0.054f, 0.86f * Fade),
                CardX, NoticeY, NoticeBoxWidth, NoticeBoxHeight);
            DrawRect(FLinearColor(PlayerContextNoticeAccent.R,
                PlayerContextNoticeAccent.G, PlayerContextNoticeAccent.B, Fade),
                CardX, NoticeY, 3.0f * UIScale, NoticeBoxHeight);
            DrawText(PlayerContextNoticeText,
                FLinearColor(1.0f, 1.0f, 1.0f, Fade),
                CardX + 12.0f * UIScale, NoticeY + 8.0f * UIScale,
                StatusFont, NoticeScale);
        }
    }
}


void AGameHUD::DrawRightStatusCard(
    const FString& Message, const FLinearColor& TextColor, float FontScale)
{
    if (Canvas == nullptr || Message.IsEmpty())
    {
        return;
    }
    UFont* StatusFont = GEngine != nullptr ? GEngine->GetSmallFont() : nullptr;
    if (StatusFont == nullptr)
    {
        return;
    }

    const float UIScale = SoccerHudPresentation5B::GetScale(Canvas);
    const float PaddingX = 12.0f * UIScale;
    const float PaddingY = 8.0f * UIScale;
    const float MaxWidth = FMath::Max(115.0f, FMath::Min(
        360.0f * UIScale, Canvas->SizeX * 0.41f));
    float ActualScale = FontScale * UIScale;
    float TextWidth = 0.0f;
    float TextHeight = 0.0f;
    GetTextSize(Message, TextWidth, TextHeight, StatusFont, ActualScale);
    if (TextWidth > MaxWidth - 2.0f * PaddingX)
    {
        ActualScale *= (MaxWidth - 2.0f * PaddingX) / TextWidth;
        GetTextSize(Message, TextWidth, TextHeight, StatusFont, ActualScale);
    }
    const float BoxWidth = TextWidth + 2.0f * PaddingX;
    const float BoxHeight = TextHeight + 2.0f * PaddingY;
    const float BoxX = FMath::Max(8.0f,
        Canvas->SizeX - BoxWidth - 18.0f * UIScale);
    const float BoxY = FMath::Max(0.0f, HUDRightStatusNextY);
    if (BoxY + BoxHeight > Canvas->SizeY - 105.0f * UIScale)
    {
        return;
    }

    DrawRect(FLinearColor(0.018f, 0.035f, 0.050f, 0.83f),
        BoxX, BoxY, BoxWidth, BoxHeight);
    DrawRect(TextColor, BoxX, BoxY, 3.0f * UIScale, BoxHeight);
    DrawText(Message, TextColor,
        BoxX + PaddingX, BoxY + PaddingY, StatusFont, ActualScale);
    HUDRightStatusNextY = BoxY + BoxHeight + 6.0f * UIScale;
}


void AGameHUD::DrawQuickTacticsFeedback()
{
    if (QuickTacticsFeedbackText.IsEmpty() ||
        FPlatformTime::Seconds() > QuickTacticsFeedbackExpiryRealTime)
    {
        return;
    }
    DrawRightStatusCard(QuickTacticsFeedbackText,
        FLinearColor(0.43f, 0.97f, 0.68f, 1.0f), 1.07f);
}


void AGameHUD::DrawInstantReplayOverlay()
{
    if (Canvas == nullptr || !IsValid(MatchManager))
    {
        return;
    }
    ASoccerInstantReplayManager* ReplayManager = MatchManager->GetInstantReplayManager();
    if (!IsValid(ReplayManager) || !ReplayManager->IsReplayPlaying())
    {
        return;
    }
    UFont* ReplayFont = GEngine != nullptr ? GEngine->GetLargeFont() : nullptr;
    UFont* HintFont = GEngine != nullptr ? GEngine->GetSmallFont() : nullptr;
    if (ReplayFont == nullptr || HintFont == nullptr)
    {
        return;
    }

    const float UIScale = SoccerHudPresentation5B::GetScale(Canvas);
    const FString ReplayText = TEXT("REPLAY");
    const FString SkipHint = ReplayManager->GetSkipReplayInputHintText();
    float ReplayScale = 1.15f * UIScale;
    float HintScale = 1.0f * UIScale;
    const float PaddingX = 16.0f * UIScale;
    const float PaddingY = 10.0f * UIScale;
    const float Gap = 6.0f * UIScale;
    const float MaxWidth = FMath::Max(100.0f,
        FMath::Min(390.0f * UIScale, Canvas->SizeX * 0.43f));

    float ReplayWidth = 0.0f;
    float ReplayHeight = 0.0f;
    GetTextSize(ReplayText, ReplayWidth, ReplayHeight, ReplayFont, ReplayScale);
    float HintWidth = 0.0f;
    float HintHeight = 0.0f;
    GetTextSize(SkipHint, HintWidth, HintHeight, HintFont, HintScale);
    if (HintWidth + 2.0f * PaddingX > MaxWidth && HintWidth > 0.0f)
    {
        HintScale *= (MaxWidth - 2.0f * PaddingX) / HintWidth;
        GetTextSize(SkipHint, HintWidth, HintHeight, HintFont, HintScale);
    }
    const float BoxWidth = FMath::Max(ReplayWidth, HintWidth) + 2.0f * PaddingX;
    const float BoxHeight = ReplayHeight + HintHeight + Gap + 2.0f * PaddingY;
    const float BoxX = FMath::Max(8.0f, Canvas->SizeX - BoxWidth - 18.0f * UIScale);
    const float BoxY = HUDTopBandBottomY + 11.0f * UIScale;

    DrawRect(FLinearColor(0.015f, 0.038f, 0.060f, 0.90f),
        BoxX, BoxY, BoxWidth, BoxHeight);
    DrawRect(FLinearColor(0.98f, 0.80f, 0.25f, 1.0f),
        BoxX, BoxY, 3.0f * UIScale, BoxHeight);
    DrawText(ReplayText, FLinearColor::White,
        BoxX + PaddingX, BoxY + PaddingY, ReplayFont, ReplayScale);
    DrawText(SkipHint, FLinearColor(0.88f, 0.90f, 0.93f, 1.0f),
        BoxX + PaddingX, BoxY + PaddingY + ReplayHeight + Gap,
        HintFont, HintScale);
}

bool AGameHUD::IsPeriodSummaryVisible5F() const
{
	return IsValid(PeriodSummaryWidget5F) && PeriodSummaryWidget5F->IsInViewport();
}

void AGameHUD::DismissFinalSummary5F()
{
	if (IsValid(MatchManager) &&
		MatchManager->GetCurrentMatchPeriod() == ESoccerMatchPeriod::FullTime)
	{
		bFinalSummaryDismissed5F = true;
		UpdatePeriodSummary5F();
	}
}

void AGameHUD::UpdatePeriodSummary5F()
{
	if (!IsValid(MatchManager))
	{
		return;
	}
	const ESoccerMatchPeriod Period = MatchManager->GetCurrentMatchPeriod();
	if (Period == ESoccerMatchPeriod::FirstHalf)
	{
		// A new match in the same world must be allowed to show its final recap.
		bFinalSummaryDismissed5F = false;
	}
	const bool bHalfTime = Period == ESoccerMatchPeriod::HalfTime;
	const bool bFullTime = Period == ESoccerMatchPeriod::FullTime;
	const bool bOtherMenu = IsClubSelectionMenuVisible() ||
		IsFormationMenuVisible() || IsQuickTacticsMenuVisible() ||
		IsSubstitutionMenuVisible();
	const bool bShouldShow = (bHalfTime || (bFullTime && !bFinalSummaryDismissed5F)) &&
		!bOtherMenu;
	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!bShouldShow || PlayerController == nullptr)
	{
		if (IsPeriodSummaryVisible5F())
		{
			PeriodSummaryWidget5F->RemoveFromParent();
			if (PlayerController && !bOtherMenu)
			{
				PlayerController->bShowMouseCursor = bPeriodSummaryPreviousCursor5F;
				PlayerController->SetInputMode(FInputModeGameOnly());
			}
		}
		return;
	}
	if (!IsValid(PeriodSummaryWidget5F))
	{
		PeriodSummaryWidget5F = CreateWidget<USoccerPeriodSummaryWidget>(
			PlayerController, USoccerPeriodSummaryWidget::StaticClass());
	}
	if (IsValid(PeriodSummaryWidget5F) && !PeriodSummaryWidget5F->IsInViewport())
	{
		bPeriodSummaryPreviousCursor5F = PlayerController->bShowMouseCursor;
		PeriodSummaryWidget5F->InitializeForMatch(MatchManager);
		PeriodSummaryWidget5F->AddToViewport(180);
		PlayerController->bShowMouseCursor = true;
		PlayerController->bEnableClickEvents = true;
		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(PeriodSummaryWidget5F->TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PlayerController->SetInputMode(InputMode);
		PeriodSummaryWidget5F->SetKeyboardFocus();
	}
}

void AGameHUD::DrawHUD()
{
    Super::DrawHUD();

    if (!IsValid(MatchManager))
    {
        FindMatchManager();
    }

    HUDTopBandBottomY = 0.0f;
    HUDRightStatusNextY = 0.0f;
    HUDMainAnnouncementBottomY = 0.0f;
    ObserveMatchAnnouncement();
	UpdatePeriodSummary5F();

    // Keep the result visible in replay, but hide normal play controls.
    DrawMatchScoreboard();
    DrawMatchClock();

    ASoccerInstantReplayManager* ReplayManager =
        IsValid(MatchManager) ? MatchManager->GetInstantReplayManager() : nullptr;
    if (IsValid(ReplayManager) && ReplayManager->IsReplayPlaying())
    {
        DrawInstantReplayOverlay();
        DrawMatchAnnouncement();
        return;
    }

    DrawMatchAnnouncement();
    DrawCompactPossession5D();
    if (IsValid(MatchManager) &&
        (MatchManager->GetCurrentMatchPeriod() == ESoccerMatchPeriod::HalfTime ||
         MatchManager->GetCurrentMatchPeriod() == ESoccerMatchPeriod::FullTime))
    {
        // The interactive 5F screen owns the full report. Retain the legacy
        // Canvas report only as a fallback if UMG cannot be constructed.
        if (!IsPeriodSummaryVisible5F() && !bFinalSummaryDismissed5F)
        {
            DrawPeriodSummary5D();
        }
        return;
    }
    DrawHumanPassRequestIndicator();
    DrawHumanJumpHeaderIndicator();
    DrawQuickTacticsFeedback();
    DrawSubstitutionPresentation();
    DrawSoccerDebugTextFeed();
    DrawSoccerDebugPanel();
    DrawPlayerStatusPanel();

    // The aiming reticle is a gameplay indicator and must remain untouched.
    AThirdPersonCppCharacter* PlayerCharacter = Cast<AThirdPersonCppCharacter>(
        UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
    if (IsValid(PlayerCharacter))
    {
        float AimScreenX = 0.0f;
        float AimScreenY = 0.0f;
        if (PlayerCharacter->GetAimCursorScreenPosition(AimScreenX, AimScreenY))
        {
            const float CursorSize = 10.0f;
            DrawLine(AimScreenX - CursorSize * 6.0f, AimScreenY,
                AimScreenX + CursorSize * 6.0f, AimScreenY, FLinearColor::Green, 2.0f);
            DrawLine(AimScreenX, AimScreenY - CursorSize,
                AimScreenX, AimScreenY + CursorSize, FLinearColor::Green, 2.0f);
        }
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
		float TextScale = 1.25f * SoccerHudPresentation5B::GetScale(Canvas);
		float TextWidth = 0.0f;
		float TextHeight = 0.0f;
		GetTextSize(
			SubstitutionPresentationText,
			TextWidth,
			TextHeight,
			PresentationFont,
			TextScale
		);
		const float UIScale = SoccerHudPresentation5B::GetScale(Canvas);
		const float PaddingX = 23.0f * UIScale;
		const float PaddingY = 13.0f * UIScale;
		const float MaxWidth = FMath::Max(140.0f, Canvas->SizeX - 45.0f * UIScale);
		if (TextWidth + 2.0f * PaddingX > MaxWidth)
		{
			TextScale *= (MaxWidth - 2.0f * PaddingX) / TextWidth;
			GetTextSize(SubstitutionPresentationText, TextWidth, TextHeight,
				PresentationFont, TextScale);
		}
		const float BoxX = Canvas->SizeX * 0.5f - TextWidth * 0.5f - PaddingX;
		const float BoxHeight = TextHeight + PaddingY * 2.0f;
		const float BoxY = FMath::Max(Canvas->SizeY * 0.45f,
			HUDMainAnnouncementBottomY + 14.0f * UIScale);
		if (BoxY + BoxHeight < Canvas->SizeY - 115.0f * UIScale)
		{
		DrawRect(
			FLinearColor(0.015f, 0.055f, 0.075f, 0.90f),
			BoxX,
			BoxY,
			TextWidth + PaddingX * 2.0f,
			BoxHeight
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
		float PendingScale = 1.05f * SoccerHudPresentation5B::GetScale(Canvas);
		float PendingWidth = 0.0f;
		float PendingHeight = 0.0f;
		GetTextSize(
			PendingMessage,
			PendingWidth,
			PendingHeight,
			PendingFont,
			PendingScale
		);
		const float UIScale = SoccerHudPresentation5B::GetScale(Canvas);
		const float PendingMaxWidth = FMath::Max(125.0f, Canvas->SizeX - 48.0f * UIScale);
		if (PendingWidth + 32.0f * UIScale > PendingMaxWidth)
		{
			PendingScale *= (PendingMaxWidth - 32.0f * UIScale) / PendingWidth;
			GetTextSize(PendingMessage, PendingWidth, PendingHeight,
				PendingFont, PendingScale);
		}
		const float PendingX = Canvas->SizeX * 0.5f - PendingWidth * 0.5f;
		const float PendingY = FMath::Max(HUDMainAnnouncementBottomY + 16.0f * UIScale,
			FMath::Min(Canvas->SizeY * 0.73f, Canvas->SizeY - 159.0f * UIScale));
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
    AThirdPersonCppCharacter* PlayerCharacter = Cast<AThirdPersonCppCharacter>(
        UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
    if (!IsValid(PlayerCharacter))
    {
        return;
    }

    FString StatusText;
    FLinearColor StatusColor;
    if (PlayerCharacter->GetHumanJumpHeaderHUDStatus(StatusText, StatusColor))
    {
        DrawRightStatusCard(StatusText, StatusColor, 1.06f);
    }
}

void AGameHUD::DrawMatchClock()
{
    if (Canvas == nullptr)
    {
        return;
    }
    const float UIScale = SoccerHudPresentation5B::GetScale(Canvas);
    HUDRightStatusNextY = HUDTopBandBottomY + 12.0f * UIScale;
    if (!IsValid(MatchManager) || !MatchManager->ShouldShowMatchClockHUD())
    {
        return;
    }
    UFont* ClockFont = GEngine != nullptr ? GEngine->GetSmallFont() : nullptr;
    if (ClockFont == nullptr)
    {
        return;
    }

    const ESoccerMatchPeriod MatchPeriod = MatchManager->GetCurrentMatchPeriod();
    FString PeriodText = TEXT("1T");
    if (MatchPeriod == ESoccerMatchPeriod::HalfTime) PeriodText = TEXT("ET");
    else if (MatchPeriod == ESoccerMatchPeriod::SecondHalf) PeriodText = TEXT("2T");
    else if (MatchPeriod == ESoccerMatchPeriod::FullTime) PeriodText = TEXT("FT");

    const int32 TotalSeconds = FMath::Max(0,
        FMath::FloorToInt(MatchManager->GetTotalMatchElapsedSeconds()));
    FString ClockText = FString::Printf(TEXT("%s    %02d:%02d"),
        *PeriodText, TotalSeconds / 60, TotalSeconds % 60);
    if (MatchPeriod == ESoccerMatchPeriod::FirstHalf ||
        MatchPeriod == ESoccerMatchPeriod::SecondHalf)
    {
        const float ExtraSeconds = MatchManager->GetCurrentHalfAddedTimeSeconds();
        if (MatchManager->IsCurrentHalfInAddedTime() && ExtraSeconds > 0.01f)
        {
            const int32 RoundedSeconds = FMath::CeilToInt(ExtraSeconds);
            ClockText += FString::Printf(TEXT("   +%d:%02d"),
                RoundedSeconds / 60, RoundedSeconds % 60);
        }
    }
    if (MatchPeriod == ESoccerMatchPeriod::HalfTime)
    {
        ClockText += FString::Printf(TEXT("   %.1fs"),
            MatchManager->GetHalfTimeRemainingSeconds());
    }
    const FString StateText = GetMatchStateText();
    if (!StateText.IsEmpty())
    {
        ClockText += TEXT("  |  ") + StateText;
    }

    float ClockScale = 1.18f * UIScale;
    float TextWidth = 0.0f;
    float TextHeight = 0.0f;
    GetTextSize(ClockText, TextWidth, TextHeight, ClockFont, ClockScale);
    const float MaxWidth = FMath::Max(95.0f, Canvas->SizeX - 30.0f * UIScale);
    if (TextWidth + 25.0f * UIScale > MaxWidth)
    {
        ClockScale *= (MaxWidth - 25.0f * UIScale) / TextWidth;
        GetTextSize(ClockText, TextWidth, TextHeight, ClockFont, ClockScale);
    }
    const float BoxWidth = TextWidth + 25.0f * UIScale;
    const float BoxHeight = TextHeight + 13.0f * UIScale;
    const float BoxX = (Canvas->SizeX - BoxWidth) * 0.5f;
    const float BoxY = HUDTopBandBottomY + 4.0f * UIScale;
    DrawRect(FLinearColor(0.020f, 0.047f, 0.060f, 0.88f),
        BoxX, BoxY, BoxWidth, BoxHeight);
    DrawText(ClockText, FLinearColor(0.87f, 0.95f, 0.98f, 1.0f),
        BoxX + 12.5f * UIScale,
        BoxY + 6.5f * UIScale, ClockFont, ClockScale);
    HUDTopBandBottomY = BoxY + BoxHeight;
    HUDRightStatusNextY = HUDTopBandBottomY + 10.0f * UIScale;
}

void AGameHUD::DrawHumanPassRequestIndicator()
{
    if (!IsValid(MatchManager))
    {
        FindMatchManager();
    }
    if (!IsValid(MatchManager) || !MatchManager->ShouldShowHumanPassRequestHUD())
    {
        return;
    }

    const ESoccerHumanPassRequestType RequestType =
        MatchManager->GetActiveHumanPassRequestType();
    FString StatusText = TEXT("PEDIDO: NINGUNO");
    FLinearColor StatusColor(0.67f, 0.73f, 0.75f, 1.0f);
    if (RequestType == ESoccerHumanPassRequestType::Normal)
    {
        StatusText = TEXT("PEDIDO: A LOS PIES");
        StatusColor = FLinearColor(0.29f, 0.85f, 1.0f, 1.0f);
    }
    else if (RequestType == ESoccerHumanPassRequestType::AerialHeader)
    {
        StatusText = TEXT("PEDIDO: AÉREO");
        StatusColor = FLinearColor(1.0f, 0.79f, 0.24f, 1.0f);
    }
    if (RequestType != ESoccerHumanPassRequestType::None)
    {
        const float RemainingTime = MatchManager->GetActiveHumanPassRequestRemainingTime();
        if (RemainingTime >= 0.0f)
        {
            StatusText += FString::Printf(TEXT("  %.1f s"), RemainingTime);
        }
    }
    if (RequestType == ESoccerHumanPassRequestType::None)
    {
        return;
    }
    DrawRightStatusCard(StatusText, StatusColor, 1.1f);

    FString BriefText;
    FLinearColor BriefColor;
    if (MatchManager->GetHumanPassRequestHUDBrief(BriefText, BriefColor))
    {
        DrawRightStatusCard(BriefText, BriefColor, 0.96f);
    }
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

    // Goal / periods are now drawn by the large event banner, not twice.
	switch (MatchManager->GetMatchPlayState())
	{
	case ESoccerMatchPlayState::Resetting:
		return TEXT("Reiniciando");

	case ESoccerMatchPlayState::Playing:
	default:
		return FString();
	}
}

void AGameHUD::ObserveMatchAnnouncement()
{
    if (!IsValid(MatchManager))
    {
        return;
    }

    const FSoccerMatchAnnouncement& Event = MatchManager->GetLatestMatchAnnouncement();
    if (Event.Revision == 0 || Event.Revision == ObservedAnnouncementRevision)
    {
        return;
    }

    ObservedAnnouncementRevision = Event.Revision;
    MatchAnnouncementTitle = Event.Title;
    MatchAnnouncementDetail = Event.Detail;
    MatchAnnouncementAccent = Event.Accent;
    MatchAnnouncementStartRealTime = FPlatformTime::Seconds();
    MatchAnnouncementExpiryRealTime = MatchAnnouncementStartRealTime + Event.DurationSeconds;
}

void AGameHUD::DrawMatchAnnouncement()
{
    if (Canvas == nullptr || MatchAnnouncementTitle.IsEmpty())
    {
        return;
    }

    const double Now = FPlatformTime::Seconds();
    const float Remaining = static_cast<float>(MatchAnnouncementExpiryRealTime - Now);
    if (Remaining <= 0.0f)
    {
        return;
    }

    UFont* TitleFont = GEngine != nullptr ? GEngine->GetMediumFont() : nullptr;
    UFont* DetailFont = GEngine != nullptr ? GEngine->GetSmallFont() : nullptr;
    if (TitleFont == nullptr || DetailFont == nullptr)
    {
        return;
    }

    const float FadeIn = FMath::Clamp(
        static_cast<float>(Now - MatchAnnouncementStartRealTime) / 0.16f, 0.0f, 1.0f);
    const float FadeOut = FMath::Clamp(Remaining / 0.42f, 0.0f, 1.0f);
    const float Opacity = FMath::Min(FadeIn, FadeOut);
    const float ScreenScale = FMath::Clamp(
        FMath::Min(Canvas->SizeX / 1920.0f, Canvas->SizeY / 1080.0f), 0.72f, 1.15f);
    const bool bIsGoal = MatchAnnouncementTitle.StartsWith(TEXT("¡GO"));

    float TitleScale = (bIsGoal ? 3.15f : 2.18f) * ScreenScale;
    float DetailScale = 1.42f * ScreenScale;
    float TitleWidth = 0.0f;
    float TitleHeight = 0.0f;
    float DetailWidth = 0.0f;
    float DetailHeight = 0.0f;
    GetTextSize(MatchAnnouncementTitle, TitleWidth, TitleHeight, TitleFont, TitleScale);
    GetTextSize(MatchAnnouncementDetail, DetailWidth, DetailHeight, DetailFont, DetailScale);

    const float MaxTextWidth = FMath::Max(250.0f, Canvas->SizeX - 110.0f * ScreenScale);
    if (TitleWidth > MaxTextWidth)
    {
        TitleScale *= MaxTextWidth / TitleWidth;
        GetTextSize(MatchAnnouncementTitle, TitleWidth, TitleHeight, TitleFont, TitleScale);
    }
    if (DetailWidth > MaxTextWidth)
    {
        DetailScale *= MaxTextWidth / DetailWidth;
        GetTextSize(MatchAnnouncementDetail, DetailWidth, DetailHeight, DetailFont, DetailScale);
    }

    const float Gap = 9.0f * ScreenScale;
    const float PaddingX = 30.0f * ScreenScale;
    const float PaddingY = 16.0f * ScreenScale;
    const float BoxWidth = FMath::Min(Canvas->SizeX - 30.0f,
        FMath::Max(TitleWidth, DetailWidth) + PaddingX * 2.0f);
    const float BoxHeight = TitleHeight + DetailHeight + Gap + PaddingY * 2.0f;
    const float X = Canvas->SizeX * 0.5f - BoxWidth * 0.5f;
    // Below scoreboard and clock; does not cover pass-request HUD.
    const float Y = FMath::Max(HUDTopBandBottomY + 68.0f * ScreenScale,
        Canvas->SizeY * 0.30f);
    HUDMainAnnouncementBottomY = Y + BoxHeight;

    DrawRect(FLinearColor(0.015f, 0.025f, 0.045f, 0.78f * Opacity),
        X, Y, BoxWidth, BoxHeight);
    DrawRect(FLinearColor(MatchAnnouncementAccent.R, MatchAnnouncementAccent.G,
        MatchAnnouncementAccent.B, Opacity),
        X, Y, 6.0f * ScreenScale, BoxHeight);

    DrawText(MatchAnnouncementTitle,
        FLinearColor(1.0f, 1.0f, 1.0f, Opacity),
        Canvas->SizeX * 0.5f - TitleWidth * 0.5f,
        Y + PaddingY, TitleFont, TitleScale);
    DrawText(MatchAnnouncementDetail,
        FLinearColor(MatchAnnouncementAccent.R, MatchAnnouncementAccent.G,
            MatchAnnouncementAccent.B, Opacity),
        Canvas->SizeX * 0.5f - DetailWidth * 0.5f,
        Y + PaddingY + TitleHeight + Gap, DetailFont, DetailScale);
}

// Stage 5D: match data in a deliberately separate strip below the clock.
// These shares only describe recognized CONTROLLED possession, not all play time.
void AGameHUD::DrawCompactPossession5D()
{
    if (Canvas == nullptr || !IsValid(MatchManager) ||
        !MatchManager->ShouldShowMatchClockHUD() ||
        !MatchManager->IsMatchPeriodGameplayActive())
    {
        return;
    }
    const FSoccerMatchStatistics5D& Stats = MatchManager->GetMatchStatistics5D();
    const float HomeSeconds = Stats.PlayerTeam.ControlledPossessionSeconds;
    const float AwaySeconds = Stats.OpponentTeam.ControlledPossessionSeconds;
    const float Total = HomeSeconds + AwaySeconds;
    if (Total < 1.0f) return;

    UFont* Font = GEngine != nullptr ? GEngine->GetSmallFont() : nullptr;
    if (Font == nullptr) return;
    const float UIScale = SoccerHudPresentation5B::GetScale(Canvas);
    const float BoxWidth = FMath::Min(350.0f * UIScale,
        Canvas->SizeX - 24.0f * UIScale);
    const float BoxHeight = 36.0f * UIScale;
    const float BoxX = (Canvas->SizeX - BoxWidth) * 0.5f;
    const float BoxY = HUDTopBandBottomY + 5.0f * UIScale;
    const float Share = FMath::Clamp(HomeSeconds / Total, 0.0f, 1.0f);
    const int32 HomePct = FMath::RoundToInt(Share * 100.0f);
    const FString Label = FString::Printf(TEXT("POSESIÓN CONTROLADA   %d%% - %d%%"),
        HomePct, 100 - HomePct);
    float LabelWidth = 0.0f, LabelHeight = 0.0f;
    float LabelScale = 0.91f * UIScale;
    GetTextSize(Label, LabelWidth, LabelHeight, Font, LabelScale);
    if (LabelWidth > BoxWidth - 16.0f * UIScale && LabelWidth > 0.0f)
    {
        LabelScale *= (BoxWidth - 16.0f * UIScale) / LabelWidth;
        GetTextSize(Label, LabelWidth, LabelHeight, Font, LabelScale);
    }
    DrawRect(FLinearColor(0.015f, 0.037f, 0.049f, 0.87f),
        BoxX, BoxY, BoxWidth, BoxHeight);
    DrawText(Label, FLinearColor(0.91f, 0.96f, 0.98f, 1.0f),
        BoxX + (BoxWidth - LabelWidth) * 0.5f, BoxY + 4.0f * UIScale,
        Font, LabelScale);
    const float BarX = BoxX + 10.0f * UIScale;
    const float BarY = BoxY + 26.0f * UIScale;
    const float BarW = BoxWidth - 20.0f * UIScale;
    const float BarH = 5.0f * UIScale;
    DrawRect(SoccerHudPresentation5B::GetClubAccent(MatchManager,
        ESoccerTeam::OpponentTeam), BarX, BarY, BarW, BarH);
    DrawRect(SoccerHudPresentation5B::GetClubAccent(MatchManager,
        ESoccerTeam::PlayerTeam), BarX, BarY, BarW * Share, BarH);
    HUDTopBandBottomY = BoxY + BoxHeight;
    HUDRightStatusNextY = HUDTopBandBottomY + 9.0f * UIScale;
}

void AGameHUD::DrawPeriodSummary5D()
{
    if (Canvas == nullptr || !IsValid(MatchManager) ||
        (MatchManager->GetCurrentMatchPeriod() != ESoccerMatchPeriod::HalfTime &&
         MatchManager->GetCurrentMatchPeriod() != ESoccerMatchPeriod::FullTime) ||
        IsClubSelectionMenuVisible() || IsFormationMenuVisible() ||
        IsQuickTacticsMenuVisible() || IsSubstitutionMenuVisible())
    {
        return;
    }

    UFont* Font = GEngine != nullptr ? GEngine->GetSmallFont() : nullptr;
    UFont* TitleFont = GEngine != nullptr ? GEngine->GetMediumFont() : nullptr;
    if (Font == nullptr || TitleFont == nullptr) return;

    const FSoccerMatchStatistics5D& Stats = MatchManager->GetMatchStatistics5D();
    const FSoccerMatchTeamStatistics5D& Local = Stats.PlayerTeam;
    const FSoccerMatchTeamStatistics5D& Rival = Stats.OpponentTeam;
    const float TotalPossession = Local.ControlledPossessionSeconds +
        Rival.ControlledPossessionSeconds;
    const float Share = TotalPossession > 0.01f
        ? Local.ControlledPossessionSeconds / TotalPossession : 0.5f;
    const bool bHasPossession = TotalPossession > 0.01f;
    const int32 LocalPercent = FMath::RoundToInt(Share * 100.0f);

    const float UIScale = SoccerHudPresentation5B::GetScale(Canvas);
    const float BoxW = FMath::Min(540.0f * UIScale, Canvas->SizeX - 24.0f * UIScale);
    const float BoxH = 372.0f * UIScale;
    const float X = (Canvas->SizeX - BoxW) * 0.5f;
    // Always below the referee's four-second decision banner if it is active.
    const float PreferredY = FMath::Max(Canvas->SizeY * 0.36f,
        HUDMainAnnouncementBottomY + 10.0f * UIScale);
    const float Y = FMath::Min(PreferredY,
        FMath::Max(HUDTopBandBottomY + 10.0f * UIScale,
            Canvas->SizeY - BoxH - 9.0f * UIScale));
    const float InnerX = X + 16.0f * UIScale;
    const float InnerW = BoxW - 32.0f * UIScale;
    const float CenterX = X + BoxW * 0.5f;

    DrawRect(FLinearColor(0.014f, 0.035f, 0.050f, 0.93f), X, Y, BoxW, BoxH);
    const FLinearColor LocalAccent = SoccerHudPresentation5B::GetClubAccent(MatchManager,
        ESoccerTeam::PlayerTeam);
    const FLinearColor RivalAccent = SoccerHudPresentation5B::GetClubAccent(MatchManager,
        ESoccerTeam::OpponentTeam);
    DrawRect(LocalAccent, X, Y, BoxW * 0.5f, 3.0f * UIScale);
    DrawRect(RivalAccent, CenterX, Y, BoxW * 0.5f, 3.0f * UIScale);

    const bool bFinal = MatchManager->GetCurrentMatchPeriod() == ESoccerMatchPeriod::FullTime;
    const FString Heading = bFinal ? TEXT("RESUMEN FINAL") : TEXT("RESUMEN DEL PRIMER TIEMPO");
    float HeadingScale = 1.18f * UIScale;
    float HeadingW = 0.0f, HeadingH = 0.0f;
    GetTextSize(Heading, HeadingW, HeadingH, TitleFont, HeadingScale);
    if (HeadingW > InnerW && HeadingW > 0.0f) HeadingScale *= InnerW / HeadingW;
    GetTextSize(Heading, HeadingW, HeadingH, TitleFont, HeadingScale);
    DrawText(Heading, FLinearColor::White,
        CenterX - HeadingW * 0.5f, Y + 12.0f * UIScale, TitleFont, HeadingScale);

    const FString Names[2] = {
        SoccerHudPresentation5B::GetClubName(MatchManager, ESoccerTeam::PlayerTeam),
        SoccerHudPresentation5B::GetClubName(MatchManager, ESoccerTeam::OpponentTeam)};
    const float NameMaxW = InnerW * 0.39f;
    for (int32 i=0; i<2; ++i)
    {
        FString Visible = Names[i];
        float Width=0.0f, Height=0.0f;
        float Scale=1.00f * UIScale;
        GetTextSize(Visible, Width, Height, Font, Scale);
        if (Width > NameMaxW && Width > 0.0f)
        {
            Scale = FMath::Max(0.65f * UIScale, Scale * NameMaxW / Width);
            GetTextSize(Visible, Width, Height, Font, Scale);
        }
        for (int32 k=Names[i].Len()-1; Width > NameMaxW && k>=3; --k)
        {
            Visible = Names[i].Left(k) + TEXT("...");
            GetTextSize(Visible, Width, Height, Font, Scale);
        }
        const float TeamTextX = i == 0 ? InnerX : X + BoxW - 16.0f * UIScale - Width;
        DrawText(Visible, FLinearColor(0.86f, 0.95f, 0.96f, 1.0f),
            TeamTextX, Y + 45.0f * UIScale, Font, Scale);
    }
    const FString Goals = FString::Printf(TEXT("%d - %d"),
        MatchManager->GetPlayerTeamScore(), MatchManager->GetOpponentTeamScore());
    float GoalsW=0.0f, GoalsH=0.0f;
    GetTextSize(Goals, GoalsW, GoalsH, TitleFont, 1.15f * UIScale);
    DrawText(Goals, FLinearColor::White, CenterX - GoalsW*0.5f,
        Y + 45.0f * UIScale, TitleFont, 1.15f * UIScale);

    const float BarX = InnerX;
    const float BarY = Y + 93.0f * UIScale;
    const float BarWidth = InnerW;
    DrawText(TEXT("POSESIÓN CONTROLADA"), FLinearColor(0.69f,0.82f,0.85f,1.0f),
        InnerX, Y + 72.0f * UIScale, Font, 0.87f * UIScale);
    const FString PossessionValue = bHasPossession
        ? FString::Printf(TEXT("%d%%  -  %d%%"), LocalPercent, 100-LocalPercent)
        : TEXT("--   /   --");
    float PossW=0.0f, PossH=0.0f;
    GetTextSize(PossessionValue, PossW, PossH, Font, 0.91f * UIScale);
    DrawText(PossessionValue, FLinearColor::White,
        X + BoxW - 16.0f * UIScale - PossW, Y + 72.0f * UIScale,
        Font, 0.91f * UIScale);
    DrawRect(RivalAccent, BarX, BarY, BarWidth, 8.0f * UIScale);
    DrawRect(LocalAccent, BarX, BarY, BarWidth * Share, 8.0f * UIScale);

    // Values represent confirmed referee decisions; offsides are attributed to
    // the offending side and other decisions to the beneficiary side.
    struct FDisplayStat5D { const TCHAR* Label; int32 Left; int32 Right; };
    const FDisplayStat5D Rows[] = {
        { TEXT("REMATES"), Local.Shots, Rival.Shots },
        { TEXT("AL ARCO"), Local.ShotsOnTarget, Rival.ShotsOnTarget },
        { TEXT("ATAJADAS"), Local.GoalkeeperSaves, Rival.GoalkeeperSaves },
        { TEXT("CÓRNERS"), Local.CornersWon, Rival.CornersWon },
        { TEXT("TIROS LIBRES"), Local.FreeKicksWon, Rival.FreeKicksWon },
        { TEXT("OFFSIDES"), Local.OffsidesCommitted, Rival.OffsidesCommitted },
        { TEXT("PENALES"), Local.PenaltiesWon, Rival.PenaltiesWon },
        { TEXT("CAMBIOS"),
            MatchManager->GetMatchSquadState(ESoccerTeam::PlayerTeam).SubstitutionHistory.Num(),
            MatchManager->GetMatchSquadState(ESoccerTeam::OpponentTeam).SubstitutionHistory.Num() }
    };
    const int32 NumberOfRows = static_cast<int32>(sizeof(Rows) / sizeof(Rows[0]));
    for (int32 i = 0; i < NumberOfRows; ++i)
    {
        const float RowY = Y + (114.0f + i*27.0f) * UIScale;
        if ((i % 2) == 0)
        {
            DrawRect(FLinearColor(0.065f,0.12f,0.145f,0.55f),
                InnerX, RowY - 2.0f * UIScale, InnerW, 25.0f * UIScale);
        }
        const FString LeftValue = FString::FromInt(Rows[i].Left);
        const FString RightValue = FString::FromInt(Rows[i].Right);
        float LeftW=0.0f, LeftH=0.0f, RightW=0.0f, RightH=0.0f;
        float LabelW=0.0f, LabelH=0.0f;
        const float RowScale = 1.00f * UIScale;
        GetTextSize(LeftValue, LeftW, LeftH, Font, RowScale);
        GetTextSize(RightValue, RightW, RightH, Font, RowScale);
        GetTextSize(Rows[i].Label, LabelW, LabelH, Font, 0.91f * UIScale);
        DrawText(LeftValue, FLinearColor::White,
            InnerX + 13.0f * UIScale, RowY, Font, RowScale);
        DrawText(Rows[i].Label, FLinearColor(0.72f,0.86f,0.89f,1.0f),
            CenterX - LabelW * 0.5f, RowY, Font, 0.91f * UIScale);
        DrawText(RightValue, FLinearColor::White,
            X + BoxW - 29.0f * UIScale - RightW, RowY, Font, RowScale);
    }
    DrawText(TEXT("Posesión: solo tiempo con control confirmado de pelota"),
        FLinearColor(0.60f,0.73f,0.76f,1.0f), InnerX,
        Y + 345.0f * UIScale, Font, 0.76f * UIScale);
}

void AGameHUD::DrawMatchScoreboard()
{
    if (Canvas == nullptr || !IsValid(MatchManager))
    {
        return;
    }
    UFont* ScoreFont = GEngine != nullptr ? GEngine->GetMediumFont() : nullptr;
    UFont* TeamFont = GEngine != nullptr ? GEngine->GetSmallFont() : nullptr;
    if (ScoreFont == nullptr || TeamFont == nullptr)
    {
        return;
    }

    const float UIScale = SoccerHudPresentation5B::GetScale(Canvas);
    const float Margin = 12.0f * UIScale;
    const float BoxWidth = FMath::Max(1.0f,
        FMath::Min(785.0f * UIScale, Canvas->SizeX - 2.0f * Margin));
    const float BoxHeight = 57.0f * UIScale;
    const float BoxX = (Canvas->SizeX - BoxWidth) * 0.5f;
    const float BoxY = 15.0f * UIScale;
    HUDTopBandBottomY = BoxY + BoxHeight;

    DrawRect(FLinearColor(0.012f, 0.028f, 0.046f, 0.89f),
        BoxX, BoxY, BoxWidth, BoxHeight);
    const FLinearColor LocalAccent = SoccerHudPresentation5B::GetClubAccent(
        MatchManager, ESoccerTeam::PlayerTeam);
    const FLinearColor RivalAccent = SoccerHudPresentation5B::GetClubAccent(
        MatchManager, ESoccerTeam::OpponentTeam);
    DrawRect(LocalAccent, BoxX, BoxY, BoxWidth * 0.5f, 3.0f * UIScale);
    DrawRect(RivalAccent, BoxX + BoxWidth * 0.5f, BoxY,
        BoxWidth * 0.5f, 3.0f * UIScale);

    const FString GoalsText = FString::Printf(TEXT("%d  -  %d"),
        MatchManager->GetPlayerTeamScore(), MatchManager->GetOpponentTeamScore());
    const float GoalsScale = 1.75f * UIScale;
    float GoalsWidth = 0.0f;
    float GoalsHeight = 0.0f;
    GetTextSize(GoalsText, GoalsWidth, GoalsHeight, ScoreFont, GoalsScale);
    const float MidX = Canvas->SizeX * 0.5f;
    DrawText(GoalsText, FLinearColor::White,
        MidX - GoalsWidth * 0.5f,
        BoxY + (BoxHeight - GoalsHeight) * 0.5f,
        ScoreFont, GoalsScale);

    // Separate team name slots keep the score in the exact center.
    const float SideWidth = FMath::Max(36.0f * UIScale,
        (BoxWidth - GoalsWidth - 58.0f * UIScale) * 0.5f);
    const float TeamStartX[2] = {
        MidX - GoalsWidth * 0.5f - 15.0f * UIScale - SideWidth,
        MidX + GoalsWidth * 0.5f + 15.0f * UIScale
    };
    const ESoccerTeam Teams[2] = {
        ESoccerTeam::PlayerTeam, ESoccerTeam::OpponentTeam
    };
    for (int32 TeamIndex = 0; TeamIndex < 2; ++TeamIndex)
    {
        const FString ClubName = SoccerHudPresentation5B::GetClubName(MatchManager, Teams[TeamIndex]);
        FString VisibleName = ClubName;
        float TeamScale = 1.13f * UIScale;
        float NameWidth = 0.0f;
        float NameHeight = 0.0f;
        GetTextSize(VisibleName, NameWidth, NameHeight, TeamFont, TeamScale);
        if (NameWidth > SideWidth && NameWidth > 0.0f)
        {
            TeamScale = FMath::Max(0.66f * UIScale,
                TeamScale * SideWidth / NameWidth);
            GetTextSize(VisibleName, NameWidth, NameHeight, TeamFont, TeamScale);
        }
        for (int32 NameLength = ClubName.Len() - 1;
            NameWidth > SideWidth && NameLength >= 3; --NameLength)
        {
            VisibleName = ClubName.Left(NameLength) + TEXT("...");
            GetTextSize(VisibleName, NameWidth, NameHeight, TeamFont, TeamScale);
        }
        const float NameX = TeamIndex == 0
            ? TeamStartX[0] + SideWidth - NameWidth
            : TeamStartX[1];
        DrawText(VisibleName, FLinearColor(0.94f, 0.97f, 1.0f, 1.0f),
            NameX, BoxY + (BoxHeight - NameHeight) * 0.5f,
            TeamFont, TeamScale);
    }
}
