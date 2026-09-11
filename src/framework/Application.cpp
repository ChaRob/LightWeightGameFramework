#include "Application.h"

#include <iostream>
#include <glad/gl.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace
{
    int ConvertKey(Key key)
    {
        switch (key)
        {
        case Key::W:
            return GLFW_KEY_W;

        case Key::A:
            return GLFW_KEY_A;

        case Key::S:
            return GLFW_KEY_S;

        case Key::D:
            return GLFW_KEY_D;

        case Key::ESCAPE:
            return GLFW_KEY_ESCAPE;
        }

        return GLFW_KEY_UNKNOWN;
    }

    int ConvertMouseButton(MouseButton _button)
    {
        switch (_button)
        {
        case MouseButton::Left:
            return GLFW_MOUSE_BUTTON_LEFT;

        case MouseButton::Right:
            return GLFW_MOUSE_BUTTON_RIGHT;

        case MouseButton::Middle:
            return GLFW_MOUSE_BUTTON_MIDDLE;
        }

        return GLFW_MOUSE_BUTTON_LEFT;
    }

    int GetMouseButtonIndex(MouseButton _button)
    {
        switch (_button)
        {
        case MouseButton::Left:
            return 0;

        case MouseButton::Right:
            return 1;

        case MouseButton::Middle:
            return 2;
        }

        return 0;
    }

    int GetKeyIndex(Key _key)
    {
		return static_cast<int>(_key);
    }
}

Application::~Application()
{
    if (m_window != nullptr)
    {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }

    glfwTerminate();
}

bool Application::Initialize()
{
    if (glfwInit() != GLFW_TRUE)
    {
        std::cerr << "Failed to initialize GLFW.\n";
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(
        GLFW_OPENGL_PROFILE,
        GLFW_OPENGL_CORE_PROFILE
    );

    // 이번 단계에서는 Window 크기 고정
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    m_window = glfwCreateWindow(
        m_width,
        m_height,
        "LightWeightGameFramework",
        nullptr,
        nullptr
    );

    if (m_window == nullptr)
    {
        std::cerr << "Failed to create GLFW window.\n";
        return false;
    }

    glfwMakeContextCurrent(m_window);

    if (gladLoadGL(reinterpret_cast<GLADloadfunc>(glfwGetProcAddress)) == 0)
    {
        std::cerr << "Failed to initialize GLAD.\n";
        return false;
    }

    std::cout
        << "OpenGL Version: "
        << reinterpret_cast<const char*>(glGetString(GL_VERSION))
        << '\n';

    glViewport(
        0,
        0,
        m_width,
        m_height
    );

    m_previousTime = glfwGetTime();

    return true;
}

bool Application::IsRunning() const
{
    return glfwWindowShouldClose(m_window) == GLFW_FALSE;
}

void Application::BeginFrame()
{
	const int keyCount = static_cast<int>(Key::Count);

	for (int i = 0; i < keyCount; ++i)
	{
		m_keyPrevious[i] = m_keyCurrent[i];
	}

    for (int i = 0; i < 3; ++i)
    {
        m_mouseButtonPrevious[i] = m_mouseButtonCurrent[i];
    }

    glfwPollEvents();

	for (int i = 0; i < keyCount; ++i)
	{
		const Key key = static_cast<Key>(i);
		const int glfwKey = ConvertKey(key);
		m_keyCurrent[i] = glfwGetKey(m_window, glfwKey) == GLFW_PRESS;
	}

    m_mouseButtonCurrent[0] = glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    m_mouseButtonCurrent[1] = glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    m_mouseButtonCurrent[2] = glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS;

    const double currentTime = glfwGetTime();

    m_deltaTime = static_cast<float>(currentTime - m_previousTime);

    m_previousTime = currentTime;
}

void Application::EndFrame()
{
    glfwSwapBuffers(m_window);
}

float Application::GetDeltaTime() const
{
    return m_deltaTime;
}

bool Application::IsKeyDown(Key key) const
{
	const int index = GetKeyIndex(key);
	return m_keyCurrent[index];
}

bool Application::IsKeyPressed(Key key) const
{
	const int index = GetKeyIndex(key);
	return m_keyCurrent[index] && m_keyPrevious[index] == false;
}

bool Application::IsKeyReleased(Key key) const
{
	const int index = GetKeyIndex(key);
	return m_keyCurrent[index] == false && m_keyPrevious[index];
}

void Application::RequestClose()
{
    glfwSetWindowShouldClose(
        m_window,
        GLFW_TRUE
    );
}

int Application::GetWidth() const
{
    return m_width;
}

int Application::GetHeight() const
{
    return m_height;
}

bool Application::IsMouseButtonDown(MouseButton _button) const
{
    const int index = GetMouseButtonIndex(_button);

    return m_mouseButtonCurrent[index];
}

bool Application::IsMouseButtonPressed(MouseButton _button) const
{
    const int index = GetMouseButtonIndex(_button);

    return m_mouseButtonCurrent[index] && m_mouseButtonPrevious[index] == false;
}

bool Application::IsMouseButtonReleased(MouseButton _button) const
{
    const int index = GetMouseButtonIndex(_button);

    return m_mouseButtonCurrent[index] == false && m_mouseButtonPrevious[index];
}

double Application::GetMouseX() const
{
    double x = 0.0;
    double y = 0.0;

    glfwGetCursorPos(m_window, &x, &y);

    return x;
}

double Application::GetMouseY() const
{
    double x = 0.0;
    double y = 0.0;

    glfwGetCursorPos(m_window, &x, &y);

    return static_cast<double>(m_height) - y;
}