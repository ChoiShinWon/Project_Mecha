#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayTagContainer.h"
#include "GA_QuickBoost_Boss.generated.h"

class AEnemyMecha;
class UAnimMontage;

UCLASS()
class PROJECT_MECHA_API UGA_QuickBoost_Boss : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UGA_QuickBoost_Boss();

protected:
    // == 부스트 파라미터 ==
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boost")
    float BoostDistance = 800.0f;        // 얼마나 이동할지 (짧게 튀기)

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boost")
    float BoostDuration = 0.15f;         // 몇 초 동안 이동하는 걸로 볼지 (짧을수록 더 폭발적)

    // 방향 옵션
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boost")
    bool bUseForwardDirection = true;    // true면 바라보는 방향, false면 타겟 기준으로

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boost")
    bool bBoostAwayFromTarget = false;   // true면 타겟 반대 방향(후퇴), false면 타겟 방향(돌진)

    // 태그 (필요하면)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tags")
    FGameplayTag Tag_AbilityQuickBoost;  // Ability.QuickBoost.Boss 같은거

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tags")
    FGameplayTag Tag_StateBoosting;      // State.Boosting

    // 몽타주 (선택)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
    UAnimMontage* BoostMontage;

    // 이동 세팅 저장
    bool bSavedMovementParams = false;
    float SavedBrakingFrictionFactor = 0.0f;
    float SavedBrakingDecelWalking = 0.0f;

    FTimerHandle BoostTimerHandle;

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
    ACharacter* GetEnemyChar(const FGameplayAbilityActorInfo* ActorInfo) const;
    AActor* GetCurrentTarget(ACharacter* EnemyChar) const;

    UFUNCTION()
    void StopBoost();
};
