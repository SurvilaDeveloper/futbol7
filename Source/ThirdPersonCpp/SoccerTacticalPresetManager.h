#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SoccerTacticalPresetTypes.h"
#include "SoccerTacticalPresetManager.generated.h"

class ASoccerMatchManager;

/*
 * Service layer between the coach UI/match and local persistence.
 *
 * The widget never writes SaveGame files directly. A future backend can be
 * added behind this class while FSoccerTacticalPreset and the match-facing API
 * remain unchanged.
 */
UCLASS(BlueprintType)
class THIRDPERSONCPP_API USoccerTacticalPresetManager : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(ASoccerMatchManager* InMatchManager);

	// User-authored presets persisted in the local SaveGame.
	const TArray<FSoccerTacticalPreset>& GetPresets() const;

	// Read-only presets shipped by the game and rebuilt from code on startup.
	const TArray<FSoccerTacticalPreset>& GetBuiltInPresets() const;

	bool SaveCurrentPlayerStrategyAsNewPreset(
		const FString& RequestedName,
		FGuid& OutPresetId,
		FString& OutMessage
	);

	bool OverwritePresetWithCurrentPlayerStrategy(
		const FGuid& PresetId,
		FString& OutMessage
	);

	bool RenamePreset(
		const FGuid& PresetId,
		const FString& RequestedName,
		FString& OutMessage
	);

	bool DuplicatePreset(
		const FGuid& PresetId,
		FGuid& OutDuplicatePresetId,
		FString& OutMessage
	);

	// Moves a preset one position inside the persistent library order.
	// Direction < 0 moves up; Direction > 0 moves down. The preset GUID,
	// active identity and quick-slot references remain unchanged.
	bool MovePreset(
		const FGuid& PresetId,
		int32 Direction,
		FString& OutMessage
	);

	// Portable JSON exchange. The .sav remains the normal local store; JSON is
	// only for backup/sharing and as a future backend-friendly interchange form.
	bool ExportPresetToJson(
		const FGuid& PresetId,
		FString& OutExportedFileName,
		FString& OutMessage
	) const;

	bool ImportPresetFromJson(
		const FString& JsonFileName,
		FGuid& OutImportedPresetId,
		FString& OutMessage
	);

	void GetAvailableJsonExchangeFiles(
		TArray<FString>& OutFileNames
	) const;

	bool DeletePreset(
		const FGuid& PresetId,
		FString& OutMessage
	);

	bool ApplyPresetToPlayerTeam(
		const FGuid& PresetId,
		FString& OutMessage
	);

	// Stage 16C: four persistent quick-access slots. Slots are zero-based in
	// C++ and displayed to the player as 1..4. A preset may occupy only one
	// quick slot at a time so the four choices always remain distinct.
	bool AssignPresetToQuickSlot(
		int32 QuickSlotIndex,
		const FGuid& PresetId,
		FString& OutMessage
	);

	bool ClearQuickPresetSlot(
		int32 QuickSlotIndex,
		FString& OutMessage
	);

	bool ApplyQuickPresetSlotToPlayerTeam(
		int32 QuickSlotIndex,
		FString& OutMessage
	);

	FGuid GetQuickPresetIdForSlot(int32 QuickSlotIndex) const;
	const FSoccerTacticalPreset* GetQuickPresetForSlot(int32 QuickSlotIndex) const;
	static int32 GetQuickPresetSlotCount();

	bool ReloadFromDisk(FString& OutMessage);

	// Finds only user-authored/persisted presets.
	const FSoccerTacticalPreset* FindPreset(const FGuid& PresetId) const;
	const FSoccerTacticalPreset* FindBuiltInPreset(const FGuid& PresetId) const;
	const FSoccerTacticalPreset* FindAnyPreset(const FGuid& PresetId) const;
	bool IsBuiltInPreset(const FGuid& PresetId) const;
	FGuid GetActivePresetId() const;
	bool IsActivePresetModified() const;

	static FString GetLocalSaveSlotName();
	static FString GetJsonExchangeDirectory();
	static int32 GetCurrentPresetDataVersion();
	static int32 GetCurrentSaveFormatVersion();

private:
	void BuildBuiltInPresets();

	bool CaptureCurrentPlayerStrategy(
		const FString& PresetName,
		FSoccerTacticalPreset& OutPreset
	) const;

	bool PersistLibrary(FString& OutMessage) const;
	bool ValidateAndNormalizeName(
		const FString& RequestedName,
		FString& OutNormalizedName,
		FString& OutMessage
	) const;
	bool IsNameUsedByAnotherPreset(
		const FString& Name,
		const FGuid& IgnoredPresetId = FGuid()
	) const;
	bool IsPresetDataSupported(
		const FSoccerTacticalPreset& Preset,
		FString& OutMessage
	) const;
	bool DoesPresetMatchCurrentPlayerStrategy(
		const FSoccerTacticalPreset& Preset
	) const;
	bool ValidateImportedPresetData(
		const FSoccerTacticalPreset& Preset,
		FString& OutMessage
	) const;
	FString BuildUniqueImportedPresetName(const FString& RequestedName) const;

	UPROPERTY()
	ASoccerMatchManager* MatchManager = nullptr;

	// Code-defined read-only templates. Never serialized to the user SaveGame.
	TArray<FSoccerTacticalPreset> BuiltInPresets;

	UPROPERTY()
	TArray<FSoccerTacticalPreset> Presets;

	UPROPERTY()
	TArray<FGuid> QuickPresetSlotIds;

	FGuid ActivePresetId;
};
