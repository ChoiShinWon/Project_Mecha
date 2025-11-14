// EnemyAirship.h
// 설명:
// - GAS(Gameplay Ability System)를 사용하는 적 비행선 Pawn 클래스.
// - ASC와 AttributeSet을 통해 체력을 관리하고, 월드 스페이스 체력바 위젯을 표시한다.
// - AI Controller가 자동으로 부여되며, 체력이 0이 되면 사망 처리 후 제거된다.
//
// Description:
// - Enemy airship Pawn class using GAS (Gameplay Ability System).
// - Manages health via ASC and AttributeSet, displays world-space health bar widget.
// - AI Controller is automatically assigned; destroyed when health reaches 0.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "AbilitySystemInterface.h"
#include "EnemyAirship.generated.h"

class UStaticMeshComponent;
class UWidgetComponent;
class UParticleSystemComponent;
class UFloatingPawnMovement;
class UAbilitySystemComponent;
class UGameplayEffect;
class UMechaAttributeSet;

// 설명:
// - 적 비행선 클래스. GAS를 통해 체력 관리 및 데미지 처리를 수행한다.
// - 체력바 위젯이 항상 카메라를 향하도록 빌보딩되며, 거리에 따라 크기가 조정된다.
//
// Description:
// - Enemy airship class. Manages health and damage processing via GAS.
// - Health bar widget always billboards toward camera and scales based on distance.
UCLASS()
class PROJECT_MECHA_API AEnemyAirship : public APawn, public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    // 생성자: 컴포넌트 초기화 및 기본 설정.
    // Constructor: Initializes components and default settings.
    AEnemyAirship();

    // 게임 시작 시 호출: ASC 초기화, 초기 Attribute 적용, 체력 변화 델리게이트 바인딩.
    // Called at game start: Initializes ASC, applies initial attributes, binds health change delegate.
    virtual void BeginPlay() override;

    // 매 프레임 호출: 체력바 위젯 빌보딩 및 거리 기반 스케일 조정.
    // Called every frame: Billboards health bar widget and adjusts scale based on distance.
    virtual void Tick(float DeltaTime) override;

    // IAbilitySystemInterface 구현: AbilitySystemComponent를 반환.
    // IAbilitySystemInterface implementation: Returns AbilitySystemComponent.
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override { return AbilitySystem; }

    // ===== Getters (접근자) =====

    // 현재 체력 반환 (Blueprint에서 호출 가능).
    // Returns current health (callable from Blueprint).
    UFUNCTION(BlueprintCallable, Category = "GAS|Attributes")
    float GetHealth() const;

    // 최대 체력 반환 (Blueprint에서 호출 가능).
    // Returns maximum health (callable from Blueprint).
    UFUNCTION(BlueprintCallable, Category = "GAS|Attributes")
    float GetMaxHealth() const;

protected:
    // ===== Components (컴포넌트) =====

    // 비행선의 메시 컴포넌트 (루트).
    // Airship body mesh component (root).
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy")
    UStaticMeshComponent* BodyMesh;

    // 월드 스페이스 체력바 위젯 컴포넌트.
    // World-space health bar widget component.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy")
    UWidgetComponent* HealthBarWidget;

    // 부스터 트레일 파티클 시스템 (선택 사항).
    // Booster trail particle system (optional).
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy")
    UParticleSystemComponent* BoostTrail;

    // 이동 컴포넌트: 공중 부유 이동 방식.
    // Movement component: Floating pawn movement.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy")
    UFloatingPawnMovement* Movement;

    // ===== GAS (Gameplay Ability System) =====

    // Ability System Component: 능력 및 Attribute 관리.
    // Ability System Component: Manages abilities and attributes.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
    UAbilitySystemComponent* AbilitySystem;

    // Attribute Set: 체력, 에너지 등 수치 관리.
    // Attribute Set: Manages health, energy, etc.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
    UMechaAttributeSet* AttributeSet;

    // 적 전용 초기 Attribute 설정 GE (MaxHealth, Health 등 초기값 설정).
    // Enemy-specific initial attribute setup GE (sets MaxHealth, Health, etc.).
    UPROPERTY(EditDefaultsOnly, Category = "GAS|Effects")
    TSubclassOf<UGameplayEffect> GE_InitAttributes_Enemy;

private:
    // ===== Health Change Callback (체력 변화 콜백) =====

    // 체력 변화 델리게이트 핸들.
    // Health change delegate handle.
    FDelegateHandle HealthChangedHandle;

    // 체력이 변경될 때 호출되는 함수: 체력이 0 이하면 사망 처리.
    // Called when health changes: Triggers death if health <= 0.
    void OnHealthChanged(const struct FOnAttributeChangeData& Data);

    // 사망 처리 함수: 이펙트/사운드 재생 후 액터 제거.
    // Death handling function: Plays effects/sounds, then destroys actor.
    void HandleDeath();
};
