// EnemyAirship.h
// GAS(Gameplay Ability System)를 사용하는 적 비행선 Pawn 클래스
// ASC와 AttributeSet을 통해 체력을 관리하고, 월드 스페이스 체력바 위젯을 표시
// AI Controller가 자동으로 부여되며, 체력이 0이 되면 사망 처리 후 제거

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

// 적 비행선 클래스
// GAS를 통해 체력 관리 및 데미지 처리를 수행
// 체력바 위젯이 항상 카메라를 향하도록 빌보딩되며, 거리에 따라 크기가 조정됨
UCLASS()
class PROJECT_MECHA_API AEnemyAirship : public APawn, public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    // 생성자: 컴포넌트 초기화 및 기본 설정
    AEnemyAirship();

    // 게임 시작 시 호출: ASC 초기화, 초기 Attribute 적용, 체력 변화 델리게이트 바인딩
    virtual void BeginPlay() override;

    // 매 프레임 호출: 체력바 위젯 빌보딩 및 거리 기반 스케일 조정
    virtual void Tick(float DeltaTime) override;

    // IAbilitySystemInterface 구현: AbilitySystemComponent를 반환
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override { return AbilitySystem; }

    // ===== 접근자 =====
    UFUNCTION(BlueprintCallable, Category = "GAS|Attributes")
    float GetHealth() const;

    UFUNCTION(BlueprintCallable, Category = "GAS|Attributes")
    float GetMaxHealth() const;

protected:
    // ===== 컴포넌트 =====
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy")
    UStaticMeshComponent* BodyMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy")
    UWidgetComponent* HealthBarWidget;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy")
    UParticleSystemComponent* BoostTrail;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy")
    UFloatingPawnMovement* Movement;

    // ===== GAS =====
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
    UAbilitySystemComponent* AbilitySystem;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
    UMechaAttributeSet* AttributeSet;

    UPROPERTY(EditDefaultsOnly, Category = "GAS|Effects")
    TSubclassOf<UGameplayEffect> GE_InitAttributes_Enemy;

private:
    // ===== 체력 변화 콜백 =====
    FDelegateHandle HealthChangedHandle;

    void OnHealthChanged(const struct FOnAttributeChangeData& Data);
    void HandleDeath();
};
