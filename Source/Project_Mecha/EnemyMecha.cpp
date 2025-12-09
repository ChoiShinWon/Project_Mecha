// EnemyMecha.cpp
// 적 메카 캐릭터 - GAS, AI, 체력 관리, 미사일/대시/호버/보스 패턴 능력

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
#include "BossHealthWidget.h"
#include "WBP_GameComplete.h"
#include "Blueprint/UserWidget.h"
#include "Animation/AnimInstance.h"
#include "Particles/ParticleSystemComponent.h"

// ========================================
// 생성자
// ========================================
AEnemyMecha::AEnemyMecha()
{
    PrimaryActorTick.bCanEverTick = true;
    CurrentTarget = nullptr;

    // ========== GAS 초기화 ==========
    AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystem"));
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

    // ========== Hover Particle 소켓 기본값 ==========
    HoverParticleSockets.Add(TEXT("Foot_L"));
    HoverParticleSockets.Add(TEXT("Foot_R"));
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

        // ========== 능력 부여 ==========
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

            // 호버 능력 등록
            if (HoverAbilityClass_Enemy)
            {
                AbilitySystem->GiveAbility(
                    FGameplayAbilitySpec(HoverAbilityClass_Enemy, 1, 2)
                );
            }

            // === Boss 미사일 레인 Ability 등록 ===
            if (bIsBoss && BossMissileRainAbilityClass)
            {
                AbilitySystem->GiveAbility(
                    FGameplayAbilitySpec(BossMissileRainAbilityClass, 1, 3)
                );
            }

            // 미션 매니저 찾기
            if (MissionManager == nullptr)
            {
                AActor* Found = UGameplayStatics::GetActorOfClass(GetWorld(), AMissionManager::StaticClass());
                if (Found)
                {
                    MissionManager = Cast<AMissionManager>(Found);
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

    // ========== 보스 체력바 생성 ==========
    if (bIsBoss)
    {
        CreateBossHealthWidget();
    }

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

    // ========== Hover Particle 컴포넌트 동적 생성 ==========
    if (HoverParticleSystem && GetMesh())
    {
        for (const FName& SocketName : HoverParticleSockets)
        {
            if (GetMesh()->DoesSocketExist(SocketName))
            {
                UParticleSystemComponent* ParticleComp = NewObject<UParticleSystemComponent>(this);
                if (ParticleComp)
                {
                    ParticleComp->SetTemplate(HoverParticleSystem);
                    ParticleComp->bAutoActivate = false;
                    ParticleComp->SetupAttachment(GetMesh(), SocketName);
                    ParticleComp->SetRelativeScale3D(HoverParticleScale);
                    ParticleComp->SetRelativeRotation(HoverParticleRotation);
                    ParticleComp->RegisterComponent();

                    HoverParticleComponents.Add(ParticleComp);
                }
            }
        }
    }
}

// ========================================
// 스탯 초기화
// ========================================
void AEnemyMecha::InitializeAttributes()
{
    if (!AbilitySystem || !InitAttributesEffect)
    {
        return;
    }

    FGameplayEffectContextHandle ContextHandle = AbilitySystem->MakeEffectContext();
    ContextHandle.AddSourceObject(this);

    FGameplayEffectSpecHandle SpecHandle =
        AbilitySystem->MakeOutgoingSpec(InitAttributesEffect, 1.f, ContextHandle);

    if (SpecHandle.IsValid())
    {
        AbilitySystem->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());

        if (AttributeSet)
        {
            AttributeSet->SetHealth(AttributeSet->GetMaxHealth());
        }
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
            AnimInst->OnMontageBlendingOut.AddDynamic(this, &AEnemyMecha::OnDeathMontageEnded);
        }
    }
    else
    {
        SetLifeSpan(0.1f);
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

    // ========== 보스 체력바 정리 ==========
    if (bIsBoss && BossHealthWidget)
    {
        BossHealthWidget->OnBossDead();
        BossHealthWidget = nullptr;
    }

    // ========== 보스 사망 슬로우 모션 ==========
    if (bIsBoss && bUseDeathSlowMotion)
    {
        StartDeathSlowMotion();
    }

    // ========== 미션 매니저에 킬 보고 ==========
    if (MissionManager)
    {
        MissionManager->NotifyEnemyKilled(this);
    }
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

    // ========== 보스 체력바 업데이트 ==========
    if (bIsBoss)
    {
        UpdateBossHealthWidget(NewHealth, MaxHealth);
    }

    if (bIsDead)
    {
        return;
    }

    if (NewHealth <= 0.f)
    {
        bIsDead = true;

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

    AnimInst->Montage_Play(HitReactMontage, 1.0f);

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
        return;
    }

    USkeletalMeshComponent* SkeletalMeshComp = GetMesh();
    if (!SkeletalMeshComp)
    {
        return;
    }

    FVector SpawnLocation;
    FRotator SpawnRotation;

    if (SkeletalMeshComp->DoesSocketExist(FireSocketName))
    {
        SpawnLocation = SkeletalMeshComp->GetSocketLocation(FireSocketName);
        const FVector ToTarget = CurrentTarget->GetActorLocation() - SpawnLocation;
        SpawnRotation = ToTarget.Rotation();
    }
    else
    {
        SpawnLocation = GetActorLocation() + GetActorForwardVector() * FallbackSpawnOffset;
        SpawnRotation = GetActorRotation();
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.Instigator = this;

    AActor* Missile = World->SpawnActor<AActor>(
        MissileClass_Enemy,
        SpawnLocation,
        SpawnRotation,
        SpawnParams
    );

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

// 호버 능력 발동
void AEnemyMecha::FireHoverAbility()
{
    if (!AbilitySystem || !HoverAbilityClass_Enemy)
        return;

    AbilitySystem->TryActivateAbilityByClass(HoverAbilityClass_Enemy);
}

// Boss 미사일 레인 패턴 Ability 발동
void AEnemyMecha::FireBossMissileRainAbility()
{
    if (!AbilitySystem || !BossMissileRainAbilityClass)
    {
        return;
    }

    // 이미 죽은 상태면 패턴 발동하지 않음
    if (AbilitySystem->HasMatchingGameplayTag(
        FGameplayTag::RequestGameplayTag(TEXT("State.Dead"))))
    {
        return;
    }

    AbilitySystem->TryActivateAbilityByClass(BossMissileRainAbilityClass);
}

// ========================================
// Hover Particle 제어
// ========================================

void AEnemyMecha::ActivateHoverParticles()
{
    for (UParticleSystemComponent* ParticleComp : HoverParticleComponents)
    {
        if (ParticleComp && !ParticleComp->IsActive())
        {
            ParticleComp->Activate(true);
        }
    }
}

void AEnemyMecha::DeactivateHoverParticles()
{
    for (UParticleSystemComponent* ParticleComp : HoverParticleComponents)
    {
        if (ParticleComp && ParticleComp->IsActive())
        {
            ParticleComp->Deactivate();
        }
    }
}

void AEnemyMecha::SetHoverParticleScale(FVector NewScale)
{
    HoverParticleScale = NewScale;

    for (UParticleSystemComponent* ParticleComp : HoverParticleComponents)
    {
        if (ParticleComp)
        {
            ParticleComp->SetRelativeScale3D(NewScale);
        }
    }
}

void AEnemyMecha::SetHoverParticleRotation(FRotator NewRotation)
{
    HoverParticleRotation = NewRotation;

    for (UParticleSystemComponent* ParticleComp : HoverParticleComponents)
    {
        if (ParticleComp)
        {
            ParticleComp->SetRelativeRotation(NewRotation);
        }
    }
}

// ========================================
// 보스 체력바 위젯 생성
// ========================================
void AEnemyMecha::CreateBossHealthWidget()
{
    if (!bIsBoss || !BossHealthWidgetClass)
    {
        return;
    }

    APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    if (!PC)
    {
        return;
    }

    BossHealthWidget = CreateWidget<UBossHealthWidget>(PC, BossHealthWidgetClass);
    if (BossHealthWidget)
    {
        BossHealthWidget->AddToViewport(100);
        BossHealthWidget->InitWithBoss(AbilitySystem, AttributeSet, BossName);
        BossHealthWidget->ShowBossHealth();
    }
}

// ========================================
// 보스 체력바 업데이트
// ========================================
void AEnemyMecha::UpdateBossHealthWidget(float NewHealth, float MaxHealth)
{
    if (BossHealthWidget)
    {
        BossHealthWidget->ApplyHealth(NewHealth, MaxHealth);
    }
}

// ========================================
// 보스 사망 슬로우 모션 시작
// ========================================
void AEnemyMecha::StartDeathSlowMotion()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    UGameplayStatics::SetGlobalTimeDilation(World, DeathSlowMotionScale);

    const float RealTimeDuration = DeathSlowMotionDuration * DeathSlowMotionScale;

    World->GetTimerManager().SetTimer(
        TimerHandle_SlowMotionRestore,
        this,
        &AEnemyMecha::RestoreNormalTime,
        RealTimeDuration,
        false
    );
}

// ========================================
// 슬로우 모션 복원 (원래 속도로)
// ========================================
void AEnemyMecha::RestoreNormalTime()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    UGameplayStatics::SetGlobalTimeDilation(World, 1.0f);

    if (bIsBoss && GameCompleteWidgetClass)
    {
        APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
        if (PC && !GameCompleteWidget)
        {
            GameCompleteWidget = CreateWidget<UWBP_GameComplete>(PC, GameCompleteWidgetClass);
            if (GameCompleteWidget)
            {
                GameCompleteWidget->AddToViewport(200);
                GameCompleteWidget->ShowGameComplete(2.0f);
            }
        }
    }
}

// ========================================
// Death Montage 종료 콜백
// ========================================
void AEnemyMecha::OnDeathMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
    if (Montage == DeathMontage)
    {
        if (UAnimInstance* AnimInst = GetMesh()->GetAnimInstance())
        {
            AnimInst->OnMontageBlendingOut.RemoveDynamic(this, &AEnemyMecha::OnDeathMontageEnded);
        }

        if (GetMesh())
        {
            GetMesh()->bPauseAnims = true;
        }

        Destroy();
    }
}

// ========================================
// Blackboard 전투 상태 초기화 (디버깅/복구용)
// ========================================
void AEnemyMecha::ResetBlackboardCombatState()
{
    if (AAIController* AICon = Cast<AAIController>(GetController()))
    {
        if (UBlackboardComponent* BB = AICon->GetBlackboardComponent())
        {
            BB->SetValueAsBool(TEXT("IsAttacking"), false);
            BB->SetValueAsBool(TEXT("IsDashing"), false);
            BB->SetValueAsBool(TEXT("ShouldDash"), false);

            BB->SetValueAsBool(TEXT("InHookRange"), false);
            BB->SetValueAsBool(TEXT("InUppercutRange"), false);
            BB->SetValueAsBool(TEXT("InFireRange"), false);
        }
    }
}
