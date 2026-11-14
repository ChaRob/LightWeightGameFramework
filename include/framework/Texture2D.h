#pragma once

#include <string_view>

class Renderer2D;
class AssetReader;

enum class TextureFilter { Linear, Nearest };

class Texture2D
{
public:
    Texture2D() = default;
    ~Texture2D();

    Texture2D(const Texture2D&) = delete;
    Texture2D& operator=(const Texture2D&) = delete;

    bool Load(const char* _path);
    bool Load(const char* _path, TextureFilter _filter);

    // Loads a logical asset through either configured source.
    // Requires a live GL context, as does the physical path overload.
    bool Load(const AssetReader& _assets, std::string_view _path,
              TextureFilter _filter = TextureFilter::Linear);

    int GetWidth() const;
    int GetHeight() const;

private:
    friend class Renderer2D;

    void Bind() const;
    bool UploadPixels(unsigned char* _pixels, int _width, int _height,
                      TextureFilter _filter, std::string_view _path);

    unsigned int m_textureId = 0;

    int m_width = 0;
    int m_height = 0;
};