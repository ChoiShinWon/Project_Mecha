// GA_MissleFire.cpp
// 설명:
// - UGA_MissleFire 클래스의 구현부.
// - 미사일 순차 발사, 적 탐지, 유도 미사일 설정, 데미지 GE 초기화.
//


#include "GA_MissleFire.h"

#include "GameFramework/Character.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SphereComponent.h"
#include "Components/SceneComponent.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "EnemyAirship.h"
#include "AbilitySystemComponent.h"

// 생성자: Instancing Policy 및 Net Execution Policy 설정.
UGA_MissleFire::UGA_MissleFire()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    // [Cooldown] 쿨타임 태그 설정
    Tag_CooldownMissile = FGameplayTag::RequestGameplayTag(TEXT("Cooldown.MissileFire"));

    // 이 태그가 붙어 있으면 능력 발동 불가
    ActivationBlockedTags.AddTag(Tag_CooldownMissile);
}


// Ability 활성화 로직: 순차적으로 미사일 발사.
// - CommitAbility로 코스트/쿨다운 체크.
// - 서버 권한에서만 실행.
// - NumProjectiles 개수만큼 순차적으로 타이머 설정하여 SpawnMissle 호출.
// - 모든 미사일 발사 후 능력 종료.

void UGA_MissleFire::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
  

    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        UE_LOG(LogTemp, Warning, TEXT("[GA_MissileFire] CommitAbility FAILED"));
        return;
    }

    ACharacter* OwnerChar = Cast<ACharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr);
    if (!OwnerChar || !MissleProjectileClass)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    if (!OwnerChar->HasAuthority())
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    for (int32 i = 0; i < NumProjectiles; ++i)
    {
        FTimerHandle Th;
        TWeakObjectPtr<UGA_MissleFire> WeakThis(this);
        OwnerChar->GetWorldTimerManager().SetTimer(
            Th,
            FTimerDelegate::CreateLambda([WeakThis, OwnerChar, i]()
                {
                    if (!WeakThis.IsValid() || !OwnerChar) return;
                    WeakThis->SpawnMissle(i, OwnerChar);
                }),
            i * TimeBetweenShots,
            false
        );
    }

    const float TotalTime = (NumProjectiles - 1) * TimeBetweenShots + 0.05f;
    FTimerHandle EndTh;
    OwnerChar->GetWorldTimerManager().SetTimer(
        EndTh,
        FTimerDelegate::CreateLambda([this, Handle, ActorInfo, ActivationInfo]()
            {
                if (IsActive()) EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
            }),
        TotalTime,
        false
    );

    // 🔹 발동 성공했으니 쿨타임 태그 적용
    ApplyMissileCooldown(Handle, ActorInfo, ActivationInfo);
}


void UGA_MissleFire::SpawnMissle(int32 /*Index*/, ACharacter* OwnerChar)
{


    if (!OwnerChar || !MissleProjectileClass) return;

    AActor* Target = PickBestTarget(OwnerChar);

    FVector  SpawnLoc;
    FRotator SpawnRot;

    static const FName SocketName(TEXT("MissileSocket"));
    if (USkeletalMeshComponent* Mesh = OwnerChar->GetMesh(); Mesh && Mesh->DoesSocketExist(SocketName))
    {
        SpawnLoc = Mesh->GetSocketLocation(SocketName);
        SpawnRot = Mesh->GetSocketRotation(SocketName);
    }
    else
    {
        SpawnLoc = OwnerChar->GetActorLocation() + OwnerChar->GetActorForwardVector() * SpawnOffset + FVector(0, 0, 50);
        SpawnRot = OwnerChar->GetControlRotation();
    }

    if (Target)
    {
        const FVector Dir = (Target->GetActorLocation() - SpawnLoc).GetSafeNormal();
        SpawnRot = Dir.Rotation();
        SpawnRot.Yaw += FMath::RandRange(-SpreadAngle, SpreadAngle);
        SpawnRot.Pitch += FMath::RandRange(-SpreadAngle, SpreadAngle);
    }

    FActorSpawnParameters Params;
    Params.Owner = OwnerChar;
    Params.Instigator = OwnerChar;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    AActor* Missle = OwnerChar->GetWorld()->SpawnActor<AActor>(MissleProjectileClass, SpawnLoc, SpawnRot, Params);


    UE_LOG(LogTemp, Warning,
        TEXT("[GA_MissileFire] SpawnActor result = %s"),
        Missle ? *Missle->GetName() : TEXT("NULL"));

    if (!Missle) return;

    Missle->SetReplicates(true);
    Missle->SetReplicateMovement(true);
    Missle->SetActorEnableCollision(true);
    Missle->SetActorTickEnabled(true);

    if (UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(Missle->GetRootComponent()))
    {
        Prim->IgnoreActorWhenMoving(OwnerChar, true);
    }

    const FVector LaunchDir = SpawnRot.Vector();
    UProjectileMovementComponent* Move = ForceMakeMovableAndLaunch(Missle, LaunchDir);

    if (Target && Move)
    {
        Move->bIsHomingProjectile = true;
        Move->HomingTargetComponent = Target->GetRootComponent();
        Move->HomingAccelerationMagnitude = HomingAcceleration;
        Move->bRotationFollowsVelocity = true;
        Move->Activate(true);
    }

    InitDamageOnMissle(Missle, OwnerChar);
}

// 가장 가까운 적 선택 함수.
AActor* UGA_MissleFire::PickBestTarget(const AActor* Owner) const
{
    if (!Owner) return nullptr;
    UWorld* World = Owner->GetWorld();
    if (!World) return nullptr;

    const FVector OLoc = Owner->GetActorLocation();

    if (Owner->ActorHasTag(TEXT("Enemy")) || Owner->IsA(AEnemyAirship::StaticClass()))
    {
        return UGameplayStatics::GetPlayerPawn(World, 0);
    }

    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), AllActors);

    TArray<AActor*> Candidates;
    for (AActor* A : AllActors)
    {
        if (A && A->ActorHasTag(TEXT("Enemy")))
        {
            Candidates.Add(A);
        }
    }

    AActor* Best = nullptr;
    float BestDistSq = MaxLockDistance * MaxLockDistance;

    for (AActor* A : Candidates)
    {
        if (!A) continue;
        const float D2 = FVector::DistSquared(OLoc, A->GetActorLocation());
        if (D2 < BestDistSq) { BestDistSq = D2; Best = A; }
    }
    return Best;
}

void UGA_MissleFire::SetupHoming(AActor* Missle, AActor* Target) const
{
    if (!Missle || !Target) return;
    if (UProjectileMovementComponent* Move = Missle->FindComponentByClass<UProjectileMovementComponent>())
    {
        Move->bIsHomingProjectile = true;
        Move->HomingTargetComponent = Target->GetRootComponent();
        Move->HomingAccelerationMagnitude = HomingAcceleration;
        Move->bRotationFollowsVelocity = true;

        if (Move->Velocity.IsNearlyZero())
        {
            const FVector Dir = (Target->GetActorLocation() - Missle->GetActorLocation()).GetSafeNormal();
            Move->Velocity = Dir * FMath::Max(Move->InitialSpeed, InitialLaunchSpeed);
        }
        Move->Activate(true);
    }
}

void UGA_MissleFire::InitDamageOnMissle(AActor* Missle, AActor* Source) const
{
    if (!Missle) return;

    static const FName FN_Setup(TEXT("SetupDamageSimple"));
    if (UFunction* Fn = Missle->FindFunction(FN_Setup))
    {
        struct FSetupParams { TSubclassOf<UGameplayEffect> GE; FName SetBy; float Damage; AActor* SourceActor; } Params;
        Params.GE = GE_MissleDamage;
        Params.SetBy = SetByCallerDamageName;
        Params.Damage = BaseDamage;
        Params.SourceActor = Source;
        Missle->ProcessEvent(Fn, &Params);
    }
}

UProjectileMovementComponent* UGA_MissleFire::ForceMakeMovableAndLaunch(AActor* Missle, const FVector& LaunchDir) const
{
    if (!Missle) return nullptr;

    UPrimitiveComponent* RootPrim = Cast<UPrimitiveComponent>(Missle->GetRootComponent());
    if (!RootPrim)
    {
        USphereComponent* Sphere = NewObject<USphereComponent>(Missle, TEXT("AutoRoot_Sphere"));
        Sphere->InitSphereRadius(12.f);
        Sphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Sphere->SetCollisionResponseToAllChannels(ECR_Ignore);
        Sphere->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
        Sphere->SetMobility(EComponentMobility::Movable);
        Sphere->RegisterComponent();

        USceneComponent* OldRoot = Missle->GetRootComponent();
        const FTransform OldXf = OldRoot ? OldRoot->GetComponentTransform() : Missle->GetActorTransform();

        Missle->SetRootComponent(Sphere);
        Sphere->SetWorldTransform(OldXf);
        if (OldRoot) OldRoot->AttachToComponent(Sphere, FAttachmentTransformRules::KeepWorldTransform);

        RootPrim = Sphere;
    }
    else
    {
        RootPrim->SetMobility(EComponentMobility::Movable);
        RootPrim->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    }

    UProjectileMovementComponent* Move = Missle->FindComponentByClass<UProjectileMovementComponent>();
    if (!Move)
    {
        Move = NewObject<UProjectileMovementComponent>(Missle, TEXT("Auto_Project_Movement"));
        Move->RegisterComponent();
    }

    Move->SetUpdatedComponent(RootPrim);
    Move->ProjectileGravityScale = 0.f;
    Move->InitialSpeed = FMath::Max(Move->InitialSpeed, InitialLaunchSpeed);
    Move->MaxSpeed = FMath::Max(Move->MaxSpeed, Move->InitialSpeed);
    Move->bRotationFollowsVelocity = true;
    Move->bInitialVelocityInLocalSpace = false;

    Move->Velocity = LaunchDir.GetSafeNormal() * Move->InitialSpeed;
    Move->Activate(true);

    return Move;
}

// ================== [Cooldown] 쿨타임 적용 ==================

// ================== [Cooldown] 미사일 쿨타임 태그 적용 ==================

void UGA_MissleFire::ApplyMissileCooldown(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo)
{
    if (!ActorInfo) return;

    UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
    if (!ASC) return;

    // 이미 쿨타임 태그가 붙어 있으면 중복 적용 안 함
    if (ASC->HasMatchingGameplayTag(Tag_CooldownMissile))
        return;

    // 1) 쿨타임 태그 부여
    ASC->AddLooseGameplayTag(Tag_CooldownMissile);

    // 2) 일정 시간이 지나면 태그 제거 (쿨타임 해제)
    AActor* OwnerActor = Cast<AActor>(ActorInfo->AvatarActor.Get());
    if (!OwnerActor) return;

    FTimerHandle CooldownTimerHandle;
    TWeakObjectPtr<UAbilitySystemComponent> WeakASC(ASC);

    OwnerActor->GetWorldTimerManager().SetTimer(
        CooldownTimerHandle,
        FTimerDelegate::CreateLambda([WeakASC, this]()
            {
                if (!WeakASC.IsValid()) return;
                WeakASC->RemoveLooseGameplayTag(Tag_CooldownMissile);
            }),
        CooldownDuration,
        false
    );
}
