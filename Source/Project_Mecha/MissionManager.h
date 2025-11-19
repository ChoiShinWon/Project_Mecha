#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MissionManager.generated.h"

UENUM(BlueprintType)
enum class EMissionState : uint8
{
    Inactive,
    InProgress,
    Cleared,
    Failed
};

UCLASS()
class PROJECT_MECHA_API AMissionManager : public AActor
{
    GENERATED_BODY()

public:
    AMissionManager();

protected:
    virtual void BeginPlay() override;

public:
    // 필요 킬 수 (에디터에서 설정 가능)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission")
    int32 RequiredKillCount = 10;

    // 현재까지 잡은 적 수
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mission")
    int32 CurrentKillCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mission")
    EMissionState MissionState = EMissionState::Inactive;

    // 레벨 시작 시 자동으로 부를 거지만, 나중에 수동 시작도 가능하게 둠
    UFUNCTION(BlueprintCallable, Category = "Mission")
    void StartMission();

    // 적이 죽을 때 Enemy가 이 함수를 호출
    UFUNCTION(BlueprintCallable, Category = "Mission")
    void NotifyEnemyKilled(AActor* DeadEnemy);

protected:
    void HandleMissionCleared();

    // 미션 클리어 시 BP에서 연출 붙이는 이벤트
    UFUNCTION(BlueprintImplementableEvent, Category = "Mission")
    void OnMissionClearedBP();
};
