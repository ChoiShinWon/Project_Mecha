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

    // === 미션 설정 ===
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
    int32 RequiredKillCount = 10;

    UPROPERTY(BlueprintReadOnly, Category = "Mission")
    int32 CurrentKillCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Mission")
    bool bMissionActive = false;

    // === 보스 페이즈 설정 ===
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss")
    TSubclassOf<AEnemyMecha> BossClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss")
    AActor* BossSpawnPoint = nullptr;

    UPROPERTY(BlueprintReadWrite, Category = "Boss")
    AEnemyMecha* BossInstance = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "Boss")
    bool bBossPhaseStarted = false;

    UPROPERTY(BlueprintReadOnly, Category = "Boss")
    bool bBossDefeated = false;

    // === 미션 함수 ===
    UFUNCTION(BlueprintCallable, Category = "Mission")
    void StartMission();

    UFUNCTION(BlueprintCallable, Category = "Mission")
    void NotifyEnemyKilled(AEnemyMecha* KilledEnemy);

    // 보스가 죽었을 때 호출 (EnemyMecha에서 호출)
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

    // === 내부: 보스 페이즈 시작 ===
    void StartBossPhase();

    // === BP에서 구현할 이벤트들 ===
    UFUNCTION(BlueprintImplementableEvent, Category = "Mission|BP")
    void OnMissionStartedBP(int32 InRequiredKillCount);

    UFUNCTION(BlueprintImplementableEvent, Category = "Mission|BP")
    void OnMissionProgressBP(int32 InCurrentKillCount, int32 InRequiredKillCount);

    UFUNCTION(BlueprintImplementableEvent, Category = "Mission|BP")
    void OnMissionClearedBP();

    UFUNCTION(BlueprintImplementableEvent, Category = "Boss|BP")
    void OnBossPhaseStartedBP();
};
