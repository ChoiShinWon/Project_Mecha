#include "GA_Dash_Enemy.h"
#include "EnemyMecha.h"

#include "AbilitySystemComponent.h"
#include "MechaAttributeSet.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"
#include "TimerManager.h"

#include "AIController.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimInstance.h"
#include "Engine/World.h"

UGA_Dash_Enemy::UGA_Dash_Enemy()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    Tag_AbilityDash = FGameplayTag::RequestGameplayTag(TEXT("Ability.Dash.Enemy"));
    Tag_StateDashing = FGameplayTag::RequestGameplayTag(TEXT("State.Dashing"));
    Tag_CooldownDash = FGameplayTag::RequestGameplayTag(TEXT("Cooldown.Dash"));

    AbilityTags.AddTag(Tag_AbilityDash);
    ActivationOwnedTags.AddTag(Tag_StateDashing);
}

ACharacter* UGA_Dash_Enemy::GetEnemyCharacter(const FGameplayAbilityActorInfo* ActorInfo) const
{
    return Cast<ACharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr);
}

AActor* UGA_Dash_Enemy::GetDashTarget(ACharacter* EnemyChar) const
{
    if (!EnemyChar)
        return nullptr;

    AEnemyMecha* EnemyMecha = Cast<AEnemyMecha>(EnemyChar);
    if (!EnemyMecha)
        return nullptr;

    return EnemyMecha->CurrentTarget;   // Missile과 동일한 타겟 사용
}

void UGA_Dash_Enemy::ActivateAbility(
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

    ACharacter* EnemyChar = GetEnemyCharacter(ActorInfo);
    if (!EnemyChar)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 미사일과 동일하게 EnemyMecha의 CurrentTarget 사용
    AActor* TargetActor = GetDashTarget(EnemyChar);
    if (!TargetActor)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 대쉬 시작 전에 AI MoveTo 끊어주기 (뚝뚝 끊기는 원인 차단)
    if (AAIController* AICon = Cast<AAIController>(EnemyChar->GetController()))
    {
        AICon->StopMovement();
    }

    // === 방향/거리 계산 ===
    FVector ToTarget = TargetActor->GetActorLocation() - EnemyChar->GetActorLocation();
    ToTarget.Z = 0.0f;

    float Distance = ToTarget.Size();
    if (Distance < KINDA_SMALL_NUMBER)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    ToTarget.Normalize();

    // 한 번에 쭉 붙도록: 전체 거리에서 MinStopDistance만 남기고 전부 이동
    float TravelDistance = FMath::Max(Distance - MinStopDistance, 0.0f);

    // DashDistance를 최대값으로 사용하고 싶으면 여기서 한 번 더 Clamp
    if (DashDistance > 0.0f)
    {
        TravelDistance = FMath::Min(TravelDistance, DashDistance);
    }

    if (TravelDistance <= 0.0f)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // DashDuration 동안 TravelDistance를 이동하도록 속도 계산
    float Speed = (DashDuration > 0.f) ? (TravelDistance / DashDuration) : 0.f;
    if (Speed <= 0.0f)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    FVector LaunchVelocity = ToTarget * Speed;

    // 실제 대쉬
    EnemyChar->LaunchCharacter(LaunchVelocity, true, false);

    // Dash 몽타주 재생 (있을 때만)
    if (DashMontage && EnemyChar->GetMesh())
    {
        if (UAnimInstance* AnimInst = EnemyChar->GetMesh()->GetAnimInstance())
        {
            AnimInst->Montage_Play(DashMontage);
        }
    }

    // 쿨다운 이펙트 (있으면)
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

    // DashDuration 이후 강제로 정지 + EndAbility 호출
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

void UGA_Dash_Enemy::StopDash()
{
    const FGameplayAbilityActorInfo* Info = GetCurrentActorInfo();
    ACharacter* EnemyChar = GetEnemyCharacter(Info);

    if (EnemyChar)
    {
        if (UCharacterMovementComponent* MoveComp = EnemyChar->GetCharacterMovement())
        {
            MoveComp->StopMovementImmediately();
        }
    }

    FGameplayAbilitySpecHandle Handle = GetCurrentAbilitySpecHandle();
    FGameplayAbilityActivationInfo ActInfo = GetCurrentActivationInfo();

    EndAbility(Handle, Info, ActInfo, true, false);
}

void UGA_Dash_Enemy::EndAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility,
    bool bWasCancelled)
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(DashTimerHandle);
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
