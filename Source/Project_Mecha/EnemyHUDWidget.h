#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EnemyHUDWidget.generated.h"

class UProgressBar;
class UTextBlock;
class UAbilitySystemComponent;
class UMechaAttributeSet;
class UWidgetAnimation;

/**
 * 적 전용 HUD 위젯 (체력바 + 텍스트)
 */
UCLASS()
class PROJECT_MECHA_API UEnemyHUDWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // Enemy 쪽에서 ASC + AttributeSet 넘겨줄 때 호출 (초기 세팅용)
    UFUNCTION(BlueprintCallable, Category = "Mecha|UI")
    void InitWithASC(UAbilitySystemComponent* InASC, UMechaAttributeSet* InAttributes);

    // EnemyMecha에서 체력/최대체력 바뀔 때마다 직접 호출
    UFUNCTION(BlueprintCallable, Category = "Mecha|UI")
    void ApplyHealth(float NewHealth, float MaxHealth);

    // 적이 죽었을 때 호출
    UFUNCTION(BlueprintCallable, Category = "Mecha|UI")
    void OnOwnerDead();

protected:
    virtual void NativeDestruct() override;

    // === UMG 바인딩 ===
    UPROPERTY(meta = (BindWidget))
    UProgressBar* HPBar;

    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* HPText;

    // === GAS 참조 (나중을 위해 보관만) ===
    UPROPERTY()
    UAbilitySystemComponent* ASC = nullptr;

    UPROPERTY()
    UMechaAttributeSet* Attributes = nullptr;

    // HP가 줄어들 때 재생할 애니메이션
    UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
    UWidgetAnimation* HitFlash;

    // 죽을 때 재생할 애니메이션
    UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
    UWidgetAnimation* DeathFade;

    // 실제로 바/텍스트를 업데이트하는 내부 함수
    void UpdateHP(float NewHealth, float MaxHealth);

    float LastHealth = -1.f;
    bool bHasLastHealth = false;
};
