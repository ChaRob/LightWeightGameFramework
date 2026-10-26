#include "Game.h"

#include <iostream>

#include <framework/Application.h>
#include <framework/AudioSystem.h>
#include <framework/Key.h>
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

    // Audio failure is not fatal for this sample.
    AudioSystem audio;
    AudioSystem::EffectId clickEffect = AudioSystem::InvalidEffect;
    if (audio.Initialize())
    {
        clickEffect = audio.LoadEffect("assets/audio/click.wav");
    }
    else
    {
        std::cerr << "[Sample] Audio unavailable; continuing without sound.\n";
    }

    std::cout << "[Sample] Audio keys: SPACE=effect, M=play/loop music, "
                 "P=pause, R=resume, N=stop.\n";

    Game game;
    if (!game.Initialize())
    {
        return -1;
    }

    while (app.IsRunning())
    {
        app.BeginFrame();
        audio.Update();

        if (app.IsKeyPressed(Key::SPACE))
        {
            // LimitReached is expected during a burst, not a fatal error.
            const auto result = audio.PlayEffect(clickEffect);
            if (result == AudioSystem::PlayResult::BackendError)
            {
                std::cerr << "[Sample] Failed to play effect.\n";
            }
        }

        if (app.IsKeyPressed(Key::M))
        {
            audio.PlayMusic("assets/audio/music.wav");
        }
        if (app.IsKeyPressed(Key::P))
        {
            audio.PauseMusic();
        }
        if (app.IsKeyPressed(Key::R))
        {
            audio.ResumeMusic();
        }
        if (app.IsKeyPressed(Key::N))
        {
            audio.StopMusic();
        }

        renderer.BeginFrame();
        game.Update(app);
        game.Render(renderer);
        app.EndFrame();
    }

    return 0;
}
