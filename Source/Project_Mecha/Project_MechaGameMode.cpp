// Copyright Epic Games, Inc. All Rights Reserved.

#include "Project_MechaGameMode.h"
#include "Project_MechaCharacter.h"
#include "UObject/ConstructorHelpers.h"

AProject_MechaGameMode::AProject_MechaGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
