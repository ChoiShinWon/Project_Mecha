// WBP_MechaHUD.cpp
// 플레이어 HUD 위젯 - 에너지, 체력, 탄약 표시

#include "WBP_MechaHUD.h"
#include "AbilitySystemComponent.h"
#include "MechaAttributeSet.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

// ========================================
// ASC 및 AttributeSet 초기화
// ========================================
void UWBP_MechaHUD::InitWithASC(UAbilitySystemComponent* InASC, const UMechaAttributeSet* InAttrs)
{
	ASC = InASC;
	Attrs = InAttrs;

	// 에너지 바인딩은 블루프린트에서 처리 (기존 흐름 유지)
	BP_BindAttributeListeners();

	// ========== 탄약 바인딩 ==========
	UnbindAmmoListeners();   // 중복 방지
	BindAmmoListeners();
	RefreshAmmoOnce();       // 초기값 즉시 표시
}

// ========================================
// 에너지 프로그레스 바 업데이트
// ========================================
void UWBP_MechaHUD::SetEnergyPercent(float Ratio)
{
	if (!PB_Energy) return;

	PB_Energy->SetPercent(Ratio);

	// ========== 에너지 비율에 따라 색상 변경 ==========
	const FLinearColor Picked =
		(Ratio >= CautionThreshold) ? ColorHigh :    // 60% 이상: 파란색
		(Ratio >= WarningThreshold) ? ColorMid :     // 30~60%: 주황색
		ColorLow;                                     // 30% 미만: 빨간색

	PB_Energy->SetFillColorAndOpacity(Picked);
}

// ========================================
// 체력 프로그레스 바 업데이트
// ========================================
void UWBP_MechaHUD::SetHealthPercent(float InPercent)
{
	if (!PB_Health) return;

	const float Clamped = FMath::Clamp(InPercent, 0.f, 1.f);
	PB_Health->SetPercent(Clamped);

	// ========== 체력 비율에 따라 색상 변경 ==========
	const FLinearColor HPColor =
		(Clamped >= HealthCautionThreshold) ? HealthColorHigh :  // 높음: 초록
		(Clamped >= HealthWarningThreshold) ? HealthColorMid :   // 중간: 주황
		HealthColorLow;                                          // 낮음: 빨강

	PB_Health->SetFillColorAndOpacity(HPColor);

	// ========== LOW HP 경고 표시 ==========
	if (Txt_LowHPWarning)
	{
		const float WarningThresholdHP = HealthWarningThreshold;  // 기본 0.3

		// 경고 임계값 이하면 경고 표시
		if (Clamped <= WarningThresholdHP)
		{
			Txt_LowHPWarning->SetVisibility(ESlateVisibility::Visible);
			
			// 깜빡이는 애니메이션 재생
			if (LowHP_WarningPulse)
			{
				PlayAnimation(LowHP_WarningPulse, 0.f, 0);  // 무한 루프
			}
		}
		else
		{
			// 경고 해제
			if (LowHP_WarningPulse)
			{
				StopAnimation(LowHP_WarningPulse);
			}
			Txt_LowHPWarning->SetVisibility(ESlateVisibility::Hidden);
		}
	}
}

// ========================================
// 탄약 Attribute 리스너 바인딩
// ========================================
void UWBP_MechaHUD::BindAmmoListeners()
{
	if (!ASC) return;

	// 탄창 변경 감지
	HandleMag = ASC->GetGameplayAttributeValueChangeDelegate(
		UMechaAttributeSet::GetAmmoMagazineAttribute())
		.AddUObject(this, &UWBP_MechaHUD::OnMagChanged);

	// 최대 탄창 변경 감지
	HandleMaxMag = ASC->GetGameplayAttributeValueChangeDelegate(
		UMechaAttributeSet::GetMaxMagazineAttribute())
		.AddUObject(this, &UWBP_MechaHUD::OnMaxMagChanged);

	// 예비탄 변경 감지
	HandleReserve = ASC->GetGameplayAttributeValueChangeDelegate(
		UMechaAttributeSet::GetAmmoReserveAttribute())
		.AddUObject(this, &UWBP_MechaHUD::OnReserveChanged);
}

// ========================================
// 탄약 Attribute 리스너 해제
// ========================================
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

// ========================================
// 초기 탄약 값 표시
// ========================================
void UWBP_MechaHUD::RefreshAmmoOnce() const
{
	if (!ASC) return;

	const int32 Mag = FMath::RoundToInt(ASC->GetNumericAttribute(UMechaAttributeSet::GetAmmoMagazineAttribute()));
	const int32 MaxMag = FMath::RoundToInt(ASC->GetNumericAttribute(UMechaAttributeSet::GetMaxMagazineAttribute()));
	const int32 Res = FMath::RoundToInt(ASC->GetNumericAttribute(UMechaAttributeSet::GetAmmoReserveAttribute()));

	ApplyAmmoToUI(Mag, MaxMag, Res);
}

// ========================================
// UI에 탄약 표시
// ========================================
void UWBP_MechaHUD::ApplyAmmoToUI(int32 Mag, int32 MaxMag, int32 Reserve) const
{
	// ========== 탄창 텍스트 ==========
	if (TxtAmmoMag)
	{
		TxtAmmoMag->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), Mag, MaxMag)));
	}

	// ========== 예비탄 텍스트 ==========
	if (TxtAmmoReserve)
	{
		TxtAmmoReserve->SetText(FText::AsNumber(Reserve));
	}

	// ========== 블루프린트 이벤트 호출 (추가 연출용) ==========
	const_cast<UWBP_MechaHUD*>(this)->BP_UpdateAmmoUI(Mag, MaxMag, Reserve);
}

// ========================================
// Attribute 변경 콜백들
// ========================================
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
