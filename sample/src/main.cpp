#include "Game.h"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <framework/Application.h>
#include <framework/AssetReader.h>
#include <framework/AudioSystem.h>
#include <framework/Key.h>
#include <framework/Renderer2D.h>
#include <lwgf/AssetBuildConfig.h>

namespace
{
    namespace fs = std::filesystem;

    fs::path ExecutableDirectory(const fs::path& _argv0)
    {
#ifdef _WIN32
        (void)_argv0;
        std::vector<wchar_t> buffer(512);
        while (true)
        {
            const auto length = GetModuleFileNameW(nullptr, buffer.data(),
                                                   static_cast<DWORD>(buffer.size()));
            if (length == 0)
                throw std::runtime_error("Cannot determine executable path");
            if (length < buffer.size())
                return fs::path(std::wstring(buffer.data(), length)).parent_path();
            if (buffer.size() >= 32768)
                throw std::runtime_error("Executable path exceeds supported length");
            buffer.resize(buffer.size() * 2);
        }
#else
        return fs::absolute(_argv0).parent_path();
#endif
    }

    std::string Utf8(const fs::path& _path)
    {
        const auto utf8 = _path.u8string();
        return std::string(utf8.begin(), utf8.end());
    }
}

template<typename Char>
int RunSample(int _argc, Char** _argv)
{
    bool packaged = lwgf::build::UsePackage;
    fs::path root;
    if (_argc == 1)
    {
        if (packaged)
            root = ExecutableDirectory(fs::path(_argv[0])) / lwgf::build::PackageDirectory;
        else
            root = fs::path(lwgf::build::DirectoryRoot);
    }
    else if (_argc == 3 &&
             (fs::path(_argv[1]) == "--directory" || fs::path(_argv[1]) == "--package"))
    {
        packaged = fs::path(_argv[1]) == "--package";
        root = fs::absolute(fs::path(_argv[2]));
    }
    else
    {
        std::cerr << "Usage: SampleGame [--directory <root> | --package <root>]\n";
        return 2;
    }

    AssetReader assets;
    if (!(packaged ? assets.OpenPackage(root) : assets.OpenDirectory(root)))
        return -1;
    std::cout << "[Sample] Asset mode: " << (packaged ? "Package" : "Directory")
              << " (" << Utf8(root) << ")\n";

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
    if (audio.Initialize(assets))
    {
        clickEffect = audio.LoadEffect("audio/click.wav");
    }
    else
    {
        std::cerr << "[Sample] Audio unavailable; continuing without sound.\n";
    }

    std::cout << "[Sample] Audio keys: SPACE=effect, M=play/loop music, "
                 "P=pause, R=resume, N=stop.\n";

    Game game;
    if (!game.Initialize(assets))
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
            audio.PlayMusic("audio/music.wav");
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

#ifdef _WIN32
int wmain(int _argc, wchar_t** _argv)
#else
int main(int _argc, char** _argv)
#endif
{
    try
    {
        return RunSample(_argc, _argv);
    }
    catch (const std::exception& _exception)
    {
        std::cerr << "[Sample] " << _exception.what() << '\n';
        return -1;
    }
}
