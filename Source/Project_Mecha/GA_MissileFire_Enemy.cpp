#include "GA_MissileFire_Enemy.h"
#include "EnemyMecha.h"
#include "AbilitySystemComponent.h"
#include "GameplayTagContainer.h"

UGA_MissileFire_Enemy::UGA_MissileFire_Enemy()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// Enemy 미사일 발사용 Ability 태그
	AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Ability.Missile.Enemy")));
}

void UGA_MissileFire_Enemy::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!ActorInfo || !ActorInfo->AvatarActor.IsValid())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AEnemyMecha* Enemy = Cast<AEnemyMecha>(ActorInfo->AvatarActor.Get());
	if (!Enemy || !Enemy->CurrentTarget)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 몽타주 재생 (실제 발사는 AnimNotify에서 처리)
	if (FireMontage)
	{
		Enemy->PlayAnimMontage(FireMontage);
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, false, false);
}
