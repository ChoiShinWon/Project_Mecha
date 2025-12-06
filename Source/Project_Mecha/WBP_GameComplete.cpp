// WBP_GameComplete.cpp
// 게임 종료(승리) 화면 위젯

#include "WBP_GameComplete.h"
#include "TimerManager.h"
#include "Components/CanvasPanel.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"

void UWBP_GameComplete::NativeConstruct()
{
	Super::NativeConstruct();

	// 초기에는 숨김
	SetVisibility(ESlateVisibility::Hidden);
}

void UWBP_GameComplete::ShowGameComplete(float FadeInDuration)
{
	if (FadeInDuration <= 0.0f)
	{
		FadeInDuration = 2.0f;
	}

	// 위젯 표시
	SetVisibility(ESlateVisibility::Visible);

	// 초기 투명도 설정
	CurrentFadeAlpha = 0.0f;
	TargetFadeAlpha = 1.0f;
	FadeSpeed = 1.0f / FadeInDuration;

	// 페이드 인 타이머 시작
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			FadeInTimerHandle,
			this,
			&UWBP_GameComplete::UpdateFadeIn,
			0.016f,  // ~60fps
			true
		);
	}

	// 블루프린트 이벤트 호출
	OnGameCompleteShown();
}

void UWBP_GameComplete::HideGameComplete()
{
	// 타이머 정리
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FadeInTimerHandle);
	}

	SetVisibility(ESlateVisibility::Hidden);
}

void UWBP_GameComplete::UpdateFadeIn()
{
	// 알파 값 증가
	CurrentFadeAlpha += FadeSpeed * 0.016f;

	if (CurrentFadeAlpha >= TargetFadeAlpha)
	{
		CurrentFadeAlpha = TargetFadeAlpha;

		// 타이머 중지
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(FadeInTimerHandle);
		}
	}

	// 위젯 전체 투명도 설정
	SetRenderOpacity(CurrentFadeAlpha);
}

// ========================================
// 레벨 재시작
// ========================================
void UWBP_GameComplete::RestartLevel()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 현재 레벨 이름 가져오기
	FString CurrentLevelName = World->GetMapName();
	
	// 레벨 이름 정리 (PIE 프리픽스 제거)
	CurrentLevelName.RemoveFromStart(World->StreamingLevelsPrefix);

	// 레벨 재시작
	UGameplayStatics::OpenLevel(World, FName(*CurrentLevelName), false);
}

// ========================================
// 메인 메뉴로 이동
// ========================================
void UWBP_GameComplete::GoToMainMenu()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 메인 메뉴로 이동
	UGameplayStatics::OpenLevel(World, MainMenuLevelName, false);
}

