// GA_Attack.cpp
// 근접 공격 능력 - 구체 스윕으로 적 탐지, GAS 데미지 적용

#include "GA_Attack.h"
#include "MechaCharacterBase.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "GameplayEffectExtension.h"

#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "GameFramework/Controller.h"

// ========================================
// 생성자
// ========================================
UGA_Attack::UGA_Attack()
{
	// 액터마다 하나의 인스턴스만 생성
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	
	// 클라이언트에서 예측 실행, 서버에서 확정
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	
	// 공격 능력 태그 추가
	AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Ability.Attack")));
}

// ========================================
// 능력 활성화 - 공격 시작
// ========================================
void UGA_Attack::ActivateAbility(
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

	// 오너 캐릭터 획득
	AMechaCharacterBase* Mecha = Cast<AMechaCharacterBase>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr);
	if (!Mecha)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 블루프린트 이벤트 호출 (몽타주 시작, 애님 노티파이 등)
	OnAttackTriggered(Mecha);
}

// ========================================
// 능력 종료 - 정리 작업
// ========================================
void UGA_Attack::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);

	// 필요 시 태그 정리 등
	if (const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
	{
		// 추가 정리 작업
	}
}

// ========================================
// 공격 판정 실행 (애님 노티파이에서 호출)
// ========================================
void UGA_Attack::PerformAttackTrace(AActor* SourceActor)
{
	if (!SourceActor) return;
	UWorld* World = SourceActor->GetWorld();
	if (!World) return;

	// ========== 구체 스윕 트레이스 ==========
	const FVector Start = SourceActor->GetActorLocation() + FVector(0, 0, 50);  // 캐릭터 중심에서 약간 위
	const FVector End = Start + (SourceActor->GetActorForwardVector() * AttackRange);  // 전방으로

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GA_AttackTrace), false, SourceActor);
	
	// 구체 스윕으로 적 탐지
	const bool bHit = World->SweepSingleByChannel(
		Hit, Start, End, FQuat::Identity, ECC_Pawn,
		FCollisionShape::MakeSphere(AttackRadius), 
		Params
	);

	// 히트 실패
	if (!bHit || !Hit.GetActor())
	{
		UE_LOG(LogTemp, Verbose, TEXT("[GA_Attack] No hit"));
		return;
	}

	AActor* HitActor = Hit.GetActor();

	// ========== 데미지 적용 ==========
	ApplyDamage_GASOrEngine(SourceActor, HitActor, Hit.ImpactPoint);

	// ========== 히트 피드백 ==========
	// 히트 이펙트
	if (HitEffect) 
		UGameplayStatics::SpawnEmitterAtLocation(World, HitEffect, Hit.ImpactPoint);
	
	// 히트 사운드
	if (HitSound)  
		UGameplayStatics::PlaySoundAtLocation(World, HitSound, Hit.ImpactPoint);
}

// ========================================
// 데미지 적용 (GAS 우선, 폴백으로 엔진 데미지)
// ========================================
void UGA_Attack::ApplyDamage_GASOrEngine(AActor* SourceActor, AActor* HitActor, const FVector& HitPoint)
{
	if (!SourceActor || !HitActor) return;

	// ========== 1. GAS 데미지 적용 시도 ==========
	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor);
	UAbilitySystemComponent* SourceASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(SourceActor);

	// 대상이 ASC를 가지고 있고, GE가 설정되어 있으면 GAS 데미지 적용
	if (TargetASC && GE_MeleeDamage)
	{
		// GE 컨텍스트 생성
		FGameplayEffectContextHandle Ctx = SourceASC ? SourceASC->MakeEffectContext() : FGameplayEffectContextHandle();
		Ctx.AddSourceObject(SourceActor);

		// GE Spec 생성
		FGameplayEffectSpecHandle SpecHandle = (SourceASC ? SourceASC : TargetASC)->MakeOutgoingSpec(GE_MeleeDamage, 1.f, Ctx);
		
		if (SpecHandle.IsValid())
		{
			// SetByCaller로 데미지 값 전달
			SpecHandle.Data->SetSetByCallerMagnitude(
				FGameplayTag::RequestGameplayTag(*SetByCallerDamageName.ToString()), 
				AttackDamage
			);

			// 대상에게 GE 적용
			TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
			
			UE_LOG(LogTemp, Log, TEXT("[GA_Attack] Applied GAS Damage %.1f to %s"), 
				AttackDamage, *HitActor->GetName());
			return;
		}
	}

	// ========== 2. 폴백: 엔진 기본 데미지 ==========
	// ASC가 없거나 GE가 설정되지 않은 경우
	UGameplayStatics::ApplyDamage(
		HitActor, 
		AttackDamage,
		SourceActor->GetInstigatorController(),
		SourceActor, 
		nullptr  // DamageTypeClass
	);

	UE_LOG(LogTemp, Log, TEXT("[GA_Attack] Fallback ApplyDamage %.1f to %s"), 
		AttackDamage, *HitActor->GetName());
}
