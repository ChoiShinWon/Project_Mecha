#include "GA_Hover_Enemy.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "EnemyMecha.h"

UGA_Hover_Enemy::UGA_Hover_Enemy()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

    // 필요하면 여기서 기본 태그 설정
    // Tag_StateHovering = FGameplayTag::RequestGameplayTag(FName("State.Hovering"));
}

void UGA_Hover_Enemy::ActivateAbility(
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

    OwnerCharacter = Cast<ACharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr);
    ASC = GetAbilitySystemComponentFromActorInfo();

    if (!OwnerCharacter.IsValid())
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    StartHover();

    // ★ 일정 시간 뒤 자동으로 EndAbility 호출
    if (OwnerCharacter.IsValid())
    {
        FTimerManager& TM = OwnerCharacter->GetWorldTimerManager();
        TM.ClearTimer(HoverTimerHandle);
        TM.SetTimer(
            HoverTimerHandle,
            this,
            &UGA_Hover_Enemy::OnHoverDurationEnded,
            HoverDuration,
            false
        );
    }
}

void UGA_Hover_Enemy::EndAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility, bool bWasCancelled)
{
    // 타이머 정리
    if (OwnerCharacter.IsValid())
    {
        OwnerCharacter->GetWorldTimerManager().ClearTimer(HoverTimerHandle);
    }

    StopHover();

    if (ASC.IsValid() && Tag_StateHovering.IsValid())
    {
        ASC->RemoveLooseGameplayTag(Tag_StateHovering);
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_Hover_Enemy::StartHover()
{
	if (!OwnerCharacter.IsValid()) return;

	UCharacterMovementComponent* Move = OwnerCharacter->GetCharacterMovement();
	if (!Move) return;

	// 저장
	SavedGravityScale = Move->GravityScale;
	SavedBrakingFrictionFactor = Move->BrakingFrictionFactor;
	SavedGroundFriction = Move->GroundFriction;

	// 태그 추가
	if (ASC.IsValid() && Tag_StateHovering.IsValid())
	{
		ASC->AddLooseGameplayTag(Tag_StateHovering);
	}

	// ========== Hover Particle 활성화 ==========
	if (AEnemyMecha* EnemyMecha = Cast<AEnemyMecha>(OwnerCharacter.Get()))
	{
		EnemyMecha->ActivateHoverParticles();
	}

	// 공중에서 미끄러지듯 움직이게
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
}

void UGA_Hover_Enemy::StopHover()
{
	if (!OwnerCharacter.IsValid()) return;

	UCharacterMovementComponent* Move = OwnerCharacter->GetCharacterMovement();
	if (!Move) return;

	// ========== Hover Particle 비활성화 ==========
	if (AEnemyMecha* EnemyMecha = Cast<AEnemyMecha>(OwnerCharacter.Get()))
	{
		EnemyMecha->DeactivateHoverParticles();
	}

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

void UGA_Hover_Enemy::OnHoverDurationEnded()
{
    // 이미 끝난 상태면 무시
    if (!IsActive())
        return;

    // ★ 여기서 EndAbility 호출 → StopHover + 태그 제거까지 호출됨
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
