# LWGF Audio V1

LWGF Audio는 GLFW/OpenGL 및 게임 상태에서 독립된 기능입니다. 사운드 재생,
정지, 일시정지는 Game이 결정합니다. Framework는 파일 디코딩, 동시 재생,
리소스 정리 및 장치 초기화를 담당하며 자체 Audio Engine을 만들지 않습니다.

## 의존성 및 기본 방식

- miniaudio **0.11.25** (CMake FetchContent)
- 효과음: WAV 권장. LoadEffect() 시 메모리 디코딩, 재생 시 복사 인스턴스 생성.
- BGM: WAV/MP3 파일을 PlayMusic()에서 스트리밍으로 준비 후 재생.
- 기본 디코더: WAV/MP3/FLAC. OGG는 이번 범위에서 제외.
- AudioSystem은 Game이 소유합니다. 복사할 수 없습니다.

## Game에서 사용하는 방법

~~~cpp
#include <framework/AudioSystem.h>

AudioSystem audio;
if (!audio.Initialize()) {
    // Game이 사운드 없이 계속할지, 종료할지 결정합니다.
}
const auto click = audio.LoadEffect("assets/audio/click.wav");
if (click == AudioSystem::InvalidEffect) {
    // 다른 사운드에 영향을 주지 않습니다.
}

// 게임 루프에서 매 프레임 audio.Update() 호출.
// 이벤트 발생 시 auto result = audio.PlayEffect(click);
// 결과: Success / NotInitialized / InvalidEffect / LimitReached / BackendError.

// BGM
audio.PlayMusic("assets/audio/music.wav", true);
audio.PauseMusic();
audio.ResumeMusic();
audio.StopMusic();

// 음량: 각각 0.0 ~ 1.0, 기본값 1.0
audio.SetMasterVolume(0.8f);
audio.SetEffectVolume(0.7f);
audio.SetMusicVolume(0.5f);
~~~

Sample 키: SPACE=효과음, M=BGM 시작, P=일시정지,
R=재개, N=정지. Sample용 WAV는 직접 생성한 테스트 음원입니다.

## 확정 정책

| 상황 | 처리 |
| --- | --- |
| 장치 초기화 실패 | 원인 로그, 실패 반환. Game이 계속 실행 여부 결정 |
| 파일 누락/손상 | 해당 요청만 실패, 경로 및 백엔드 오류 로그 |
| 동일 효과음 연속 재생 | 독립된 복사본으로 중첩 재생 |
| SFX 동시 재생 한도 | 전체 공통. 기본 16은 측정 전 임시 설정 |
| 한도 초과 | LimitReached 반환, 새 요청 거절, 기존 사운드 유지 |
| 재생 완료 | Update()에서 인스턴스 정리. 다음 PlayEffect()에서도 정리 |
| 새 BGM 요청 | 새 파일 준비/시작 성공 후 기존 음악 교체 |
| 새 BGM 실패 | 기존 BGM 유지 |
| 동일한 BGM 요청 | 재생 또는 일시정지 상태 유지, 끝난 음악은 재시작 가능 |
| PauseMusic | 위치 유지, ResumeMusic으로 이어서 재생 |
| StopMusic | 현재 음악 해제, 다음 재생은 처음부터 시작 |
| Game PAUSED | Framework 자동 대응 없음. Game이 직접 Audio 제어 |
| 장치 연결 해제 | Game 실행은 유지, 자동 복구는 보장하지 않음 |
| 음량 | Master × SFX 또는 Master × BGM. 개별 음량 제외 |
| 리소스 해제 | UnloadEffect()는 그 효과음의 재생 중 인스턴스도 종료 |
| Shutdown | 재생 객체를 먼저 해제하고 Audio 장치 정리 |

같은 음악을 일시정지한 상태에서 같은 경로로 PlayMusic을 다시 호출하면
재개되지 않습니다. 반드시 ResumeMusic을 호출해야 합니다.
루프 설정을 바꾸려면 StopMusic 후 새로 PlayMusic을 호출합니다.

Initialize() 중복 호출은 실패하며 Shutdown()은 중복 호출해도 안전합니다.
공개 AudioSystem API는 Game 스레드에서 호출하는 것으로 가정합니다.
백엔드 오디오 콜백에서 직접 호출하지 않습니다.

## 범위 제외 / 검증

3D/Spatial Audio, 개별 사운드 볼륨, Crossfade, 우선순위 기반 재생 교체,
동적 장치 재연결, 에디터, 설정 저장 시스템은 제외합니다.

Windows 실제 장치에서 효과음 중첩/제한, BGM 반복 및 Pause/Resume,
파일 누락, 장치 제거, Release 에셋 복사, 메모리/CPU/프레임 시간 등을
실제로 검증해야 합니다. 기본 동시 재생 수 16은 성능 목표치가 아닙니다.
