#include <framework/Font.h>

#include "GraphicsAssetData.h"

#include <glad/gl.h>

#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <vector>

// The implementation macro must appear in exactly one translation unit,
// before including the stb_truetype header.
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

Font::~Font()
{
    if (m_textureId != 0)
    {
        glDeleteTextures(1, &m_textureId);
    }
}

bool Font::Load(const char* _path, float _pixelHeight)
{
    if (_path == nullptr || _path[0] == '\0' ||
        !std::isfinite(_pixelHeight) || _pixelHeight <= 0.0f)
    {
        std::cerr << "Invalid font path or pixel height.\n";
        return false;
    }

    std::ifstream file(_path, std::ios::binary | std::ios::ate);

    if (!file)
    {
        std::cerr << "Failed to open font: " << _path << '\n';
        return false;
    }

    const std::streamoff fileSize = file.tellg();

    if (fileSize <= 0 ||
        fileSize > static_cast<std::streamoff>(std::numeric_limits<int>::max()))
    {
        std::cerr << "Invalid font file size: " << _path << '\n';
        return false;
    }

    std::vector<std::byte> fontData(static_cast<std::size_t>(fileSize));
    file.seekg(0, std::ios::beg);

    if (!file.read(reinterpret_cast<char*>(fontData.data()),
                   static_cast<std::streamsize>(fileSize)))
    {
        std::cerr << "Failed to read font: " << _path << '\n';
        return false;
    }

    return LoadData(fontData, _pixelHeight, _path);
}

bool Font::Load(const AssetReader& _assets, std::string_view _path, float _pixelHeight)
{
    if (!std::isfinite(_pixelHeight) || _pixelHeight <= 0.0f)
    {
        std::cerr << "Invalid font pixel height.\n";
        return false;
    }

    std::vector<std::byte> fontData;
    if (!lwgf::detail::ReadGraphicsAsset(_assets, _path, fontData))
        return false;
    return LoadData(fontData, _pixelHeight, _path);
}

bool Font::LoadData(std::span<const std::byte> _data, float _pixelHeight,
                    std::string_view _path)
{
    // stb_truetype assumes trusted input and has no length-aware parser.
    // Reject obviously short/invalid SFNT or collection headers before use.
    const auto* fontData = reinterpret_cast<const unsigned char*>(_data.data());
    if (_data.size() < 12)
    {
        std::cerr << "Invalid font data size: " << _path << '\n';
        return false;
    }

    // A TTC with at least one font must have its first offset at bytes 12..15.
    if (fontData[0] == 't' && fontData[1] == 't' && fontData[2] == 'c' &&
        fontData[3] == 'f' && _data.size() < 16)
    {
        std::cerr << "Invalid font collection: " << _path << '\n';
        return false;
    }

    const int offset = stbtt_GetFontOffsetForIndex(fontData, 0);
    if (offset < 0 || static_cast<std::size_t>(offset) > _data.size() - 12)
    {
        std::cerr << "Unsupported font data: " << _path << '\n';
        return false;
    }

    // Try a small atlas first; a larger size allows taller glyphs.
    constexpr std::array<int, 2> atlasSizes = { 512, 1024 };
    std::array<stbtt_packedchar, CharacterCount> packed{};
    std::vector<unsigned char> atlas;
    int atlasSize = 0;
    bool packedSuccessfully = false;

    for (const int size : atlasSizes)
    {
        atlas.assign(static_cast<std::size_t>(size) * size, 0);
        packed.fill({});
        stbtt_pack_context context{};

        if (stbtt_PackBegin(&context, atlas.data(), size, size, 0, 2, nullptr) == 0)
        {
            std::cerr << "Failed to allocate font pack context.\n";
            return false;
        }

        const int success = stbtt_PackFontRange(
            &context, fontData, 0, _pixelHeight,
            static_cast<int>(FirstCharacter),
            static_cast<int>(CharacterCount),
            packed.data()
        );

        stbtt_PackEnd(&context);

        if (success != 0)
        {
            atlasSize = size;
            packedSuccessfully = true;
            break;
        }
    }

    if (!packedSuccessfully)
    {
        std::cerr << "Font glyphs do not fit the atlas: " << _path << '\n';
        return false;
    }

    std::array<Glyph, CharacterCount> glyphs{};

    for (unsigned int i = 0; i < CharacterCount; ++i)
    {
        float penX = 0.0f;
        float penY = 0.0f;
        stbtt_aligned_quad quad{};

        stbtt_GetPackedQuad(
            packed.data(), atlasSize, atlasSize, static_cast<int>(i),
            &penX, &penY, &quad, 0
        );

        Glyph& glyph = glyphs[i];
        glyph.xOffset = quad.x0;
        glyph.yOffset = -quad.y1;
        glyph.width = quad.x1 - quad.x0;
        glyph.height = quad.y1 - quad.y0;
        glyph.advance = packed[i].xadvance;
        // stb uses a top-down bitmap; OpenGL uploads its first row at V=0.
        glyph.u0 = quad.s0;
        glyph.v0 = quad.t1;
        glyph.u1 = quad.s1;
        glyph.v1 = quad.t0;
    }

    unsigned int newTexture = 0;
    glGenTextures(1, &newTexture);

    if (newTexture == 0)
    {
        std::cerr << "Failed to create font atlas texture: " << _path << '\n';
        return false;
    }

    int previousUnpackAlignment = 4;
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &previousUnpackAlignment);

    glBindTexture(GL_TEXTURE_2D, newTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(
        GL_TEXTURE_2D, 0, GL_R8, atlasSize, atlasSize, 0,
        GL_RED, GL_UNSIGNED_BYTE, atlas.data()
    );
    glPixelStorei(GL_UNPACK_ALIGNMENT, previousUnpackAlignment);
    glBindTexture(GL_TEXTURE_2D, 0);

    if (glGetError() != GL_NO_ERROR)
    {
        std::cerr << "Failed to upload font atlas: " << _path << '\n';
        glDeleteTextures(1, &newTexture);
        return false;
    }

    if (m_textureId != 0)
    {
        glDeleteTextures(1, &m_textureId);
    }

    m_textureId = newTexture;
    m_pixelHeight = _pixelHeight;
    m_glyphs = glyphs;
    return true;
}

float Font::GetPixelHeight() const
{
    return m_pixelHeight;
}

bool Font::IsLoaded() const
{
    return m_textureId != 0;
}
