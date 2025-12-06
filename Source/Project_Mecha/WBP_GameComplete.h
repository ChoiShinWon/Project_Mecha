// WBP_GameComplete.h
// 게임 종료(승리) 화면 위젯

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WBP_GameComplete.generated.h"

UCLASS()
class PROJECT_MECHA_API UWBP_GameComplete : public UUserWidget
{
	GENERATED_BODY()

public:
	// 게임 종료 화면 표시 (페이드 인 효과)
	UFUNCTION(BlueprintCallable, Category = "GameComplete")
	void ShowGameComplete(float FadeInDuration = 2.0f);

	// 게임 종료 화면 숨기기
	UFUNCTION(BlueprintCallable, Category = "GameComplete")
	void HideGameComplete();

	// 레벨 재시작 (Restart 버튼용)
	UFUNCTION(BlueprintCallable, Category = "GameComplete")
	void RestartLevel();

	// 메인 메뉴로 이동 (선택사항)
	UFUNCTION(BlueprintCallable, Category = "GameComplete")
	void GoToMainMenu();

protected:
	virtual void NativeConstruct() override;

	// 페이드 인 타이머
	FTimerHandle FadeInTimerHandle;
	float CurrentFadeAlpha = 0.0f;
	float TargetFadeAlpha = 1.0f;
	float FadeSpeed = 1.0f;

	void UpdateFadeIn();

	// 게임 종료 타이머
	FTimerHandle GameEndTimerHandle;
	
	// 게임 종료 함수 (5초 후 자동 호출)
	void QuitGameAfterDelay();

	// 메인 메뉴 레벨 이름 (블루프린트에서 설정 가능)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameComplete|Settings")
	FName MainMenuLevelName = FName(TEXT("MainMenu"));

	// 게임 종료까지 대기 시간 (초)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameComplete|Settings")
	float GameEndDelay = 5.0f;

	// 게임 종료 화면 표시 후 블루프린트 이벤트 호출
	UFUNCTION(BlueprintImplementableEvent, Category = "GameComplete")
	void OnGameCompleteShown();
};

