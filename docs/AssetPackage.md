# LWGF Asset Package V1 — 작업 초안

상태: 내부 V1 wire format, 무결성, CLI Packer 및 AssetReader/AssetStream 공개 API 구현. Texture/Font/Audio 및 게임별 CMake 자동 패키징 연동 구현. 실제 MazeRunner 연동 및 성능 검증은 남아 있다.
출시 포맷은 테스트 완료 전 변경될 수 있다.

## Product Goal

MazeRunner 및 이후 게임에서 원본 에셋 개별 노출을 줄이고 배포를 단순화한다.
개발 중에는 원본 파일을 편하게 수정할 수 있어야 하며 기존 리소스 로딩 편의성과
저사양 환경에서의 메모리 사용 특성을 유지한다.
자체 포맷은 암호화, DRM 또는 원본 추출 방지를 보장하지 않는다.

## 합의된 Requirement / Policy

- Windows 우선, Android 실제 배포는 제외.
- Debug/Release 빌드 구성과 독립적인 명시적 Directory/Package 모드.
- Directory 모드에서는 원본 파일을 직접 사용. Package 모드에서 원본으로 자동 fallback 금지.
- 하나의 논리 패키지. assets.pak은 인덱스 전용이며 실제 데이터는 assets.1, assets.2, ...에 저장.
- 바이너리 단위 분할: 하나의 에셋이 여러 데이터 파일에 걸쳐도 읽기와 seek가 가능해야 한다.
- 데이터 파일 기본 최대 크기는 512 MiB (536870912 bytes), 게임별로 더 작거나 크게 설정 가능.
  인덱스 파일은 데이터 분할 제한의 적용 대상이 아니다.
- 에셋 상대 경로 유지, 필요한 에셋만 읽고 전체 패키지 메모리 적재 및 임시 원본 추출 금지.
- Texture, Font, Audio 및 일반 바이너리/JSON 데이터 사용. BGM 스트리밍 유지.
- 초기화에서 index CRC32, part header의 generation ID/번호/크기 검증. 배포 전 명시적인 전체 payload CRC32 검증(B안).
- V1 데이터 압축 없음. 압축 방식의 실제 추가는 이후 필요성 확인 및 포맷 버전 정책에 따라 검토.
- 에셋만 추가/수정/삭제해도 패키징에 반영. 낡은 part 및 원본 에셋이 배포 결과에 남지 않아야 한다.
- 외부 게임의 add_subdirectory 구성에서 Packer 및 CMake 연동 사용 가능.
- 기존 리소스 소유권 유지. 캐시, 다중 독립 패키지, 에디터, DRM, 암호화, DLC 제외.

## 이번 첫 작업: 분할 바이너리 I/O

내부 SplitAssetWriter는 연속 바이트를 순서대로 여러 part에 기록한다.
SplitAssetReader는 논리 오프셋으로 읽어 part 경계를 투명하게 처리한다.
전체 크기 및 분할 크기는 미래의 인덱스 계층에서 전달받는다.
동일한 Reader 인스턴스의 동시 호출은 지원하지 않는다. 각 스트림의 파일 위치는 독립적이어야 한다.
Writer는 새 staging 영역 전용이며 기존 part 덮어쓰기를 거절한다.
Part header와 CRC32를 추가하여 혼합 세대/잘못된 순번/크기 검사를 지원한다.
전체 payload 검증은 VerifyPayload()로 명시적으로 수행한다.
빈 데이터에는 part를 만들지 않는다. Packer는 index-only 빈 패키지를 허용한다.
임시 파일 정리 및 배포 세트 교체는 Packer가 담당한다.
Index는 내부 메타데이터이며 게임은 AssetReader/AssetStream 공개 API를 사용한다.

## 남은 검증

- MazeRunner 실제 배포 구성(게임 전용 DLL/라이선스/저장 데이터 포함)과 성능 측정.

## 무결성 정책 (B안 확정)

- 초기화: index 전체의 CRC32 및 구조 검증, 모든 part의 파일 크기, magic, version,
  generation ID, part 번호 확인. payload는 읽지 않는다.
- 명시적 전체 검사: 각 part payload의 IEEE CRC32를 64 KiB 버퍼로 순차 계산하고
  index에 기록된 값과 비교한다. 이 기능은 추후 Packer verify 명령에 연결한다.
- 일반 로딩에서 CRC 전체 스캔을 보장하지 않는다. 동일 크기 payload 손상은
  초기화에서 발견되지 않을 수 있다. CRC32는 암호화/인증/변조 방어 수단이 아니다.
- 패키지는 사용 중 변경되지 않는다고 가정한다. Reader는 파일을 다시 열 때도
  header/크기를 확인하며 한 인스턴스를 여러 스레드가 동시에 쓰지 않는다.
- Generation ID는 16바이트의 nonzero 값이다.
  Packer는 std::random_device로 매번 새 ID를 생성하며 내부 I/O 테스트는 고정 ID를 사용한다.

## 내부 V1 바이너리 명세 (구현)

모든 정수는 unsigned little-endian, padding 없음. 구조체를 그대로 기록하지 않는다.
512 MiB 기본 제한은 header 포함 실제 part 파일 크기다. capacity = limit - 32.
논리 offset은 header를 제외한 연속 payload 기준이다.
최소 part limit은 33 bytes, 최대는 streamoff 표현 한계이다.

### Part 파일 (assets.1, assets.2, ...)

| Offset | Bytes | Field |
| --- | ---: | --- |
| 0 | 4 | ASCII LWPD |
| 4 | 4 | Version = 1 |
| 8 | 16 | Generation ID |
| 24 | 8 | Part number (1-based) |
| 32 | remaining | Payload |

마지막 part를 제외한 part 파일은 limit과 크기가 같다. 0 bytes payload이면 part가 없다.
CRC32 대상은 header를 제외한 payload만이다. part 개수는 ceil(total/capacity).

### Index 파일 (assets.pak)

| Offset | Bytes | Field |
| --- | ---: | --- |
| 0 | 4 | ASCII LWPK |
| 4 | 4 | Version = 1 |
| 8 | 4 | Flags = 0 (uncompressed) |
| 12 | 16 | Generation ID |
| 28 | 8 | Part size limit (physical bytes) |
| 36 | 8 | Logical total payload size |
| 44 | 8 | Part count |
| 52 | 8 | Entry count |
| 60 | 4 × part count | Payload CRC32 table (part number order) |
| variable | variable | Asset entries |
| EOF - 4 | 4 | Index CRC32 (all preceding bytes) |

각 entry는 u32 path byte length, u64 logical offset, u64 size, path bytes 순서이다.
경로의 종단 NUL은 기록하지 않는다. V1은 무압축이므로 stored size = original size.
Entry는 payload 기록 순서이며 연속 범위 전체를 중복 없이 덮어야 한다. 빈 entry는
size=0으로 표현한다. index는 64 MiB로 임시 상한을 두어 비정상 파일의 메모리 사용을 제한한다.
Codec은 Packer와 동일한 논리 경로 검증 및 경로 충돌 검사를 사용한다.
경로는 Package 모드에서 정확한 index 식별자이며 Directory 모드에서도 같은 UTF-8 논리 경로를 사용한다.

Index 읽기 실패 시 출력 metadata를 유지하고, 잘린 index, magic/version/flags,
CRC, 비정상 count/length, 중복 경로, overflow/범위 오류, 남는 데이터를 거부한다.
Index 쓰기는 새 staging 파일에만 허용한다. Packer가 staging 정리, generation ID 생성,
패키지 전용 출력 디렉터리 교체 및 사용자용 오류 메시지를 담당한다.

## Packer CLI 사용법

LWGF 루트의 Windows 개발 환경에서:

~~~powershell
cmake --build out/build/x64-debug --target LWGFAssetPacker
.\out\build\x64-debug\LWGFAssetPacker.exe pack .\sample\assets .\out\packed-assets
.\out\build\x64-debug\LWGFAssetPacker.exe verify .\out\packed-assets
~~~

선택적 마지막 인자는 header 포함 최대 part 크기(bytes)이다. 예를 들어 실제 Sample
에셋을 작은 여러 파일로 분할하려면:

~~~powershell
.\out\build\x64-debug\LWGFAssetPacker.exe pack .\sample\assets .\out\packed-assets 65536
~~~

출력 부모 디렉터리는 이미 존재해야 한다. 기본값은 536870912 bytes.
CLI 종료 코드: 0 성공, 1 처리 실패, 2 사용법/용량 인자 오류.
`LWGF_BUILD_ASSET_PACKER` CMake option은 기본 ON. CMake cross compile 시에는
호스트 도구를 자동 생성하지 않는다(Android host tool 연결은 미래 작업).
Packer target은 그래픽/오디오 라이브러리를 링크하지 않는다. 현재 최상위
CMake configure 자체는 기존 Framework 의존성을 계속 준비한다.

### V1 경로 및 출력 정책 (이번 구현의 기본값)

- 에셋 루트 기준 UTF-8 상대 경로, 구분자 /, 정확한 대소문자로 lookup한다.
- 절대 경로, 빈 component, . / .., backslash, NUL/control 문자, Windows 금지 문자,
  trailing dot/space, Windows 예약 장치명, 잘못된 UTF-8 거부.
- 경로 길이 최대 4096 UTF-8 bytes(임시 V1 metadata 상한; OS long path 지원 보장 아님).
- 파일 및 폴더의 대소문자 충돌/중복 및 파일-폴더 alias 거부. Windows는
  CompareStringOrdinal의 ignore-case 비교, 비-Windows 개발 빌드는 ASCII만 case-fold.
  Unicode 정규화는 하지 않는다. Android 실제 사용 정책은 이후 검토한다.
- 원본 루트 안의 모든 일반 파일을 포함(숨김 파일도 포함). 별도 exclude 규칙 없음.
  Symlink/Windows reparse point 및 특수 파일은 거부. 빈 폴더는 기록하지 않는다.
- 빈 파일 허용. 원본 파일이 없거나 전부 빈 파일이면 index만 존재할 수 있다.
- 입력 순서는 논리 경로 기준 정렬. 매번 새 generation ID 생성.
- Source와 Output은 서로 포함하지 않는 별도 디렉터리여야 한다. Output은
  신규/빈 폴더 또는 이전 정상 구조의 패키지 파일만 있는 전용 디렉터리여야 한다.
  실행 파일, 세이브, DLL, 라이선스 등의 외부 파일이 섞이면 교체를 거절한다.
  기존 index/part header가 손상되거나 누락된 경우에도 교체하지 않는다.
  이런 경우 개발자가 별도 새 출력 폴더를 사용해야 한다.
- 64 KiB 고정 버퍼로 원본을 읽어 staging에 기록한다. index 크기와 part 개수를
  사전 확인하고, 원본 파일의 크기/수정시간 변화를 확인한다. 원본은 패키징 중
  변경하지 않아야 하며 동시 변경 전체에 대한 snapshot은 보장하지 않는다.
- 새 패키지는 전체 CRC 검증 후 같은 볼륨 내 디렉터리 rename으로 교체한다.
  이전 패키지 전체를 별도 backup으로 이동하고 성공 시 staging과 함께 정리하므로
  감소한 part가 결과물에 남지 않는다. 실패 시 이전 폴더 복원을 시도한다.
  복원까지 실패하면 이전 폴더를 보존하고 경로를 출력한다. 전원 장애에 대한
  완전한 atomic transaction 또는 동시 실행/동시 pack은 지원하지 않는다.
- verify는 index와 기대 part를 전부 검사하지만 배포 디렉터리의 다른 파일까지
  금지하지 않는다. 배포 시 원본 및 불필요한 파일 제외는 CMake 배포 단계에서 검증한다.

### 검증

`AssetPackerTests`: 바이너리 원본 byte 비교, UTF-8 경로, 빈 파일/패키지,
경계 분할, 재패키징/삭제, generation ID 갱신, 손상 검출, unrelated 파일 보호,
경로 검증 및 staging 정리.

`AssetPackerSampleTests`: 실제 sample PNG/JPG/TTF/WAV 에셋을 CLI로 pack/verify,
64 KiB 분할 파일 크기 검사 및 모든 실제 원본 파일과 package entry byte 단위 비교.
CTest는 실제 Sample 패키지의 공개 AssetReader API 비교까지 수행한다.

## 런타임 AssetReader / AssetStream

Game이 하나의 AssetReader 객체를 소유하며 source를 명시적으로 선택한다.
전역 Singleton, 자동 mode 탐색 또는 fallback은 사용하지 않는다.
물리 root는 filesystem::path, 논리 asset path는 UTF-8 상대 경로이다.
예: root가 sample/assets 또는 out/packed-assets이면 asset은 "images.jpg" 또는
"fonts/AnonymousPro-Regular.ttf"로 지정한다. "assets/" prefix를 붙이지 않는다.

~~~cpp
#include <framework/AssetReader.h>
#include <array>

AssetReader assets;
const bool packaged = true; // Game의 명시적 설정
if (!(packaged ? assets.OpenPackage("out/packed-assets")
              : assets.OpenDirectory("sample/assets")))
{
    return -1;
}

std::vector<std::byte> data;
if (!assets.ReadAll("images.jpg", data))
{
    return -1;
}

// Stream은 개별 에셋의 독립적인 읽기 위치를 관리한다.
auto music = assets.Open("audio/music.wav");
if (!music)
{
    return -1;
}
std::array<std::byte, 4096> buffer{};
std::size_t bytesRead = 0;
if (!music->Read(buffer, bytesRead))
{
    return -1;
}
music->Seek(0);
~~~

위 코드는 파일 바이트 읽기 예시다. Texture/Font는 아래 공개 Load overload를 사용하고 Audio는 초기화 시 AssetReader를 지정한다.
실제 Game은 root를 실행 파일 위치 또는 개발 설정에서 결정할 수 있다.
상대 root는 OpenDirectory/OpenPackage 호출 시 절대 경로로 고정한다.

### 동작 정책 및 소유권

- Directory 모드: root 아래 요청한 경로의 각 component를 실제 이름과 정확히 비교한다.
  요청 경로 내 symlink/Windows reparse point 및 특수 파일을 거부한다.
  전체 폴더를 캐싱하지 않고 다음 Open에서 원본 변경/추가/삭제를 반영한다.
  이미 열린 스트림 또는 읽기 도중 원본 변경은 지원하지 않는다.
- Package 모드: index CRC/구조와 모든 part header/size를 최초 한 번 검증한다.
  전체 payload CRC scan은 하지 않는다. stream을 열 때마다 전체 part를 재검사하지
  않고, 실제로 읽는 part를 열 때 header 및 size를 다시 검사한다.
  원본 디렉터리 또는 같은 폴더의 loose file로 fallback하지 않는다.
- AssetReader의 source 설정 실패 시 기존 source를 유지한다.
  Close()는 source 설정과 index metadata를 해제한다. 이미 열린 AssetStream은
  독립적인 파일 핸들/metadata를 소유하므로 reader의 Close/reconfigure/destruction
  이후에도 유지된다. 사용 중인 원본/패키지는 변경하거나 교체하지 않아야 한다.
- Stream의 Seek는 asset 기준 절대 offset이며 EOF까지 허용한다.
  Read는 요청 buffer 크기와 남은 asset 크기 중 작은 값만 읽는다.
  EOF는 성공 및 0 bytes, I/O 실패는 false 및 0 bytes를 반환하고 cursor는 유지한다.
  실패 시 buffer 일부가 변경되었을 수 있다.
- ReadAll은 호출자의 vector에 전체 asset을 반환한다. 빈 asset은 성공/빈 vector.
  실패 시 기존 vector는 유지한다. 큰 asset/BGM은 Open/Read로 스트리밍한다.
- Reader 설정/변경 및 동일 stream 동시 호출은 동기화하지 않는다.
  별개 stream에는 공유 file cursor가 없다. 오류는 로그와 실패 값으로 전달한다.
  루트 상위 경로는 설정 시 canonicalize하며 동시 파일 시스템 변경에 대한 보안
  sandbox/TOCTOU 방어는 제공하지 않는다.

### 검증

AssetReaderTests: 두 mode의 API, 한글 경로, 정확한 대소문자, 변경된 원본,
빈 에셋/패키지, 부분 읽기, 경계 횡단, 독립 cursor, EOF와 범위 제한,
source 재설정 실패 및 stream 수명, no fallback, 손상/누락 오류를 검증한다.
AssetPackerSampleTests는 실제 sample 파일을 두 mode에서 ReadAll 및 분할 stream으로
읽어 동일한 데이터를 얻는지 추가 비교한다.

## 에셋 루트와 Texture/Font 로딩

에셋 루트는 CMakeLists.txt 위치로 고정되지 않는다. Packer에 지정한 source directory와
Directory 모드에 지정한 root가 같은 논리 경로 기준을 가진다.

- LWGF SampleGame의 원본 에셋: `LightWeightGameFramework/sample/assets/`.
- 별도 게임 프로젝트는 자기 프로젝트의 `assets/` 등을 선택할 수 있다.
  예를 들어 MazeRunner의 최상위 CMakeLists.txt 옆에 assets/가 있으면 그 폴더를 지정한다.
- 배포 시에는 assets.pak과 assets.N이 있는 디렉터리를 Package root로 지정한다.
  패키지 파일을 exe와 같은 폴더에 둘 경우 그 폴더 자체가 root이다.
- 논리 경로는 항상 root 내부 기준. `assets/`는 앞에 붙이지 않는다.

~~~cpp
#include <framework/AssetReader.h>
#include <framework/Texture2D.h>
#include <framework/Font.h>

// Application과 Renderer 초기화 이후, GL context가 살아 있는 상태에서:
AssetReader assets;
if (!assets.OpenDirectory("sample/assets"))
    return -1;
// Package 모드에서는 위 한 줄을 assets.OpenPackage("out/packed-assets")로 변경.

Texture2D texture;
Font font;
if (!texture.Load(assets, "player_walk_down.png", TextureFilter::Nearest) ||
    !font.Load(assets, "fonts/AnonymousPro-Regular.ttf", 48.0f))
    return -1;
~~~

Texture2D와 Font는 기존 Load(const char*, ...) 물리 경로 API를 유지한다.
새 Load(const AssetReader&, string_view, ...)는 동일한 AssetReader로 선택된 모드에서
해당 에셋 하나를 메모리에 읽어 stb_image/stb_truetype으로 디코딩한다. 디코딩 후
공통 GPU 업로드/atlas 생성 코드를 사용하고 원본 파일을 임시로 추출하지 않는다.
원본 인코딩 데이터 크기 0 또는 INT_MAX 초과는 메모리 할당 전에 거부한다.
Texture/Font는 성공 시 GPU 자원을 자체 소유하며 AssetReader 수명에 의존하지 않는다.
실패 시 기존 GPU 자원/크기/폰트 상태를 유지한다. Texture는 생성/업로드 오류도 확인한다.
stb_truetype은 신뢰 가능한 폰트 파일용이며 임의로 조작된 폰트에 대한 안전한 파서를
제공하지 않는다. 새 코드는 기본 길이/offset 검사를 추가하나 완전한 포맷 검증은 아니다.

### SampleGame 수동 실행

프로젝트 루트의 PowerShell에서:

~~~powershell
cmake --build out/build/x64-debug
.\out\build\x64-debug\sample\SampleGame.exe --directory .\sample\assets

.\out\build\x64-debug\LWGFAssetPacker.exe pack .\sample\assets .\out\packed-assets 65536
.\out\build\x64-debug\sample\SampleGame.exe --package .\out\packed-assets
~~~

인자 없는 기존 SampleGame 실행은 exe 옆의 assets/를 Directory root로 선택한다.
명시적인 상대 root는 실행 당시 CWD 기준으로 절대 경로를 고정한다.
`--directory .\sample\assets`는 원본 폴더를 직접 읽으므로 이미지/폰트 교체를
다음 실행에서 바로 확인할 수 있다. 현재 CMake의 POST_BUILD 복사 정책은 변경하지 않았다.
두 모드에서 Texture/Font/Audio가 같은 AssetReader를 사용한다. 원본 오디오 fallback은 하지 않는다.

`GraphicsAssetTests`는 GPU가 없는 CI에서 실제 Sample JPG/PNG/TTF를
legacy file / Directory / Package 세 경로로 로딩하고, OpenGL 업로드 호출을
test double로 대체하여 RGBA 이미지와 glyph atlas byte/크기/filter 및 실패 시
기존 texture 보존을 비교한다. 실제 GPU 렌더링, 애니메이션/폰트 시각 결과는
Windows SampleGame을 실행하여 확인한다.

## AudioSystem 연동

기존 `Initialize(maxConcurrentEffects)`와 물리 경로 `LoadEffect/PlayMusic` 호출은
그대로 유지한다. 새 `Initialize(assets, maxConcurrentEffects)` overload로
게임이 이미 구성한 AssetReader를 지정한다. 이후 해당 AudioSystem에서의 모든
LoadEffect/PlayMusic 경로는 논리 UTF-8 상대 경로이며 혼용/fallback은 지원하지 않는다.

~~~cpp
AssetReader assets;
if (!assets.OpenPackage("out/packed-assets"))
    return -1;

AudioSystem audio;
if (!audio.Initialize(assets))
    return -1;

const auto click = audio.LoadEffect("audio/click.wav");
if (click == AudioSystem::InvalidEffect)
    return -1;

audio.PlayEffect(click);
audio.PlayMusic("audio/music.wav", true);

// 각 frame에 audio.Update();
// audio는 assets보다 먼저 Shutdown 또는 소멸해야 한다.
~~~

### 기술 및 정책

- miniaudio의 engine 내부 resource manager에 custom read-only VFS를 설정한다.
  기존 miniaudio backend와 decoded effect/stream music flags를 유지한다.
- VFS는 파일을 열 때 독립 AssetStream을 생성한다. Read/Seek/Tell/Info/Close
  callback으로 miniaudio가 해당 asset 범위만 읽는다. seek는 start/current/end를
  지원하며 signed offset overflow 및 범위 초과를 거부한다.
- 효과음은 기존과 같이 decoded PCM을 보관하고 voice는 ma_sound_init_copy로 재사용한다.
  BGM은 기존 MA_SOUND_FLAG_STREAM 기반 resource-manager worker streaming을 유지한다.
  전체 인코딩 데이터를 ReadAll 하거나 원본 파일을 추출하지 않는다.
- AudioSystem은 AssetReader를 빌려 사용한다(소유권 없음). worker가 파일을 열 수
  있으므로 reader의 lifetime과 source 설정은 Shutdown 완료까지 유지해야 한다.
  Source 변경은 AudioSystem Shutdown 후 reader 재설정, AudioSystem 재초기화 순서로 한다.
  AssetStream별 file cursor는 독립적이며 동일 stream 동시 호출은 지원하지 않는다.
- 미설정 reader, 잘못된 논리 경로, 누락 asset 및 VFS I/O 오류는 실패 반환.
  기존 effect voice limit 및 음악 재생/일시정지/재개/정지/같은 곡/교체 실패 정책 유지.
  실행 중 에셋 변경은 지원하지 않으며 이미 decoded/cache된 효과음 변경은 unload/reload 필요.
- SampleGame 두 mode에서 동일 audio API 사용: SPACE 효과음, M BGM 반복,
  P 일시정지, R 재개, N 정지. 장치 초기화 실패 시 기존처럼 무음으로 실행한다.

### 테스트

`AudioAssetTests`는 실제 Sample WAV를 64 KiB part로 패키징하고 Directory/Package에서
VFS의 독립 cursor, EOF 및 signed seek, 쓰기/잘못된 경로 거부를 검증한다.
실제 miniaudio decoder로 두 mode의 PCM을 전부 비교하고 중간 seek도 검사한다.
no-device engine과 동일한 VFS/flags로 decoded effect/voice copy 및 streaming music을
초기화하고 실제 PCM mix, loop, stop/seek/restart 및 누락 track 오류를 검사한다.
실제 스피커 출력 및 AudioSystem 전체 상호작용은 Windows SampleGame에서 확인한다.

## CMake 자동 패키징 및 배포용 빌드

게임 프로젝트는 LWGF를 add_subdirectory한 후 자기 실행 파일 target을 설정한다.
Framework root는 자동으로 CMake helper를 포함한다. CMake 함수 호출 예:

~~~cmake
add_subdirectory(external/LightWeightGameFramework)
add_executable(MazeRunner src/main.cpp)
target_link_libraries(MazeRunner PRIVATE LWGF::Framework)

set(GAME_ASSET_MODE "DIRECTORY" CACHE STRING "DIRECTORY or PACKAGE")
set(GAME_ASSET_PART_SIZE "536870912" CACHE STRING "Physical part size in bytes")

lwgf_configure_assets(
    TARGET MazeRunner
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets"
    MODE "${GAME_ASSET_MODE}"
    PART_SIZE_BYTES "${GAME_ASSET_PART_SIZE}"
)
~~~

SOURCE_DIR은 게임이 지정하며 상대 경로는 호출 CMakeLists.txt 위치 기준이다.
PART_SIZE_BYTES는 생략 시 512 MiB(536870912), header 포함 최대 물리 part 크기.
MODE는 Debug/Release와 독립적인 DIRECTORY 또는 PACKAGE(대문자)이다.
helper는 target을 정의한 CMake 디렉터리에서 target마다 한 번 호출한다.
동일 런타임 디렉터리에 여러 게임의 assets를 패키징하지 않는다.

helper는 게임 target에 전용 `<lwgf/AssetBuildConfig.h>`를 생성한다.
Game은 시작 시 다음처럼 AssetReader를 설정한다. exeDirectory는 Windows의
실행 파일 위치이며 SampleGame은 GetModuleFileNameW를 사용한다.

~~~cpp
#include <lwgf/AssetBuildConfig.h>
#include <framework/AssetReader.h>

AssetReader assets;
const bool opened = lwgf::build::UsePackage
    ? assets.OpenPackage(exeDirectory / lwgf::build::PackageDirectory)
    : assets.OpenDirectory(std::filesystem::path(lwgf::build::DirectoryRoot));
if (!opened)
    return -1;
~~~

- DIRECTORY: 원본 에셋 디렉터리의 절대 경로를 target 전용 header에 UTF-8로
  기록한다. 복사, 패키징, 에셋 수정에 따른 재빌드 없이 다음 실행에 원본 변경 반영.
  개발 빌드로서 원본 폴더가 있는 PC에서만 실행할 수 있다.
- PACKAGE: 원본 경로를 실행 파일에 설정값으로 포함하지 않는다.
  게임 target에 `<target>_assets` custom target을 의존성으로 연결하고
  게임 exe 옆의 전용 assets/ 디렉터리에 Packer를 실행한다.
  에셋만 수정/추가/삭제하거나 C++ 변경이 없어도 빌드 시 실행한다.
  검증 실패는 빌드 실패로 전파되며 이전 패키지는 기존 Packer 정책에 따라 보호된다.
  이전 실행 파일이 빌드 폴더에 남아 있을 수 있으므로 실패한 빌드 결과를 배포하지 않는다.
- V1은 PACKAGE 빌드마다 전체 재패키징 및 검증을 수행한다. 변경 없는 빌드의
  증분 최적화는 실제 큰 게임에서 비용을 측정한 후 필요 시 도입한다.
- Package 출력은 exe 옆 assets/ 전용 디렉터리로 한정한다. 기존 raw asset 또는
  관련 없는 파일이 섞인 디렉터리는 Packer가 교체를 거부하며 자동 삭제하지 않는다.
  기존 POST_BUILD 원본 복사와 함께 사용하지 말고 깨끗한 별도 PACKAGE build tree를 사용한다.
  Framework는 게임의 다른 임의 출력 폴더, DLL, 세이브, 라이선스, installer를 관리하지 않는다.
  Packer 실행 파일과 CMake build tree 전체는 배포 대상이 아니다.

### SampleGame 빌드 구성

Visual Studio CMake preset:

| Preset | Build type | Asset mode |
| --- | --- | --- |
| x64-debug | Debug | DIRECTORY |
| x64-release | Release | DIRECTORY |
| x64-debug-package | Debug | PACKAGE |
| x64-release-package | Release | PACKAGE |

기존 두 preset은 원본 디렉터리 직접 사용, 새 두 preset은 별도 build tree에서
자동 패키징. SampleGame의 CMake 설정은 LWGF_SAMPLE_ASSET_MODE 및
LWGF_SAMPLE_ASSET_PART_SIZE로 변경할 수 있다.

~~~powershell
cmake --preset x64-debug-package -DLWGF_SAMPLE_ASSET_PART_SIZE=65536
cmake --build out/build/x64-debug-package --target SampleGame
.\out\build\x64-debug-package\sample\SampleGame.exe
~~~

Package 출력 구조(64 KiB 예):

~~~text
out/build/x64-debug-package/sample/
    SampleGame.exe
    assets/
        assets.pak
        assets.1
        assets.2
        ...
~~~

기존 수동 --directory / --package 실행 인자는 테스트를 위해 유지한다.
이전 build tree에 남은 loose assets는 삭제하지 않으며 새 Package preset과 분리된다.
자동화 검증은 AssetBuildTests(외부 fixture, 실제 packer, 추가/수정/삭제, 빌드 실패,
원본 보호, 별도 mode) 및 Windows CI의 실제 packaged SampleGame 빌드를 사용한다.
Fixture는 add_subdirectory를 통한 helper 가시성과 호출 방식 검증을 위해 host packer를
import한다. 실제 LWGF submodule/게임 통합은 MazeRunner 단계에서 검증한다.
MSVC/Ninja에서는 별도 fixture를 configure/build하므로, 상위 CMake configure 때의
PATH/INCLUDE/LIB/LIBPATH 및 관련 SDK 환경을 AssetBuildTests에 한해 복원한다.
CTest 호출 셸이 x86 환경이거나 SDK 검색 경로가 다른 경우에도 상위 configure가
정상적인 x64 개발 환경이었다면 fixture는 같은 환경에서 빌드된다.
상위 configure 자체는 Visual Studio의 x64 preset 또는 x64 Native Tools 환경에서
수행해야 한다. 기존 configure 환경이 잘못되었다면 해당 환경에서 다시 configure한다.

## 다음 단계 / 완료 기준

1. 분할 I/O 및 경계 테스트 (Windows CI Debug/Release 및 사용자 로컬 Debug 통과).
2. 무결성 B안 및 내부 index codec (Windows CI Debug/Release, 사용자 로컬 테스트 통과).
3. CLI Packer 및 경로/출력 정책 (Windows CI Debug/Release, 사용자 로컬 테스트 통과).
4. AssetReader/AssetStream 공개 API, Directory/Package 소스 (Windows CI Debug/Release, 사용자 로컬 테스트 통과).
5. Texture/Font 연동 및 SampleGame 모드 선택 (Windows CI Debug/Release, 사용자 로컬 테스트 통과).
6. Audio의 독립 스트리밍 연동 (Windows CI Debug/Release, 사용자 로컬 테스트 및 실제 소리 확인).
7. 외부 CMake 연동 및 에셋 변경/삭제 반영 (이번 작업, 검증 필요).
8. SampleGame 및 MazeRunner 실제 Windows 실행, 기존 CTest 회귀 검사.
9. 동일 에셋/동일 실행 환경에서 RAM, 시작 시간, 프레임 시간, 배포 용량 비교.

MazeRunner HUD 폰트는 현재 LWGF sample에서 복사되므로 게임 assets 루트만
패키징하면 누락된다. 원본 위치 통합과 OFL 라이선스 배포는 게임 연동 시 검토한다.
