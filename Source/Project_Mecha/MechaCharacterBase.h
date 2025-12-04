// MechaCharacterBase.h

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
class UAnimMontage;
class UParticleSystemComponent;
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

UENUM(BlueprintType)
enum class EHitReactDirection : uint8
{
    Front UMETA(DisplayName = "Front"),
    Back  UMETA(DisplayName = "Back"),
    Left  UMETA(DisplayName = "Left"),
    Right UMETA(DisplayName = "Right")
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
   

    // ---- Components ----
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
    USpringArmComponent* SpringArm;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
    UCameraComponent* FollowCamera;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
    UAbilitySystemComponent* AbilitySystem;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
    UMechaAttributeSet* AttributeSet;

    // ---- Overheat Particle System ----
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VFX")
    UParticleSystemComponent* OverheatParticleComponent;

    // Overheat 시 재생할 파티클 시스템 
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VFX")
    UParticleSystem* OverheatParticleSystem;

    // ---- Input assets ----
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputMappingContext* DefaultMappingContext;

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

    //  락온 입력 액션 (마우스 휠 버튼)
    UPROPERTY(EditDefaultsOnly, Category = "Input") UInputAction* IA_LockOn;

    // AbilitySystemComponent 별칭
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

    //  락온 토글 입력
    UFUNCTION() void Input_LockOnToggle(const FInputActionValue& Value);

public:
    /** 총알 블루프린트 클래스 지정 (BP_Bullet) */
    UPROPERTY(EditDefaultsOnly, Category = "FireSocket")
    TSubclassOf<class AActor> ProjectileClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
    FName MuzzleSocketName = TEXT("FireSocket");

    /** 총구 위치 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
    USceneComponent* MuzzleLocation;

    // === Hover 상태 플래그 접근 ===
    UFUNCTION(BlueprintCallable, Category = "Hover")
    void SetHovering(bool bNewHover);

    UFUNCTION(BlueprintPure, Category = "Hover")
    bool IsHovering() const { return bIsHovering; }

    // === Hit React ===
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HitReact")
    UAnimMontage* HitReactMontage;

    /** 공격자 월드 위치를 넘기면 앞/뒤/좌/우 방향 섹션 재생 */
    UFUNCTION(BlueprintCallable, Category = "HitReact")
    void PlayHitReactFromDirection(const FVector& AttackWorldLocation);

    // === Death 몽타주주 ===
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Death|Montage")
    UAnimMontage* DeathMontage;

    // 죽음 후 Ragdoll 활성화 여부 (true면 물리 시뮬레이션, false면 애니메이션 포즈 유지)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Death|Settings")
    bool bUseRagdollOnDeath = false;

    // 죽음 후 콜리전 비활성화 여부 (true면 적의 공격이 안 맞음)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Death|Settings")
    bool bDisableCollisionOnDeath = true;

    // 플레이어가 죽었는지 확인
    UFUNCTION(BlueprintPure, Category = "Death")
    bool IsDead() const { return bIsDead; }

    // === Overheat 상태 확인용 ===
    UFUNCTION(BlueprintPure, Category = "GAS|Energy")
    bool IsOverheated() const;

protected:
    UPROPERTY(EditDefaultsOnly, Category = "GAS")
    int32 HoverInputID = 2;

    // ---- Gameplay Effects ----
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|Effects")
    TSubclassOf<class UGameplayEffect> GE_InitAttributes;

    UPROPERTY(EditDefaultsOnly, Category = "GAS|Effects")
    TSubclassOf<UGameplayEffect> GE_QuickBoost;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|Effects")
    TSubclassOf<class UGameplayEffect> GE_EnergyRegen_Infinite;

    // 에너지 재생
    UPROPERTY()
    FActiveGameplayEffectHandle EnergyRegenEffectHandle;

    // UI
    UPROPERTY(EditDefaultsOnly, Category = "UI")
    TSubclassOf<class UWBP_MechaHUD> HUDWidgetClass;

    UPROPERTY(BlueprintReadWrite)
    UWBP_MechaHUD* MechaHUDWidget = nullptr;

    // Game Over UI
    UPROPERTY(EditDefaultsOnly, Category = "UI|GameOver")
    TSubclassOf<class UWBP_GameOver> GameOverWidgetClass;

    UPROPERTY(BlueprintReadWrite)
    class UWBP_GameOver* GameOverWidget = nullptr;

    // 게임오버 페이드 인 지속 시간
    UPROPERTY(EditDefaultsOnly, Category = "UI|GameOver")
    float GameOverFadeInDuration = 2.0f;

    // ---- Default Tags ----
    UPROPERTY(EditDefaultsOnly, Category = "GAS|Tags", meta = (Categories = "State,Ability,Status,Input"))
    FGameplayTagContainer DefaultOwnedTags;

    // ---- Startup Abilities ----
    UPROPERTY(EditDefaultsOnly, Category = "GAS|Abilities")
    TArray<TSubclassOf<UGameplayAbility>> StartupAbilities;

    UPROPERTY()
    UMechaAttributeSet* AttributeSetVar = nullptr;

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

    // Death
    bool bIsDead = false;
    void HandleDeath();
    void ShowGameOverScreen();

    // Cached tags
    FGameplayTag Tag_Boosting;
    FGameplayTag Tag_Overheated;
    FGameplayTag Tag_StateHovering;

    // QuickBoost 튜닝
    UPROPERTY(EditDefaultsOnly, Category = "QuickBoost")
    float LaunchXY = 1500.f;

    UPROPERTY(EditDefaultsOnly, Category = "QuickBoost")
    float LaunchZ = 200.f;

    // 부스팅 중 최소 상승 속도 (Z축)
    UPROPERTY(EditDefaultsOnly, Category = "QuickBoost")
    float MinBoostVelocityZ = 100.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hover", meta = (AllowPrivateAccess = "true"))
    bool bIsHovering = false;

    // Overheat
    FTimerHandle Timer_OverheatClear;

    // 에너지 0 됐을 때 Overheat 잠김 지속 시간 (초)
    UPROPERTY(EditDefaultsOnly, Category = "GAS|Energy")
    float OverheatLockout = 5.0f;

    // Input handlers (이동/룩/점프만 - 부스트는 GA로)
    void Input_Move(const FInputActionValue& Value);
    void Input_Look(const FInputActionValue& Value);
    void Input_JumpStart(const FInputActionValue& Value);
    void Input_JumpStop(const FInputActionValue& Value);
    void Input_SprintStart(const FInputActionValue& Value);
    void Input_SprintStop(const FInputActionValue& Value);
    void Input_BoostMode_Pressed(const FInputActionValue& Value);

    //  Lock-On 상태
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LockOn", meta = (AllowPrivateAccess = "true"))
    bool bIsLockedOn = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LockOn", meta = (AllowPrivateAccess = "true"))
    AActor* CurrentLockOnTarget = nullptr;

    // 카메라가 타겟 쪽으로 회전하는 속도
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LockOn", meta = (AllowPrivateAccess = "true"))
    float LockOnTurnSpeed = 8.0f;

    // 피치 제한
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LockOn", meta = (AllowPrivateAccess = "true"))
    float LockOnPitchMin = -60.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LockOn", meta = (AllowPrivateAccess = "true"))
    float LockOnPitchMax = 60.0f;

    // 타겟 탐색 거리/시야각
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LockOn", meta = (AllowPrivateAccess = "true"))
    float LockOnMaxDistance = 5000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LockOn", meta = (AllowPrivateAccess = "true"))
    float LockOnMaxAngle = 60.0f;

    // 락온 ON/OFF 때 기존 이동 회전 설정 기억
    bool bSavedUseControllerRotationYaw = false;
    bool bSavedOrientRotationToMovement = true;

    // 락온 로직
    void ToggleLockOn();
    void ClearLockOn();
    AActor* FindLockOnTarget();
    void UpdateLockOnView(float DeltaTime);
};
