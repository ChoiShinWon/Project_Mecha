#include "WBP_EnemyHealth.h"
#include "AbilitySystemComponent.h"
#include "MechaAttributeSet.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

void UWBP_EnemyHealth::InitWithASC(UAbilitySystemComponent* InASC, const UMechaAttributeSet* InAttrs)
{
    ASC = InASC;
    Attrs = InAttrs;

    UnbindHealth();
    BindHealth();
    RefreshOnce();          // 초기값 즉시 반영
}


void UWBP_EnemyHealth::NativeDestruct()
{
    UnbindHealth();
    Super::NativeDestruct();
}

void UWBP_EnemyHealth::BindHealth()
{
    if (!ASC || !Attrs) return;

    HealthChangedHandle =
        ASC->GetGameplayAttributeValueChangeDelegate(UMechaAttributeSet::GetHealthAttribute())
        .AddUObject(this, &UWBP_EnemyHealth::OnHealthChanged);
}

void UWBP_EnemyHealth::UnbindHealth()
{
    if (!ASC) return;
    ASC->GetGameplayAttributeValueChangeDelegate(UMechaAttributeSet::GetHealthAttribute())
        .Remove(HealthChangedHandle);
    HealthChangedHandle.Reset();
}

void UWBP_EnemyHealth::RefreshOnce() const
{
    if (!ASC || !Attrs) return;
    const float H = ASC->GetNumericAttribute(UMechaAttributeSet::GetHealthAttribute());
    const float M = ASC->GetNumericAttribute(UMechaAttributeSet::GetMaxHealthAttribute());
    ApplyHealthToUI(H, M);
}

void UWBP_EnemyHealth::ApplyHealthToUI(float Health, float MaxHealth) const
{
    const float Ratio = (MaxHealth > 0.f) ? (Health / MaxHealth) : 0.f;

    if (PB_Health)
    {
        PB_Health->SetPercent(FMath::Clamp(Ratio, 0.f, 1.f));
        PB_Health->SetFillColorAndOpacity(Ratio <= WarningThreshold ? ColorLow : ColorNormal);
    }

    if (TxtHealth)
    {
        // 정수로 깔끔히 표기 (예: 850 / 1000)
        const int32 H = FMath::RoundToInt(Health);
        const int32 M = FMath::RoundToInt(MaxHealth);
        TxtHealth->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), H, M)));
    }
}

void UWBP_EnemyHealth::OnHealthChanged(const FOnAttributeChangeData& Data)
{
    if (!ASC) return;
    const float H = Data.NewValue;
    const float M = ASC->GetNumericAttribute(UMechaAttributeSet::GetMaxHealthAttribute());
    ApplyHealthToUI(H, M);
}
