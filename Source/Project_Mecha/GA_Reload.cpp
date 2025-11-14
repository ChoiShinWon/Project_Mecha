// GA_Reload.cpp
#include "GA_Reload.h"
#include "MechaAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "GameFramework/Character.h"
#include "Animation/AnimInstance.h"

UGA_Reload::UGA_Reload()
{
    InstancingPolicy   = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

// 능력 활성화
// - 태그로 발사 차단 → 몽타주/지연 → 완료 시 탄약 이전 → 태그 해제
void UGA_Reload::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        K2_EndAbility();
        return;
    }

    UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
    if (!ASC) { K2_EndAbility(); return; }

    // 이미 풀탄/예비탄 없음 → 장전 불필요
    const float Mag = ASC->GetNumericAttribute(UMechaAttributeSet::GetAmmoMagazineAttribute());
    const float Max = ASC->GetNumericAttribute(UMechaAttributeSet::GetMaxMagazineAttribute());
    const float Res = ASC->GetNumericAttribute(UMechaAttributeSet::GetAmmoReserveAttribute());
    if (Mag >= Max || Res <= 0.f)
    {
        K2_EndAbility();
        return;
    }

    // 장전 태그 부여(발사 차단)
    ASC->AddLooseGameplayTag(Tag_StateReloading);
    ASC->AddLooseGameplayTag(Tag_BlockFire);

    // 장전 몽타주(선택)
    if (ReloadMontage && ActorInfo && ActorInfo->AvatarActor.IsValid())
    {
        auto* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
            this, NAME_None, ReloadMontage, 1.0f, NAME_None, false, 1.0f, 0.0f, true);
        MontageTask->ReadyForActivation();
    }

    // 능력 생존과 함께 안전하게 대기
    auto* WaitTask = UAbilityTask_WaitDelay::WaitDelay(this, ReloadTime);
    WaitTask->OnFinish.AddDynamic(this, &UGA_Reload::OnReloadDelayFinished);
    WaitTask->ReadyForActivation();
}

// 대기 완료 콜백
// - 서버 권위로 예비탄에서 탄창으로 탄약을 옮긴다
void UGA_Reload::OnReloadDelayFinished()
{
    UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
    if (!ASC)
    {
        K2_EndAbility();
        return;
    }

    // 서버 권위에서 탄약 이전(예비탄 → 탄창)
    if (GetAvatarActorFromActorInfo()->HasAuthority())
    {
        const float Mag = ASC->GetNumericAttribute(UMechaAttributeSet::GetAmmoMagazineAttribute());
        const float Max = ASC->GetNumericAttribute(UMechaAttributeSet::GetMaxMagazineAttribute());
        const float Res = ASC->GetNumericAttribute(UMechaAttributeSet::GetAmmoReserveAttribute());

        const float Need = FMath::Max(0.f, Max - Mag);
        const float Take = FMath::Clamp(Need, 0.f, Res);

        if (Take > 0.f)
        {
            ASC->ApplyModToAttribute(UMechaAttributeSet::GetAmmoMagazineAttribute(), EGameplayModOp::Additive, +Take);
            ASC->ApplyModToAttribute(UMechaAttributeSet::GetAmmoReserveAttribute(),  EGameplayModOp::Additive, -Take);
        }
    }

    // 태그 정리
    ASC->RemoveLooseGameplayTag(Tag_StateReloading);
    ASC->RemoveLooseGameplayTag(Tag_BlockFire);

    K2_EndAbility();
}

// 종료 처리
// - 안전하게 태그 제거
void UGA_Reload::EndAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility, bool bWasCancelled)
{
    // 안전 차원에서 태그 제거
    if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
    {
        ASC->RemoveLooseGameplayTag(Tag_StateReloading);
        ASC->RemoveLooseGameplayTag(Tag_BlockFire);
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
