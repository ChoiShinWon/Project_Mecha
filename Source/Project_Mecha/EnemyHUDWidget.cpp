// EnemyHUDWidget.cpp
// 적 HUD 위젯 - 체력바 표시, 피격 플래시, 사망 연출

#include "EnemyHUDWidget.h"

#include "AbilitySystemComponent.h"
#include "MechaAttributeSet.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Animation/WidgetAnimation.h"
#include "TimerManager.h"
#include "Engine/World.h"

// ========================================
// ASC 및 AttributeSet 초기화
// ========================================
void UEnemyHUDWidget::InitWithASC(UAbilitySystemComponent* InASC, UMechaAttributeSet* InAttributes)
{
	ASC = InASC;
	Attributes = InAttributes;

	// 초기 체력 바 표시
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
void UEnemyHUDWidget::ApplyHealth(float NewHealth, float MaxHealth)
{
	UpdateHP(NewHealth, MaxHealth);
}

// ========================================
// 체력바 갱신 및 색상 변경
// ========================================
void UEnemyHUDWidget::UpdateHP(float NewHealth, float MaxHealth)
{
	// ========== 피격 플래시 연출 ==========
	// 체력이 줄어들었으면 플래시 애니메이션 재생
	if (bHasLastHealth && NewHealth < LastHealth)
	{
		if (HitFlash)
		{
			PlayAnimation(HitFlash, 0.f, 1);  // 1회 재생
		}
	}

	// ========== 체력바 업데이트 ==========
	if (HPBar && MaxHealth > 0.f)
	{
		const float Percent = FMath::Clamp(NewHealth / MaxHealth, 0.f, 1.f);
		HPBar->SetPercent(Percent);

		// 체력 비율에 따라 색상 변경
		FLinearColor BarColor;

		if (Percent > 0.7f)
		{
			// 70% 이상: 초록 (안전)
			BarColor = FLinearColor(0.1f, 0.8f, 0.1f);
		}
		else if (Percent > 0.3f)
		{
			// 30~70%: 주황 (경고)
			BarColor = FLinearColor(1.0f, 0.6f, 0.1f);
		}
		else
		{
			// 30% 이하: 빨강 (위험)
			BarColor = FLinearColor(0.9f, 0.1f, 0.1f);
		}

		HPBar->SetFillColorAndOpacity(BarColor);
	}

	// ========== 체력 텍스트 업데이트 ==========
	if (HPText)
	{
		const int32 IntHealth = FMath::RoundToInt(NewHealth);
		const int32 IntMax = FMath::RoundToInt(MaxHealth);

		HPText->SetText(FText::FromString(
			FString::Printf(TEXT("%d / %d"), IntHealth, IntMax)
		));
	}

	// 현재 체력 저장 (다음 비교용)
	LastHealth = NewHealth;
	bHasLastHealth = true;
}

// ========================================
// 위젯 종료 시 정리
// ========================================
void UEnemyHUDWidget::NativeDestruct()
{
	Super::NativeDestruct();
}

// ========================================
// 사망 시 페이드아웃 연출
// ========================================
void UEnemyHUDWidget::OnOwnerDead()
{
	if (IsDesignTime())
	{
		return;
	}

	// ========== 사망 페이드 애니메이션 ==========
	if (DeathFade)
	{
		PlayAnimation(DeathFade, 0.f, 1);

		const float Duration = DeathFade->GetEndTime();

		// 애니메이션 완료 후 위젯 숨기기
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
		// 애니메이션 없으면 즉시 숨기기
		SetVisibility(ESlateVisibility::Collapsed);
	}
}
