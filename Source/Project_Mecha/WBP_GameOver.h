// WBP_GameOver.h
// 게임오버 화면 위젯

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WBP_GameOver.generated.h"

UCLASS()
class PROJECT_MECHA_API UWBP_GameOver : public UUserWidget
{
	GENERATED_BODY()

public:
	// 게임오버 화면 표시 (페이드 인 효과)
	UFUNCTION(BlueprintCallable, Category = "GameOver")
	void ShowGameOver(float FadeInDuration = 2.0f);

	// 게임오버 화면 숨기기
	UFUNCTION(BlueprintCallable, Category = "GameOver")
	void HideGameOver();

	// 레벨 재시작 (Restart 버튼용)
	UFUNCTION(BlueprintCallable, Category = "GameOver")
	void RestartLevel();

	// 메인 메뉴로 이동 (선택사항)
	UFUNCTION(BlueprintCallable, Category = "GameOver")
	void GoToMainMenu();

protected:
	virtual void NativeConstruct() override;

	// 페이드 인 타이머
	FTimerHandle FadeInTimerHandle;
	float CurrentFadeAlpha = 0.0f;
	float TargetFadeAlpha = 1.0f;
	float FadeSpeed = 1.0f;

	void UpdateFadeIn();

	// 메인 메뉴 레벨 이름 (블루프린트에서 설정 가능)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameOver|Settings")
	FName MainMenuLevelName = FName(TEXT("MainMenu"));

	// 리스폰 또는 메인 메뉴 함수 (블루프린트에서 구현 가능)
	UFUNCTION(BlueprintImplementableEvent, Category = "GameOver")
	void OnGameOverShown();
};

