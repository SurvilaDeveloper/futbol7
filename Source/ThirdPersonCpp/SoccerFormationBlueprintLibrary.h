#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SoccerFormationTypes.h"
#include "SoccerPlayerProfileTypes.h"
#include "SoccerFormationBlueprintLibrary.generated.h"

class ASoccerField;

/**
 * Blueprint/UMG-facing read API for the built-in Futbol7 formation catalog.
 * The future coach screen can consume this without duplicating formation data.
 */
UCLASS()
class THIRDPERSONCPP_API USoccerFormationBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Soccer|Formation")
	static TArray<ESoccerFormationSystem> GetAllFormationSystems();

	UFUNCTION(BlueprintPure, Category = "Soccer|Formation")
	static FSoccerFormationDefinition GetFormationDefinition(
		ESoccerFormationSystem FormationSystem
	);

	UFUNCTION(BlueprintPure, Category = "Soccer|Formation")
	static bool GetFormationSlotById(
		ESoccerFormationSystem FormationSystem,
		FName SlotId,
		FSoccerFormationSlot& OutSlot
	);

	UFUNCTION(BlueprintPure, Category = "Soccer|Formation")
	static bool IsFormationSlotValid(
		ESoccerFormationSystem FormationSystem,
		FName SlotId
	);

	UFUNCTION(BlueprintPure, Category = "Soccer|Formation")
	static FText GetFormationSlotDisplayName(
		const FSoccerFormationSlot& Slot
	);

	UFUNCTION(BlueprintPure, Category = "Soccer|Formation")
	static ESoccerPlayerNaturalPosition GetFormationSlotNaturalPosition(
		const FSoccerFormationSlot& Slot
	);

	UFUNCTION(BlueprintPure, Category = "Soccer|Formation|Geometry")
	static bool GetFormationSlotWorldLocation(
		const ASoccerField* SoccerField,
		float OwnGoalLineSign,
		const FSoccerFormationSlot& Slot,
		FVector& OutWorldLocation,
		float LocalZ = 0.0f
	);

	UFUNCTION(BlueprintPure, Category = "Soccer|Formation|Validation")
	static bool IsBuiltInFormationCatalogValid();
};
