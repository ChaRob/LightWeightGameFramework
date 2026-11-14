add_executable(
    SampleGame

    src/main.cpp
    src/Game.cpp
)

target_link_libraries(
    SampleGame

    PRIVATE
        LWGF::Framework
)

set(LWGF_SAMPLE_ASSET_MODE "DIRECTORY" CACHE STRING "Sample asset source: DIRECTORY or PACKAGE")
set_property(CACHE LWGF_SAMPLE_ASSET_MODE PROPERTY STRINGS DIRECTORY PACKAGE)
set(LWGF_SAMPLE_ASSET_PART_SIZE "536870912" CACHE STRING "Sample package part size in bytes")

lwgf_configure_assets(
    TARGET SampleGame
    SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets"
    MODE "${LWGF_SAMPLE_ASSET_MODE}"
    PART_SIZE_BYTES "${LWGF_SAMPLE_ASSET_PART_SIZE}"
)
