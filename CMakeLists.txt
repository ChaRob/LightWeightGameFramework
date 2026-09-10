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

FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG 2c980bb59875b0d32144a71867fbdebb2f77cd20
)

FetchContent_MakeAvailable(stb)

add_library(glad STATIC
    third_party/glad/src/gl.c
)

target_include_directories(glad
    PUBLIC
        third_party/glad/include
)

add_executable(LightWeightGameFramework
    src/main.cpp
    src/Game.cpp
    src/framework/Application.cpp
    src/framework/Renderer2D.cpp
    src/framework/Texture2D.cpp
)

add_custom_command(
    TARGET LightWeightGameFramework
    POST_BUILD

    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:LightWeightGameFramework>/assets
)

target_compile_features(
    LightWeightGameFramework
    PRIVATE
        cxx_std_20
)

target_include_directories(
    LightWeightGameFramework
    PRIVATE
        ${stb_SOURCE_DIR}
)

target_link_libraries(
    LightWeightGameFramework
    PRIVATE 
        glfw
        glad
)
