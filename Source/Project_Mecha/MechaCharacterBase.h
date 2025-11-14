#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"
#include "InputActionValue.h"
#include "Blueprint/UserWidget.h"
#include "MechaCharacterBase.generated.h"

class UAbilitySystemComponent;
class UMechaAttributeSet;
class UInputMappingContext;
class UInputAction;
class USpringArmComponent;
class UCameraComponent;
class UGameplayEffect;
class UGameplayAbility;
class UWBP_MechaHUD;
class USceneComponent;
struct FOnAttributeChangeData;

UENUM(BlueprintType)
enum class EMechaAbilityInputID : uint8
{
    None = 255,
    QuickBoost = 0,
    Attack = 1,
    Hover = 2,
    BoostMode = 3,
    MissleFire = 4,
    AssaultBoost = 5,
    GunFire = 6,
    Reload = 7
};

UCLASS()
class PROJECT_MECHA_API AMechaCharacterBase : public ACharacter, public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    AMechaCharacterBase();

    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override { return AbilitySystem; }

    UFUNCTION(BlueprintCallable, Category = "GAS")
    UMechaAttributeSet* GetMechaAttributeSet() const { return AttributeSet; }

    // 라이프사이클/입력 바인딩
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

protected:
    virtual void OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode = 0) override;

    // ---- Components ----
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
    USpringArmComponent* SpringArm;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
    UCameraComponent* FollowCamera;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
    UAbilitySystemComponent* AbilitySystem;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
    UMechaAttributeSet* AttributeSet;

    // ---- Input assets ----
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input") UInputMappingContext* DefaultMappingContext;
    UPROPERTY(EditDefaultsOnly, Category = "Input") UInputAction* IA_Move;
    UPROPERTY(EditDefaultsOnly, Category = "Input") UInputAction* IA_Look;
    UPROPERTY(EditDefaultsOnly, Category = "Input") UInputAction* IA_Jump;

    UPROPERTY(EditDefaultsOnly, Category = "Input") UInputAction* IA_Attack;
    UPROPERTY(EditDefaultsOnly, Category = "Input") UInputAction* IA_Sprint; // QuickBoost
    UPROPERTY(EditDefaultsOnly, Category = "Input") UInputAction* IA_Hover;
    UPROPERTY(EditDefaultsOnly, Category = "Input") UInputAction* IA_MissleFire;

    UPROPERTY(EditDefaultsOnly, Category = "Input") UInputAction* IA_BoostMode;
    UPROPERTY(EditDefaultsOnly, Category = "Input") UInputAction* IA_AssaultBoost;
    UPROPERTY(EditDefaultsOnly, Category = "Input") UInputAction* IA_GunFire;
    UPROPERTY(EditDefaultsOnly, Category = "Input") UInputAction* IA_Reload;

    // AbilitySystemComponent 별칭(선택)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
    UAbilitySystemComponent* ASC = nullptr;

    // Input Function
    UFUNCTION() void Input_Hover_Pressed();
    UFUNCTION() void Input_Hover_Released();
    UFUNCTION() void Input_Attack_Pressed();
    UFUNCTION() void Input_Attack_Released();
    UFUNCTION() void Input_MissleFire(const FInputActionValue& Value);
    UFUNCTION() void Input_AssaultBoost(const FInputActionValue& Value);
    UFUNCTION() void Input_GunFire_Pressed();
    UFUNCTION() void Input_GunFire_Released();
    UFUNCTION() void Input_Reload_Pressed();

public:
    /** 총알 블루프린트 클래스 지정 (예: BP_Bullet) */
    UPROPERTY(EditDefaultsOnly, Category = "FireSocket")
    TSubclassOf<class AActor> ProjectileClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
    FName MuzzleSocketName = TEXT("FireSocket");

    /** 총구 위치 (Mesh의 소켓 또는 SceneComponent) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
    USceneComponent* MuzzleLocation;

protected:
    UPROPERTY(EditDefaultsOnly, Category = "GAS")
    int32 HoverInputID = 2;

    // ---- Gameplay Effects ----
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|Effects") TSubclassOf<class UGameplayEffect> GE_InitAttributes;
    UPROPERTY(EditDefaultsOnly, Category = "GAS|QuickBoost") TSubclassOf<UGameplayEffect> GE_QuickBoost;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|Effects") TSubclassOf<class UGameplayEffect> GE_EnergyRegen_Infinite;

    // 에너지 재생
    UPROPERTY() FActiveGameplayEffectHandle EnergyRegenEffectHandle;

    // UI
    UPROPERTY(EditDefaultsOnly, Category = "UI")
    TSubclassOf<class UWBP_MechaHUD> HUDWidgetClass;

    UPROPERTY(BlueprintReadWrite)
    UWBP_MechaHUD* MechaHUDWidget = nullptr;

    // ---- Default Tags ----
    UPROPERTY(EditDefaultsOnly, Category = "GAS|Tags", meta = (Categories = "State,Ability,Status,Input"))
    FGameplayTagContainer DefaultOwnedTags;

    // ---- Startup Abilities ----
    UPROPERTY(EditDefaultsOnly, Category = "GAS|Abilities")
    TArray<TSubclassOf<UGameplayAbility>> StartupAbilities;

    UPROPERTY() UMechaAttributeSet* AttributeSetVar = nullptr;

    void InitASCOnce();

private:
    bool bASCInitialized = false;

    // Attribute 반응
    FDelegateHandle MoveSpeedChangedHandle;
    void OnMoveSpeedChanged(const FOnAttributeChangeData& Data);
    void ApplyMoveSpeedToCharacter(float NewSpeed);

    FDelegateHandle EnergyChangedHandle;
    void OnEnergyChanged(const FOnAttributeChangeData& Data);

    // Health
    float GetHealth() const;
    float GetMaxHealth() const;
    FDelegateHandle HealthChangedHandle;
    void OnHealthChanged(const FOnAttributeChangeData& Data);

    // Cached tags
    FGameplayTag Tag_Boosting;
    FGameplayTag Tag_Overheated;

    // QuickBoost 튜닝
    UPROPERTY(EditDefaultsOnly, Category = "QuickBoost") float LaunchXY = 1500.f;
    UPROPERTY(EditDefaultsOnly, Category = "QuickBoost") float LaunchZ = 200.f;

    // Overheat
    FTimerHandle Timer_OverheatClear;
    UPROPERTY(EditDefaultsOnly, Category = "GAS|Energy") float OverheatLockout = 1.0f;

    // Input handlers (이동/룩/점프만—부스트는 GA로)
    void Input_Move(const FInputActionValue& Value);
    void Input_Look(const FInputActionValue& Value);
    void Input_JumpStart(const FInputActionValue& Value);
    void Input_JumpStop(const FInputActionValue& Value);
    void Input_SprintStart(const FInputActionValue& Value);
    void Input_SprintStop(const FInputActionValue& Value);
    void Input_BoostMode_Pressed(const FInputActionValue& Value);
};
