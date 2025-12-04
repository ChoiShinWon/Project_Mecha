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
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/PlayerController.h"

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

	// 카메라 효과 적용
	ApplyCameraEffects();

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

	// ========== 카메라 즉시 복원 시작 (Super::EndAbility 이후!) ==========
	if (OwnerCharacter)
	{
		// 복원 모드 활성화
		bIsRestoring = true;
		TargetFOV = OriginalFOV;
		TargetCameraDistance = OriginalCameraDistance;

		// 복원 함수 호출
		RestoreCameraEffects();
	}
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

// ========================================
// 카메라 효과 적용
// ========================================
void UGA_Hover::ApplyCameraEffects()
{
	if (!OwnerCharacter) return;

	// ========== 현재 카메라 상태 저장 ==========
	bIsRestoring = false;  // 호버 시작 (복원 아님)

	UCameraComponent* Camera = OwnerCharacter->FindComponentByClass<UCameraComponent>();
	if (Camera)
	{
		OriginalFOV = Camera->FieldOfView;
		CurrentFOV = OriginalFOV;
		TargetFOV = bEnableFOVChange ? HoverFOV : OriginalFOV;
	}

	USpringArmComponent* SpringArm = OwnerCharacter->FindComponentByClass<USpringArmComponent>();
	if (SpringArm)
	{
		OriginalCameraDistance = SpringArm->TargetArmLength;
		CurrentCameraDistance = OriginalCameraDistance;
		TargetCameraDistance = bEnableCameraDistance ? HoverCameraDistance : OriginalCameraDistance;
	}

	// ========== 부드러운 전환 타이머 시작 ==========
	if (UWorld* World = OwnerCharacter->GetWorld())
	{
		World->GetTimerManager().SetTimer(
			CameraUpdateTimer,
			this,
			&UGA_Hover::UpdateCameraSmooth,
			0.016f,  // ~60fps
			true     // 반복
		);
	}

	// ========== 카메라 쉐이크 ==========
	if (HoverCameraShake)
	{
		if (APlayerController* PC = Cast<APlayerController>(OwnerCharacter->GetController()))
		{
			PC->PlayerCameraManager->StartCameraShake(HoverCameraShake);
		}
	}
}

// ========================================
// 카메라 효과 복원
// ========================================
void UGA_Hover::RestoreCameraEffects()
{
	if (!OwnerCharacter) return;

	// ========== 즉시 복원 시작 ==========
	bIsRestoring = true;  // 복원 모드 활성화
	TargetFOV = OriginalFOV;
	TargetCameraDistance = OriginalCameraDistance;

	// 즉시 한번 업데이트 (복원 즉시 시작!)
	UpdateCameraSmooth();

	if (UWorld* World = OwnerCharacter->GetWorld())
	{
		// 기존 타이머 정리 후 즉시 재시작
		World->GetTimerManager().ClearTimer(CameraUpdateTimer);

		World->GetTimerManager().SetTimer(
			CameraUpdateTimer,
			this,
			&UGA_Hover::UpdateCameraSmooth,
			0.016f,  // ~60fps
			true     // 반복
		);
	}

	if (UWorld* World = OwnerCharacter->GetWorld())
	{
		// 기존 Cleanup 타이머 정리
		World->GetTimerManager().ClearTimer(CleanupTimer);

		// Failsafe: 10초 후에도 복원이 안 되면 강제 정리 (보통은 UpdateCameraSmooth에서 자동 정리됨)
		World->GetTimerManager().SetTimer(
			CleanupTimer,
			[this]()
			{
				if (UWorld* World = OwnerCharacter ? OwnerCharacter->GetWorld() : nullptr)
				{
					// 최종 카메라 상태 확인 및 강제 설정
					UCameraComponent* Camera = OwnerCharacter->FindComponentByClass<UCameraComponent>();
					if (Camera && bEnableFOVChange)
					{
						// 정확하게 설정
						Camera->SetFieldOfView(OriginalFOV);
					}

					USpringArmComponent* SpringArm = OwnerCharacter->FindComponentByClass<USpringArmComponent>();
					if (SpringArm && bEnableCameraDistance)
					{
						SpringArm->TargetArmLength = OriginalCameraDistance;
					}

					World->GetTimerManager().ClearTimer(CameraUpdateTimer);
				}
			},
			10.0f,  // 충분한 시간, 보통은 자동 완료됨
			false
		);
	}
}

// ========================================
// 카메라 부드러운 업데이트 (매 프레임 호출)
// ========================================
void UGA_Hover::UpdateCameraSmooth()
{
	if (!OwnerCharacter)
	{
		return;
	}

	const float DeltaTime = 0.016f;  // ~60fps

	// ========== FOV 부드러운 전환 ==========
	if (bEnableFOVChange)
	{
		UCameraComponent* Camera = OwnerCharacter->FindComponentByClass<UCameraComponent>();
		if (Camera)
		{
			// 현재 FOV 가져오기
			float OldFOV = Camera->FieldOfView;

			// 복원 중일 때는 더 느린 속도 사용
			float CurrentBlendSpeed = bIsRestoring ? FOVRestoreSpeed : FOVBlendSpeed;

			// 타겟 FOV로 부드럽게 보간
			float NewFOV = FMath::FInterpTo(OldFOV, TargetFOV, DeltaTime, CurrentBlendSpeed);
			Camera->SetFieldOfView(NewFOV);

			CurrentFOV = NewFOV;

			// ========== 복원 완료 체크 (목표에 거의 도달하면 타이머 정리) ==========
			if (bIsRestoring && FMath::Abs(NewFOV - TargetFOV) < 0.5f)
			{
				// 정확하게 설정
				Camera->SetFieldOfView(TargetFOV);

				// 거리도 체크
				bool bDistanceRestored = true;
				if (bEnableCameraDistance)
				{
					USpringArmComponent* SpringArm = OwnerCharacter->FindComponentByClass<USpringArmComponent>();
					if (SpringArm)
					{
						float CurrentDist = SpringArm->TargetArmLength;
						if (FMath::Abs(CurrentDist - TargetCameraDistance) < 5.0f)
						{
							SpringArm->TargetArmLength = TargetCameraDistance;
						}
						else
						{
							bDistanceRestored = false;
						}
					}
				}

				// 둘 다 복원 완료되면 타이머 정리
				if (bDistanceRestored)
				{
					if (UWorld* World = OwnerCharacter->GetWorld())
					{
						World->GetTimerManager().ClearTimer(CameraUpdateTimer);
						World->GetTimerManager().ClearTimer(CleanupTimer);  // Failsafe 타이머도 정리
					}
					bIsRestoring = false;
				}
			}
		}
	}

	// ========== 카메라 거리 부드러운 전환 ==========
	if (bEnableCameraDistance)
	{
		USpringArmComponent* SpringArm = OwnerCharacter->FindComponentByClass<USpringArmComponent>();
		if (SpringArm)
		{
			// 현재 거리 가져오기
			float OldDistance = SpringArm->TargetArmLength;

			// 복원 중일 때는 더 느린 속도 사용
			float CurrentBlendSpeed = bIsRestoring ? FOVRestoreSpeed : FOVBlendSpeed;

			// 타겟 거리로 부드럽게 보간
			float NewDistance = FMath::FInterpTo(OldDistance, TargetCameraDistance, DeltaTime, CurrentBlendSpeed);
			SpringArm->TargetArmLength = NewDistance;

			CurrentCameraDistance = NewDistance;
		}
	}
}
