// EnemyAirship.cpp
// 적 비행선 - GAS 기반 체력 관리, 빌보딩 체력바

#include "EnemyAirship.h"

#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Particles/ParticleSystemComponent.h"
#include "GameFramework/FloatingPawnMovement.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "MechaAttributeSet.h"

#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Blueprint/UserWidget.h"
#include "WBP_EnemyHealth.h"

// ========================================
// 생성자
// ========================================
AEnemyAirship::AEnemyAirship()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = true;

	// ========== 메시 (루트) ==========
	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	SetRootComponent(BodyMesh);
	BodyMesh->SetCollisionProfileName(TEXT("Pawn"));
	BodyMesh->SetGenerateOverlapEvents(true);

	// ========== 체력바 위젯 (월드 스페이스) ==========
	HealthBarWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthBar"));
	HealthBarWidget->SetupAttachment(BodyMesh);
	HealthBarWidget->SetDrawAtDesiredSize(true);
	HealthBarWidget->SetWidgetSpace(EWidgetSpace::World);
	HealthBarWidget->SetRelativeLocation(FVector(0.f, 0.f, 150.f));
	HealthBarWidget->SetTwoSided(true);
	HealthBarWidget->SetPivot(FVector2D(0.5f, 0.0f));

	// ========== 부스터 트레일 파티클 ==========
	BoostTrail = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("BoostTrail"));
	BoostTrail->SetupAttachment(BodyMesh);
	BoostTrail->bAutoActivate = false;
	BoostTrail->SetRelativeLocation(FVector(-100.f, 0.f, 30.f));

	// ========== 이동 컴포넌트 ==========
	Movement = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("Movement"));
	Movement->MaxSpeed = 600.f;
	Movement->Acceleration = 800.f;
	Movement->Deceleration = 800.f;

	// ========== GAS 초기화 ==========
	AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystem"));
	AbilitySystem->SetIsReplicated(true);
	AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	AttributeSet = CreateDefaultSubobject<UMechaAttributeSet>(TEXT("AttributeSet"));

	// ========== AI 자동 빙의 ==========
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}

// ========================================
// BeginPlay - 초기화
// ========================================
void AEnemyAirship::BeginPlay()
{
	Super::BeginPlay();

	// ========== ASC 초기화 ==========
	if (AbilitySystem)
	{
		AbilitySystem->InitAbilityActorInfo(this, this);

		// ========== 서버에서 스탯 초기화 ==========
		if (HasAuthority() && GE_InitAttributes_Enemy)
		{
			FGameplayEffectContextHandle Ctx = AbilitySystem->MakeEffectContext();
			Ctx.AddSourceObject(this);

			FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(GE_InitAttributes_Enemy, 1.f, Ctx);
			if (Spec.IsValid())
			{
				AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
			}
		}

		// ========== 체력 최대치로 설정 ==========
		if (AttributeSet)
		{
			AttributeSet->SetHealth(AttributeSet->GetMaxHealth());

			// 체력 변경 델리게이트 바인딩
			HealthChangedHandle =
				AbilitySystem->GetGameplayAttributeValueChangeDelegate(AttributeSet->GetHealthAttribute())
				.AddUObject(this, &AEnemyAirship::OnHealthChanged);
		}
	}

	// ========== 체력바 위젯 초기화 ==========
	if (HealthBarWidget)
	{
		if (UUserWidget* W = HealthBarWidget->GetUserWidgetObject())
		{
			if (auto* EH = Cast<UWBP_EnemyHealth>(W))
			{
				EH->InitWithASC(AbilitySystem, AttributeSet);
			}
		}
	}
}

// ========================================
// Tick - 체력바 빌보딩 및 스케일 조정
// ========================================
void AEnemyAirship::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!HealthBarWidget) return;

	// ========== 카메라를 향해 체력바 회전 ==========
	if (const APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		const FVector CamLoc = Cam->GetCameraLocation();
		const FVector WidgetLoc = HealthBarWidget->GetComponentLocation();

		FRotator FaceRot = UKismetMathLibrary::FindLookAtRotation(WidgetLoc, CamLoc);
		FaceRot.Pitch = 0.f;
		FaceRot.Roll = 0.f;
		HealthBarWidget->SetWorldRotation(FaceRot);

		// ========== 거리 기반 스케일 조정 ==========
		// 가까우면 크게, 멀면 작게
		const float Dist = FVector::Dist(CamLoc, WidgetLoc);
		const float Scale = FMath::GetMappedRangeValueClamped(
			FVector2D(800.f, 3000.f),   // 거리 구간
			FVector2D(0.9f, 0.5f),      // 스케일 범위
			Dist
		);
		HealthBarWidget->SetWorldScale3D(FVector(Scale));
	}
}

// ========================================
// 체력 변경 콜백
// ========================================
void AEnemyAirship::OnHealthChanged(const FOnAttributeChangeData& Data)
{
	// 체력이 0 이하로 떨어지면 사망
	if (Data.NewValue <= 0.f)
	{
		HandleDeath();
	}
}

// ========================================
// 사망 처리
// ========================================
void AEnemyAirship::HandleDeath()
{
	// TODO: 폭발 이펙트, 사운드, 점수 추가 등
	Destroy();
}

// ========================================
// Getter 함수들
// ========================================
float AEnemyAirship::GetHealth() const
{
	return AttributeSet ? AttributeSet->GetHealth() : 0.f;
}

float AEnemyAirship::GetMaxHealth() const
{
	return AttributeSet ? AttributeSet->GetMaxHealth() : 0.f;
}
