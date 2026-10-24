#include <framework/Application.h>

#include <cstddef>
#include <iostream>
#include <iterator>
#include <glad/gl.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace
{
    // Ordered to match the public Key enum. Only valid GLFW keys are polled.
    constexpr int kGlfwKeyCodes[] =
    {
        // Printable keys
        GLFW_KEY_SPACE,
        GLFW_KEY_APOSTROPHE,
        GLFW_KEY_COMMA,
        GLFW_KEY_MINUS,
        GLFW_KEY_PERIOD,
        GLFW_KEY_SLASH,
        GLFW_KEY_0,
        GLFW_KEY_1,
        GLFW_KEY_2,
        GLFW_KEY_3,
        GLFW_KEY_4,
        GLFW_KEY_5,
        GLFW_KEY_6,
        GLFW_KEY_7,
        GLFW_KEY_8,
        GLFW_KEY_9,
        GLFW_KEY_SEMICOLON,
        GLFW_KEY_EQUAL,
        GLFW_KEY_A,
        GLFW_KEY_B,
        GLFW_KEY_C,
        GLFW_KEY_D,
        GLFW_KEY_E,
        GLFW_KEY_F,
        GLFW_KEY_G,
        GLFW_KEY_H,
        GLFW_KEY_I,
        GLFW_KEY_J,
        GLFW_KEY_K,
        GLFW_KEY_L,
        GLFW_KEY_M,
        GLFW_KEY_N,
        GLFW_KEY_O,
        GLFW_KEY_P,
        GLFW_KEY_Q,
        GLFW_KEY_R,
        GLFW_KEY_S,
        GLFW_KEY_T,
        GLFW_KEY_U,
        GLFW_KEY_V,
        GLFW_KEY_W,
        GLFW_KEY_X,
        GLFW_KEY_Y,
        GLFW_KEY_Z,
        GLFW_KEY_LEFT_BRACKET,
        GLFW_KEY_BACKSLASH,
        GLFW_KEY_RIGHT_BRACKET,
        GLFW_KEY_GRAVE_ACCENT,
        GLFW_KEY_WORLD_1,
        GLFW_KEY_WORLD_2,

        // Editing and navigation
        GLFW_KEY_ESCAPE,
        GLFW_KEY_ENTER,
        GLFW_KEY_TAB,
        GLFW_KEY_BACKSPACE,
        GLFW_KEY_INSERT,
        GLFW_KEY_DELETE,
        GLFW_KEY_RIGHT,
        GLFW_KEY_LEFT,
        GLFW_KEY_DOWN,
        GLFW_KEY_UP,
        GLFW_KEY_PAGE_UP,
        GLFW_KEY_PAGE_DOWN,
        GLFW_KEY_HOME,
        GLFW_KEY_END,

        // Lock and system keys
        GLFW_KEY_CAPS_LOCK,
        GLFW_KEY_SCROLL_LOCK,
        GLFW_KEY_NUM_LOCK,
        GLFW_KEY_PRINT_SCREEN,
        GLFW_KEY_PAUSE,

        // Function keys
        GLFW_KEY_F1,
        GLFW_KEY_F2,
        GLFW_KEY_F3,
        GLFW_KEY_F4,
        GLFW_KEY_F5,
        GLFW_KEY_F6,
        GLFW_KEY_F7,
        GLFW_KEY_F8,
        GLFW_KEY_F9,
        GLFW_KEY_F10,
        GLFW_KEY_F11,
        GLFW_KEY_F12,
        GLFW_KEY_F13,
        GLFW_KEY_F14,
        GLFW_KEY_F15,
        GLFW_KEY_F16,
        GLFW_KEY_F17,
        GLFW_KEY_F18,
        GLFW_KEY_F19,
        GLFW_KEY_F20,
        GLFW_KEY_F21,
        GLFW_KEY_F22,
        GLFW_KEY_F23,
        GLFW_KEY_F24,
        GLFW_KEY_F25,

        // Numeric keypad
        GLFW_KEY_KP_0,
        GLFW_KEY_KP_1,
        GLFW_KEY_KP_2,
        GLFW_KEY_KP_3,
        GLFW_KEY_KP_4,
        GLFW_KEY_KP_5,
        GLFW_KEY_KP_6,
        GLFW_KEY_KP_7,
        GLFW_KEY_KP_8,
        GLFW_KEY_KP_9,
        GLFW_KEY_KP_DECIMAL,
        GLFW_KEY_KP_DIVIDE,
        GLFW_KEY_KP_MULTIPLY,
        GLFW_KEY_KP_SUBTRACT,
        GLFW_KEY_KP_ADD,
        GLFW_KEY_KP_ENTER,
        GLFW_KEY_KP_EQUAL,

        // Modifiers and menu
        GLFW_KEY_LEFT_SHIFT,
        GLFW_KEY_LEFT_CONTROL,
        GLFW_KEY_LEFT_ALT,
        GLFW_KEY_LEFT_SUPER,
        GLFW_KEY_RIGHT_SHIFT,
        GLFW_KEY_RIGHT_CONTROL,
        GLFW_KEY_RIGHT_ALT,
        GLFW_KEY_RIGHT_SUPER,
        GLFW_KEY_MENU,
    };

    static_assert(std::size(kGlfwKeyCodes) == static_cast<std::size_t>(Key::Count),
        "Every Key value must have a GLFW mapping.");

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

bool Application::Initialize(const char* _windowTitle)
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
        (_windowTitle != nullptr && _windowTitle[0] != '\0') ? _windowTitle : "LightWeightGameFramework",
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
		m_keyCurrent[i] = glfwGetKey(m_window, kGlfwKeyCodes[i]) == GLFW_PRESS;
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

bool Application::IsKeyDown(Key _key) const
{
	const int index = GetKeyIndex(_key);
	return index >= 0 && index < static_cast<int>(Key::Count) && m_keyCurrent[index];
}

bool Application::IsKeyPressed(Key _key) const
{
	const int index = GetKeyIndex(_key);
	return index >= 0 && index < static_cast<int>(Key::Count)
		&& m_keyCurrent[index] && m_keyPrevious[index] == false;
}

bool Application::IsKeyReleased(Key _key) const
{
	const int index = GetKeyIndex(_key);
	return index >= 0 && index < static_cast<int>(Key::Count)
		&& m_keyCurrent[index] == false && m_keyPrevious[index];
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