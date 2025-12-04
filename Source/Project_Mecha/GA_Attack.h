// GA_Attack.h
// 설명:
// - 근접 공격(Melee Attack) 능력 클래스.
// - Blueprint에서 몽타주와 노티파이를 사용하여 공격 타이밍을 제어한다.
// - PerformAttackTrace 함수를 통해 구체 스윕으로 타겟을 감지하고, GAS 데미지 또는 엔진 데미지를 적용한다.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayTagContainer.h"
#include "GA_Attack.generated.h"

class UGameplayEffect;

// 설명:
// - 근접 공격 능력. 활성화 시 Blueprint 이벤트로 몽타주를 시작하고,
//   노티파이에서 PerformAttackTrace를 호출하여 적에게 데미지를 입힌다.
UCLASS()
class PROJECT_MECHA_API UGA_Attack : public UGameplayAbility
{
    GENERATED_BODY()

public:
    // 생성자: Instancing Policy 및 Net Execution Policy 설정
    UGA_Attack();

    // Blueprint (몽타주 노티파이 등)에서 호출하는 공격 판정 함수
    // 구체 스윕으로 전방의 적을 감지하고 데미지를 적용한다
    UFUNCTION(BlueprintCallable, Category = "Attack")
    void PerformAttackTrace(AActor* SourceActor);

protected:
    // Ability 활성화 로직: Blueprint 이벤트 OnAttackTriggered 호출
    virtual void ActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData) override;

    // Ability 종료 로직: 필요 시 태그 정리
    virtual void EndAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        bool bReplicateEndAbility, bool bWasCancelled) override;

    // Blueprint에서 몽타주 트리거 시 호출되는 이벤트
    UFUNCTION(BlueprintImplementableEvent, Category = "Attack")
    void OnAttackTriggered(class AMechaCharacterBase* Mecha);

    // 대상 액터에 GAS 데미지를 우선 적용, ASC가 없으면 엔진 기본 데미지로 폴백
    void ApplyDamage_GASOrEngine(AActor* SourceActor, AActor* HitActor, const FVector& HitPoint);

protected:
    // ===== Attack Settings (공격 설정) =====

    // 공격 범위 (전방 거리)
    UPROPERTY(EditDefaultsOnly, Category = "Attack")
    float AttackRange = 250.f;

    // 공격 반경 (구체 스윕 반경)
    UPROPERTY(EditDefaultsOnly, Category = "Attack")
    float AttackRadius = 80.f;

    // 공격 트레이스 시작 위치 Z축 오프셋 (캐릭터 중심에서 위로)
    UPROPERTY(EditDefaultsOnly, Category = "Attack")
    float TraceStartZOffset = 50.f;

    // 공격 데미지
    UPROPERTY(EditDefaultsOnly, Category = "Attack")
    float AttackDamage = 20.f;

    // ===== GAS =====

    // SetByCaller로 데미지를 전달하는 GE (대상에게 적용)
    UPROPERTY(EditDefaultsOnly, Category = "Attack|GAS")
    TSubclassOf<UGameplayEffect> GE_MeleeDamage;

    // SetByCaller 키 이름 (에디터에서 변경 가능)
    UPROPERTY(EditDefaultsOnly, Category = "Attack|GAS")
    FName SetByCallerDamageName = TEXT("Data.Damage");

    // ===== FX (시각/음향 효과) =====

    // 히트 이펙트 파티클
    UPROPERTY(EditDefaultsOnly, Category = "FX")
    class UParticleSystem* HitEffect = nullptr;

    // 히트 사운드
    UPROPERTY(EditDefaultsOnly, Category = "FX")
    class USoundBase* HitSound = nullptr;
};
