// GA_MissleFire.cpp
// 설명:
// - UGA_MissleFire 클래스의 구현부.
// - 미사일 순차 발사, 적 탐지, 유도 미사일 설정, 데미지 GE 초기화.
//
// Description:
// - Implementation of UGA_MissleFire class.
// - Sequential missile firing, enemy detection, homing missile setup, damage GE initialization.

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

// 생성자: Instancing Policy 및 Net Execution Policy 설정.
// Constructor: Sets Instancing Policy and Net Execution Policy.
UGA_MissleFire::UGA_MissleFire()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

// Ability 활성화 로직: 순차적으로 미사일 발사.
// - CommitAbility로 코스트/쿨다운 체크.
// - 서버 권한에서만 실행.
// - NumProjectiles 개수만큼 순차적으로 타이머 설정하여 SpawnMissle 호출.
// - 모든 미사일 발사 후 능력 종료.
//
// Ability activation logic: Fires missiles sequentially.
// - Checks cost/cooldown via CommitAbility.
// - Only executes on server authority.
// - Sets timers to call SpawnMissle sequentially for NumProjectiles count.
// - Ends ability after all missiles are fired.
void UGA_MissleFire::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo)) return;

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
    if (!Missle) return;

    Missle->SetReplicates(true);
    Missle->SetReplicateMovement(true);
    Missle->SetActorEnableCollision(true);
    Missle->SetActorTickEnabled(true);

    // �� �����ڿ� ��� �浹�ؼ� ���ߴ� ���� ����
    if (UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(Missle->GetRootComponent()))
    {
        Prim->IgnoreActorWhenMoving(OwnerChar, true);
    }

    // �� �������� �����̰ԡ� ���� ��ġ + �ʱ� �ӵ� ����
    const FVector LaunchDir = SpawnRot.Vector();
    UProjectileMovementComponent* Move = ForceMakeMovableAndLaunch(Missle, LaunchDir);

    // �� Ÿ�� ������ Homing ���� (Launch ���Ŀ� �ٿ��� OK)
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
// - Owner가 적이면 플레이어를 타겟으로 반환.
// - Owner가 플레이어면 MaxLockDistance 내의 가장 가까운 EnemyAirship 반환.
//
// Picks nearest enemy.
// - Returns player as target if Owner is enemy.
// - Returns nearest EnemyAirship within MaxLockDistance if Owner is player.
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

    // "Enemy" 태그를 가진 모든 액터를 검색 (EnemyMecha, EnemyAirship 등 모두 포함)
    // Search all actors with "Enemy" tag (includes EnemyMecha, EnemyAirship, etc.)
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

    // 0) ��Ʈ�� Primitive�� �ƴϸ�, �浹 ������ Sphere�� ����� ��Ʈ�� �°�
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

    // 1) ProjectileMovement�� ������ ����
    UProjectileMovementComponent* Move = Missle->FindComponentByClass<UProjectileMovementComponent>();
    if (!Move)
    {
        Move = NewObject<UProjectileMovementComponent>(Missle, TEXT("Auto_Project_Movement"));
        Move->RegisterComponent();
    }

    // 2) UpdatedComponent ���� + �⺻ �Ķ���� ����
    Move->SetUpdatedComponent(RootPrim);
    Move->ProjectileGravityScale = 0.f;     // �߷����� ó������ ���� ����
    Move->InitialSpeed = FMath::Max(Move->InitialSpeed, InitialLaunchSpeed);
    Move->MaxSpeed = FMath::Max(Move->MaxSpeed, Move->InitialSpeed);
    Move->bRotationFollowsVelocity = true;
    Move->bInitialVelocityInLocalSpace = false; // ���� �������� ���

    // 3) �ʱ� �ӵ� ���� + Ȱ��ȭ
    Move->Velocity = LaunchDir.GetSafeNormal() * Move->InitialSpeed;
    Move->Activate(true);

    return Move;
}
