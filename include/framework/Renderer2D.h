#pragma once

#include <string_view>

struct Color
{
    float r;
    float g;
    float b;
    float a;
};

class Texture2D;
class Font;

class Renderer2D
{
public:
    Renderer2D() = default;
    ~Renderer2D();

    Renderer2D(const Renderer2D&) = delete;
    Renderer2D& operator=(const Renderer2D&) = delete;

    bool Initialize(int screenWidth, int screenHeight);

    void BeginFrame();

    void DrawRect(
        float x,
        float y,
        float width,
        float height,
        const Color& color
    );

    void DrawTexture(
        const Texture2D& texture,
        float x,
        float y,
        float width,
        float height
    );

    // (_x, _baselineY) is the left-hand baseline, in bottom-left-origin pixels.
    void DrawText(
        const Font& _font,
        std::string_view _text,
        float _x,
        float _baselineY,
        float _pixelHeight,
        const Color& _color
    );

    // Returns pen advance, not the tight ink bounding box.
    float MeasureTextWidth(
        const Font& _font,
        std::string_view _text,
        float _pixelHeight
    ) const;

private:
    int m_screenWidth = 0;
    int m_screenHeight = 0;

    unsigned int m_vao = 0;
    unsigned int m_vbo = 0;
    unsigned int m_ebo = 0;

    unsigned int m_shaderProgram = 0;

    int m_positionLocation = -1;
    int m_sizeLocation = -1;
    int m_colorLocation = -1;

    int m_textureLocation = -1;
    int m_useTextureLocation = -1;
    int m_uvMinLocation = -1;
    int m_uvMaxLocation = -1;
};