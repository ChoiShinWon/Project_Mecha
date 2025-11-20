// GA_Hover.cpp
// 설명:
// - UGA_Hover 클래스의 구현부.
// - 호버링 시작/중지, 에너지 소모, 과열 처리 로직.
//
// Description:
// - Implementation of UGA_Hover class.
// - Hover start/stop, energy drain, overheating logic.

#include "GA_Hover.h"
#include "AbilitySystemComponent.h"
#include "MechaAttributeSet.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

// 생성자: Instancing Policy 설정.
// Constructor: Sets Instancing Policy.
UGA_Hover::UGA_Hover()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	/*SetCanBeCanceled(true);

	Tag_AbilityHover = FGameplayTag::RequestGameplayTag(TEXT("Ability.Hover"));
	Tag_StateHovering = FGameplayTag::RequestGameplayTag(TEXT("State.Hovering"));
	Tag_StateOverheat = FGameplayTag::RequestGameplayTag(TEXT("State.Overheat"));
	Tag_BlockHover = FGameplayTag::RequestGameplayTag(TEXT("Block.Hover"));
	Tag_CooldownHover = FGameplayTag::RequestGameplayTag(TEXT("Cooldown.Hover"));

	ActivationBlockedTags.AddTag(Tag_BlockHover);
	ActivationBlockedTags.AddTag(Tag_StateOverheat);
	ActivationBlockedTags.AddTag(Tag_CooldownHover);
	AbilityTags.AddTag(Tag_AbilityHover);*/
	// GameplayCue 실행용 태그 추가

}

// 에너지가 충분한지 확인.
// - ASC에서 Energy Attribute를 가져와 임계값과 비교.
//
// Checks if energy is sufficient.
// - Gets Energy Attribute from ASC and compares with threshold.
bool UGA_Hover::IsEnergySufficient(float Threshold) const
{
	if (!ASC) return false;
	return ASC->GetNumericAttribute(UMechaAttributeSet::GetEnergyAttribute()) > Threshold;
}



// Ability 활성화 로직: 에너지 체크, 호버 시작, 에너지 소모 GE 적용.
// - CommitAbility로 코스트/쿨다운 체크.
// - 에너지가 충분하지 않으면 능력 종료.
// - 에너지 변화 델리게이트 바인딩: 에너지가 0이 되면 자동 종료 및 과열 적용.
// - StartHover 호출하여 Flying 모드 전환.
// - ApplyDrainGE 호출하여 에너지 소모 시작.
//
// Ability activation logic: Checks energy, starts hover, applies energy drain GE.
// - Checks cost/cooldown via CommitAbility.
// - Ends ability if energy is insufficient.
// - Binds energy change delegate: Auto-ends and applies overheating when energy reaches 0.
// - Calls StartHover to switch to Flying mode.
// - Calls ApplyDrainGE to start energy drain.
void UGA_Hover::ActivateAbility(
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

	OwnerCharacter = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
	ASC = GetAbilitySystemComponentFromActorInfo();
	if (!OwnerCharacter || !ASC)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!IsEnergySufficient(MinEnergyToStart))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (ASC)
	{
		EnergyChangedHandle =
			ASC->GetGameplayAttributeValueChangeDelegate(UMechaAttributeSet::GetEnergyAttribute())
			.AddLambda([WeakThis = TWeakObjectPtr<UGA_Hover>(this)](const FOnAttributeChangeData& Data)
				{
					if (UGA_Hover* Self = WeakThis.Get())
					{
						if (Data.NewValue <= KINDA_SMALL_NUMBER && Self->IsActive())
						{
							Self->RemoveDrainGE();
							Self->StopHover(true);
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

	StartHover();
	ApplyDrainGE();
}

void UGA_Hover::InputReleased(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayAbilityActivationInfo ActivationInfo)
{
	if (!IsActive()) return;

	RemoveDrainGE();
	StopHover(false);
	EndAbility(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), GetCurrentActivationInfo(),
		true, false);
}

void UGA_Hover::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (ASC)
	{
		if (EnergyChangedHandle.IsValid())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(UMechaAttributeSet::GetEnergyAttribute())
				.Remove(EnergyChangedHandle);
			EnergyChangedHandle.Reset();
		}

		ASC->RemoveLooseGameplayTag(Tag_StateHovering);
	}
	RemoveDrainGE();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}


// 호버 시작: Flying 모드 전환, 상승 임펄스 적용.
// - 현재 이동 설정을 저장 (종료 시 복원용).
// - State.Hovering 태그 추가.
// - BrakingFrictionFactor, GroundFriction을 0으로 설정 (미끄러운 움직임).
// - Flying 모드로 전환하고, 지상에 있으면 상승 임펄스 적용.
// - 주기적으로 상승 보정 함수 호출 (ApplyHoverLift).
void UGA_Hover::StartHover()
{
	if (!OwnerCharacter) return;
	auto* Move = OwnerCharacter->GetCharacterMovement();
	if (!Move) return;

	//  캐릭터 플래그 세팅
	if (AMechaCharacterBase* Mecha = Cast<AMechaCharacterBase>(OwnerCharacter))
	{
		Mecha->SetHovering(true);
	}

	SavedGravityScale = Move->GravityScale;
	SavedBrakingFrictionFactor = Move->BrakingFrictionFactor;
	SavedGroundFriction = Move->GroundFriction;

	if (ASC) ASC->AddLooseGameplayTag(Tag_StateHovering);

	Move->BrakingFrictionFactor = 0.f;
	Move->GroundFriction = 0.f;

	const bool bOnGround = Move->IsMovingOnGround();

	if (bUseFlyingMode)
	{
		Move->SetMovementMode(MOVE_Flying);
		if (bOnGround)
		{
			OwnerCharacter->LaunchCharacter(FVector(0, 0, HoverLiftImpulse), false, false);
		}
		else if (Move->Velocity.Z < 80.f)
		{
			Move->Velocity.Z = 80.f;
		}
	}
	else
	{
		if (bOnGround)
		{
			Move->SetMovementMode(MOVE_Falling);
			OwnerCharacter->LaunchCharacter(FVector(0, 0, HoverLiftImpulse), false, false);
		}
		Move->GravityScale = GravityScaleWhileHover;
	}

	if (UWorld* World = OwnerCharacter->GetWorld())
	{
		World->GetTimerManager().SetTimer(
			HoverAscendTimer,
			this,
			&UGA_Hover::ApplyHoverLift,
			0.05f,
			true
		);
	}
}

void UGA_Hover::StopHover(bool bFromEnergyDepleted)
{
	auto* Move = OwnerCharacter ? OwnerCharacter->GetCharacterMovement() : nullptr;
	if (Move)
	{
		if (bUseFlyingMode)
		{
			Move->SetMovementMode(MOVE_Falling);
			Move->GravityScale = ExitFallGravityScale;
		}
		else
		{
			Move->GravityScale = SavedGravityScale;
		}
		Move->BrakingFrictionFactor = SavedBrakingFrictionFactor;
		Move->GroundFriction = SavedGroundFriction;
	}

	// 캐릭터 플래그 해제
	if (AMechaCharacterBase* Mecha = Cast<AMechaCharacterBase>(OwnerCharacter))
	{
		Mecha->SetHovering(false);
	}

	if (ASC) ASC->RemoveLooseGameplayTag(Tag_StateHovering);
	if (bFromEnergyDepleted) ApplyOverheat();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HoverAscendTimer);
	}
}


void UGA_Hover::ApplyDrainGE()
{
	if (!ASC || !GE_Hover_EnergyDrain) return;
	FGameplayEffectContextHandle Ctx = ASC->MakeEffectContext();
	FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(GE_Hover_EnergyDrain, GetAbilityLevel(), Ctx);
	if (Spec.IsValid())
		DrainGEHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
}

void UGA_Hover::RemoveDrainGE()
{
	if (ASC && DrainGEHandle.IsValid())
	{
		ASC->RemoveActiveGameplayEffect(DrainGEHandle);
		DrainGEHandle.Invalidate();
	}
}

void UGA_Hover::ApplyOverheat()
{
	if (!ASC) return;

	ASC->AddLooseGameplayTag(Tag_StateOverheat);
	ASC->AddLooseGameplayTag(Tag_CooldownHover);
	ASC->AddLooseGameplayTag(Tag_BlockHover);

	TWeakObjectPtr<UAbilitySystemComponent> WeakASC = ASC;

	if (UWorld* World = GetWorld())
	{
		FTimerHandle Handle;
		World->GetTimerManager().SetTimer(
			Handle,
			FTimerDelegate::CreateLambda([WeakASC,
				Tag_Overheat = Tag_StateOverheat,
				Tag_Cooldown = Tag_CooldownHover,
				Tag_Block = Tag_BlockHover]()
				{
					if (UAbilitySystemComponent* ASC_Local = WeakASC.Get())
					{
						ASC_Local->RemoveLooseGameplayTag(Tag_Overheat);
						ASC_Local->RemoveLooseGameplayTag(Tag_Cooldown);
						ASC_Local->RemoveLooseGameplayTag(Tag_Block);
					}
				}),
			CooldownSeconds, false);
	}
}


// 호버 상승 보정 함수 (타이머에서 주기적 호출).
// - 에너지가 충분할 때만 상승 보정 적용.
// - Flying 모드일 때만 동작.
// - Z축 속도를 점진적으로 증가시켜 천천히 상승.
//
// Hover lift adjustment function (called periodically by timer).
// - Applies lift adjustment only when energy is sufficient.
// - Only works in Flying mode.
// - Gradually increases Z-axis velocity for slow ascent.
void UGA_Hover::ApplyHoverLift()
{
	if (!OwnerCharacter || !ASC) return;

	// 에너지가 충분할 때만 상승
	if (!IsEnergySufficient(1.f))
		return;

	UCharacterMovementComponent* Move = OwnerCharacter->GetCharacterMovement();
	if (!Move || Move->MovementMode != MOVE_Flying)
		return;

	// 천천히 상승 (너무 빠르면 불안정해짐)
	FVector CurrentVel = Move->Velocity;
	float MaxUpSpeed = 400.f; // 상승 최대 속도
	float LiftForce = 200.f;  // 가속치

	if (CurrentVel.Z < MaxUpSpeed)
	{
		FVector NewVel = CurrentVel + FVector(0, 0, LiftForce * GetWorld()->GetDeltaSeconds());
		Move->Velocity = NewVel;
	}
}