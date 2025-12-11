// GA_BossMissileRain.cpp

#include "GA_BossMissileRain.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "EnemyMecha.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "GameplayTagContainer.h"
#include "Animation/AnimInstance.h"

UGA_BossMissileRain::UGA_BossMissileRain()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

    // 기본값 세팅
    HoverDuration = 7.0f;
    ShotsPerSide = 5;     // 좌 5발, 우 5발
    FireInterval = 0.7f;  // 0.7초마다 한 쌍 발사
    LeftMuzzleSocketName = TEXT("Muzzle01");
    RightMuzzleSocketName = TEXT("Muzzle02");
}

void UGA_BossMissileRain::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    if (!ActorInfo || !ActorInfo->AvatarActor.IsValid())
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 슈퍼 아머 태그 부여 (피격 HitReact/Montage 안 끊기도록)
    {
        AActor* AvatarActor = ActorInfo->AvatarActor.Get();
        if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(AvatarActor))
        {
            if (UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent())
            {
                ASC->AddLooseGameplayTag(
                    FGameplayTag::RequestGameplayTag(TEXT("State.SuperArmor"))
                );
            }
        }
    }

    // 1) 보스를 공중으로 띄우고, 몽타주 재생
    StartHover(ActorInfo);

    // 1-1) Blackboard에 "IsUsingMissileRain" = true 세팅
    if (AEnemyMecha* BossChar = Cast<AEnemyMecha>(ActorInfo->AvatarActor.Get()))
    {
        if (AAIController* AICon = Cast<AAIController>(BossChar->GetController()))
        {
            if (UBlackboardComponent* BB = AICon->GetBlackboardComponent())
            {
                BB->SetValueAsBool(TEXT("IsUsingMissileRain"), true);
            }
        }
    }

    // 2) 내부 카운터 초기화
    ShotsFiredPairs = 0;

    UWorld* World = GetWorld();
    if (!World)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 3) 미사일 발사 타이머 시작 (쌍 발사)
    World->GetTimerManager().SetTimer(
        FireTimerHandle,
        this,
        &UGA_BossMissileRain::SpawnMissilePair,
        FireInterval,
        true,
        0.0f
    );

    // 4) 7초 후 패턴 종료
    World->GetTimerManager().SetTimer(
        EndTimerHandle,
        this,
        &UGA_BossMissileRain::OnMissileRainFinished,
        HoverDuration,
        false
    );
}


void UGA_BossMissileRain::EndAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility,
    bool bWasCancelled)
{
    UWorld* World = GetWorld();
    if (World)
    {
        World->GetTimerManager().ClearTimer(FireTimerHandle);
        World->GetTimerManager().ClearTimer(EndTimerHandle);
    }

    // 호버/애니 상태 원복
    EndHover(ActorInfo);

    if (ActorInfo && ActorInfo->AvatarActor.IsValid())
    {
        if (AEnemyMecha* BossChar = Cast<AEnemyMecha>(ActorInfo->AvatarActor.Get()))
        {
            //  1) SuperArmor 태그 해제
            if (UAbilitySystemComponent* ASC = BossChar->GetAbilitySystemComponent())
            {
                ASC->RemoveLooseGameplayTag(
                    FGameplayTag::RequestGameplayTag(TEXT("State.SuperArmor"))
                );
            }

            // 🔹 2) Blackboard에 "IsUsingMissileRain" = false 세팅
            if (AAIController* AICon = Cast<AAIController>(BossChar->GetController()))
            {
                if (UBlackboardComponent* BB = AICon->GetBlackboardComponent())
                {
                    BB->SetValueAsBool(TEXT("IsUsingMissileRain"), false);
                }
            }
        }
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_BossMissileRain::StartHover(const FGameplayAbilityActorInfo* ActorInfo)
{
    if (!ActorInfo || !ActorInfo->AvatarActor.IsValid())
    {
        return;
    }

    ACharacter* BossChar = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
    if (!BossChar)
    {
        return;
    }

    // 이동 모드 Flying + 중력 제거
    if (UCharacterMovementComponent* MoveComp = BossChar->GetCharacterMovement())
    {
        MoveComp->SetMovementMode(MOVE_Flying);
        MoveComp->GravityScale = 0.0f;
    }

    // 살짝 위로 띄워주기 (선택)
    BossChar->LaunchCharacter(FVector(0.f, 0.f, 300.f), false, true);

    // 몽타주 재생
    if (FireMontage)
    {
        if (USkeletalMeshComponent* Mesh = BossChar->GetMesh())
        {
            if (UAnimInstance* AnimInstance = Mesh->GetAnimInstance())
            {
                AnimInstance->Montage_Play(FireMontage, 1.0f);
            }
        }
    }
}

void UGA_BossMissileRain::EndHover(const FGameplayAbilityActorInfo* ActorInfo)
{
    if (!ActorInfo || !ActorInfo->AvatarActor.IsValid())
    {
        return;
    }

    ACharacter* BossChar = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
    if (!BossChar)
    {
        return;
    }

    if (UCharacterMovementComponent* MoveComp = BossChar->GetCharacterMovement())
    {
        // 보스 기본 이동 모드로 되돌리기 (일반적으로 Walking)
        MoveComp->SetMovementMode(MOVE_Walking);
        MoveComp->GravityScale = 1.0f;
    }

    // 몽타주 정리 (원하면)
    if (FireMontage)
    {
        if (USkeletalMeshComponent* Mesh = BossChar->GetMesh())
        {
            if (UAnimInstance* AnimInstance = Mesh->GetAnimInstance())
            {
                AnimInstance->Montage_Stop(0.2f, FireMontage);
            }
        }
    }
}

void UGA_BossMissileRain::SpawnMissilePair()
{
    // 쏴야 할 만큼 다 쐈으면 타이머 정지
    if (ShotsFiredPairs >= ShotsPerSide)
    {
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().ClearTimer(FireTimerHandle);
        }
        return;
    }

    const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
    if (!ActorInfo || !ActorInfo->AvatarActor.IsValid())
    {
        return;
    }

    ACharacter* BossChar = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
    if (!BossChar)
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!World || !MissileClass)
    {
        return;
    }

    // 타겟: 플레이어 (0번 플레이어 기준)
    APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(World, 0);
    const bool bHasTarget = IsValid(PlayerPawn);

    USkeletalMeshComponent* Mesh = BossChar->GetMesh();
    if (!Mesh)
    {
        return;
    }

    // 1) 왼쪽 소켓 위치/회전
    const FTransform LeftMuzzleTransform = Mesh->GetSocketTransform(LeftMuzzleSocketName, RTS_World);
    FVector LeftLoc = LeftMuzzleTransform.GetLocation();
    FRotator LeftRot = LeftMuzzleTransform.Rotator();

    // 2) 오른쪽 소켓 위치/회전
    const FTransform RightMuzzleTransform = Mesh->GetSocketTransform(RightMuzzleSocketName, RTS_World);
    FVector RightLoc = RightMuzzleTransform.GetLocation();
    FRotator RightRot = RightMuzzleTransform.Rotator();

    // 타겟이 있으면 타겟을 향하도록 회전 보정
    if (bHasTarget)
    {
        const FVector TargetLoc = PlayerPawn->GetActorLocation();

        LeftRot = UKismetMathLibrary::FindLookAtRotation(LeftLoc, TargetLoc);
        RightRot = UKismetMathLibrary::FindLookAtRotation(RightLoc, TargetLoc);

        // 100% 명중 방지용 랜덤 오프셋 (조금만 건드림)
        const float RandYawOffsetL = FMath::RandRange(-5.0f, 5.0f);
        const float RandPitchOffsetL = FMath::RandRange(-3.0f, 3.0f);
        LeftRot.Yaw += RandYawOffsetL;
        LeftRot.Pitch += RandPitchOffsetL;

        const float RandYawOffsetR = FMath::RandRange(-5.0f, 5.0f);
        const float RandPitchOffsetR = FMath::RandRange(-3.0f, 3.0f);
        RightRot.Yaw += RandYawOffsetR;
        RightRot.Pitch += RandPitchOffsetR;
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = BossChar;
    SpawnParams.Instigator = BossChar;

    // 왼쪽 미사일
    World->SpawnActor<AActor>(
        MissileClass,
        LeftLoc,
        LeftRot,
        SpawnParams
    );

    // 오른쪽 미사일
    World->SpawnActor<AActor>(
        MissileClass,
        RightLoc,
        RightRot,
        SpawnParams
    );

    // 한 쌍 발사 완료
    ++ShotsFiredPairs;
}

void UGA_BossMissileRain::OnMissileRainFinished()
{
    // 7초 끝났으니 능력 종료
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
