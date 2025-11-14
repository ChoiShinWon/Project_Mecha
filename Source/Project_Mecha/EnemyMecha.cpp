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
}



void AEnemyMecha::BeginPlay()
{
    Super::BeginPlay();

    if (AbilitySystem)
    {
        AbilitySystem->InitAbilityActorInfo(this, this);

        if (HasAuthority() && MissileAbilityClass_Enemy)
        {
            AbilitySystem->GiveAbility(
                FGameplayAbilitySpec(MissileAbilityClass_Enemy, 1, 0)
            );
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
            ->GetGameplayAttributeValueChangeDelegate(AttributeSet->GetHealthAttribute())
            .AddUObject(this, &AEnemyMecha::OnHealthChanged);
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

    //  이동/제어 끊기 (선택)
    if (auto* Move = GetCharacterMovement())
    {
        Move->StopMovementImmediately();
        Move->DisableMovement();
    }

    //  사망 몽타주 재생
    if (DeathMontage)
    {
        if (UAnimInstance* AnimInst = GetMesh()->GetAnimInstance())
        {
            AnimInst->Montage_Play(DeathMontage);
        }
    }

    //  몇 초 뒤에 삭제 
    SetLifeSpan(1.0f);   // 5초 뒤 Destroy
}


void AEnemyMecha::OnHealthChanged(const FOnAttributeChangeData& Data)
{
    // 이미 죽었으면 중복 처리 방지
    if (bIsDead)
    {
        return;
    }

    if (Data.NewValue <= 0.f)
    {
        bIsDead = true;

        // Dead 태그 추가 (나중에 BT/GA에서 체크 가능)
        if (AbilitySystem)
        {
            AbilitySystem->AddLooseGameplayTag(
                FGameplayTag::RequestGameplayTag(TEXT("State.Dead"))
            );
        }

        HandleDeath();
    }
}


void AEnemyMecha::FireMissileAbility()
{
    if (!AbilitySystem || !MissileAbilityClass_Enemy)
        return;

    AbilitySystem->TryActivateAbilityByClass(MissileAbilityClass_Enemy);
}
