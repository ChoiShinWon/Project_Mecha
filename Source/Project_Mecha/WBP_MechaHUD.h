// WBP_MechaHUD.h
// 설명:
// - 플레이어 HUD 위젯 클래스.
// - 에너지 바, 체력 바, 탄약 정보(탄창/예비탄)를 표시한다.
// - ASC와 AttributeSet을 통해 Attribute 변화를 자동 감지하여 UI를 갱신한다.
//
// Description:
// - Player HUD widget class.
// - Displays energy bar, health bar, ammo info (magazine/reserve).
// - Automatically detects Attribute changes via ASC and AttributeSet to update UI.

#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WBP_MechaHUD.generated.h"

class UProgressBar;
class UTextBlock;
class UAbilitySystemComponent;
class UMechaAttributeSet;

// 설명:
// - 플레이어 HUD 위젯. 에너지, 체력, 탄약 UI를 관리한다.
// - InitWithASC 호출 시 ASC와 AttributeSet을 바인딩하여 자동 갱신.
//
// Description:
// - Player HUD widget. Manages energy, health, and ammo UI.
// - Binds ASC and AttributeSet on InitWithASC call for automatic updates.
UCLASS()
class PROJECT_MECHA_API UWBP_MechaHUD : public UUserWidget
{
    GENERATED_BODY()
public:



    /** ĳ���Ϳ��� HUD ���� ���� ȣ�� (ASC/Attrs ����) */
    UFUNCTION(BlueprintCallable)
    void InitWithASC(UAbilitySystemComponent* InASC, const UMechaAttributeSet* InAttrs);

    /** ������ ���α׷����� ���� (���� ���� ����) */
    UFUNCTION(BlueprintCallable)
    void SetEnergyPercent(float Ratio);


    UFUNCTION(BlueprintCallable)
    void SetHealthPercent(float InPercent);

protected:
    // ===================== ���� ������ UI =====================
    /** UMG���� �̸� "PB_Energy" �� ���ε��Ǿ� �־�� �� */
    UPROPERTY(meta = (BindWidget))
    UProgressBar* PB_Energy = nullptr;


    UPROPERTY(meta = (BindWidget))                
    UProgressBar* PB_Health = nullptr;

    /** GAS ���� (�б� ����) */
    UPROPERTY(BlueprintReadOnly, Category = "GAS")
    UAbilitySystemComponent* ASC = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "GAS")
    const UMechaAttributeSet* Attrs = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Energy")
    float CautionThreshold = 0.6f;     // 60% �̻�: ����

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Energy")
    float WarningThreshold = 0.3f;     // 30~60%: �̵�

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Energy")
    FLinearColor ColorHigh = FLinearColor(0.25f, 0.9f, 1.f, 1.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Energy")
    FLinearColor ColorMid = FLinearColor(1.f, 0.7f, 0.2f, 1.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Energy")
    FLinearColor ColorLow = FLinearColor(1.f, 0.25f, 0.25f, 1.f);

    /** ����: BP���� ������ �� ���ε��ϴ� �̺�Ʈ (�ǵ帮�� ����) */
    UFUNCTION(BlueprintImplementableEvent)
    void BP_BindAttributeListeners();

    // ===================== [�߰�] ź�� UI =====================
    /** ������ �ڵ� ���õ�(��� BP �̺�Ʈ�� ó�� ����) */
    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* TxtAmmoMag = nullptr;        // "30 / 30"

    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* TxtAmmoReserve = nullptr;    // "90"

    /** (����) BP���� �߰� ����� �� */
    UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "Update Ammo UI (Mag/Max/Reserve)"))
    void BP_UpdateAmmoUI(int32 Mag, int32 MaxMag, int32 Reserve);

private:
    // ��������Ʈ �ڵ�(���� ����)
    FDelegateHandle HandleMag;
    FDelegateHandle HandleMaxMag;
    FDelegateHandle HandleReserve;

    // ���ε�/����
    void BindAmmoListeners();
    void UnbindAmmoListeners();

    // ��� 1ȸ �ݿ� & �ǹݿ�
    void RefreshAmmoOnce() const;
    void ApplyAmmoToUI(int32 Mag, int32 MaxMag, int32 Reserve) const;

    // �ݹ�
    void OnMagChanged(const struct FOnAttributeChangeData& Data);
    void OnMaxMagChanged(const struct FOnAttributeChangeData& Data);
    void OnReserveChanged(const struct FOnAttributeChangeData& Data);
};
