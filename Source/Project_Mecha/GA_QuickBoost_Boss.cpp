#include "GA_QuickBoost_Boss.h"
#include "EnemyMecha.h"

#include "AbilitySystemComponent.h"
#include "TimerManager.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimInstance.h"
#include "Engine/World.h"

UGA_QuickBoost_Boss::UGA_QuickBoost_Boss()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

    Tag_AbilityQuickBoost = FGameplayTag::RequestGameplayTag(TEXT("Ability.QuickBoost.Boss"));
    Tag_StateBoosting = FGameplayTag::RequestGameplayTag(TEXT("State.Boosting"));

    AbilityTags.AddTag(Tag_AbilityQuickBoost);
    ActivationOwnedTags.AddTag(Tag_StateBoosting);
}

ACharacter* UGA_QuickBoost_Boss::GetEnemyChar(const FGameplayAbilityActorInfo* ActorInfo) const
{
    return Cast<ACharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr);
}

AActor* UGA_QuickBoost_Boss::GetCurrentTarget(ACharacter* EnemyChar) const
{
    if (!EnemyChar) return nullptr;

    if (AEnemyMecha* EnemyMecha = Cast<AEnemyMecha>(EnemyChar))
    {
        return EnemyMecha->CurrentTarget; // 기존에 쓰는 타겟 그대로 재사용
    }
    return nullptr;
}

void UGA_QuickBoost_Boss::ActivateAbility(
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

    ACharacter* EnemyChar = GetEnemyChar(ActorInfo);
    if (!EnemyChar)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 이동 AI 끊기 (부스트 도중 흔들리는거 방지)
    if (AAIController* AICon = Cast<AAIController>(EnemyChar->GetController()))
    {
        AICon->StopMovement();
    }

    // Movement 세팅 저장 + 마찰 제거
    if (UCharacterMovementComponent* MoveComp = EnemyChar->GetCharacterMovement())
    {
        if (!bSavedMovementParams)
        {
            SavedBrakingFrictionFactor = MoveComp->BrakingFrictionFactor;
            SavedBrakingDecelWalking = MoveComp->BrakingDecelerationWalking;
            bSavedMovementParams = true;
        }

        MoveComp->BrakingFrictionFactor = 0.0f;
        MoveComp->BrakingDecelerationWalking = 0.0f;
    }

    // == 방향 계산 ==
    FVector Dir = FVector::ZeroVector;

    if (bUseForwardDirection)
    {
        Dir = EnemyChar->GetActorForwardVector();
    }
    else
    {
        AActor* Target = GetCurrentTarget(EnemyChar);
        if (Target)
        {
            FVector ToTarget = Target->GetActorLocation() - EnemyChar->GetActorLocation();
            ToTarget.Z = 0.0f;

            if (!ToTarget.IsNearlyZero())
            {
                ToTarget.Normalize();
                Dir = bBoostAwayFromTarget ? (-ToTarget) : ToTarget;
            }
        }
    }

    if (Dir.IsNearlyZero())
    {
        // 방향 못 구했으면 그냥 앞으로
        Dir = EnemyChar->GetActorForwardVector();
    }

    // BoostDuration 동안 BoostDistance 이동하도록 속도 계산
    const float Speed = (BoostDuration > 0.f) ? (BoostDistance / BoostDuration) : 0.f;
    if (Speed <= 0.f)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    const FVector LaunchVel = Dir.GetSafeNormal2D() * Speed;

    // 한 번에 툭 튀게
    EnemyChar->LaunchCharacter(LaunchVel, true, false);

    // 몽타주 재생 (있으면)
    if (BoostMontage && EnemyChar->GetMesh())
    {
        if (UAnimInstance* AnimInst = EnemyChar->GetMesh()->GetAnimInstance())
        {
            if (!AnimInst->Montage_IsPlaying(BoostMontage))
            {
                AnimInst->Montage_Play(BoostMontage);
            }
        }
    }

    // 원하면 블랙보드 플래그 (Boss용이면 필요 없을 수도 있음)
    if (AEnemyMecha* EnemyMecha = Cast<AEnemyMecha>(EnemyChar))
    {
        if (AAIController* AICon = Cast<AAIController>(EnemyMecha->GetController()))
        {
            if (UBlackboardComponent* BB = AICon->GetBlackboardComponent())
            {
                BB->SetValueAsBool(TEXT("IsBoosting"), true);
            }
        }
    }

    // BoostDuration 후에 종료
    if (UWorld* World = EnemyChar->GetWorld())
    {
        World->GetTimerManager().SetTimer(
            BoostTimerHandle,
            this,
            &UGA_QuickBoost_Boss::StopBoost,
            BoostDuration,
            false
        );
    }
}

void UGA_QuickBoost_Boss::StopBoost()
{
    const FGameplayAbilityActorInfo* Info = GetCurrentActorInfo();
    FGameplayAbilitySpecHandle Handle = GetCurrentAbilitySpecHandle();
    FGameplayAbilityActivationInfo ActInfo = GetCurrentActivationInfo();

    EndAbility(Handle, Info, ActInfo, true, false);
}

void UGA_QuickBoost_Boss::EndAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility,
    bool bWasCancelled)
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(BoostTimerHandle);
    }

    ACharacter* EnemyChar = GetEnemyChar(ActorInfo);

    if (EnemyChar)
    {
        if (UCharacterMovementComponent* MoveComp = EnemyChar->GetCharacterMovement())
        {
            MoveComp->StopMovementImmediately();

            if (bSavedMovementParams)
            {
                MoveComp->BrakingFrictionFactor = SavedBrakingFrictionFactor;
                MoveComp->BrakingDecelerationWalking = SavedBrakingDecelWalking;
            }
        }

        if (AEnemyMecha* EnemyMecha = Cast<AEnemyMecha>(EnemyChar))
        {
            if (AAIController* AICon = Cast<AAIController>(EnemyMecha->GetController()))
            {
                if (UBlackboardComponent* BB = AICon->GetBlackboardComponent())
                {
                    BB->SetValueAsBool(TEXT("IsBoosting"), false);
                }
            }
        }

        if (BoostMontage && EnemyChar->GetMesh())
        {
            if (UAnimInstance* AnimInst = EnemyChar->GetMesh()->GetAnimInstance())
            {
                if (AnimInst->Montage_IsPlaying(BoostMontage))
                {
                    AnimInst->Montage_Stop(0.1f, BoostMontage);
                }
            }
        }
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
