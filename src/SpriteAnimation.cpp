#include <framework/SpriteAnimation.h>

#include <cmath>
#include <limits>
#include <utility>

bool SpriteAnimation::Configure(std::vector<SpriteFrame> _frames, AnimationMode _mode)
{
    if (_frames.empty() || (_mode != AnimationMode::Loop && _mode != AnimationMode::Once))
        return false;

    double total = 0.0;
    for (const SpriteFrame& frame : _frames)
    {
        if (!frame.region.IsValid() ||
            !frame.region.FitsWithin(std::numeric_limits<int>::max(), std::numeric_limits<int>::max()) ||
            !std::isfinite(frame.duration) || frame.duration <= 0.0)
            return false;

        const double next = total + frame.duration;
        if (!std::isfinite(next) || next <= total)
            return false;
        total = next;
    }

    m_frames = std::move(_frames);
    m_mode = _mode;
    m_totalDuration = total;
    Reset();
    return true;
}

bool SpriteAnimation::ConfigureStrip(TextureRegion _first, int _count, double _duration,
                                     AnimationMode _mode)
{
    if (!_first.IsValid() || _count <= 0 || !std::isfinite(_duration) || _duration <= 0.0 ||
        (_mode != AnimationMode::Loop && _mode != AnimationMode::Once) ||
        _first.height > std::numeric_limits<int>::max() - _first.y ||
        _count > (std::numeric_limits<int>::max() - _first.x) / _first.width ||
        !std::isfinite(_duration * _count))
        return false;

    std::vector<SpriteFrame> frames;
    frames.reserve(static_cast<std::size_t>(_count));
    for (int i = 0; i < _count; ++i)
    {
        frames.push_back({{_first.x + i * _first.width, _first.y,
                           _first.width, _first.height}, _duration});
    }
    return Configure(std::move(frames), _mode);
}

void SpriteAnimation::Play()
{
    if (!m_frames.empty() && !m_finished)
        m_playing = true;
}

void SpriteAnimation::Stop()
{
    m_playing = false;
}

void SpriteAnimation::Reset()
{
    m_frameIndex = 0;
    m_elapsed = 0.0;
    m_playing = false;
    m_finished = false;
}

void SpriteAnimation::Update(double _deltaTime)
{
    if (!m_playing || m_frames.empty() || !std::isfinite(_deltaTime) || _deltaTime <= 0.0)
        return;

    if (m_mode == AnimationMode::Once)
    {
        if (_deltaTime >= m_totalDuration - m_elapsed)
        {
            m_elapsed = m_totalDuration;
            m_frameIndex = m_frames.size() - 1;
            m_playing = false;
            m_finished = true;
            return;
        }
        m_elapsed += _deltaTime;
    }
    else
    {
        const double step = std::fmod(_deltaTime, m_totalDuration);
        const double remaining = m_totalDuration - m_elapsed;
        m_elapsed = step >= remaining ? step - remaining : m_elapsed + step;
        // Addition can round up to the end of the cycle.
        if (m_elapsed >= m_totalDuration)
            m_elapsed = 0.0;
    }

    // At most one scan, regardless of the number of elapsed cycles.
    double end = 0.0;
    m_frameIndex = m_frames.size() - 1;
    for (std::size_t i = 0; i < m_frames.size(); ++i)
    {
        end += m_frames[i].duration;
        if (m_elapsed < end)
        {
            m_frameIndex = i;
            break;
        }
    }
}

TextureRegion SpriteAnimation::GetCurrentFrame() const
{
    return m_frames.empty() ? TextureRegion{} : m_frames[m_frameIndex].region;
}
