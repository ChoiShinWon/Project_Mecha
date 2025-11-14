// WBP_MechaHUD.cpp
// 설명:
// - UWBP_MechaHUD 클래스의 구현부.
// - ASC와 AttributeSet을 바인딩하여 Attribute 변화 시 UI를 자동 갱신한다.
// - 에너지, 체력, 탄약 UI를 관리한다.
//
// Description:
// - Implementation of UWBP_MechaHUD class.
// - Binds ASC and AttributeSet to automatically update UI on Attribute changes.
// - Manages energy, health, and ammo UI.

#include "WBP_MechaHUD.h"
#include "AbilitySystemComponent.h"
#include "MechaAttributeSet.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

// ASC와 AttributeSet 초기화 및 Attribute 리스너 바인딩.
// - BP에서 에너지 리스너 바인딩 (BP_BindAttributeListeners).
// - C++에서 탄약 리스너 바인딩 (BindAmmoListeners).
// - 초기값 즉시 표시 (RefreshAmmoOnce).
//
// Initializes ASC and AttributeSet, binds Attribute listeners.
// - Binds energy listeners in BP (BP_BindAttributeListeners).
// - Binds ammo listeners in C++ (BindAmmoListeners).
// - Displays initial values immediately (RefreshAmmoOnce).
void UWBP_MechaHUD::InitWithASC(UAbilitySystemComponent* InASC, const UMechaAttributeSet* InAttrs)
{
    ASC = InASC;
    Attrs = InAttrs;

    // ✅ 기존 에너지 바인딩은 BP에서 처리 (기존 흐름 유지)
    BP_BindAttributeListeners();

    // ✅ 탄약 바인딩 "추가"
    UnbindAmmoListeners();   // 중복 방지
    BindAmmoListeners();
    RefreshAmmoOnce();       // 초기값 즉시 표시
}

// 에너지 프로그레스 바 설정 (비율 및 색상 변경).
// - 비율에 따라 색상을 변경한다 (60% 이상: 파란색, 30~60%: 주황색, 30% 미만: 빨간색).
//
// Sets energy progress bar (ratio and color change).
// - Changes color based on ratio (60%+: blue, 30~60%: orange, <30%: red).
void UWBP_MechaHUD::SetEnergyPercent(float Ratio)
{
    if (!PB_Energy) return;

    PB_Energy->SetPercent(Ratio);

    const FLinearColor Picked =
        (Ratio >= CautionThreshold) ? ColorHigh :
        (Ratio >= WarningThreshold) ? ColorMid :
        ColorLow;

    PB_Energy->SetFillColorAndOpacity(Picked);
}

// 체력 프로그레스 바 설정.
// Sets health progress bar.
void UWBP_MechaHUD::SetHealthPercent(float InPercent)
{
    if (!PB_Health) return;

    const float Clamped = FMath::Clamp(InPercent, 0.f, 1.f);
    PB_Health->SetPercent(Clamped);

 
}

// ===================== 탄약 바인딩 구현 =====================

// 탄약 Attribute 리스너 바인딩.
// - AmmoMagazine, MaxMagazine, AmmoReserve Attribute 변화 델리게이트에 콜백 함수 바인딩.
//
// Binds ammo Attribute listeners.
// - Binds callback functions to AmmoMagazine, MaxMagazine, AmmoReserve Attribute change delegates.
void UWBP_MechaHUD::BindAmmoListeners()
{
    if (!ASC) return;

    HandleMag = ASC->GetGameplayAttributeValueChangeDelegate(
        UMechaAttributeSet::GetAmmoMagazineAttribute())
        .AddUObject(this, &UWBP_MechaHUD::OnMagChanged);

    HandleMaxMag = ASC->GetGameplayAttributeValueChangeDelegate(
        UMechaAttributeSet::GetMaxMagazineAttribute())
        .AddUObject(this, &UWBP_MechaHUD::OnMaxMagChanged);

    HandleReserve = ASC->GetGameplayAttributeValueChangeDelegate(
        UMechaAttributeSet::GetAmmoReserveAttribute())
        .AddUObject(this, &UWBP_MechaHUD::OnReserveChanged);
}

void UWBP_MechaHUD::UnbindAmmoListeners()
{
    if (!ASC) return;

    ASC->GetGameplayAttributeValueChangeDelegate(
        UMechaAttributeSet::GetAmmoMagazineAttribute()).Remove(HandleMag);
    ASC->GetGameplayAttributeValueChangeDelegate(
        UMechaAttributeSet::GetMaxMagazineAttribute()).Remove(HandleMaxMag);
    ASC->GetGameplayAttributeValueChangeDelegate(
        UMechaAttributeSet::GetAmmoReserveAttribute()).Remove(HandleReserve);

    HandleMag.Reset();
    HandleMaxMag.Reset();
    HandleReserve.Reset();
}

void UWBP_MechaHUD::RefreshAmmoOnce() const
{
    if (!ASC) return;

    const int32 Mag = FMath::RoundToInt(ASC->GetNumericAttribute(UMechaAttributeSet::GetAmmoMagazineAttribute()));
    const int32 MaxMag = FMath::RoundToInt(ASC->GetNumericAttribute(UMechaAttributeSet::GetMaxMagazineAttribute()));
    const int32 Res = FMath::RoundToInt(ASC->GetNumericAttribute(UMechaAttributeSet::GetAmmoReserveAttribute()));

    ApplyAmmoToUI(Mag, MaxMag, Res);
}

void UWBP_MechaHUD::ApplyAmmoToUI(int32 Mag, int32 MaxMag, int32 Reserve) const
{
    // 1) C++에서 직접 위젯 채우기(있으면)
    if (TxtAmmoMag)
    {
        TxtAmmoMag->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), Mag, MaxMag)));
    }
    if (TxtAmmoReserve)
    {
        TxtAmmoReserve->SetText(FText::AsNumber(Reserve));
    }

    // 2) BP 이벤트 호출(없어도 무방) — BP에서 별도 연출 가능
    const_cast<UWBP_MechaHUD*>(this)->BP_UpdateAmmoUI(Mag, MaxMag, Reserve);
}

void UWBP_MechaHUD::OnMagChanged(const FOnAttributeChangeData& Data)
{
    if (!ASC) return;
    const int32 Mag = FMath::RoundToInt(Data.NewValue);
    const int32 MaxMag = FMath::RoundToInt(ASC->GetNumericAttribute(UMechaAttributeSet::GetMaxMagazineAttribute()));
    const int32 Res = FMath::RoundToInt(ASC->GetNumericAttribute(UMechaAttributeSet::GetAmmoReserveAttribute()));
    ApplyAmmoToUI(Mag, MaxMag, Res);
}

void UWBP_MechaHUD::OnMaxMagChanged(const FOnAttributeChangeData& Data)
{
    if (!ASC) return;
    const int32 Mag = FMath::RoundToInt(ASC->GetNumericAttribute(UMechaAttributeSet::GetAmmoMagazineAttribute()));
    const int32 MaxMag = FMath::RoundToInt(Data.NewValue);
    const int32 Res = FMath::RoundToInt(ASC->GetNumericAttribute(UMechaAttributeSet::GetAmmoReserveAttribute()));
    ApplyAmmoToUI(Mag, MaxMag, Res);
}

void UWBP_MechaHUD::OnReserveChanged(const FOnAttributeChangeData& Data)
{
    if (!ASC) return;
    const int32 Mag = FMath::RoundToInt(ASC->GetNumericAttribute(UMechaAttributeSet::GetAmmoMagazineAttribute()));
    const int32 MaxMag = FMath::RoundToInt(ASC->GetNumericAttribute(UMechaAttributeSet::GetMaxMagazineAttribute()));
    const int32 Res = FMath::RoundToInt(Data.NewValue);
    ApplyAmmoToUI(Mag, MaxMag, Res);
}
