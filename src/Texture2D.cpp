#include <framework/Texture2D.h>

#include "GraphicsAssetData.h"

#include <iostream>
#include <memory>
#include <glad/gl.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace
{
    bool ValidFilter(TextureFilter _filter)
    {
        return _filter == TextureFilter::Linear || _filter == TextureFilter::Nearest;
    }
}

Texture2D::~Texture2D()
{
    if (m_textureId != 0)
        glDeleteTextures(1, &m_textureId);
}

bool Texture2D::Load(const char* _path)
{
    return Load(_path, TextureFilter::Linear);
}

bool Texture2D::Load(const char* _path, TextureFilter _filter)
{
    if (_path == nullptr || _path[0] == '\0' || !ValidFilter(_filter))
    {
        std::cerr << "Invalid texture path or filter.\n";
        return false;
    }

    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_set_flip_vertically_on_load(1);
    auto* pixels = stbi_load(_path, &width, &height, &channels, STBI_rgb_alpha);
    return UploadPixels(pixels, width, height, _filter, _path);
}

bool Texture2D::Load(const AssetReader& _assets, std::string_view _path,
                     TextureFilter _filter)
{
    if (!ValidFilter(_filter))
    {
        std::cerr << "Invalid texture filter.\n";
        return false;
    }

    std::vector<std::byte> data;
    if (!lwgf::detail::ReadGraphicsAsset(_assets, _path, data))
        return false;

    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_set_flip_vertically_on_load(1);
    auto* pixels = stbi_load_from_memory(
        reinterpret_cast<const stbi_uc*>(data.data()),
        static_cast<int>(data.size()), &width, &height, &channels, STBI_rgb_alpha);
    return UploadPixels(pixels, width, height, _filter, _path);
}

bool Texture2D::UploadPixels(unsigned char* _pixels, int _width, int _height,
                             TextureFilter _filter, std::string_view _path)
{
    std::unique_ptr<unsigned char, decltype(&stbi_image_free)> pixels(_pixels, stbi_image_free);
    if (!pixels)
    {
        std::cerr << "Failed to load texture: " << _path << '\n';
        if (const char* reason = stbi_failure_reason())
            std::cerr << "Reason: " << reason << '\n';
        return false;
    }

    unsigned int newTexture = 0;
    glGenTextures(1, &newTexture);
    if (newTexture == 0)
    {
        std::cerr << "Failed to create texture: " << _path << '\n';
        return false;
    }

    glBindTexture(GL_TEXTURE_2D, newTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    const int filter = _filter == TextureFilter::Nearest ? GL_NEAREST : GL_LINEAR;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, _width, _height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, pixels.get());
    glBindTexture(GL_TEXTURE_2D, 0);

    if (glGetError() != GL_NO_ERROR)
    {
        std::cerr << "Failed to upload texture: " << _path << '\n';
        glDeleteTextures(1, &newTexture);
        return false;
    }

    if (m_textureId != 0)
        glDeleteTextures(1, &m_textureId);

    m_textureId = newTexture;
    m_width = _width;
    m_height = _height;
    return true;
}

void Texture2D::Bind() const
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_textureId);
}

int Texture2D::GetWidth() const
{
    return m_width;
}

int Texture2D::GetHeight() const
{
    return m_height;
}
