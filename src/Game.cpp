#include "Game.h"

#include "framework/Application.h"
#include "framework/MouseButton.h"
#include "framework/Renderer2D.h"

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
}

bool Game::Contains(const TextureObject& _textureObject, float _x, float _y) const
{
    return
        _x >= _textureObject.x &&
        _x <= _textureObject.x + _textureObject.width &&
        _y >= _textureObject.y &&
        _y <= _textureObject.y + _textureObject.height;
}