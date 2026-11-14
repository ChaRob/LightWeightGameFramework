#pragma once

#include <framework/TextureRegion.h>

#include <cstddef>
#include <vector>

struct SpriteFrame
{
    TextureRegion region;
    double duration = 0.0; // Seconds.
};

enum class AnimationMode { Loop, Once };

// CPU-only playback state. Does not own or reference a Texture2D.
class SpriteAnimation
{
public:
    // Successful configuration resets and stops playback; failure preserves it.
    bool Configure(std::vector<SpriteFrame> _frames, AnimationMode _mode = AnimationMode::Loop);
    bool ConfigureStrip(TextureRegion _first, int _count, double _duration,
                        AnimationMode _mode = AnimationMode::Loop);

    void Play(); // Resume; repeated calls are harmless. Reset to replay a completed Once.
    void Stop(); // Pause at the current time.
    void Reset(); // First frame, stopped, not finished.
    void Update(double _deltaTime);

    bool IsPlaying() const { return m_playing; }
    bool IsFinished() const { return m_finished; }
    std::size_t GetFrameIndex() const { return m_frameIndex; }
    TextureRegion GetCurrentFrame() const;

private:
    std::vector<SpriteFrame> m_frames;
    AnimationMode m_mode = AnimationMode::Loop;
    std::size_t m_frameIndex = 0;
    double m_totalDuration = 0.0;
    double m_elapsed = 0.0;
    bool m_playing = false;
    bool m_finished = false;
};
