#include <iostream>
#include <glad/gl.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

constexpr int screenWidth = 1280;
constexpr int screenHeight = 720;

int main()
{
    if (glfwInit() != GLFW_TRUE)
    {
        std::cerr << "Failed to initialize GLFW.\n";
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(
        screenWidth,
        screenHeight,
        "LightWeightGameFramework",
        nullptr,
        nullptr
    );

    if (window == nullptr)
    {
        std::cerr << "Failed to create GLFW window.\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);

    if (gladLoadGL(reinterpret_cast<GLADloadfunc>(glfwGetProcAddress)) == 0)
    {
        std::cerr << "Failed to initialize GLAD.\n";

        glfwDestroyWindow(window);
        glfwTerminate();

        return -1;
    }

    std::cout
        << "OpenGL Version: "
        << reinterpret_cast<const char*>(glGetString(GL_VERSION))
        << '\n';

    glViewport(0, 0, screenWidth, screenHeight);

    // 왼쪽 아래가 (0, 0), 오른쪽 위가 (1, 1)인 기본 사각형
    float vertices[] =
    {
        0.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 0.0f,
        1.0f, 1.0f, 0.0f,
        0.0f, 1.0f, 0.0f
    };

    unsigned int indices[] =
    {
        0, 1, 2,
        2, 3, 0
    };

    unsigned int vao = 0;
    unsigned int vbo = 0;
    unsigned int ebo = 0;

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        sizeof(vertices),
        vertices,
        GL_STATIC_DRAW
    );

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        sizeof(indices),
        indices,
        GL_STATIC_DRAW
    );

    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        3 * sizeof(float),
        nullptr
    );

    glEnableVertexAttribArray(0);

    const char* vertexShaderSource =
        "#version 330 core\n"
        "layout (location = 0) in vec3 position;\n"
        "uniform vec2 uPosition;\n"
        "uniform vec2 uSize;\n"
        "void main()\n"
        "{\n"
        "    vec2 transformedPosition = position.xy * uSize + uPosition;\n"
        "    gl_Position = vec4(transformedPosition, position.z, 1.0);\n"
        "}\n";

    const char* fragmentShaderSource =
        "#version 330 core\n"
        "out vec4 fragmentColor;\n"
        "void main()\n"
        "{\n"
        "    fragmentColor = vec4(1.0, 0.5, 0.2, 1.0);\n"
        "}\n";

    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);

    glShaderSource(
        vertexShader,
        1,
        &vertexShaderSource,
        nullptr
    );

    glCompileShader(vertexShader);

    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

    glShaderSource(
        fragmentShader,
        1,
        &fragmentShaderSource,
        nullptr
    );

    glCompileShader(fragmentShader);

    unsigned int shaderProgram = glCreateProgram();

    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);

    glLinkProgram(shaderProgram);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    int positionLocation = glGetUniformLocation(shaderProgram, "uPosition");
    int sizeLocation = glGetUniformLocation(shaderProgram, "uSize");

    // ============================
    // 실제 게임에서 지정하고 싶은 값
    // ============================

    float x = 100.0f;
    float y = 100.0f;

    float width = 64.0f;
    float height = 64.0f;

    float speed = 200.0f;

    // Pixel 좌표 -> OpenGL NDC 좌표 변환
    float ndcWidth = (width / static_cast<float>(screenWidth)) * 2.0f;
    float ndcHeight = (height / static_cast<float>(screenHeight)) * 2.0f;

    double previousTime = glfwGetTime();

    glClearColor(0.1f, 0.15f, 0.2f, 1.0f);

    while (glfwWindowShouldClose(window) == GLFW_FALSE)
    {
        double currentTime = glfwGetTime();
        float deltaTime = static_cast<float>(currentTime - previousTime);
        previousTime = currentTime;

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        {
            y += speed * deltaTime;
        }

        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        {
            y -= speed * deltaTime;
        }

        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        {
            x -= speed * deltaTime;
        }

        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        {
            x += speed * deltaTime;
        }

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }

        float ndcX = (x / static_cast<float>(screenWidth)) * 2.0f - 1.0f;
        float ndcY = (y / static_cast<float>(screenHeight)) * 2.0f - 1.0f;

        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(shaderProgram);
        glUniform2f(positionLocation, ndcX, ndcY);
        glUniform2f(sizeLocation, ndcWidth, ndcHeight);
        glBindVertexArray(vao);

        glDrawElements(
            GL_TRIANGLES,
            6,
            GL_UNSIGNED_INT,
            nullptr
        );

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &ebo);
    glDeleteProgram(shaderProgram);

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}