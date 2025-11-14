// GA_MissleFire.h
// 설명:
// - 미사일 발사 능력 클래스.
// - 여러 발의 미사일을 순차적으로 발사하며, 자동으로 가장 가까운 적을 추적한다.
// - ProjectileMovementComponent를 강제로 생성하여 유도 미사일 기능을 구현한다.
//
// Description:
// - Missile firing ability class.
// - Fires multiple missiles sequentially, automatically targeting the nearest enemy.
// - Forcibly creates ProjectileMovementComponent to implement homing missile functionality.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_MissleFire.generated.h"

class UProjectileMovementComponent;
class UGameplayEffect;
class AEnemyAirship;

// 설명:
// - 미사일 발사 능력. 순차 발사, 적 자동 추적, 유도 미사일.
//
// Description:
// - Missile firing ability. Sequential firing, auto-targeting, homing missiles.
UCLASS()
class PROJECT_MECHA_API UGA_MissleFire : public UGameplayAbility
{
    GENERATED_BODY()

public:
    // 생성자: Instancing Policy 및 Net Execution Policy 설정.
    // Constructor: Sets Instancing Policy and Net Execution Policy.
    UGA_MissleFire();

    // Ability 활성화 로직: 순차적으로 미사일 발사.
    // Ability activation logic: Fires missiles sequentially.
    virtual void ActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData
    ) override;

protected:
    // ===== Internal Functions (내부 함수) =====

    // 미사일 스폰 함수 (타이머에서 호출).
    // Spawns missile (called by timer).
    void SpawnMissle(int32 Index, class ACharacter* OwnerChar);

    // 가장 가까운 적 선택 함수.
    // Picks nearest enemy.
    AActor* PickBestTarget(const AActor* Owner) const;

    // 유도 미사일 설정 함수 (사용되지 않음, SpawnMissle에서 직접 처리).
    // Sets up homing missile (unused, handled directly in SpawnMissle).
    void SetupHoming(class AActor* Missle, AActor* Target) const;

    // 미사일에 데미지 GE 설정 함수 (Blueprint 함수 호출).
    // Sets damage GE on missile (calls Blueprint function).
    void InitDamageOnMissle(AActor* Missle, AActor* Source) const;

    // 미사일을 강제로 이동 가능하게 만들고 발사하는 함수.
    // - 루트가 Primitive가 아니면 Sphere를 생성하여 루트로 설정.
    // - ProjectileMovementComponent가 없으면 생성.
    // - 초기 속도 및 발사 방향 설정.
    //
    // Forcibly makes missile movable and launches it.
    // - Creates Sphere as root if root is not Primitive.
    // - Creates ProjectileMovementComponent if not present.
    // - Sets initial velocity and launch direction.
    class UProjectileMovementComponent* ForceMakeMovableAndLaunch(class AActor* Missle, const FVector& LaunchDir) const;

public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS", meta = (DisplayName = "GE Melee Damage"))
    TSubclassOf<UGameplayEffect> GE_MissleDamage;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS", meta = (DisplayName = "Set by Caller Damage Name"))
    FName SetByCallerDamageName = TEXT("Data.Damage");

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS")
    float BaseDamage = 80.f;

protected:
    UPROPERTY(EditDefaultsOnly, Category = "Missle")
    TSubclassOf<AActor> MissleProjectileClass;

    UPROPERTY(EditDefaultsOnly, Category = "Missle")
    int32 NumProjectiles = 4;

    UPROPERTY(EditDefaultsOnly, Category = "Missle")
    float TimeBetweenShots = 0.15f;

    UPROPERTY(EditDefaultsOnly, Category = "Missle")
    float SpawnOffset = 100.f;

    UPROPERTY(EditDefaultsOnly, Category = "Missle")
    float SpreadAngle = 6.0f;

    /** Ÿ�� ��� ������Ű�� �⺻ �߻� �ӵ� */
    UPROPERTY(EditDefaultsOnly, Category = "Missle|Movement")
    float InitialLaunchSpeed = 3000.f;

    UPROPERTY(EditDefaultsOnly, Category = "Missle|Homing")
    float HomingAcceleration = 8000.f;

    UPROPERTY(EditDefaultsOnly, Category = "Missle|Homing")
    float MaxLockDistance = 12000.f;
};
