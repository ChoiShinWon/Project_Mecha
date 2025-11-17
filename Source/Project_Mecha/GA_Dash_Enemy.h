#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayTagContainer.h"
#include "GA_Dash_Enemy.generated.h"

class AEnemyMecha;
class UAnimMontage;

UCLASS()
class PROJECT_MECHA_API UGA_Dash_Enemy : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UGA_Dash_Enemy();

protected:
    // == Dash 파라미터 ==
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dash")
    float DashDistance = 3000.0f;        // 대쉬 최대 거리 (0이면 무제한)

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dash")
    float DashDuration = 0.35f;          // 대쉬에 걸리는 시간 (짧을수록 빠르게 훅)

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dash")
    float MinStopDistance = 300.0f;      // 타겟과 이 정도는 남겨두고 멈춤

    // == 태그 (정리용) ==
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tags")
    FGameplayTag Tag_AbilityDash;        // Ability.Dash.Enemy

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tags")
    FGameplayTag Tag_StateDashing;       // State.Dashing

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tags")
    FGameplayTag Tag_CooldownDash;       // Cooldown.Dash (원하면 GE에서 사용)

    // == 쿨다운 이펙트 (선택) ==
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects")
    TSubclassOf<class UGameplayEffect> DashCooldownEffectClass;

    // == 대쉬 몽타주 (선택) ==
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dash|Animation")
    UAnimMontage* DashMontage;

    FTimerHandle DashTimerHandle;

    virtual void ActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData) override;

    virtual void EndAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        bool bReplicateEndAbility,
        bool bWasCancelled) override;

protected:
    UFUNCTION()
    void StopDash();

    class ACharacter* GetEnemyCharacter(const FGameplayAbilityActorInfo* ActorInfo) const;

    // EnemyMecha에서 CurrentTarget 꺼내기
    AActor* GetDashTarget(ACharacter* EnemyChar) const;
};
