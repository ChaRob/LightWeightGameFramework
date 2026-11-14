include_guard(GLOBAL)

# Configure the game target's default asset mode, independently of build config.
# DIRECTORY: access the original source directly.
# PACKAGE: always pack into a dedicated assets folder beside the executable.
function(lwgf_configure_assets)
    cmake_parse_arguments(PARSE_ARGV 0 ARG "" "TARGET;SOURCE_DIR;MODE;PART_SIZE_BYTES" "")
    if(ARG_UNPARSED_ARGUMENTS OR ARG_KEYWORDS_MISSING_VALUES)
        message(FATAL_ERROR "Invalid lwgf_configure_assets arguments")
    endif()
    if(NOT ARG_TARGET OR NOT TARGET "${ARG_TARGET}" OR NOT ARG_SOURCE_DIR OR NOT ARG_MODE)
        message(FATAL_ERROR "lwgf_configure_assets requires TARGET, SOURCE_DIR and MODE")
    endif()
    get_target_property(target_type "${ARG_TARGET}" TYPE)
    get_target_property(imported "${ARG_TARGET}" IMPORTED)
    get_target_property(alias "${ARG_TARGET}" ALIASED_TARGET)
    get_target_property(configured "${ARG_TARGET}" LWGF_ASSETS_CONFIGURED)
    if(NOT target_type STREQUAL "EXECUTABLE" OR imported OR alias OR configured)
        message(FATAL_ERROR "LWGF assets require a real executable target, configured only once: ${ARG_TARGET}")
    endif()
    get_target_property(target_source "${ARG_TARGET}" SOURCE_DIR)
    if(NOT target_source STREQUAL CMAKE_CURRENT_SOURCE_DIR)
        message(FATAL_ERROR "Call lwgf_configure_assets in the directory that defines the game target")
    endif()
    if(NOT ARG_MODE STREQUAL "DIRECTORY" AND NOT ARG_MODE STREQUAL "PACKAGE")
        message(FATAL_ERROR "Invalid LWGF asset MODE: ${ARG_MODE} (expected DIRECTORY or PACKAGE)")
    endif()
    if(NOT DEFINED ARG_PART_SIZE_BYTES)
        set(ARG_PART_SIZE_BYTES 536870912)
    endif()
    string(LENGTH "${ARG_PART_SIZE_BYTES}" part_digits)
    if(NOT ARG_PART_SIZE_BYTES MATCHES "^[1-9][0-9]*$" OR
       part_digits GREATER 19 OR
       (part_digits EQUAL 19 AND ARG_PART_SIZE_BYTES STRGREATER "9223372036854775807") OR
       (part_digits LESS 3 AND ARG_PART_SIZE_BYTES LESS 33))
        message(FATAL_ERROR "PART_SIZE_BYTES must be an integer in [33, 9223372036854775807]")
    endif()

    get_filename_component(asset_root "${ARG_SOURCE_DIR}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    if(NOT IS_DIRECTORY "${asset_root}")
        message(FATAL_ERROR "LWGF asset source directory does not exist: ${asset_root}")
    endif()

    # Encode the UTF-8 source path as byte escapes so the generated C++ header
    # does not depend on the compiler's input code page (MSVC included).
    set(LWGF_CONFIG_PACKAGE false)
    set(LWGF_CONFIG_DIRECTORY_BYTES "")
    if(ARG_MODE STREQUAL "DIRECTORY")
        string(HEX "${asset_root}" root_hex)
        string(REGEX REPLACE "(..)" "\\\\x\\1" LWGF_CONFIG_DIRECTORY_BYTES "${root_hex}")
    else()
        set(LWGF_CONFIG_PACKAGE true)
        if(NOT TARGET LWGFAssetPacker)
            message(FATAL_ERROR "PACKAGE mode requires LWGFAssetPacker (native host build)")
        endif()
        set(asset_target "${ARG_TARGET}_assets")
        if(TARGET "${asset_target}")
            message(FATAL_ERROR "LWGF asset target already exists: ${asset_target}")
        endif()

        # Custom target deliberately runs even when the executable is up to
        # date: additions, removals and same-size edits all reach the packer.
        # The packer owns the directory transaction and refuses unrelated files.
        # CMP0112 NEW: TARGET_FILE_DIR does not create a reverse dependency.
        cmake_policy(PUSH)
        cmake_policy(SET CMP0112 NEW)
        add_custom_target("${asset_target}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:${ARG_TARGET}>"
            COMMAND "$<TARGET_FILE:LWGFAssetPacker>" pack
                "${asset_root}" "$<TARGET_FILE_DIR:${ARG_TARGET}>/assets" "${ARG_PART_SIZE_BYTES}"
            DEPENDS LWGFAssetPacker
            COMMENT "LWGF: packaging assets for ${ARG_TARGET}"
            VERBATIM
        )
        cmake_policy(POP)
        add_dependencies("${ARG_TARGET}" "${asset_target}")
    endif()

    set(config_dir "${CMAKE_CURRENT_BINARY_DIR}/lwgf-assets/${ARG_TARGET}")
    configure_file(
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/AssetBuildConfig.h.in"
        "${config_dir}/lwgf/AssetBuildConfig.h"
        @ONLY
    )
    target_include_directories("${ARG_TARGET}" PRIVATE "${config_dir}")
    set_property(TARGET "${ARG_TARGET}" PROPERTY LWGF_ASSETS_CONFIGURED TRUE)
    message(STATUS "LWGF assets for ${ARG_TARGET}: ${ARG_MODE}, source=${asset_root}, part=${ARG_PART_SIZE_BYTES} bytes")
endfunction()
