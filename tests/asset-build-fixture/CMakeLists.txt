cmake_minimum_required(VERSION 3.20)
project(ExternalGame LANGUAGES CXX)

# Emulate add_subdirectory(LWGF) without fetching/building all graphics
# dependencies again. Use the real, already built host packer and module.
add_subdirectory(framework)
add_executable(ExternalGame main.cpp)
target_compile_features(ExternalGame PRIVATE cxx_std_20)
set_target_properties(ExternalGame PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/runtime/$<CONFIG>")
lwgf_configure_assets(
    TARGET ExternalGame
    SOURCE_DIR "${ASSET_SOURCE}"
    MODE "${ASSET_MODE}"
    PART_SIZE_BYTES "${PART_BYTES}"
)
