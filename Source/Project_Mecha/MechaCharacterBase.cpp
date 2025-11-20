#include "MechaCharacterBase.h"
#include "AbilitySystemComponent.h"
#include "MechaAttributeSet.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "WBP_MechaHUD.h"
#include "GA_Attack.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "MissionManager.h"
#include "Kismet/GameplayStatics.h"

AMechaCharacterBase::AMechaCharacterBase()
{
    bReplicates = true;
    PrimaryActorTick.bCanEverTick = true;

    // Camera
    SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
    SpringArm->SetupAttachment(RootComponent);
    SpringArm->TargetArmLength = 400.f;
    SpringArm->bUsePawnControlRotation = true;

    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
    FollowCamera->bUsePawnControlRotation = false;

    // Movement
    auto Move = GetCharacterMovement();
    Move->bOrientRotationToMovement = true;
    Move->RotationRate = FRotator(0.f, 720.f, 0.f);
    Move->MaxWalkSpeed = 300.f;
    Move->JumpZVelocity = 600.f;
    Move->AirControl = 0.4f;

    // GAS
    AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystem"));
    AbilitySystem->SetIsReplicated(true);
    AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

    AttributeSet = CreateDefaultSubobject<UMechaAttributeSet>(TEXT("AttributeSet"));

    // 총구
    MuzzleLocation = CreateDefaultSubobject<USceneComponent>(TEXT("FireSocket"));
    MuzzleLocation->SetupAttachment(GetMesh(), MuzzleSocketName);
}

void AMechaCharacterBase::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    // MoveSpeedMultiplier 적용 (BaseSpeed * Multiplier)
    UCharacterMovementComponent* BaseMove = GetCharacterMovement();
    if (BaseMove && AttributeSet)
    {
        float BaseSpeed = 300.f;
        float Multiplier = AttributeSet->GetMoveSpeedMultiplier();
        BaseMove->MaxWalkSpeed = BaseSpeed * Multiplier;
    }

    // Hovering 강제 유지 - "지금 Hovering인가?"는 bIsHovering만 믿는다
    if (bIsHovering)
    {
        if (auto* Move = GetCharacterMovement())
        {
            if (Move->MovementMode != MOVE_Flying)
            {
                Move->SetMovementMode(MOVE_Flying);
                Move->GravityScale = 0.05f;
            }

            if (AbilitySystem && AbilitySystem->HasMatchingGameplayTag(Tag_Boosting))
            {
                Move->Velocity.Z = FMath::Max(Move->Velocity.Z, 100.f);
            }
        }
    }
}


void AMechaCharacterBase::BeginPlay()
{
    Super::BeginPlay();

    InitASCOnce();

    // Input Mapping Context
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

    // 서버 전용 처리 (초기 어트리뷰트 & 무한 회복 GE)
    if (HasAuthority() && AbilitySystem)
    {
        FGameplayEffectContextHandle Ctx = AbilitySystem->MakeEffectContext();
        Ctx.AddSourceObject(this);

        // Init Attributes
        if (GE_InitAttributes)
        {
            FGameplayEffectSpecHandle Spec = AbilitySystem->MakeOutgoingSpec(GE_InitAttributes, 1.f, Ctx);
            if (Spec.IsValid())
            {
                AbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
            }
        }

        if (AttributeSet) { AttributeSet->SetHealth(AttributeSet->GetMaxHealth()); }

        // 무한 회복 효과
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

    // 클라이언트 HUD 세팅 (ASC/Attrs 주입 → BP에서 타이머 시작)
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
                if (!MechaHUDWidget->IsInViewport())
                {
                    MechaHUDWidget->AddToViewport();
                }

                // AttributeSet은 ASC에서 직접 획득 (const로 안전하게 전달)
                const UMechaAttributeSet* Attrs =
                    AbilitySystem ? AbilitySystem->GetSet<UMechaAttributeSet>() : nullptr;

                MechaHUDWidget->InitWithASC(AbilitySystem, Attrs);

                // === [ADD] 시작 프레임 헬스 퍼센트 세팅 ===
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

void AMechaCharacterBase::InitASCOnce()
{
    if (bASCInitialized || !AbilitySystem || !AttributeSet) return;
    bASCInitialized = true;

    // 포인터 통일(선택)
    ASC = AbilitySystem;

    AbilitySystem->InitAbilityActorInfo(this, this);

    // AttributeSet 캐시(ASC에서 보강)
    AttributeSet = AbilitySystem ? const_cast<UMechaAttributeSet*>(AbilitySystem->GetSet<UMechaAttributeSet>()) : nullptr;

    // Cache tags
    Tag_Boosting = FGameplayTag::RequestGameplayTag(TEXT("State.Boosting"));
    Tag_Overheated = FGameplayTag::RequestGameplayTag(TEXT("State.Overheated"));
    Tag_StateHovering = FGameplayTag::RequestGameplayTag(TEXT("State.Hovering"));

    // DefaultOwnedTags
    if (DefaultOwnedTags.Num() > 0)
        AbilitySystem->AddLooseGameplayTags(DefaultOwnedTags);

    // Startup Abilities (server)
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

    // Attribute delegates
    MoveSpeedChangedHandle =
        AbilitySystem->GetGameplayAttributeValueChangeDelegate(AttributeSet->GetMoveSpeedAttribute())
        .AddUObject(this, &AMechaCharacterBase::OnMoveSpeedChanged);
    ApplyMoveSpeedToCharacter(AttributeSet->GetMoveSpeed());

    EnergyChangedHandle =
        AbilitySystem->GetGameplayAttributeValueChangeDelegate(AttributeSet->GetEnergyAttribute())
        .AddUObject(this, &AMechaCharacterBase::OnEnergyChanged);

    // === [ADD] Health 변경 델리게이트 바인딩 ===
    HealthChangedHandle =
        AbilitySystem->GetGameplayAttributeValueChangeDelegate(AttributeSet->GetHealthAttribute())
        .AddUObject(this, &AMechaCharacterBase::OnHealthChanged);
}

void AMechaCharacterBase::OnMoveSpeedChanged(const FOnAttributeChangeData& Data)
{
    ApplyMoveSpeedToCharacter(Data.NewValue);
}

void AMechaCharacterBase::ApplyMoveSpeedToCharacter(float NewSpeed)
{
    if (auto* MoveComp = GetCharacterMovement())
        MoveComp->MaxWalkSpeed = NewSpeed;
}

void AMechaCharacterBase::OnEnergyChanged(const FOnAttributeChangeData& Data)
{
    const float NewEnergy = Data.NewValue;

    if (NewEnergy <= 0.01f)
    {
        if (!AbilitySystem->HasMatchingGameplayTag(Tag_Overheated))
            AbilitySystem->AddLooseGameplayTag(Tag_Overheated);

        GetWorldTimerManager().ClearTimer(Timer_OverheatClear);
        GetWorldTimerManager().SetTimer(Timer_OverheatClear, [this]()
            {
                if (AbilitySystem) AbilitySystem->RemoveLooseGameplayTag(Tag_Overheated);
            }, OverheatLockout, false);
    }
}

// ----------------------------플레이어 입력 모두 이곳에서 처리--------------------------------------
void AMechaCharacterBase::Input_Move(const FInputActionValue& Value)
{
    const FVector2D Axis = Value.Get<FVector2D>();
    if (!Controller || Axis.IsNearlyZero()) return;

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

void AMechaCharacterBase::Input_JumpStart(const FInputActionValue&) { Jump(); }
void AMechaCharacterBase::Input_JumpStop(const FInputActionValue&) { StopJumping(); }

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

void AMechaCharacterBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        if (IA_Move)   EIC->BindAction(IA_Move, ETriggerEvent::Triggered, this, &AMechaCharacterBase::Input_Move);
        if (IA_Look)   EIC->BindAction(IA_Look, ETriggerEvent::Triggered, this, &AMechaCharacterBase::Input_Look);
        if (IA_Jump) {
            EIC->BindAction(IA_Jump, ETriggerEvent::Started, this, &AMechaCharacterBase::Input_JumpStart);
            EIC->BindAction(IA_Jump, ETriggerEvent::Completed, this, &AMechaCharacterBase::Input_JumpStop);
            EIC->BindAction(IA_Jump, ETriggerEvent::Canceled, this, &AMechaCharacterBase::Input_JumpStop);
        }

        if (IA_Sprint)
        {
            EIC->BindAction(IA_Sprint, ETriggerEvent::Started, this, &AMechaCharacterBase::Input_SprintStart);
            EIC->BindAction(IA_Sprint, ETriggerEvent::Completed, this, &AMechaCharacterBase::Input_SprintStop);
        }

        if (IA_Hover)
        {
            EIC->BindAction(IA_Hover, ETriggerEvent::Started, this, &AMechaCharacterBase::Input_Hover_Pressed);
            EIC->BindAction(IA_Hover, ETriggerEvent::Completed, this, &AMechaCharacterBase::Input_Hover_Released);
            EIC->BindAction(IA_Hover, ETriggerEvent::Canceled, this, &AMechaCharacterBase::Input_Hover_Released);
        }

        if (IA_BoostMode)
        {
            EIC->BindAction(IA_BoostMode, ETriggerEvent::Started, this, &AMechaCharacterBase::Input_BoostMode_Pressed);
        }

        if (IA_Attack)
        {
            EIC->BindAction(IA_Attack, ETriggerEvent::Started, this, &AMechaCharacterBase::Input_Attack_Pressed);
            EIC->BindAction(IA_Attack, ETriggerEvent::Completed, this, &AMechaCharacterBase::Input_Attack_Released);
        }

        if (IA_MissleFire)
        {
            EIC->BindAction(IA_MissleFire, ETriggerEvent::Started, this, &AMechaCharacterBase::Input_MissleFire);
        }

        if (IA_AssaultBoost)
        {
            EIC->BindAction(IA_AssaultBoost, ETriggerEvent::Started, this, &AMechaCharacterBase::Input_AssaultBoost);
        }

        if (IA_GunFire)
        {
            EIC->BindAction(IA_GunFire, ETriggerEvent::Started, this, &AMechaCharacterBase::Input_GunFire_Pressed);
            EIC->BindAction(IA_GunFire, ETriggerEvent::Completed, this, &AMechaCharacterBase::Input_GunFire_Released);
        }

        if (IA_Reload)
        {
            EIC->BindAction(IA_Reload, ETriggerEvent::Started, this, &AMechaCharacterBase::Input_Reload_Pressed);
        }
    }
}

void AMechaCharacterBase::OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode)
{
    Super::OnMovementModeChanged(PrevMovementMode, PreviousCustomMode);

    if (!AbilitySystem) return;

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

// -------------------------------------------------------------------------------------------------



void AMechaCharacterBase::SetHovering(bool bNewHover)
{
    bIsHovering = bNewHover;
}

// === Health 연동 ===
void AMechaCharacterBase::OnHealthChanged(const FOnAttributeChangeData& Data)
{
    if (!MechaHUDWidget || !AttributeSet) return;

    const float OldHealth = Data.OldValue;
    const float NewHealth = Data.NewValue;
    const float MaxHealth = AttributeSet->GetMaxHealth();

    // 1) HP 바 갱신
    MechaHUDWidget->SetHealthPercent(MaxHealth > 0.f ? NewHealth / MaxHealth : 0.f);

    // 2) HP가 줄어들었을 때만 화면 테두리 연출 실행
    if (NewHealth < OldHealth)
    {
        const float Damage = OldHealth - NewHealth;

        // BP에서 구현한 이벤트 호출
        MechaHUDWidget->PlayDamageOverlay(Damage);
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


