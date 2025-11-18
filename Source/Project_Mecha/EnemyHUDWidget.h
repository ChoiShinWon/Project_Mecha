#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayEffectTypes.h"   // ★ FOnAttributeChangeData 정의 포함
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
    // Enemy 쪽에서 ASC + AttributeSet 넘겨줄 때 호출
    UFUNCTION(BlueprintCallable, Category = "Mecha|UI")
    void InitWithASC(UAbilitySystemComponent* InASC, UMechaAttributeSet* InAttributes);

    // 죽었을때 호출할 함수 선언
    UFUNCTION(BlueprintCallable, Category = "Mecha|UI")
    void OnOwnerDead();

protected:
    virtual void NativeDestruct() override;

    // === UMG 바인딩 ===
    // WBP_Enemy_HUD 안의 ProgressBar 이름: HPBar
    UPROPERTY(meta = (BindWidget))
    UProgressBar* HPBar;

    // WBP_Enemy_HUD 안의 TextBlock 이름: HPText (없어도 됨)
    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* HPText;

    // === GAS 참조 ===
    UPROPERTY()
    UAbilitySystemComponent* ASC;

    UPROPERTY()
    UMechaAttributeSet* Attributes;

    // HP가 줄어들 때 재생할 애니메이션
    UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
    UWidgetAnimation* HitFlash;

    UPROPERTY(Transient, meta = (BindWidgetAnimOptional))
    UWidgetAnimation* DeathFade;

    // Health 변경 델리게이트 핸들
    FDelegateHandle HealthChangedHandle;

    // 델리게이트 콜백 (블루프린트에서 쓸 일 없으니 UFUNCTION 아님)
    void OnHealthChanged(const FOnAttributeChangeData& Data);

    // 실제로 바/텍스트를 업데이트하는 내부 함수
    void UpdateHP(float NewHealth, float MaxHealth);

    float LastHealth = -1.f;
    bool bHasLastHealth = false;
};
