// GA_MissleFire.cpp
// 미사일 순차 발사, 적 탐지, 유도 미사일 설정, 데미지 적용을 담당하는 Gameplay Ability

#include "GA_MissleFire.h"

#include "GameFramework/Character.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SphereComponent.h"
#include "Components/SceneComponent.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "EnemyMecha.h"
#include "AbilitySystemComponent.h"

// ========================================
// 생성자
// ========================================
UGA_MissleFire::UGA_MissleFire()
{
	// 액터마다 하나의 인스턴스만 생성
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	
	// 클라이언트에서 예측 실행, 서버에서 확정
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	// 쿨타임 태그 등록
	Tag_CooldownMissile = FGameplayTag::RequestGameplayTag(TEXT("Cooldown.MissileFire"));
	
	// 쿨타임 태그가 있으면 능력 발동 차단
	ActivationBlockedTags.AddTag(Tag_CooldownMissile);
}

// ========================================
// 능력 활성화 - 미사일 순차 발사 시작
// ========================================
void UGA_MissleFire::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	// 코스트/쿨다운 체크
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		return;
	}

	// 오너 캐릭터 유효성 검사
	ACharacter* OwnerChar = Cast<ACharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr);
	if (!OwnerChar || !MissleProjectileClass)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 서버에서만 미사일 스폰 실행
	if (!OwnerChar->HasAuthority())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	// 첫 번째 미사일은 즉시 발사
	SpawnMissle(0, OwnerChar);

	// 기존 타이머 정리 (재활성화 시)
	ClearAllTimers();
	MissileFireTimerHandles.Reset();

	// 나머지 미사일들은 시간차를 두고 순차 발사
	for (int32 i = 1; i < NumProjectiles; ++i)
	{
		FTimerHandle Th;
		TWeakObjectPtr<UGA_MissleFire> WeakThis(this);
		
		// 타이머로 미사일 발사 예약
		OwnerChar->GetWorldTimerManager().SetTimer(
			Th,
			FTimerDelegate::CreateLambda([WeakThis, OwnerChar, i]()
				{
					if (!WeakThis.IsValid() || !OwnerChar) return;
					WeakThis->SpawnMissle(i, OwnerChar);
				}),
			i * TimeBetweenShots,  // 발사 간격
			false
		);
		
		// 타이머 핸들 저장 (나중에 취소 가능하도록)
		MissileFireTimerHandles.Add(Th);
	}

	// 모든 미사일 발사 완료 후 능력 종료
	const float TotalTime = (NumProjectiles - 1) * TimeBetweenShots + EndAbilityBufferTime;
	OwnerChar->GetWorldTimerManager().SetTimer(
		EndAbilityTimerHandle,
		FTimerDelegate::CreateLambda([this, Handle, ActorInfo, ActivationInfo]()
			{
				if (IsActive()) EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
			}),
		TotalTime,
		false
	);

	// 쿨타임 시작
	ApplyMissileCooldown(Handle, ActorInfo, ActivationInfo);
}

// ========================================
// 미사일 스폰 및 설정
// ========================================
void UGA_MissleFire::SpawnMissle(int32 Index, ACharacter* OwnerChar)
{
	// 유효성 검사
	if (!OwnerChar || !MissleProjectileClass)
	{
		return;
	}

	// 타겟 검색
	AActor* Target = PickBestTarget(OwnerChar);

	FVector  SpawnLoc;
	FRotator SpawnRot;

	// ========== 스폰 위치 계산 ==========
	static const FName SocketName(TEXT("MissileSocket"));
	if (USkeletalMeshComponent* Mesh = OwnerChar->GetMesh(); Mesh && Mesh->DoesSocketExist(SocketName))
	{
		// 소켓이 있으면 소켓 위치에서 스폰
		SpawnLoc = Mesh->GetSocketLocation(SocketName);
		SpawnRot = Mesh->GetSocketRotation(SocketName);
		
		// 캐릭터 충돌 방지를 위해 앞쪽으로 추가 오프셋
		SpawnLoc += Mesh->GetSocketRotation(SocketName).Vector() * SocketCollisionOffset;
	}
	else
	{
		// 소켓이 없으면 캐릭터 앞쪽에 스폰
		SpawnLoc = OwnerChar->GetActorLocation() + OwnerChar->GetActorForwardVector() * (SpawnOffset + SocketCollisionOffset) + FVector(0, 0, FallbackZOffset);
		SpawnRot = OwnerChar->GetControlRotation();
	}

	// ========== 타겟 방향으로 회전 ==========
	if (Target)
	{
		// 타겟 방향 계산
		const FVector Dir = (Target->GetActorLocation() - SpawnLoc).GetSafeNormal();
		SpawnRot = Dir.Rotation();
		
		// 랜덤 확산 적용
		SpawnRot.Yaw += FMath::RandRange(-SpreadAngle, SpreadAngle);
		SpawnRot.Pitch += FMath::RandRange(-SpreadAngle, SpreadAngle);
	}

	// ========== 미사일 스폰 ==========
	FActorSpawnParameters Params;
	Params.Owner = OwnerChar;
	Params.Instigator = OwnerChar;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AActor* Missle = OwnerChar->GetWorld()->SpawnActor<AActor>(MissleProjectileClass, SpawnLoc, SpawnRot, Params);

	if (!Missle) return;

	// ========== 네트워크 설정 ==========
	Missle->SetReplicates(true);
	Missle->SetReplicateMovement(true);
	Missle->SetActorEnableCollision(true);
	Missle->SetActorTickEnabled(true);

	// ========== 충돌 무시 설정 (캐릭터와 충돌 방지) ==========
	if (UPrimitiveComponent* MissilePrim = Cast<UPrimitiveComponent>(Missle->GetRootComponent()))
	{
		// 미사일이 오너를 무시
		MissilePrim->IgnoreActorWhenMoving(OwnerChar, true);
		MissilePrim->MoveIgnoreActors.AddUnique(OwnerChar);
		
		// 오너의 모든 컴포넌트와 상호 무시 (양방향)
		TArray<UPrimitiveComponent*> OwnerPrims;
		OwnerChar->GetComponents<UPrimitiveComponent>(OwnerPrims);
		for (UPrimitiveComponent* OwnerPrim : OwnerPrims)
		{
			if (OwnerPrim)
			{
				MissilePrim->IgnoreComponentWhenMoving(OwnerPrim, true);
				OwnerPrim->IgnoreComponentWhenMoving(MissilePrim, true);
			}
		}
	}

	// ========== 발사 및 유도 설정 ==========
	const FVector LaunchDir = SpawnRot.Vector();
	UProjectileMovementComponent* Move = ForceMakeMovableAndLaunch(Missle, LaunchDir);

	// 타겟이 있으면 유도 미사일로 설정
	if (Target && Move)
	{
		Move->bIsHomingProjectile = true;
		Move->HomingTargetComponent = Target->GetRootComponent();
		Move->HomingAccelerationMagnitude = HomingAcceleration;
		Move->bRotationFollowsVelocity = true;
		Move->Activate(true);
	}

	// 데미지 설정
	InitDamageOnMissle(Missle, OwnerChar);
}

// ========================================
// 가장 가까운 적 찾기
// ========================================
AActor* UGA_MissleFire::PickBestTarget(const AActor* Owner) const
{
	if (!Owner) return nullptr;
	UWorld* World = Owner->GetWorld();
	if (!World) return nullptr;

	const FVector OLoc = Owner->GetActorLocation();

	// Enemy가 발사하는 경우 → 플레이어를 타겟으로
	if (Owner->ActorHasTag(TEXT("Enemy")))
	{
		return UGameplayStatics::GetPlayerPawn(World, 0);
	}

	// 플레이어가 발사하는 경우 → 가장 가까운 Enemy 찾기
	// 성능 최적화: 특정 Enemy 클래스만 직접 검색 (모든 액터를 가져오지 않음)
	TArray<AActor*> Candidates;
	
	// EnemyMecha 클래스로 검색
	TArray<AActor*> EnemyMechas;
	UGameplayStatics::GetAllActorsOfClass(World, AEnemyMecha::StaticClass(), EnemyMechas);
	Candidates.Append(EnemyMechas);
	

	// 최단 거리의 적 선택
	AActor* Best = nullptr;
	const float MaxDistSq = MaxLockDistance * MaxLockDistance;
	float BestDistSq = MaxDistSq;

	for (AActor* Candidate : Candidates)
	{
		if (!Candidate) continue;
		
		const float DistSq = FVector::DistSquared(OLoc, Candidate->GetActorLocation());
		if (DistSq < BestDistSq) 
		{ 
			BestDistSq = DistSq; 
			Best = Candidate; 
		}
	}
	return Best;
}

// ========================================
// 미사일에 데미지 설정 적용
// ========================================
void UGA_MissleFire::InitDamageOnMissle(AActor* Missle, AActor* Source) const
{
	if (!Missle) return;

	// 블루프린트 함수 "SetupDamageSimple" 호출
	static const FName FN_Setup(TEXT("SetupDamageSimple"));
	if (UFunction* Fn = Missle->FindFunction(FN_Setup))
	{
		// 데미지 파라미터 구조체 생성
		struct FSetupParams 
		{ 
			TSubclassOf<UGameplayEffect> GE; 
			FName SetBy; 
			float Damage; 
			AActor* SourceActor; 
		} Params;
		
		Params.GE = GE_MissleDamage;
		Params.SetBy = SetByCallerDamageName;
		Params.Damage = BaseDamage;
		Params.SourceActor = Source;
		
		// 함수 실행
		Missle->ProcessEvent(Fn, &Params);
	}
}

// ========================================
// 미사일에 Projectile Movement 컴포넌트 설정 및 발사
// ========================================
UProjectileMovementComponent* UGA_MissleFire::ForceMakeMovableAndLaunch(AActor* Missle, const FVector& LaunchDir) const
{
	if (!Missle) return nullptr;

	// ========== Root 컴포넌트 확인 및 생성 ==========
	UPrimitiveComponent* RootPrim = Cast<UPrimitiveComponent>(Missle->GetRootComponent());
	if (!RootPrim)
	{
		// Root가 없거나 Primitive가 아니면 새로 생성
		USphereComponent* Sphere = NewObject<USphereComponent>(Missle, TEXT("AutoRoot_Sphere"));
		Sphere->InitSphereRadius(12.f);
		Sphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Sphere->SetCollisionResponseToAllChannels(ECR_Ignore);
		Sphere->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
		Sphere->SetMobility(EComponentMobility::Movable);
		Sphere->RegisterComponent();

		// 기존 Root를 새 Sphere에 붙이기
		USceneComponent* OldRoot = Missle->GetRootComponent();
		const FTransform OldXf = OldRoot ? OldRoot->GetComponentTransform() : Missle->GetActorTransform();

		Missle->SetRootComponent(Sphere);
		Sphere->SetWorldTransform(OldXf);
		if (OldRoot) OldRoot->AttachToComponent(Sphere, FAttachmentTransformRules::KeepWorldTransform);

		RootPrim = Sphere;
	}
	else
	{
		// Root가 있으면 Movable로 설정
		RootPrim->SetMobility(EComponentMobility::Movable);
		RootPrim->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	}

	// ========== Projectile Movement 컴포넌트 설정 ==========
	UProjectileMovementComponent* Move = Missle->FindComponentByClass<UProjectileMovementComponent>();
	if (!Move)
	{
		// 컴포넌트가 없으면 생성
		Move = NewObject<UProjectileMovementComponent>(Missle, TEXT("Auto_Project_Movement"));
		Move->RegisterComponent();
	}

	// 이동 컴포넌트 설정
	Move->SetUpdatedComponent(RootPrim);
	Move->ProjectileGravityScale = 0.f;  // 중력 무시
	Move->InitialSpeed = FMath::Max(Move->InitialSpeed, InitialLaunchSpeed);
	Move->MaxSpeed = FMath::Max(Move->MaxSpeed, Move->InitialSpeed);
	Move->bRotationFollowsVelocity = true;  // 속도 방향으로 회전
	Move->bInitialVelocityInLocalSpace = false;

	// 발사 속도 설정
	Move->Velocity = LaunchDir.GetSafeNormal() * Move->InitialSpeed;
	Move->Activate(true);

	return Move;
}

// ========================================
// 쿨타임 적용
// ========================================
void UGA_MissleFire::ApplyMissileCooldown(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	if (!ActorInfo) return;

	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	if (!ASC) return;

	// 이미 쿨타임 중이면 중복 적용 방지
	if (ASC->HasMatchingGameplayTag(Tag_CooldownMissile))
		return;

	// 쿨타임 태그 부여
	ASC->AddLooseGameplayTag(Tag_CooldownMissile);

	// 일정 시간 후 쿨타임 태그 제거
	AActor* OwnerActor = Cast<AActor>(ActorInfo->AvatarActor.Get());
	if (!OwnerActor) return;

	// 기존 쿨타임 타이머 취소 (중복 방지)
	if (CooldownTimerHandle.IsValid())
	{
		OwnerActor->GetWorldTimerManager().ClearTimer(CooldownTimerHandle);
	}

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

// ========================================
// 타이머 정리
// ========================================
void UGA_MissleFire::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	// 모든 타이머 정리
	ClearAllTimers();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// ========================================
// 모든 타이머 취소
// ========================================
void UGA_MissleFire::ClearAllTimers()
{
	if (const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo())
	{
		if (AActor* OwnerActor = Cast<AActor>(ActorInfo->AvatarActor.Get()))
		{
			FTimerManager& TimerManager = OwnerActor->GetWorldTimerManager();

			// 미사일 발사 타이머들 취소
			for (FTimerHandle& Handle : MissileFireTimerHandles)
			{
				if (Handle.IsValid())
				{
					TimerManager.ClearTimer(Handle);
				}
			}
			MissileFireTimerHandles.Empty();

			// 능력 종료 타이머 취소
			if (EndAbilityTimerHandle.IsValid())
			{
				TimerManager.ClearTimer(EndAbilityTimerHandle);
			}

			// 쿨타임 타이머는 유지 (쿨타임은 계속 진행되어야 함)
			// 필요시 쿨타임 타이머도 취소하려면 아래 주석 해제
			// if (CooldownTimerHandle.IsValid())
			// {
			//     TimerManager.ClearTimer(CooldownTimerHandle);
			// }
		}
	}
}
