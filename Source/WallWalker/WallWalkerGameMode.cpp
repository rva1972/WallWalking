// Copyright Epic Games, Inc. All Rights Reserved.

#include "WallWalkerGameMode.h"
#include "WallWalkerCharacter.h"
#include "UObject/ConstructorHelpers.h"

AWallWalkerGameMode::AWallWalkerGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPersonCPP/Blueprints/ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
