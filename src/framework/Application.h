#pragma once

#include "Key.h"
#include "MouseButton.h"

struct GLFWwindow;

class Application
{
public:
    Application() = default;
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    bool Initialize();

    bool IsRunning() const;

    void BeginFrame();
    void EndFrame();

    float GetDeltaTime() const;

    bool IsKeyDown(Key key) const;

    void RequestClose();

    int GetWidth() const;
    int GetHeight() const;

    bool IsMouseButtonDown(MouseButton _button) const;
    bool IsMouseButtonPressed(MouseButton _button) const;
    bool IsMouseButtonReleased(MouseButton _button) const;

    double GetMouseX() const;
    double GetMouseY() const;

private:
    GLFWwindow* m_window = nullptr;

    double m_previousTime = 0.0;
    float m_deltaTime = 0.0f;

    int m_width = 1280;
    int m_height = 720;

    bool m_mouseButtonCurrent[3] = {};
    bool m_mouseButtonPrevious[3] = {};
};