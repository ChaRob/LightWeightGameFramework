#include "Game.h"

#include "framework/Application.h"
#include "framework/MouseButton.h"
#include "framework/Renderer2D.h"

bool Game::Initialize()
{
    return m_texture.Load("assets/images.jpg");
}

void Game::Update(Application& _app)
{
    if (_app.IsMouseButtonPressed(MouseButton::Left))
    {
        const float mouseX = static_cast<float>(_app.GetMouseX());
        const float mouseY = static_cast<float>(_app.GetMouseY());

        const bool isInsideTexture =
            mouseX >= m_textureX &&
            mouseX <= m_textureX + m_textureWidth &&
            mouseY >= m_textureY &&
            mouseY <= m_textureY + m_textureHeight;

        if (isInsideTexture)
        {
            m_isDragging = true;

            m_dragOffsetX = mouseX - m_textureX;
            m_dragOffsetY = mouseY - m_textureY;
        }
    }

    if (m_isDragging)
    {
        if (_app.IsMouseButtonDown(MouseButton::Left))
        {
            const float mouseX = static_cast<float>(_app.GetMouseX());
            const float mouseY = static_cast<float>(_app.GetMouseY());

            m_textureX = mouseX - m_dragOffsetX;
            m_textureY = mouseY - m_dragOffsetY;
        }

        if (_app.IsMouseButtonReleased(MouseButton::Left))
        {
            m_isDragging = false;
        }
    }
}

void Game::Render(Renderer2D& _renderer)
{
    _renderer.DrawTexture(
        m_texture,
        m_textureX,
        m_textureY,
        m_textureWidth,
        m_textureHeight
    );
}