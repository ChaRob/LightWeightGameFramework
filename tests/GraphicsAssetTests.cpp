#include <framework/AssetReader.h>
#include <framework/Font.h>
#include <framework/Texture2D.h>
#include <glad/gl.h>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <iostream>
#include <set>
#include <vector>

#define CHECK(_condition) do { if (!(_condition)) { \
    std::cerr << "Check failed at line " << __LINE__ << ": " << #_condition << '\n'; \
    return __LINE__; } } while (false)

namespace
{
    namespace fs = std::filesystem;

    struct Upload
    {
        GLsizei width = 0;
        GLsizei height = 0;
        GLenum format = 0;
        GLint internalFormat = 0;
        GLint minFilter = 0;
        GLint magFilter = 0;
        std::vector<unsigned char> pixels;
        bool operator==(const Upload&) const = default;
    };

    // Test double only: capture the bytes submitted by the real stb decoders
    // and emulate texture lifetime without a GPU, window or OpenGL context.
    Upload lastUpload;
    std::set<GLuint> liveTextures;
    GLuint nextId = 1;
    GLuint bound = 0;
    GLint unpackAlignment = 4;
    GLenum nextError = GL_NO_ERROR;
    bool failCreate = false;

    void GLAD_API_PTR GenTextures(GLsizei _count, GLuint* _textures)
    {
        for (GLsizei i = 0; i < _count; ++i)
        {
            _textures[i] = failCreate ? 0 : nextId++;
            if (_textures[i] != 0)
                liveTextures.insert(_textures[i]);
        }
    }

    void GLAD_API_PTR DeleteTextures(GLsizei _count, const GLuint* _textures)
    {
        for (GLsizei i = 0; i < _count; ++i)
            liveTextures.erase(_textures[i]);
    }

    void GLAD_API_PTR BindTexture(GLenum, GLuint _texture)
    {
        bound = _texture;
    }

    void GLAD_API_PTR TexParameteri(GLenum, GLenum _name, GLint _value)
    {
        if (_name == GL_TEXTURE_MIN_FILTER)
            lastUpload.minFilter = _value;
        if (_name == GL_TEXTURE_MAG_FILTER)
            lastUpload.magFilter = _value;
    }

    void GLAD_API_PTR TexImage2D(GLenum, GLint, GLint _internalFormat, GLsizei _width,
                               GLsizei _height, GLint, GLenum _format, GLenum _type,
                               const void* _pixels)
    {
        lastUpload.width = _width;
        lastUpload.height = _height;
        lastUpload.format = _format;
        lastUpload.internalFormat = _internalFormat;
        const auto* bytes = static_cast<const unsigned char*>(_pixels);
        if (bound == 0 || _width <= 0 || _height <= 0 || _pixels == nullptr ||
            _type != GL_UNSIGNED_BYTE || (_format != GL_RGBA && _format != GL_RED))
        {
            nextError = GL_INVALID_VALUE;
            return;
        }
        const auto count = static_cast<std::size_t>(_width) *
                           static_cast<std::size_t>(_height) * (_format == GL_RGBA ? 4u : 1u);
        lastUpload.pixels.assign(bytes, bytes + count);
    }

    void GLAD_API_PTR GetIntegerv(GLenum _name, GLint* _value)
    {
        if (_name == GL_UNPACK_ALIGNMENT)
            *_value = unpackAlignment;
    }

    void GLAD_API_PTR PixelStorei(GLenum _name, GLint _value)
    {
        if (_name == GL_UNPACK_ALIGNMENT)
            unpackAlignment = _value;
    }

    GLenum GLAD_API_PTR GetError()
    {
        const auto error = nextError;
        nextError = GL_NO_ERROR;
        return error;
    }

    void InstallGlTestDouble()
    {
        glad_glGenTextures = GenTextures;
        glad_glDeleteTextures = DeleteTextures;
        glad_glBindTexture = BindTexture;
        glad_glTexParameteri = TexParameteri;
        glad_glTexImage2D = TexImage2D;
        glad_glGetIntegerv = GetIntegerv;
        glad_glPixelStorei = PixelStorei;
        glad_glGetError = GetError;
    }

    int RunTests(const fs::path& _source, const fs::path& _package)
    {
        InstallGlTestDouble();

        // Keep the legacy narrow file path tests independent of the source
        // directory's character set by using a simple relative filename.
        const auto source = fs::absolute(_source);
        const auto package = fs::absolute(_package);
        fs::current_path(source);

        AssetReader original;
        AssetReader packed;
        CHECK(original.OpenDirectory(source));
        CHECK(packed.OpenPackage(package));

        {
            Texture2D legacy;
            Texture2D directory;
            Texture2D packaged;

            CHECK(legacy.Load("images.jpg"));
            const auto legacyUpload = lastUpload;
            CHECK(legacyUpload.width > 0 && legacyUpload.height > 0);
            CHECK(legacyUpload.format == GL_RGBA && legacyUpload.internalFormat == GL_RGBA8);
            CHECK(legacyUpload.minFilter == GL_LINEAR && legacyUpload.magFilter == GL_LINEAR);
            CHECK(!legacyUpload.pixels.empty());

            CHECK(directory.Load(original, "images.jpg"));
            CHECK(lastUpload == legacyUpload);
            CHECK(packaged.Load(packed, "images.jpg"));
            CHECK(lastUpload == legacyUpload);
            CHECK(directory.GetWidth() == packaged.GetWidth());
            CHECK(directory.GetHeight() == packaged.GetHeight());

            for (const char* path : {"images2.png", "player_walk_down.png", "region_test.png"})
            {
                CHECK(directory.Load(original, path, TextureFilter::Nearest));
                const auto directoryUpload = lastUpload;
                CHECK(directoryUpload.minFilter == GL_NEAREST);
                CHECK(directoryUpload.magFilter == GL_NEAREST);
                CHECK(packaged.Load(packed, path, TextureFilter::Nearest));
                CHECK(lastUpload == directoryUpload);
            }

            const int width = packaged.GetWidth();
            const int height = packaged.GetHeight();
            const auto beforeFailure = liveTextures;
            CHECK(!packaged.Load(packed, "missing.png"));
            CHECK(!packaged.Load(packed, "fonts/AnonymousPro-Regular.ttf"));
            CHECK(!packaged.Load(packed, "images.jpg", static_cast<TextureFilter>(-1)));
            failCreate = true;
            CHECK(!packaged.Load(packed, "images.jpg"));
            failCreate = false;
            nextError = GL_OUT_OF_MEMORY;
            CHECK(!packaged.Load(packed, "images.jpg"));
            CHECK(packaged.GetWidth() == width && packaged.GetHeight() == height);
            CHECK(liveTextures == beforeFailure);
        }
        CHECK(liveTextures.empty());

        {
            Font legacy;
            Font directory;
            Font packaged;
            constexpr auto path = "fonts/AnonymousPro-Regular.ttf";

            CHECK(legacy.Load(path, 48.0f));
            const auto legacyUpload = lastUpload;
            CHECK(legacyUpload.format == GL_RED && legacyUpload.internalFormat == GL_R8);
            CHECK(legacyUpload.width > 0 && legacyUpload.height > 0);
            CHECK(std::any_of(legacyUpload.pixels.begin(), legacyUpload.pixels.end(),
                              [](unsigned char _pixel) { return _pixel != 0; }));

            CHECK(directory.Load(original, path, 48.0f));
            CHECK(lastUpload == legacyUpload);
            CHECK(packaged.Load(packed, path, 48.0f));
            CHECK(lastUpload == legacyUpload);
            CHECK(packaged.IsLoaded() && packaged.GetPixelHeight() == 48.0f);
            CHECK(unpackAlignment == 4);

            const auto beforeFailure = liveTextures;
            CHECK(!packaged.Load(packed, "missing.ttf"));
            CHECK(!packaged.Load(packed, "images.jpg"));
            CHECK(!packaged.Load(packed, path, 0.0f));
            failCreate = true;
            CHECK(!packaged.Load(packed, path, 32.0f));
            failCreate = false;
            nextError = GL_OUT_OF_MEMORY;
            CHECK(!packaged.Load(packed, path, 32.0f));
            CHECK(packaged.IsLoaded() && packaged.GetPixelHeight() == 48.0f);
            CHECK(liveTextures == beforeFailure);
            CHECK(unpackAlignment == 4);

            packed.Close();
            original.Close();
            CHECK(packaged.IsLoaded() && directory.IsLoaded());
        }
        CHECK(liveTextures.empty());
        CHECK(bound == 0);
        return 0;
    }
}

#ifdef _WIN32
int wmain(int _argc, wchar_t** _argv)
#else
int main(int _argc, char** _argv)
#endif
{
    if (_argc != 3)
        return 1;
    return RunTests(std::filesystem::path(_argv[1]), std::filesystem::path(_argv[2]));
}
