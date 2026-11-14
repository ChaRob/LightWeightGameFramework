# Sprite Rendering & Animation V1

## 목적 및 설계

MazeRunner의 16×16 픽셀아트 걷기 애니메이션을 위해 기존 Texture2D와 Renderer2D를 확장했다.
Goals.md의 Game First / 최소 구현 원칙에 따라 Sprite 객체, Animation Controller, Resource Manager를 추가하지 않는다.

| 대안 | Game 사용성 및 소유권 | 판단 |
| --- | --- | --- |
| A. 기존 렌더러 확장 + SpriteAnimation | Texture2D는 Game 소유, Animation은 값 기반 CPU 상태 | 선택. 기존 UV와 Texture 로딩 재사용 |
| B. Sprite와 Animation 분리 | Sprite가 Texture 및 Region을 참조하는 별도 수명 규칙 필요 | 현재는 불필요 |
| C. Game 코드에서 타이머와 Region 직접 관리 | API는 가장 적지만 프레임 전환/예외 처리 반복 | 기각. 재사용할 공통 로직이 명확함 |

## 사용 예제

Game 초기화 시 한 번:

```cpp
#include <framework/Texture2D.h>
#include <framework/Renderer2D.h>
#include <framework/SpriteAnimation.h>

Texture2D playerTexture;
SpriteAnimation walk;

// OpenGL Context 생성 이후 Load.
if (!playerTexture.Load("assets/player_walk_down.png", TextureFilter::Nearest) ||
    !walk.ConfigureStrip({0, 0, 16, 16}, 4, 0.07))
{
    return false;
}
walk.Play();
```

Update / Render:

```cpp
// Update: seconds
walk.Update(_app.GetDeltaTime());

// Render: x/y는 화면의 좌하단 기준, 크기는 화면 픽셀.
_renderer.DrawTextureRegion(playerTexture, walk.GetCurrentFrame(),
                            100.0f, 100.0f, 48.0f, 48.0f);
```

다른 순서, 여러 줄, 서로 다른 지속 시간은 직접 지정한다.

```cpp
SpriteAnimation action;
action.Configure({
    {{16, 16, 16, 16}, 0.10},
    {{ 0,  0, 16, 16}, 0.20}
}, AnimationMode::Once);
action.Play();
```

## 좌표계와 필터

- `TextureRegion{x, y, width, height}`는 Aseprite와 동일한 원본 이미지 좌상단 기준 정수 픽셀이다.
- 화면의 x/y는 기존 Renderer2D와 동일한 좌하단 기준이다.
- stb_image가 이미지를 수직 반전하여 업로드하므로 UV는 u0=x/W, u1=(x+w)/W, v0=(H-y-h)/H, v1=(H-y)/H로 계산한다.
- `Load(path)`의 기존 Linear 동작은 유지한다. `Load(path, TextureFilter::Nearest)`는 MIN/MAG 필터를 GL_NEAREST로 지정한다. 필터는 로드 단위다.
- Region의 Linear 샘플링은 fragment shader에서 첫/마지막 texel 중심까지 제한하여 이웃 프레임의 픽셀 혼입을 방지한다. 전체 Texture와 Font 렌더링의 샘플링은 변경하지 않는다.
- 정수 배율/정수 화면 위치가 픽셀아트에 권장된다. 알파 블렌딩은 기존 방식 그대로다.
- `DrawTextureRegion`은 로드되지 않은 Texture, 원본 범위를 벗어난 Region, 비정상적인 화면 영역(비유한 수치, 0 이하 크기)에 대해 false를 반환하고 그리지 않는다. 화면 밖 배치는 허용한다.
- 전체 Texture는 항상 전체 UV, Font는 Glyph UV, Region은 자신의 UV를 지정한다. Rect는 UV를 사용하지 않는다.

## 재생 정책 및 수명

- `Configure` / `ConfigureStrip`은 초기화 시 호출한다. 성공하면 첫 프레임에서 정지하고, 실패하면 기존 설정과 상태를 유지한다.
- Strip은 가로 방향의 연속 프레임만 생성한다. 임의 배치는 Configure를 사용한다.
- `Play()`는 현재 위치부터 재생하며 여러 번 호출해도 초기화하지 않는다. `Stop()`은 현재 프레임과 경과 시간을 보존한다.
- `Reset()`은 첫 프레임, 정지, 완료 해제로 초기화한다.
- Loop는 한 주기 이후 반복한다. Once는 마지막 프레임의 지속 시간까지 경과한 뒤 완료 상태로 정지한다. 완료된 Once를 다시 시작하려면 Reset 후 Play를 호출한다.
- 빈 구성, 유효하지 않은 Region/시간, 정수 또는 총 지속 시간 표현 범위 초과는 거절한다. 0 이하, NaN, 무한 Delta Time은 무시한다.
- Animation은 Texture를 참조하지 않으므로 실제 이미지 크기 검사는 DrawTextureRegion에서 한다. 빈 Animation의 GetCurrentFrame은 유효하지 않은 빈 Region을 반환한다.
- Game이 Texture2D를 소유하며 렌더링 동안 살아 있어야 하고 OpenGL Context 종료 전에 해제해야 한다.
- 복사된 Animation도 재생 상태가 독립적이다. 초기 구성 시에만 프레임 배열을 할당하며 Update/Render는 GPU 리소스를 만들거나 Texture를 다시 로드하지 않는다.
- 큰 Delta Time은 Loop에서 나머지 연산으로 주기를 건너뛰고 최대 한 번의 프레임 배열 순회로 처리한다. 0.07초 같은 십진수 시간의 경계 오차는 제한된 허용 오차로 보정한다. 극단적으로 큰 값은 부동소수점 정밀도 한계가 있다.

## Sample 및 검증

Sample의 오른쪽 영역에서 다음을 확인한다.

1. 전체 64×16 걷기 시트가 4개 프레임을 모두 표시하는지 확인.
2. NEAREST / LINEAR가 같은 프레임을 표시하면서 선명도 차이가 있는지, 투명 배경이 정상인지 확인.
3. ONCE는 마지막 자세를 유지하고 F를 누를 때마다 한 번만 재생하는지 확인.
4. 8×8 다중 행 테스트 이미지의 FULL은 좌상단 빨강/노랑, 우상단 초록/청록, 좌하단 파랑/자홍, 우하단 흰색/회색이다. TOP LEFT와 BOT RIGHT가 해당 색과 방향으로 표시되고 인접 영역의 색이 섞이지 않는지 확인.
5. 기존 사각형/폰트/전체 이미지/입력/오디오 동작이 유지되는지 확인.

단위 테스트는 GPU/Window 없이 실행되는 SpriteAnimationTests이며 기존 AudioSystemTests와 함께 CTest로 등록한다.

```sh
cmake -S . -B build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug
ctest --test-dir build-debug --output-on-failure
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
ctest --test-dir build-release --output-on-failure
```

SampleGame은 실행 파일 디렉터리를 작업 디렉터리로 실행한다. Windows 실제 화면 검증 및 변경 전후 RAM/CPU/GPU/Frame Time 측정은 별도로 기록한다.
