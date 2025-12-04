// BossHealthWidget.cpp
// 보스 체력바 위젯 구현

#include "BossHealthWidget.h"
#include "AbilitySystemComponent.h"
#include "MechaAttributeSet.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Animation/WidgetAnimation.h"
#include "TimerManager.h"
#include "Engine/World.h"

// ========================================
// 보스 정보로 초기화
// ========================================
void UBossHealthWidget::InitWithBoss(UAbilitySystemComponent* InASC, UMechaAttributeSet* InAttributes, const FText& InBossName)
{
    ASC = InASC;
    Attributes = InAttributes;

    // 보스 이름 설정
    if (TxtBossName)
    {
        TxtBossName->SetText(InBossName);
    }

    // 초기 체력 표시
    if (Attributes)
    {
        const float CurrentHealth = Attributes->GetHealth();
        const float MaxHealth = Attributes->GetMaxHealth();
        ApplyHealth(CurrentHealth, MaxHealth);
    }
}

// ========================================
// 체력 업데이트
// ========================================
void UBossHealthWidget::ApplyHealth(float NewHealth, float MaxHealth)
{
    UpdateHealthBar(NewHealth, MaxHealth);
}

// ========================================
// 체력바 갱신
// ========================================
void UBossHealthWidget::UpdateHealthBar(float NewHealth, float MaxHealth)
{
    // ========== 피격 플래시 연출 ==========
    // 체력이 줄어들었으면 플래시 애니메이션 재생
    if (bHasLastHealth && NewHealth < LastHealth)
    {
        if (HitFlash)
        {
            PlayAnimation(HitFlash, 0.f, 1);
        }
    }

    // ========== 체력바 업데이트 ==========
    if (PB_BossHealth && MaxHealth > 0.f)
    {
        const float Percent = FMath::Clamp(NewHealth / MaxHealth, 0.f, 1.f);
        PB_BossHealth->SetPercent(Percent);

        // 체력 비율에 따라 색상 변경
        FLinearColor BarColor;
        if (Percent > 0.5f)
        {
            // 50% 이상
            BarColor = ColorHigh;
        }
        else if (Percent > 0.25f)
        {
            // 25~50%
            BarColor = ColorMid;
        }
        else
        {
            // 25% 이하
            BarColor = ColorLow;
        }
        PB_BossHealth->SetFillColorAndOpacity(BarColor);
    }

    // ========== 체력 텍스트 업데이트 ==========
    if (TxtBossHealthValue)
    {
        const int32 IntHealth = FMath::RoundToInt(NewHealth);
        const int32 IntMax = FMath::RoundToInt(MaxHealth);
        TxtBossHealthValue->SetText(FText::FromString(
            FString::Printf(TEXT("%d / %d"), IntHealth, IntMax)
        ));
    }

    // 현재 체력 저장 (다음 비교용)
    LastHealth = NewHealth;
    bHasLastHealth = true;
}

// ========================================
// 보스 체력바 표시
// ========================================
void UBossHealthWidget::ShowBossHealth()
{
    SetVisibility(ESlateVisibility::Visible);

    if (AppearAnim)
    {
        PlayAnimation(AppearAnim);
    }
}

// ========================================
// 보스 체력바 숨김
// ========================================
void UBossHealthWidget::HideBossHealth()
{
    SetVisibility(ESlateVisibility::Hidden);
}

// ========================================
// 보스 사망 처리
// ========================================
void UBossHealthWidget::OnBossDead()
{
    if (IsDesignTime())
    {
        return;
    }

    if (DeathFade)
    {
        PlayAnimation(DeathFade);

        // 애니메이션 끝난 후 제거
        FTimerHandle TimerHandle;
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().SetTimer(
                TimerHandle,
                [this]()
                {
                    RemoveFromParent();
                },
                DeathFade->GetEndTime(),
                false
            );
        }
    }
    else
    {
        // 애니메이션 없으면 바로 제거
        RemoveFromParent();
    }
}

// ========================================
// 위젯 소멸 시 정리
// ========================================
void UBossHealthWidget::NativeDestruct()
{
    ASC = nullptr;
    Attributes = nullptr;
    Super::NativeDestruct();
}

