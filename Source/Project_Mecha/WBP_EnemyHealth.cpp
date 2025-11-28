// WBP_EnemyHealth.cpp
// 적 체력바 위젯 - 월드 스페이스 체력바

#include "WBP_EnemyHealth.h"
#include "AbilitySystemComponent.h"
#include "MechaAttributeSet.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

// ========================================
// ASC 및 AttributeSet 초기화
// ========================================
void UWBP_EnemyHealth::InitWithASC(UAbilitySystemComponent* InASC, const UMechaAttributeSet* InAttrs)
{
	ASC = InASC;
	Attrs = InAttrs;

	// 델리게이트 바인딩
	UnbindHealth();
	BindHealth();
	RefreshOnce();  // 초기값 즉시 반영
}

// ========================================
// 위젯 종료 시 정리
// ========================================
void UWBP_EnemyHealth::NativeDestruct()
{
	UnbindHealth();
	Super::NativeDestruct();
}

// ========================================
// 체력 변경 델리게이트 바인딩
// ========================================
void UWBP_EnemyHealth::BindHealth()
{
	if (!ASC || !Attrs) return;

	HealthChangedHandle =
		ASC->GetGameplayAttributeValueChangeDelegate(UMechaAttributeSet::GetHealthAttribute())
		.AddUObject(this, &UWBP_EnemyHealth::OnHealthChanged);
}

// ========================================
// 체력 변경 델리게이트 해제
// ========================================
void UWBP_EnemyHealth::UnbindHealth()
{
	if (!ASC) return;
	
	ASC->GetGameplayAttributeValueChangeDelegate(UMechaAttributeSet::GetHealthAttribute())
		.Remove(HealthChangedHandle);
	HealthChangedHandle.Reset();
}

// ========================================
// 초기 체력 표시
// ========================================
void UWBP_EnemyHealth::RefreshOnce() const
{
	if (!ASC || !Attrs) return;
	
	const float H = ASC->GetNumericAttribute(UMechaAttributeSet::GetHealthAttribute());
	const float M = ASC->GetNumericAttribute(UMechaAttributeSet::GetMaxHealthAttribute());
	ApplyHealthToUI(H, M);
}

// ========================================
// UI에 체력 표시
// ========================================
void UWBP_EnemyHealth::ApplyHealthToUI(float Health, float MaxHealth) const
{
	const float Ratio = (MaxHealth > 0.f) ? (Health / MaxHealth) : 0.f;

	// ========== 체력바 ==========
	if (PB_Health)
	{
		PB_Health->SetPercent(FMath::Clamp(Ratio, 0.f, 1.f));
		
		// 낮은 체력일 때 색상 변경
		PB_Health->SetFillColorAndOpacity(Ratio <= WarningThreshold ? ColorLow : ColorNormal);
	}

	// ========== 체력 텍스트 ==========
	if (TxtHealth)
	{
		const int32 H = FMath::RoundToInt(Health);
		const int32 M = FMath::RoundToInt(MaxHealth);
		TxtHealth->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), H, M)));
	}
}

// ========================================
// 체력 변경 콜백
// ========================================
void UWBP_EnemyHealth::OnHealthChanged(const FOnAttributeChangeData& Data)
{
	if (!ASC) return;
	
	const float H = Data.NewValue;
	const float M = ASC->GetNumericAttribute(UMechaAttributeSet::GetMaxHealthAttribute());
	ApplyHealthToUI(H, M);
}
