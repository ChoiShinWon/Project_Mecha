// GA_Attack.cpp
// 설명:
// - UGA_Attack 클래스의 구현부.
// - 근접 공격 판정 로직 (구체 스윕), GAS/엔진 데미지 적용, 히트 이펙트 재생.
//
// Description:
// - Implementation of UGA_Attack class.
// - Melee attack detection logic (sphere sweep), GAS/engine damage application, hit effect playback.

#include "GA_Attack.h"
#include "MechaCharacterBase.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "GameplayEffectExtension.h"

#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "GameFramework/Controller.h"

// 생성자: Instancing Policy, Net Execution Policy, Ability Tags 설정.
// Constructor: Sets Instancing Policy, Net Execution Policy, and Ability Tags.
UGA_Attack::UGA_Attack()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
    AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Ability.Attack")));
}

// Ability 활성화 로직: Blueprint 이벤트 OnAttackTriggered 호출.
// - CommitAbility로 코스트/쿨다운 체크.
// - OnAttackTriggered 이벤트를 BP에서 구현하여 몽타주 시작.
//
// Ability activation logic: Calls Blueprint event OnAttackTriggered.
// - Checks cost/cooldown via CommitAbility.
// - OnAttackTriggered event implemented in BP to start montage.
void UGA_Attack::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    AMechaCharacterBase* Mecha = Cast<AMechaCharacterBase>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr);
    if (!Mecha)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // BP(몽타주 시작/노티파이 등)로 트리거 넘김
    OnAttackTriggered(Mecha);
}

void UGA_Attack::EndAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility, bool bWasCancelled)
{
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);

    if (const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
    {
        // 필요 시 태그 정리 등
    }
}

// 공격 판정 함수: 구체 스윕으로 전방의 적을 감지하고 데미지 적용.
// - 캐릭터 전방으로 구체 스윕 트레이스.
// - 히트 시 ApplyDamage_GASOrEngine 호출하여 데미지 적용.
// - 히트 이펙트 및 사운드 재생.
//
// Attack trace function: Detects enemies ahead via sphere sweep and applies damage.
// - Sphere sweep trace forward from character.
// - On hit, calls ApplyDamage_GASOrEngine to apply damage.
// - Plays hit effect and sound.
void UGA_Attack::PerformAttackTrace(AActor* SourceActor)
{
    if (!SourceActor) return;
    UWorld* World = SourceActor->GetWorld();
    if (!World) return;

    const FVector Start = SourceActor->GetActorLocation() + FVector(0, 0, 50);
    const FVector End = Start + (SourceActor->GetActorForwardVector() * AttackRange);

    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(GA_AttackTrace), false, SourceActor);
    const bool bHit = World->SweepSingleByChannel(
        Hit, Start, End, FQuat::Identity, ECC_Pawn,
        FCollisionShape::MakeSphere(AttackRadius), Params);

    if (!bHit || !Hit.GetActor())
    {
        UE_LOG(LogTemp, Verbose, TEXT("[GA_Attack] No hit"));
        return;
    }

    AActor* HitActor = Hit.GetActor();

    // 데미지 적용 (GAS 우선, 폴백으로 엔진 Damage)
    ApplyDamage_GASOrEngine(SourceActor, HitActor, Hit.ImpactPoint);

    // 피드백
    if (HitEffect) UGameplayStatics::SpawnEmitterAtLocation(World, HitEffect, Hit.ImpactPoint);
    if (HitSound)  UGameplayStatics::PlaySoundAtLocation(World, HitSound, Hit.ImpactPoint);
}

// 대상 액터에 GAS 데미지를 우선 적용, ASC가 없으면 엔진 기본 데미지로 폴백.
// - 대상의 ASC를 찾아 GE_MeleeDamage를 SetByCaller로 적용.
// - ASC가 없으면 엔진 기본 ApplyDamage 사용.
//
// Applies GAS damage to target actor first, falls back to engine damage if no ASC.
// - Finds target's ASC and applies GE_MeleeDamage via SetByCaller.
// - Uses engine's ApplyDamage if no ASC.
void UGA_Attack::ApplyDamage_GASOrEngine(AActor* SourceActor, AActor* HitActor, const FVector& HitPoint)
{
    if (!SourceActor || !HitActor) return;

    // 1) 대상의 ASC 획득
    UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor);
    UAbilitySystemComponent* SourceASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(SourceActor);

    if (TargetASC && GE_MeleeDamage)
    {
        // SetByCaller로 전달할 스펙 생성
        FGameplayEffectContextHandle Ctx = SourceASC ? SourceASC->MakeEffectContext() : FGameplayEffectContextHandle();
        Ctx.AddSourceObject(SourceActor);

        FGameplayEffectSpecHandle SpecHandle = (SourceASC ? SourceASC : TargetASC)->MakeOutgoingSpec(GE_MeleeDamage, 1.f, Ctx);
        if (SpecHandle.IsValid())
        {
            // SetByCaller(Damage) = AttackDamage
            SpecHandle.Data->SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(*SetByCallerDamageName.ToString()), AttackDamage);

            // 대상에 적용
            TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
            UE_LOG(LogTemp, Log, TEXT("[GA_Attack] Applied GAS Damage %.1f to %s"), AttackDamage, *HitActor->GetName());
            return;
        }
    }

    // 2) 폴백: 엔진 기본 데미지
    UGameplayStatics::ApplyDamage(
        HitActor, AttackDamage,
        SourceActor->GetInstigatorController(),
        SourceActor, /*DamageTypeClass*/ nullptr);

    UE_LOG(LogTemp, Log, TEXT("[GA_Attack] Fallback ApplyDamage %.1f to %s"), AttackDamage, *HitActor->GetName());
}
