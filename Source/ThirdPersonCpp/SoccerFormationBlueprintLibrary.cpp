#include "SoccerFormationBlueprintLibrary.h"

#include "SoccerFormationLibrary.h"

TArray<ESoccerFormationSystem> USoccerFormationBlueprintLibrary::GetAllFormationSystems()
{
	return SoccerFormationLibrary::GetAllSystems();
}

FSoccerFormationDefinition USoccerFormationBlueprintLibrary::GetFormationDefinition(
	ESoccerFormationSystem FormationSystem
)
{
	return SoccerFormationLibrary::GetDefinition(FormationSystem);
}

bool USoccerFormationBlueprintLibrary::GetFormationSlotById(
	ESoccerFormationSystem FormationSystem,
	FName SlotId,
	FSoccerFormationSlot& OutSlot
)
{
	OutSlot = FSoccerFormationSlot();

	const FSoccerFormationDefinition& Definition =
		SoccerFormationLibrary::GetDefinition(FormationSystem);

	const FSoccerFormationSlot* FoundSlot =
		SoccerFormationLibrary::FindSlotById(Definition, SlotId);

	if (FoundSlot == nullptr)
	{
		return false;
	}

	OutSlot = *FoundSlot;
	return true;
}

bool USoccerFormationBlueprintLibrary::IsFormationSlotValid(
	ESoccerFormationSystem FormationSystem,
	FName SlotId
)
{
	return SoccerFormationLibrary::IsSlotValidForFormation(
		FormationSystem,
		SlotId
	);
}

FText USoccerFormationBlueprintLibrary::GetFormationSlotDisplayName(
	const FSoccerFormationSlot& Slot
)
{
	return SoccerFormationLibrary::GetSlotDisplayName(Slot);
}

ESoccerPlayerNaturalPosition USoccerFormationBlueprintLibrary::GetFormationSlotNaturalPosition(
	const FSoccerFormationSlot& Slot
)
{
	return SoccerFormationLibrary::GetNaturalPositionForSlot(Slot);
}

bool USoccerFormationBlueprintLibrary::GetFormationSlotWorldLocation(
	const ASoccerField* SoccerField,
	float OwnGoalLineSign,
	const FSoccerFormationSlot& Slot,
	FVector& OutWorldLocation,
	float LocalZ
)
{
	return SoccerFormationLibrary::TryResolveSlotWorldLocation(
		SoccerField,
		OwnGoalLineSign,
		Slot,
		OutWorldLocation,
		LocalZ
	);
}

bool USoccerFormationBlueprintLibrary::IsBuiltInFormationCatalogValid()
{
	return SoccerFormationLibrary::IsBuiltInCatalogValid();
}
