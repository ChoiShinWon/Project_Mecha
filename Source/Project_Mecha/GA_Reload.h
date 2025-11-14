// GA_Reload.h
#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
// 설명:
// - 재장전 능력. 시작 시 '장전 중' 태그를 걸어 발사를 차단하고,
//   지정 시간 대기 후 탄약을 보충한 다음 태그를 해제합니다.
// - 탄창/예비탄 Attribute를 사용하여 서버 권위로 안전하게 수치를 이전합니다.
#include "GameplayTagContainer.h"
#include "GA_Reload.generated.h"

UCLASS()
class PROJECT_MECHA_API UGA_Reload : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UGA_Reload();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData) override;

    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        bool bReplicateEndAbility, bool bWasCancelled) override;

    /** 장전 시간(초) */
    UPROPERTY(EditDefaultsOnly, Category="Reload")
    float ReloadTime = 2.0f;

    /** 장전 몽타주(선택) */
    UPROPERTY(EditDefaultsOnly, Category="Reload|Montage")
    UAnimMontage* ReloadMontage = nullptr;

/** 장전 중/발사 차단 태그
 *  - 재장전 동안 발사 입력을 무시하도록 ASC에 부여/해제
 */
    UPROPERTY(EditDefaultsOnly, Category="Tags")
    FGameplayTag Tag_StateReloading;   // "State.Reloading"

    UPROPERTY(EditDefaultsOnly, Category="Tags")
    FGameplayTag Tag_BlockFire;        // "Block.Fire"

private:
    UFUNCTION()
    void OnReloadDelayFinished();
};
