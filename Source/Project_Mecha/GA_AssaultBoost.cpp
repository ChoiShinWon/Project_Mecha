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

// 생성자: 인스턴싱 정책만 지정
UGA_AssaultBoost::UGA_AssaultBoost()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

bool UGA_AssaultBoost::IsEnergySufficient(float Threshold) const
{
    if (!ASC) return false;
    return ASC->GetNumericAttribute(UMechaAttributeSet::GetEnergyAttribute()) > Threshold;
}

// 능력 활성화
// - 에너지 충분 검증 → 이동모드/속도 설정 → 몽타주/파티클 → 드레인 GE/리스너 → 타이머로 종료
// - 방법 A: 부스트 중 마우스 룩 입력 차단(SetIgnoreLookInput)
void UGA_AssaultBoost::ActivateAbility(
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

    OwnerChar = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
    ASC = GetAbilitySystemComponentFromActorInfo();

    if (!OwnerChar || !ASC)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    if (!IsEnergySufficient(MinEnergyToStart))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // ✅ 방법 A: 부스트 동안 마우스 룩 입력 차단
    if (OwnerChar)
    {
        if (APlayerController* PC = Cast<APlayerController>(OwnerChar->GetController()))
        {
            bPrevIgnoreLookInput = PC->IsLookInputIgnored();
            PC->SetIgnoreLookInput(true);
        }
    }

    // ✅ 전방 돌진
    if (UCharacterMovementComponent* Move = OwnerChar->GetCharacterMovement())
    {
        Move->SetMovementMode(MOVE_Flying);
        Move->StopMovementImmediately();
        Move->Velocity = OwnerChar->GetActorForwardVector() * BoostForce;
    }

    // ✅ 몽타주 재생
    if (BoostMontage)
    {
        if (UAnimInstance* Anim = OwnerChar->GetMesh()->GetAnimInstance())
        {
            Anim->Montage_Play(BoostMontage);
        }
    }

    // ✅ 파티클 생성
    if (BoostParticle)
    {
        USkeletalMeshComponent* Mesh = OwnerChar->GetMesh();
        if (Mesh)
        {
            ActiveBoostFX.Empty();
            for (const FName& SocketName : BoostSockets)
            {
                if (Mesh->DoesSocketExist(SocketName))
                {
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

    // ✅ Drain GE 적용
    ApplyDrainGE();

    // ✅ 에너지 변화 감시 (Overheat)
    EnergyChangedHandle =
        ASC->GetGameplayAttributeValueChangeDelegate(UMechaAttributeSet::GetEnergyAttribute())
        .AddUObject(this, &UGA_AssaultBoost::OnEnergyChanged);

    // ✅ 일정 시간 후 자동 종료
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
            BoostDuration, false
        );
    }
}

void UGA_AssaultBoost::ApplyDrainGE()
{
    if (!ASC || !GE_AssaultBoostDrain) return;

    FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
    FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(GE_AssaultBoostDrain, GetAbilityLevel(), Context);

    if (Spec.IsValid())
    {
        DrainGEHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
        UE_LOG(LogTemp, Warning, TEXT(" GE_AssaultBoostDrain applied!"));  
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT(" Spec 생성 실패: GE_AssaultBoostDrain"));
    }
}

void UGA_AssaultBoost::RemoveDrainGE()
{
    if (ASC && DrainGEHandle.IsValid())
    {
        ASC->RemoveActiveGameplayEffect(DrainGEHandle);
        DrainGEHandle.Invalidate();
    }
}

void UGA_AssaultBoost::OnEnergyChanged(const FOnAttributeChangeData& Data)
{
    const float NewEnergy = Data.NewValue;
    if (NewEnergy <= KINDA_SMALL_NUMBER && IsActive())
    {
        RemoveDrainGE();
        ApplyOverheat();
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
    }
}

void UGA_AssaultBoost::ApplyOverheat()
{
    if (!ASC) return;
    ASC->AddLooseGameplayTag(Tag_StateOverheat);
}

// 능력 종료
// - 드레인 해제/리스너 제거/파티클 끄기/중력 복원
// - 차단했던 마우스 룩 입력 원복
void UGA_AssaultBoost::EndAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility, bool bWasCancelled)
{
    RemoveDrainGE();

    if (ASC && EnergyChangedHandle.IsValid())
    {
        ASC->GetGameplayAttributeValueChangeDelegate(UMechaAttributeSet::GetEnergyAttribute())
            .Remove(EnergyChangedHandle);
        EnergyChangedHandle.Reset();
    }

    // 파티클 비활성화
    for (UParticleSystemComponent* FX : ActiveBoostFX)
        if (FX) FX->DeactivateSystem();
    ActiveBoostFX.Empty();

    // 애니메이션/중력 복원
    if (OwnerChar)
    {
        // ✅ 방법 A 원복: 부스트 종료 시 마우스 룩 입력 원래 상태로 복구
        if (APlayerController* PC = Cast<APlayerController>(OwnerChar->GetController()))
        {
            PC->SetIgnoreLookInput(bPrevIgnoreLookInput);
        }

        if (UAnimInstance* Anim = OwnerChar->GetMesh()->GetAnimInstance())
            Anim->Montage_Stop(0.2f);

        if (UCharacterMovementComponent* Move = OwnerChar->GetCharacterMovement())
        {
            Move->StopMovementImmediately();
            Move->SetMovementMode(MOVE_Falling);
            Move->GravityScale = 1.0f;
        }
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
