#include "MissionManager.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

AMissionManager::AMissionManager()
{
    PrimaryActorTick.bCanEverTick = false;
}

void AMissionManager::BeginPlay()
{
    Super::BeginPlay();

    // 레벨 시작 시 자동으로 미션 시작
    StartMission();
}

void AMissionManager::StartMission()
{
    MissionState = EMissionState::InProgress;
    CurrentKillCount = 0;

    UE_LOG(LogTemp, Log, TEXT("MissionManager: Mission Started (RequiredKillCount = %d)"), RequiredKillCount);
}

void AMissionManager::NotifyEnemyKilled(AActor* DeadEnemy)
{
    if (MissionState != EMissionState::InProgress)
    {
        return;
    }

    ++CurrentKillCount;

    UE_LOG(LogTemp, Log, TEXT("MissionManager: Enemy killed. %d / %d"),
        CurrentKillCount, RequiredKillCount);

    if (CurrentKillCount >= RequiredKillCount)
    {
        HandleMissionCleared();
    }
}

void AMissionManager::HandleMissionCleared()
{
    if (MissionState == EMissionState::Cleared)
    {
        return;
    }

    MissionState = EMissionState::Cleared;

    UE_LOG(LogTemp, Log, TEXT("MissionManager: Mission Cleared"));

    // 여기서 BP 쪽 연출 실행
    OnMissionClearedBP();
}
