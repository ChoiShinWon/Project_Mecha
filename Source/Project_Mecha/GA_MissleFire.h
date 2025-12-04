// GA_MissleFire.h
// 설명:
// - 미사일 발사 능력 클래스.
// - 여러 발의 미사일을 순차적으로 발사하며, 자동으로 가장 가까운 적을 추적한다.
// - ProjectileMovementComponent를 강제로 생성하여 유도 미사일 기능을 구현한다.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayTagContainer.h"                // [Cooldown] 태그용
#include "GA_MissleFire.generated.h"

class UProjectileMovementComponent;
class UGameplayEffect;

// 설명:
// - 미사일 발사 능력. 순차 발사, 적 자동 추적, 유도 미사일.

UCLASS()
class PROJECT_MECHA_API UGA_MissleFire : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UGA_MissleFire();

    virtual void ActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData
    ) override;

    virtual void EndAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        bool bReplicateEndAbility,
        bool bWasCancelled) override;

protected:
    // ===== Internal Functions (내부 함수) =====

    void SpawnMissle(int32 Index, class ACharacter* OwnerChar);
    AActor* PickBestTarget(const AActor* Owner) const;
    void InitDamageOnMissle(AActor* Missle, AActor* Source) const;
    class UProjectileMovementComponent* ForceMakeMovableAndLaunch(class AActor* Missle, const FVector& LaunchDir) const;

    // [Cooldown] 미사일 쿨타임 적용 헬퍼
    void ApplyMissileCooldown(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo);

    // 타이머 정리 헬퍼 함수
    void ClearAllTimers();

public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS", meta = (DisplayName = "GE Melee Damage"))
    TSubclassOf<UGameplayEffect> GE_MissleDamage;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS", meta = (DisplayName = "Set by Caller Damage Name"))
    FName SetByCallerDamageName = TEXT("Data.Damage");

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS")
    float BaseDamage = 80.f;

protected:
    UPROPERTY(EditDefaultsOnly, Category = "Missle")
    TSubclassOf<AActor> MissleProjectileClass;

    UPROPERTY(EditDefaultsOnly, Category = "Missle")
    int32 NumProjectiles = 4;

    UPROPERTY(EditDefaultsOnly, Category = "Missle")
    float TimeBetweenShots = 0.15f;

    UPROPERTY(EditDefaultsOnly, Category = "Missle")
    float SpawnOffset = 100.f;

    // 소켓에서 스폰할 때 충돌 방지를 위한 추가 오프셋
    UPROPERTY(EditDefaultsOnly, Category = "Missle")
    float SocketCollisionOffset = 80.f;

    // 소켓이 없을 때 Z축 오프셋
    UPROPERTY(EditDefaultsOnly, Category = "Missle")
    float FallbackZOffset = 50.f;

    UPROPERTY(EditDefaultsOnly, Category = "Missle")
    float SpreadAngle = 6.0f;

    // 모든 미사일 발사 완료 후 능력 종료까지의 버퍼 시간
    UPROPERTY(EditDefaultsOnly, Category = "Missle")
    float EndAbilityBufferTime = 0.05f;

    /** 타격 여부와 상관없이 기본 발사 속도 */
    UPROPERTY(EditDefaultsOnly, Category = "Missle|Movement")
    float InitialLaunchSpeed = 3000.f;

    UPROPERTY(EditDefaultsOnly, Category = "Missle|Homing")
    float HomingAcceleration = 8000.f;

    UPROPERTY(EditDefaultsOnly, Category = "Missle|Homing")
    float MaxLockDistance = 12000.f;

    // ================== [Cooldown] ==================

    // 쿨타임 태그
    FGameplayTag Tag_CooldownMissile;

    // 쿨타임 길이 (초)
    UPROPERTY(EditDefaultsOnly, Category = "Cooldown")
    float CooldownDuration = 5.0f;

    // ================== 타이머 핸들 관리 ==================
    // 미사일 발사 타이머 핸들들 (순차 발사용)
    TArray<FTimerHandle> MissileFireTimerHandles;
    
    // 능력 종료 타이머 핸들
    FTimerHandle EndAbilityTimerHandle;
    
    // 쿨타임 타이머 핸들
    FTimerHandle CooldownTimerHandle;
};
