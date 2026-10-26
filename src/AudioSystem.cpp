#include <framework/AudioSystem.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <string>
#include <unordered_map>
#include <utility>

#include <miniaudio.h>

namespace
{
    constexpr ma_uint32 kEffectFlags =
        MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_NO_SPATIALIZATION | MA_SOUND_FLAG_NO_PITCH;

    constexpr ma_uint32 kMusicFlags =
        MA_SOUND_FLAG_STREAM | MA_SOUND_FLAG_NO_SPATIALIZATION | MA_SOUND_FLAG_NO_PITCH;

    void LogAudioError(const char* _operation, const char* _path, ma_result _result)
    {
        std::cerr << "[Audio] " << _operation;
        if (_path != nullptr)
        {
            std::cerr << " '" << _path << "'";
        }
        std::cerr << " failed: " << ma_result_description(_result) << '\n';
    }

    // ma_sound must remain at a stable address and be uninitialized before its engine.
    struct OwnedSound
    {
        ma_sound sound{};
        bool initialized = false;

        OwnedSound() = default;
        OwnedSound(const OwnedSound&) = delete;
        OwnedSound& operator=(const OwnedSound&) = delete;

        ~OwnedSound()
        {
            Reset();
        }

        void Reset()
        {
            if (initialized)
            {
                ma_sound_uninit(&sound);
                initialized = false;
            }
        }
    };
}

struct AudioSystem::Impl
{
    struct Voice
    {
        OwnedSound instance;
        EffectId source = InvalidEffect;
    };

    explicit Impl(std::size_t _capacity)
        : voices(std::make_unique<Voice[]>(_capacity)),
          capacity(_capacity)
    {
    }

    ~Impl()
    {
        music.reset();
        for (std::size_t i = 0; i < capacity; ++i)
        {
            voices[i].instance.Reset();
        }
        effects.clear();

        if (engineInitialized)
        {
            ma_engine_uninit(&engine);
        }
    }

    ma_engine engine{};
    bool engineInitialized = false;

    std::unordered_map<EffectId, std::unique_ptr<OwnedSound>> effects;
    EffectId nextEffectId = 1;

    std::unique_ptr<Voice[]> voices;
    std::size_t capacity = 0;

    std::unique_ptr<OwnedSound> music;
    std::string musicPath;
    bool musicPaused = false;
};

AudioSystem::AudioSystem() = default;

AudioSystem::~AudioSystem()
{
    Shutdown();
}

bool AudioSystem::Initialize(std::size_t _maxConcurrentEffects)
{
    if (m_impl != nullptr)
    {
        std::cerr << "[Audio] Already initialized.\n";
        return false;
    }

    if (_maxConcurrentEffects == 0)
    {
        std::cerr << "[Audio] Concurrent effect limit must be greater than zero.\n";
        return false;
    }

    auto impl = std::make_unique<Impl>(_maxConcurrentEffects);
    const ma_result result = ma_engine_init(nullptr, &impl->engine);
    if (result != MA_SUCCESS)
    {
        LogAudioError("Initialize", nullptr, result);
        return false;
    }

    impl->engineInitialized = true;
    ma_engine_set_volume(&impl->engine, m_masterVolume);
    m_impl = std::move(impl);
    return true;
}

void AudioSystem::Shutdown()
{
    m_impl.reset();
}

bool AudioSystem::IsInitialized() const
{
    return m_impl != nullptr;
}

AudioSystem::EffectId AudioSystem::LoadEffect(const char* _path)
{
    if (m_impl == nullptr || _path == nullptr || _path[0] == '\0')
    {
        std::cerr << "[Audio] LoadEffect requires initialized audio and a nonempty path.\n";
        return InvalidEffect;
    }

    if (m_impl->nextEffectId == InvalidEffect)
    {
        std::cerr << "[Audio] Effect ID space exhausted.\n";
        return InvalidEffect;
    }

    auto effect = std::make_unique<OwnedSound>();
    const ma_result result = ma_sound_init_from_file(
        &m_impl->engine, _path, kEffectFlags, nullptr, nullptr, &effect->sound);

    if (result != MA_SUCCESS)
    {
        LogAudioError("LoadEffect", _path, result);
        return InvalidEffect;
    }

    effect->initialized = true;
    const EffectId id = m_impl->nextEffectId++;
    m_impl->effects.emplace(id, std::move(effect));
    return id;
}

bool AudioSystem::UnloadEffect(EffectId _effect)
{
    if (m_impl == nullptr)
    {
        return false;
    }

    const auto it = m_impl->effects.find(_effect);
    if (it == m_impl->effects.end())
    {
        return false;
    }

    // Copies must be gone before the source sound is destroyed.
    for (std::size_t i = 0; i < m_impl->capacity; ++i)
    {
        Impl::Voice& voice = m_impl->voices[i];
        if (voice.source == _effect)
        {
            voice.instance.Reset();
            voice.source = InvalidEffect;
        }
    }

    m_impl->effects.erase(it);
    return true;
}

AudioSystem::PlayResult AudioSystem::PlayEffect(EffectId _effect)
{
    if (m_impl == nullptr)
    {
        return PlayResult::NotInitialized;
    }

    const auto it = m_impl->effects.find(_effect);
    if (it == m_impl->effects.end())
    {
        return PlayResult::InvalidEffect;
    }

    // Reclaim a voice even if the game did not run Update() yet this frame.
    Update();

    Impl::Voice* freeVoice = nullptr;
    for (std::size_t i = 0; i < m_impl->capacity; ++i)
    {
        Impl::Voice& voice = m_impl->voices[i];
        if (!voice.instance.initialized)
        {
            freeVoice = &voice;
            break;
        }
    }

    if (freeVoice == nullptr)
    {
        return PlayResult::LimitReached; // A normal runtime condition; do not spam logs.
    }

    const ma_result initResult = ma_sound_init_copy(
        &m_impl->engine, &it->second->sound, kEffectFlags, nullptr, &freeVoice->instance.sound);

    if (initResult != MA_SUCCESS)
    {
        LogAudioError("PlayEffect (copy)", nullptr, initResult);
        return PlayResult::BackendError;
    }

    freeVoice->instance.initialized = true;
    freeVoice->source = _effect;
    ma_sound_set_volume(&freeVoice->instance.sound, m_effectVolume);

    const ma_result startResult = ma_sound_start(&freeVoice->instance.sound);
    if (startResult != MA_SUCCESS)
    {
        LogAudioError("PlayEffect (start)", nullptr, startResult);
        freeVoice->instance.Reset();
        freeVoice->source = InvalidEffect;
        return PlayResult::BackendError;
    }

    return PlayResult::Success;
}

void AudioSystem::StopAllEffects()
{
    if (m_impl == nullptr)
    {
        return;
    }

    for (std::size_t i = 0; i < m_impl->capacity; ++i)
    {
        m_impl->voices[i].instance.Reset();
        m_impl->voices[i].source = InvalidEffect;
    }
}

void AudioSystem::Update()
{
    if (m_impl == nullptr)
    {
        return;
    }

    for (std::size_t i = 0; i < m_impl->capacity; ++i)
    {
        Impl::Voice& voice = m_impl->voices[i];
        if (voice.instance.initialized && ma_sound_at_end(&voice.instance.sound))
        {
            voice.instance.Reset();
            voice.source = InvalidEffect;
        }
    }
}

std::size_t AudioSystem::GetActiveEffectCount() const
{
    if (m_impl == nullptr)
    {
        return 0;
    }

    std::size_t count = 0;
    for (std::size_t i = 0; i < m_impl->capacity; ++i)
    {
        const Impl::Voice& voice = m_impl->voices[i];
        if (voice.instance.initialized && !ma_sound_at_end(&voice.instance.sound))
        {
            ++count;
        }
    }
    return count;
}

bool AudioSystem::PlayMusic(const char* _path, bool _loop)
{
    if (m_impl == nullptr || _path == nullptr || _path[0] == '\0')
    {
        std::cerr << "[Audio] PlayMusic requires initialized audio and a nonempty path.\n";
        return false;
    }

    if (m_impl->music != nullptr && m_impl->musicPath == _path)
    {
        // Same-track requests do not cancel a pause or restart an active track.
        if (m_impl->musicPaused || !ma_sound_at_end(&m_impl->music->sound))
        {
            return true;
        }

        const ma_result seekResult = ma_sound_seek_to_pcm_frame(&m_impl->music->sound, 0);
        if (seekResult != MA_SUCCESS)
        {
            LogAudioError("PlayMusic (seek)", _path, seekResult);
            return false;
        }

        const ma_result restartResult = ma_sound_start(&m_impl->music->sound);
        if (restartResult != MA_SUCCESS)
        {
            LogAudioError("PlayMusic (restart)", _path, restartResult);
            return false;
        }

        return true;
    }

    auto nextMusic = std::make_unique<OwnedSound>();
    const ma_uint32 flags = kMusicFlags | (_loop ? MA_SOUND_FLAG_LOOPING : 0);
    const ma_result loadResult = ma_sound_init_from_file(
        &m_impl->engine, _path, flags, nullptr, nullptr, &nextMusic->sound);

    if (loadResult != MA_SUCCESS)
    {
        LogAudioError("PlayMusic (prepare)", _path, loadResult);
        return false; // Existing music is untouched.
    }

    nextMusic->initialized = true;
    ma_sound_set_volume(&nextMusic->sound, m_musicVolume);

    // Start first so a start failure also leaves the old music intact.
    const ma_result startResult = ma_sound_start(&nextMusic->sound);
    if (startResult != MA_SUCCESS)
    {
        LogAudioError("PlayMusic (start)", _path, startResult);
        return false;
    }

    std::string nextPath(_path);
    m_impl->music.swap(nextMusic);
    m_impl->musicPath.swap(nextPath);
    m_impl->musicPaused = false;
    // nextMusic now holds the old music; its destructor stops and releases it.
    return true;
}

bool AudioSystem::PauseMusic()
{
    if (m_impl == nullptr || m_impl->music == nullptr)
    {
        return false;
    }

    if (m_impl->musicPaused)
    {
        return true;
    }

    const ma_result result = ma_sound_stop(&m_impl->music->sound);
    if (result != MA_SUCCESS)
    {
        LogAudioError("PauseMusic", nullptr, result);
        return false;
    }

    m_impl->musicPaused = true;
    return true;
}

bool AudioSystem::ResumeMusic()
{
    if (m_impl == nullptr || m_impl->music == nullptr)
    {
        return false;
    }

    if (!m_impl->musicPaused)
    {
        return true;
    }

    const ma_result result = ma_sound_start(&m_impl->music->sound);
    if (result != MA_SUCCESS)
    {
        LogAudioError("ResumeMusic", nullptr, result);
        return false;
    }

    m_impl->musicPaused = false;
    return true;
}

void AudioSystem::StopMusic()
{
    if (m_impl != nullptr)
    {
        m_impl->music.reset();
        m_impl->musicPath.clear();
        m_impl->musicPaused = false;
    }
}

void AudioSystem::SetMasterVolume(float _volume)
{
    if (!std::isfinite(_volume)) return;
    m_masterVolume = std::clamp(_volume, 0.0f, 1.0f);
    if (m_impl != nullptr)
    {
        ma_engine_set_volume(&m_impl->engine, m_masterVolume);
    }
}

void AudioSystem::SetEffectVolume(float _volume)
{
    if (!std::isfinite(_volume)) return;
    m_effectVolume = std::clamp(_volume, 0.0f, 1.0f);
    if (m_impl != nullptr)
    {
        for (std::size_t i = 0; i < m_impl->capacity; ++i)
        {
            OwnedSound& sound = m_impl->voices[i].instance;
            if (sound.initialized)
            {
                ma_sound_set_volume(&sound.sound, m_effectVolume);
            }
        }
    }
}

void AudioSystem::SetMusicVolume(float _volume)
{
    if (!std::isfinite(_volume)) return;
    m_musicVolume = std::clamp(_volume, 0.0f, 1.0f);
    if (m_impl != nullptr && m_impl->music != nullptr)
    {
        ma_sound_set_volume(&m_impl->music->sound, m_musicVolume);
    }
}
