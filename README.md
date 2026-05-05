# 🤖 Project: FRONTLINE

![메인 게임플레이](./Images/Skill2.gif)
![부스트 및 전투 연출](./Images/Skill7.gif)

> "Unreal Engine 5의 GAS(Gameplay Ability System)와 Event-Driven 아키텍처를 기반으로 설계된 3D 메카 액션 게임"  
> "A 3D mecha action game designed with Unreal Engine 5 GAS and an event-driven architecture."

---

## 📖 프로젝트 개요 (Overview)

- **개발 기간:** 2025.10.30 ~ 2025.12.15 (약 2개월)  
  **Development Period:** 2025.10.30 ~ 2025.12.15 (about 2 months)
- **개발 인원:** 1인 개발  
  **Team Size:** Solo development
- **사용 엔진:** Unreal Engine 5.3  
  **Engine:** Unreal Engine 5.3
- **핵심 기술:** C++, Blueprint, GAS, Enhanced Input, UMG, Behavior Tree  
  **Core Tech:** C++, Blueprint, GAS, Enhanced Input, UMG, Behavior Tree

---

## 🛠️ 핵심 시스템 및 기술 명세 (Core Systems)

### 1. ⚙️ GAS 기반 스탯 및 어빌리티 시스템
캐릭터의 체력, 에너지, 탄약, 전투 스킬을 GAS 구조로 통합해 확장성과 유지보수성을 확보했습니다.  
Built a GAS based architecture for health, energy, ammo, and combat abilities with strong scalability and maintainability.

- **통합 Attribute 관리:** `MechaAttributeSet`에서 플레이어와 적의 공통 스탯을 중앙 관리  
  **Unified Attribute Management:** Centralized shared stats for player and enemy in `MechaAttributeSet`.
- **태그 기반 상태 제어:** `State.Overheated`, `Block.Fire` 등 GameplayTag로 조건 기반 제어  
  **Tag Driven State Control:** Conditional restrictions through gameplay tags such as `State.Overheated` and `Block.Fire`.
- **모듈형 어빌리티 설계:** `GA_AssaultBoost`, `GA_Hover`, `GA_Attack` 등 역할별 분리  
  **Modular Ability Design:** Role separated abilities such as `GA_AssaultBoost`, `GA_Hover`, and `GA_Attack`.
- **동적 데미지 처리:** `SetByCaller` 기반으로 상황별 데미지 수치 적용  
  **Dynamic Damage Application:** Context based damage values using `SetByCaller`.

### 2. ⚡ Tick-less Event-Driven UI 최적화
Tick 기반 UI 갱신을 제거하고, 데이터 변화 시점에만 반응하도록 이벤트 중심으로 구성했습니다.  
Replaced tick based UI updates with an event driven approach that updates only on data changes.

- **GAS Delegate 바인딩:** `GetGameplayAttributeValueChangeDelegate`로 체력, 에너지, 탄약 UI 갱신  
  **GAS Delegate Binding:** HUD updates for health, energy, and ammo via attribute change delegates.
- **미션 시스템 분리:** `MissionManager`에서 `bCanEverTick = false` 적용 후 이벤트 푸시 방식 구성  
  **Mission System Decoupling:** Disabled ticking in `MissionManager` and pushed mission updates only on events.
- **안전한 위젯 수명 관리:** 보스 처치 시 애니메이션 완료 타이밍에 맞춰 위젯 정리  
  **Safe Widget Lifecycle:** Removed widgets at exact animation completion timing after boss defeat.

### 3. 🎯 벡터 수학 기반 전투 연출 및 조준 처리
벡터 연산을 활용해 전투 피드백과 카메라 연출의 정확도와 몰입감을 높였습니다.  
Used vector math to improve combat feedback precision and camera feel.

- **방향 피격 판정:** Dot Product 기반 4방향 피격 몽타주 분기  
  **Directional Hit React:** Four direction montage selection using dot products.
- **다이내믹 카메라 쉬프트:** 입력 방향 기반 SpringArm 오프셋 보간  
  **Dynamic Camera Shift:** Input driven SpringArm offset interpolation.
- **유도 미사일 타겟팅:** 화면 조준점 계산 후 `HomingTargetComponent` 동적 지정  
  **Homing Missile Targeting:** Runtime homing target assignment from crosshair based aim projection.

### 4. 🤖 AI Controller 및 보스 패턴 설계
일반 적 전투 루프와 보스 전용 패턴을 분리하여 전투 난이도와 템포를 설계했습니다.  
Designed combat pacing by separating standard enemy loops and boss specific patterns.

- **보스 미사일 레인 패턴:** 공중 페이즈 전환 후 순차 유도 미사일 발사  
  **Boss Missile Rain Pattern:** Air phase transition with sequential homing missile fire.
- **패턴 안정성 보장:** 슈퍼아머 태그로 패턴 중단 상황 최소화  
  **Pattern Stability:** Reduced interruption risk with super armor tag during key phases.
- **락온 시스템 최적화:** 거리, 시야각 조건 기반 타겟 선택 및 부드러운 카메라 고정  
  **Optimized Lock-On:** Radius and FOV filtered target selection with smooth camera interpolation.

---

## 🎮 게임 조작법 (Controls)

| 키 (Key) | 액션 (Action) |
| :---: | :--- |
| **W, A, S, D** | 메카 이동 (Enhanced Input Axis) |
| **Space** | 점프 / 홀드 시 호버 |
| **L-Shift** | 퀵 부스트 |
| **L-Click** | 총기 발사 |
| **R** | 재장전 |
| **MMB** | 타겟 락온 |

---

## 📂 주요 소스 코드 구조 (Directory Structure)

- [**`MechaCharacterBase.cpp`**](./Source/Project_Mecha/MechaCharacterBase.cpp)  
  플레이어 코어 로직 (입력, 락온, 피격 방향 판정)  
  Player core logic (input, lock-on, directional hit reaction)
- [**`MechaAttributeSet.cpp`**](./Source/Project_Mecha/MechaAttributeSet.cpp)  
  GAS Attribute 정의 및 값 보정 로직  
  GAS attribute definitions and clamping logic
- [**`MissionManager.cpp`**](./Source/Project_Mecha/MissionManager.cpp)  
  Tick-less 미션 진행 및 보스 페이즈 관리  
  Tick-less mission progression and boss phase control
- [**`GA_AssaultBoost.cpp`**](./Source/Project_Mecha/GA_AssaultBoost.cpp)  
  에너지 소모 기반 부스트 능력 및 카메라 연출  
  Energy draining boost ability and camera effects
- [**`GA_MissleFire.cpp`**](./Source/Project_Mecha/GA_MissleFire.cpp)  
  미사일 발사 및 타겟 추적 로직  
  Missile launch and target tracking logic
- [**`WBP_MechaHUD.cpp`**](./Source/Project_Mecha/WBP_MechaHUD.cpp)  
  Delegate 기반 실시간 HUD 갱신  
  Delegate based real-time HUD updates

---

## ✅ 구현 포인트 요약 (Key Implementation Highlights)

- GAS 기반 전투 상태와 행동 제어 체계 구축  
  Built a GAS driven combat state and action control system.
- 이벤트 중심 UI 업데이트로 불필요한 프레임 비용 절감  
  Reduced unnecessary frame cost through event-driven UI updates.
- 벡터 수학 기반 타격 판정과 카메라 연출로 전투 몰입도 강화  
  Improved combat immersion with vector based hit logic and camera effects.
- 미션과 보스 페이즈를 분리한 구조로 콘텐츠 확장성 확보  
  Improved content scalability with a separated mission and boss phase architecture.
