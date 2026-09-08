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

    if (gladLoadGL(
        reinterpret_cast<GLADloadfunc>(glfwGetProcAddress)
    ) == 0)
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
    glfwPollEvents();

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
    const int glfwKey = ConvertKey(key);

    return glfwGetKey(m_window, glfwKey) == GLFW_PRESS;
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
