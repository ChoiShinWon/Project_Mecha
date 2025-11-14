// EnemyAirship.cpp
// 설명:
// - AEnemyAirship 클래스의 구현부.
// - 컴포넌트 초기화, GAS 설정, 체력바 빌보딩, 사망 처리 로직을 포함한다.
//
// Description:
// - Implementation of AEnemyAirship class.
// - Includes component initialization, GAS setup, health bar billboarding, and death handling logic.

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

// 생성자: 컴포넌트 초기화 및 기본 설정.
// - BodyMesh: 비행선 메시 (루트 컴포넌트).
// - HealthBarWidget: 월드 스페이스 체력바.
// - BoostTrail: 부스터 트레일 파티클 (기본 비활성화).
// - Movement: 공중 부유 이동 설정.
// - AbilitySystem: GAS 컴포넌트 초기화 및 복제 설정.
// - AttributeSet: Attribute 관리 (체력, 에너지 등).
// - AutoPossessAI: AI Controller 자동 부여.
//
// Constructor: Initializes components and default settings.
// - BodyMesh: Airship mesh (root component).
// - HealthBarWidget: World-space health bar.
// - BoostTrail: Booster trail particle (default inactive).
// - Movement: Floating pawn movement setup.
// - AbilitySystem: GAS component initialization and replication setup.
// - AttributeSet: Attribute management (health, energy, etc.).
// - AutoPossessAI: AI Controller automatically assigned.
AEnemyAirship::AEnemyAirship()
{
    bReplicates = true;
    PrimaryActorTick.bCanEverTick = true;

    // Mesh (Root)
    BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
    SetRootComponent(BodyMesh);
    BodyMesh->SetCollisionProfileName(TEXT("Pawn"));
    BodyMesh->SetGenerateOverlapEvents(true);

    // HealthBar (World space: 크기/회전 제어용)
    HealthBarWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthBar"));
    HealthBarWidget->SetupAttachment(BodyMesh);
    HealthBarWidget->SetDrawAtDesiredSize(true);
    HealthBarWidget->SetWidgetSpace(EWidgetSpace::World);
    HealthBarWidget->SetRelativeLocation(FVector(0.f, 0.f, 150.f));
    HealthBarWidget->SetTwoSided(true);
    HealthBarWidget->SetPivot(FVector2D(0.5f, 0.0f)); // 위쪽으로 약간 띄워보이게

    // BoostTrail (Cascade 또는 Niagara 변환 가능)
    BoostTrail = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("BoostTrail"));
    BoostTrail->SetupAttachment(BodyMesh);
    BoostTrail->bAutoActivate = false;
    BoostTrail->SetRelativeLocation(FVector(-100.f, 0.f, 30.f));

    // Movement
    Movement = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("Movement"));
    Movement->MaxSpeed = 600.f;
    Movement->Acceleration = 800.f;
    Movement->Deceleration = 800.f;

    // GAS
    AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystem"));
    AbilitySystem->SetIsReplicated(true);
    AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

    AttributeSet = CreateDefaultSubobject<UMechaAttributeSet>(TEXT("AttributeSet"));

    // AI
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}

// 게임 시작 시 호출: ASC 초기화, 초기 Attribute 적용, 체력 변화 델리게이트 바인딩.
// - ASC 초기화: InitAbilityActorInfo 호출.
// - 서버 권한에서만 GE_InitAttributes_Enemy 적용 (MaxHealth, Health 등 초기값 설정).
// - Health = MaxHealth로 보정.
// - 체력 변화 델리게이트 바인딩: 체력이 0 이하가 되면 사망 처리.
// - 체력바 위젯 초기화: ASC와 AttributeSet을 위젯에 전달.
//
// Called at game start: Initializes ASC, applies initial attributes, binds health change delegate.
// - ASC initialization: Calls InitAbilityActorInfo.
// - On server authority, applies GE_InitAttributes_Enemy (sets MaxHealth, Health, etc.).
// - Adjusts Health = MaxHealth.
// - Binds health change delegate: Triggers death if health <= 0.
// - Initializes health bar widget: Passes ASC and AttributeSet to widget.
void AEnemyAirship::BeginPlay()
{
    Super::BeginPlay();

    // ASC init + 초기 어트리뷰트 적용
    if (AbilitySystem)
    {
        AbilitySystem->InitAbilityActorInfo(this, this);

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

        if (AttributeSet)
        {
            // 시작 시 Health = MaxHealth 보정
            AttributeSet->SetHealth(AttributeSet->GetMaxHealth());

            // 체력 변화 델리게이트
            HealthChangedHandle =
                AbilitySystem->GetGameplayAttributeValueChangeDelegate(AttributeSet->GetHealthAttribute())
                .AddUObject(this, &AEnemyAirship::OnHealthChanged);
        }
    }

    // 위젯 초기화(ASC/Attr 주입)
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

// 매 프레임 호출: 체력바 위젯 빌보딩 및 거리 기반 스케일 조정.
// - 카메라를 향해 체력바 위젯을 회전시킨다 (빌보딩).
// - 카메라와의 거리에 따라 위젯 크기를 조정한다 (가까울수록 크게, 멀수록 작게).
//
// Called every frame: Billboards health bar widget and adjusts scale based on distance.
// - Rotates health bar widget to face camera (billboarding).
// - Adjusts widget size based on distance from camera (larger when closer, smaller when farther).
void AEnemyAirship::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!HealthBarWidget) return;

    // 1) 카메라를 향해 빌보딩
    if (const APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0))
    {
        const FVector CamLoc = Cam->GetCameraLocation();
        const FVector WidgetLoc = HealthBarWidget->GetComponentLocation();

        FRotator FaceRot = UKismetMathLibrary::FindLookAtRotation(WidgetLoc, CamLoc);
        FaceRot.Pitch = 0.f; // 필요 시 고정
        FaceRot.Roll = 0.f;
        HealthBarWidget->SetWorldRotation(FaceRot);

        // 2) 거리 기반 스케일 보정 (가까울수록 크게)
        const float Dist = FVector::Dist(CamLoc, WidgetLoc);
        const float Scale = FMath::GetMappedRangeValueClamped(
            FVector2D(800.f, 3000.f),   // 거리 구간
            FVector2D(0.9f, 0.5f),      // 출력 스케일
            Dist
        );
        HealthBarWidget->SetWorldScale3D(FVector(Scale));
    }
}

// 체력이 변경될 때 호출되는 함수: 체력이 0 이하면 사망 처리.
// Called when health changes: Triggers death if health <= 0.
void AEnemyAirship::OnHealthChanged(const FOnAttributeChangeData& Data)
{
    if (Data.NewValue <= 0.f)
    {
        HandleDeath();
    }
}

// 사망 처리 함수: 이펙트/사운드 재생 후 액터 제거.
// - TODO: 사망 이펙트, 사운드, 득점 처리 추가 예정.
// Death handling function: Plays effects/sounds, then destroys actor.
// - TODO: Add death effects, sounds, and scoring.
void AEnemyAirship::HandleDeath()
{
    // TODO: 사망 이펙트/사운드/득점 처리
    Destroy();
}

// 현재 체력 반환 (Blueprint에서 호출 가능).
// Returns current health (callable from Blueprint).
float AEnemyAirship::GetHealth() const
{
    return AttributeSet ? AttributeSet->GetHealth() : 0.f;
}

// 최대 체력 반환 (Blueprint에서 호출 가능).
// Returns maximum health (callable from Blueprint).
float AEnemyAirship::GetMaxHealth() const
{
    return AttributeSet ? AttributeSet->GetMaxHealth() : 0.f;
}
