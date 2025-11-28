// EnemyMecha.cpp
// 적 메카 캐릭터 - GAS, AI, 체력 관리, 미사일/대시 능력

#include "EnemyMecha.h"

#include "AbilitySystemComponent.h"
#include "MechaAttributeSet.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"

#include "AIController.h"
#include "BrainComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Engine/World.h"

#include "MissionManager.h"
#include "Kismet/GameplayStatics.h"

#include "Components/WidgetComponent.h"
#include "EnemyHUDWidget.h"
#include "Blueprint/UserWidget.h"
#include "Animation/AnimInstance.h"

// ========================================
// 생성자
// ========================================
AEnemyMecha::AEnemyMecha()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = true;
	CurrentTarget = nullptr;

	// ========== GAS 초기화 ==========
	AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystem"));
	AbilitySystem->SetIsReplicated(true);
	AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	AttributeSet = CreateDefaultSubobject<UMechaAttributeSet>(TEXT("AttributeSet"));

	// ========== AI 파라미터 기본값 ==========
	AggroRadius = 2500.f;
	MeleeRange = 200.f;
	RangedRange = 1500.f;
	PatrolRadius = 800.f;
	LeashDistance = 5000.f;

	// 발사 소켓 기본값
	FireSocketName = TEXT("FireSocket");

	// ========== Enemy HUD 위젯 컴포넌트 ==========
	EnemyHUDWidgetComp = CreateDefaultSubobject<UWidgetComponent>(TEXT("EnemyHUDWidgetComp"));
	EnemyHUDWidgetComp->SetupAttachment(GetRootComponent());

	// 머리 위에 배치
	EnemyHUDWidgetComp->SetRelativeLocation(FVector(0.f, 0.f, 200.f));
	EnemyHUDWidgetComp->SetWidgetSpace(EWidgetSpace::Screen);
	EnemyHUDWidgetComp->SetDrawSize(FVector2D(200.f, 40.f));

	// ========== HitReact 기본값 ==========
	HitReactInterval = 0.4f;
	bCanPlayHitReact = true;
}

// ========================================
// ASC 반환
// ========================================
UAbilitySystemComponent* AEnemyMecha::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}

// ========================================
// BeginPlay - 초기화
// ========================================
void AEnemyMecha::BeginPlay()
{
	Super::BeginPlay();

	if (AbilitySystem)
	{
		AbilitySystem->InitAbilityActorInfo(this, this);

		// ========== 서버에서 능력 부여 ==========
		if (HasAuthority())
		{
			// 미사일 능력 등록
			if (MissileAbilityClass_Enemy)
			{
				AbilitySystem->GiveAbility(
					FGameplayAbilitySpec(MissileAbilityClass_Enemy, 1, 0)
				);
			}

			// 대시 능력 등록
			if (DashAbilityClass_Enemy)
			{
				AbilitySystem->GiveAbility(
					FGameplayAbilitySpec(DashAbilityClass_Enemy, 1, 1)
				);
			}

			// 미션 매니저 찾기
			if (MissionManager == nullptr)
			{
				AActor* Found = UGameplayStatics::GetActorOfClass(GetWorld(), AMissionManager::StaticClass());
				if (Found)
				{
					MissionManager = Cast<AMissionManager>(Found);
					UE_LOG(LogTemp, Log, TEXT("EnemyMecha: Found MissionManager %s"), *MissionManager->GetName());
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("EnemyMecha: MissionManager not found in level"));
				}
			}

			// HUD 초기화 (서버)
			if (EnemyHUDWidgetComp && AbilitySystem && AttributeSet)
			{
				UUserWidget* WidgetObject = EnemyHUDWidgetComp->GetUserWidgetObject();
				if (UEnemyHUDWidget* EnemyHUD = Cast<UEnemyHUDWidget>(WidgetObject))
				{
					EnemyHUD->InitWithASC(AbilitySystem, AttributeSet);
				}
			}
		}
	}

	// 스폰 위치 저장 (AI 패트롤 기준점)
	HomeLocation = GetActorLocation();

	// 스탯 초기화
	InitializeAttributes();

	// ========== 체력 변경 델리게이트 바인딩 ==========
	if (AbilitySystem && AttributeSet)
	{
		HealthChangedHandle =
			AbilitySystem
			->GetGameplayAttributeValueChangeDelegate(UMechaAttributeSet::GetHealthAttribute())
			.AddUObject(this, &AEnemyMecha::OnHealthChanged);
	}

	// HUD 초기화 (클라이언트 포함)
	if (EnemyHUDWidgetComp && AbilitySystem && AttributeSet)
	{
		UUserWidget* WidgetObject = EnemyHUDWidgetComp->GetUserWidgetObject();
		if (UEnemyHUDWidget* EnemyHUD = Cast<UEnemyHUDWidget>(WidgetObject))
		{
			EnemyHUD->InitWithASC(AbilitySystem, AttributeSet);
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("AEnemyMecha::BeginPlay - %s"), *GetName());
}

// ========================================
// 스탯 초기화
// ========================================
void AEnemyMecha::InitializeAttributes()
{
	if (!AbilitySystem || !InitAttributesEffect)
	{
		UE_LOG(LogTemp, Warning, TEXT("AEnemyMecha::InitializeAttributes - Missing ASC or InitAttributesEffect on %s"), *GetName());
		return;
	}

	// GE 적용
	FGameplayEffectContextHandle ContextHandle = AbilitySystem->MakeEffectContext();
	ContextHandle.AddSourceObject(this);

	FGameplayEffectSpecHandle SpecHandle =
		AbilitySystem->MakeOutgoingSpec(InitAttributesEffect, 1.f, ContextHandle);

	if (SpecHandle.IsValid())
	{
		AbilitySystem->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());

		// 체력을 최대치로 설정
		if (AttributeSet)
		{
			AttributeSet->SetHealth(AttributeSet->GetMaxHealth());
		}

		UE_LOG(LogTemp, Warning, TEXT("AEnemyMecha::InitializeAttributes - Applied InitAttributesEffect on %s (Health=%f / MaxHealth=%f)"),
			*GetName(),
			AttributeSet ? AttributeSet->GetHealth() : -1.f,
			AttributeSet ? AttributeSet->GetMaxHealth() : -1.f);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("AEnemyMecha::InitializeAttributes - Spec invalid on %s"), *GetName());
	}
}

// ========================================
// Getter 함수들
// ========================================
float AEnemyMecha::GetHealth() const
{
	return AttributeSet ? AttributeSet->GetHealth() : 0.f;
}

float AEnemyMecha::GetMaxHealth() const
{
	return AttributeSet ? AttributeSet->GetMaxHealth() : 0.f;
}

void AEnemyMecha::ReInitializeAttributes()
{
	InitializeAttributes();
}

// ========================================
// 사망 처리
// ========================================
void AEnemyMecha::HandleDeath()
{
	// ========== AI/BT 정지 ==========
	if (AAIController* AICon = Cast<AAIController>(GetController()))
	{
		if (UBrainComponent* Brain = AICon->GetBrainComponent())
		{
			Brain->StopLogic(TEXT("Dead"));
		}

		if (UBlackboardComponent* BB = AICon->GetBlackboardComponent())
		{
			BB->SetValueAsBool(TEXT("IsDead"), true);
		}
	}

	// ========== 이동 정지 ==========
	if (auto* Move = GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->DisableMovement();
	}

	// ========== 사망 애니메이션 재생 ==========
	if (DeathMontage)
	{
		if (UAnimInstance* AnimInst = GetMesh()->GetAnimInstance())
		{
			AnimInst->Montage_Play(DeathMontage);
		}
	}

	// ========== HUD 정리 ==========
	if (EnemyHUDWidgetComp)
	{
		if (UUserWidget* WidgetObject = EnemyHUDWidgetComp->GetUserWidgetObject())
		{
			if (UEnemyHUDWidget* EnemyHUD = Cast<UEnemyHUDWidget>(WidgetObject))
			{
				EnemyHUD->OnOwnerDead();
			}
		}
	}

	// ========== 미션 매니저에 킬 보고 ==========
	if (MissionManager)
	{
		MissionManager->NotifyEnemyKilled(this);
	}

	// 1초 후 제거
	SetLifeSpan(1.0f);
}

// ========================================
// 체력 변경 콜백
// ========================================
void AEnemyMecha::OnHealthChanged(const FOnAttributeChangeData& Data)
{
	const float NewHealth = Data.NewValue;
	const float MaxHealth = AttributeSet ? AttributeSet->GetMaxHealth() : 1.f;

	// ========== HUD 체력바 업데이트 ==========
	if (EnemyHUDWidgetComp)
	{
		if (UUserWidget* WidgetObject = EnemyHUDWidgetComp->GetUserWidgetObject())
		{
			if (UEnemyHUDWidget* EnemyHUD = Cast<UEnemyHUDWidget>(WidgetObject))
			{
				EnemyHUD->ApplyHealth(NewHealth, MaxHealth);
			}
		}
	}

	// 이미 죽었으면 더 처리 안 함
	if (bIsDead)
	{
		return;
	}

	// ========== 체력이 0 이하로 떨어지면 사망 ==========
	if (NewHealth <= 0.f)
	{
		bIsDead = true;

		// Dead 태그 추가
		if (AbilitySystem)
		{
			AbilitySystem->AddLooseGameplayTag(
				FGameplayTag::RequestGameplayTag(TEXT("State.Dead"))
			);
		}

		HandleDeath();
	}
}

// ========================================
// 피격 리액션 재생
// ========================================
void AEnemyMecha::PlayHitReact()
{
	// 죽었거나, 몽타주가 없거나, 쿨타임 중이면 무시
	if (bIsDead || !HitReactMontage || !bCanPlayHitReact)
	{
		return;
	}

	bCanPlayHitReact = false;

	UAnimInstance* AnimInst = (GetMesh() ? GetMesh()->GetAnimInstance() : nullptr);
	if (!AnimInst)
	{
		ResetHitReactWindow();
		return;
	}

	// 피격 애니메이션 재생
	AnimInst->Montage_Play(HitReactMontage, 1.0f);

	// 일정 시간 후 다시 재생 가능하도록
	GetWorldTimerManager().ClearTimer(TimerHandle_HitReactInterval);
	GetWorldTimerManager().SetTimer(
		TimerHandle_HitReactInterval,
		this,
		&AEnemyMecha::ResetHitReactWindow,
		HitReactInterval,
		false
	);
}

void AEnemyMecha::ResetHitReactWindow()
{
	bCanPlayHitReact = true;
}

// ========================================
// 능력 발동 함수들 (AI/BT에서 호출)
// ========================================

// 미사일 능력 발동
void AEnemyMecha::FireMissileAbility()
{
	if (!AbilitySystem || !MissileAbilityClass_Enemy)
		return;

	AbilitySystem->TryActivateAbilityByClass(MissileAbilityClass_Enemy);
}

// 미사일 직접 발사 (애님 노티파이에서 호출)
void AEnemyMecha::FireMissileFromNotify()
{
	if (!MissileClass_Enemy || !CurrentTarget)
	{
		UE_LOG(LogTemp, Warning, TEXT("AEnemyMecha::FireMissileFromNotify - No MissileClass_Enemy or CurrentTarget on %s"), *GetName());
		return;
	}

	USkeletalMeshComponent* SkeletalMeshComp = GetMesh();
	if (!SkeletalMeshComp)
	{
		UE_LOG(LogTemp, Warning, TEXT("AEnemyMecha::FireMissileFromNotify - No Mesh on %s"), *GetName());
		return;
	}

	FVector SpawnLocation;
	FRotator SpawnRotation;

	// ========== 발사 위치 및 방향 계산 ==========
	if (SkeletalMeshComp->DoesSocketExist(FireSocketName))
	{
		// 소켓이 있으면 소켓 위치에서
		SpawnLocation = SkeletalMeshComp->GetSocketLocation(FireSocketName);
		const FVector ToTarget = CurrentTarget->GetActorLocation() - SpawnLocation;
		SpawnRotation = ToTarget.Rotation();
	}
	else
	{
		// 소켓이 없으면 앞쪽에서
		SpawnLocation = GetActorLocation() + GetActorForwardVector() * 100.f;
		SpawnRotation = GetActorRotation();
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// ========== 미사일 스폰 ==========
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = this;

	AActor* Missile = World->SpawnActor<AActor>(
		MissileClass_Enemy,
		SpawnLocation,
		SpawnRotation,
		SpawnParams
	);

	// ========== 유도 미사일 설정 ==========
	if (Missile)
	{
		if (UProjectileMovementComponent* MoveComp =
			Missile->FindComponentByClass<UProjectileMovementComponent>())
		{
			MoveComp->bIsHomingProjectile = true;

			if (CurrentTarget->GetRootComponent())
			{
				MoveComp->HomingTargetComponent = CurrentTarget->GetRootComponent();
			}

			MoveComp->Velocity = SpawnRotation.Vector() * MoveComp->InitialSpeed;
		}
	}
}

// 대시 능력 발동
void AEnemyMecha::FireDashAbility()
{
	if (!AbilitySystem || !DashAbilityClass_Enemy)
		return;

	AbilitySystem->TryActivateAbilityByClass(DashAbilityClass_Enemy);
}
