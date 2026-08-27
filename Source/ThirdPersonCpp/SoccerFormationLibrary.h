#pragma once

#include "CoreMinimal.h"
#include "SoccerFormationTypes.h"

/*
 * Built-in seven-a-side formation catalog.
 *
 * Stage 1 keeps presets in C++ so all nine systems are immediately available
 * without requiring .uasset creation. A later UI/data-authoring stage can put
 * the same FSoccerFormationDefinition data behind assets without changing the
 * runtime consumers.
 */
namespace SoccerFormationLibrary
{
	THIRDPERSONCPP_API const FSoccerFormationDefinition& GetDefinition(
		ESoccerFormationSystem System
	);

	THIRDPERSONCPP_API bool IsValidSevenASideDefinition(
		const FSoccerFormationDefinition& Definition
	);
}
