#include <iostream>
#include <GLFW/glfw3.h>

int main()
{
    if (glfwInit() != GLFW_TRUE)
    {
        std::cerr << "Failed to initialize GLFW.\n";
        return -1;
    }

    GLFWwindow* window = glfwCreateWindow(
        1280,
        720,
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

    while (glfwWindowShouldClose(window) == GLFW_FALSE)
    {
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}