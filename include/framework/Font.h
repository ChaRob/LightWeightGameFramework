#pragma once

#include <array>

class Renderer2D;

// Owns a rasterized, single-channel glyph atlas. Requires a live GL context.
class Font
{
public:
    Font() = default;
    ~Font();

    Font(const Font&) = delete;
    Font& operator=(const Font&) = delete;

    // Loads printable ASCII (32..126) from a TTF/TrueType-outline font.
    // Retains the previous font on failure.
    bool Load(const char* _path, float _pixelHeight = 48.0f);

    float GetPixelHeight() const;
    bool IsLoaded() const;

private:
    friend class Renderer2D;

    struct Glyph
    {
        float xOffset = 0.0f;
        float yOffset = 0.0f; // Bottom edge relative to baseline, Y up.
        float width = 0.0f;
        float height = 0.0f;
        float advance = 0.0f;
        float u0 = 0.0f;
        float v0 = 0.0f;
        float u1 = 0.0f;
        float v1 = 0.0f;
    };

    static constexpr unsigned int FirstCharacter = 32;
    static constexpr unsigned int CharacterCount = 95;

    std::array<Glyph, CharacterCount> m_glyphs{};
    unsigned int m_textureId = 0;
    float m_pixelHeight = 0.0f;
};
