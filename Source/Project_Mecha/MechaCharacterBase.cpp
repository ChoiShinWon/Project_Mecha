// MechaCharacterBase.cpp
// 플레이어 메카 캐릭터 베이스 클래스 - GAS, 입력 처리, 락온, HUD

#include "MechaCharacterBase.h"

#include "AbilitySystemComponent.h"
#include "MechaAttributeSet.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "WBP_MechaHUD.h"
#include "WBP_GameOver.h"
#include "GA_Attack.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "MissionManager.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"

#include "EnemyMecha.h"
#include "Particles/ParticleSystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"

// ========================================
// 생성자
// ========================================
AMechaCharacterBase::AMechaCharacterBase()
{
    bReplicates = true;
    PrimaryActorTick.bCanEverTick = true;

    // ========== 카메라 설정 ==========
    SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
    SpringArm->SetupAttachment(RootComponent);
    SpringArm->TargetArmLength = 400.f;
    SpringArm->bUsePawnControlRotation = true;

    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
    FollowCamera->bUsePawnControlRotation = false;

    // ========== 이동 설정 ==========
    auto Move = GetCharacterMovement();
    Move->bOrientRotationToMovement = true;  // 이동 방향으로 회전
    Move->RotationRate = FRotator(0.f, 720.f, 0.f);
    Move->MaxWalkSpeed = 300.f;
    Move->JumpZVelocity = 600.f;
    Move->AirControl = 0.4f;

    // 락온용 기본 설정 백업
    bSavedUseControllerRotationYaw = bUseControllerRotationYaw;
    bSavedOrientRotationToMovement = Move->bOrientRotationToMovement;

    // ========== GAS 초기화 ==========
    AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystem"));
    AbilitySystem->SetIsReplicated(true);
    AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

    AttributeSet = CreateDefaultSubobject<UMechaAttributeSet>(TEXT("AttributeSet"));

    // ========== 총구 위치 컴포넌트 ==========
    MuzzleLocation = CreateDefaultSubobject<USceneComponent>(TEXT("FireSocket"));
    MuzzleLocation->SetupAttachment(GetMesh(), MuzzleSocketName);

    // ========== Overheat 파티클 컴포넌트 ==========
    OverheatParticleComponent = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("OverheatParticle"));
    OverheatParticleComponent->SetupAttachment(GetMesh());
    OverheatParticleComponent->bAutoActivate = false;  // 기본적으로 비활성화
    OverheatParticleComponent->SetRelativeLocation(FVector(0.f, 0.f, 50.f));  // 캐릭터 중심 위치
}

// ========================================
// Tick - 매 프레임 업데이트
// ========================================
void AMechaCharacterBase::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    // ========== 이동 속도 배율 적용 ==========
    UCharacterMovementComponent* BaseMove = GetCharacterMovement();
    if (BaseMove && AttributeSet)
    {
        float BaseSpeed = 300.f;
        float Multiplier = AttributeSet->GetMoveSpeedMultiplier();
        BaseMove->MaxWalkSpeed = BaseSpeed * Multiplier;
    }

    // ========== 호버링 상태 강제 유지 ==========
    if (bIsHovering)
    {
        if (auto* Move = GetCharacterMovement())
        {
            // Flying 모드가 아니면 강제로 전환
            if (Move->MovementMode != MOVE_Flying)
            {
                Move->SetMovementMode(MOVE_Flying);
                Move->GravityScale = 0.05f;
            }

            // 부스팅 중에는 최소 상승 속도 유지
            if (AbilitySystem && AbilitySystem->HasMatchingGameplayTag(Tag_Boosting))
            {
                Move->Velocity.Z = FMath::Max(Move->Velocity.Z, 100.f);
            }
        }
    }

    // ========== 락온 시 타겟 추적 ==========
    if (bIsLockedOn && CurrentLockOnTarget)
    {
        UpdateLockOnView(DeltaSeconds);
    }
}

// ========================================
// BeginPlay - 게임 시작 시 초기화
// ========================================
void AMechaCharacterBase::BeginPlay()
{
    Super::BeginPlay();

    // ASC 초기화
    InitASCOnce();

    // ========== Enhanced Input 등록 ==========
    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        if (UEnhancedInputLocalPlayerSubsystem* Subsys =
            ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
        {
            if (DefaultMappingContext)
            {
                Subsys->AddMappingContext(DefaultMappingContext, 0);
            }
        }
    }

    // ========== 서버에서 스탯 초기화 ==========
    if (HasAuthority() && AbilitySystem)
    {
        FGameplayEffectContextHandle Ctx = AbilitySystem->MakeEffectContext();
        Ctx.AddSourceObject(this);

        // 초기 스탯 적용
        if (GE_InitAttributes)
        {
            FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(GE_InitAttributes, 1.f, Ctx);
            if (Spec.IsValid())
            {
                AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
            }
        }

        // 체력을 최대치로 설정
        if (AttributeSet)
        {
            AttributeSet->SetHealth(AttributeSet->GetMaxHealth());
        }

        // 에너지 무한 회복 적용
        if (GE_EnergyRegen_Infinite)
        {
            FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(GE_EnergyRegen_Infinite, 1.f, Ctx);
            if (Spec.IsValid())
            {
                EnergyRegenEffectHandle = AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
                UE_LOG(LogTemp, Warning, TEXT("EnergyRegenEffect Applied Successfully!"));
            }
        }
    }

    // ========== HUD 위젯 생성 및 초기화 ==========
    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        if (HUDWidgetClass)
        {
            if (!MechaHUDWidget)
            {
                MechaHUDWidget = CreateWidget<UWBP_MechaHUD>(PC, HUDWidgetClass);
            }

            if (MechaHUDWidget)
            {
                // 뷰포트에 추가
                if (!MechaHUDWidget->IsInViewport())
                {
                    MechaHUDWidget->AddToViewport();
                }

                // ASC와 AttributeSet 전달
                const UMechaAttributeSet* Attrs =
                    AbilitySystem ? AbilitySystem->GetSet<UMechaAttributeSet>() : nullptr;

                MechaHUDWidget->InitWithASC(AbilitySystem, Attrs);

                // 초기 체력 바 설정
                if (AttributeSet)
                {
                    const float H = AttributeSet->GetHealth();
                    const float M = AttributeSet->GetMaxHealth();
                    MechaHUDWidget->SetHealthPercent(M > 0.f ? H / M : 0.f);
                }
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("MechaHUDWidget is null. Set HUDWidgetClass to WBP_MechaHUD in editor."));
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("HUDWidgetClass is not set on %s"), *GetName());
        }
    }
}

// ========================================
// ASC 한 번만 초기화
// ========================================
void AMechaCharacterBase::InitASCOnce()
{
    if (bASCInitialized || !AbilitySystem || !AttributeSet) return;
    bASCInitialized = true;

    // ASC 포인터 통일
    ASC = AbilitySystem;

    // ActorInfo 초기화
    AbilitySystem->InitAbilityActorInfo(this, this);

    // AttributeSet 캐시
    AttributeSet = AbilitySystem ? const_cast<UMechaAttributeSet*>(AbilitySystem->GetSet<UMechaAttributeSet>()) : nullptr;

    // ========== 게임플레이 태그 캐싱 ==========
    Tag_Boosting = FGameplayTag::RequestGameplayTag(TEXT("State.Boosting"));
    Tag_Overheated = FGameplayTag::RequestGameplayTag(TEXT("State.Overheated"));
    Tag_StateHovering = FGameplayTag::RequestGameplayTag(TEXT("State.Hovering"));

    // ========== 기본 소유 태그 적용 ==========
    if (DefaultOwnedTags.Num() > 0)
    {
        AbilitySystem->AddLooseGameplayTags(DefaultOwnedTags);
    }

    // ========== 시작 능력 부여 (서버에서만) ==========
    if (HasAuthority())
    {
        for (int32 i = 0; i < StartupAbilities.Num(); ++i)
        {
            if (TSubclassOf<UGameplayAbility> GAClass = StartupAbilities[i])
            {
                FGameplayAbilitySpec Spec(GAClass, 1, i, this);
                AbilitySystem->GiveAbility(Spec);
            }
        }
    }

    // ========== Attribute 변경 델리게이트 바인딩 ==========
    // 이동 속도 변경
    MoveSpeedChangedHandle =
        AbilitySystem->GetGameplayAttributeValueChangeDelegate(AttributeSet->GetMoveSpeedAttribute())
        .AddUObject(this, &AMechaCharacterBase::OnMoveSpeedChanged);
    ApplyMoveSpeedToCharacter(AttributeSet->GetMoveSpeed());

    // 에너지 변경
    EnergyChangedHandle =
        AbilitySystem->GetGameplayAttributeValueChangeDelegate(AttributeSet->GetEnergyAttribute())
        .AddUObject(this, &AMechaCharacterBase::OnEnergyChanged);

    // 체력 변경
    HealthChangedHandle =
        AbilitySystem->GetGameplayAttributeValueChangeDelegate(AttributeSet->GetHealthAttribute())
        .AddUObject(this, &AMechaCharacterBase::OnHealthChanged);
}

// ========================================
// Attribute 변경 콜백들
// ========================================
void AMechaCharacterBase::OnMoveSpeedChanged(const FOnAttributeChangeData& Data)
{
    ApplyMoveSpeedToCharacter(Data.NewValue);
}

void AMechaCharacterBase::ApplyMoveSpeedToCharacter(float NewSpeed)
{
    if (auto* MoveComp = GetCharacterMovement())
    {
        MoveComp->MaxWalkSpeed = NewSpeed;
    }
}

void AMechaCharacterBase::OnEnergyChanged(const FOnAttributeChangeData& Data)
{
    if (!AbilitySystem) return;

    const float NewEnergy = Data.NewValue;

    // ========== 에너지 고갈 시 과열 처리 ==========
    if (NewEnergy <= 0.01f)
    {
        // 이미 과열이 아니면 태그 추가
        if (!AbilitySystem->HasMatchingGameplayTag(Tag_Overheated))
        {
            AbilitySystem->AddLooseGameplayTag(Tag_Overheated);
            UE_LOG(LogTemp, Warning, TEXT("[Energy] Overheated ON"));

            // ========== Overheat 파티클 활성화 ==========
            if (OverheatParticleComponent)
            {
                // 블루프린트에서 설정한 파티클 시스템이 있으면 적용
                if (OverheatParticleSystem && OverheatParticleComponent->Template != OverheatParticleSystem)
                {
                    OverheatParticleComponent->SetTemplate(OverheatParticleSystem);
                }

                OverheatParticleComponent->Activate(true);
                
            }
        }

        // 일정 시간 후 과열 해제
        GetWorldTimerManager().ClearTimer(Timer_OverheatClear);
        GetWorldTimerManager().SetTimer(
            Timer_OverheatClear,
            [this]()
            {
                if (AbilitySystem)
                {
                    AbilitySystem->RemoveLooseGameplayTag(Tag_Overheated);
                    UE_LOG(LogTemp, Warning, TEXT("[Energy] Overheated OFF"));

                    // ========== Overheat 파티클 비활성화 ==========
                    if (OverheatParticleComponent)
                    {
                        OverheatParticleComponent->Deactivate();
                        UE_LOG(LogTemp, Log, TEXT("[VFX] Overheat Particle Deactivated"));
                    }
                }
            },
            OverheatLockout,
            false
        );
    }

    // 에너지 값 자체는 Attribute / GE에서 처리하니까
    // 여기서 굳이 SetEnergy로 건드릴 필요는 없다.
}

// ========================================
// 입력 핸들러들
// ========================================
void AMechaCharacterBase::Input_Move(const FInputActionValue& Value)
{
    const FVector2D Axis = Value.Get<FVector2D>();
    if (!Controller || Axis.IsNearlyZero()) return;

    // 컨트롤러 방향 기준으로 이동
    const FRotator YawRot(0.f, Controller->GetControlRotation().Yaw, 0.f);
    const FVector Forward = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
    const FVector Right = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);
    AddMovementInput(Forward, Axis.Y);
    AddMovementInput(Right, Axis.X);
}

void AMechaCharacterBase::Input_Look(const FInputActionValue& Value)
{
    const FVector2D Axis = Value.Get<FVector2D>();
    AddControllerYawInput(Axis.X);
    AddControllerPitchInput(Axis.Y);
}

void AMechaCharacterBase::Input_JumpStart(const FInputActionValue&)
{
    Jump();
}

void AMechaCharacterBase::Input_JumpStop(const FInputActionValue&)
{
    StopJumping();
}

void AMechaCharacterBase::Input_SprintStart(const FInputActionValue&)
{
    if (AbilitySystem)
        AbilitySystem->AbilityLocalInputPressed((int32)EMechaAbilityInputID::QuickBoost);
}

void AMechaCharacterBase::Input_SprintStop(const FInputActionValue&)
{
    if (AbilitySystem)
        AbilitySystem->AbilityLocalInputReleased((int32)EMechaAbilityInputID::QuickBoost);
}

void AMechaCharacterBase::Input_BoostMode_Pressed(const FInputActionValue&)
{
    if (AbilitySystem)
        AbilitySystem->AbilityLocalInputPressed((int32)EMechaAbilityInputID::BoostMode);
}

// ========================================
// 입력 바인딩 설정
// ========================================
void AMechaCharacterBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        // ========== 이동/룩 ==========
        if (IA_Move)
            EIC->BindAction(IA_Move, ETriggerEvent::Triggered, this, &AMechaCharacterBase::Input_Move);
        if (IA_Look)
            EIC->BindAction(IA_Look, ETriggerEvent::Triggered, this, &AMechaCharacterBase::Input_Look);

        // ========== 점프 ==========
        if (IA_Jump)
        {
            EIC->BindAction(IA_Jump, ETriggerEvent::Started, this, &AMechaCharacterBase::Input_JumpStart);
            EIC->BindAction(IA_Jump, ETriggerEvent::Completed, this, &AMechaCharacterBase::Input_JumpStop);
            EIC->BindAction(IA_Jump, ETriggerEvent::Canceled, this, &AMechaCharacterBase::Input_JumpStop);
        }

        // ========== 스프린트 ==========
        if (IA_Sprint)
        {
            EIC->BindAction(IA_Sprint, ETriggerEvent::Started, this, &AMechaCharacterBase::Input_SprintStart);
            EIC->BindAction(IA_Sprint, ETriggerEvent::Completed, this, &AMechaCharacterBase::Input_SprintStop);
        }

        // ========== 호버 ==========
        if (IA_Hover)
        {
            EIC->BindAction(IA_Hover, ETriggerEvent::Started, this, &AMechaCharacterBase::Input_Hover_Pressed);
            EIC->BindAction(IA_Hover, ETriggerEvent::Completed, this, &AMechaCharacterBase::Input_Hover_Released);
            EIC->BindAction(IA_Hover, ETriggerEvent::Canceled, this, &AMechaCharacterBase::Input_Hover_Released);
        }

        // ========== 부스트 모드 ==========
        if (IA_BoostMode)
        {
            EIC->BindAction(IA_BoostMode, ETriggerEvent::Started, this, &AMechaCharacterBase::Input_BoostMode_Pressed);
        }

        // ========== 근접 공격 ==========
        if (IA_Attack)
        {
            EIC->BindAction(IA_Attack, ETriggerEvent::Started, this, &AMechaCharacterBase::Input_Attack_Pressed);
            EIC->BindAction(IA_Attack, ETriggerEvent::Completed, this, &AMechaCharacterBase::Input_Attack_Released);
        }

        // ========== 미사일 ==========
        if (IA_MissleFire)
        {
            EIC->BindAction(IA_MissleFire, ETriggerEvent::Started, this, &AMechaCharacterBase::Input_MissleFire);
        }

        // ========== 어설트 부스트 ==========
        if (IA_AssaultBoost)
        {
            EIC->BindAction(IA_AssaultBoost, ETriggerEvent::Started, this, &AMechaCharacterBase::Input_AssaultBoost);
        }

        // ========== 총 발사 ==========
        if (IA_GunFire)
        {
            EIC->BindAction(IA_GunFire, ETriggerEvent::Started, this, &AMechaCharacterBase::Input_GunFire_Pressed);
            EIC->BindAction(IA_GunFire, ETriggerEvent::Completed, this, &AMechaCharacterBase::Input_GunFire_Released);
        }

        // ========== 장전 ==========
        if (IA_Reload)
        {
            EIC->BindAction(IA_Reload, ETriggerEvent::Started, this, &AMechaCharacterBase::Input_Reload_Pressed);
        }

        // ========== 락온 ==========
        if (IA_LockOn)
        {
            EIC->BindAction(IA_LockOn, ETriggerEvent::Started, this, &AMechaCharacterBase::Input_LockOnToggle);
        }
    }
}

// ========================================
// 이동 모드 변경 감지
// ========================================
void AMechaCharacterBase::OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode)
{
    Super::OnMovementModeChanged(PrevMovementMode, PreviousCustomMode);

    if (!AbilitySystem) return;

    // 호버링 중인데 Flying 모드가 아니면 강제로 복원
    if (AbilitySystem->HasMatchingGameplayTag(Tag_StateHovering))
    {
        if (auto* CM = GetCharacterMovement())
        {
            if (CM->MovementMode != MOVE_Flying)
            {
                CM->SetMovementMode(MOVE_Flying);
                CM->GravityScale = 0.05f;
                UE_LOG(LogTemp, Warning, TEXT("[Hover Guard] Forced back to Flying mode."));
            }
        }
    }
}

// ========================================
// 능력 입력 핸들러들
// ========================================
void AMechaCharacterBase::Input_Hover_Pressed()
{
    if (AbilitySystem)
        AbilitySystem->AbilityLocalInputPressed((int32)EMechaAbilityInputID::Hover);
}

void AMechaCharacterBase::Input_Hover_Released()
{
    if (AbilitySystem)
        AbilitySystem->AbilityLocalInputReleased((int32)EMechaAbilityInputID::Hover);
}

void AMechaCharacterBase::Input_Attack_Pressed()
{
    if (AbilitySystem)
        AbilitySystem->AbilityLocalInputPressed((int32)EMechaAbilityInputID::Attack);
}

void AMechaCharacterBase::Input_Attack_Released()
{
    if (AbilitySystem)
        AbilitySystem->AbilityLocalInputReleased((int32)EMechaAbilityInputID::Attack);
}

void AMechaCharacterBase::Input_MissleFire(const FInputActionValue&)
{
    if (AbilitySystem)
        AbilitySystem->AbilityLocalInputPressed((int32)EMechaAbilityInputID::MissleFire);
}

void AMechaCharacterBase::Input_AssaultBoost(const FInputActionValue&)
{
    if (AbilitySystem)
        AbilitySystem->AbilityLocalInputPressed((int32)EMechaAbilityInputID::AssaultBoost);
}

void AMechaCharacterBase::Input_GunFire_Pressed()
{
    if (AbilitySystem)
        AbilitySystem->AbilityLocalInputPressed((int32)EMechaAbilityInputID::GunFire);
}

void AMechaCharacterBase::Input_GunFire_Released()
{
    if (AbilitySystem)
        AbilitySystem->AbilityLocalInputReleased((int32)EMechaAbilityInputID::GunFire);
}

void AMechaCharacterBase::Input_Reload_Pressed()
{
    if (AbilitySystem)
        AbilitySystem->AbilityLocalInputPressed((int32)EMechaAbilityInputID::Reload);
}

void AMechaCharacterBase::Input_LockOnToggle(const FInputActionValue&)
{
    ToggleLockOn();
}

// ========================================
// 호버링 상태 설정
// ========================================
void AMechaCharacterBase::SetHovering(bool bNewHover)
{
    bIsHovering = bNewHover;
}

// ========================================
// 체력 변경 콜백 - HUD 업데이트 및 데미지 연출
// ========================================
void AMechaCharacterBase::OnHealthChanged(const FOnAttributeChangeData& Data)
{
    if (!MechaHUDWidget || !AttributeSet) return;

    const float OldHealth = Data.OldValue;
    const float NewHealth = Data.NewValue;
    const float MaxHealth = AttributeSet->GetMaxHealth();

    // 체력 바 업데이트
    MechaHUDWidget->SetHealthPercent(MaxHealth > 0.f ? NewHealth / MaxHealth : 0.f);

    // 피격 시 화면 테두리 연출
    if (NewHealth < OldHealth)
    {
        const float Damage = OldHealth - NewHealth;
        MechaHUDWidget->PlayDamageOverlay(Damage);
    }

    // 이미 죽었으면 더 처리 안 함
    if (bIsDead)
    {
        return;
    }

    // ========== 체력이 0 이하로 떨어지면 사망 ==========
    if (NewHealth <= 0.f)
    {
        bIsDead = true;

        // Dead 태그 추가
        if (AbilitySystem)
        {
            AbilitySystem->AddLooseGameplayTag(
                FGameplayTag::RequestGameplayTag(TEXT("State.Dead"))
            );
        }

        HandleDeath();
    }
}

float AMechaCharacterBase::GetHealth() const
{
    return AttributeSet ? AttributeSet->GetHealth() : 0.f;
}

float AMechaCharacterBase::GetMaxHealth() const
{
    return AttributeSet ? AttributeSet->GetMaxHealth() : 0.f;
}

// === Overheat 상태 확인 ===
bool AMechaCharacterBase::IsOverheated() const
{
    return (AbilitySystem && AbilitySystem->HasMatchingGameplayTag(Tag_Overheated));
}

// ========================================
// 사망 처리
// ========================================
void AMechaCharacterBase::HandleDeath()
{
    UE_LOG(LogTemp, Warning, TEXT("[Death] Player %s has died!"), *GetName());

    // ========== 입력 비활성화 ==========
    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        DisableInput(PC);
    }

    // ========== 이동 정지 ==========
    if (auto* Move = GetCharacterMovement())
    {
        Move->StopMovementImmediately();
        Move->DisableMovement();
    }

    // ========== 모든 어빌리티 취소 ==========
    if (AbilitySystem)
    {
        AbilitySystem->CancelAllAbilities();
    }

    // ========== 콜리전 비활성화 (옵션) ==========
    if (bDisableCollisionOnDeath)
    {
        // ========== 캡슐 콜리전 완전 비활성화 ==========
        if (UCapsuleComponent* Capsule = GetCapsuleComponent())
        {
            Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            UE_LOG(LogTemp, Log, TEXT("[Death] Capsule collision disabled for %s"), *GetName());
        }

        // ========== Mesh 콜리전 응답 변경 - Enemy가 감지/공격 못하게 ==========
        if (USkeletalMeshComponent* SkelMesh = GetMesh())
        {
            // Pawn 채널 무시 (Enemy AI 감지 방지)
            SkelMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
            
            // 커스텀 채널들 무시 (프로젝타일 등)
            SkelMesh->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Ignore);
            SkelMesh->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Ignore);
            
            // Visibility 채널 무시 (AI 시야 감지 방지)
            SkelMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
            
            UE_LOG(LogTemp, Log, TEXT("[Death] Mesh collision responses changed for %s"), *GetName());
        }
    }
    else
    {
        // 콜리전은 유지하되, AI 감지만 막기
        if (UCapsuleComponent* Capsule = GetCapsuleComponent())
        {
            // Pawn 채널 무시 (Enemy AI가 타겟으로 인식 못함)
            Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
            
            // Visibility 무시 (AI 시야에 안 보임)
            Capsule->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
            
            UE_LOG(LogTemp, Log, TEXT("[Death] AI detection disabled for %s"), *GetName());
        }
    }

    // ========== Ragdoll 또는 애니메이션 ==========
    if (bUseRagdollOnDeath)
    {
        // Ragdoll 모드: 물리 시뮬레이션 활성화
        if (USkeletalMeshComponent* SkelMesh = GetMesh())
        {
            SkelMesh->SetSimulatePhysics(true);
            SkelMesh->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
            SkelMesh->SetCollisionProfileName(TEXT("Ragdoll"));
            UE_LOG(LogTemp, Log, TEXT("[Death] Ragdoll activated for %s"), *GetName());
        }
    }
    else
    {
        // 애니메이션 모드: Death 몽타주 재생
        if (DeathMontage)
        {
            if (UAnimInstance* AnimInst = GetMesh()->GetAnimInstance())
            {
                // 몽타주 재생 (BlendOut 시간을 0으로 설정하면 마지막 포즈 유지)
                AnimInst->Montage_Play(DeathMontage, 1.0f);
                
                // 몽타주가 끝나도 마지막 프레임 유지
                // (BlendOut을 0으로 설정하거나, 애니메이션의 마지막 프레임에 Pose Snapshot 사용)
                UE_LOG(LogTemp, Log, TEXT("[Death] Playing death montage for %s"), *GetName());
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[Death] No death montage set for %s"), *GetName());
        }
    }

    // ========== 게임오버 UI 표시 ==========
    ShowGameOverScreen();
}

// ========================================
// 게임오버 화면 표시
// ========================================
void AMechaCharacterBase::ShowGameOverScreen()
{
    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC)
    {
        UE_LOG(LogTemp, Warning, TEXT("[GameOver] No PlayerController found"));
        return;
    }

    // ========== 게임오버 위젯 생성 (아직 없으면) ==========
    if (!GameOverWidget && GameOverWidgetClass)
    {
        GameOverWidget = CreateWidget<UWBP_GameOver>(PC, GameOverWidgetClass);
        if (GameOverWidget)
        {
            GameOverWidget->AddToViewport(999);  // 최상위 Z-Order
            UE_LOG(LogTemp, Log, TEXT("[GameOver] Game Over widget created"));
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[GameOver] Failed to create Game Over widget"));
            return;
        }
    }

    // ========== 게임오버 화면 표시 ==========
    if (GameOverWidget)
    {
        GameOverWidget->ShowGameOver(GameOverFadeInDuration);
        UE_LOG(LogTemp, Warning, TEXT("[GameOver] Game Over screen displayed"));

        // 마우스 커서 표시 (메뉴 선택 가능하도록)
        PC->bShowMouseCursor = true;
        PC->bEnableClickEvents = true;
        PC->bEnableMouseOverEvents = true;
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[GameOver] GameOverWidget is null. Set GameOverWidgetClass in Blueprint"));
    }
}

// ========================================
// 락온 시스템
// ========================================

// 락온 토글
void AMechaCharacterBase::ToggleLockOn()
{
    // 이미 락온 중이면 해제
    if (bIsLockedOn)
    {
        ClearLockOn();
        return;
    }

    // 새 타겟 찾기
    AActor* NewTarget = FindLockOnTarget();
    if (!NewTarget)
    {
        UE_LOG(LogTemp, Log, TEXT("LockOn: No target found"));
        return;
    }

    CurrentLockOnTarget = NewTarget;
    bIsLockedOn = true;

    // ========== 회전 방식 변경 (AC식) ==========
    if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
    {
        // 기존 설정 백업
        bSavedUseControllerRotationYaw = bUseControllerRotationYaw;
        bSavedOrientRotationToMovement = MoveComp->bOrientRotationToMovement;

        // 컨트롤러 회전 따라가도록 변경
        bUseControllerRotationYaw = true;
        MoveComp->bOrientRotationToMovement = false;
    }

    UE_LOG(LogTemp, Log, TEXT("LockOn: %s"), *NewTarget->GetName());
}

// 락온 해제
void AMechaCharacterBase::ClearLockOn()
{
    bIsLockedOn = false;
    CurrentLockOnTarget = nullptr;

    // ========== 회전 설정 복원 ==========
    if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
    {
        bUseControllerRotationYaw = bSavedUseControllerRotationYaw;
        MoveComp->bOrientRotationToMovement = bSavedOrientRotationToMovement;
    }

    UE_LOG(LogTemp, Log, TEXT("LockOn: Cleared"));
}

// 락온 타겟 찾기
AActor* AMechaCharacterBase::FindLockOnTarget()
{
    UWorld* World = GetWorld();
    if (!World) return nullptr;

    FVector MyLocation = GetActorLocation();

    // 플레이어 시점 방향
    FRotator ViewRot = Controller ? Controller->GetControlRotation() : GetActorRotation();
    FRotator YawRot(0.f, ViewRot.Yaw, 0.f);
    FVector  Forward = YawRot.Vector();

    // ========== 후보 수집 (EnemyMecha만) ==========
    TArray<AActor*> Candidates;
    UGameplayStatics::GetAllActorsOfClass(World, AEnemyMecha::StaticClass(), Candidates);

    float   BestDistSq = TNumericLimits<float>::Max();
    AActor* BestTarget = nullptr;

    // ========== 거리 및 각도 체크 ==========
    for (AActor* Candidate : Candidates)
    {
        if (!Candidate || Candidate == this) continue;

        FVector ToTarget = Candidate->GetActorLocation() - MyLocation;
        float   DistSq = ToTarget.SizeSquared();

        // 최대 거리 체크
        if (DistSq > FMath::Square(LockOnMaxDistance))
            continue;

        // 시야 각도 체크
        FVector Dir = ToTarget.GetSafeNormal();
        float   Dot = FVector::DotProduct(Forward, Dir);
        float   AngleDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Dot, -1.f, 1.f)));

        if (AngleDeg > LockOnMaxAngle)
            continue;

        // 가장 가까운 타겟 선택
        if (DistSq < BestDistSq)
        {
            BestDistSq = DistSq;
            BestTarget = Candidate;
        }
    }

    return BestTarget;
}

// 락온 시 카메라 타겟 추적
void AMechaCharacterBase::UpdateLockOnView(float DeltaTime)
{
    if (!Controller || !CurrentLockOnTarget)
    {
        ClearLockOn();
        return;
    }

    // 타겟이 죽었거나 제거되면 락온 해제
    if (!CurrentLockOnTarget->IsValidLowLevel() || CurrentLockOnTarget->IsPendingKill())
    {
        ClearLockOn();
        return;
    }

    const FVector ViewLocation = GetPawnViewLocation();
    const FVector TargetLocation = CurrentLockOnTarget->GetActorLocation();

    // ========== 타겟을 바라보는 회전 계산 ==========
    FRotator DesiredRot = UKismetMathLibrary::FindLookAtRotation(ViewLocation, TargetLocation);

    // 피치 각도 제한
    DesiredRot.Pitch = FMath::Clamp(DesiredRot.Pitch, LockOnPitchMin, LockOnPitchMax);

    // 부드럽게 보간
    const FRotator CurrentRot = Controller->GetControlRotation();
    const FRotator NewRot = FMath::RInterpTo(CurrentRot, DesiredRot, DeltaTime, LockOnTurnSpeed);

    Controller->SetControlRotation(NewRot);
}

// ========================================
// 피격 리액션 (방향에 따라 다른 애니메이션)
// ========================================
void AMechaCharacterBase::PlayHitReactFromDirection(const FVector& AttackWorldLocation)
{
    // ========== 죽었으면 HitReact 재생 안 함 ==========
    if (bIsDead)
    {
        return;
    }

    if (!HitReactMontage || !GetMesh())
    {
        return;
    }

    // ========== 공격자 방향 계산 ==========
    const FVector MyLocation = GetActorLocation();
    FVector ToAttacker = AttackWorldLocation - MyLocation;
    ToAttacker.Z = 0.f;  // XY 평면만 사용

    if (!ToAttacker.Normalize())
    {
        return;
    }

    const FVector Forward = GetActorForwardVector();
    const FVector Right = GetActorRightVector();

    const float ForwardDot = FVector::DotProduct(Forward, ToAttacker);
    const float RightDot = FVector::DotProduct(Right, ToAttacker);

    FName SectionName = NAME_None;

    // ========== 앞/뒤/좌/우 판정 ==========
    const float FrontBackThreshold = 0.7f;  // 약 ±45도
    if (ForwardDot > FrontBackThreshold)
    {
        SectionName = FName("Front");
    }
    else if (ForwardDot < -FrontBackThreshold)
    {
        SectionName = FName("Back");
    }
    else
    {
        // 좌우 판정
        if (RightDot >= 0.f)
        {
            SectionName = FName("Right");
        }
        else
        {
            SectionName = FName("Left");
        }
    }

    if (SectionName.IsNone())
    {
        return;
    }

    // ========== 히트 리액션 몽타주 재생 ==========
    if (UAnimInstance* Anim = GetMesh()->GetAnimInstance())
    {
        // 이미 재생 중이 아니면 시작
        if (!Anim->Montage_IsPlaying(HitReactMontage))
        {
            Anim->Montage_Play(HitReactMontage, 1.f);
        }

        // 해당 방향 섹션으로 점프
        Anim->Montage_JumpToSection(SectionName, HitReactMontage);

        UE_LOG(LogTemp, Verbose, TEXT("HitReact: Section %s"), *SectionName.ToString());
    }
}
