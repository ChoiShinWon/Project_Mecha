// GA_GunFire.h
#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
// 설명:
// - 플레이어 발사 능력.
// - 활성화 시 재장전/차단 태그 확인 → 투사체 스폰 → 탄창(AmmoMagazine) 1 감소.
#include "GameplayTagContainer.h"
#include "GA_GunFire.generated.h"

UCLASS()
class PROJECT_MECHA_API UGA_GunFire : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UGA_GunFire();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData) override;

    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        bool bReplicateEndAbility, bool bWasCancelled) override;

    UPROPERTY(EditDefaultsOnly, Category="GunFire|Montage")
    UAnimMontage* FireMontage = nullptr;

    UPROPERTY(EditDefaultsOnly, Category="FX")
    UParticleSystem* MuzzleFlash = nullptr;

// 발사 차단/상태 태그
// - 장전 중 또는 시스템적으로 발사를 막아야 할 때 부여되는 태그
    UPROPERTY(EditDefaultsOnly, Category="Tags")
    FGameplayTag Tag_StateReloading;   // "State.Reloading"

    UPROPERTY(EditDefaultsOnly, Category="Tags")
    FGameplayTag Tag_BlockFire;        // "Block.Fire"

protected:
    void SpawnProjectile(const FGameplayAbilityActorInfo* ActorInfo);

    UFUNCTION()
    void OnMontageNotify(); // 필요 시 사용

    // 소켓이 없을 때 사용하는 스폰 오프셋 (앞쪽 거리)
    UPROPERTY(EditDefaultsOnly, Category = "GunFire|Spawn")
    float FallbackSpawnOffset = 100.f;
};
