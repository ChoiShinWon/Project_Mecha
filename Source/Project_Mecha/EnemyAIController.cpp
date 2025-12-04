// EnemyAIController.cpp
// 적 메카용 AI 컨트롤러 - Behavior Tree 실행, 블랙보드 초기화

#include "EnemyAIController.h"
#include "EnemyMecha.h"

#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"

// ========================================
// 생성자
// ========================================
AEnemyAIController::AEnemyAIController()
{
	// 블랙보드 컴포넌트 생성
	BlackboardComp = CreateDefaultSubobject<UBlackboardComponent>(TEXT("BlackboardComp"));
}

// ========================================
// Pawn 빙의 시 호출
// ========================================
void AEnemyAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// ========== EnemyMecha 확인 ==========
	AEnemyMecha* Enemy = Cast<AEnemyMecha>(InPawn);
	if (!Enemy)
	{
		return;
	}

	// ========== Behavior Tree 가져오기 ==========
	UBehaviorTree* BT = Enemy->GetBehaviorTree();
	if (!BT)
	{
		return;
	}

	// ========== 블랙보드 초기화 ==========
	if (BT->BlackboardAsset)
	{
		const bool bBBOK = UseBlackboard(BT->BlackboardAsset, BlackboardComp);
		if (!bBBOK || !BlackboardComp)
		{
			return;
		}
	}
	else
	{
		return;
	}

	// ========== 블랙보드 초기값 설정 ==========
	if (BlackboardComp)
	{
		// 스폰 위치를 Home 위치로 설정
		const FVector HomeLoc = Enemy->GetActorLocation();
		BlackboardComp->SetValueAsVector(TEXT("HomeLocation"), HomeLoc);

		// 죽음 플래그 초기화
		BlackboardComp->SetValueAsBool(TEXT("IsDead"), false);
	}

	// ========== Behavior Tree 실행 ==========
	RunBehaviorTree(BT);
}
