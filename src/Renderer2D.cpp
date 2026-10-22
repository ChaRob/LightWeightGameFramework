#include <framework/Renderer2D.h>
#include <framework/Texture2D.h>
#include <framework/Font.h>

#include <cmath>

#include <iostream>
#include <glad/gl.h>

namespace
{
    bool CompileShader(unsigned int shader, const char* shaderName)
    {
        glCompileShader(shader);

        int success = 0;

        glGetShaderiv(shader, GL_COMPILE_STATUS, &success
        );

        if (success == GL_TRUE)
        {
            return true;
        }

        char errorLog[1024] = {};

        glGetShaderInfoLog(
            shader,
            sizeof(errorLog),
            nullptr,
            errorLog
        );

        std::cerr
            << "Failed to compile "
            << shaderName
            << " shader.\n"
            << errorLog
            << '\n';

        return false;
    }

    bool LinkProgram(unsigned int program)
    {
        glLinkProgram(program);

        int success = 0;

        glGetProgramiv(program, GL_LINK_STATUS, &success
        );

        if (success == GL_TRUE)
        {
            return true;
        }

        char errorLog[1024] = {};

        glGetProgramInfoLog(
            program,
            sizeof(errorLog),
            nullptr,
            errorLog
        );

        std::cerr
            << "Failed to link shader program.\n"
            << errorLog
            << '\n';

        return false;
    }
}

Renderer2D::~Renderer2D()
{
    if (m_vao != 0)
    {
        glDeleteVertexArrays(1, &m_vao);
    }

    if (m_vbo != 0)
    {
        glDeleteBuffers(1, &m_vbo);
    }

    if (m_ebo != 0)
    {
        glDeleteBuffers(1, &m_ebo);
    }

    if (m_shaderProgram != 0)
    {
        glDeleteProgram(m_shaderProgram);
    }
}

bool Renderer2D::Initialize(int screenWidth, int screenHeight)
{
    m_screenWidth = screenWidth;
    m_screenHeight = screenHeight;

    float vertices[] =
    {
        // Position             // UV
        0.0f, 0.0f, 0.0f,      0.0f, 0.0f,
        1.0f, 0.0f, 0.0f,      1.0f, 0.0f,
        1.0f, 1.0f, 0.0f,      1.0f, 1.0f,
        0.0f, 1.0f, 0.0f,      0.0f, 1.0f
    };

    unsigned int indices[] =
    {
        0, 1, 2,
        2, 3, 0
    };

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glGenBuffers(1, &m_ebo);

    glBindVertexArray(m_vao);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

    glBufferData(
        GL_ARRAY_BUFFER,
        sizeof(vertices),
        vertices,
        GL_STATIC_DRAW
    );

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);

    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        sizeof(indices),
        indices,
        GL_STATIC_DRAW
    );

    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        5 * sizeof(float),
        nullptr
    );

    glEnableVertexAttribArray(0);

    glVertexAttribPointer(
        1,
        2,
        GL_FLOAT,
        GL_FALSE,
        5 * sizeof(float),
        reinterpret_cast<void*>(3 * sizeof(float))
    );

    glEnableVertexAttribArray(1);

    const char* vertexShaderSource =
        "#version 330 core\n"
        "layout (location = 0) in vec3 position;\n"
        "layout (location = 1) in vec2 texCoord;\n"
        "uniform vec2 uPosition;\n"
        "uniform vec2 uSize;\n"
        "uniform vec2 uUVMin;\n"
        "uniform vec2 uUVMax;\n"
        "out vec2 vTexCoord;\n"
        "void main()\n"
        "{\n"
        "    vec2 transformedPosition = position.xy * uSize + uPosition;\n"
        "    gl_Position = vec4(transformedPosition, position.z, 1.0);\n"
        "    vTexCoord = mix(uUVMin, uUVMax, texCoord);\n"
        "}\n";

    const char* fragmentShaderSource =
        "#version 330 core\n"
        "in vec2 vTexCoord;\n"
        "out vec4 fragmentColor;\n"
        "uniform vec4 uColor;\n"
        "uniform sampler2D uTexture;\n"
        "uniform int uUseTexture;\n"
        "void main()\n"
        "{\n"
        "    if (uUseTexture == 1)\n"
        "    {\n"
        "        fragmentColor = texture(uTexture, vTexCoord) * uColor;\n"
        "    }\n"
        "    else if (uUseTexture == 2)\n"
        "    {\n"
        "        float coverage = texture(uTexture, vTexCoord).r;\n"
        "        fragmentColor = vec4(uColor.rgb, uColor.a * coverage);\n"
        "    }\n"
        "    else\n"
        "    {\n"
        "        fragmentColor = uColor;\n"
        "    }\n"
        "}\n";

    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);

    glShaderSource(
        vertexShader,
        1,
        &vertexShaderSource,
        nullptr
    );

    if (!CompileShader(vertexShader, "vertex"))
    {
        glDeleteShader(vertexShader);
        return false;
    }

    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

    glShaderSource(
        fragmentShader,
        1,
        &fragmentShaderSource,
        nullptr
    );

    if (!CompileShader(fragmentShader, "fragment"))
    {
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        return false;
    }

    m_shaderProgram = glCreateProgram();

    glAttachShader(m_shaderProgram, vertexShader);
    glAttachShader(m_shaderProgram, fragmentShader);

    const bool linkSuccess = LinkProgram(m_shaderProgram);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    if (!linkSuccess)
    {
        return false;
    }

    m_positionLocation = glGetUniformLocation(m_shaderProgram, "uPosition");
    m_sizeLocation = glGetUniformLocation(m_shaderProgram, "uSize");
    m_colorLocation = glGetUniformLocation(m_shaderProgram, "uColor");
    m_textureLocation = glGetUniformLocation(m_shaderProgram, "uTexture");
    m_useTextureLocation = glGetUniformLocation(m_shaderProgram, "uUseTexture");
    m_uvMinLocation = glGetUniformLocation(m_shaderProgram, "uUVMin");
    m_uvMaxLocation = glGetUniformLocation(m_shaderProgram, "uUVMax");

    if (m_positionLocation < 0 ||
        m_sizeLocation < 0 ||
        m_colorLocation < 0 ||
        m_textureLocation < 0 ||
        m_useTextureLocation < 0 ||
        m_uvMinLocation < 0 ||
        m_uvMaxLocation < 0)
    {
        std::cerr << "Failed to get shader uniform location.\n";
        return false;
    }

    glUseProgram(m_shaderProgram);

    glUniform1i(
        m_textureLocation,
        0
    );

    glUniform2f(m_uvMinLocation, 0.0f, 0.0f);
    glUniform2f(m_uvMaxLocation, 1.0f, 1.0f);

    glEnable(GL_BLEND);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glClearColor(
        0.1f,
        0.15f,
        0.2f,
        1.0f
    );

    return true;
}

void Renderer2D::BeginFrame()
{
    glClear(GL_COLOR_BUFFER_BIT);
}

void Renderer2D::DrawRect(
    float x,
    float y,
    float width,
    float height,
    const Color& color
)
{
    const float ndcX = (x / static_cast<float>(m_screenWidth)) * 2.0f - 1.0f;
    const float ndcY = (y / static_cast<float>(m_screenHeight)) * 2.0f - 1.0f;

    const float ndcWidth = (width / static_cast<float>(m_screenWidth)) * 2.0f;
    const float ndcHeight = (height / static_cast<float>(m_screenHeight)) * 2.0f;

    glUseProgram(m_shaderProgram);

    glUniform1i(
        m_useTextureLocation,
        0
    );

    glUniform2f(
        m_positionLocation,
        ndcX,
        ndcY
    );

    glUniform2f(
        m_sizeLocation,
        ndcWidth,
        ndcHeight
    );

    glUniform4f(
        m_colorLocation,
        color.r,
        color.g,
        color.b,
        color.a
    );

    glBindVertexArray(m_vao);

    glDrawElements(
        GL_TRIANGLES,
        6,
        GL_UNSIGNED_INT,
        nullptr
    );
}

void Renderer2D::DrawTexture(
    const Texture2D& texture,
    float x,
    float y,
    float width,
    float height
)
{
    const float ndcX = (x / static_cast<float>(m_screenWidth)) * 2.0f - 1.0f;
    const float ndcY = (y / static_cast<float>(m_screenHeight)) * 2.0f - 1.0f;

    const float ndcWidth = (width / static_cast<float>(m_screenWidth)) * 2.0f;
    const float ndcHeight = (height / static_cast<float>(m_screenHeight)) * 2.0f;

    glUseProgram(m_shaderProgram);

    glUniform1i(
        m_useTextureLocation,
        1
    );

    glUniform2f( m_positionLocation, ndcX, ndcY );
    glUniform2f( m_sizeLocation, ndcWidth, ndcHeight );
    glUniform4f( m_colorLocation, 1.0f, 1.0f, 1.0f, 1.0f );

    glUniform2f(m_uvMinLocation, 0.0f, 0.0f);
    glUniform2f(m_uvMaxLocation, 1.0f, 1.0f);

    texture.Bind();

    glBindVertexArray(m_vao);

    glDrawElements(
        GL_TRIANGLES,
        6,
        GL_UNSIGNED_INT,
        nullptr
    );
}

void Renderer2D::DrawText(
    const Font& _font,
    std::string_view _text,
    float _x,
    float _baselineY,
    float _pixelHeight,
    const Color& _color
)
{
    if (!_font.IsLoaded() || _text.empty() ||
        !std::isfinite(_pixelHeight) || _pixelHeight <= 0.0f ||
        m_screenWidth <= 0 || m_screenHeight <= 0)
    {
        return;
    }

    const float scale = _pixelHeight / _font.m_pixelHeight;
    float penX = _x;

    glUseProgram(m_shaderProgram);
    glUniform1i(m_useTextureLocation, 2);
    glUniform4f(m_colorLocation, _color.r, _color.g, _color.b, _color.a);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, _font.m_textureId);
    glBindVertexArray(m_vao);

    for (const char character : _text)
    {
        const unsigned char value = static_cast<unsigned char>(character);
        const unsigned int index =
            (value >= Font::FirstCharacter &&
             value < Font::FirstCharacter + Font::CharacterCount)
            ? value - Font::FirstCharacter
            : static_cast<unsigned int>('?' - Font::FirstCharacter);

        const Font::Glyph& glyph = _font.m_glyphs[index];

        if (glyph.width > 0.0f && glyph.height > 0.0f)
        {
            const float x = penX + glyph.xOffset * scale;
            const float y = _baselineY + glyph.yOffset * scale;
            const float width = glyph.width * scale;
            const float height = glyph.height * scale;

            const float ndcX = (x / static_cast<float>(m_screenWidth)) * 2.0f - 1.0f;
            const float ndcY = (y / static_cast<float>(m_screenHeight)) * 2.0f - 1.0f;
            const float ndcWidth = (width / static_cast<float>(m_screenWidth)) * 2.0f;
            const float ndcHeight = (height / static_cast<float>(m_screenHeight)) * 2.0f;

            glUniform2f(m_positionLocation, ndcX, ndcY);
            glUniform2f(m_sizeLocation, ndcWidth, ndcHeight);
            glUniform2f(m_uvMinLocation, glyph.u0, glyph.v0);
            glUniform2f(m_uvMaxLocation, glyph.u1, glyph.v1);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
        }

        penX += glyph.advance * scale;
    }
}

float Renderer2D::MeasureTextWidth(
    const Font& _font,
    std::string_view _text,
    float _pixelHeight
) const
{
    if (!_font.IsLoaded() || !std::isfinite(_pixelHeight) || _pixelHeight <= 0.0f)
    {
        return 0.0f;
    }

    float advance = 0.0f;

    for (const char character : _text)
    {
        const unsigned char value = static_cast<unsigned char>(character);
        const unsigned int index =
            (value >= Font::FirstCharacter &&
             value < Font::FirstCharacter + Font::CharacterCount)
            ? value - Font::FirstCharacter
            : static_cast<unsigned int>('?' - Font::FirstCharacter);

        advance += _font.m_glyphs[index].advance;
    }

    return advance * (_pixelHeight / _font.m_pixelHeight);
}
