// EnemyAirshipAIController.cpp
// 비행선용 AI 컨트롤러 - Behavior Tree 실행

#include "EnemyAirshipAIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"

// ========================================
// 생성자
// ========================================
AEnemyAirshipAIController::AEnemyAirshipAIController()
{
	bWantsPlayerState = false;
}

// ========================================
// Pawn 빙의 시 호출
// ========================================
void AEnemyAirshipAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// Behavior Tree 실행
	if (BTAsset)
	{
		RunBehaviorTree(BTAsset);
	}
}
