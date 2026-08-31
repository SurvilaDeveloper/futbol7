#pragma once

#include "CoreMinimal.h"
#include "SoccerFormationTypes.h"
#include "SoccerPlayerProfileTypes.h"

class ASoccerField;

/*
 * Built-in seven-a-side formation catalog.
 *
 * Formation definitions are structural data: they describe which slots exist
 * and where those slots live in normalized team-relative pitch coordinates.
 * They do not assign concrete players and they do not issue tactical orders.
 */
namespace SoccerFormationLibrary
{
	THIRDPERSONCPP_API const FSoccerFormationDefinition& GetDefinition(
		ESoccerFormationSystem System
	);

	/** Stable list used by manager UI / validation. */
	THIRDPERSONCPP_API const TArray<ESoccerFormationSystem>& GetAllSystems();

	THIRDPERSONCPP_API bool IsValidSevenASideDefinition(
		const FSoccerFormationDefinition& Definition
	);

	/** Validates every built-in formation and its declared enum identity. */
	THIRDPERSONCPP_API bool IsBuiltInCatalogValid();

	/** Returns nullptr when SlotId does not exist in this formation. */
	THIRDPERSONCPP_API const FSoccerFormationSlot* FindSlotById(
		const FSoccerFormationDefinition& Definition,
		FName SlotId
	);

	THIRDPERSONCPP_API bool IsSlotValidForFormation(
		ESoccerFormationSystem System,
		FName SlotId
	);

	/** Human-readable football position for UI. SlotId remains the stable key. */
	THIRDPERSONCPP_API FText GetSlotDisplayName(
		const FSoccerFormationSlot& Slot
	);

	/**
	 * Maps a formation slot to the closest permanent player-profile position.
	 * Defensive/attacking midfield are still midfield profile positions; the
	 * formation line remains available separately for tactical evaluation.
	 */
	THIRDPERSONCPP_API ESoccerPlayerNaturalPosition GetNaturalPositionForSlot(
		const FSoccerFormationSlot& Slot
	);

	/**
	 * Converts team-relative normalized slot coordinates into authoritative
	 * SoccerField world space.
	 *
	 * OwnGoalLineSign: -1 = team's own goal is local -X, +1 = local +X.
	 * LateralAlpha is interpreted from the team's own attacking perspective,
	 * so left/right automatically mirror when the team changes ends.
	 */
	THIRDPERSONCPP_API bool TryResolveSlotWorldLocation(
		const ASoccerField* SoccerField,
		float OwnGoalLineSign,
		const FSoccerFormationSlot& Slot,
		FVector& OutWorldLocation,
		float LocalZ = 0.0f
	);
}
