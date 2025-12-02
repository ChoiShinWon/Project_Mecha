// GA_AssaultBoost.cpp
// 어설트 부스트 능력 - 전방 돌진, 에너지 소모, 과열 처리

#include "GA_AssaultBoost.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "Animation/AnimInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystemComponent.h"
#include "AbilitySystemComponent.h"
#include "MechaAttributeSet.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"

// ========================================
// 생성자
// ========================================
UGA_AssaultBoost::UGA_AssaultBoost()
{
	// 액터마다 하나의 인스턴스만 생성
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

// ========================================
// 에너지 충분 여부 확인
// ========================================
bool UGA_AssaultBoost::IsEnergySufficient(float Threshold) const
{
	if (!ASC) return false;
	return ASC->GetNumericAttribute(UMechaAttributeSet::GetEnergyAttribute()) > Threshold;
}

// ========================================
// 능력 활성화 - 돌진 시작
// ========================================
void UGA_AssaultBoost::ActivateAbility(
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
	OwnerChar = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
	ASC = GetAbilitySystemComponentFromActorInfo();

	if (!OwnerChar || !ASC)
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

	// ========== 마우스 룩 입력 차단 (돌진 중 시점 고정) ==========
	if (OwnerChar)
	{
		if (APlayerController* PC = Cast<APlayerController>(OwnerChar->GetController()))
		{
			// 현재 상태 저장 (종료 시 복원용)
			bPrevIgnoreLookInput = PC->IsLookInputIgnored();
			PC->SetIgnoreLookInput(true);
		}
	}

	// ========== 카메라 효과 적용 ==========
	ApplyCameraEffects();

	// ========== 전방 돌진 ==========
	if (UCharacterMovementComponent* Move = OwnerChar->GetCharacterMovement())
	{
		Move->SetMovementMode(MOVE_Flying);     // 공중 상태로 전환
		Move->StopMovementImmediately();        // 기존 속도 초기화
		Move->Velocity = OwnerChar->GetActorForwardVector() * BoostForce;  // 전방으로 가속
	}

	// ========== 돌진 애니메이션 재생 ==========
	if (BoostMontage)
	{
		if (UAnimInstance* Anim = OwnerChar->GetMesh()->GetAnimInstance())
		{
			Anim->Montage_Play(BoostMontage);
		}
	}

	// ========== 부스터 파티클 생성 ==========
	if (BoostParticle)
	{
		USkeletalMeshComponent* Mesh = OwnerChar->GetMesh();
		if (Mesh)
		{
			ActiveBoostFX.Empty();

			// 지정된 소켓들에 파티클 부착
			for (const FName& SocketName : BoostSockets)
			{
				if (Mesh->DoesSocketExist(SocketName))
				{
					// 발 소켓은 회전 조정
					FRotator Rot = (SocketName == "Foot_L" || SocketName == "Foot_R")
						? FRotator(270, 0, 180)
						: FRotator::ZeroRotator;

					UParticleSystemComponent* FX = UGameplayStatics::SpawnEmitterAttached(
						BoostParticle, Mesh, SocketName,
						FVector::ZeroVector, Rot,
						EAttachLocation::SnapToTarget, true
					);

					if (FX)
					{
						FX->SetWorldScale3D(FVector(5.0f));
						ActiveBoostFX.Add(FX);
					}
				}
			}
		}
	}

	// ========== 에너지 소모 시작 ==========
	ApplyDrainGE();

	// ========== 에너지 변화 감시 (에너지 고갈 시 자동 종료) ==========
	EnergyChangedHandle =
		ASC->GetGameplayAttributeValueChangeDelegate(UMechaAttributeSet::GetEnergyAttribute())
		.AddUObject(this, &UGA_AssaultBoost::OnEnergyChanged);

	// ========== 일정 시간 후 자동 종료 ==========
	if (UWorld* World = OwnerChar->GetWorld())
	{
		FTimerHandle EndHandle;
		World->GetTimerManager().SetTimer(
			EndHandle,
			[this, Handle, ActorInfo, ActivationInfo]()
			{
				if (IsActive())
					EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
			},
			BoostDuration, 
			false
		);
	}
}

// ========================================
// 에너지 소모 효과 적용
// ========================================
void UGA_AssaultBoost::ApplyDrainGE()
{
	if (!ASC || !GE_AssaultBoostDrain) return;

	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(GE_AssaultBoostDrain, GetAbilityLevel(), Context);

	if (Spec.IsValid())
	{
		DrainGEHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}
}

// ========================================
// 에너지 소모 효과 제거
// ========================================
void UGA_AssaultBoost::RemoveDrainGE()
{
	if (ASC && DrainGEHandle.IsValid())
	{
		ASC->RemoveActiveGameplayEffect(DrainGEHandle);
		DrainGEHandle.Invalidate();
	}
}

// ========================================
// 에너지 변화 콜백
// ========================================
void UGA_AssaultBoost::OnEnergyChanged(const FOnAttributeChangeData& Data)
{
	const float NewEnergy = Data.NewValue;

	// 에너지가 거의 0이 되면 과열 상태로 종료
	if (NewEnergy <= KINDA_SMALL_NUMBER && IsActive())
	{
		RemoveDrainGE();
		ApplyOverheat();
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

// ========================================
// 과열 상태 적용
// ========================================
void UGA_AssaultBoost::ApplyOverheat()
{
	if (!ASC) return;
	ASC->AddLooseGameplayTag(Tag_StateOverheat);
}

// ========================================
// 능력 종료 - 정리 작업
// ========================================
void UGA_AssaultBoost::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	// 에너지 소모 중단
	RemoveDrainGE();

	// ========== 에너지 변화 델리게이트 해제 ==========
	if (ASC && EnergyChangedHandle.IsValid())
	{
		ASC->GetGameplayAttributeValueChangeDelegate(UMechaAttributeSet::GetEnergyAttribute())
			.Remove(EnergyChangedHandle);
		EnergyChangedHandle.Reset();
	}

	// ========== 부스터 파티클 비활성화 ==========
	for (UParticleSystemComponent* FX : ActiveBoostFX)
	{
		if (FX)
			FX->DeactivateSystem();
	}
	ActiveBoostFX.Empty();

	// ========== 캐릭터 상태 복원 ==========
	if (OwnerChar)
	{
		// 마우스 룩 입력 원래 상태로 복구
		if (APlayerController* PC = Cast<APlayerController>(OwnerChar->GetController()))
		{
			PC->SetIgnoreLookInput(bPrevIgnoreLookInput);
		}

		// 애니메이션 정지
		if (UAnimInstance* Anim = OwnerChar->GetMesh()->GetAnimInstance())
		{
			Anim->Montage_Stop(0.2f);
		}

		// 이동 모드 및 중력 복원
		if (UCharacterMovementComponent* Move = OwnerChar->GetCharacterMovement())
		{
			Move->StopMovementImmediately();
			Move->SetMovementMode(MOVE_Falling);  // 낙하 상태로
			Move->GravityScale = 1.0f;            // 정상 중력
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);

	// ========== 카메라 즉시 복원 시작 (Super::EndAbility 이후!) ==========
	if (OwnerChar)
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
// 카메라 효과 적용
// ========================================
void UGA_AssaultBoost::ApplyCameraEffects()
{
	if (!OwnerChar) return;

	// ========== 현재 카메라 상태 저장 ==========
	bIsRestoring = false;  // 부스트 시작 (복원 아님)

	UCameraComponent* Camera = OwnerChar->FindComponentByClass<UCameraComponent>();
	if (Camera)
	{
		OriginalFOV = Camera->FieldOfView;
		CurrentFOV = OriginalFOV;
		TargetFOV = bEnableFOVChange ? BoostFOV : OriginalFOV;
	}

	USpringArmComponent* SpringArm = OwnerChar->FindComponentByClass<USpringArmComponent>();
	if (SpringArm)
	{
		OriginalCameraDistance = SpringArm->TargetArmLength;
		CurrentCameraDistance = OriginalCameraDistance;
		TargetCameraDistance = bEnableCameraDistance ? BoostCameraDistance : OriginalCameraDistance;
	}

	// ========== 부드러운 전환 타이머 시작 ==========
	if (UWorld* World = OwnerChar->GetWorld())
	{
		World->GetTimerManager().SetTimer(
			CameraUpdateTimer,
			this,
			&UGA_AssaultBoost::UpdateCameraSmooth,
			0.016f,  // ~60fps
			true     // 반복
		);
	}

	// ========== 카메라 쉐이크 ==========
	if (BoostCameraShake)
	{
		if (APlayerController* PC = Cast<APlayerController>(OwnerChar->GetController()))
		{
			PC->ClientStartCameraShake(BoostCameraShake);
		}
	}
}

// ========================================
// 카메라 효과 복원
// ========================================
void UGA_AssaultBoost::RestoreCameraEffects()
{
	if (!OwnerChar) return;

	// ========== 즉시 복원 시작 ==========
	bIsRestoring = true;  // 복원 모드 활성화
	TargetFOV = OriginalFOV;
	TargetCameraDistance = OriginalCameraDistance;

	// 즉시 한번 업데이트 (복원 즉시 시작!)
	UpdateCameraSmooth();

	if (UWorld* World = OwnerChar->GetWorld())
	{
		// 기존 타이머 정리 후 즉시 재시작
		World->GetTimerManager().ClearTimer(CameraUpdateTimer);

		World->GetTimerManager().SetTimer(
			CameraUpdateTimer,
			this,
			&UGA_AssaultBoost::UpdateCameraSmooth,
			0.016f,  // ~60fps
			true     // 반복
		);
	}

	if (UWorld* World = OwnerChar->GetWorld())
	{
		// 기존 Cleanup 타이머 정리
		World->GetTimerManager().ClearTimer(CleanupTimer);

		// Failsafe: 10초 후에도 복원이 안 되면 강제 정리 (보통은 UpdateCameraSmooth에서 자동 정리됨)
		World->GetTimerManager().SetTimer(
			CleanupTimer,
			[this]()
			{
				if (UWorld* World = OwnerChar ? OwnerChar->GetWorld() : nullptr)
				{
					// 최종 카메라 상태 확인 및 강제 설정
					UCameraComponent* Camera = OwnerChar->FindComponentByClass<UCameraComponent>();
					if (Camera && bEnableFOVChange)
					{
						// 정확하게 설정
						Camera->SetFieldOfView(OriginalFOV);
					}

					USpringArmComponent* SpringArm = OwnerChar->FindComponentByClass<USpringArmComponent>();
					if (SpringArm && bEnableCameraDistance)
					{
						SpringArm->TargetArmLength = OriginalCameraDistance;
					}

					World->GetTimerManager().ClearTimer(CameraUpdateTimer);
				}
			},
			10.0f,  // 2초 → 10초 (충분한 시간, 보통은 자동 완료됨)
			false
		);
	}
}

// ========================================
// 카메라 부드러운 업데이트 (매 프레임 호출)
// ========================================
void UGA_AssaultBoost::UpdateCameraSmooth()
{
	if (!OwnerChar)
	{
		return;
	}

	const float DeltaTime = 0.016f;  // ~60fps

	// ========== FOV 부드러운 전환 ==========
	if (bEnableFOVChange)
	{
		UCameraComponent* Camera = OwnerChar->FindComponentByClass<UCameraComponent>();
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
					USpringArmComponent* SpringArm = OwnerChar->FindComponentByClass<USpringArmComponent>();
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
					if (UWorld* World = OwnerChar->GetWorld())
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
		USpringArmComponent* SpringArm = OwnerChar->FindComponentByClass<USpringArmComponent>();
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
