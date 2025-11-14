// EnemyAIController.cpp

#include "EnemyAIController.h"
#include "EnemyMecha.h"

#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"

AEnemyAIController::AEnemyAIController()
{
    
    BlackboardComp = CreateDefaultSubobject<UBlackboardComponent>(TEXT("BlackboardComp"));
}

void AEnemyAIController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);

    AEnemyMecha* Enemy = Cast<AEnemyMecha>(InPawn);
    if (!Enemy)
    {
        UE_LOG(LogTemp, Warning, TEXT("EnemyAIController::OnPossess - Pawn is NOT EnemyMecha"));
        return;
    }

    UBehaviorTree* BT = Enemy->GetBehaviorTree();
    if (!BT)
    {
        UE_LOG(LogTemp, Warning, TEXT("EnemyAIController::OnPossess - BehaviorTreeAsset is NULL"));
        return;
    }

    // === 블랙보드 초기화 ===
    if (BT->BlackboardAsset)
    {
        bool bBBOK = UseBlackboard(BT->BlackboardAsset, BlackboardComp);
        if (!bBBOK || !BlackboardComp)
        {
            UE_LOG(LogTemp, Warning, TEXT("EnemyAIController::OnPossess - UseBlackboard FAILED"));
            return;
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("EnemyAIController::OnPossess - BehaviorTree has no BlackboardAsset"));
        return;
    }

    // === Behavior Tree 실행 ===
    bool bBTOK = RunBehaviorTree(BT);
    if (!bBTOK)
    {
        UE_LOG(LogTemp, Warning, TEXT("EnemyAIController::OnPossess - RunBehaviorTree FAILED"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("EnemyAIController::OnPossess - BT started successfully"));
}
