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
class UWidgetAnimation;


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

    /** 캐릭터에서 HUD 초기 설정 호출 (ASC/Attrs 전달) */
    UFUNCTION(BlueprintCallable)
    void InitWithASC(UAbilitySystemComponent* InASC, const UMechaAttributeSet* InAttrs);

    /** 에너지 프로그레스바 설정 (비율 및 색상 변경) */
    UFUNCTION(BlueprintCallable)
    void SetEnergyPercent(float Ratio);

    /** 체력 프로그레스바 설정 */
    UFUNCTION(BlueprintCallable)
    void SetHealthPercent(float InPercent);

    UFUNCTION(BlueprintImplementableEvent, Category = "Mecha|UI")
    void PlayDamageOverlay(float DamageAmount);

    // BP에서 실제 구현할 이벤트 (그래프 짤 곳)
    UFUNCTION(BlueprintImplementableEvent, Category = "Mission")
    void BP_UpdateMissionStarted(int32 RequiredKillCount);

    UFUNCTION(BlueprintImplementableEvent, Category = "Mission")
    void BP_UpdateMissionProgress(int32 CurrentKillCount, int32 RequiredKillCount);

    UFUNCTION(BlueprintImplementableEvent, Category = "Mission")
    void BP_ShowMissionClear(float MissionTime);

    // 다른 BP에서 호출할 "정식 함수"
    UFUNCTION(BlueprintCallable, Category = "Mission")
    void UpdateMissionStarted(int32 RequiredKillCount)
    {
        BP_UpdateMissionStarted(RequiredKillCount);
    }

    UFUNCTION(BlueprintCallable, Category = "Mission")
    void UpdateMissionProgress(int32 CurrentKillCount, int32 RequiredKillCount)
    {
        BP_UpdateMissionProgress(CurrentKillCount, RequiredKillCount);
    }

    UFUNCTION(BlueprintCallable, Category = "Mission")
    void ShowMissionClear(float MissionTime)
    {
        BP_ShowMissionClear(MissionTime);
    }


protected:
    // ===================== 에너지 / 체력 UI =====================
    /** UMG에서 이름 "PB_Energy"로 바인딩 */
    UPROPERTY(meta = (BindWidget))
    UProgressBar* PB_Energy = nullptr;

    /** UMG에서 이름 "PB_Health"로 바인딩 */
    UPROPERTY(meta = (BindWidget))
    UProgressBar* PB_Health = nullptr;

    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* Txt_LowHPWarning = nullptr;

    UPROPERTY(meta = (BindWidgetAnimOptional), Transient)
    UWidgetAnimation* LowHP_WarningPulse = nullptr;

    /** GAS 참조 (읽기 전용) */
    UPROPERTY(BlueprintReadOnly, Category = "GAS")
    UAbilitySystemComponent* ASC = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "GAS")
    const UMechaAttributeSet* Attrs = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Energy")
    float CautionThreshold = 0.6f;     // 60% 이상: 안전

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Energy")
    float WarningThreshold = 0.3f;     // 30~60%: 경고

    // === 아머드 코어 스타일: 하얀색 통일 ===
    // Armored Core Style: White unified colors
    
    // 에너지바 색상 - 전부 하얀색
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Energy")
    FLinearColor ColorHigh = FLinearColor(1.0f, 1.0f, 1.0f, 1.f);    // 하얀색

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Energy")
    FLinearColor ColorMid = FLinearColor(1.0f, 1.0f, 1.0f, 1.f);     // 하얀색

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Energy")
    FLinearColor ColorLow = FLinearColor(1.0f, 0.3f, 0.3f, 1.f);     // 위험 시 약간 빨강 틴트

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Health")
    float HealthCautionThreshold = 0.6f;   // 60% 이상

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Health")
    float HealthWarningThreshold = 0.3f;   // 30~60%

    // 체력바 색상 - 전부 하얀색
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Health")
    FLinearColor HealthColorHigh = FLinearColor(1.0f, 1.0f, 1.0f, 1.f);    // 하얀색

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Health")
    FLinearColor HealthColorMid = FLinearColor(1.0f, 1.0f, 1.0f, 1.f);     // 하얀색

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI|Health")
    FLinearColor HealthColorLow = FLinearColor(1.0f, 0.3f, 0.3f, 1.f);     // 위험 시 약간 빨강 틴트

    /** 에너지/기타 Attribute 리스너를 BP에서 묶어주는 이벤트 */
    UFUNCTION(BlueprintImplementableEvent)
    void BP_BindAttributeListeners();

    // ===================== [추가] 탄약 UI =====================
    /** 탄창 텍스트 ("30 / 30") */
    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* TxtAmmoMag = nullptr;

    /** 예비 탄약 텍스트 ("90") */
    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* TxtAmmoReserve = nullptr;

    /** (선택) BP에서 추가 연출용 */
    UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "Update Ammo UI (Mag/Max/Reserve)"))
    void BP_UpdateAmmoUI(int32 Mag, int32 MaxMag, int32 Reserve);

private:
    // 델리게이트 핸들
    FDelegateHandle HandleMag;
    FDelegateHandle HandleMaxMag;
    FDelegateHandle HandleReserve;

    // 탄약 리스너 바인딩/해제
    void BindAmmoListeners();
    void UnbindAmmoListeners();

    // 초기 1회 쿼리 & UI 반영
    void RefreshAmmoOnce() const;
    void ApplyAmmoToUI(int32 Mag, int32 MaxMag, int32 Reserve) const;

    // 콜백
    void OnMagChanged(const struct FOnAttributeChangeData& Data);
    void OnMaxMagChanged(const struct FOnAttributeChangeData& Data);
    void OnReserveChanged(const struct FOnAttributeChangeData& Data);
};
