// MissionManager.cpp

#include "MissionManager.h"
#include "EnemyMecha.h"
#include "Engine/World.h"

AMissionManager::AMissionManager()
{
    PrimaryActorTick.bCanEverTick = false;

    // 멀티까지 당장 안 볼 거면 굳이 켤 필요 없음
    // 나중에 확장할 생각이면 bReplicates = true; 유지해도 큰 문제는 없다.
    // bReplicates = true;
}

void AMissionManager::StartMission()
{
    // 서버만 따질 거면 HasAuthority 체크 유지,
    // 완전 싱글이면 이 if 문 통째로 지워도 된다.
    if (!HasAuthority())
    {
        return;
    }

    CurrentKillCount = 0;
    bMissionActive = true;
    bBossPhaseStarted = false;
    bBossDefeated = false;
    BossInstance = nullptr;

    MissionStartTime = GetWorld()->GetTimeSeconds();
    MissionEndTime = 0.f;

    // 그냥 로컬에서 바로 BP 이벤트 호출
    OnMissionStartedBP(RequiredKillCount);
}

void AMissionManager::NotifyEnemyKilled(AEnemyMecha* KilledEnemy)
{
    if (!HasAuthority() || !bMissionActive)
    {
        return;
    }

    ++CurrentKillCount;

    OnMissionProgressBP(CurrentKillCount, RequiredKillCount);

    if (CurrentKillCount >= RequiredKillCount && !bBossPhaseStarted)
    {
        StartBossPhase();
    }
}

void AMissionManager::StartBossPhase()
{
    if (!HasAuthority())
    {
        return;
    }

    if (bBossPhaseStarted)
    {
        return;
    }

    bBossPhaseStarted = true;

    // 여기서는 "보스 페이즈 시작" 신호만 BP로 넘긴다.
    OnBossPhaseStartedBP();
}



// 보스가 죽었을 때 Boss 쪽(EnemyMecha / BP_BossCrunch)에서 호출
void AMissionManager::NotifyBossDefeated(AEnemyMecha* DefeatedBoss)
{
    if (!HasAuthority())
    {
        return;
    }

    // 다른 Enemy가 잘못 호출하는 거 방지
    if (DefeatedBoss != BossInstance)
    {
        return;
    }

    if (bBossDefeated)
    {
        return;
    }

    bBossDefeated = true;
    bMissionActive = false;
    MissionEndTime = GetWorld()->GetTimeSeconds();

    // 여기서 진짜 최종 미션 클리어 (랭크 화면/결과 화면으로 연결)
    OnMissionClearedBP();
}
