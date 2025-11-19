// EnemyAIController.h

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "EnemyAIController.generated.h"

class UBlackboardComponent;

UCLASS()
class PROJECT_MECHA_API AEnemyAIController : public AAIController
{
    GENERATED_BODY()

public:
    AEnemyAIController();

    virtual void OnPossess(APawn* InPawn) override;

protected:
    // UseBlackboard 에서 생성/연결해주는 블랙보드 컴포넌트
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI", meta = (AllowPrivateAccess = "true"))
    UBlackboardComponent* BlackboardComp;

    UPROPERTY(EditDefaultsOnly, Category = "AI")
    UBehaviorTree* DefaultBehaviorTree;
};
