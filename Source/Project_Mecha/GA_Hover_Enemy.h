#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayTagContainer.h"
#include "GA_Hover_Enemy.generated.h"

class ACharacter;
class UAbilitySystemComponent;

UCLASS()
class PROJECT_MECHA_API UGA_Hover_Enemy : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UGA_Hover_Enemy();

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
    // == 호버 파라미터 ==
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hover|Enemy")
    bool bUseFlyingMode = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hover|Enemy")
    float HoverLiftImpulse = 600.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hover|Enemy")
    float ExitFallGravityScale = 1.5f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hover|Enemy")
    float GravityScaleWhileHover = 0.2f;

    // Enemy가 호버할 수 있는 최대 지속 시간
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hover|Enemy")
    float HoverDuration = 1.5f;

    // == 게임플레이 태그 ==
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hover|Tags")
    FGameplayTag Tag_StateHovering;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hover|Tags")
    FGameplayTag Tag_CooldownHover;

    // 쿨다운 이펙트 (선택)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hover|Effects")
    TSubclassOf<class UGameplayEffect> HoverCooldownEffectClass;

    // 쿨타임 (초) - GE 대신 단순 타이머로 사용 가능
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hover|Enemy")
    float HoverCooldownDuration = 5.0f;

private:
    TWeakObjectPtr<ACharacter> OwnerCharacter;
    TWeakObjectPtr<UAbilitySystemComponent> ASC;

    float SavedGravityScale = 1.f;
    float SavedBrakingFrictionFactor = 2.f;
    float SavedGroundFriction = 8.f;

    FTimerHandle HoverTimerHandle;
    FTimerHandle CooldownTimerHandle;

    void StartHover();
    void StopHover();

    UFUNCTION()
    void OnHoverDurationEnded();

};
