# 🤖 Project: FRONTLINE

(Images/Frontline 스킬 (2).gif) (Images/Frontline 스킬 (7).gif) (Images/Frontline 스킬 (5).gif)

> "Unreal Engine 5의 GAS(Gameplay Ability System)와 Event-Driven 아키텍처를 기반으로 설계된 3D 메카 액션 게임"

## 📖 프로젝트 개요 (Overview)
- **개발 기간:** 2025.10.30 ~ 2025.12.15 ([2]개월)
- **개발 인원:** 1인 개발 
- **사용 엔진:** Unreal Engine 5.3
- **핵심 기술:** C++, Blueprint, GAS(Gameplay Ability System), Enhanced Input, UMG, Behavior Tree

---

## 🛠️ 핵심 시스템 및 기술 명세 (Core Systems)

### 1. ⚙️ GAS 기반의 견고한 스탯 및 어빌리티 시스템
캐릭터의 체력, 에너지, 탄약부터 각종 공격 스킬(어설트 부스트, 호버링, 미사일 발사)을 GAS 아키텍처로 구현하여 확장성과 안정성을 확보했습니다.

* **통합 Attribute 관리:** 
`MechaAttributeSet`을 통해 플레이어와 적의 공통 스탯(Health, Energy, Ammo)을 중앙 집중화.
* **상태 제어(Tag-Driven):** 
에너지가 고갈되면 `State.Overheated` 태그를 부여해 이동기를 제한하고, 탄약이 없으면 발사(`Block.Fire`)를 막는 등 태그 기반의 직관적인 상태 제어 구현.
* **모듈화된 어빌리티(Gameplay Ability):**
`GA_AssaultBoost`, `GA_Hover`: `MOVE_Flying` 모드 전환 및 실시간 에너지 소모(Drain GE) 제어.
  * `GA_Attack`: `SphereTrace`를 통한 타격 판정 및 `SetByCaller`를 활용한 동적 데미지 GE 적용.

### 2. ⚡ Tick 연산을 배제한 Event-Driven UI 최적화
매 프레임(Tick) 연산되는 무거운 UI 갱신 방식을 탈피하고, 변화가 있을 때만 반응하는 리스너(Listener) 패턴으로 UI를 완벽하게 최적화했습니다.
* **GAS Delegate 바인딩:** 탄약, 체력, 에너지 수치 변화 시 `GetGameplayAttributeValueChangeDelegate`를 통해 C++ 단에서 이벤트가 발생할 때만 UI를 업데이트. (`WBP_MechaHUD`, `WBP_EnemyHealth`)
* **Mission Manager 의존성 분리:** 전역 관리자인 `MissionManager`는 `bCanEverTick = false`로 설정하여 Tick을 차단. 적이 사망할 때만 `NotifyEnemyKilled`가 호출되어 UI에 킬 카운트를 푸시(Push)하도록 결합도를 최소화.
* **안전한 메모리 생명주기 관리:** 보스 처치 시 `FTimerManager`와 **Lambda** 식을 결합하여, DeathFade UMG 애니메이션이 종료되는 정확한 시점에 위젯을 메모리에서 해제(`RemoveFromParent`). (`BossHealthWidget`)

### 3. 🎯 3D 벡터 수학(Vector Math)을 활용한 물리 및 카메라 연출
단순한 애니메이션 재생을 넘어, 벡터 연산을 활용해 역동적인 게임 필(Game Feel)을 구현했습니다.
* **방향 기반 피격 연출 (Directional Hit-React):** 타격 위치와 플레이어의 `Forward`, `Right` 벡터 간의 **내적(Dot Product)**을 계산하여 4방향(Front/Back/Left/Right) 중 정확한 피격 몽타주 섹션을 동적 재생.
* **다이내믹 카메라 쉬프트:** 퀵부스트 사용 시 이동 입력 방향(`CachedMoveRight`)을 판별하고, `FMath::FInterpTo`로 스프링암(SpringArm) 오프셋을 이동 반대 방향으로 부드럽게 보간하여 속도감 극대화.
* **타겟팅 & 유도 미사일 (Homing System):** `DeprojectScreenPositionToWorld`로 화면 크로스헤어 기준 조준점을 계산하고, `ProjectileMovementComponent`의 `HomingTargetComponent`를 런타임에 동적으로 할당해 정밀한 추적 구현.

### 4. 🤖 AI Controller 및 보스전(Boss Phase) 패턴 설계
* **다단계 보스 패턴 (`GA_BossMissileRain`):** * 보스가 공중으로 부양(`LaunchCharacter` & `MOVE_Flying`)함과 동시에 `State.SuperArmor` 태그를 부여해 패턴 끊김을 방지.
  * C++ 타이머를 활용해 양쪽 소켓에서 플레이어의 현재 위치(LookAtRotation)를 추적하며 순차적으로 유도 미사일을 발사하는 복합 패턴 구현.
* **최적화된 락온(Lock-On) 시스템:** `OverlapMultiByChannel`로 반경 내 적을 1차 필터링하고, 플레이어 시야각(Dot Product 계산) 내에서 가장 가까운 적을 식별하여 카메라를 부드럽게 고정(`FMath::RInterpTo`).

---

## 🎮 게임 조작법 (Controls)

| 키 (Key) | 액션 (Action) |
| :---: | :--- |
| **W, A, S, D** | 메카 이동 (Enhanced Input Axis 제어) |
| **Space** | 점프 / 꾹 누르면 호버링(Hover) |
| **L-Shift** | 퀵 부스트 (방향 기반 카메라 쉬프트 적용) |
| **L-Click** | 총기 발사 |
| **R** | 재장전 |
| **MMB** | 타겟 락온 (가장 가까운 적 시점 고정) |

---

## 📂 주요 소스 코드 구조 (Directory Structure)
*(💡  아래 링크를 클릭하시면 해당 C++ 파일로 이동합니다.!)*

* [**`MechaCharacterBase.cpp`**](./Source/Project_Mecha/MechaCharacterBase.cpp) : 플레이어 코어 로직 (Enhanced Input, 락온 시스템, 방향 피격 판정)
* [**`MechaAttributeSet.cpp`**](./Source/Project_Mecha/MechaAttributeSet.cpp) : GAS 스탯 정의 및 클램프(Clamp) 로직
* [**`MissionManager.cpp`**](./Source/Project_Mecha/MissionManager.cpp) : Event-Driven 기반 Tick-less 미션 및 보스 페이즈 관리자
* [**`GA_AssaultBoost.cpp`**](./Source/Project_Mecha/GA_AssaultBoost.cpp) : 에너지 소모 및 동적 카메라 효과(FOV, 오프셋 보간) 어빌리티
* [**`GA_MissleFire.cpp`**](./Source/Project_Mecha/GA_MissleFire.cpp) : 타겟 탐색(`OverlapMultiByChannel`) 및 유도 미사일 타이머 순차 발사 로직
* [**`WBP_MechaHUD.cpp`**](./Source/Project_Mecha/WBP_MechaHUD.cpp) : Delegate 기반 실시간 UI 업데이트 로직
---
