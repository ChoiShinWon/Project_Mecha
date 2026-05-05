# 🤖 Project: FRONTLINE

![프로젝트 커버](./Images/Cover.png)
![전투 및 부스트 연출](./Images/Combat.gif)

> Unreal Engine 5 기반 3D 메카 액션 게임입니다.  
> 본 저장소는 팀 결과물 전체 소개가 아닌, **개인 기여 중심 포트폴리오 문서**를 목적으로 작성되었습니다.

---

## 🔗 프로젝트 링크 (Links)
* **Repository:** [GitHub Repository 링크 입력](#)
* **시연 영상(선택):** [YouTube 또는 Vimeo 링크 입력](#)

---

## 📖 프로젝트 개요 (Overview)
* **프로젝트 성격:** 개인 포트폴리오용 정리 저장소
* **저장소 공개 여부:** Public
* **개발 기간:** 2025.10.30 ~ 2025.12.15 (약 2개월)
* **개발 인원:** 1인 개발
* **사용 엔진:** Unreal Engine 5.3
* **주요 라이브러리 및 시스템:** C++, Blueprint, GAS, Enhanced Input, UMG, Behavior Tree
* **대상 플랫폼:** PC (Windows)
* **대상 플랫폼 버전:** Windows 10 / Windows 11
* **지원 포지션 관점:** 게임플레이 프로그래머

---

## 🎯 핵심 기여 요약 (My Contributions)

### 1) 전투 루프 설계 및 조작감 구현
플레이어 입력부터 능력 실행, 피격 반응, UI 피드백까지 일관된 전투 루프를 설계했습니다.
* 이동, 점프, 부스트, 사격, 재장전 입력 체계 구현
* 리소스(체력, 에너지, 탄약)와 액션 상태 동기화
* 피격 방향 판정과 카메라 반응을 결합한 전투 피드백 구성

### 2) GAS 기반 전투 시스템 모듈화
확장 가능한 능력 구조를 목표로 Gameplay Ability를 기능 단위로 분리했습니다.
* `MechaAttributeSet` 기반 공통 스탯 통합 관리
* `GA_AssaultBoost`, `GA_Hover`, `GA_Attack`, `GA_GunFire`, `GA_Reload` 구현
* GameplayTag(`State.Overheated`, `Block.Fire`) 기반 상태 제어

### 3) Tick-less Event-Driven UI 최적화
매 프레임 갱신 방식 대신, 값 변경 시점에만 UI를 업데이트하도록 설계했습니다.
* Attribute Delegate 기반 HUD 갱신
* MissionManager의 Tick 비활성화(`bCanEverTick = false`)
* 미션 진행, 보스 체력, 전투 경고 UI를 이벤트 기반으로 분리 연동

### 4) 적 AI 및 보스 페이즈 패턴 구현
일반 적과 보스 로직을 분리해 전투 템포와 난이도 곡선을 구성했습니다.
* 일반 적의 추적, 공격, 피격, 사망 흐름 구현
* `GA_BossMissileRain` 기반 보스 패턴 구성
* 락온 타겟 탐색 최적화 (거리, 시야각 기반 필터링)

---

## 🧩 프로젝트 구조 및 개발 컨벤션 (Structure and Conventions)

### 💻 코드 스타일 및 아키텍처 규칙
* **Unreal Engine 표준 네이밍** 준수
* `UPROPERTY`, `UFUNCTION` 카테고리 명시
* `bool` 접두사 `b` 사용
* 전투 규칙과 상태는 C++ 중심으로 관리, 연출 및 배치는 Blueprint로 분리
* 이벤트 기반 업데이트를 우선 적용해 Tick 의존 최소화

### 📦 에셋 관리 방식 (Asset Management)
* C++ 클래스는 `Source/Project_Mecha`에서 기능 단위로 관리
* UMG, BP, 이펙트 에셋은 `Content` 하위에서 역할별 폴더로 분리 관리
* GAS 관련 Ability/Effect 에셋은 네이밍 규칙(`GA_`, `GE_`)으로 일관화

### 📂 폴더 구조 (Directory Structure)
```text
Source/Project_Mecha/
├─ MechaCharacterBase.*      # 플레이어 코어 전투 로직
├─ MechaAttributeSet.*       # GAS 스탯 정의 및 제어
├─ EnemyMecha.*              # 적/보스 전투 로직
├─ MissionManager.*          # 미션 진행 및 보스 페이즈 관리
├─ GA_*.*                    # Gameplay Ability 구현
├─ WBP_*.*                   # HUD/전투 UI 위젯 클래스
└─ ...
Content/
Config/
