// MechaAttributeSet.cpp
// GAS Attribute Set - 체력, 에너지, 이동 속도, 탄약 관리

#include "MechaAttributeSet.h"
#include "Net/UnrealNetwork.h"

// ========================================
// 생성자 - 기본값 설정
// ========================================
UMechaAttributeSet::UMechaAttributeSet()
{
	// ========== 생명력 ==========
	InitHealth(100.f);
	InitMaxHealth(100.f);
	
	// ========== 에너지 ==========
	InitEnergy(100.f);
	InitMaxEnergy(100.f);

	// ========== 이동 속도 ==========
	InitMoveSpeed(600.f);
	MoveSpeedMultiplier.SetBaseValue(1.0f);
	MoveSpeedMultiplier.SetCurrentValue(1.0f);

	// ========== 탄약 ==========
	InitAmmoMagazine(30.f);   // 탄창
	InitMaxMagazine(30.f);    // 최대 탄창
	InitAmmoReserve(90.f);    // 예비탄
}

// ========================================
// 네트워크 복제 설정
// ========================================
void UMechaAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 모든 Attribute를 클라이언트에 복제
	DOREPLIFETIME_CONDITION_NOTIFY(UMechaAttributeSet, Health,        COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UMechaAttributeSet, MaxHealth,     COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UMechaAttributeSet, Energy,        COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UMechaAttributeSet, MaxEnergy,     COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UMechaAttributeSet, MoveSpeed,     COND_None, REPNOTIFY_Always);

	DOREPLIFETIME_CONDITION_NOTIFY(UMechaAttributeSet, AmmoMagazine,  COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UMechaAttributeSet, MaxMagazine,   COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UMechaAttributeSet, AmmoReserve,   COND_None, REPNOTIFY_Always);
}

// ========================================
// Attribute 변경 전 클램프
// ========================================
void UMechaAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	// 이동 속도 범위 제한
	if (Attribute == GetMoveSpeedAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 100.f, 3000.f);
	}

	// 최대 탄창은 최소 1
	if (Attribute == GetMaxMagazineAttribute())
	{
		NewValue = FMath::Max(1.f, NewValue);
	}
}

// ========================================
// Gameplay Effect 적용 후 클램프
// ========================================
void UMechaAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	// ========== 체력 클램프 ==========
	if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		SetHealth(FMath::Clamp(GetHealth(), 0.f, GetMaxHealth()));
	}
	// ========== 에너지 클램프 ==========
	else if (Data.EvaluatedData.Attribute == GetEnergyAttribute())
	{
		SetEnergy(FMath::Clamp(GetEnergy(), 0.f, GetMaxEnergy()));
	}
	// ========== 탄창 클램프 ==========
	else if (Data.EvaluatedData.Attribute == GetAmmoMagazineAttribute())
	{
		SetAmmoMagazine(FMath::Clamp(GetAmmoMagazine(), 0.f, GetMaxMagazine()));
	}
	// ========== 예비탄 클램프 ==========
	else if (Data.EvaluatedData.Attribute == GetAmmoReserveAttribute())
	{
		SetAmmoReserve(FMath::Max(0.f, GetAmmoReserve()));
	}
}

// ========================================
// 복제 알림 함수들
// ========================================
void UMechaAttributeSet::OnRep_Health(const FGameplayAttributeData& OldValue)       
{ 
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMechaAttributeSet, Health, OldValue); 
}

void UMechaAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldValue)    
{ 
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMechaAttributeSet, MaxHealth, OldValue); 
}

void UMechaAttributeSet::OnRep_Energy(const FGameplayAttributeData& OldValue)       
{ 
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMechaAttributeSet, Energy, OldValue); 
}

void UMechaAttributeSet::OnRep_MaxEnergy(const FGameplayAttributeData& OldValue)    
{ 
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMechaAttributeSet, MaxEnergy, OldValue); 
}

void UMechaAttributeSet::OnRep_MoveSpeed(const FGameplayAttributeData& OldValue)    
{ 
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMechaAttributeSet, MoveSpeed, OldValue); 
}

void UMechaAttributeSet::OnRep_AmmoMagazine(const FGameplayAttributeData& OldValue) 
{ 
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMechaAttributeSet, AmmoMagazine, OldValue); 
}

void UMechaAttributeSet::OnRep_MaxMagazine(const FGameplayAttributeData& OldValue)  
{ 
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMechaAttributeSet, MaxMagazine, OldValue); 
}

void UMechaAttributeSet::OnRep_AmmoReserve(const FGameplayAttributeData& OldValue)  
{ 
	GAMEPLAYATTRIBUTE_REPNOTIFY(UMechaAttributeSet, AmmoReserve, OldValue); 
}
