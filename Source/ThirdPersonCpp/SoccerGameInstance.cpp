#include "SoccerGameInstance.h"

#include "Kismet/GameplayStatics.h"
#include "SoccerFormationLibrary.h"
#include "SoccerTeamSaveGame.h"
#include "SoccerPlayerProfile.h"
#include "SoccerSquadCatalog.h"
#include "SoccerClubProfile.h"
#include "SoccerCoachProfile.h"
#include "AssetRegistryModule.h"
#include "Modules/ModuleManager.h"

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

    RefreshPlayerProfileRegistry();

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

bool USoccerGameInstance::RefreshPlayerProfileRegistry()
{
    RuntimePlayerProfilesById.Reset();
    RuntimePlayerTeamCatalog = nullptr;
    RuntimeSquadCatalogsByClubId.Reset();
    RuntimeInitialClubIdByPlayerId.Reset();
    RuntimeCoachProfilesById.Reset();
    RuntimeCoachByClubId.Reset();
    RuntimeInitialClubIdByCoachId.Reset();

    FAssetRegistryModule& AssetRegistryModule =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

    TArray<FAssetData> CatalogAssets;
    AssetRegistryModule.Get().GetAssetsByClass(
        USoccerSquadCatalog::StaticClass()->GetFName(),
        CatalogAssets,
        true
    );

    USoccerSquadCatalog* FirstValidCatalog = nullptr;
    USoccerSquadCatalog* LegacyDefaultCatalog = nullptr;
    USoccerSquadCatalog* ExplicitHumanDefaultCatalog = nullptr;
    TArray<USoccerSquadCatalog*> ValidCatalogs;

    for (const FAssetData& CatalogAssetData : CatalogAssets)
    {
        USoccerSquadCatalog* CandidateCatalog = Cast<USoccerSquadCatalog>(
            CatalogAssetData.GetAsset()
        );
        if (!IsValid(CandidateCatalog))
        {
            continue;
        }

        if (FirstValidCatalog == nullptr)
        {
            FirstValidCatalog = CandidateCatalog;
        }

        if (
            CandidateCatalog->bDefaultHumanControlledClub &&
            ExplicitHumanDefaultCatalog == nullptr
        )
        {
            ExplicitHumanDefaultCatalog = CandidateCatalog;
        }

        if (
            CandidateCatalog->bDefaultPlayerTeamCatalog &&
            LegacyDefaultCatalog == nullptr
        )
        {
            LegacyDefaultCatalog = CandidateCatalog;
        }

        const FName ClubId = CandidateCatalog->GetClubId();
        if (ClubId.IsNone())
        {
            ValidCatalogs.Add(CandidateCatalog);
            UE_LOG(
                LogSoccerTeamPersistence,
                Warning,
                TEXT("[ClubRoster] Squad catalog '%s' has no valid ClubProfile; it remains available only as a legacy fallback."),
                *CandidateCatalog->GetName()
            );
            continue;
        }

        if (RuntimeSquadCatalogsByClubId.Contains(ClubId))
        {
            UE_LOG(
                LogSoccerTeamPersistence,
                Error,
                TEXT("[ClubRoster] Duplicate squad catalog for ClubId '%s'. Keeping first catalog."),
                *ClubId.ToString()
            );
            continue;
        }

        RuntimeSquadCatalogsByClubId.Add(ClubId, CandidateCatalog);
        ValidCatalogs.Add(CandidateCatalog);
    }

    RuntimePlayerTeamCatalog =
        ExplicitHumanDefaultCatalog != nullptr
            ? ExplicitHumanDefaultCatalog
            : (LegacyDefaultCatalog != nullptr
                ? LegacyDefaultCatalog
                : FirstValidCatalog);

    TArray<FAssetData> CoachAssets;
    AssetRegistryModule.Get().GetAssetsByClass(
        USoccerCoachProfile::StaticClass()->GetFName(),
        CoachAssets,
        true
    );

    for (const FAssetData& CoachAssetData : CoachAssets)
    {
        USoccerCoachProfile* CoachProfile = Cast<USoccerCoachProfile>(
            CoachAssetData.GetAsset()
        );
        if (!IsValid(CoachProfile))
        {
            continue;
        }
        if (!CoachProfile->HasValidCoachId())
        {
            UE_LOG(
                LogSoccerTeamPersistence,
                Warning,
                TEXT("[CoachRegistry] Coach asset '%s' has no CoachId and was ignored."),
                *CoachProfile->GetName()
            );
            continue;
        }

        const FName CoachId = CoachProfile->Identity.CoachId;
        if (RuntimeCoachProfilesById.Contains(CoachId))
        {
            UE_LOG(
                LogSoccerTeamPersistence,
                Error,
                TEXT("[CoachRegistry] Duplicate CoachId '%s'. Keeping first profile."),
                *CoachId.ToString()
            );
            continue;
        }
        RuntimeCoachProfilesById.Add(CoachId, CoachProfile);
    }

    const TArray<FName> RegisteredClubIds = GetAvailableClubIds();
    for (const FName RegisteredClubId : RegisteredClubIds)
    {
        USoccerSquadCatalog* SquadCatalog =
            FindSquadCatalogByClubId(RegisteredClubId);
        USoccerClubProfile* ClubProfile = IsValid(SquadCatalog)
            ? SquadCatalog->ClubProfile
            : nullptr;
        USoccerCoachProfile* CoachProfile = IsValid(ClubProfile)
            ? ClubProfile->CurrentCoach
            : nullptr;
        if (!IsValid(CoachProfile))
        {
            UE_LOG(
                LogSoccerTeamPersistence,
                Warning,
                TEXT("[CoachRegistry] Club '%s' has no current coach."),
                *RegisteredClubId.ToString()
            );
            continue;
        }

        if (!CoachProfile->HasValidCoachId())
        {
            UE_LOG(
                LogSoccerTeamPersistence,
                Error,
                TEXT("[CoachRegistry] Club '%s' references coach asset '%s' without CoachId."),
                *RegisteredClubId.ToString(),
                *CoachProfile->GetName()
            );
            continue;
        }

        const FName CoachId = CoachProfile->Identity.CoachId;
        USoccerCoachProfile* RegisteredCoach =
            FindCoachProfileById(CoachId);
        if (IsValid(RegisteredCoach) && RegisteredCoach != CoachProfile)
        {
            UE_LOG(
                LogSoccerTeamPersistence,
                Error,
                TEXT("[CoachRegistry] Club '%s' references duplicate CoachId '%s' through asset '%s'; assignment ignored."),
                *RegisteredClubId.ToString(),
                *CoachId.ToString(),
                *CoachProfile->GetName()
            );
            continue;
        }
        const FName* ExistingClubId =
            RuntimeInitialClubIdByCoachId.Find(CoachId);
        if (ExistingClubId != nullptr && *ExistingClubId != RegisteredClubId)
        {
            UE_LOG(
                LogSoccerTeamPersistence,
                Error,
                TEXT("[CoachRegistry] Coach '%s' is assigned to clubs '%s' and '%s'. Keeping first assignment."),
                *CoachId.ToString(),
                *ExistingClubId->ToString(),
                *RegisteredClubId.ToString()
            );
            continue;
        }

        if (!RuntimeCoachProfilesById.Contains(CoachId))
        {
            RuntimeCoachProfilesById.Add(CoachId, CoachProfile);
        }
        RuntimeCoachByClubId.Add(RegisteredClubId, CoachProfile);
        RuntimeInitialClubIdByCoachId.Add(CoachId, RegisteredClubId);
    }

    UE_LOG(
        LogSoccerTeamPersistence,
        Display,
        TEXT("[CoachRegistry] Ready: coaches=%d, clubAssignments=%d."),
        RuntimeCoachProfilesById.Num(),
        RuntimeCoachByClubId.Num()
    );

    TArray<USoccerPlayerProfile*> CandidateProfiles;

    if (ValidCatalogs.Num() > 0)
    {
        for (USoccerSquadCatalog* SquadCatalog : ValidCatalogs)
        {
            if (!IsValid(SquadCatalog))
            {
                continue;
            }

            const FName ClubId = SquadCatalog->GetClubId();
            for (USoccerPlayerProfile* Profile : SquadCatalog->PlayerProfiles)
            {
                if (!IsValid(Profile) || !Profile->HasValidPlayerId())
                {
                    continue;
                }

                CandidateProfiles.AddUnique(Profile);

                if (!ClubId.IsNone())
                {
                    const FName PlayerId = Profile->Identity.PlayerId;
                    const FName* ExistingClubId =
                        RuntimeInitialClubIdByPlayerId.Find(PlayerId);

                    if (ExistingClubId == nullptr)
                    {
                        RuntimeInitialClubIdByPlayerId.Add(PlayerId, ClubId);
                    }
                    else if (*ExistingClubId != ClubId)
                    {
                        UE_LOG(
                            LogSoccerTeamPersistence,
                            Error,
                            TEXT("[ClubRoster] PlayerId '%s' appears in clubs '%s' and '%s'. Keeping first membership."),
                            *PlayerId.ToString(),
                            *ExistingClubId->ToString(),
                            *ClubId.ToString()
                        );
                    }
                }
            }
        }
    }
    else
    {
        // Development fallback: before a PlayerTeam catalog exists, resolve all
        // player profiles so the manager layer remains immediately testable.
        TArray<FAssetData> ProfileAssets;
        AssetRegistryModule.Get().GetAssetsByClass(
            USoccerPlayerProfile::StaticClass()->GetFName(),
            ProfileAssets,
            true
        );

        for (const FAssetData& ProfileAssetData : ProfileAssets)
        {
            USoccerPlayerProfile* Profile = Cast<USoccerPlayerProfile>(
                ProfileAssetData.GetAsset()
            );
            if (IsValid(Profile))
            {
                CandidateProfiles.Add(Profile);
            }
        }
    }

    for (USoccerPlayerProfile* Profile : CandidateProfiles)
    {
        if (!IsValid(Profile) || !Profile->HasValidPlayerId())
        {
            continue;
        }

        const FName PlayerId = Profile->Identity.PlayerId;
        if (RuntimePlayerProfilesById.Contains(PlayerId))
        {
            UE_LOG(
                LogSoccerTeamPersistence,
                Warning,
                TEXT("[TeamSetup] Duplicate runtime PlayerId '%s'. Keeping first profile."),
                *PlayerId.ToString()
            );
            continue;
        }

        RuntimePlayerProfilesById.Add(PlayerId, Profile);
    }

    UE_LOG(
        LogSoccerTeamPersistence,
        Display,
        TEXT("[ClubRoster] Registry ready: clubs=%d, profiles=%d, defaultHumanSquad=%s."),
        RuntimeSquadCatalogsByClubId.Num(),
        RuntimePlayerProfilesById.Num(),
        IsValid(RuntimePlayerTeamCatalog)
            ? *RuntimePlayerTeamCatalog->GetName()
            : TEXT("PlayerProfile fallback")
    );

    const FName HumanClubId = GetDefaultHumanClubId();
    if (
        CurrentMatchSetup.OpponentTeamClubId.IsNone() ||
        CurrentMatchSetup.OpponentTeamClubId == HumanClubId ||
        FindSquadCatalogByClubId(CurrentMatchSetup.OpponentTeamClubId) == nullptr
    )
    {
        CurrentMatchSetup = FSoccerMatchSetup();
        CurrentMatchSetup.PlayerTeamClubId = HumanClubId;
        const TArray<FName> AvailableClubIds = GetAvailableClubIds();
        for (const FName ClubId : AvailableClubIds)
        {
            if (ClubId != HumanClubId)
            {
                CurrentMatchSetup.OpponentTeamClubId = ClubId;
                break;
            }
        }
    }

    return RuntimePlayerProfilesById.Num() > 0;
}

USoccerPlayerProfile* USoccerGameInstance::FindPlayerProfileById(
    FName PlayerIdToFind
) const
{
    if (PlayerIdToFind.IsNone())
    {
        return nullptr;
    }

    USoccerPlayerProfile* const* FoundProfile =
        RuntimePlayerProfilesById.Find(PlayerIdToFind);

    return FoundProfile != nullptr ? *FoundProfile : nullptr;
}

int32 USoccerGameInstance::GetResolvedPlayerProfileCount() const
{
    return RuntimePlayerProfilesById.Num();
}

TArray<FName> USoccerGameInstance::GetAvailableClubIds() const
{
    TArray<FName> ClubIds;
    RuntimeSquadCatalogsByClubId.GetKeys(ClubIds);

    // Deterministic UE4-compatible order without relying on comparator helper
    // types that differ between engine versions.
    for (int32 LeftIndex = 0; LeftIndex < ClubIds.Num(); ++LeftIndex)
    {
        for (int32 RightIndex = LeftIndex + 1; RightIndex < ClubIds.Num(); ++RightIndex)
        {
            if (ClubIds[RightIndex].ToString() < ClubIds[LeftIndex].ToString())
            {
                ClubIds.Swap(LeftIndex, RightIndex);
            }
        }
    }

    return ClubIds;
}

USoccerSquadCatalog* USoccerGameInstance::FindSquadCatalogByClubId(
    FName ClubIdToFind
) const
{
    if (ClubIdToFind.IsNone())
    {
        return nullptr;
    }

    USoccerSquadCatalog* const* FoundCatalog =
        RuntimeSquadCatalogsByClubId.Find(ClubIdToFind);

    return FoundCatalog != nullptr ? *FoundCatalog : nullptr;
}

USoccerSquadCatalog* USoccerGameInstance::GetDefaultHumanSquadCatalog() const
{
    return RuntimePlayerTeamCatalog;
}

FName USoccerGameInstance::GetInitialClubIdForPlayer(
    FName PlayerIdToFind
) const
{
    const FName* FoundClubId =
        RuntimeInitialClubIdByPlayerId.Find(PlayerIdToFind);

    return FoundClubId != nullptr ? *FoundClubId : NAME_None;
}

TArray<FName> USoccerGameInstance::GetAvailableCoachIds() const
{
    TArray<FName> CoachIds;
    RuntimeCoachProfilesById.GetKeys(CoachIds);
    for (int32 LeftIndex = 0; LeftIndex < CoachIds.Num(); ++LeftIndex)
    {
        for (int32 RightIndex = LeftIndex + 1; RightIndex < CoachIds.Num(); ++RightIndex)
        {
            if (CoachIds[RightIndex].ToString() < CoachIds[LeftIndex].ToString())
            {
                CoachIds.Swap(LeftIndex, RightIndex);
            }
        }
    }
    return CoachIds;
}

USoccerCoachProfile* USoccerGameInstance::FindCoachProfileById(
    FName CoachIdToFind
) const
{
    USoccerCoachProfile* const* FoundCoach =
        RuntimeCoachProfilesById.Find(CoachIdToFind);
    return FoundCoach != nullptr ? *FoundCoach : nullptr;
}

USoccerCoachProfile* USoccerGameInstance::GetCoachProfileForClubId(
    FName ClubIdToFind
) const
{
    USoccerCoachProfile* const* FoundCoach =
        RuntimeCoachByClubId.Find(ClubIdToFind);
    return FoundCoach != nullptr ? *FoundCoach : nullptr;
}

FName USoccerGameInstance::GetInitialClubIdForCoach(
    FName CoachIdToFind
) const
{
    const FName* FoundClubId =
        RuntimeInitialClubIdByCoachId.Find(CoachIdToFind);
    return FoundClubId != nullptr ? *FoundClubId : NAME_None;
}

FName USoccerGameInstance::GetDefaultHumanClubId() const
{
    return IsValid(RuntimePlayerTeamCatalog)
        ? RuntimePlayerTeamCatalog->GetClubId()
        : NAME_None;
}

FName USoccerGameInstance::GetSelectedOpponentClubId() const
{
    return CurrentMatchSetup.OpponentTeamClubId;
}

bool USoccerGameInstance::SetSelectedOpponentClubId(FName OpponentClubId)
{
	return ConfigureStandaloneMatch(
		GetDefaultHumanClubId(),
		OpponentClubId
	);
}

FSoccerMatchSetup USoccerGameInstance::GetCurrentMatchSetup() const
{
	return CurrentMatchSetup;
}

bool USoccerGameInstance::ConfigureStandaloneMatch(
	FName HumanClubId,
	FName OpponentClubId
)
{
	if (
		HumanClubId.IsNone() ||
		OpponentClubId.IsNone() ||
		HumanClubId == OpponentClubId ||
		FindSquadCatalogByClubId(HumanClubId) == nullptr ||
		FindSquadCatalogByClubId(OpponentClubId) == nullptr
	)
	{
		return false;
	}

	CurrentMatchSetup = FSoccerMatchSetup();
	CurrentMatchSetup.PlayerTeamClubId = HumanClubId;
	CurrentMatchSetup.OpponentTeamClubId = OpponentClubId;
	CurrentMatchSetup.bPlayerTeamIsHome = true;
	UE_LOG(
		LogSoccerTeamPersistence,
		Display,
		TEXT("[MatchSetup] Standalone match configured: PlayerTeam=%s, OpponentTeam=%s."),
		*HumanClubId.ToString(),
		*OpponentClubId.ToString()
	);
	return true;
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


bool USoccerGameInstance::SetCoachStrategySnapshot(
    ESoccerFormationSystem NewFormationSystem,
    const FSoccerTeamTacticalPlan& NewTacticalPlan,
    const TArray<FSoccerSlotTacticalInstruction>& NewSlotInstructions
)
{
    const bool bPreviousAutoSave = bAutoSaveTeamChanges;
    bAutoSaveTeamChanges = false;

    const bool bFormationAccepted = SetFormationSystem(NewFormationSystem);
    if (bFormationAccepted)
    {
        SetTacticalPlan(NewTacticalPlan);

        for (const FSoccerSlotTacticalInstruction& Instruction : NewSlotInstructions)
        {
            if (IsValidSlotForFormation(Instruction.SlotId, NewFormationSystem))
            {
                SetSlotTacticalInstruction(Instruction);
            }
        }

        RebuildSlotInstructionsForCurrentFormation(CurrentTeamSetup);
        NormalizeLoadedTeamSetup(CurrentTeamSetup);
        MarkTeamSetupChanged();
    }

    bAutoSaveTeamChanges = bPreviousAutoSave;

    if (!bFormationAccepted)
    {
        return false;
    }

    return PersistIfNeeded();
}

bool USoccerGameInstance::SynchronizeSquadWithPlayerIds(
    const TArray<FName>& AuthoritativePlayerIds
)
{
    TArray<FName> NormalizedAuthoritativeIds;
    NormalizedAuthoritativeIds.Reserve(AuthoritativePlayerIds.Num());
    for (const FName CandidatePlayerId : AuthoritativePlayerIds)
    {
        AddUniqueValidPlayerId(NormalizedAuthoritativeIds, CandidatePlayerId);
    }

    // Never erase a persisted team just because a catalog failed to resolve.
    if (NormalizedAuthoritativeIds.Num() == 0)
    {
        return false;
    }

    const TArray<FName> PreviousSquad = CurrentTeamSetup.SquadPlayerIds;
    const TArray<FName> PreviousBench = CurrentTeamSetup.BenchPlayerIds;
    const TMap<FName, FName> PreviousLineup = CurrentTeamSetup.StartingLineupBySlot;

    // Content catalog is authoritative for squad membership. Keep the user's
    // tactical decisions only for player IDs that still exist in that catalog.
    TArray<FName> StaleStartingSlotIds;
    for (const TPair<FName, FName>& LineupEntry : CurrentTeamSetup.StartingLineupBySlot)
    {
        if (!NormalizedAuthoritativeIds.Contains(LineupEntry.Value))
        {
            StaleStartingSlotIds.Add(LineupEntry.Key);
        }
    }
    for (const FName StaleStartingSlotId : StaleStartingSlotIds)
    {
        CurrentTeamSetup.StartingLineupBySlot.Remove(StaleStartingSlotId);
    }

    TArray<FName> PreservedBench;
    PreservedBench.Reserve(NormalizedAuthoritativeIds.Num());
    for (const FName ExistingBenchPlayerId : CurrentTeamSetup.BenchPlayerIds)
    {
        if (
            NormalizedAuthoritativeIds.Contains(ExistingBenchPlayerId) &&
            FindStartingSlotForPlayer(ExistingBenchPlayerId).IsNone()
        )
        {
            AddUniqueValidPlayerId(PreservedBench, ExistingBenchPlayerId);
        }
    }

    // New catalog members enter the bench in catalog order. Starters remain in
    // their existing slots whenever both the player and slot are still valid.
    for (const FName CatalogPlayerId : NormalizedAuthoritativeIds)
    {
        if (FindStartingSlotForPlayer(CatalogPlayerId).IsNone())
        {
            AddUniqueValidPlayerId(PreservedBench, CatalogPlayerId);
        }
    }

    CurrentTeamSetup.SquadPlayerIds = NormalizedAuthoritativeIds;
    CurrentTeamSetup.BenchPlayerIds = MoveTemp(PreservedBench);
    NormalizeLoadedTeamSetup(CurrentTeamSetup);

    bool bLineupChanged = PreviousLineup.Num() != CurrentTeamSetup.StartingLineupBySlot.Num();
    if (!bLineupChanged)
    {
        for (const TPair<FName, FName>& PreviousLineupEntry : PreviousLineup)
        {
            const FName* CurrentPlayerId =
                CurrentTeamSetup.StartingLineupBySlot.Find(PreviousLineupEntry.Key);
            if (CurrentPlayerId == nullptr || *CurrentPlayerId != PreviousLineupEntry.Value)
            {
                bLineupChanged = true;
                break;
            }
        }
    }

    const bool bChanged =
        PreviousSquad != CurrentTeamSetup.SquadPlayerIds ||
        PreviousBench != CurrentTeamSetup.BenchPlayerIds ||
        bLineupChanged;

    if (!bChanged)
    {
        return true;
    }

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

bool USoccerGameInstance::MoveStartingSlotPlayerToBench(FName FormationSlotId)
{
    if (!IsValidSlotForFormation(FormationSlotId, CurrentTeamSetup.FormationSystem))
    {
        return false;
    }

    const FName PlayerIdToBench = GetPlayerInStartingSlot(FormationSlotId);
    if (PlayerIdToBench.IsNone())
    {
        return false;
    }

    CurrentTeamSetup.StartingLineupBySlot.Remove(FormationSlotId);
    AddPlayerToBenchIfNeeded(CurrentTeamSetup, PlayerIdToBench);
    NormalizeLoadedTeamSetup(CurrentTeamSetup);
    MarkTeamSetupChanged();
    return PersistIfNeeded();
}

bool USoccerGameInstance::SwapStartingSlots(
    FName FirstFormationSlotId,
    FName SecondFormationSlotId
)
{
    if (
        FirstFormationSlotId.IsNone() ||
        SecondFormationSlotId.IsNone() ||
        FirstFormationSlotId == SecondFormationSlotId ||
        !IsValidSlotForFormation(FirstFormationSlotId, CurrentTeamSetup.FormationSystem) ||
        !IsValidSlotForFormation(SecondFormationSlotId, CurrentTeamSetup.FormationSystem)
    )
    {
        return false;
    }

    const FName FirstPlayerId = GetPlayerInStartingSlot(FirstFormationSlotId);
    const FName SecondPlayerId = GetPlayerInStartingSlot(SecondFormationSlotId);

    if (FirstPlayerId.IsNone() && SecondPlayerId.IsNone())
    {
        return true;
    }

    // Clear both sources first so the one-occupied / one-empty case cannot duplicate a player.
    CurrentTeamSetup.StartingLineupBySlot.Remove(FirstFormationSlotId);
    CurrentTeamSetup.StartingLineupBySlot.Remove(SecondFormationSlotId);

    if (!SecondPlayerId.IsNone())
    {
        CurrentTeamSetup.StartingLineupBySlot.Add(FirstFormationSlotId, SecondPlayerId);
    }

    if (!FirstPlayerId.IsNone())
    {
        CurrentTeamSetup.StartingLineupBySlot.Add(SecondFormationSlotId, FirstPlayerId);
    }

    // Normalize is kept as a defensive invariant check before autosave.
    NormalizeLoadedTeamSetup(CurrentTeamSetup);
    MarkTeamSetupChanged();
    return PersistIfNeeded();
}

bool USoccerGameInstance::SwapStarterWithBench(
    FName FormationSlotId,
    FName BenchPlayerId
)
{
    if (
        BenchPlayerId.IsNone() ||
        !IsValidSlotForFormation(FormationSlotId, CurrentTeamSetup.FormationSystem)
    )
    {
        return false;
    }

    const int32 BenchIndex = CurrentTeamSetup.BenchPlayerIds.IndexOfByKey(BenchPlayerId);
    if (BenchIndex == INDEX_NONE)
    {
        return false;
    }

    EnsurePlayerExistsInSquad(CurrentTeamSetup, BenchPlayerId);

    const FName OutgoingStarterId = GetPlayerInStartingSlot(FormationSlotId);
    CurrentTeamSetup.BenchPlayerIds.RemoveAt(BenchIndex);

    if (!OutgoingStarterId.IsNone() && OutgoingStarterId != BenchPlayerId)
    {
        EnsurePlayerExistsInSquad(CurrentTeamSetup, OutgoingStarterId);
        CurrentTeamSetup.BenchPlayerIds.Insert(OutgoingStarterId, BenchIndex);
    }

    RemovePlayerFromStartingLineup(CurrentTeamSetup, BenchPlayerId);
    CurrentTeamSetup.StartingLineupBySlot.Add(FormationSlotId, BenchPlayerId);

    NormalizeLoadedTeamSetup(CurrentTeamSetup);
    MarkTeamSetupChanged();
    return PersistIfNeeded();
}

bool USoccerGameInstance::ReorderBenchPlayer(FName BenchPlayerId, int32 NewBenchIndex)
{
    if (BenchPlayerId.IsNone())
    {
        return false;
    }

    const int32 CurrentBenchIndex = CurrentTeamSetup.BenchPlayerIds.IndexOfByKey(BenchPlayerId);
    if (CurrentBenchIndex == INDEX_NONE || CurrentTeamSetup.BenchPlayerIds.Num() <= 0)
    {
        return false;
    }

    const int32 SafeNewBenchIndex = FMath::Clamp(
        NewBenchIndex,
        0,
        CurrentTeamSetup.BenchPlayerIds.Num() - 1
    );

    if (CurrentBenchIndex == SafeNewBenchIndex)
    {
        return true;
    }

    CurrentTeamSetup.BenchPlayerIds.RemoveAt(CurrentBenchIndex);
    CurrentTeamSetup.BenchPlayerIds.Insert(BenchPlayerId, SafeNewBenchIndex);

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

TArray<FName> USoccerGameInstance::GetSquadPlayerIds() const
{
    return CurrentTeamSetup.SquadPlayerIds;
}

TArray<FName> USoccerGameInstance::GetBenchPlayerIds() const
{
    return CurrentTeamSetup.BenchPlayerIds;
}

TArray<FName> USoccerGameInstance::GetStartingPlayerIdsInFormationOrder() const
{
    TArray<FName> OrderedPlayerIds;

    const FSoccerFormationDefinition& FormationDefinition =
        SoccerFormationLibrary::GetDefinition(CurrentTeamSetup.FormationSystem);

    if (!SoccerFormationLibrary::IsValidSevenASideDefinition(FormationDefinition))
    {
        return OrderedPlayerIds;
    }

    OrderedPlayerIds.Reserve(FormationDefinition.Slots.Num());

    for (const FSoccerFormationSlot& FormationSlot : FormationDefinition.Slots)
    {
        OrderedPlayerIds.Add(GetPlayerInStartingSlot(FormationSlot.SlotId));
    }

    return OrderedPlayerIds;
}

TArray<FName> USoccerGameInstance::GetEmptyStartingSlotIds() const
{
    TArray<FName> EmptySlotIds;

    const FSoccerFormationDefinition& FormationDefinition =
        SoccerFormationLibrary::GetDefinition(CurrentTeamSetup.FormationSystem);

    if (!SoccerFormationLibrary::IsValidSevenASideDefinition(FormationDefinition))
    {
        return EmptySlotIds;
    }

    for (const FSoccerFormationSlot& FormationSlot : FormationDefinition.Slots)
    {
        if (GetPlayerInStartingSlot(FormationSlot.SlotId).IsNone())
        {
            EmptySlotIds.Add(FormationSlot.SlotId);
        }
    }

    return EmptySlotIds;
}

int32 USoccerGameInstance::GetStartingPlayerCount() const
{
    int32 StartingPlayerCount = 0;

    const FSoccerFormationDefinition& FormationDefinition =
        SoccerFormationLibrary::GetDefinition(CurrentTeamSetup.FormationSystem);

    if (!SoccerFormationLibrary::IsValidSevenASideDefinition(FormationDefinition))
    {
        return 0;
    }

    for (const FSoccerFormationSlot& FormationSlot : FormationDefinition.Slots)
    {
        if (!GetPlayerInStartingSlot(FormationSlot.SlotId).IsNone())
        {
            ++StartingPlayerCount;
        }
    }

    return StartingPlayerCount;
}

bool USoccerGameInstance::HasCompleteStartingLineup() const
{
    const FSoccerFormationDefinition& FormationDefinition =
        SoccerFormationLibrary::GetDefinition(CurrentTeamSetup.FormationSystem);

    if (!SoccerFormationLibrary::IsValidSevenASideDefinition(FormationDefinition))
    {
        return false;
    }

    if (FormationDefinition.Slots.Num() != 7)
    {
        return false;
    }

    for (const FSoccerFormationSlot& FormationSlot : FormationDefinition.Slots)
    {
        if (GetPlayerInStartingSlot(FormationSlot.SlotId).IsNone())
        {
            return false;
        }
    }

    return true;
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
