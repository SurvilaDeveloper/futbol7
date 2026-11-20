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
