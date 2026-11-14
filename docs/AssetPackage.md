# LWGF Asset Package V1 — 작업 초안

상태: 핵심 분할/무압축/무결성 정책 합의, 내부 V1 wire format 및 검증 구현 중.
공개 API와 에셋 경로/배포 정책은 아직 검토 중이며 출시 포맷은 테스트 완료 전 변경될 수 있다.

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
빈 데이터에서 part를 만들지 않는 동작은 내부 primitive의 동작이며 빈 패키지 허용 정책은 미확정.
임시 파일 정리 및 배포 세트 교체는 상위 Packer의 책임이다.
이 단계의 index는 내부 메타데이터이며 공개 API와 최종 경로 정책은 미확정이다.

## 미결정 Policy

- 경로 규칙: UTF-8, slash, 대소문자, Windows 충돌, symlink 및 상위 경로 처리.
- 오류 반환/로그와 초기화 실패 후 기존 상태 보존 정책.
- 빈 패키지, 에셋 루트 결정, 패키징 결과 교체/실패 복구 정책.
- 기존 Load(path) 유지 범위와 논리 경로 API 및 명시적 소유권.

## 무결성 정책 (B안 확정)

- 초기화: index 전체의 CRC32 및 구조 검증, 모든 part의 파일 크기, magic, version,
  generation ID, part 번호 확인. payload는 읽지 않는다.
- 명시적 전체 검사: 각 part payload의 IEEE CRC32를 64 KiB 버퍼로 순차 계산하고
  index에 기록된 값과 비교한다. 이 기능은 추후 Packer verify 명령에 연결한다.
- 일반 로딩에서 CRC 전체 스캔을 보장하지 않는다. 동일 크기 payload 손상은
  초기화에서 발견되지 않을 수 있다. CRC32는 암호화/인증/변조 방어 수단이 아니다.
- 패키지는 사용 중 변경되지 않는다고 가정한다. Reader는 파일을 다시 열 때도
  header/크기를 확인하며 한 인스턴스를 여러 스레드가 동시에 쓰지 않는다.
- Generation ID는 16바이트의 nonzero 값이다. Packer에서 매번 새 ID 생성 예정.
  현재 내부 Writer/Reader는 ID를 인자로 받으며 테스트는 고정 ID를 사용한다.

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
Codec은 nonempty/no embedded NUL 및 정확히 같은 경로 중복만 검사한다.
UTF-8 검증, 상대 경로 정규화, 대소문자 충돌, traversal/symlink 등은 Packer와
Directory/Package 소스 도입 전에 확정하고 구현한다. 현재 경로를 OS 파일 접근에 사용하지 않는다.

Index 읽기 실패 시 출력 metadata를 유지하고, 잘린 index, magic/version/flags,
CRC, 비정상 count/length, 중복 경로, overflow/범위 오류, 남는 데이터를 거부한다.
Index 쓰기는 새 staging 파일에만 허용한다. staging 정리, generation ID 생성,
최종 배포 세트 교체 및 사용자용 오류 메시지는 Packer 단계의 책임이다.

## 다음 단계 / 완료 기준

1. 분할 I/O 및 경계 테스트 (Windows CI Debug/Release 및 사용자 로컬 Debug 통과).
2. 무결성 B안, 내부 index codec 및 part 검증 테스트 추가. 남은 경로/배포 Policy 및 Packer/Reader 공개 API 연동은 다음 작업.
3. Directory/Package 소스와 기존 Texture/Font 연동, Audio의 독립 스트리밍 연동.
4. 외부 CMake 연동 및 에셋 변경/삭제 감지, 배포 결과물 검증.
5. SampleGame 및 MazeRunner 실제 Windows 실행, 기존 CTest 회귀 검사.
6. 동일 에셋/동일 실행 환경에서 RAM, 시작 시간, 프레임 시간, 배포 용량 비교.

MazeRunner HUD 폰트는 현재 LWGF sample에서 복사되므로 게임 assets 루트만
패키징하면 누락된다. 원본 위치 통합과 OFL 라이선스 배포는 게임 연동 시 검토한다.
