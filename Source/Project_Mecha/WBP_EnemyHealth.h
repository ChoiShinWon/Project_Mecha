#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WBP_EnemyHealth.generated.h"

class UProgressBar;
class UTextBlock;
class UAbilitySystemComponent;
class UMechaAttributeSet;

UCLASS()
class PROJECT_MECHA_API UWBP_EnemyHealth : public UUserWidget
{
    GENERATED_BODY()

public:
    /** EnemyAirship가 BeginPlay에서 호출해서 ASC/Attrs 주입 */
    UFUNCTION(BlueprintCallable, Category = "Init")
    void InitWithASC(UAbilitySystemComponent* InASC, const UMechaAttributeSet* InAttrs);

protected:
    virtual void NativeDestruct() override;

    /** 위젯 바인딩: ProgressBar(필수), Text(선택) */
    UPROPERTY(meta = (BindWidget)) UProgressBar* PB_Health = nullptr;
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* TxtHealth = nullptr;

    /** 참조 캐시 */
    UPROPERTY(BlueprintReadOnly, Category = "GAS") UAbilitySystemComponent* ASC = nullptr;
    UPROPERTY(BlueprintReadOnly, Category = "GAS") const UMechaAttributeSet* Attrs = nullptr;

    /** 색상/연출 임계값(원하면 조정) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Health")
    float WarningThreshold = 0.3f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Health")
    FLinearColor ColorNormal = FLinearColor(0.3f, 1.f, 0.3f, 1.f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Health")
    FLinearColor ColorLow = FLinearColor(1.f, 0.25f, 0.25f, 1.f);

private:
    FDelegateHandle HealthChangedHandle;

    void BindHealth();
    void UnbindHealth();

    void RefreshOnce() const;
    void ApplyHealthToUI(float Health, float MaxHealth) const;

    void OnHealthChanged(const struct FOnAttributeChangeData& Data);
};
