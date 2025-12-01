// GA_Hover.cpp
// 호버링(공중 부유) 능력 - 에너지 소모, 과열 처리, Flying 모드 전환

#include "GA_Hover.h"
#include "AbilitySystemComponent.h"
#include "MechaAttributeSet.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

// ========================================
// 생성자
// ========================================
UGA_Hover::UGA_Hover()
{
	// 액터마다 하나의 인스턴스만 생성
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

// ========================================
// 에너지 충분 여부 확인
// ========================================
bool UGA_Hover::IsEnergySufficient(float Threshold) const
{
	if (!ASC) return false;
	return ASC->GetNumericAttribute(UMechaAttributeSet::GetEnergyAttribute()) > Threshold;
}

// ========================================
// 능력 활성화 - 호버링 시작
// ========================================
void UGA_Hover::ActivateAbility(
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

	// 오너 캐릭터 및 ASC 획득
	OwnerCharacter = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
	ASC = GetAbilitySystemComponentFromActorInfo();
	if (!OwnerCharacter || !ASC)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 에너지 부족하면 발동 실패
	if (!IsEnergySufficient(MinEnergyToStart))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// ========== 에너지 변화 감지 델리게이트 바인딩 ==========
	// 에너지가 0이 되면 자동으로 호버링 종료 및 과열 처리
	if (ASC)
	{
		EnergyChangedHandle =
			ASC->GetGameplayAttributeValueChangeDelegate(UMechaAttributeSet::GetEnergyAttribute())
			.AddLambda([WeakThis = TWeakObjectPtr<UGA_Hover>(this)](const FOnAttributeChangeData& Data)
				{
					if (UGA_Hover* Self = WeakThis.Get())
					{
						// 에너지가 거의 0이 되면
						if (Data.NewValue <= KINDA_SMALL_NUMBER && Self->IsActive())
						{
							Self->RemoveDrainGE();
							Self->StopHover(true);  // 과열 상태로 종료
							Self->EndAbility(
								Self->GetCurrentAbilitySpecHandle(),
								Self->GetCurrentActorInfo(),
								Self->GetCurrentActivationInfo(),
								true, false
							);
						}
					}
				});
	}

	// 호버링 시작 및 에너지 소모 시작
	StartHover();
	ApplyDrainGE();
}

// ========================================
// 입력 해제 - 호버링 수동 종료
// ========================================
void UGA_Hover::InputReleased(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayAbilityActivationInfo ActivationInfo)
{
	if (!IsActive()) return;

	// 에너지 소모 중단 및 호버링 종료
	RemoveDrainGE();
	StopHover(false);  // 정상 종료 (과열 아님)
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(),
		true, false);
}

// ========================================
// 능력 종료 - 정리 작업
// ========================================
void UGA_Hover::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	// 델리게이트 해제
	if (ASC)
	{
		if (EnergyChangedHandle.IsValid())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(UMechaAttributeSet::GetEnergyAttribute())
				.Remove(EnergyChangedHandle);
			EnergyChangedHandle.Reset();
		}

		// 호버링 상태 태그 제거
		ASC->RemoveLooseGameplayTag(Tag_StateHovering);
	}

	RemoveDrainGE();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// ========================================
// 호버링 시작 처리
// ========================================
void UGA_Hover::StartHover()
{
	if (!OwnerCharacter) return;
	auto* Move = OwnerCharacter->GetCharacterMovement();
	if (!Move) return;

	// 캐릭터에 호버링 상태 플래그 설정
	if (AMechaCharacterBase* Mecha = Cast<AMechaCharacterBase>(OwnerCharacter))
	{
		Mecha->SetHovering(true);
	}

	// ========== 현재 이동 설정 저장 (종료 시 복원용) ==========
	SavedGravityScale = Move->GravityScale;
	SavedBrakingFrictionFactor = Move->BrakingFrictionFactor;
	SavedGroundFriction = Move->GroundFriction;

	// 호버링 상태 태그 추가
	if (ASC) ASC->AddLooseGameplayTag(Tag_StateHovering);

	// ========== 이동 설정 조정 (미끄러운 공중 이동) ==========
	Move->BrakingFrictionFactor = 0.f;
	Move->GroundFriction = 0.f;

	const bool bOnGround = Move->IsMovingOnGround();

	// ========== Flying 모드 전환 ==========
	if (bUseFlyingMode)
	{
		Move->SetMovementMode(MOVE_Flying);
		
		// 지상에 있으면 상승 임펄스 적용
		if (bOnGround)
		{
			OwnerCharacter->LaunchCharacter(FVector(0, 0, HoverLiftImpulse), false, false);
		}
		// 공중에서 하강 중이면 속도 보정
		else if (Move->Velocity.Z < 80.f)
		{
			Move->Velocity.Z = 80.f;
		}
	}
	else
	{
		// Flying 모드 사용 안 하는 경우 (낙하 + 중력 조정)
		if (bOnGround)
		{
			Move->SetMovementMode(MOVE_Falling);
			OwnerCharacter->LaunchCharacter(FVector(0, 0, HoverLiftImpulse), false, false);
		}
		Move->GravityScale = GravityScaleWhileHover;
	}

	// ========== 주기적 상승 보정 타이머 시작 ==========
	if (UWorld* World = OwnerCharacter->GetWorld())
	{
		World->GetTimerManager().SetTimer(
			HoverAscendTimer,
			this,
			&UGA_Hover::ApplyHoverLift,
			0.03f,  // 0.03초마다 실행
			true    // 반복
		);
	}
}

// ========================================
// 호버링 종료 처리
// ========================================
void UGA_Hover::StopHover(bool bFromEnergyDepleted)
{
	auto* Move = OwnerCharacter ? OwnerCharacter->GetCharacterMovement() : nullptr;
	if (Move)
	{
		// ========== 이동 모드 복원 ==========
		if (bUseFlyingMode)
		{
			Move->SetMovementMode(MOVE_Falling);
			Move->GravityScale = ExitFallGravityScale;  // 낙하 시 중력
		}
		else
		{
			Move->GravityScale = SavedGravityScale;
		}

		// 저장된 이동 설정 복원
		Move->BrakingFrictionFactor = SavedBrakingFrictionFactor;
		Move->GroundFriction = SavedGroundFriction;
	}

	// 캐릭터 호버링 플래그 해제
	if (AMechaCharacterBase* Mecha = Cast<AMechaCharacterBase>(OwnerCharacter))
	{
		Mecha->SetHovering(false);
	}

	// 호버링 상태 태그 제거
	if (ASC) ASC->RemoveLooseGameplayTag(Tag_StateHovering);

	

	// 상승 보정 타이머 정리
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HoverAscendTimer);
	}
}

// ========================================
// 에너지 소모 효과 적용
// ========================================
void UGA_Hover::ApplyDrainGE()
{
	if (!ASC || !GE_Hover_EnergyDrain) return;
	
	// 에너지 소모 GE 적용
	FGameplayEffectContextHandle Ctx = ASC->MakeEffectContext();
	FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(GE_Hover_EnergyDrain, GetAbilityLevel(), Ctx);
	if (Spec.IsValid())
		DrainGEHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
}

// ========================================
// 에너지 소모 효과 제거
// ========================================
void UGA_Hover::RemoveDrainGE()
{
	if (ASC && DrainGEHandle.IsValid())
	{
		ASC->RemoveActiveGameplayEffect(DrainGEHandle);
		DrainGEHandle.Invalidate();
	}
}



// ========================================
// 호버링 상승 보정 (주기적 호출)
// ========================================
void UGA_Hover::ApplyHoverLift()
{
	if (!OwnerCharacter || !ASC) return;

	// 에너지가 부족하면 상승 보정 안 함
	if (!IsEnergySufficient(1.f))
		return;

	UCharacterMovementComponent* Move = OwnerCharacter->GetCharacterMovement();
	if (!Move || Move->MovementMode != MOVE_Flying)
		return;

	// ========== Z축 속도 점진적 증가 (천천히 상승) ==========
	FVector CurrentVel = Move->Velocity;
	float MaxUpSpeed = 800.f;  // 상승 최대 속도
	float LiftForce = 600.f;   // 가속치

	if (CurrentVel.Z < MaxUpSpeed)
	{
		FVector NewVel = CurrentVel + FVector(0, 0, LiftForce * GetWorld()->GetDeltaSeconds());
		Move->Velocity = NewVel;
	}
}
