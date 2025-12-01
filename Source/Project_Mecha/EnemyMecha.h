// EnemyMecha.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "EnemyMecha.generated.h"

class UAbilitySystemComponent;
class UMechaAttributeSet;
class UGameplayEffect;
class UBehaviorTree;
class UGameplayAbility;
class UEnemyHUDWidget;
class UWidgetComponent;
class AMissionManager;
class UAnimMontage;
struct FOnAttributeChangeData;

UCLASS()
class PROJECT_MECHA_API AEnemyMecha
    : public ACharacter
    , public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    AEnemyMecha();

    // === AbilitySystemInterface 구현 ===
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

    // ASC
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
    UAbilitySystemComponent* AbilitySystem;

    // Enemy가 사용할 미사일 Ability 클래스 (GA_MissileFire_Enemy)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS")
    TSubclassOf<UGameplayAbility> MissileAbilityClass_Enemy;

    // Enemy가 실제로 발사할 미사일 액터 클래스 (BP_EnemyMissile)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = "true"))
    TSubclassOf<AActor> MissileClass_Enemy;

    // 발사 소켓 이름 (FireSocket)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = "true"))
    FName FireSocketName = TEXT("FireSocket");

    // AI가 채워줄 타겟
    UPROPERTY(BlueprintReadWrite, Category = "AI")
    AActor* CurrentTarget;

    // Behavior Tree 등에서 호출할 Ability 발사 함수
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void FireMissileAbility();

    // AnimNotify에서 직접 호출할 실제 미사일 발사 함수
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void FireMissileFromNotify();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Abilities")
    TSubclassOf<UGameplayAbility> DashAbilityClass_Enemy;

    UFUNCTION(BlueprintCallable, Category = "Abilities")
    void FireDashAbility();

    UPROPERTY(BlueprintReadWrite, Category = "Dash")
    bool bDashOnCooldown = false;   // true면 아직 다음 Dash 못씀

    UPROPERTY(BlueprintReadWrite, Category = "Hover")
    bool bHoverOnCooldown = false;  // true면 아직 다음 Hover 못씀

protected:

    // 플레이어와 같은 AttributeSet 사용 (Health, MaxHealth 등)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
    UMechaAttributeSet* AttributeSet;

    // Enemy 전용 초기 스탯 이펙트 (GE_InitAttributes_Enemy)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
    TSubclassOf<UGameplayEffect> InitAttributesEffect;

    // === AI 파라미터 ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
    float AggroRadius;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI")
    float MeleeRange;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
    float LeashDistance;

    // 스폰된 위치 (패트롤 기준점)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
    FVector HomeLocation;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI")
    float RangedRange;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI")
    float PatrolRadius;

    // 이 적이 사용할 Behavior Tree 에셋
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI")
    UBehaviorTree* BehaviorTreeAsset;

    // === Death / HitReact 몽타주 ===
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Death|Montage")
    UAnimMontage* DeathMontage;

    // 맞았을 때 재생할 HitReact 몽타주
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HitReact")
    UAnimMontage* HitReactMontage;

    // HitReact 최소 간격 (초) – 너무 자주 안 튕기게
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HitReact")
    float HitReactInterval = 0.4f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Abilities")
    TSubclassOf<UGameplayAbility> HoverAbilityClass_Enemy;


    // 현재 HitReact를 재생할 수 있는 상태인지
    bool bCanPlayHitReact = true;

    // HitReact 간격 관리용 타이머
    FTimerHandle TimerHandle_HitReactInterval;

    // === Enemy HUD 위젯 컴포넌트 ===
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI", meta = (AllowPrivateAccess = "true"))
    UWidgetComponent* EnemyHUDWidgetComp;

    UPROPERTY()
    AMissionManager* MissionManager = nullptr;

protected:
    virtual void BeginPlay() override;

    // 스탯 초기화 (BeginPlay에서 한 번 호출)
    void InitializeAttributes();

    // === 체력/죽음 처리 ===
    bool bIsDead = false;

    FDelegateHandle HealthChangedHandle;

    // 델리게이트 콜백
    void OnHealthChanged(const FOnAttributeChangeData& Data);

    void HandleDeath();

    // HitReact 쿨타임 리셋
    UFUNCTION()
    void ResetHitReactWindow();

public:
    // === Getter 함수들 ===
    UFUNCTION(BlueprintCallable, Category = "Enemy|Attributes")
    float GetHealth() const;

    UFUNCTION(BlueprintCallable, Category = "Enemy|Attributes")
    float GetMaxHealth() const;

    UFUNCTION(BlueprintCallable, Category = "Enemy|AI")
    UBehaviorTree* GetBehaviorTree() const { return BehaviorTreeAsset; }

    UFUNCTION(BlueprintCallable, Category = "Enemy|AI")
    float GetAggroRadius() const { return AggroRadius; }

    UFUNCTION(BlueprintCallable, Category = "Enemy|AI")
    float GetMeleeRange() const { return MeleeRange; }

    UFUNCTION(BlueprintCallable, Category = "Enemy|AI")
    float GetAttackRange() const { return RangedRange; }

    UFUNCTION(BlueprintCallable, Category = "Enemy|AI")
    float GetRangedRange() const { return RangedRange; }

    UFUNCTION(BlueprintCallable, Category = "Enemy|AI")
    float GetPatrolRadius() const { return PatrolRadius; }

    UFUNCTION(BlueprintCallable, Category = "Enemy|AI")
    FVector GetHomeLocation() const { return HomeLocation; }

    UFUNCTION(BlueprintCallable, Category = "Enemy|AI")
    float GetLeashDistance() const { return LeashDistance; }

    //  필요하면 BP에서 다시 InitAttributesEffect를 적용하고 싶을 때 사용
    UFUNCTION(BlueprintCallable, Category = "Enemy|Attributes")
    void ReInitializeAttributes();

    // === HitReact 재생 함수 (Projectile에서 호출) ===
    UFUNCTION(BlueprintCallable, Category = "Enemy|HitReact")
    void PlayHitReact();

    UFUNCTION(BlueprintCallable, Category = "Enemy|Abilities")
    void FireHoverAbility();
    
};
