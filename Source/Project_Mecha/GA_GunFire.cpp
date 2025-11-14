// GA_GunFire.cpp
#include "GA_GunFire.h"
#include "GA_Reload.h"

#include "MechaAttributeSet.h"
#include "MechaCharacterBase.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"

#include "GameFramework/Character.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Animation/AnimInstance.h"

UGA_GunFire::UGA_GunFire()
{
    InstancingPolicy   = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

// 능력 활성화
// - 태그로 발사 가능 여부 확인 → 투사체 스폰 → 탄창 1 감소(서버) → 필요 시 자동 장전
void UGA_GunFire::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        K2_EndAbility();
        return;
    }

    UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
    if (!ASC) { K2_EndAbility(); return; }

    // 장전/발사차단 태그면 종료
    if (ASC->HasMatchingGameplayTag(Tag_StateReloading) || ASC->HasMatchingGameplayTag(Tag_BlockFire))
    {
        K2_EndAbility();
        return;
    }

    // 탄창 확인
    const float Mag = ASC->GetNumericAttribute(UMechaAttributeSet::GetAmmoMagazineAttribute());
    if (Mag <= 0.f)
    {
        ASC->TryActivateAbilityByClass(UGA_Reload::StaticClass());
        K2_EndAbility();
        return;
    }

    // 발사 몽타주(선택)
    if (FireMontage && ActorInfo && ActorInfo->AvatarActor.IsValid())
    {
        auto* PlayTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
            this, NAME_None, FireMontage, 1.0f, NAME_None, false, 1.0f);
        PlayTask->ReadyForActivation();
    }

    // 투사체 스폰
    SpawnProjectile(ActorInfo);

    // 서버에서 탄약 감소
    if (GetAvatarActorFromActorInfo()->HasAuthority())
    {
        ASC->ApplyModToAttribute(UMechaAttributeSet::GetAmmoMagazineAttribute(), EGameplayModOp::Additive, -1.f);

        const float NewMag = ASC->GetNumericAttribute(UMechaAttributeSet::GetAmmoMagazineAttribute());
        if (NewMag <= 0.f)
        {
            ASC->TryActivateAbilityByClass(UGA_Reload::StaticClass());
        }
    }

    // 연사 템포 후 능력 종료(반자동)
    const float FireInterval = 0.3f;
    auto* DelayTask = UAbilityTask_WaitDelay::WaitDelay(this, FireInterval);
    DelayTask->OnFinish.AddDynamic(this, &UGA_GunFire::K2_EndAbility);
    DelayTask->ReadyForActivation();
}

void UGA_GunFire::OnMontageNotify()
{
    // 필요 시 노티파이로 스폰 트리거를 옮길 때 사용
    const FGameplayAbilityActorInfo* Info = GetCurrentActorInfo();
    if (Info) { SpawnProjectile(Info); }
}


// 투사체 스폰
// - Muzzle 위치/크로스헤어 라인트레이스로 방향 결정
// - 서버에서만 스폰/이동컴포넌트 속도 설정
void UGA_GunFire::SpawnProjectile(const FGameplayAbilityActorInfo* ActorInfo)
{
    if (!ActorInfo || !ActorInfo->AvatarActor.IsValid()) return;

    AMechaCharacterBase* Mecha = Cast<AMechaCharacterBase>(ActorInfo->AvatarActor.Get());
    if (!Mecha || !Mecha->ProjectileClass) return;

    FVector SpawnLoc = FVector::ZeroVector;
    FRotator SpawnRot = FRotator::ZeroRotator;


    // 1) Muzzle
    if (Mecha->MuzzleLocation)
    {
        SpawnLoc = Mecha->MuzzleLocation->GetComponentLocation();
    }
    else if (Mecha->GetMesh() && Mecha->GetMesh()->DoesSocketExist(Mecha->MuzzleSocketName))
    {
        SpawnLoc = Mecha->GetMesh()->GetSocketLocation(Mecha->MuzzleSocketName);
    }
    else
    {
        SpawnLoc = Mecha->GetActorLocation() + Mecha->GetActorForwardVector() * 100.f;
    }


    // 2) 화면 중앙 라인트레이스로 조준점 산출
    FVector TargetPoint = SpawnLoc + Mecha->GetActorForwardVector() * 10000.f;
    FVector LaunchDir   = Mecha->GetActorForwardVector();

    if (APlayerController* PC = Cast<APlayerController>(Mecha->GetController()))
    {
        int32 ViewX, ViewY;
        PC->GetViewportSize(ViewX, ViewY);

        const float ScreenX = ViewX / 2.0f;
        const float ScreenY = ViewY / 2.0f;

        FVector WorldLoc, WorldDir;
        if (PC->DeprojectScreenPositionToWorld(ScreenX, ScreenY, WorldLoc, WorldDir))
        {
            const FVector TraceStart = WorldLoc;
            const FVector TraceEnd   = WorldLoc + WorldDir * 10000.f;

            FHitResult Hit;
            FCollisionQueryParams Params;
            Params.AddIgnoredActor(Mecha);

            if (Mecha->GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, Params))
            {
                TargetPoint = Hit.ImpactPoint;
            }
            else
            {
                TargetPoint = TraceEnd;
            }
            LaunchDir = (TargetPoint - SpawnLoc).GetSafeNormal();
        }
    }
    SpawnRot = LaunchDir.Rotation();


    // 3) 총구 파티클
    if (MuzzleFlash)
    {
        UGameplayStatics::SpawnEmitterAtLocation(Mecha->GetWorld(), MuzzleFlash, SpawnLoc, SpawnRot);
    }


    // 4) 서버에서 Projectile 스폰
    if (Mecha->HasAuthority())
    {
        FActorSpawnParameters Params;
        Params.Owner = Mecha;
        Params.Instigator = Mecha;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        AActor* Projectile = Mecha->GetWorld()->SpawnActor<AActor>(Mecha->ProjectileClass, SpawnLoc, SpawnRot, Params);
        if (Projectile)
        {
            Projectile->SetReplicates(true);
            Projectile->SetReplicateMovement(true);

            if (UProjectileMovementComponent* MoveComp = Projectile->FindComponentByClass<UProjectileMovementComponent>())
            {
                MoveComp->Velocity = LaunchDir * MoveComp->InitialSpeed;
                MoveComp->Activate();
            }
        }
    }
}

void UGA_GunFire::EndAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility, bool bWasCancelled)
{
    // 필요 시 몽타주 정리 (PlayMontageAndWait 사용 시 자동 정리됨)
    if (ActorInfo && ActorInfo->AvatarActor.IsValid() && FireMontage)
    {
        if (ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get()))
        {
            if (UAnimInstance* AnimInstance = Character->GetMesh()->GetAnimInstance())
            {
                if (AnimInstance->Montage_IsPlaying(FireMontage))
                {
                    AnimInstance->Montage_Stop(0.2f, FireMontage);
                }
            }
        }
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
