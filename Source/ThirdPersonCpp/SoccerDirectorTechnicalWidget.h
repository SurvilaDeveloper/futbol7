#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SoccerFormationTypes.h"
#include "SoccerDirectorTechnicalWidget.generated.h"

class UBorder;
class UButton;
class UCanvasPanel;
class UImage;
class UScrollBox;
class USoccerGameInstance;
class USoccerPlayerProfile;
class USoccerSquadCatalog;
class UTextBlock;
class UVerticalBox;

/**
 * Native squad/lineup page embedded in the existing coach menu.
 *
 * Static player definitions are resolved from a SoccerSquadCatalog. If no
 * PlayerTeam catalog exists yet, editor/runtime discovery of all
 * SoccerPlayerProfile assets is used as a temporary fallback so this stage can
 * be tested immediately. The actual user decisions always live in SaveGame.
 */
UCLASS()
class THIRDPERSONCPP_API USoccerDirectorTechnicalWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** Re-resolves the persistent setup and redraws roster, pitch and details. */
    void RefreshFromPersistentTeamSetup(bool bRediscoverProfiles = false);

    /** Keyboard/gamepad hooks delegated by the parent coach menu. */
    void NavigatePlayer(int32 Direction);
    void NavigateFormationSlot(int32 Direction);
    void ConfirmFocusedSlotAssignment();

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeConstruct() override;

private:
    void BuildWidgetTree();
    void ResolveGameInstance();
    void DiscoverPlayerProfiles();
    void SynchronizePersistentSquad();

    void RefreshFormationHeader();
    void RefreshRosterList();
    void RefreshFormationPitch();
    void RefreshBenchList();
    void RefreshSelectedPlayerDetails();
    void RefreshSelectionSummary();

    void SelectPlayer(FName PlayerId);
    void SelectFormationSlot(FName FormationSlotId);
    void AssignSelectedPlayerToSlot(FName FormationSlotId);

    USoccerPlayerProfile* FindProfile(FName PlayerId) const;
    FString GetPlayerDisplayName(FName PlayerId) const;
    FString GetPlayerStatusLabel(FName PlayerId) const;
    TArray<FName> GetOrderedAvailablePlayerIds() const;
    TArray<FName> GetCurrentFormationSlotIds() const;
    FName ResolveSafeFocusedSlotId() const;

    void AddAttributeGroupHeader(UVerticalBox* Parent, const FString& Label);
    void AddAttributeRow(UVerticalBox* Parent, const FString& Label, int32 Value);
    void AddPositionPreferenceRows(UVerticalBox* Parent, const USoccerPlayerProfile* Profile);

    UFUNCTION()
    void HandleRosterPlayerClicked(FName PlayerId);

    UFUNCTION()
    void HandleBenchPlayerClicked(FName PlayerId);

    UFUNCTION()
    void HandleFormationSlotClicked(FName FormationSlotId);

    UFUNCTION()
    void HandlePreviousFormationClicked();

    UFUNCTION()
    void HandleNextFormationClicked();

    UFUNCTION()
    void HandleMoveSelectedToBenchClicked();

    UFUNCTION()
    void HandleBenchMoveUpClicked();

    UFUNCTION()
    void HandleBenchMoveDownClicked();

    UPROPERTY()
    USoccerGameInstance* SoccerGameInstance = nullptr;

    UPROPERTY()
    USoccerSquadCatalog* ActiveSquadCatalog = nullptr;

    UPROPERTY()
    TArray<USoccerPlayerProfile*> AvailableProfiles;

    UPROPERTY()
    TMap<FName, USoccerPlayerProfile*> ProfilesById;

    UPROPERTY()
    UTextBlock* DataSourceText = nullptr;

    UPROPERTY()
    UTextBlock* FormationText = nullptr;

    UPROPERTY()
    UTextBlock* LineupStatusText = nullptr;

    UPROPERTY()
    UScrollBox* RosterScrollBox = nullptr;

    UPROPERTY()
    UCanvasPanel* FormationCanvas = nullptr;

    UPROPERTY()
    UVerticalBox* BenchListBox = nullptr;

    UPROPERTY()
    UTextBlock* SelectedPlayerNameText = nullptr;

    UPROPERTY()
    UTextBlock* SelectedPlayerMetaText = nullptr;

    UPROPERTY()
    UImage* SelectedPlayerPortraitImage = nullptr;

    UPROPERTY()
    UTextBlock* SelectedPlayerPortraitFallbackText = nullptr;

    UPROPERTY()
    UTextBlock* SelectedSlotSuitabilityText = nullptr;

    UPROPERTY()
    UVerticalBox* PlayerAttributesBox = nullptr;

    UPROPERTY()
    UTextBlock* SelectionSummaryText = nullptr;

    UPROPERTY()
    UButton* MoveSelectedToBenchButton = nullptr;

    UPROPERTY()
    UButton* BenchMoveUpButton = nullptr;

    UPROPERTY()
    UButton* BenchMoveDownButton = nullptr;

    FName SelectedPlayerId = NAME_None;
    FName FocusedFormationSlotId = NAME_None;

    bool bProfilesDiscovered = false;

    float PitchCanvasWidth = 550.0f;
    float PitchCanvasHeight = 540.0f;
};
