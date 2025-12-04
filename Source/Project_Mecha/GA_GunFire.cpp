// GA_GunFire.cpp
// 총 발사 능력 - 탄창 관리, 크로스헤어 조준, 투사체 발사

#include "GA_GunFire.h"

#include "MechaAttributeSet.h"
#include "MechaCharacterBase.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"

#include "GameFramework/Character.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Animation/AnimInstance.h"

// ========================================
// 생성자
// ========================================
UGA_GunFire::UGA_GunFire()
{
	// 액터마다 하나의 인스턴스만 생성
	InstancingPolicy   = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	
	// 클라이언트에서 예측 실행, 서버에서 확정
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

// ========================================
// 능력 활성화 - 총 발사
// ========================================
void UGA_GunFire::ActivateAbility(
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

	// ========== 발사 가능 여부 체크 ==========
	// 장전 중이거나 발사 차단 태그가 있으면 발사 불가
	if (ASC->HasMatchingGameplayTag(Tag_StateReloading) || ASC->HasMatchingGameplayTag(Tag_BlockFire))
	{
		K2_EndAbility();
		return;
	}

	// ========== 탄창 확인 ==========
	const float Mag = ASC->GetNumericAttribute(UMechaAttributeSet::GetAmmoMagazineAttribute());
	if (Mag <= 0.f)
	{
		// 탄약이 없으면 발사 불가
		K2_EndAbility();
		return;
	}

	// ========== 발사 애니메이션 재생 ==========
	if (FireMontage && ActorInfo && ActorInfo->AvatarActor.IsValid())
	{
		auto* PlayTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this, NAME_None, FireMontage, 1.0f, NAME_None, false, 1.0f);
		PlayTask->ReadyForActivation();
	}

	// ========== 투사체 스폰 ==========
	SpawnProjectile(ActorInfo);

	// ========== 탄약 소모 (서버에서만) ==========
	if (GetAvatarActorFromActorInfo()->HasAuthority())
	{
		// 탄창 1 감소
		ASC->ApplyModToAttribute(UMechaAttributeSet::GetAmmoMagazineAttribute(), EGameplayModOp::Additive, -1.f);
	}

	// ========== 연사 간격 후 능력 종료 ==========
	const float FireInterval = 0.3f;
	auto* DelayTask = UAbilityTask_WaitDelay::WaitDelay(this, FireInterval);
	DelayTask->OnFinish.AddDynamic(this, &UGA_GunFire::K2_EndAbility);
	DelayTask->ReadyForActivation();
}

// ========================================
// 애니메이션 노티파이 콜백
// ========================================
void UGA_GunFire::OnMontageNotify()
{
	// 애니메이션 특정 프레임에서 발사하고 싶을 때 사용
	const FGameplayAbilityActorInfo* Info = GetCurrentActorInfo();
	if (Info) 
	{ 
		SpawnProjectile(Info); 
	}
}

// ========================================
// 투사체 스폰 및 발사
// ========================================
void UGA_GunFire::SpawnProjectile(const FGameplayAbilityActorInfo* ActorInfo)
{
	if (!ActorInfo || !ActorInfo->AvatarActor.IsValid()) return;

	AMechaCharacterBase* Mecha = Cast<AMechaCharacterBase>(ActorInfo->AvatarActor.Get());
	if (!Mecha || !Mecha->ProjectileClass) return;

	FVector SpawnLoc = FVector::ZeroVector;
	FRotator SpawnRot = FRotator::ZeroRotator;

	// ========== 1. 총구 위치 계산 ==========
	if (Mecha->MuzzleLocation)
	{
		// MuzzleLocation 컴포넌트가 있으면 사용
		SpawnLoc = Mecha->MuzzleLocation->GetComponentLocation();
	}
	else if (Mecha->GetMesh() && Mecha->GetMesh()->DoesSocketExist(Mecha->MuzzleSocketName))
	{
		// 소켓이 있으면 소켓 위치 사용
		SpawnLoc = Mecha->GetMesh()->GetSocketLocation(Mecha->MuzzleSocketName);
	}
	else
	{
		// 둘 다 없으면 캐릭터 앞쪽
		SpawnLoc = Mecha->GetActorLocation() + Mecha->GetActorForwardVector() * FallbackSpawnOffset;
	}

	// ========== 2. 화면 중앙 크로스헤어로 조준점 계산 ==========
	FVector TargetPoint = SpawnLoc + Mecha->GetActorForwardVector() * 10000.f;
	FVector LaunchDir   = Mecha->GetActorForwardVector();

	if (APlayerController* PC = Cast<APlayerController>(Mecha->GetController()))
	{
		int32 ViewX, ViewY;
		PC->GetViewportSize(ViewX, ViewY);

		// 화면 중앙 좌표
		const float ScreenX = ViewX / 2.0f;
		const float ScreenY = ViewY / 2.0f;

		FVector WorldLoc, WorldDir;
		if (PC->DeprojectScreenPositionToWorld(ScreenX, ScreenY, WorldLoc, WorldDir))
		{
			const FVector TraceStart = WorldLoc;
			const FVector TraceEnd   = WorldLoc + WorldDir * 10000.f;

			// 라인 트레이스로 조준점 찾기
			FHitResult Hit;
			FCollisionQueryParams Params;
			Params.AddIgnoredActor(Mecha);

			if (Mecha->GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, Params))
			{
				// 무언가 맞으면 그 지점
				TargetPoint = Hit.ImpactPoint;
			}
			else
			{
				// 안 맞으면 트레이스 끝점
				TargetPoint = TraceEnd;
			}

			// 총구에서 조준점으로 방향 계산
			LaunchDir = (TargetPoint - SpawnLoc).GetSafeNormal();
		}
	}
	SpawnRot = LaunchDir.Rotation();

	// ========== 3. 총구 섬광 이펙트 ==========
	if (MuzzleFlash)
	{
		UGameplayStatics::SpawnEmitterAtLocation(Mecha->GetWorld(), MuzzleFlash, SpawnLoc, SpawnRot);
	}

	// ========== 4. 투사체 스폰 (서버에서만) ==========
	if (Mecha->HasAuthority())
	{
		FActorSpawnParameters Params;
		Params.Owner = Mecha;
		Params.Instigator = Mecha;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AActor* Projectile = Mecha->GetWorld()->SpawnActor<AActor>(Mecha->ProjectileClass, SpawnLoc, SpawnRot, Params);
		if (Projectile)
		{
			// 네트워크 복제 설정
			Projectile->SetReplicates(true);
			Projectile->SetReplicateMovement(true);

			// 발사 속도 설정
			if (UProjectileMovementComponent* MoveComp = Projectile->FindComponentByClass<UProjectileMovementComponent>())
			{
				MoveComp->Velocity = LaunchDir * MoveComp->InitialSpeed;
				MoveComp->Activate();
			}
		}
	}
}

// ========================================
// 능력 종료 - 정리 작업
// ========================================
void UGA_GunFire::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	// 발사 애니메이션 정리 (필요시)
	if (ActorInfo && ActorInfo->AvatarActor.IsValid() && FireMontage)
	{
		if (ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get()))
		{
			if (UAnimInstance* AnimInstance = Character->GetMesh()->GetAnimInstance())
			{
				if (AnimInstance->Montage_IsPlaying(FireMontage))
				{
					AnimInstance->Montage_Stop(0.2f, FireMontage);
				}
			}
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
