// Fill out your copyright notice in the Description page of Project Settings.


#include "GA_MissileFire_Enemy.h"
#include "EnemyMecha.h"
#include "AbilitySystemComponent.h"
#include "GameplayTagContainer.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameFramework/Character.h"
#include "TimerManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"


// 생성자
UGA_MissileFire_Enemy::UGA_MissileFire_Enemy()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Ability.Missile.Enemy")));

	MuzzleSocketName = TEXT("Muzzle"); // 미사일 발사할 소켓 이름
	MissileDamage = 70.f; // 미사일 데미지
}

void UGA_MissileFire_Enemy::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	// 코스트 / 쿨타임 체크

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!ActorInfo || !ActorInfo->AvatarActor.IsValid() || !MissileClass)
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

	// 스폰 위치 / 회전 계산
	USkeletalMeshComponent* Mesh = Enemy->GetMesh();
	FVector SpawnLocation;
	FRotator SpawnRotation;

	if (Mesh && Mesh->DoesSocketExist(MuzzleSocketName))
	{
		SpawnLocation = Mesh->GetSocketLocation(MuzzleSocketName);

		const FVector ToTarget = Enemy->CurrentTarget->GetActorLocation() - SpawnLocation;
		SpawnRotation = ToTarget.Rotation();
	}
	else
	{
		SpawnLocation = Enemy->GetActorLocation() + Enemy->GetActorForwardVector() * 100.f;
		SpawnRotation = Enemy->GetActorRotation();
	}

	UWorld* World = Enemy->GetWorld();
	if (!World)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Enemy;
	SpawnParams.Instigator = Enemy;

	// 여기서 Projectile Spawn
	AActor* Missile = World->SpawnActor<AActor>(
		MissileClass,
		SpawnLocation,
		SpawnRotation,
		SpawnParams
	);

	if (Missile)
	{
		//ProjectileMovement 찾아서 유도 설정
		if (UProjectileMovementComponent* MoveComp =
			Missile->FindComponentByClass<UProjectileMovementComponent>())
		{
			MoveComp->bIsHomingProjectile = true;
			if (Enemy->CurrentTarget->GetRootComponent())
			{
				MoveComp->HomingTargetComponent = Enemy->CurrentTarget->GetRootComponent();
			}
		}


	}

	/*StartCooldown(ActorInfo);*/

	EndAbility(Handle, ActorInfo, ActivationInfo, false, false);
}


