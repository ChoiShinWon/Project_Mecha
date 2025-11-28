// EnemyMecha.cpp

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

AEnemyMecha::AEnemyMecha()
{
    bReplicates = true;
    PrimaryActorTick.bCanEverTick = true;
    CurrentTarget = nullptr;

    // === GAS 세팅 ===
    AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystem"));
    AbilitySystem->SetIsReplicated(true);
    AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

    AttributeSet = CreateDefaultSubobject<UMechaAttributeSet>(TEXT("AttributeSet"));

    // === AI 기본값 ===
    AggroRadius = 2500.f;
    MeleeRange = 200.f;
    RangedRange = 1500.f;
    PatrolRadius = 800.f;
    LeashDistance = 5000.f;

    // 발사 소켓 기본값
    FireSocketName = TEXT("FireSocket");

    // === Enemy HUD 위젯 컴포넌트 생성 ===
    EnemyHUDWidgetComp = CreateDefaultSubobject<UWidgetComponent>(TEXT("EnemyHUDWidgetComp"));
    EnemyHUDWidgetComp->SetupAttachment(GetRootComponent());

    // 머리 위 정도로 위치
    EnemyHUDWidgetComp->SetRelativeLocation(FVector(0.f, 0.f, 200.f));
    EnemyHUDWidgetComp->SetWidgetSpace(EWidgetSpace::Screen);
    EnemyHUDWidgetComp->SetDrawSize(FVector2D(200.f, 40.f));

    // HitReact 기본값
    HitReactInterval = 0.4f;
    bCanPlayHitReact = true;
}

UAbilitySystemComponent* AEnemyMecha::GetAbilitySystemComponent() const
{
    return AbilitySystem;
}

void AEnemyMecha::BeginPlay()
{
    Super::BeginPlay();

    if (AbilitySystem)
    {
        AbilitySystem->InitAbilityActorInfo(this, this);

        if (HasAuthority())
        {
            // === 미사일 어빌리티 등록 ===
            if (MissileAbilityClass_Enemy)
            {
                AbilitySystem->GiveAbility(
                    FGameplayAbilitySpec(MissileAbilityClass_Enemy, 1, 0)
                );
            }

            // Dash 어빌리티 등록
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

            // 서버에서 HUD Init
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

    // 스폰 위치 저장 (패트롤 기준)
    HomeLocation = GetActorLocation();

    // 스탯 초기화 (GE_InitAttributes_Enemy)
    InitializeAttributes();

    // Health 변화 감지
    if (AbilitySystem && AttributeSet)
    {
        HealthChangedHandle =
            AbilitySystem
            ->GetGameplayAttributeValueChangeDelegate(UMechaAttributeSet::GetHealthAttribute())
            .AddUObject(this, &AEnemyMecha::OnHealthChanged);
    }

    // 클라이언트 포함 HUD Init
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

void AEnemyMecha::InitializeAttributes()
{
    if (!AbilitySystem || !InitAttributesEffect)
    {
        UE_LOG(LogTemp, Warning, TEXT("AEnemyMecha::InitializeAttributes - Missing ASC or InitAttributesEffect on %s"), *GetName());
        return;
    }

    FGameplayEffectContextHandle ContextHandle = AbilitySystem->MakeEffectContext();
    ContextHandle.AddSourceObject(this);

    FGameplayEffectSpecHandle SpecHandle =
        AbilitySystem->MakeOutgoingSpec(InitAttributesEffect, 1.f, ContextHandle);

    if (SpecHandle.IsValid())
    {
        AbilitySystem->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());

        // Health = MaxHealth로 맞춰주기 (초기 풀피)
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

void AEnemyMecha::HandleDeath()
{
    // AI / BT 정지
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

    // 이동/제어 끊기
    if (auto* Move = GetCharacterMovement())
    {
        Move->StopMovementImmediately();
        Move->DisableMovement();
    }

    // 사망 몽타주 재생
    if (DeathMontage)
    {
        if (UAnimInstance* AnimInst = GetMesh()->GetAnimInstance())
        {
            AnimInst->Montage_Play(DeathMontage);
        }
    }

    // HUD 정리
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

    // 미션 매니저에 킬 보고
    if (MissionManager)
    {
        MissionManager->NotifyEnemyKilled(this);
    }

    // 몇 초 뒤에 삭제 
    SetLifeSpan(1.0f);
}

void AEnemyMecha::OnHealthChanged(const FOnAttributeChangeData& Data)
{
    const float NewHealth = Data.NewValue;
    const float MaxHealth = AttributeSet ? AttributeSet->GetMaxHealth() : 1.f;

    // === 1) HUD에 체력 갱신 전달 ===
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

    // === 2) 이미 죽었으면 더 처리할 필요 없음 ===
    if (bIsDead)
    {
        return;
    }

    // === 3) 0 이하로 떨어지면 죽음 처리 ===
    if (NewHealth <= 0.f)
    {
        bIsDead = true;

        // Dead 태그 추가 (기존 유지)
        if (AbilitySystem)
        {
            AbilitySystem->AddLooseGameplayTag(
                FGameplayTag::RequestGameplayTag(TEXT("State.Dead"))
            );
        }

        HandleDeath();
    }
}

// === HitReact 재생 (Projectile에서 직접 호출) ===
void AEnemyMecha::PlayHitReact()
{
    // 죽었거나, 몽타주 없거나, 쿨타임 중이면 무시
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

    // 이동 완전 정지는 하지 않는다. (AI가 계속 행동 가능)
    // 필요하면 나중에 감속이나 특정 상황에서만 정지 추가.

    AnimInst->Montage_Play(HitReactMontage, 1.0f);

    // HitReactInterval 동안 추가 경직 막기
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

// === Ability 호출들 ===

void AEnemyMecha::FireMissileAbility()
{
    if (!AbilitySystem || !MissileAbilityClass_Enemy)
        return;

    AbilitySystem->TryActivateAbilityByClass(MissileAbilityClass_Enemy);
}

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

    if (SkeletalMeshComp->DoesSocketExist(FireSocketName))
    {
        SpawnLocation = SkeletalMeshComp->GetSocketLocation(FireSocketName);
        const FVector ToTarget = CurrentTarget->GetActorLocation() - SpawnLocation;
        SpawnRotation = ToTarget.Rotation();
    }
    else
    {
        SpawnLocation = GetActorLocation() + GetActorForwardVector() * 100.f;
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

// Dash 발동 함수
void AEnemyMecha::FireDashAbility()
{
    if (!AbilitySystem || !DashAbilityClass_Enemy)
        return;

    AbilitySystem->TryActivateAbilityByClass(DashAbilityClass_Enemy);
}
