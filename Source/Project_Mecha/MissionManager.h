// MissionManager.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MissionManager.generated.h"

class AEnemyMecha;

UCLASS()
class PROJECT_MECHA_API AMissionManager : public AActor
{
    GENERATED_BODY()

public:
    AMissionManager();

    // === 미션 상태 ===
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
    int32 RequiredKillCount = 5;

    UPROPERTY(BlueprintReadOnly, Category = "Mission")
    int32 CurrentKillCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Mission")
    bool bMissionActive = false;

    UFUNCTION(BlueprintCallable, Category = "Mission")
    void StartMission();

    UFUNCTION(BlueprintCallable, Category = "Mission")
    void NotifyEnemyKilled(AEnemyMecha* KilledEnemy);

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

    // === BP에서 구현할 이벤트들 ===
    UFUNCTION(BlueprintImplementableEvent, Category = "Mission|BP")
    void OnMissionStartedBP(int32 InRequiredKillCount);

    UFUNCTION(BlueprintImplementableEvent, Category = "Mission|BP")
    void OnMissionProgressBP(int32 InCurrentKillCount, int32 InRequiredKillCount);

    UFUNCTION(BlueprintImplementableEvent, Category = "Mission|BP")
    void OnMissionClearedBP();

    // === 서버 → 모든 클라로 브로드캐스트 ===
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_OnMissionStarted(int32 InRequiredKillCount);

    UFUNCTION(NetMulticast, Reliable)
    void Multicast_OnMissionProgress(int32 InCurrentKillCount, int32 InRequiredKillCount);

    UFUNCTION(NetMulticast, Reliable)
    void Multicast_OnMissionCleared();
};
