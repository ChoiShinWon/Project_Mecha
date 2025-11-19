#include "EnemyHUDWidget.h"

#include "AbilitySystemComponent.h"
#include "MechaAttributeSet.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Animation/WidgetAnimation.h"
#include "TimerManager.h"
#include "Engine/World.h"

void UEnemyHUDWidget::InitWithASC(UAbilitySystemComponent* InASC, UMechaAttributeSet* InAttributes)
{
    ASC = InASC;
    Attributes = InAttributes;

    // AttributeSet이 있으면 초기 체력 한 번 세팅
    if (Attributes)
    {
        const float CurrentHealth = Attributes->GetHealth();
        const float MaxHealth = Attributes->GetMaxHealth();
        ApplyHealth(CurrentHealth, MaxHealth);
    }
}

void UEnemyHUDWidget::ApplyHealth(float NewHealth, float MaxHealth)
{
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
    // 이제 델리게이트 같은 건 안 쓰니까 그냥 부모만 호출
    Super::NativeDestruct();
}

void UEnemyHUDWidget::OnOwnerDead()
{
    if (IsDesignTime())
    {
        return;
    }

    // DeathFade 애니메이션이 있으면 재생
    if (DeathFade)
    {
        PlayAnimation(DeathFade, 0.f, 1);

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
