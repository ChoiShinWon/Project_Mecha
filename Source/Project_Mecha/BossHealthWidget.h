// BossHealthWidget.h
// 보스 체력바 위젯 - 화면 상단에 표시

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BossHealthWidget.generated.h"

class UProgressBar;
class UTextBlock;
class UAbilitySystemComponent;
class UMechaAttributeSet;
class UWidgetAnimation;

UCLASS()
class PROJECT_MECHA_API UBossHealthWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // ========================================
    // 보스 ASC와 AttributeSet으로 초기화
    // ========================================
    UFUNCTION(BlueprintCallable, Category = "Boss|UI")
    void InitWithBoss(UAbilitySystemComponent* InASC, UMechaAttributeSet* InAttributes, const FText& InBossName);

    // ========================================
    // 체력 업데이트
    // ========================================
    UFUNCTION(BlueprintCallable, Category = "Boss|UI")
    void ApplyHealth(float NewHealth, float MaxHealth);

    // ========================================
    // 보스 사망 시 호출
    // ========================================
    UFUNCTION(BlueprintCallable, Category = "Boss|UI")
    void OnBossDead();

    // ========================================
    // 위젯 표시/숨김
    // ========================================
    UFUNCTION(BlueprintCallable, Category = "Boss|UI")
    void ShowBossHealth();

    UFUNCTION(BlueprintCallable, Category = "Boss|UI")
    void HideBossHealth();

protected:
    virtual void NativeDestruct() override;

    // === UMG 바인딩 ===
    // 보스 이름 텍스트
    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* TxtBossName;

    // 체력바
    UPROPERTY(meta = (BindWidget))
    UProgressBar* PB_BossHealth;

    // 체력 텍스트 (선택)
    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* TxtBossHealthValue;

    // === 애니메이션 (선택) ===
    // 등장 애니메이션
    UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
    UWidgetAnimation* AppearAnim;

    // 피격 플래시
    UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
    UWidgetAnimation* HitFlash;

    // 사망 페이드아웃
    UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
    UWidgetAnimation* DeathFade;

    // === 체력바 색상 설정 ===
    // 체력 높을 때 색상 (50% 이상)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|UI|Colors")
    FLinearColor ColorHigh = FLinearColor(0.8f, 0.1f, 0.1f, 1.f);

    // 체력 중간 색상 (25~50%)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|UI|Colors")
    FLinearColor ColorMid = FLinearColor(1.0f, 0.4f, 0.1f, 1.f);

    // 체력 낮을 때 색상 (25% 이하)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss|UI|Colors")
    FLinearColor ColorLow = FLinearColor(1.0f, 0.8f, 0.1f, 1.f);

    // === GAS 참조 ===
    UPROPERTY()
    UAbilitySystemComponent* ASC = nullptr;

    UPROPERTY()
    UMechaAttributeSet* Attributes = nullptr;

private:
    void UpdateHealthBar(float NewHealth, float MaxHealth);

    float LastHealth = -1.f;
    bool bHasLastHealth = false;
};

