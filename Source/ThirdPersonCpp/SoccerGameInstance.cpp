#include "SoccerGameInstance.h"

#include "Kismet/GameplayStatics.h"
#include "SoccerFormationLibrary.h"
#include "SoccerTeamSaveGame.h"

DEFINE_LOG_CATEGORY_STATIC(LogSoccerTeamPersistence, Log, All);

namespace
{
    const FString TeamSetupSaveSlotName = TEXT("SoccerTeamSetup");
    const int32 TeamSetupSaveUserIndex = 0;
    const int32 CurrentTeamSetupSaveFormatVersion = 1;
    const int32 CurrentTeamSetupDataVersion = 1;

    void AddUniqueValidPlayerId(TArray<FName>& PlayerIds, FName CandidatePlayerId)
    {
        if (!CandidatePlayerId.IsNone() && !PlayerIds.Contains(CandidatePlayerId))
        {
            PlayerIds.Add(CandidatePlayerId);
        }
    }

    void RemoveInvalidAndDuplicatePlayerIds(TArray<FName>& PlayerIds)
    {
        TArray<FName> NormalizedIds;
        NormalizedIds.Reserve(PlayerIds.Num());

        for (const FName CandidatePlayerId : PlayerIds)
        {
            AddUniqueValidPlayerId(NormalizedIds, CandidatePlayerId);
        }

        PlayerIds = MoveTemp(NormalizedIds);
    }
}

USoccerGameInstance::USoccerGameInstance()
{
    bAutoSaveTeamChanges = true;
}

void USoccerGameInstance::Init()
{
    Super::Init();

    const bool bFormationCatalogValid =
        SoccerFormationLibrary::IsBuiltInCatalogValid();

    if (bFormationCatalogValid)
    {
        UE_LOG(
            LogSoccerTeamPersistence,
            Display,
            TEXT("[TeamSetup] Formation catalog ready: %d valid seven-a-side systems."),
            SoccerFormationLibrary::GetAllSystems().Num()
        );
    }
    else
    {
        UE_LOG(
            LogSoccerTeamPersistence,
            Error,
            TEXT("[TeamSetup] Built-in formation catalog validation failed.")
        );
    }

    bTeamSetupLoaded = LoadOrCreateTeamSetup();

    if (bTeamSetupLoaded)
    {
        UE_LOG(
            LogSoccerTeamPersistence,
            Display,
            TEXT("[TeamSetup] Ready. Formation=%d, squad=%d, starters=%d, bench=%d, slot='%s'."),
            static_cast<int32>(CurrentTeamSetup.FormationSystem),
            CurrentTeamSetup.SquadPlayerIds.Num(),
            CurrentTeamSetup.StartingLineupBySlot.Num(),
            CurrentTeamSetup.BenchPlayerIds.Num(),
            *GetTeamSaveSlotName()
        );
    }
}

void USoccerGameInstance::Shutdown()
{
    if (bTeamSetupDirty)
    {
        SaveTeamSetup();
    }

    Super::Shutdown();
}

FSoccerTeamSetup USoccerGameInstance::GetCurrentTeamSetup() const
{
    return CurrentTeamSetup;
}

bool USoccerGameInstance::HasLoadedTeamSetup() const
{
    return bTeamSetupLoaded;
}

bool USoccerGameInstance::SaveTeamSetup()
{
    NormalizeLoadedTeamSetup(CurrentTeamSetup);

    USoccerTeamSaveGame* NewSaveObject = Cast<USoccerTeamSaveGame>(
        UGameplayStatics::CreateSaveGameObject(USoccerTeamSaveGame::StaticClass())
    );

    if (NewSaveObject == nullptr)
    {
        UE_LOG(LogSoccerTeamPersistence, Error, TEXT("[TeamSetup] Could not create SaveGame object."));
        return false;
    }

    NewSaveObject->SaveFormatVersion = CurrentTeamSetupSaveFormatVersion;
    NewSaveObject->TeamSetup = CurrentTeamSetup;
    NewSaveObject->TeamSetup.DataVersion = CurrentTeamSetupDataVersion;

    const bool bSavedSuccessfully = UGameplayStatics::SaveGameToSlot(
        NewSaveObject,
        TeamSetupSaveSlotName,
        TeamSetupSaveUserIndex
    );

    if (bSavedSuccessfully)
    {
        bTeamSetupDirty = false;
        bTeamSetupLoaded = true;

        UE_LOG(
            LogSoccerTeamPersistence,
            Display,
            TEXT("[TeamSetup] Saved to Saved/SaveGames/%s.sav."),
            *TeamSetupSaveSlotName
        );
    }
    else
    {
        UE_LOG(LogSoccerTeamPersistence, Error, TEXT("[TeamSetup] SaveGameToSlot failed."));
    }

    return bSavedSuccessfully;
}

bool USoccerGameInstance::ReloadTeamSetup()
{
    const bool bReloadedSuccessfully = LoadOrCreateTeamSetup();
    bTeamSetupLoaded = bReloadedSuccessfully;
    return bReloadedSuccessfully;
}

bool USoccerGameInstance::ResetTeamSetupToDefaults()
{
    BuildDefaultTeamSetup(CurrentTeamSetup);
    bTeamSetupLoaded = true;
    bTeamSetupDirty = true;
    return SaveTeamSetup();
}

bool USoccerGameInstance::SetFormationSystem(ESoccerFormationSystem NewFormationSystem)
{
    if (CurrentTeamSetup.FormationSystem == NewFormationSystem)
    {
        return true;
    }

    const FSoccerFormationDefinition& NewDefinition =
        SoccerFormationLibrary::GetDefinition(NewFormationSystem);

    if (!SoccerFormationLibrary::IsValidSevenASideDefinition(NewDefinition))
    {
        UE_LOG(LogSoccerTeamPersistence, Warning, TEXT("[TeamSetup] Rejected invalid formation."));
        return false;
    }

    TArray<FName> RemovedSlotIds;
    TArray<FName> PlayersFromRemovedSlots;
    for (const TPair<FName, FName>& LineupEntry : CurrentTeamSetup.StartingLineupBySlot)
    {
        if (!IsValidSlotForFormation(LineupEntry.Key, NewFormationSystem))
        {
            RemovedSlotIds.Add(LineupEntry.Key);
            AddUniqueValidPlayerId(PlayersFromRemovedSlots, LineupEntry.Value);
        }
    }

    for (const FName RemovedSlotId : RemovedSlotIds)
    {
        CurrentTeamSetup.StartingLineupBySlot.Remove(RemovedSlotId);
    }

    for (const FName PlayerIdFromRemovedSlot : PlayersFromRemovedSlots)
    {
        AddPlayerToBenchIfNeeded(CurrentTeamSetup, PlayerIdFromRemovedSlot);
    }

    CurrentTeamSetup.FormationSystem = NewFormationSystem;
    RebuildSlotInstructionsForCurrentFormation(CurrentTeamSetup);
    NormalizeLoadedTeamSetup(CurrentTeamSetup);
    MarkTeamSetupChanged();
    return PersistIfNeeded();
}

bool USoccerGameInstance::SetTacticalPlan(const FSoccerTeamTacticalPlan& NewTacticalPlan)
{
    CurrentTeamSetup.TacticalPlan = NewTacticalPlan;
    MarkTeamSetupChanged();
    return PersistIfNeeded();
}

bool USoccerGameInstance::SetSlotTacticalInstruction(
    const FSoccerSlotTacticalInstruction& NewInstruction
)
{
    if (
        NewInstruction.SlotId.IsNone() ||
        !IsValidSlotForFormation(NewInstruction.SlotId, CurrentTeamSetup.FormationSystem)
    )
    {
        return false;
    }

    FSoccerSlotTacticalInstruction* ExistingInstruction =
        CurrentTeamSetup.SlotInstructions.FindByPredicate(
            [&NewInstruction](const FSoccerSlotTacticalInstruction& CandidateInstruction)
            {
                return CandidateInstruction.SlotId == NewInstruction.SlotId;
            }
        );

    if (ExistingInstruction != nullptr)
    {
        *ExistingInstruction = NewInstruction;
    }
    else
    {
        CurrentTeamSetup.SlotInstructions.Add(NewInstruction);
    }

    RebuildSlotInstructionsForCurrentFormation(CurrentTeamSetup);
    MarkTeamSetupChanged();
    return PersistIfNeeded();
}

bool USoccerGameInstance::AddPlayerToSquad(FName NewPlayerId)
{
    if (NewPlayerId.IsNone())
    {
        return false;
    }

    if (CurrentTeamSetup.SquadPlayerIds.Contains(NewPlayerId))
    {
        return true;
    }

    CurrentTeamSetup.SquadPlayerIds.Add(NewPlayerId);
    AddPlayerToBenchIfNeeded(CurrentTeamSetup, NewPlayerId);
    MarkTeamSetupChanged();
    return PersistIfNeeded();
}

bool USoccerGameInstance::RemovePlayerFromSquad(FName ExistingPlayerId)
{
    if (ExistingPlayerId.IsNone())
    {
        return false;
    }

    const bool bWasInSquad = CurrentTeamSetup.SquadPlayerIds.Remove(ExistingPlayerId) > 0;
    const bool bWasOnBench = CurrentTeamSetup.BenchPlayerIds.Remove(ExistingPlayerId) > 0;

    const FName PreviousStartingSlot = FindStartingSlotForPlayer(ExistingPlayerId);
    const bool bWasStarting = !PreviousStartingSlot.IsNone();
    RemovePlayerFromStartingLineup(CurrentTeamSetup, ExistingPlayerId);

    if (!bWasInSquad && !bWasOnBench && !bWasStarting)
    {
        return false;
    }

    MarkTeamSetupChanged();
    return PersistIfNeeded();
}

bool USoccerGameInstance::AssignPlayerToStartingSlot(
    FName PlayerIdToAssign,
    FName FormationSlotId
)
{
    if (
        PlayerIdToAssign.IsNone() ||
        FormationSlotId.IsNone() ||
        !IsValidSlotForFormation(FormationSlotId, CurrentTeamSetup.FormationSystem)
    )
    {
        return false;
    }

    EnsurePlayerExistsInSquad(CurrentTeamSetup, PlayerIdToAssign);

    FName DisplacedPlayerId = NAME_None;
    if (const FName* ExistingPlayerId = CurrentTeamSetup.StartingLineupBySlot.Find(FormationSlotId))
    {
        DisplacedPlayerId = *ExistingPlayerId;
    }

    RemovePlayerFromStartingLineup(CurrentTeamSetup, PlayerIdToAssign);
    RemovePlayerFromBench(CurrentTeamSetup, PlayerIdToAssign);

    if (!DisplacedPlayerId.IsNone() && DisplacedPlayerId != PlayerIdToAssign)
    {
        AddPlayerToBenchIfNeeded(CurrentTeamSetup, DisplacedPlayerId);
    }

    CurrentTeamSetup.StartingLineupBySlot.Add(FormationSlotId, PlayerIdToAssign);
    NormalizeLoadedTeamSetup(CurrentTeamSetup);
    MarkTeamSetupChanged();
    return PersistIfNeeded();
}

bool USoccerGameInstance::MovePlayerToBench(FName PlayerIdToBench)
{
    if (PlayerIdToBench.IsNone())
    {
        return false;
    }

    EnsurePlayerExistsInSquad(CurrentTeamSetup, PlayerIdToBench);
    RemovePlayerFromStartingLineup(CurrentTeamSetup, PlayerIdToBench);
    AddPlayerToBenchIfNeeded(CurrentTeamSetup, PlayerIdToBench);
    NormalizeLoadedTeamSetup(CurrentTeamSetup);
    MarkTeamSetupChanged();
    return PersistIfNeeded();
}

FName USoccerGameInstance::GetPlayerInStartingSlot(FName FormationSlotId) const
{
    const FName* FoundPlayerId = CurrentTeamSetup.StartingLineupBySlot.Find(FormationSlotId);
    return FoundPlayerId != nullptr ? *FoundPlayerId : NAME_None;
}

FName USoccerGameInstance::FindStartingSlotForPlayer(FName PlayerIdToFind) const
{
    if (PlayerIdToFind.IsNone())
    {
        return NAME_None;
    }

    for (const TPair<FName, FName>& LineupEntry : CurrentTeamSetup.StartingLineupBySlot)
    {
        if (LineupEntry.Value == PlayerIdToFind)
        {
            return LineupEntry.Key;
        }
    }

    return NAME_None;
}

bool USoccerGameInstance::IsPlayerOnBench(FName PlayerIdToFind) const
{
    return !PlayerIdToFind.IsNone() && CurrentTeamSetup.BenchPlayerIds.Contains(PlayerIdToFind);
}

bool USoccerGameInstance::IsPlayerInSquad(FName PlayerIdToFind) const
{
    return !PlayerIdToFind.IsNone() && CurrentTeamSetup.SquadPlayerIds.Contains(PlayerIdToFind);
}

FString USoccerGameInstance::GetTeamSaveSlotName()
{
    return TeamSetupSaveSlotName;
}

int32 USoccerGameInstance::GetCurrentSaveFormatVersion()
{
    return CurrentTeamSetupSaveFormatVersion;
}

int32 USoccerGameInstance::GetCurrentTeamSetupDataVersion()
{
    return CurrentTeamSetupDataVersion;
}

bool USoccerGameInstance::LoadOrCreateTeamSetup()
{
    if (!UGameplayStatics::DoesSaveGameExist(TeamSetupSaveSlotName, TeamSetupSaveUserIndex))
    {
        BuildDefaultTeamSetup(CurrentTeamSetup);
        bTeamSetupDirty = true;

        UE_LOG(
            LogSoccerTeamPersistence,
            Display,
            TEXT("[TeamSetup] No previous save found. Creating the default team setup.")
        );

        return SaveTeamSetup();
    }

    USaveGame* LoadedBaseObject = UGameplayStatics::LoadGameFromSlot(
        TeamSetupSaveSlotName,
        TeamSetupSaveUserIndex
    );

    USoccerTeamSaveGame* LoadedTeamSave = Cast<USoccerTeamSaveGame>(LoadedBaseObject);
    if (LoadedTeamSave == nullptr)
    {
        UE_LOG(
            LogSoccerTeamPersistence,
            Error,
            TEXT("[TeamSetup] Existing save could not be loaded as USoccerTeamSaveGame.")
        );
        return false;
    }

    if (LoadedTeamSave->SaveFormatVersion > CurrentTeamSetupSaveFormatVersion)
    {
        UE_LOG(
            LogSoccerTeamPersistence,
            Error,
            TEXT("[TeamSetup] Save format %d is newer than supported format %d. File left untouched."),
            LoadedTeamSave->SaveFormatVersion,
            CurrentTeamSetupSaveFormatVersion
        );
        return false;
    }

    CurrentTeamSetup = LoadedTeamSave->TeamSetup;
    NormalizeLoadedTeamSetup(CurrentTeamSetup);

    // Version 1 is the initial format. Future migrations belong here before normalize/save.
    CurrentTeamSetup.DataVersion = CurrentTeamSetupDataVersion;
    bTeamSetupDirty = false;

    UE_LOG(
        LogSoccerTeamPersistence,
        Display,
        TEXT("[TeamSetup] Loaded persistent team configuration from disk.")
    );

    return true;
}

bool USoccerGameInstance::PersistIfNeeded()
{
    if (!bAutoSaveTeamChanges)
    {
        return true;
    }

    return SaveTeamSetup();
}

void USoccerGameInstance::MarkTeamSetupChanged()
{
    bTeamSetupDirty = true;
    bTeamSetupLoaded = true;
}

void USoccerGameInstance::BuildDefaultTeamSetup(FSoccerTeamSetup& OutTeamSetup) const
{
    OutTeamSetup = FSoccerTeamSetup();
    OutTeamSetup.DataVersion = CurrentTeamSetupDataVersion;
    OutTeamSetup.FormationSystem = ESoccerFormationSystem::OneThreeTwoOne;
    RebuildSlotInstructionsForCurrentFormation(OutTeamSetup);
}

void USoccerGameInstance::NormalizeLoadedTeamSetup(FSoccerTeamSetup& InOutTeamSetup) const
{
    InOutTeamSetup.DataVersion = CurrentTeamSetupDataVersion;

    const FSoccerFormationDefinition& FormationDefinition =
        SoccerFormationLibrary::GetDefinition(InOutTeamSetup.FormationSystem);

    if (!SoccerFormationLibrary::IsValidSevenASideDefinition(FormationDefinition))
    {
        InOutTeamSetup.FormationSystem = ESoccerFormationSystem::OneThreeTwoOne;
    }

    RemoveInvalidAndDuplicatePlayerIds(InOutTeamSetup.SquadPlayerIds);
    RemoveInvalidAndDuplicatePlayerIds(InOutTeamSetup.BenchPlayerIds);

    TArray<FName> InvalidLineupSlotIds;
    TArray<FName> PlayersToRecoverToBench;
    TArray<FName> SeenStartingPlayerIds;

    for (const TPair<FName, FName>& LineupEntry : InOutTeamSetup.StartingLineupBySlot)
    {
        const bool bSlotIsValid = IsValidSlotForFormation(
            LineupEntry.Key,
            InOutTeamSetup.FormationSystem
        );
        const bool bPlayerIsValid = !LineupEntry.Value.IsNone();
        const bool bPlayerAlreadyUsed = SeenStartingPlayerIds.Contains(LineupEntry.Value);

        if (!bSlotIsValid || !bPlayerIsValid || bPlayerAlreadyUsed)
        {
            if (bPlayerIsValid && !bPlayerAlreadyUsed)
            {
                AddUniqueValidPlayerId(PlayersToRecoverToBench, LineupEntry.Value);
            }

            InvalidLineupSlotIds.Add(LineupEntry.Key);
            continue;
        }

        SeenStartingPlayerIds.Add(LineupEntry.Value);
        EnsurePlayerExistsInSquad(InOutTeamSetup, LineupEntry.Value);
    }

    for (const FName InvalidSlotId : InvalidLineupSlotIds)
    {
        InOutTeamSetup.StartingLineupBySlot.Remove(InvalidSlotId);
    }

    for (const FName PlayerIdToRecover : PlayersToRecoverToBench)
    {
        AddPlayerToBenchIfNeeded(InOutTeamSetup, PlayerIdToRecover);
    }

    for (const FName BenchPlayerId : InOutTeamSetup.BenchPlayerIds)
    {
        EnsurePlayerExistsInSquad(InOutTeamSetup, BenchPlayerId);
    }

    // Starting players cannot simultaneously appear on the bench.
    for (const FName StartingPlayerId : SeenStartingPlayerIds)
    {
        InOutTeamSetup.BenchPlayerIds.Remove(StartingPlayerId);
    }

    RemoveInvalidAndDuplicatePlayerIds(InOutTeamSetup.SquadPlayerIds);
    RemoveInvalidAndDuplicatePlayerIds(InOutTeamSetup.BenchPlayerIds);
    RebuildSlotInstructionsForCurrentFormation(InOutTeamSetup);
}

void USoccerGameInstance::RebuildSlotInstructionsForCurrentFormation(
    FSoccerTeamSetup& InOutTeamSetup
) const
{
    const FSoccerFormationDefinition& FormationDefinition =
        SoccerFormationLibrary::GetDefinition(InOutTeamSetup.FormationSystem);

    if (!SoccerFormationLibrary::IsValidSevenASideDefinition(FormationDefinition))
    {
        return;
    }

    TArray<FSoccerSlotTacticalInstruction> RebuiltInstructions;
    RebuiltInstructions.Reserve(FormationDefinition.Slots.Num());

    for (const FSoccerFormationSlot& FormationSlot : FormationDefinition.Slots)
    {
        const FSoccerSlotTacticalInstruction* ExistingInstruction =
            InOutTeamSetup.SlotInstructions.FindByPredicate(
                [&FormationSlot](const FSoccerSlotTacticalInstruction& CandidateInstruction)
                {
                    return CandidateInstruction.SlotId == FormationSlot.SlotId;
                }
            );

        if (ExistingInstruction != nullptr)
        {
            RebuiltInstructions.Add(*ExistingInstruction);
        }
        else
        {
            FSoccerSlotTacticalInstruction DefaultInstruction;
            DefaultInstruction.SlotId = FormationSlot.SlotId;
            RebuiltInstructions.Add(DefaultInstruction);
        }
    }

    InOutTeamSetup.SlotInstructions = MoveTemp(RebuiltInstructions);
}

bool USoccerGameInstance::IsValidSlotForFormation(
    FName FormationSlotId,
    ESoccerFormationSystem FormationSystem
) const
{
    const FSoccerFormationDefinition& FormationDefinition =
        SoccerFormationLibrary::GetDefinition(FormationSystem);

    return
        SoccerFormationLibrary::IsValidSevenASideDefinition(FormationDefinition) &&
        SoccerFormationLibrary::IsSlotValidForFormation(
            FormationSystem,
            FormationSlotId
        );
}

void USoccerGameInstance::AddPlayerToBenchIfNeeded(
    FSoccerTeamSetup& InOutTeamSetup,
    FName PlayerIdToAdd
) const
{
    if (PlayerIdToAdd.IsNone())
    {
        return;
    }

    EnsurePlayerExistsInSquad(InOutTeamSetup, PlayerIdToAdd);

    bool bPlayerIsStarting = false;
    for (const TPair<FName, FName>& LineupEntry : InOutTeamSetup.StartingLineupBySlot)
    {
        if (LineupEntry.Value == PlayerIdToAdd)
        {
            bPlayerIsStarting = true;
            break;
        }
    }

    if (!bPlayerIsStarting && !InOutTeamSetup.BenchPlayerIds.Contains(PlayerIdToAdd))
    {
        InOutTeamSetup.BenchPlayerIds.Add(PlayerIdToAdd);
    }
}

void USoccerGameInstance::RemovePlayerFromBench(
    FSoccerTeamSetup& InOutTeamSetup,
    FName PlayerIdToRemove
) const
{
    InOutTeamSetup.BenchPlayerIds.Remove(PlayerIdToRemove);
}

void USoccerGameInstance::RemovePlayerFromStartingLineup(
    FSoccerTeamSetup& InOutTeamSetup,
    FName PlayerIdToRemove
) const
{
    if (PlayerIdToRemove.IsNone())
    {
        return;
    }

    TArray<FName> SlotIdsToClear;
    for (const TPair<FName, FName>& LineupEntry : InOutTeamSetup.StartingLineupBySlot)
    {
        if (LineupEntry.Value == PlayerIdToRemove)
        {
            SlotIdsToClear.Add(LineupEntry.Key);
        }
    }

    for (const FName SlotIdToClear : SlotIdsToClear)
    {
        InOutTeamSetup.StartingLineupBySlot.Remove(SlotIdToClear);
    }
}

void USoccerGameInstance::EnsurePlayerExistsInSquad(
    FSoccerTeamSetup& InOutTeamSetup,
    FName PlayerIdToEnsure
) const
{
    AddUniqueValidPlayerId(InOutTeamSetup.SquadPlayerIds, PlayerIdToEnsure);
}
