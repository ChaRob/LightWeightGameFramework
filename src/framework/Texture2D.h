#pragma once

class Renderer2D;

class Texture2D
{
public:
    Texture2D() = default;
    ~Texture2D();

    Texture2D(const Texture2D&) = delete;
    Texture2D& operator=(const Texture2D&) = delete;

    bool Load(const char* path);

    int GetWidth() const;
    int GetHeight() const;

private:
    friend class Renderer2D;

    void Bind() const;

    unsigned int m_textureId = 0;

    int m_width = 0;
    int m_height = 0;
};