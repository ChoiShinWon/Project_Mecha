// GA_Reload.cpp
// 장전 능력 - 예비탄에서 탄창으로 탄약 보충, 장전 중 발사 차단

#include "GA_Reload.h"
#include "MechaAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "GameFramework/Character.h"
#include "Animation/AnimInstance.h"

// ========================================
// 생성자
// ========================================
UGA_Reload::UGA_Reload()
{
	// 액터마다 하나의 인스턴스만 생성
	InstancingPolicy   = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

// ========================================
// 능력 활성화 - 장전 시작
// ========================================
void UGA_Reload::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	// 코스트/쿨다운 체크
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		K2_EndAbility();
		return;
	}

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC) 
	{ 
		K2_EndAbility(); 
		return; 
	}

	// ========== 장전 필요 여부 확인 ==========
	const float Mag = ASC->GetNumericAttribute(UMechaAttributeSet::GetAmmoMagazineAttribute());  // 현재 탄창
	const float Max = ASC->GetNumericAttribute(UMechaAttributeSet::GetMaxMagazineAttribute());   // 최대 탄창
	const float Res = ASC->GetNumericAttribute(UMechaAttributeSet::GetAmmoReserveAttribute());   // 예비탄

	// 이미 탄창이 가득 찼거나 예비탄이 없으면 장전 불필요
	if (Mag >= Max || Res <= 0.f)
	{
		K2_EndAbility();
		return;
	}

	// ========== 장전 상태 태그 적용 ==========
	ASC->AddLooseGameplayTag(Tag_StateReloading);  // 장전 중 상태
	ASC->AddLooseGameplayTag(Tag_BlockFire);       // 발사 차단

	// ========== 장전 애니메이션 재생 ==========
	if (ReloadMontage && ActorInfo && ActorInfo->AvatarActor.IsValid())
	{
		auto* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this, NAME_None, ReloadMontage, 1.0f, NAME_None, false, 1.0f, 0.0f, true);
		MontageTask->ReadyForActivation();
	}

	// ========== 장전 시간만큼 대기 ==========
	auto* WaitTask = UAbilityTask_WaitDelay::WaitDelay(this, ReloadTime);
	WaitTask->OnFinish.AddDynamic(this, &UGA_Reload::OnReloadDelayFinished);
	WaitTask->ReadyForActivation();
}

// ========================================
// 장전 완료 콜백
// ========================================
void UGA_Reload::OnReloadDelayFinished()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC)
	{
		K2_EndAbility();
		return;
	}

	// ========== 탄약 이전 ==========
	{
		const float Mag = ASC->GetNumericAttribute(UMechaAttributeSet::GetAmmoMagazineAttribute());
		const float Max = ASC->GetNumericAttribute(UMechaAttributeSet::GetMaxMagazineAttribute());
		const float Res = ASC->GetNumericAttribute(UMechaAttributeSet::GetAmmoReserveAttribute());

		// 필요한 탄약 계산
		const float Need = FMath::Max(0.f, Max - Mag);
		
		// 예비탄에서 가져올 수 있는 만큼만
		const float Take = FMath::Clamp(Need, 0.f, Res);

		if (Take > 0.f)
		{
			// 탄창 증가
			ASC->ApplyModToAttribute(UMechaAttributeSet::GetAmmoMagazineAttribute(), EGameplayModOp::Additive, +Take);
			
			// 예비탄 감소
			ASC->ApplyModToAttribute(UMechaAttributeSet::GetAmmoReserveAttribute(),  EGameplayModOp::Additive, -Take);
		}
	}

	// ========== 장전 상태 태그 제거 ==========
	ASC->RemoveLooseGameplayTag(Tag_StateReloading);
	ASC->RemoveLooseGameplayTag(Tag_BlockFire);

	K2_EndAbility();
}

// ========================================
// 능력 종료 - 정리 작업
// ========================================
void UGA_Reload::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	// 안전을 위해 태그 제거 (중복 제거해도 문제없음)
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->RemoveLooseGameplayTag(Tag_StateReloading);
		ASC->RemoveLooseGameplayTag(Tag_BlockFire);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
