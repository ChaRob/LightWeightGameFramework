#include <framework/AudioSystem.h>

// Explicit checks remain active in Release/NDEBUG builds.
#define CHECK(_condition) do { if (!(_condition)) return __LINE__; } while (false)

int main()
{
    // Device-independent API tests: CI may not provide speakers.
    AudioSystem audio;
    CHECK(!audio.IsInitialized());
    CHECK(!audio.Initialize(0));
    CHECK(!audio.IsInitialized());

    CHECK(audio.LoadEffect("not-found.wav") == AudioSystem::InvalidEffect);
    CHECK(audio.PlayEffect(1) == AudioSystem::PlayResult::NotInitialized);
    CHECK(!audio.UnloadEffect(1));
    CHECK(!audio.PlayMusic("not-found.wav"));
    CHECK(!audio.PauseMusic());
    CHECK(!audio.ResumeMusic());
    CHECK(audio.GetActiveEffectCount() == 0);

    audio.SetMasterVolume(-2.0f);
    audio.SetEffectVolume(2.0f);
    audio.SetMusicVolume(0.5f);
    audio.StopAllEffects();
    audio.StopMusic();
    audio.Update();
    audio.Shutdown();
    audio.Shutdown();
    CHECK(!audio.IsInitialized());
    return 0;
}
