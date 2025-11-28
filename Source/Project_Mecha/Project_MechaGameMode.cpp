// Project_MechaGameMode.cpp
// 게임 모드 - 기본 Pawn 클래스 설정

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Project_MechaGameMode.h"
#include "Project_MechaCharacter.h"
#include "UObject/ConstructorHelpers.h"

// ========================================
// 생성자
// ========================================
AProject_MechaGameMode::AProject_MechaGameMode()
{
	// 기본 Pawn 클래스를 블루프린트 캐릭터로 설정
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
