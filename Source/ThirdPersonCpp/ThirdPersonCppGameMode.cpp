// Copyright Epic Games, Inc. All Rights Reserved.

#include "ThirdPersonCppGameMode.h"
#include "ThirdPersonCppCharacter.h"
#include "UObject/ConstructorHelpers.h"
#include "GameHUD.h"

AThirdPersonCppGameMode::AThirdPersonCppGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPersonCPP/Blueprints/ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}

	HUDClass = AGameHUD::StaticClass();
}
