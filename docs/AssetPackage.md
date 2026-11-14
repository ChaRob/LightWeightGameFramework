# LWGF Asset Package V1 — 작업 초안

상태: Product Goal 및 핵심 분할 요구사항 합의, 나머지 Policy/Technical Design은 검토 중.
이 문서는 완성된 공개 API나 확정된 PAK 바이너리 명세가 아니다.

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
현재 primitive는 크기 및 누락 검증만 한다. 동일 크기 내용 변경, 혼합 세대 및 index 검증은 아직 구현하지 않는다.
빈 데이터에서 part를 만들지 않는 동작은 내부 primitive의 동작이며 빈 패키지 허용 정책은 미확정.
임시 파일 정리 및 배포 세트 교체는 상위 Packer의 책임이다.
이 단계에서는 공개 API나 PAK 헤더/인덱스 형식을 확정하지 않는다.

## 미결정 Policy

- 무결성 검사: 같은 크기의 손상과 다른 빌드의 part 혼용을 어떻게 감지할지.
- 경로 규칙: UTF-8, slash, 대소문자, Windows 충돌, symlink 및 상위 경로 처리.
- 오류 반환/로그와 초기화 실패 후 기존 상태 보존 정책.
- 빈 패키지, 에셋 루트 결정, 패키징 결과 교체/실패 복구 정책.
- 기존 Load(path) 유지 범위와 논리 경로 API 및 명시적 소유권.

## 무결성 정책 검토 (미확정)

목표는 잘못된 배포 파일을 조기에 발견하는 것이며 악의적인 변조 방어는 아니다.
서로 다른 패키지 생성 세대의 part 혼용과 동일 크기의 데이터 손상은 별개의 문제다.
현재 SplitAssetReader의 크기 검증만으로는 두 문제를 발견할 수 없다.

- A: 모든 part의 CRC32를 index에 기록하고 게임 초기화 때 전체 payload 검사.
  손상/혼용 발견 범위가 넓으나 시작 시 모든 데이터 파일을 읽어야 한다.
- B (권장 초안): 초기화 때 index 구조 및 checksum, part의 package generation ID,
  part 번호, 실제 파일 크기를 검사. 전체 payload CRC32는 Packer의 명시적 verify 단계에서
  순차 검사한다. 일반 메모리 로딩은 필요 시 개별 에셋 checksum 검사도 가능하지만
  BGM 임의 seek에서 전체 에셋 CRC를 자동 검증한다고 보장할 수 없다.
  따라서 런타임에서 동일 크기의 payload 비트 손상이 언제나 검출되는 것은 아니다.
- C: 현재처럼 크기와 존재 여부만 검사. 비용은 최소이나 같은 크기의 혼용/손상 검출 불가.

B안에서는 part metadata를 파일 자체에 저장할 필요가 있다. header를 도입한다면
512 MiB 설정값은 header 포함 실제 파일 크기 상한으로 유지해야 하며, 실제 payload
capacity는 상한에서 header 크기를 뺀 값이 된다. 기존 raw split I/O는 내부 기반이며
이 변경은 아직 구현하지 않았다. 작은 테스트 분할 용량은 header보다 크게 조정해야 한다.
CRC32는 우발적 손상 검사이며 암호화 또는 인증 수단이 아니다.

### PAK index 구조 검토 초안 (미확정)

- Header: magic, format version, generation ID, part size limit, logical total size,
  part count, entry count, index checksum 및 필요한 format flags.
- Part metadata: part 번호, generation ID, 실제/논리 데이터 크기 및 payload checksum 검토.
- Entry: UTF-8 상대 경로, 논리 offset, size. V1에서 stored size와 original size는 동일.
- Offset/size는 64비트, 정수는 little-endian으로 명시 직렬화. 구조체 메모리 직접 기록 금지.
- Reader는 오버플로, 범위를 벗어난 entry, 중복 경로, 지원하지 않는 version/flags,
  잘못된 count/길이 및 잘린 index를 거부해야 한다.
- 이 초안은 wire format의 확정이 아니다. 무결성/경로 정책 결정 후 필드와 검증 규칙을 확정한다.

## 다음 단계 / 완료 기준

1. 분할 I/O 및 경계 테스트 (Windows CI Debug/Release 및 사용자 로컬 Debug 통과).
2. 남은 Policy 합의, 버전 있는 index 명세, Packer/Reader 및 구조 손상 테스트.
3. Directory/Package 소스와 기존 Texture/Font 연동, Audio의 독립 스트리밍 연동.
4. 외부 CMake 연동 및 에셋 변경/삭제 감지, 배포 결과물 검증.
5. SampleGame 및 MazeRunner 실제 Windows 실행, 기존 CTest 회귀 검사.
6. 동일 에셋/동일 실행 환경에서 RAM, 시작 시간, 프레임 시간, 배포 용량 비교.

MazeRunner HUD 폰트는 현재 LWGF sample에서 복사되므로 게임 assets 루트만
패키징하면 누락된다. 원본 위치 통합과 OFL 라이선스 배포는 게임 연동 시 검토한다.
