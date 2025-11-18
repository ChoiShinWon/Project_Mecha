#include "EnemyHUDWidget.h"

#include "AbilitySystemComponent.h"
#include "MechaAttributeSet.h"
#include "GameplayEffectTypes.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Animation/WidgetAnimation.h"
#include "TimerManager.h"  

void UEnemyHUDWidget::InitWithASC(UAbilitySystemComponent* InASC, UMechaAttributeSet* InAttributes)
{
    ASC = InASC;
    Attributes = InAttributes;

    if (!ASC || !Attributes)
    {
        return;
    }

    // 초기 값으로 세팅
    const float CurrentHealth = Attributes->GetHealth();
    const float MaxHealth = Attributes->GetMaxHealth();
    UpdateHP(CurrentHealth, MaxHealth);

    // 이전 체력 기준값 저장
    LastHealth = CurrentHealth;
    bHasLastHealth = true;

    // 기존 델리게이트 제거
    if (HealthChangedHandle.IsValid())
    {
        ASC->GetGameplayAttributeValueChangeDelegate(UMechaAttributeSet::GetHealthAttribute())
            .Remove(HealthChangedHandle);
    }

    // Health 변경 델리게이트 등록
    HealthChangedHandle =
        ASC->GetGameplayAttributeValueChangeDelegate(UMechaAttributeSet::GetHealthAttribute())
        .AddUObject(this, &UEnemyHUDWidget::OnHealthChanged);
}

void UEnemyHUDWidget::OnHealthChanged(const FOnAttributeChangeData& Data)
{
    if (!Attributes)
    {
        return;
    }

    const float NewHealth = Data.NewValue;
    const float MaxHealth = Attributes->GetMaxHealth();

    UpdateHP(NewHealth, MaxHealth);
}

void UEnemyHUDWidget::UpdateHP(float NewHealth, float MaxHealth)
{
    // === HP가 줄어들었으면 HitFlash 재생 ===
    if (bHasLastHealth && NewHealth < LastHealth)
    {
        if (HitFlash)
        {
            PlayAnimation(HitFlash, 0.f, 1);   // 시작부터 1번 재생
        }
    }

    // === HP Bar / 색상 ===
    if (HPBar && MaxHealth > 0.f)
    {
        const float Percent = FMath::Clamp(NewHealth / MaxHealth, 0.f, 1.f);
        HPBar->SetPercent(Percent);

        FLinearColor BarColor;

        if (Percent > 0.7f)
        {
            // 70% 이상: 초록(안전)
            BarColor = FLinearColor(0.1f, 0.8f, 0.1f);
        }
        else if (Percent > 0.3f)
        {
            // 30% ~ 70%: 주황(주의)
            BarColor = FLinearColor(1.0f, 0.6f, 0.1f);
        }
        else
        {
            // 30% 미만: 빨강(위험)
            BarColor = FLinearColor(0.9f, 0.1f, 0.1f);
        }

        HPBar->SetFillColorAndOpacity(BarColor);
    }

    // === 텍스트 갱신 ===
    if (HPText)
    {
        const int32 IntHealth = FMath::RoundToInt(NewHealth);
        const int32 IntMax = FMath::RoundToInt(MaxHealth);

        HPText->SetText(FText::FromString(
            FString::Printf(TEXT("%d / %d"), IntHealth, IntMax)
        ));
    }

    // === 마지막 체력 갱신 ===
    LastHealth = NewHealth;
    bHasLastHealth = true;
}


void UEnemyHUDWidget::NativeDestruct()
{
    if (ASC && HealthChangedHandle.IsValid())
    {
        ASC->GetGameplayAttributeValueChangeDelegate(UMechaAttributeSet::GetHealthAttribute())
            .Remove(HealthChangedHandle);
    }

    Super::NativeDestruct();
}

void UEnemyHUDWidget::OnOwnerDead()
{
    // 이미 파괴되는 중이면 의미 없음
    if (IsDesignTime())
    {
        return;
    }

    // DeathFade 애니메이션이 있으면 재생
    if (DeathFade)
    {
        PlayAnimation(DeathFade, 0.f, 1);

        // 애니메이션 끝날 때 완전히 숨기고 싶으면 타이머로 처리
        const float Duration = DeathFade->GetEndTime();

        if (Duration > 0.f)
        {
            FTimerHandle TimerHandle;
            if (UWorld* World = GetWorld())
            {
                World->GetTimerManager().SetTimer(
                    TimerHandle,
                    [this]()
                    {
                        SetVisibility(ESlateVisibility::Collapsed);
                    },
                    Duration,
                    false
                );
            }
        }
    }
    else
    {
        // 애니메이션이 없으면 바로 숨기기
        SetVisibility(ESlateVisibility::Collapsed);
    }
}