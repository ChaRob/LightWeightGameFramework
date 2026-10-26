#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

// Game-owned audio service. Does not depend on GLFW, OpenGL or game pause state.
// Create one instance, initialize it, and call Update() once per game frame.
class AudioSystem
{
public:
    using EffectId = std::uint32_t;
    static constexpr EffectId InvalidEffect = 0;

    enum class PlayResult
    {
        Success,
        NotInitialized,
        InvalidEffect,
        LimitReached,
        BackendError
    };

    AudioSystem() = default;
    ~AudioSystem();

    AudioSystem(const AudioSystem&) = delete;
    AudioSystem& operator=(const AudioSystem&) = delete;

    // The default voice limit is provisional; tune it after profiling the game.
    bool Initialize(std::size_t _maxConcurrentEffects = 16);
    void Shutdown();
    bool IsInitialized() const;

    // Load short sound effects into decoded memory. InvalidEffect means failure.
    EffectId LoadEffect(const char* _path);
    bool UnloadEffect(EffectId _effect);
    PlayResult PlayEffect(EffectId _effect);
    void StopAllEffects();

    // Reclaim finished voices; call once each frame (no allocation in Update).
    void Update();
    std::size_t GetActiveEffectCount() const;

    // Music streams from file. A failed switch leaves the current music intact.
    // Requesting the same track while playing/paused preserves its state.
    bool PlayMusic(const char* _path, bool _loop = true);
    bool PauseMusic();
    bool ResumeMusic();
    void StopMusic();

    // Linear gain: 0.0 = silent, 1.0 = normal. Non-finite values are ignored.
    void SetMasterVolume(float _volume);
    void SetEffectVolume(float _volume);
    void SetMusicVolume(float _volume);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;

    float m_masterVolume = 1.0f;
    float m_effectVolume = 1.0f;
    float m_musicVolume = 1.0f;
};
