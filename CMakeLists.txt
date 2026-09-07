cmake_minimum_required(VERSION 3.20)

project(LightWeightGameFramework LANGUAGES C CXX)

include(FetchContent)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.5.1
)

FetchContent_MakeAvailable(glfw)

add_library(glad STATIC
    third_party/glad/src/gl.c
)

target_include_directories(glad
    PUBLIC
        third_party/glad/include
)

add_executable(LightWeightGameFramework
    src/main.cpp
)

target_compile_features(LightWeightGameFramework PRIVATE cxx_std_20)

target_link_libraries(
    LightWeightGameFramework
    PRIVATE 
        glfw
        glad
)
