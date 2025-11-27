// MissionManager.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MissionManager.generated.h"

class AEnemyMecha;
class AActor;

UCLASS()
class PROJECT_MECHA_API AMissionManager : public AActor
{
    GENERATED_BODY()

public:
    AMissionManager();

    // === 미션 상태 ===
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
    int32 RequiredKillCount = 10;   // 보스 나오기 전까지 잡아야 할 적 수

    UPROPERTY(BlueprintReadOnly, Category = "Mission")
    int32 CurrentKillCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Mission")
    bool bMissionActive = false;

    // === 보스 페이즈 상태 ===
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss")
    TSubclassOf<AEnemyMecha> BossClass;      // BP_BossCrunch 넣을 자리

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss")
    AActor* BossSpawnPoint = nullptr;        // TargetPoint 등

    UPROPERTY(BlueprintReadWrite, Category = "Boss")
    AEnemyMecha* BossInstance = nullptr;     // 스폰된 보스

    UPROPERTY(BlueprintReadOnly, Category = "Boss")
    bool bBossPhaseStarted = false;          // 보스 페이즈 돌입 여부

    UPROPERTY(BlueprintReadOnly, Category = "Boss")
    bool bBossDefeated = false;              // 보스 처치 여부

    // === 미션 제어 ===
    UFUNCTION(BlueprintCallable, Category = "Mission")
    void StartMission();

    UFUNCTION(BlueprintCallable, Category = "Mission")
    void NotifyEnemyKilled(AEnemyMecha* KilledEnemy);

    // 보스가 죽었을 때(EnemyMecha / BP_BossCrunch에서 호출)
    UFUNCTION(BlueprintCallable, Category = "Boss")
    void NotifyBossDefeated(AEnemyMecha* DefeatedBoss);

    UFUNCTION(BlueprintPure, Category = "Mission")
    float GetMissionClearDuration() const
    {
        return (MissionEndTime > MissionStartTime)
            ? (MissionEndTime - MissionStartTime)
            : 0.f;
    }

protected:
    float MissionStartTime = 0.f;
    float MissionEndTime = 0.f;

    // === 내부용: 보스 페이즈 시작 ===
    void StartBossPhase();

    // === BP에서 구현할 이벤트들 ===
    // → 이제 NetMulticast 없이, 로컬에서 직접 호출해서 UI/컷신 처리

    UFUNCTION(BlueprintImplementableEvent, Category = "Mission|BP")
    void OnMissionStartedBP(int32 InRequiredKillCount);

    UFUNCTION(BlueprintImplementableEvent, Category = "Mission|BP")
    void OnMissionProgressBP(int32 InCurrentKillCount, int32 InRequiredKillCount);

    // "보스까지 죽인 뒤" 최종 클리어 시점
    UFUNCTION(BlueprintImplementableEvent, Category = "Mission|BP")
    void OnMissionClearedBP();

    // 보스 페이즈 시작 시(보스 스폰 직후) BP 컷신/카메라 처리용
    UFUNCTION(BlueprintImplementableEvent, Category = "Boss|BP")
    void OnBossPhaseStartedBP();
};
