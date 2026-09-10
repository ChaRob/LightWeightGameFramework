#include "Game.h"

#include "framework/Application.h"
#include "framework/Renderer2D.h"

int main()
{
    Application app;

	if (!app.Initialize())
	{
		return -1;
	}

	Renderer2D renderer;

	if (!renderer.Initialize(app.GetWidth(), app.GetHeight()))
	{
		return -1;
	}

	Game game;

	if (!game.Initialize())
	{
		return -1;
	}

    // ============================
    // 실제 게임에서 지정하고 싶은 값
    // ============================

    float x = 100.0f;
    float y = 100.0f;

    float width = 64.0f;
    float height = 64.0f;

    float speed = 200.0f;

	const Color color = { 0.2f, 1.0f, 0.3f, 1.0f };
    const Color color2 = { 0.1f, 0.4f, 0.8f, 1.0f };
    const Color color3 = { 0.4f, 0.7f, 0.5f, 1.0f };

    while (app.IsRunning())
    {
        app.BeginFrame();

        const float deltaTime = app.GetDeltaTime();

        if (app.IsKeyDown(Key::W))
        {
            y += speed * deltaTime;
        }

        if (app.IsKeyDown(Key::S))
        {
            y -= speed * deltaTime;
        }

        if (app.IsKeyDown(Key::A))
        {
            x -= speed * deltaTime;
        }

        if (app.IsKeyDown(Key::D))
        {
            x += speed * deltaTime;
        }

        if (app.IsKeyDown(Key::ESCAPE))
        {
            app.RequestClose();
        }

		renderer.BeginFrame();
		game.Update(app);

		renderer.DrawRect(x, y, width, height, color);
        renderer.DrawRect(x + 50, y + 50, width, height, color2);
        renderer.DrawRect(x + 100, y + 100, width, height, color3);

		game.Render(renderer);
        app.EndFrame();
    }

    return 0;
}