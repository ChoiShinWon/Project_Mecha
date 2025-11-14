// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_MissileFire_Enemy.generated.h"

/**
 * Enemy 전용 미사일 발사 Ability
 *  - EnemyMecha의 CurrentTarget을 향해 BP_EnemyMissile 발사
 */
UCLASS()
class PROJECT_MECHA_API UGA_MissileFire_Enemy : public UGameplayAbility
{
	GENERATED_BODY()
	
public:
	UGA_MissileFire_Enemy();

	// 스폰할 미사일 클래스
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Missile")
	TSubclassOf<AActor> MissileClass;

	// 총구 소켓 이름
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Missile")
	FName MuzzleSocketName;

	// 데미지 값 (참고용)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Missile")
	float MissileDamage;

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
};
