#include "Game.h"

#include <cstdio>

#include <framework/Application.h>
#include <framework/MouseButton.h>
#include <framework/Renderer2D.h>
#include <framework/Key.h>

bool Game::Initialize()
{
    if (!m_textures[0].texture.Load("assets/images.jpg"))
    {
        return false;
    }

    if (!m_textures[1].texture.Load("assets/images2.png"))
    {
        return false;
    }

    if (!m_font.Load("assets/fonts/AnonymousPro-Regular.ttf", 48.0f))
    {
        return false;
    }

    m_textures[0].x = 200.0f;
    m_textures[0].y = 200.0f;

    m_textures[1].x = 700.0f;
    m_textures[1].y = 350.0f;

    return true;
}

void Game::Update(Application& _app)
{
    const float mouseX = static_cast<float>(_app.GetMouseX());
    const float mouseY = static_cast<float>(_app.GetMouseY());

    if (_app.IsMouseButtonPressed(MouseButton::Left))
    {
        for (auto iter = m_textures.rbegin(); iter != m_textures.rend(); ++iter)
        {
            if (Contains(*iter, mouseX, mouseY) == false)
            {
                continue;
            }

            m_draggingTexture = &(*iter);

            m_dragOffsetX = mouseX - m_draggingTexture->x;
            m_dragOffsetY = mouseY - m_draggingTexture->y;

            break;
        }
    }

    if (m_draggingTexture != nullptr)
    {
        if (_app.IsMouseButtonDown(MouseButton::Left))
        {
            m_draggingTexture->x = mouseX - m_dragOffsetX;
            m_draggingTexture->y = mouseY - m_dragOffsetY;
        }

        if (_app.IsMouseButtonReleased(MouseButton::Left))
        {
            m_draggingTexture = nullptr;
        }
    }

    const float moveSpeed = 300.0f;
    const float deltaTime = _app.GetDeltaTime();
    m_elapsedSeconds += deltaTime;

	// Texture movement
    if (_app.IsKeyDown(Key::W))
    {
        m_textures[0].y += moveSpeed * deltaTime;
    }

    if (_app.IsKeyDown(Key::S))
    {
        m_textures[0].y -= moveSpeed * deltaTime;
    }

    if (_app.IsKeyDown(Key::A))
    {
        m_textures[0].x -= moveSpeed * deltaTime;
    }

    if (_app.IsKeyDown(Key::D))
    {
        m_textures[0].x += moveSpeed * deltaTime;
    }

	// Rectangle movement
    if (_app.IsKeyDown(Key::W))
    {
        m_y += m_speed * deltaTime;
    }

    if (_app.IsKeyDown(Key::S))
    {
        m_y -= m_speed * deltaTime;
    }

    if (_app.IsKeyDown(Key::A))
    {
        m_x -= m_speed * deltaTime;
    }

    if (_app.IsKeyDown(Key::D))
    {
        m_x += m_speed * deltaTime;
    }

	// Exit the application when the Escape key is pressed
    if (_app.IsKeyPressed(Key::ESCAPE))
    {
        _app.RequestClose();
    }
}

void Game::Render(Renderer2D& _renderer)
{
    for (const TextureObject& textureObject : m_textures)
    {
        _renderer.DrawTexture(
            textureObject.texture,
            textureObject.x,
            textureObject.y,
            textureObject.width,
            textureObject.height
        );
    }

    const Color color =
    {
        0.2f,
        1.0f,
        0.3f,
        1.0f
    };

    _renderer.DrawRect(
        m_x,
        m_y,
        m_width,
        m_height,
        color
    );

    // Text shares the same bottom-left-origin coordinate system.
    const Color textColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    const Color accentColor = { 1.0f, 0.85f, 0.3f, 1.0f };

    _renderer.DrawText(m_font, "STAGE 12", 32.0f, 675.0f, 32.0f, textColor);

    char timeText[64] = {};
    std::snprintf(timeText, sizeof(timeText), "TIME %.1fs", m_elapsedSeconds);
    _renderer.DrawText(m_font, timeText, 32.0f, 631.0f, 32.0f, textColor);

    char scoreText[64] = {};
    std::snprintf(scoreText, sizeof(scoreText), "SCORE %d",
                  static_cast<int>(m_elapsedSeconds * 100.0f));
    _renderer.DrawText(m_font, scoreText, 32.0f, 587.0f, 32.0f, textColor);

    const float titleSize = 48.0f;
    const float titleWidth = _renderer.MeasureTextWidth(m_font, "GAME OVER", titleSize);
    _renderer.DrawText(
        m_font, "GAME OVER",
        (1280.0f - titleWidth) * 0.5f, 82.0f,
        titleSize, accentColor
    );
}

bool Game::Contains(const TextureObject& _textureObject, float _x, float _y) const
{
    return
        _x >= _textureObject.x &&
        _x <= _textureObject.x + _textureObject.width &&
        _y >= _textureObject.y &&
        _y <= _textureObject.y + _textureObject.height;
}