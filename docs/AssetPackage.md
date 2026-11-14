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

- 압축 여부: 초기 무압축 권장, 특히 스트리밍의 seek 비용 고려.
- 무결성 검사: 같은 크기의 손상과 다른 빌드의 part 혼용을 어떻게 감지할지.
- 경로 규칙: UTF-8, slash, 대소문자, Windows 충돌, symlink 및 상위 경로 처리.
- 오류 반환/로그와 초기화 실패 후 기존 상태 보존 정책.
- 빈 패키지, 에셋 루트 결정, 패키징 결과 교체/실패 복구 정책.
- 기존 Load(path) 유지 범위와 논리 경로 API 및 명시적 소유권.

## 다음 단계 / 완료 기준

1. 분할 I/O 및 경계 테스트.
2. 남은 Policy 합의, 버전 있는 index 명세, Packer/Reader 및 구조 손상 테스트.
3. Directory/Package 소스와 기존 Texture/Font 연동, Audio의 독립 스트리밍 연동.
4. 외부 CMake 연동 및 에셋 변경/삭제 감지, 배포 결과물 검증.
5. SampleGame 및 MazeRunner 실제 Windows 실행, 기존 CTest 회귀 검사.
6. 동일 에셋/동일 실행 환경에서 RAM, 시작 시간, 프레임 시간, 배포 용량 비교.

MazeRunner HUD 폰트는 현재 LWGF sample에서 복사되므로 게임 assets 루트만
패키징하면 누락된다. 원본 위치 통합과 OFL 라이선스 배포는 게임 연동 시 검토한다.
