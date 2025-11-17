#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_MissileFire_Enemy.generated.h"

/**
 * Enemy 전용 미사일 발사 Ability
 *  - 코스트 / 쿨타임 / 발사 결정 + 몽타주 재생만 담당
 *  - 실제 미사일 스폰은 AnimNotify에서 AEnemyMecha::FireMissileFromNotify()가 처리
 */
UCLASS()
class PROJECT_MECHA_API UGA_MissileFire_Enemy : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_MissileFire_Enemy();

	// 발사 몽타주
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Montage")
	UAnimMontage* FireMontage;

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
};
