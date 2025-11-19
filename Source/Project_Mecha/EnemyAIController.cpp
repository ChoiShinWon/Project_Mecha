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
        const bool bBBOK = UseBlackboard(BT->BlackboardAsset, BlackboardComp);
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

    // 여기서 HomeLocation / IsDead 키 세팅
    if (BlackboardComp)
    {
        // 블랙보드에 있는 키 이름과 정확히 맞춰야 한다.
        // Blackboard 에서 Vector Key 이름이 "HomeLocation" 인지 확인할 것.
        const FVector HomeLoc = Enemy->GetActorLocation();   // 스폰 위치 기준
        BlackboardComp->SetValueAsVector(TEXT("HomeLocation"), HomeLoc);

        // 선택사항: 죽음 플래그 초기화
        BlackboardComp->SetValueAsBool(TEXT("IsDead"), false);
    }

    // === Behavior Tree 실행 ===
    const bool bBTOK = RunBehaviorTree(BT);
    if (!bBTOK)
    {
        UE_LOG(LogTemp, Warning, TEXT("EnemyAIController::OnPossess - RunBehaviorTree FAILED"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("EnemyAIController::OnPossess - BT started successfully"));
}
