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
	void DrawInstantReplayOverlay();
	void DrawSubstitutionPresentation();
	FString ResolvePlayerDisplayName(FName PlayerId) const;

	void DrawMatchScoreboard();

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

	FString QuickTacticsFeedbackText;
	double QuickTacticsFeedbackExpiryRealTime = -1.0;
	FString SubstitutionPresentationText;
	double SubstitutionPresentationExpiryRealTime = -1.0;
	int32 ObservedPlayerTeamSubstitutionCount = 0;
	int32 ObservedOpponentTeamSubstitutionCount = 0;
	bool bSubstitutionHistoryObserved = false;
};
