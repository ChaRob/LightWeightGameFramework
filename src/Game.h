#pragma once

#include <array>

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
    struct TextureObject
    {
        Texture2D texture;

        float x = 0.0f;
        float y = 0.0f;
        float width = 256.0f;
        float height = 256.0f;
    };

    bool Contains(const TextureObject& _textureObject, float _x, float _y) const;

	std::array<TextureObject, 2> m_textures;

    TextureObject* m_draggingTexture = nullptr;

    float m_dragOffsetX = 0.0f;
    float m_dragOffsetY = 0.0f;
};