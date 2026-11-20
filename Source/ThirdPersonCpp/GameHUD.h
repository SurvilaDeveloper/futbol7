#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "GameHUD.generated.h"

class ASoccerMatchManager;
class ASoccerDebugManager;
class USoccerFormationMenuWidget;
class USoccerClubSelectionWidget;
class USoccerQuickTacticsWidget;
class USoccerSubstitutionMenuWidget;
class USoccerTacticalPresetManager;
class USoccerPeriodSummaryWidget;
class USoccerMatchSessionWidget5M;

UCLASS()
class THIRDPERSONCPP_API AGameHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void DrawHUD() override;

	void ShowFormationMenu();
	void HideFormationMenu();
	void ToggleFormationMenu();
	bool IsFormationMenuVisible() const;

	void ShowQuickTacticsMenu();
	void HideQuickTacticsMenu();
	void ToggleQuickTacticsMenu();
	bool IsQuickTacticsMenuVisible() const;

	void ShowSubstitutionMenu();
	void HideSubstitutionMenu();
	void ToggleSubstitutionMenu();
	bool IsSubstitutionMenuVisible() const;

	void ShowClubSelectionMenu();
	bool IsClubSelectionMenuVisible() const;

	void ShowQuickTacticsFeedback(const FString& Message);

	// Called only by the full-time statistics overlay.
	void DismissFinalSummary5F();
    void TogglePauseMenu5M();
    void ResumeMatch5M();
    void AbortMatch5M();
    void StartNewMatch5M();
    void ReturnToMainMenu6A();
    void DismissPostMatchMenu5M();


private:
	void OpenInitialMatchMenu();

	UFUNCTION()
	void HandleClubSelectionConfirmed();

	void FindMatchManager();
	void EnsureTacticalPresetManager();
	void DrawQuickTacticsFeedback();
	void DrawPlayerStatusPanel();
	void DrawRightStatusCard(const FString& Message, const FLinearColor& TextColor, float FontScale);
	void DrawInstantReplayOverlay();
	void DrawSubstitutionPresentation();
	FString ResolvePlayerDisplayName(FName PlayerId) const;

    void ObserveMatchAnnouncement();
    void DrawMatchAnnouncement();

	void DrawMatchScoreboard();
	void DrawCompactPossession5D();
	void DrawPeriodSummary5D();
	void UpdatePeriodSummary5F();
    void UpdateMatchSession5M();
    void ShowMatchSession5M(bool bFinished);
	bool IsPeriodSummaryVisible5F() const;


	void DrawMatchClock();

	void DrawHumanPassRequestIndicator();

	void DrawHumanJumpHeaderIndicator();

	void DrawSoccerDebugTextFeed();

	void DrawSoccerDebugPanel();

	FString GetMatchStateText() const;

	UPROPERTY()
		ASoccerMatchManager* MatchManager = nullptr;

	UPROPERTY()
		ASoccerDebugManager* DebugManager = nullptr;

	UPROPERTY()
		USoccerFormationMenuWidget* FormationMenuWidget = nullptr;

	UPROPERTY()
		USoccerClubSelectionWidget* ClubSelectionWidget = nullptr;

	UPROPERTY()
		USoccerQuickTacticsWidget* QuickTacticsWidget = nullptr;

	UPROPERTY()
		USoccerSubstitutionMenuWidget* SubstitutionMenuWidget = nullptr;

	UPROPERTY()
		USoccerTacticalPresetManager* TacticalPresetManager = nullptr;

	UPROPERTY(Transient)
	USoccerPeriodSummaryWidget* PeriodSummaryWidget5F = nullptr;
    UPROPERTY(Transient) USoccerMatchSessionWidget5M* SessionWidget5M = nullptr;
    bool bFinishedSessionDismissed5M = false;
    bool bSessionAppliedPause5M = false;
    bool bSessionPreviousCursor5M = false;
    bool bOpponentConfirmedForSession5M = false;
	bool bFinalSummaryDismissed5F = false;
	bool bPeriodSummaryPreviousCursor5F = false;


	// Calculated every DrawHUD: keeps overlays separated at different resolutions.
	float HUDTopBandBottomY = 0.0f;
	float HUDRightStatusNextY = 0.0f;
	float HUDMainAnnouncementBottomY = 0.0f;

    uint32 ObservedAnnouncementRevision = 0;
    FString MatchAnnouncementTitle;
    FString MatchAnnouncementDetail;
    FLinearColor MatchAnnouncementAccent = FLinearColor::White;
    double MatchAnnouncementStartRealTime = -1.0;
    double MatchAnnouncementExpiryRealTime = -1.0;

	FString QuickTacticsFeedbackText;
	double QuickTacticsFeedbackExpiryRealTime = -1.0;
	FString SubstitutionPresentationText;
	double SubstitutionPresentationExpiryRealTime = -1.0;
	int32 ObservedPlayerTeamSubstitutionCount = 0;
	int32 ObservedOpponentTeamSubstitutionCount = 0;
	bool bSubstitutionHistoryObserved = false;

    // Stage 5C: short contextual notices (real time; independent of game pause).
    // HUD state only. Never changes the match, possession or player data.
    bool bHudIdentityInitialized = false;
    FName ObservedHudPlayerId = NAME_None;
    bool bLowEnergyNoticeArmed = true;
    FString PlayerContextNoticeText;
    FLinearColor PlayerContextNoticeAccent = FLinearColor::White;
    double PlayerContextNoticeStartedAt = -1.0;
    double PlayerContextNoticeExpiresAt = -1.0;

};
