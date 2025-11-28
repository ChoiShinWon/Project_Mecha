// GA_MissileFire_Enemy.cpp
// 적 메카 미사일 발사 능력 - 애니메이션 몽타주만 재생

#include "GA_MissileFire_Enemy.h"
#include "EnemyMecha.h"
#include "AbilitySystemComponent.h"
#include "GameplayTagContainer.h"

// ========================================
// 생성자
// ========================================
UGA_MissileFire_Enemy::UGA_MissileFire_Enemy()
{
	// 액터마다 하나의 인스턴스만 생성
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// Enemy 미사일 발사 태그 추가
	AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Ability.Missile.Enemy")));
}

// ========================================
// 능력 활성화 - 발사 몽타주 재생
// ========================================
void UGA_MissileFire_Enemy::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	// 코스트/쿨다운 체크
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 유효성 검사
	if (!ActorInfo || !ActorInfo->AvatarActor.IsValid())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Enemy 캐릭터 획득
	AEnemyMecha* Enemy = Cast<AEnemyMecha>(ActorInfo->AvatarActor.Get());
	if (!Enemy || !Enemy->CurrentTarget)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// ========== 발사 몽타주 재생 ==========
	// 실제 미사일 발사는 애님 노티파이에서 처리
	if (FireMontage)
	{
		Enemy->PlayAnimMontage(FireMontage);
	}

	// 능력 종료
	EndAbility(Handle, ActorInfo, ActivationInfo, false, false);
}
