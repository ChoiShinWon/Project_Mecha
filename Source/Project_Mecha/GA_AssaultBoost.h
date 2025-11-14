#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
// 설명:
// - 어설트 부스트: 짧은 시간 전방으로 강제 돌진하는 능력.
// - 에너지 소모/과열 태그를 관리하며, 부스트 동안 마우스 룩 입력을 잠시 차단합니다.
#include "GA_AssaultBoost.generated.h"

UCLASS()
class PROJECT_MECHA_API UGA_AssaultBoost : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UGA_AssaultBoost();

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
        bool bReplicateEndAbility, bool bWasCancelled
    ) override;

protected:
    // ===== Movement / Duration =====
    UPROPERTY(EditDefaultsOnly, Category = "Boost")
    float BoostForce = 4000.f;

    UPROPERTY(EditDefaultsOnly, Category = "Boost")
    float BoostDuration = 0.8f;

    UPROPERTY(EditDefaultsOnly, Category = "Boost|Anim")
    UAnimMontage* BoostMontage;

    // ===== FX =====
    UPROPERTY(EditDefaultsOnly, Category = "FX")
    UParticleSystem* BoostParticle;

    UPROPERTY(EditDefaultsOnly, Category = "FX")
    TArray<FName> BoostSockets;

    // ===== Energy System =====
    UPROPERTY(EditDefaultsOnly, Category = "Boost|Energy")
    TSubclassOf<UGameplayEffect> GE_AssaultBoostDrain;

    UPROPERTY(EditDefaultsOnly, Category = "Boost|Energy")
    float MinEnergyToStart = 5.f;

    // ===== Tags =====
    UPROPERTY(EditDefaultsOnly, Category = "Boost|Tags")
    FGameplayTag Tag_StateOverheat;

    // ===== Runtime =====
    UPROPERTY() ACharacter* OwnerChar = nullptr;
    UPROPERTY() UAbilitySystemComponent* ASC = nullptr;

    FActiveGameplayEffectHandle DrainGEHandle;
    FDelegateHandle EnergyChangedHandle;

    UPROPERTY() TArray<UParticleSystemComponent*> ActiveBoostFX;

    // ===== Internal Functions =====
    bool IsEnergySufficient(float Threshold = 0.f) const;

    void ApplyDrainGE();
    void RemoveDrainGE();
    void ApplyOverheat();
    void OnEnergyChanged(const FOnAttributeChangeData& Data);

    // ===== Input Lock (Look) =====
    // 부스트 시작 시 마우스 룩 입력을 차단하고, 종료 시 원복하기 위한 이전 상태 저장
    bool bPrevIgnoreLookInput = false;
};
