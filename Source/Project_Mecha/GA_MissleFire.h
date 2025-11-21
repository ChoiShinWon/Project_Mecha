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
class AEnemyAirship;

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

protected:
    // ===== Internal Functions (내부 함수) =====

    void SpawnMissle(int32 Index, class ACharacter* OwnerChar);
    AActor* PickBestTarget(const AActor* Owner) const;
    void SetupHoming(class AActor* Missle, AActor* Target) const;
    void InitDamageOnMissle(AActor* Missle, AActor* Source) const;
    class UProjectileMovementComponent* ForceMakeMovableAndLaunch(class AActor* Missle, const FVector& LaunchDir) const;

    // [Cooldown] 미사일 쿨타임 적용 헬퍼
    void ApplyMissileCooldown(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo);

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

    UPROPERTY(EditDefaultsOnly, Category = "Missle")
    float SpreadAngle = 6.0f;

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
};
