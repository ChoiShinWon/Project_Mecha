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
    // == 대시 파라미터 ==
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dash")
    float DashDistance = 3000.0f;        // 대시 최대 거리 (0이면 무제한)

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dash")
    float DashDuration = 0.35f;          // 대시가 지속되는 시간 (초 단위)

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dash")
    float MinStopDistance = 300.0f;      // 타겟과 최소 거리 유지

    // 실제 이동 거리(TravelDistance)에 곱하는 배수
    // 1.0 : Distance - MinStopDistance 만큼
    // 2.0 : 그 2배 거리만큼 더 이동하게 함
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dash")
    float DashSpeedMultiplier = 2.0f;

    // == 태그 (게임플레이) ==
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tags")
    FGameplayTag Tag_AbilityDash;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tags")
    FGameplayTag Tag_StateDashing;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tags")
    FGameplayTag Tag_CooldownDash;

    // == 쿨다운 이펙트 (선택) ==
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects")
    TSubclassOf<class UGameplayEffect> DashCooldownEffectClass;

    // == 대시 몽타주 (선택) ==
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dash|Animation")
    UAnimMontage* DashMontage;

    // == 대시 중 Movement 파라미터 저장 ==
    bool bSavedMovementParams = false;

    float SavedBrakingFrictionFactor = 0.0f;
    float SavedBrakingDecelWalking = 0.0f;

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

    // EnemyMecha에서 CurrentTarget 가져오기
    AActor* GetDashTarget(ACharacter* EnemyChar) const;
};
