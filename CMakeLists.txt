cmake_minimum_required(VERSION 3.20)

project(
    LightWeightGameFramework
    VERSION 0.0.8
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
# Asset package I/O and host-side tool
# =========================

add_library(lwgf_asset_io STATIC
    src/SplitAssetData.cpp
    src/AssetPackageIndex.cpp
    src/AssetPath.cpp
)
target_compile_features(lwgf_asset_io PUBLIC cxx_std_20)
target_include_directories(lwgf_asset_io PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

option(LWGF_BUILD_ASSET_PACKER "Build the LWGF host asset packaging tool" ON)
if(LWGF_BUILD_ASSET_PACKER AND NOT CMAKE_CROSSCOMPILING)
    add_executable(LWGFAssetPacker
        tools/AssetPackerMain.cpp
        tools/AssetPacker.cpp
    )
    target_link_libraries(LWGFAssetPacker PRIVATE lwgf_asset_io)
endif()

# =========================
# LightWeightGameFramework
# =========================

add_library(
    LightWeightGameFramework
    STATIC

    src/Application.cpp
    src/Renderer2D.cpp
    src/Texture2D.cpp
    src/SpriteAnimation.cpp
    src/Font.cpp
    src/AudioSystem.cpp
    src/AudioAssetVfs.cpp
    src/AssetReader.cpp
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
        lwgf_asset_io
)


include(cmake/LWGFAssets.cmake)

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
        add_executable(SplitAssetDataTests tests/SplitAssetDataTests.cpp)
        target_link_libraries(SplitAssetDataTests PRIVATE lwgf_asset_io)
        add_test(NAME SplitAssetDataTests COMMAND SplitAssetDataTests)
        add_executable(AssetReaderTests tests/AssetReaderTests.cpp)
        target_include_directories(AssetReaderTests PRIVATE src)
        target_link_libraries(AssetReaderTests PRIVATE LWGF::Framework lwgf_asset_io)
        add_test(NAME AssetReaderTests COMMAND AssetReaderTests)
        if(TARGET LWGFAssetPacker)
            add_executable(AudioAssetTests tests/AudioAssetTests.cpp)
            target_include_directories(AudioAssetTests PRIVATE src)
            target_link_libraries(AudioAssetTests PRIVATE LWGF::Framework miniaudio)
            add_test(NAME AudioAssetTests
                COMMAND ${CMAKE_COMMAND}
                    "-DPACKER=$<TARGET_FILE:LWGFAssetPacker>"
                    "-DAUDIO=$<TARGET_FILE:AudioAssetTests>"
                    "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/sample/assets"
                    "-DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/AudioAssetTests.cmake")
            add_executable(GraphicsAssetTests tests/GraphicsAssetTests.cpp)
            target_link_libraries(GraphicsAssetTests PRIVATE LWGF::Framework lwgf_glad)
            add_test(NAME GraphicsAssetTests
                COMMAND ${CMAKE_COMMAND}
                    "-DPACKER=$<TARGET_FILE:LWGFAssetPacker>"
                    "-DGRAPHICS=$<TARGET_FILE:GraphicsAssetTests>"
                    "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/sample/assets"
                    "-DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/GraphicsAssetTests.cmake")
            add_executable(AssetPackerTests
                tests/AssetPackerTests.cpp
                tools/AssetPacker.cpp
            )
            target_include_directories(AssetPackerTests PRIVATE tools)
            target_link_libraries(AssetPackerTests PRIVATE lwgf_asset_io)
            add_test(NAME AssetPackerTests COMMAND AssetPackerTests)
            # Capture the parent MSVC/Ninja configure environment. A nested
            # compiler check must not inherit an unrelated x86 shell's SDK/CRT.
            set(asset_build_env_file "")
            if(MSVC AND CMAKE_GENERATOR MATCHES "Ninja")
                set(asset_build_env_file "${CMAKE_CURRENT_BINARY_DIR}/AssetBuildEnvironment.cmake")
                set(asset_build_env_content "# Local MSVC environment for AssetBuildTests only.\n")
                foreach(name IN ITEMS PATH INCLUDE LIB LIBPATH
                        VCToolsInstallDir VCToolsVersion VCINSTALLDIR VSINSTALLDIR
                        WindowsSdkDir WindowsSDKVersion UniversalCRTSdkDir UCRTVersion
                        VSCMD_ARG_HOST_ARCH VSCMD_ARG_TGT_ARCH)
                    # Bracket literals preserve semicolons, trailing backslashes,
                    # spaces and dollar signs without CMake list escaping.
                    set(value "$ENV{${name}}")
                    set(equals "=")
                    while(TRUE)
                        string(FIND "${value}" "]${equals}]" delimiter_pos)
                        if(delimiter_pos EQUAL -1)
                            break()
                        endif()
                        string(APPEND equals "=")
                    endwhile()
                    string(APPEND asset_build_env_content
                        "set(ENV{${name}} [${equals}[${value}]${equals}])\n")
                endforeach()
                file(CONFIGURE OUTPUT "${asset_build_env_file}"
                    CONTENT "${asset_build_env_content}" @ONLY)
            endif()
            add_test(NAME AssetBuildTests
                COMMAND ${CMAKE_COMMAND}
                    "-DPACKER=$<TARGET_FILE:LWGFAssetPacker>"
                    "-DCOMPARE=$<TARGET_FILE:AssetPackerTests>"
                    "-DMODULE=${CMAKE_CURRENT_SOURCE_DIR}/cmake/LWGFAssets.cmake"
                    "-DFIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/tests/asset-build-fixture"
                    "-DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}"
                    "-DGENERATOR=${CMAKE_GENERATOR}"
                    "-DPLATFORM=${CMAKE_GENERATOR_PLATFORM}"
                    "-DTOOLSET=${CMAKE_GENERATOR_TOOLSET}"
                    "-DCOMPILER=${CMAKE_CXX_COMPILER}"
                    "-DMAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}"
                    "-DCONFIG=$<CONFIG>"
                    "-DEXE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
                    "-DMSVC_ENV_FILE=${asset_build_env_file}"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/AssetBuildTests.cmake")

            add_test(NAME AssetPackerSampleTests
                COMMAND ${CMAKE_COMMAND}
                    "-DPACKER=$<TARGET_FILE:LWGFAssetPacker>"
                    "-DCOMPARE=$<TARGET_FILE:AssetPackerTests>"
                    "-DREADER=$<TARGET_FILE:AssetReaderTests>"
                    "-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/sample/assets"
                    "-DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/AssetPackerSampleTests.cmake")
        endif()
        add_executable(AudioSystemTests tests/AudioSystemTests.cpp)
        target_link_libraries(AudioSystemTests PRIVATE LWGF::Framework)
        add_test(NAME AudioSystemTests COMMAND AudioSystemTests)
        add_executable(SpriteAnimationTests tests/SpriteAnimationTests.cpp)
        target_link_libraries(SpriteAnimationTests PRIVATE LWGF::Framework)
        add_test(NAME SpriteAnimationTests COMMAND SpriteAnimationTests)
    endif()
endif()
