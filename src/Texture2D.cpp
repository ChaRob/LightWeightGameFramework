#include <framework/Texture2D.h>

#include <iostream>
#include <glad/gl.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

Texture2D::~Texture2D()
{
    if (m_textureId != 0)
    {
        glDeleteTextures(
            1,
            &m_textureId
        );
    }
}

bool Texture2D::Load(const char* _path)
{
    int width = 0;
    int height = 0;
    int channels = 0;

    stbi_set_flip_vertically_on_load(1);

    unsigned char* pixels = stbi_load(
        _path,
        &width,
        &height,
        &channels,
        STBI_rgb_alpha
    );

    if (pixels == nullptr)
    {
        std::cerr
            << "Failed to load texture: "
            << _path
            << '\n';

        const char* reason = stbi_failure_reason();

        if (reason != nullptr)
        {
            std::cerr
                << "Reason: "
                << reason
                << '\n';
        }

        return false;
    }

    if (m_textureId != 0)
    {
        glDeleteTextures(
            1,
            &m_textureId
        );

        m_textureId = 0;
    }

    glGenTextures(
        1,
        &m_textureId
    );

    glBindTexture(
        GL_TEXTURE_2D,
        m_textureId
    );

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA8,
        width,
        height,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        pixels
    );

    glBindTexture(GL_TEXTURE_2D, 0);

    stbi_image_free(pixels);

    m_width = width;
    m_height = height;

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