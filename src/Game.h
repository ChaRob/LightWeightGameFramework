#pragma once

#include "framework/Texture2D.h"

class Application;
class Renderer2D;

class Game
{
public:
    bool Initialize();

    void Update(Application& _app);
    void Render(Renderer2D& _renderer);

private:
    Texture2D m_texture;

    float m_textureX = 400.0f;
    float m_textureY = 200.0f;
    float m_textureWidth = 256.0f;
    float m_textureHeight = 256.0f;

    bool m_isDragging = false;

    float m_dragOffsetX = 0.0f;
    float m_dragOffsetY = 0.0f;
};