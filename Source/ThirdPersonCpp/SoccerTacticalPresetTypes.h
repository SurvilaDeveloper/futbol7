#pragma once

#include "CoreMinimal.h"
#include "SoccerFormationTypes.h"
#include "SoccerTacticTypes.h"
#include "SoccerPlayerInstructionTypes.h"
#include "SoccerTacticalPresetTypes.generated.h"

/*
 * Complete strategy snapshot shared by code-defined presets and user presets.
 *
 * PresetId is deliberately independent from PresetName. Built-in presets use
 * stable code-defined IDs; user presets use persistent generated IDs.
 * DataVersion is reserved for future migrations when the tactical model grows.
 */
USTRUCT(BlueprintType)
struct FSoccerTacticalPreset
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soccer|Tactical Presets")
	FGuid PresetId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soccer|Tactical Presets")
	FString PresetName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soccer|Tactical Presets")
	int32 DataVersion = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soccer|Tactical Presets")
	ESoccerFormationSystem FormationSystem = ESoccerFormationSystem::OneThreeTwoOne;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soccer|Tactical Presets")
	FSoccerTeamTacticalPlan TacticalPlan;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Soccer|Tactical Presets")
	TArray<FSoccerSlotTacticalInstruction> SlotInstructions;
};

/*
 * Portable JSON representation used only for import/export. SourcePresetId is
 * metadata from the exporting library; importing always assigns a fresh local
 * PresetId so shared files can never collide with an existing local preset.
 */
USTRUCT()
struct FSoccerTacticalPresetExchangeDocument
{
	GENERATED_BODY()

	UPROPERTY()
	FString FileType;

	UPROPERTY()
	int32 ExchangeFormatVersion = 0;

	UPROPERTY()
	bool bContainsCompleteStrategy = false;

	UPROPERTY()
	FString SourcePresetId;

	UPROPERTY()
	FString PresetName;

	UPROPERTY()
	int32 DataVersion = 0;

	UPROPERTY()
	ESoccerFormationSystem FormationSystem = ESoccerFormationSystem::OneThreeTwoOne;

	UPROPERTY()
	FSoccerTeamTacticalPlan TacticalPlan;

	UPROPERTY()
	TArray<FSoccerSlotTacticalInstruction> SlotInstructions;
};
