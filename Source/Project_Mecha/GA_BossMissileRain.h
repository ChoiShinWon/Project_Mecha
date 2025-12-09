// GA_BossMissileRain.h
#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_BossMissileRain.generated.h"

class UAnimMontage;

/**
 * 보스 패턴:
 * - 7초 동안 공중에 떠서
 * - 좌/우 소켓(Muzzle01, Muzzle02)에서 미사일 5쌍(총 10발) 발사
 * - 미사일은 플레이어 방향을 보고 발사 (추격 로직은 Projectile 쪽에서 처리)
 */
UCLASS()
class PROJECT_MECHA_API UGA_BossMissileRain : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UGA_BossMissileRain();

    // 능력 발동
    virtual void ActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData
    ) override;

    // 능력 종료
    virtual void EndAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        bool bReplicateEndAbility,
        bool bWasCancelled
    ) override;

protected:
    // ===== 에디터에서 세팅할 값 =====

    // 발사할 미사일 클래스 (BP_EnemyMissile 등)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Missile")
    TSubclassOf<AActor> MissileClass;

    // 왼쪽 / 오른쪽 머즐 소켓 이름
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Missile")
    FName LeftMuzzleSocketName;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Missile")
    FName RightMuzzleSocketName;

    // 공중에 떠있는 시간
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Missile")
    float HoverDuration;

    // 각쪽에서 몇 발씩 쏠지 (5면 좌 5발, 우 5발 = 총 10발)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Missile")
    int32 ShotsPerSide;

    // 몇 초 간격으로 한 쌍(좌+우) 발사할지
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Missile")
    float FireInterval;

    // 발사 중 재생할 Montage
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Animation")
    UAnimMontage* FireMontage;

    // ===== 내부용 상태 =====

    // 현재까지 발사한 쌍의 수 (0 ~ ShotsPerSide)
    int32 ShotsFiredPairs;

    // 타이머 핸들
    FTimerHandle FireTimerHandle;
    FTimerHandle EndTimerHandle;

    // ===== 내부 함수 =====
    UFUNCTION()
    void SpawnMissilePair();

    UFUNCTION()
    void OnMissileRainFinished();

    void StartHover(const FGameplayAbilityActorInfo* ActorInfo);
    void EndHover(const FGameplayAbilityActorInfo* ActorInfo);
};
