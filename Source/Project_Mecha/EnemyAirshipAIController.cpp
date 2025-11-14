#include "EnemyAirshipAIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"

AEnemyAirshipAIController::AEnemyAirshipAIController()
{
    bWantsPlayerState = false;
}

void AEnemyAirshipAIController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);

    if (BTAsset)
    {
        RunBehaviorTree(BTAsset);
    }
}
