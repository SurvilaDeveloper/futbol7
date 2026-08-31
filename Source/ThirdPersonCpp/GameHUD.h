#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "GameHUD.generated.h"

class ASoccerMatchManager;
class ASoccerDebugManager;
class USoccerFormationMenuWidget;
class USoccerQuickTacticsWidget;
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

	void ShowQuickTacticsFeedback(const FString& Message);

private:
	void FindMatchManager();
	void EnsureTacticalPresetManager();
	void DrawQuickTacticsFeedback();
	void DrawInstantReplayOverlay();

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
		USoccerQuickTacticsWidget* QuickTacticsWidget = nullptr;

	UPROPERTY()
		USoccerTacticalPresetManager* TacticalPresetManager = nullptr;

	FString QuickTacticsFeedbackText;
	double QuickTacticsFeedbackExpiryRealTime = -1.0;
};
