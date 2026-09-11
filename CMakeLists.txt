cmake_minimum_required(VERSION 3.20)

project(LightWeightGameFramework LANGUAGES C CXX)

include(FetchContent)


# =========================
# GLFW
# =========================

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


# =========================
# stb
# =========================

FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG 2c980bb59875b0d32144a71867fbdebb2f77cd20
)

FetchContent_MakeAvailable(stb)


# =========================
# GLAD
# =========================

add_library(lwgf_glad STATIC
    third_party/glad/src/gl.c
)

target_include_directories(
    lwgf_glad
    PUBLIC
        third_party/glad/include
)


# =========================
# LightWeightGameFramework
# =========================

add_library(
    LightWeightGameFramework
    STATIC

    src/Application.cpp
    src/Renderer2D.cpp
    src/Texture2D.cpp
)

add_library(
    LWGF::Framework
    ALIAS LightWeightGameFramework
)

target_compile_features(
    LightWeightGameFramework
    PUBLIC
        cxx_std_20
)

target_include_directories(
    LightWeightGameFramework

    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include

    PRIVATE
        ${stb_SOURCE_DIR}
)

target_link_libraries(
    LightWeightGameFramework

    PRIVATE
        glfw
        lwgf_glad
)


# =========================
# Sample Game
# =========================

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    add_subdirectory(sample)
endif()