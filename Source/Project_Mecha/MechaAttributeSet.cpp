// MechaAttributeSet.cpp
#include "MechaAttributeSet.h"
#include "Net/UnrealNetwork.h"

// 기본값 초기화
// - Vital/Movement/Ammo 기본 수치 설정
UMechaAttributeSet::UMechaAttributeSet()
{
    // Vital
    InitHealth(100.f);
    InitMaxHealth(100.f);
    InitEnergy(100.f);
    InitMaxEnergy(100.f);

    // Movement
    InitMoveSpeed(600.f);
    MoveSpeedMultiplier.SetBaseValue(1.0f);
    MoveSpeedMultiplier.SetCurrentValue(1.0f);

    // Ammo: 기본값(탄창 30 / 예비탄 90)
    InitAmmoMagazine(30.f);
    InitMaxMagazine(30.f);
    InitAmmoReserve(90.f);
}

// 복제 설정
// - 모든 Attribute를 Always로 알림
void UMechaAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME_CONDITION_NOTIFY(UMechaAttributeSet, Health,        COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UMechaAttributeSet, MaxHealth,     COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UMechaAttributeSet, Energy,        COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UMechaAttributeSet, MaxEnergy,     COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UMechaAttributeSet, MoveSpeed,     COND_None, REPNOTIFY_Always);

    DOREPLIFETIME_CONDITION_NOTIFY(UMechaAttributeSet, AmmoMagazine,  COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UMechaAttributeSet, MaxMagazine,   COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UMechaAttributeSet, AmmoReserve,   COND_None, REPNOTIFY_Always);
}

// 변경 직전 클램프/보정
void UMechaAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
    Super::PreAttributeChange(Attribute, NewValue);

    if (Attribute == GetMoveSpeedAttribute())
    {
        NewValue = FMath::Clamp(NewValue, 100.f, 3000.f);
    }

    if (Attribute == GetMaxMagazineAttribute())
    {
        NewValue = FMath::Max(1.f, NewValue);
    }
}

// GE 적용 후 보정
void UMechaAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
    Super::PostGameplayEffectExecute(Data);

    if (Data.EvaluatedData.Attribute == GetHealthAttribute())
    {
        SetHealth(FMath::Clamp(GetHealth(), 0.f, GetMaxHealth()));
    }
    else if (Data.EvaluatedData.Attribute == GetEnergyAttribute())
    {
        SetEnergy(FMath::Clamp(GetEnergy(), 0.f, GetMaxEnergy()));
    }
    else if (Data.EvaluatedData.Attribute == GetAmmoMagazineAttribute())
    {
        SetAmmoMagazine(FMath::Clamp(GetAmmoMagazine(), 0.f, GetMaxMagazine()));
    }
    else if (Data.EvaluatedData.Attribute == GetAmmoReserveAttribute())
    {
        SetAmmoReserve(FMath::Max(0.f, GetAmmoReserve()));
    }
}

void UMechaAttributeSet::OnRep_Health(const FGameplayAttributeData& OldValue)       { GAMEPLAYATTRIBUTE_REPNOTIFY(UMechaAttributeSet, Health, OldValue); }
void UMechaAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldValue)    { GAMEPLAYATTRIBUTE_REPNOTIFY(UMechaAttributeSet, MaxHealth, OldValue); }
void UMechaAttributeSet::OnRep_Energy(const FGameplayAttributeData& OldValue)       { GAMEPLAYATTRIBUTE_REPNOTIFY(UMechaAttributeSet, Energy, OldValue); }
void UMechaAttributeSet::OnRep_MaxEnergy(const FGameplayAttributeData& OldValue)    { GAMEPLAYATTRIBUTE_REPNOTIFY(UMechaAttributeSet, MaxEnergy, OldValue); }

void UMechaAttributeSet::OnRep_MoveSpeed(const FGameplayAttributeData& OldValue)    { GAMEPLAYATTRIBUTE_REPNOTIFY(UMechaAttributeSet, MoveSpeed, OldValue); }

void UMechaAttributeSet::OnRep_AmmoMagazine(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UMechaAttributeSet, AmmoMagazine, OldValue); }
void UMechaAttributeSet::OnRep_MaxMagazine(const FGameplayAttributeData& OldValue)  { GAMEPLAYATTRIBUTE_REPNOTIFY(UMechaAttributeSet, MaxMagazine, OldValue); }
void UMechaAttributeSet::OnRep_AmmoReserve(const FGameplayAttributeData& OldValue)  { GAMEPLAYATTRIBUTE_REPNOTIFY(UMechaAttributeSet, AmmoReserve, OldValue); }
