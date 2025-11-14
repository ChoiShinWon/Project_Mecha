#pragma once
#include "CoreMinimal.h"
#include "AIController.h"
#include "EnemyAirshipAIController.generated.h"

class UBehaviorTree;

UCLASS()
class PROJECT_MECHA_API AEnemyAirshipAIController : public AAIController
{
    GENERATED_BODY()
public:
    AEnemyAirshipAIController();

    virtual void OnPossess(APawn* InPawn) override;

protected:
    UPROPERTY(EditDefaultsOnly, Category = "AI")
    UBehaviorTree* BTAsset;
};
