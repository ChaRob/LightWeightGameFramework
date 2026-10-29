cmake_minimum_required(VERSION 3.20)

project(
    LightWeightGameFramework
    VERSION 0.0.7
    LANGUAGES C CXX)

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

# miniaudio 0.11.25: WAV/MP3/FLAC with no optional decoders/tools.
set(MINIAUDIO_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(MINIAUDIO_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(MINIAUDIO_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(MINIAUDIO_NO_EXTRA_NODES ON CACHE BOOL "" FORCE)
set(MINIAUDIO_NO_LIBVORBIS ON CACHE BOOL "" FORCE)
set(MINIAUDIO_NO_LIBOPUS ON CACHE BOOL "" FORCE)
set(MINIAUDIO_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    miniaudio
    GIT_REPOSITORY https://github.com/mackron/miniaudio.git
    GIT_TAG 0.11.25
)
FetchContent_MakeAvailable(miniaudio)


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
    src/Font.cpp
    src/AudioSystem.cpp
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
        miniaudio
)


# =========================
# Sample Game
# =========================

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    add_subdirectory(sample)
endif()
# Device-independent API tests (CI hosts do not require sound output).
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    include(CTest)
    if(BUILD_TESTING)
        add_executable(AudioSystemTests tests/AudioSystemTests.cpp)
        target_link_libraries(AudioSystemTests PRIVATE LWGF::Framework)
        add_test(NAME AudioSystemTests COMMAND AudioSystemTests)
    endif()
endif()
