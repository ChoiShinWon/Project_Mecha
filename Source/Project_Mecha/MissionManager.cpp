// MissionManager.cpp

#include "MissionManager.h"
#include "EnemyMecha.h"
#include "Engine/World.h"

AMissionManager::AMissionManager()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;   // ★ 반드시 켜라
}



void AMissionManager::StartMission()
{
    // 서버에서만 미션 시작
    if (!HasAuthority())
    {
        return;
    }

    CurrentKillCount = 0;
    bMissionActive = true;
    MissionStartTime = GetWorld()->GetTimeSeconds();

    // 서버 → 전체에게 전달
    Multicast_OnMissionStarted(RequiredKillCount);
}

void AMissionManager::NotifyEnemyKilled(AEnemyMecha* KilledEnemy)
{
    if (!HasAuthority() || !bMissionActive)
    {
        return;
    }

    ++CurrentKillCount;

    Multicast_OnMissionProgress(CurrentKillCount, RequiredKillCount);

    if (CurrentKillCount >= RequiredKillCount)
    {
        bMissionActive = false;
        MissionEndTime = GetWorld()->GetTimeSeconds();

        Multicast_OnMissionCleared();
    }
}

// === Multicast 구현부 ===

void AMissionManager::Multicast_OnMissionStarted_Implementation(int32 InRequiredKillCount)
{
    // 서버 + 모든 클라에서 다 실행
    OnMissionStartedBP(InRequiredKillCount);
}

void AMissionManager::Multicast_OnMissionProgress_Implementation(int32 InCurrentKillCount, int32 InRequiredKillCount)
{
    OnMissionProgressBP(InCurrentKillCount, InRequiredKillCount);
}

void AMissionManager::Multicast_OnMissionCleared_Implementation()
{
    OnMissionClearedBP();
}
