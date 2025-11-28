// GA_Dash_Enemy.cpp
// 적 메카 대시 능력 - 타겟을 향해 빠르게 돌진

#include "GA_Dash_Enemy.h"
#include "EnemyMecha.h"

#include "AbilitySystemComponent.h"
#include "MechaAttributeSet.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"
#include "TimerManager.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimInstance.h"
#include "Engine/World.h"

// ========================================
// 생성자
// ========================================
UGA_Dash_Enemy::UGA_Dash_Enemy()
{
	// 액터마다 하나의 인스턴스만 생성
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	
	// 클라이언트에서 예측 실행, 서버에서 확정
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	// ========== 게임플레이 태그 초기화 ==========
	Tag_AbilityDash = FGameplayTag::RequestGameplayTag(TEXT("Ability.Dash.Enemy"));
	Tag_StateDashing = FGameplayTag::RequestGameplayTag(TEXT("State.Dashing"));
	Tag_CooldownDash = FGameplayTag::RequestGameplayTag(TEXT("Cooldown.Dash"));

	AbilityTags.AddTag(Tag_AbilityDash);
	ActivationOwnedTags.AddTag(Tag_StateDashing);
}

// ========================================
// Enemy 캐릭터 획득
// ========================================
ACharacter* UGA_Dash_Enemy::GetEnemyCharacter(const FGameplayAbilityActorInfo* ActorInfo) const
{
	return Cast<ACharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr);
}

// ========================================
// 대시 타겟 획득
// ========================================
AActor* UGA_Dash_Enemy::GetDashTarget(ACharacter* EnemyChar) const
{
	if (!EnemyChar)
		return nullptr;

	AEnemyMecha* EnemyMecha = Cast<AEnemyMecha>(EnemyChar);
	if (!EnemyMecha)
		return nullptr;

	// EnemyMecha의 CurrentTarget 사용
	return EnemyMecha->CurrentTarget;
}

// ========================================
// 능력 활성화 - 대시 실행
// ========================================
void UGA_Dash_Enemy::ActivateAbility(
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

	// Enemy 캐릭터 획득
	ACharacter* EnemyChar = GetEnemyCharacter(ActorInfo);
	if (!EnemyChar)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 타겟 확인
	AActor* TargetActor = GetDashTarget(EnemyChar);
	if (!TargetActor)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// ========== AI 이동 명령 정지 ==========
	if (AAIController* AICon = Cast<AAIController>(EnemyChar->GetController()))
	{
		AICon->StopMovement();
	}

	// ========== 이동 파라미터 저장 및 조정 (미끄러지게) ==========
	if (UCharacterMovementComponent* MoveComp = EnemyChar->GetCharacterMovement())
	{
		if (!bSavedMovementParams)
		{
			SavedBrakingFrictionFactor = MoveComp->BrakingFrictionFactor;
			SavedBrakingDecelWalking = MoveComp->BrakingDecelerationWalking;
			bSavedMovementParams = true;
		}

		// 브레이킹 제거 (미끄러지는 효과)
		MoveComp->BrakingFrictionFactor = 0.0f;
		MoveComp->BrakingDecelerationWalking = 0.0f;
	}

	// ========== 방향 및 거리 계산 ==========
	FVector ToTarget = TargetActor->GetActorLocation() - EnemyChar->GetActorLocation();
	ToTarget.Z = 0.0f;  // XY 평면만 사용

	float Distance = ToTarget.Size();
	if (Distance < KINDA_SMALL_NUMBER)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ToTarget.Normalize();

	// ========== 대시 거리 계산 ==========
	// 타겟까지 전체 거리에서 MinStopDistance만큼 빼고 이동
	float TravelDistance = FMath::Max(Distance - MinStopDistance, 0.0f);

	// 속도 배수 적용
	TravelDistance *= DashSpeedMultiplier;

	// 최대 거리 제한 (0이면 무제한)
	if (DashDistance > 0.0f)
	{
		TravelDistance = FMath::Min(TravelDistance, DashDistance);
	}

	if (TravelDistance <= 0.0f)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// ========== 대시 속도 계산 ==========
	// DashDuration 시간 동안 TravelDistance를 이동하도록
	const float Speed = (DashDuration > 0.f) ? (TravelDistance / DashDuration) : 0.f;
	if (Speed <= 0.0f)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const FVector LaunchVelocity = ToTarget * Speed;

	// ========== 캐릭터 발사 (대시) ==========
	EnemyChar->LaunchCharacter(LaunchVelocity, true, false);

	// ========== 대시 애니메이션 재생 ==========
	if (DashMontage && EnemyChar->GetMesh())
	{
		if (UAnimInstance* AnimInst = EnemyChar->GetMesh()->GetAnimInstance())
		{
			// 이미 재생 중이 아니면 시작
			if (!AnimInst->Montage_IsPlaying(DashMontage))
			{
				AnimInst->Montage_Play(DashMontage);
			}
		}
	}

	// ========== 대시 쿨타임 플래그 설정 ==========
	if (AEnemyMecha* EnemyMecha = Cast<AEnemyMecha>(EnemyChar))
	{
		EnemyMecha->bDashOnCooldown = true;
	}

	// ========== 쿨다운 GE 적용 ==========
	if (DashCooldownEffectClass && ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
	{
		FGameplayEffectContextHandle Context =
			ActorInfo->AbilitySystemComponent->MakeEffectContext();

		FGameplayEffectSpecHandle SpecHandle =
			ActorInfo->AbilitySystemComponent->MakeOutgoingSpec(
				DashCooldownEffectClass,
				GetAbilityLevel(Handle, ActorInfo),
				Context
			);

		if (SpecHandle.IsValid())
		{
			ActorInfo->AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(
				*SpecHandle.Data.Get()
			);
		}
	}

	// ========== 타이머로 대시 종료 예약 ==========
	if (UWorld* World = EnemyChar->GetWorld())
	{
		World->GetTimerManager().SetTimer(
			DashTimerHandle,
			this,
			&UGA_Dash_Enemy::StopDash,
			DashDuration,
			false
		);
	}
}

// ========================================
// 대시 정지 및 정리
// ========================================
void UGA_Dash_Enemy::StopDash()
{
	const FGameplayAbilityActorInfo* Info = GetCurrentActorInfo();
	ACharacter* EnemyChar = GetEnemyCharacter(Info);

	if (EnemyChar)
	{
		// ========== 이동 정지 ==========
		if (UCharacterMovementComponent* MoveComp = EnemyChar->GetCharacterMovement())
		{
			MoveComp->StopMovementImmediately();

			// 이동 파라미터 복원
			if (bSavedMovementParams)
			{
				MoveComp->BrakingFrictionFactor = SavedBrakingFrictionFactor;
				MoveComp->BrakingDecelerationWalking = SavedBrakingDecelWalking;
			}
		}

		// ========== 블랙보드 플래그 초기화 ==========
		if (AEnemyMecha* EnemyMecha = Cast<AEnemyMecha>(EnemyChar))
		{
			if (AAIController* AICon = Cast<AAIController>(EnemyMecha->GetController()))
			{
				if (UBlackboardComponent* BB = AICon->GetBlackboardComponent())
				{
					BB->SetValueAsBool(TEXT("IsDashing"), false);
					BB->SetValueAsBool(TEXT("ShouldDash"), false);
				}
			}
		}

		// ========== 애니메이션 정지 ==========
		if (DashMontage && EnemyChar->GetMesh())
		{
			if (UAnimInstance* AnimInst = EnemyChar->GetMesh()->GetAnimInstance())
			{
				if (AnimInst->Montage_IsPlaying(DashMontage))
				{
					AnimInst->Montage_Stop(0.2f, DashMontage);
				}
			}
		}
	}

	// 능력 종료
	FGameplayAbilitySpecHandle Handle = GetCurrentAbilitySpecHandle();
	FGameplayAbilityActivationInfo ActInfo = GetCurrentActivationInfo();

	EndAbility(Handle, Info, ActInfo, true, false);
}

// ========================================
// 능력 종료 - 정리 작업
// ========================================
void UGA_Dash_Enemy::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	// 타이머 정리
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DashTimerHandle);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
