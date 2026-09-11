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

add_custom_command(
    TARGET SampleGame
    POST_BUILD

    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:SampleGame>/assets
)