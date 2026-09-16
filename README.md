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

## 문서

- [Goals](docs/Goals.md): LWGF의 목적, 범위, 개발 원칙

## 기본 원칙

- 실제 게임 코드는 LWGF 저장소와 분리합니다.
- 게임 프로젝트는 LWGF를 Git Submodule로 참조합니다.
- LWGF 소스 코드를 게임 저장소에 복사하지 않습니다.
- 게임별로 필요한 LWGF Commit을 독립적으로 유지합니다.
- Framework 기능은 실제 게임 개발에서 필요성이 확인된 뒤 추가합니다.
