// GA_Hover.h
// 설명:
// - 호버링(공중 부유) 능력 클래스.
// - 입력을 누르고 있는 동안 Flying 모드로 전환하여 공중에 떠 있게 한다.
// - 에너지를 지속적으로 소모하며, 에너지가 0이 되면 과열 상태로 전환된다.
//
#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayTagContainer.h"
#include "AbilitySystemComponent.h"
#include "GameplayAbilitySpec.h"
#include "MechaCharacterBase.h" 
#include "GA_Hover.generated.h"


class UAbilitySystemComponent;
class UGameplayEffect;

// 설명:
// - 호버링 능력. 입력 누르는 동안 Flying 모드 유지, 에너지 소모.
// - 에너지가 0이 되면 자동으로 종료되고 과열 상태 적용.
//
// Description:
// - Hovering ability. Maintains Flying mode while input held, drains energy.
// - Auto-ends and applies overheating when energy reaches 0.
UCLASS()
class PROJECT_MECHA_API UGA_Hover : public UGameplayAbility
{
	GENERATED_BODY()

public:
	// 생성자: Instancing Policy 설정.
	// Constructor: Sets Instancing Policy.
	UGA_Hover();

	// ===== Tags (태그) =====

	// 호버 능력 태그.
	// Hover ability tag.
	UPROPERTY(EditDefaultsOnly, Category = "Hover|Tags")
	FGameplayTag Tag_AbilityHover;

	// 호버링 상태 태그 (활성화 중).
	// Hovering state tag (active).
	UPROPERTY(EditDefaultsOnly, Category = "Hover|Tags")
	FGameplayTag Tag_StateHovering;

	// 과열 상태 태그.
	// Overheating state tag.
	UPROPERTY(EditDefaultsOnly, Category = "Hover|Tags")
	FGameplayTag Tag_StateOverheat;

	// 호버 차단 태그.
	// Hover blocking tag.
	UPROPERTY(EditDefaultsOnly, Category = "Hover|Tags")
	FGameplayTag Tag_BlockHover;

	// 호버 쿨다운 태그.
	// Hover cooldown tag.
	UPROPERTY(EditDefaultsOnly, Category = "Hover|Tags")
	FGameplayTag Tag_CooldownHover;

	// ===== Effects (효과) =====

	// 호버 중 에너지 소모 GE.
	// Energy drain GE during hover.
	UPROPERTY(EditDefaultsOnly, Category = "Hover|Effects")
	TSubclassOf<UGameplayEffect> GE_Hover_EnergyDrain;

	// 호버 쿨다운 GE.
	// Hover cooldown GE.
	UPROPERTY(EditDefaultsOnly, Category = "Hover|Effects")
	TSubclassOf<UGameplayEffect> GE_Hover_Cooldown;

	// ===== Movement (이동) =====

	// Flying 모드 사용 여부.
	// Whether to use Flying mode.
	UPROPERTY(EditDefaultsOnly, Category = "Hover|Movement")
	bool bUseFlyingMode = true;

	// 호버 중 중력 스케일.
	// Gravity scale during hover.
	UPROPERTY(EditDefaultsOnly, Category = "Hover|Movement")
	float GravityScaleWhileHover = 0.05f;

	// 호버 종료 후 낙하 중력 스케일.
	// Gravity scale during fall after hover ends.
	UPROPERTY(EditDefaultsOnly, Category = "Hover|Movement")
	float ExitFallGravityScale = 1.2f;

	// 호버 시작 시 상승 임펄스.
	UPROPERTY(EditDefaultsOnly, Category = "Hover|Movement")
	float HoverLiftImpulse = 1800.f;

	// ===== Energy (에너지) =====

	// 호버 시작에 필요한 최소 에너지.
	// Minimum energy required to start hover.
	UPROPERTY(EditDefaultsOnly, Category = "Hover|Energy")
	float MinEnergyToStart = 5.f;

	// 과열 쿨다운 시간 (초).
	// Overheating cooldown time (seconds).
	UPROPERTY(EditDefaultsOnly, Category = "Hover|Energy")
	float CooldownSeconds = 2.5f;

	// ===== Gameplay Cues =====

	// Gameplay Cue 태그 컨테이너.
	// Gameplay Cue tag container.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|Cue")
	FGameplayTagContainer GameplayCues;

	// ===== Camera Effects (카메라 효과) =====

	// FOV 변경 활성화 여부.
	// Enable FOV change.
	UPROPERTY(EditDefaultsOnly, Category = "Hover|Camera")
	bool bEnableFOVChange = false;

	// 호버 중 FOV 값.
	// FOV value during hover.
	UPROPERTY(EditDefaultsOnly, Category = "Hover|Camera", meta = (EditCondition = "bEnableFOVChange"))
	float HoverFOV = 95.0f;

	// FOV 전환 속도 (높을수록 빠름).
	// FOV transition speed (higher = faster).
	UPROPERTY(EditDefaultsOnly, Category = "Hover|Camera", meta = (EditCondition = "bEnableFOVChange"))
	float FOVBlendSpeed = 3.0f;

	// FOV 복원 속도 (높을수록 빠름).
	// FOV restore speed (higher = faster).
	UPROPERTY(EditDefaultsOnly, Category = "Hover|Camera", meta = (EditCondition = "bEnableFOVChange"))
	float FOVRestoreSpeed = 5.0f;

	// 카메라 거리 변경 활성화 여부.
	// Enable camera distance change.
	UPROPERTY(EditDefaultsOnly, Category = "Hover|Camera")
	bool bEnableCameraDistance = false;

	// 호버 중 카메라 거리.
	// Camera distance during hover.
	UPROPERTY(EditDefaultsOnly, Category = "Hover|Camera", meta = (EditCondition = "bEnableCameraDistance"))
	float HoverCameraDistance = 400.0f;

	// 카메라 쉐이크 클래스.
	// Camera shake class.
	UPROPERTY(EditDefaultsOnly, Category = "Hover|Camera")
	TSubclassOf<class UCameraShakeBase> HoverCameraShake;

protected:
	// ===== Runtime Variables (런타임 변수) =====

	// 능력 소유 캐릭터.
	// Ability owner character.
	UPROPERTY() ACharacter* OwnerCharacter = nullptr;

	// Ability System Component.
	UPROPERTY() UAbilitySystemComponent* ASC = nullptr;

	// 에너지 소모 GE 핸들.
	// Energy drain GE handle.
	FActiveGameplayEffectHandle DrainGEHandle;

	// 에너지 변화 델리게이트 핸들.
	// Energy change delegate handle.
	FDelegateHandle EnergyChangedHandle;

	// 호버 상승 보정 타이머 핸들.
	// Hover lift adjustment timer handle.
	FTimerHandle HoverAscendTimer;

	// 호버 상승 보정 함수 (타이머에서 주기적 호출).
	// Hover lift adjustment function (called periodically by timer).
	void ApplyHoverLift();

	// 저장된 이동 설정 (호버 종료 시 복원).
	// Saved movement settings (restored on hover end).
	float SavedGravityScale = 1.f;
	float SavedBrakingFrictionFactor = 0.f;
	float SavedGroundFriction = 0.f;

	// ===== Camera State (카메라 상태) =====
	float OriginalFOV = 90.f;
	float OriginalCameraDistance = 400.f;
	float CurrentFOV = 90.f;
	float TargetFOV = 90.f;
	float CurrentCameraDistance = 400.f;
	float TargetCameraDistance = 400.f;

	bool bIsRestoring = false;  // 복원 중인지 여부

	FTimerHandle CameraUpdateTimer;
	FTimerHandle CleanupTimer;  // Failsafe 타이머

	// ===== Ability Overrides (능력 오버라이드) =====

	// Ability 활성화 로직: 에너지 체크, 호버 시작, 에너지 소모 GE 적용.
	// Ability activation logic: Checks energy, starts hover, applies energy drain GE.
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	// 입력 해제 시 호출: 호버 중지, 능력 종료.
	// Called on input release: Stops hover, ends ability.
	virtual void InputReleased(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo) override;

	// Ability 종료 로직: 델리게이트 해제, 태그 정리.
	// Ability end logic: Unbinds delegates, cleans up tags.
	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;

	// ===== Internal Functions (내부 함수) =====

	// 호버 시작: Flying 모드 전환, 상승 임펄스 적용.
	// Starts hover: Switches to Flying mode, applies lift impulse.
	void StartHover();

	// 호버 중지: Falling 모드 복원, 이동 설정 복원.
	// Stops hover: Restores Falling mode, restores movement settings.
	void StopHover(bool bFromEnergyDepleted);

	// 에너지가 충분한지 확인.
	// Checks if energy is sufficient.
	bool IsEnergySufficient(float Threshold = 0.f) const;

	// 에너지 소모 GE 적용.
	// Applies energy drain GE.
	void ApplyDrainGE();

	// 에너지 소모 GE 제거.
	// Removes energy drain GE.
	void RemoveDrainGE();

	// 카메라 효과 적용.
	// Applies camera effects.
	void ApplyCameraEffects();

	// 카메라 효과 복원.
	// Restores camera effects.
	void RestoreCameraEffects();

	// 카메라 부드러운 업데이트 (매 프레임 호출).
	// Smooth camera update (called every frame).
	UFUNCTION()
	void UpdateCameraSmooth();

};
