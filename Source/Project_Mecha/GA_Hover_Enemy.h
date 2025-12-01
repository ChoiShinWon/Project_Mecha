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
    // == 설정 값 ==
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hover|Enemy")
    bool bUseFlyingMode = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hover|Enemy")
    float HoverLiftImpulse = 600.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hover|Enemy")
    float ExitFallGravityScale = 1.5f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hover|Enemy")
    float GravityScaleWhileHover = 0.2f;

    // Enemy가 공중에 떠 있는 유지 시간
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hover|Enemy")
    float HoverDuration = 1.5f; // 초 단위, 필요하면 조절

    // 상태 태그
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hover|Tags")
    FGameplayTag Tag_StateHovering;

private:
    TWeakObjectPtr<ACharacter> OwnerCharacter;
    TWeakObjectPtr<UAbilitySystemComponent> ASC;

    float SavedGravityScale = 1.f;
    float SavedBrakingFrictionFactor = 2.f;
    float SavedGroundFriction = 8.f;

    FTimerHandle HoverTimerHandle;

    void StartHover();
    void StopHover();

    UFUNCTION()
    void OnHoverDurationEnded();
};
