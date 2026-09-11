#include "Game.h"

#include <framework/Application.h>
#include <framework/Renderer2D.h>

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

    while (app.IsRunning())
    {
        app.BeginFrame();

		renderer.BeginFrame();
		game.Update(app);

		game.Render(renderer);
        app.EndFrame();
    }

    return 0;
}