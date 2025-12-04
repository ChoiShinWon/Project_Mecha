// MissionManager.cpp
// 미션 진행 관리 - 적 처치 카운트, 보스 페이즈, 미션 클리어

#include "MissionManager.h"
#include "EnemyMecha.h"
#include "Engine/World.h"

// ========================================
// 생성자
// ========================================
AMissionManager::AMissionManager()
{
	PrimaryActorTick.bCanEverTick = false;
}

// ========================================
// 미션 시작
// ========================================
void AMissionManager::StartMission()
{
	// 카운터 초기화
	CurrentKillCount = 0;
	bMissionActive = true;
	bBossPhaseStarted = false;
	bBossDefeated = false;
	BossInstance = nullptr;

	// 시간 기록
	MissionStartTime = GetWorld()->GetTimeSeconds();
	MissionEndTime = 0.f;

	// 블루프린트 이벤트 호출
	OnMissionStartedBP(RequiredKillCount);
}

// ========================================
// 적 처치 알림
// ========================================
void AMissionManager::NotifyEnemyKilled(AEnemyMecha* KilledEnemy)
{
	// 미션 활성화 상태에서만 처리
	if (!bMissionActive)
	{
		return;
	}

	// 킬 카운트 증가
	++CurrentKillCount;

	// 진행도 업데이트 이벤트
	OnMissionProgressBP(CurrentKillCount, RequiredKillCount);

	// 목표 달성 시 보스 페이즈 시작
	if (CurrentKillCount >= RequiredKillCount && !bBossPhaseStarted)
	{
		StartBossPhase();
	}
}

// ========================================
// 보스 페이즈 시작
// ========================================
void AMissionManager::StartBossPhase()
{
	// 이미 시작했으면 무시
	if (bBossPhaseStarted)
	{
		return;
	}

	bBossPhaseStarted = true;

	// 블루프린트 이벤트 호출 (보스 스폰, UI 업데이트 등)
	OnBossPhaseStartedBP();
}

// ========================================
// 보스 격파 알림
// ========================================
void AMissionManager::NotifyBossDefeated(AEnemyMecha* DefeatedBoss)
{
	// 등록된 보스가 아니면 무시
	if (DefeatedBoss != BossInstance)
	{
		return;
	}

	// 이미 처리했으면 무시
	if (bBossDefeated)
	{
		return;
	}

	// 미션 클리어 처리
	bBossDefeated = true;
	bMissionActive = false;
	MissionEndTime = GetWorld()->GetTimeSeconds();

	// 블루프린트 이벤트 호출 (승리 화면, 보상 지급 등)
	OnMissionClearedBP();
}
