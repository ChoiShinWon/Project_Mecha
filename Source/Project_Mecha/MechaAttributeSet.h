// MechaAttributeSet.h
#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffectExtension.h"
#include "MechaAttributeSet.generated.h"

// 역할(Role)
// - 캐릭터의 핵심 수치(Health/Energy/MoveSpeed)와 탄약(탄창/예비탄)을 GAS Attribute로 보관
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
    UPROPERTY(BlueprintReadOnly, Category="Attributes|Vital")
    FGameplayAttributeData Health;
    ATTRIBUTE_ACCESSORS(UMechaAttributeSet, Health)

    UPROPERTY(BlueprintReadOnly, Category="Attributes|Vital")
    FGameplayAttributeData MaxHealth;
    ATTRIBUTE_ACCESSORS(UMechaAttributeSet, MaxHealth)

    UPROPERTY(BlueprintReadOnly, Category="Attributes|Vital")
    FGameplayAttributeData Energy;
    ATTRIBUTE_ACCESSORS(UMechaAttributeSet, Energy)

    UPROPERTY(BlueprintReadOnly, Category="Attributes|Vital")
    FGameplayAttributeData MaxEnergy;
    ATTRIBUTE_ACCESSORS(UMechaAttributeSet, MaxEnergy)

    // Movement
    UPROPERTY(BlueprintReadOnly, Category="Attributes|Movement")
    FGameplayAttributeData MoveSpeed;
    ATTRIBUTE_ACCESSORS(UMechaAttributeSet, MoveSpeed)

    UPROPERTY(BlueprintReadOnly, Category="Attributes|Movement")
    FGameplayAttributeData MoveSpeedMultiplier;
    ATTRIBUTE_ACCESSORS(UMechaAttributeSet, MoveSpeedMultiplier)

    // Ammo (탄창 + 예비탄)
    UPROPERTY(BlueprintReadOnly, Category="Attributes|Ammo")
    FGameplayAttributeData AmmoMagazine;
    ATTRIBUTE_ACCESSORS(UMechaAttributeSet, AmmoMagazine)

    UPROPERTY(BlueprintReadOnly, Category="Attributes|Ammo")
    FGameplayAttributeData MaxMagazine;
    ATTRIBUTE_ACCESSORS(UMechaAttributeSet, MaxMagazine)

    UPROPERTY(BlueprintReadOnly, Category="Attributes|Ammo")
    FGameplayAttributeData AmmoReserve;
    ATTRIBUTE_ACCESSORS(UMechaAttributeSet, AmmoReserve)

    // Overrides
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
