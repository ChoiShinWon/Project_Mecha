// MechaAttributeSet.h
#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"
#include "MechaAttributeSet.generated.h"

// 역할(Role)
// - 캐릭터의 핵심 수치(Health/Energy/MoveSpeed)와 탄약(탄창/예비탄)을 GAS Attribute로 보관
// - 서버 권위로 수정되고, RepNotify를 통해 클라이언트로 동기화됨
// - ATTRIBUTE_ACCESSORS로 BP/C++에서 손쉽게 접근 가능

#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
    GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

UCLASS()
class PROJECT_MECHA_API UMechaAttributeSet : public UAttributeSet
{
    GENERATED_BODY()

public:
    UMechaAttributeSet();

    // Vital
    UPROPERTY(BlueprintReadOnly, Category="Attributes|Vital", ReplicatedUsing=OnRep_Health)
    FGameplayAttributeData Health;
    ATTRIBUTE_ACCESSORS(UMechaAttributeSet, Health)
    UFUNCTION() void OnRep_Health(const FGameplayAttributeData& OldValue);

    UPROPERTY(BlueprintReadOnly, Category="Attributes|Vital", ReplicatedUsing=OnRep_MaxHealth)
    FGameplayAttributeData MaxHealth;
    ATTRIBUTE_ACCESSORS(UMechaAttributeSet, MaxHealth)
    UFUNCTION() void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);

    UPROPERTY(BlueprintReadOnly, Category="Attributes|Vital", ReplicatedUsing=OnRep_Energy)
    FGameplayAttributeData Energy;
    ATTRIBUTE_ACCESSORS(UMechaAttributeSet, Energy)
    UFUNCTION() void OnRep_Energy(const FGameplayAttributeData& OldValue);

    UPROPERTY(BlueprintReadOnly, Category="Attributes|Vital", ReplicatedUsing=OnRep_MaxEnergy)
    FGameplayAttributeData MaxEnergy;
    ATTRIBUTE_ACCESSORS(UMechaAttributeSet, MaxEnergy)
    UFUNCTION() void OnRep_MaxEnergy(const FGameplayAttributeData& OldValue);

    // Movement
    UPROPERTY(BlueprintReadOnly, Category="Attributes|Movement", ReplicatedUsing=OnRep_MoveSpeed)
    FGameplayAttributeData MoveSpeed;
    ATTRIBUTE_ACCESSORS(UMechaAttributeSet, MoveSpeed)
    UFUNCTION() void OnRep_MoveSpeed(const FGameplayAttributeData& OldValue);

    UPROPERTY(BlueprintReadOnly, Category="Attributes|Movement")
    FGameplayAttributeData MoveSpeedMultiplier;
    ATTRIBUTE_ACCESSORS(UMechaAttributeSet, MoveSpeedMultiplier)

    // Ammo (탄창 + 예비탄)
    UPROPERTY(BlueprintReadOnly, Category="Attributes|Ammo", ReplicatedUsing=OnRep_AmmoMagazine)
    FGameplayAttributeData AmmoMagazine;
    ATTRIBUTE_ACCESSORS(UMechaAttributeSet, AmmoMagazine)
    UFUNCTION() void OnRep_AmmoMagazine(const FGameplayAttributeData& OldValue);

    UPROPERTY(BlueprintReadOnly, Category="Attributes|Ammo", ReplicatedUsing=OnRep_MaxMagazine)
    FGameplayAttributeData MaxMagazine;
    ATTRIBUTE_ACCESSORS(UMechaAttributeSet, MaxMagazine)
    UFUNCTION() void OnRep_MaxMagazine(const FGameplayAttributeData& OldValue);

    UPROPERTY(BlueprintReadOnly, Category="Attributes|Ammo", ReplicatedUsing=OnRep_AmmoReserve)
    FGameplayAttributeData AmmoReserve;
    ATTRIBUTE_ACCESSORS(UMechaAttributeSet, AmmoReserve)
    UFUNCTION() void OnRep_AmmoReserve(const FGameplayAttributeData& OldValue);

    // Overrides
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
    virtual void PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data) override;

    // ================== Attribute 제한값 상수 ==================
    // 이동 속도 최소값
    static constexpr float MinMoveSpeed = 100.f;
    
    // 이동 속도 최대값
    static constexpr float MaxMoveSpeed = 3000.f;
    
    // 최소 탄창 크기
    static constexpr float MinMagazineSize = 1.f;
};
