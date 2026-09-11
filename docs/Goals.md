# LightWeightGameFramework 목표 및 원칙

## 1. Product Goal

LightWeightGameFramework는 **저사양 Windows PC에서도 개발하고 실행할 수 있는 작은 2D 게임을 위한 개인용 C++ Framework**를 만드는 프로젝트다.

Unity나 Unreal Engine과 경쟁하는 범용 게임 엔진을 만드는 것이 목적은 아니다.

별도의 Editor에 의존하지 않고 코드 중심으로 게임을 개발할 수 있으며, 개발 환경과 실행 환경의 자원 사용량을 작게 유지해야 하는 프로젝트에서 사용할 수 있는 경량 개발 수단을 목표로 한다.

Window 관리, Input 처리, Rendering 초기화, Resource 수명 관리 등 게임마다 반복되는 저수준 작업을 Framework가 담당하여, 게임 코드가 실제 게임 로직에 더 집중할 수 있도록 한다.

시각적 편집 Tool은 Framework의 필수 사용 방식으로 두지 않는다. 실제 게임 제작 과정에서 반복 작업이나 시각적 배치가 명확한 병목으로 확인된 경우에만 해당 문제를 해결하는 최소 범위의 Tool을 검토한다.

---

## 2. 해결하려는 문제

범용 게임 엔진은 높은 생산성을 제공하지만, 작은 2D 게임에서는 Editor와 Runtime이 실제 프로젝트에서 필요한 것보다 더 많은 CPU, Memory, Storage 및 작업 환경 비용을 요구할 수 있다.

이 프로젝트에서는 **실제 게임을 제작할 수 있을 정도의 편의성을 유지하면서 2D 게임 개발 환경과 Runtime을 어느 수준까지 경량화할 수 있는지** 검증한다.

따라서 LightWeightGameFramework는 기존 게임 엔진을 완전히 대체하기 위한 프로젝트가 아니다.

Editor, Tooling, Ecosystem, 다양한 Platform 지원 등이 실제 프로젝트에 큰 가치를 제공한다면 범용 게임 엔진을 사용한다.

반대로 작은 2D 게임이며 코드 중심 개발이 적합하고, 개발 및 실행 환경의 자원 사용량을 줄이는 것이 중요하다면 LightWeightGameFramework를 선택할 수 있도록 하는 것이 목표다.

Framework 자체에 범용 Editor를 포함시키는 것을 기본 전제로 하지 않는다. 다만 실제 게임 개발에서 코드만으로 처리하기 어려운 반복 작업이나 시각적 편집 문제가 확인되면, 해당 문제를 해결하는 작은 전용 Tool은 별도로 검토할 수 있다.

---

## 3. 대상 사용 환경

초기 대상은 다음과 같다.

- 개인 개발
- Windows PC
- 작은 규모의 2D 게임
- 코드 중심 개발
- 저사양 개발 및 실행 환경
- 독립 실행형 Release Build

초기 버전은 다른 개발자를 위한 범용 게임 엔진 API나 공개 SDK를 목표로 하지 않는다.

---

## 4. 핵심 Requirement

Framework를 이용해 최종적으로 다음 작업을 수행할 수 있어야 한다.

- Application을 정상적으로 실행하고 종료할 수 있다.
- Window를 생성하고 관리할 수 있다.
- Game Loop를 실행할 수 있다.
- Frame 시간 정보를 얻을 수 있다.
- Keyboard와 Mouse 입력을 받을 수 있다.
- 기본적인 2D Graphics를 출력할 수 있다.
- Texture Resource를 로드하고 사용할 수 있다.
- 기본적인 Audio를 재생할 수 있다.
- 필요할 경우 Game State 또는 Scene을 관리할 수 있다.
- Editor 또는 별도 제작 Tool 없이도 실행 가능한 Release Build를 생성할 수 있다.

위 항목은 처음부터 모두 구현해야 하는 기능 목록이 아니다.

실제 게임 또는 현재 실행 가능한 Milestone에서 필요성이 발생했을 때 구현한다.

---

## 5. Non-Goals

초기 Framework에서는 다음 기능을 만들지 않는다.

- Unity와 유사한 범용 Game Editor
- 3D Engine
- Visual Scripting
- 범용 ECS
- 자체 Physics Engine
- Networking Framework
- Multiplayer
- Android 지원
- iOS 지원
- Linux 지원
- 범용 UI Editor
- Plugin System
- 자체 Programming Language
- 자체 Audio Engine
- 자체 Asset 제작 Tool

특정 게임 제작 과정에서 반복 작업, 실수 또는 시각적 배치 문제가 명확한 병목으로 확인된 경우에는 그 문제를 해결하는 소규모 전용 Tool을 별도로 검토할 수 있다.

실제 프로젝트에서 명확한 필요성이 확인된 경우에만 이후 다시 검토한다.

---

## 6. 설계 원칙

### Game First

Framework는 게임 제작 작업을 줄이기 위해 존재한다.

Framework 자체의 기능 개발 때문에 실제 게임 제작이 장기간 중단되는 상태를 피한다.

가능하면 다음 흐름으로 개발한다.

```text
게임에서 필요 발생
        ↓
현재 문제를 해결하는 최소 구현
        ↓
실제 게임에서 사용
        ↓
반복되거나 공통적인 문제인지 확인
        ↓
필요할 경우에만 일반화
        ↓
Framework 기능으로 확정
```

### 기본은 최소 구현

"게임 엔진이라면 있어야 할 것 같다"는 이유만으로 기능을 추가하지 않는다.

기능을 추가하기 전에 다음을 확인한다.

- 개발 시간을 줄이는가?
- 반복 작업을 줄이는가?
- 실수를 줄이는가?
- Game 측 코드의 가독성이나 사용성을 높이는가?
- 실제 프로젝트에서 사용할 가능성이 높은가?
- Framework 복잡도 증가 비용보다 충분한 가치가 있는가?

명확하지 않다면 구현을 미루거나 보류한다.

### Tool은 실제 병목이 확인된 후 만든다

Framework의 기본 사용 방식은 코드 중심으로 유지한다.

게임 데이터나 Object를 코드로 다루는 것이 실제 개발에 충분하다면 별도의 편집 Tool을 만들지 않는다.

다음과 같은 문제가 실제 게임 제작 과정에서 반복적으로 발생할 때만 Tool 도입을 검토한다.

- 동일한 배치나 설정 작업을 반복하느라 개발 시간이 크게 늘어난다.
- 수작업 때문에 실수가 자주 발생한다.
- 좌표나 배치 결과를 시각적으로 확인하지 않으면 작업 효율이 크게 떨어진다.
- 코드 중심 방식이 실제 게임 제작을 방해한다.

Tool이 필요하다고 판단되더라도 Unity와 같은 범용 Editor를 먼저 만들지 않는다.

예를 들어 Tile Map, Collision 배치, Animation Frame 설정처럼 실제로 확인된 문제를 해결하는 최소 범위의 전용 Tool부터 검토한다.

### 추측보다 측정

저사양 환경을 목표로 하지만 성능 판단은 직감이 아니라 실제 측정을 기반으로 한다.

가능한 경우 다음 항목을 기록한다.

- Idle RAM 사용량
- 실행 중 RAM 사용량
- Idle CPU 사용량
- 실행 중 CPU 사용량
- GPU 사용량
- Frame Time
- FPS
- Startup Time
- 실행 파일 및 필수 파일 용량

의미 있는 Framework 변경 전후의 측정 결과를 비교한다.

### Premature Architecture를 피한다

실제 문제가 발생하기 전에 큰 Architecture를 먼저 도입하지 않는다.

특히 다음 항목을 기본 구조로 선제 도입하지 않는다.

- ECS
- Service Locator
- Dependency Injection Framework
- Global Event Bus
- 과도한 Interface 계층
- 모든 System의 Singleton화
- 복잡한 Multithreading
- Job System

### 명시적인 소유권

C++의 소유권 수단은 실제 Object Lifetime과 Ownership 관계에 맞게 사용한다.

명확한 Ownership, RAII, 예측 가능한 Resource 정리를 우선한다.

편의성을 이유로 Shared Ownership을 남용하지 않는다.

### 단순한 Game 측 API

Framework 내부 구조보다 실제 Game 측 사용성을 우선한다.

추상화는 다음 중 하나 이상의 효과가 있을 때 의미가 있다.

- 복잡한 작업을 단순화한다.
- Platform 종속 구현을 격리한다.
- 반복 코드를 제거한다.
- 오류가 발생하기 쉬운 Resource 처리를 줄인다.
- Game 코드의 가독성을 높인다.

외부 API의 이름만 바꾸는 Wrapper라면 필요성을 다시 검토한다.

---

## 7. 초기 기술 방향

현재 초기 기술 방향은 다음과 같다.

| 영역 | 선택 |
| --- | --- |
| Language | C++20 |
| Platform | Windows |
| Build System | CMake |
| Window / Platform Layer | GLFW |
| Graphics API | OpenGL |
| OpenGL Loader | GLAD 계열 |

추가 Library는 해당 기능이 실제로 필요한 시점에 도입한다.

예를 들어 현재 Milestone에서 Texture Loading이나 Audio가 필요하지 않다면 Texture 또는 Audio 관련 Dependency를 미리 추가하지 않는다.

---

## 8. Architecture 경계

기본적인 의존 방향은 다음과 같다.

```text
Game
 │
 ▼
LightWeightGameFramework
 │
 ├─ Core
 ├─ Graphics
 ├─ Input
 ├─ Resource
 └─ Audio
 │
 ▼
GLFW / OpenGL / Third-Party Libraries
 │
 ▼
Operating System
```

일반적인 Framework 수준의 작업에서 Game 코드가 GLFW, OpenGL 또는 OS 세부 구현을 반복해서 직접 다루지 않도록 한다.

다만 이 경계를 유지하기 위해 불필요한 추상화를 추가하지 않는다.

---

## 9. 개발 및 Release 정책

일반 개발 작업은 `develop` 브랜치를 통해 통합한다.

```text
feature/*
    ↓ Pull Request
 develop
    ↓ Release Pull Request
   main
    ↓
Tag / GitHub Release
```

각 브랜치의 역할은 다음과 같다.

- `feature/*`: 개별 기능 구현, 수정 또는 문서 작업
- `develop`: 현재 통합된 최신 개발 상태
- `main`: 안정적인 Release 상태

`main`은 새로운 버전을 Release할 준비가 되었을 때만 갱신한다.

이 구조의 목적은 실험 중이거나 아직 검증되지 않은 개발 변경이 안정적인 Release 브랜치에 바로 들어가는 것을 방지하는 것이다.

---

## 10. 첫 번째 Milestone

첫 번째 실행 가능한 Milestone은 의도적으로 작게 잡는다.

```text
Program Start
    ↓
Framework 초기화
    ↓
Window 생성
    ↓
OpenGL Context 생성
    ↓
Game Loop 실행
    ↓
OS Event 처리
    ↓
Frame Rendering
    ↓
정상 종료
```

완료 기준은 다음과 같다.

- 프로그램이 정상적으로 실행된다.
- Window가 정상적으로 생성된다.
- Game Loop가 지속적으로 실행된다.
- Window Close 또는 명시적인 Input으로 정상 종료할 수 있다.
- 종료 과정에서 사용한 Resource가 정상적으로 정리된다.
- Debug Build와 Release Build를 모두 생성할 수 있다.
- 초기 RAM 및 CPU 사용량을 기록한다.

이 Milestone에서는 Texture, Audio, Scene, ECS 또는 그 밖의 상위 System을 요구하지 않는다.

---

## 11. 첫 번째 주요 완료 기준

특정 기능의 개수를 채웠다고 Framework가 완성된 것으로 판단하지 않는다.

첫 번째 주요 성공 기준은 다음과 같다.

> LightWeightGameFramework를 이용해 작은 2D 게임 하나를 제작하고 Release Build로 정상 실행한다.

게임 제작 이후 처음 세운 목적에 맞는지 다음 항목을 검증한다.

- 반복되는 저수준 작업이 실제로 줄었는가?
- 개발 시간이 줄었는가?
- 실수가 줄었는가?
- Game 코드의 가독성이 유지되었는가?
- 목표 환경에 맞게 충분히 가벼운 상태를 유지했는가?
- 실제 게임을 제작하는 동안 Framework가 실질적으로 도움이 되었는가?

위 질문에 대한 결과가 만족스럽지 않다면 기능을 더 추가하기보다 Framework의 구조와 방향을 먼저 다시 검토한다.
