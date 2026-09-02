#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "SoccerTeamSetupTypes.h"
#include "SoccerMatchSetupTypes.h"
#include "SoccerGameInstance.generated.h"

class USoccerPlayerProfile;
class USoccerSquadCatalog;

/**
 * Session owner for the persistent coach/team configuration.
 *
 * Init() loads (or creates) the SaveGame. Mutating functions autosave by default,
 * so future manager UI code only needs to call this API and does not write files.
 */
UCLASS(BlueprintType)
class THIRDPERSONCPP_API USoccerGameInstance : public UGameInstance
{
    GENERATED_BODY()

public:
    USoccerGameInstance();

    virtual void Init() override;
    virtual void Shutdown() override;

    /** Copy of the currently loaded team setup for Blueprint/UI consumers. */
    UFUNCTION(BlueprintPure, Category = "Soccer|Team Setup")
    FSoccerTeamSetup GetCurrentTeamSetup() const;

    UFUNCTION(BlueprintPure, Category = "Soccer|Team Setup")
    bool HasLoadedTeamSetup() const;

    /** Explicit disk operations. Normal UI changes use autosave automatically. */
    UFUNCTION(BlueprintCallable, Category = "Soccer|Team Setup|Persistence")
    bool SaveTeamSetup();

    UFUNCTION(BlueprintCallable, Category = "Soccer|Team Setup|Persistence")
    bool ReloadTeamSetup();

    /** Restores a clean default formation/tactic and persists it. */
    UFUNCTION(BlueprintCallable, Category = "Soccer|Team Setup|Persistence")
    bool ResetTeamSetupToDefaults();

    /** Formation changes preserve compatible assignments; removed slots go to the bench. */
    UFUNCTION(BlueprintCallable, Category = "Soccer|Team Setup|Formation")
    bool SetFormationSystem(ESoccerFormationSystem NewFormationSystem);

    UFUNCTION(BlueprintCallable, Category = "Soccer|Team Setup|Tactics")
    bool SetTacticalPlan(const FSoccerTeamTacticalPlan& NewTacticalPlan);

    UFUNCTION(BlueprintCallable, Category = "Soccer|Team Setup|Tactics")
    bool SetSlotTacticalInstruction(const FSoccerSlotTacticalInstruction& NewInstruction);

    /** Stores formation + collective plan + slot instructions with one autosave. */
    UFUNCTION(BlueprintCallable, Category = "Soccer|Team Setup|Tactics")
    bool SetCoachStrategySnapshot(
        ESoccerFormationSystem NewFormationSystem,
        const FSoccerTeamTacticalPlan& NewTacticalPlan,
        const TArray<FSoccerSlotTacticalInstruction>& NewSlotInstructions
    );

    /**
     * Synchronizes the content-side roster with persistent IDs. Existing lineup
     * and bench choices are preserved for players that still exist; removed
     * content players are discarded and newly added players enter the bench.
     */
    UFUNCTION(BlueprintCallable, Category = "Soccer|Team Setup|Squad")
    bool SynchronizeSquadWithPlayerIds(const TArray<FName>& AuthoritativePlayerIds);

    /** Adds a profile ID to the persistent squad if it is not already present. */
    UFUNCTION(BlueprintCallable, Category = "Soccer|Team Setup|Squad")
    bool AddPlayerToSquad(FName NewPlayerId);

    /** Removes the player from squad, lineup and bench. */
    UFUNCTION(BlueprintCallable, Category = "Soccer|Team Setup|Squad")
    bool RemovePlayerFromSquad(FName ExistingPlayerId);

    /**
     * Assigns a player to a legal slot in the current formation.
     * If the slot already has a player, that displaced player moves to the bench.
     */
    UFUNCTION(BlueprintCallable, Category = "Soccer|Team Setup|Lineup")
    bool AssignPlayerToStartingSlot(FName PlayerIdToAssign, FName FormationSlotId);

    /** Removes the player from any starting slot and places them on the bench. */
    UFUNCTION(BlueprintCallable, Category = "Soccer|Team Setup|Lineup")
    bool MovePlayerToBench(FName PlayerIdToBench);

    /** Moves the current occupant of a starting slot to the bench. */
    UFUNCTION(BlueprintCallable, Category = "Soccer|Team Setup|Lineup")
    bool MoveStartingSlotPlayerToBench(FName FormationSlotId);

    /**
     * Swaps the occupants of two legal starting slots. Empty slots are allowed,
     * so this can also move one starter without involving the bench.
     */
    UFUNCTION(BlueprintCallable, Category = "Soccer|Team Setup|Lineup")
    bool SwapStartingSlots(FName FirstFormationSlotId, FName SecondFormationSlotId);

    /**
     * Replaces a starter with a bench player. The outgoing starter occupies the
     * incoming substitute's previous bench index so bench ordering remains stable.
     */
    UFUNCTION(BlueprintCallable, Category = "Soccer|Team Setup|Lineup")
    bool SwapStarterWithBench(FName FormationSlotId, FName BenchPlayerId);

    /** Persists user-selected substitute ordering for the future coach UI. */
    UFUNCTION(BlueprintCallable, Category = "Soccer|Team Setup|Lineup")
    bool ReorderBenchPlayer(FName BenchPlayerId, int32 NewBenchIndex);

    UFUNCTION(BlueprintPure, Category = "Soccer|Team Setup|Lineup")
    FName GetPlayerInStartingSlot(FName FormationSlotId) const;

    UFUNCTION(BlueprintPure, Category = "Soccer|Team Setup|Lineup")
    FName FindStartingSlotForPlayer(FName PlayerIdToFind) const;

    UFUNCTION(BlueprintPure, Category = "Soccer|Team Setup|Lineup")
    bool IsPlayerOnBench(FName PlayerIdToFind) const;

    UFUNCTION(BlueprintPure, Category = "Soccer|Team Setup|Squad")
    bool IsPlayerInSquad(FName PlayerIdToFind) const;

    /** Ordered persistent squad/bench views for the future manager UI. */
    UFUNCTION(BlueprintPure, Category = "Soccer|Team Setup|Squad")
    TArray<FName> GetSquadPlayerIds() const;

    UFUNCTION(BlueprintPure, Category = "Soccer|Team Setup|Lineup")
    TArray<FName> GetBenchPlayerIds() const;

    /** Seven entries in formation-slot order; empty positions are NAME_None. */
    UFUNCTION(BlueprintPure, Category = "Soccer|Team Setup|Lineup")
    TArray<FName> GetStartingPlayerIdsInFormationOrder() const;

    UFUNCTION(BlueprintPure, Category = "Soccer|Team Setup|Lineup")
    TArray<FName> GetEmptyStartingSlotIds() const;

    UFUNCTION(BlueprintPure, Category = "Soccer|Team Setup|Lineup")
    int32 GetStartingPlayerCount() const;

    /** True only when every one of the seven legal formation slots is occupied. */
    UFUNCTION(BlueprintPure, Category = "Soccer|Team Setup|Lineup")
    bool HasCompleteStartingLineup() const;

    /** Runtime registry used by the match to resolve persistent PlayerIds back to Data Assets. */
    UFUNCTION(BlueprintCallable, Category = "Soccer|Player Profiles")
    bool RefreshPlayerProfileRegistry();

    UFUNCTION(BlueprintPure, Category = "Soccer|Player Profiles")
    USoccerPlayerProfile* FindPlayerProfileById(FName PlayerIdToFind) const;

    UFUNCTION(BlueprintPure, Category = "Soccer|Player Profiles")
    int32 GetResolvedPlayerProfileCount() const;

    /** Stage 9E: all valid club IDs discovered from SoccerSquadCatalog assets. */
    UFUNCTION(BlueprintPure, Category = "Soccer|Clubs")
    TArray<FName> GetAvailableClubIds() const;

    UFUNCTION(BlueprintPure, Category = "Soccer|Clubs")
    USoccerSquadCatalog* FindSquadCatalogByClubId(FName ClubIdToFind) const;

    UFUNCTION(BlueprintPure, Category = "Soccer|Clubs")
    USoccerSquadCatalog* GetDefaultHumanSquadCatalog() const;

    /** Initial content-side membership. Future transfers will override this in SaveGame. */
    UFUNCTION(BlueprintPure, Category = "Soccer|Clubs")
    FName GetInitialClubIdForPlayer(FName PlayerIdToFind) const;

    UFUNCTION(BlueprintPure, Category = "Soccer|Match Clubs")
    FName GetDefaultHumanClubId() const;

    UFUNCTION(BlueprintPure, Category = "Soccer|Match Clubs")
    FName GetSelectedOpponentClubId() const;

    /** Session/match choice. It is intentionally separate from team lineup persistence. */
    UFUNCTION(BlueprintCallable, Category = "Soccer|Match Clubs")
    bool SetSelectedOpponentClubId(FName OpponentClubId);

    UFUNCTION(BlueprintPure, Category = "Soccer|Match Setup")
    FSoccerMatchSetup GetCurrentMatchSetup() const;

    /** Creates/updates the standalone friendly used by the pre-match screen. */
    UFUNCTION(BlueprintCallable, Category = "Soccer|Match Setup")
    bool ConfigureStandaloneMatch(FName HumanClubId, FName OpponentClubId);

    static FString GetTeamSaveSlotName();
    static int32 GetCurrentSaveFormatVersion();
    static int32 GetCurrentTeamSetupDataVersion();

protected:
    /** Autosave is intentionally on from the first persistence stage. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Soccer|Team Setup|Persistence")
    bool bAutoSaveTeamChanges = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Soccer|Team Setup")
    FSoccerTeamSetup CurrentTeamSetup;

private:
    bool LoadOrCreateTeamSetup();
    bool PersistIfNeeded();
    void MarkTeamSetupChanged();
    void BuildDefaultTeamSetup(FSoccerTeamSetup& OutTeamSetup) const;
    void NormalizeLoadedTeamSetup(FSoccerTeamSetup& InOutTeamSetup) const;
    void RebuildSlotInstructionsForCurrentFormation(FSoccerTeamSetup& InOutTeamSetup) const;
    bool IsValidSlotForFormation(FName FormationSlotId, ESoccerFormationSystem FormationSystem) const;
    void AddPlayerToBenchIfNeeded(FSoccerTeamSetup& InOutTeamSetup, FName PlayerIdToAdd) const;
    void RemovePlayerFromBench(FSoccerTeamSetup& InOutTeamSetup, FName PlayerIdToRemove) const;
    void RemovePlayerFromStartingLineup(FSoccerTeamSetup& InOutTeamSetup, FName PlayerIdToRemove) const;
    void EnsurePlayerExistsInSquad(FSoccerTeamSetup& InOutTeamSetup, FName PlayerIdToEnsure) const;

    UPROPERTY(Transient)
    TMap<FName, USoccerPlayerProfile*> RuntimePlayerProfilesById;

    UPROPERTY(Transient)
    USoccerSquadCatalog* RuntimePlayerTeamCatalog = nullptr;

    UPROPERTY(Transient)
    TMap<FName, USoccerSquadCatalog*> RuntimeSquadCatalogsByClubId;

    UPROPERTY(Transient)
    TMap<FName, FName> RuntimeInitialClubIdByPlayerId;

    UPROPERTY(Transient)
    FSoccerMatchSetup CurrentMatchSetup;

    bool bTeamSetupLoaded = false;
    bool bTeamSetupDirty = false;
};
