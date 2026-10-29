# LightWeightGameFramework

LightWeightGameFramework(LWGF)는 저사양 Windows PC에서도 작은 2D 게임을 코드 중심으로 개발할 수 있도록 만드는 개인용 C++ 게임 Framework입니다.

이 저장소에는 Framework 코드, 문서, Framework 동작 확인을 위한 최소 Sample만 둡니다. 실제 게임 프로젝트는 별도의 Git 저장소에서 관리하고 LWGF를 Git Submodule로 참조합니다.

## 기본 구조

예시:

```text
GameProject/
├─ CMakeLists.txt
├─ CMakePresets.json
├─ src/
├─ assets/
└─ external/
   └─ LightWeightGameFramework/   <- Git Submodule
```

게임 코드는 LWGF 저장소 내부에 넣지 않습니다.

## 게임 프로젝트에 LWGF 추가

게임 프로젝트 루트에서 다음 명령을 실행합니다.

```bash
git submodule add https://github.com/ChaRob/LightWeightGameFramework.git external/LightWeightGameFramework
```

이 명령을 실행하면 `.gitmodules` 파일과 LWGF Submodule 참조가 게임 저장소에 추가됩니다.

Submodule은 LWGF의 특정 Commit을 가리키므로 게임마다 서로 다른 LWGF 상태를 사용할 수 있습니다.

## CMake에서 LWGF 연결

게임 프로젝트의 `CMakeLists.txt`에서 LWGF를 하위 프로젝트로 추가합니다.

```cmake
add_subdirectory(
    external/LightWeightGameFramework
)
```

게임 실행 Target에서는 `LWGF::Framework`를 링크합니다.

```cmake
target_link_libraries(
    GameProject
    PRIVATE
        LWGF::Framework
)
```

최소 구성 예시는 다음과 같습니다.

```cmake
cmake_minimum_required(VERSION 3.20)

project(
    GameProject
    VERSION 0.0.1
    LANGUAGES C CXX
)

add_subdirectory(
    external/LightWeightGameFramework
)

add_executable(
    GameProject
    src/main.cpp
)

target_compile_features(
    GameProject
    PRIVATE
        cxx_std_20
)

target_link_libraries(
    GameProject
    PRIVATE
        LWGF::Framework
)
```

## Submodule이 포함된 게임 저장소 Clone

새 PC에서 게임 저장소를 처음 Clone할 때는 Submodule까지 함께 받는 방법을 권장합니다.

```bash
git clone --recurse-submodules <게임 저장소 URL>
```

예시:

```bash
git clone --recurse-submodules https://github.com/ChaRob/LWGF-MazeRunner.git
```

일반 `git clone`으로 이미 저장소를 받은 경우에는 다음 명령을 한 번 실행합니다.

```bash
git submodule update --init --recursive
```

## LWGF 버전 변경

Git Submodule은 Branch 자체가 아니라 특정 Commit을 기록합니다.

게임 개발 중 새로운 LWGF 버전이 필요하다면 Submodule 디렉터리에서 원하는 Tag 또는 Commit으로 이동한 뒤 게임 저장소에서 변경된 Submodule 참조를 Commit합니다.

예시:

```bash
cd external/LightWeightGameFramework
git fetch --tags
git checkout v0.2.0
cd ../..
git add external/LightWeightGameFramework
git commit -m "chore: update LWGF to v0.2.0"
```

개발 중에는 필요한 Commit을 직접 참조할 수 있습니다. 게임의 안정적인 체크포인트나 Release에서는 가능하면 LWGF Tag가 지정된 Commit을 사용하는 것을 권장합니다.

## 다른 PC에서 Submodule 상태 적용

게임 저장소에서 Submodule 참조가 변경된 Commit을 Pull한 뒤 다음 명령을 실행합니다.

```bash
git submodule update --init --recursive
```

이 명령은 게임 저장소가 기록하고 있는 LWGF Commit으로 Submodule을 맞춥니다.

## 키보드 입력

`<framework/Key.h>`의 `Key`는 GLFW 3.5.1에서 정의한 **120개의 실제 키 식별자**를 모두 지원합니다. 기존 `Key::W`, `Key::ESCAPE`, 방향키 API는 그대로 사용할 수 있습니다.

- 문자/숫자 행: `Key::A` ~ `Key::Z`, `Key::NUM_0` ~ `Key::NUM_9`, `Key::SPACE`, 기호 키
- 이동/편집: 방향키, `Key::ENTER`, `Key::TAB`, `Key::BACKSPACE`, `Key::DELETE_KEY`, `Key::HOME`, `Key::END`, `Key::PAGE_UP`, `Key::PAGE_DOWN`
- 기능/특수: `Key::F1` ~ `Key::F25`, `Key::PRINT_SCREEN`, `Key::PAUSE`, `Key::CAPS_LOCK` 등
- 숫자패드/수정키: `Key::KP_0` ~ `Key::KP_9`, `Key::KP_ENTER`, `Key::LEFT_SHIFT`, `Key::RIGHT_CONTROL`, `Key::LEFT_ALT`, `Key::RIGHT_SUPER` 등

`Application::BeginFrame()` 이후 `IsKeyDown`(누르고 있음), `IsKeyPressed`(이번 프레임에 눌림), `IsKeyReleased`(이번 프레임에 떼어짐)를 사용합니다.

```cpp
if (_app.IsKeyPressed(Key::SPACE))
{
    // 점프 등 1회 동작
}

if (_app.IsKeyDown(Key::LEFT_SHIFT) && _app.IsKeyDown(Key::NUM_1))
{
    // Shift + 숫자행 1
}

if (_app.IsKeyReleased(Key::KP_ENTER))
{
    // 숫자패드 Enter를 뗀 순간
}
```

`NUM_1`은 숫자행, `KP_1`은 숫자패드 키입니다. 입력은 GLFW 키 코드 기준이며 문자 입력(한글 IME/유니코드), OS가 가로채는 단축키, 키의 스캔코드 구분은 지원 범위에 포함되지 않습니다. 현재 프레임 폴링 방식이므로 두 프레임 사이에 눌렀다 떼는 매우 짧은 입력은 놓칠 수 있습니다.

## 문서

- [Goals](docs/Goals.md): LWGF의 목적, 범위, 개발 원칙

## 기본 원칙

- 실제 게임 코드는 LWGF 저장소와 분리합니다.
- 게임 프로젝트는 LWGF를 Git Submodule로 참조합니다.
- LWGF 소스 코드를 게임 저장소에 복사하지 않습니다.
- 게임별로 필요한 LWGF Commit을 독립적으로 유지합니다.
- Framework 기능은 실제 게임 개발에서 필요성이 확인된 뒤 추가합니다.

## Text rendering

- [Text rendering V1](docs/TextRendering.md): font asset loading, ASCII HUD text, limits and verification.

## Audio V1

LWGF는 miniaudio를 이용하여 짧은 효과음을 메모리에 로드한 뒤 중첩 재생하고,
BGM을 스트리밍 방식으로 반복 재생할 수 있습니다.
Audio 초기화 실패는 Game이 처리하며, 파일 로드 실패 시 다른 사운드에 영향이 없습니다.
전체 효과음 동시 재생 수에 한도를 두고, 초과 요청은 거절합니다.

Sample: SPACE = 효과음, M = BGM, P = 일시정지, R = 재개, N = 정지.

- [Audio V1 사용법 및 정책](docs/Audio.md)
