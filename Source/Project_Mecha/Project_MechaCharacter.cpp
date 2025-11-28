// Project_MechaCharacter.cpp
// 언리얼 템플릿 캐릭터 (현재 미사용 - MechaCharacterBase 사용 중)

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Project_MechaCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"

DEFINE_LOG_CATEGORY(LogTemplateCharacter);

// ========================================
// 생성자
// ========================================
AProject_MechaCharacter::AProject_MechaCharacter()
{
	// ========== 충돌 캡슐 크기 ==========
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
		
	// ========== 컨트롤러 회전 설정 ==========
	// 카메라만 회전하고 캐릭터는 회전하지 않음
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// ========== 캐릭터 이동 설정 ==========
	GetCharacterMovement()->bOrientRotationToMovement = true;  // 이동 방향으로 회전
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);

	GetCharacterMovement()->JumpZVelocity = 700.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	// ========== 카메라 붐 생성 ==========
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	// ========== 카메라 생성 ==========
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
}

// ========================================
// BeginPlay
// ========================================
void AProject_MechaCharacter::BeginPlay()
{
	Super::BeginPlay();

	// ========== Enhanced Input 등록 ==========
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
}

// ========================================
// 입력 바인딩
// ========================================
void AProject_MechaCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) 
	{
		// ========== 점프 ==========
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		// ========== 이동 ==========
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AProject_MechaCharacter::Move);

		// ========== 시점 ==========
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AProject_MechaCharacter::Look);
	}
	else
	{
		UE_LOG(LogTemplateCharacter, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

// ========================================
// 이동 입력 처리
// ========================================
void AProject_MechaCharacter::Move(const FInputActionValue& Value)
{
	// 2D 벡터 입력
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// ========== 전방 방향 계산 ==========
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// ========== 이동 입력 적용 ==========
		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

// ========================================
// 시점 입력 처리
// ========================================
void AProject_MechaCharacter::Look(const FInputActionValue& Value)
{
	// 2D 벡터 입력
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// 컨트롤러 회전 적용
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}
