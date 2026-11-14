# Build a separate minimal external game with the real reusable CMake module.
foreach(required PACKER COMPARE MODULE FIXTURE OUTPUT_ROOT GENERATOR COMPILER CONFIG EXE_SUFFIX)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Missing AssetBuildTests argument: ${required}")
    endif()
endforeach()
string(RANDOM LENGTH 16 ALPHABET 0123456789abcdef suffix)
set(root "${OUTPUT_ROOT}/asset-build-test-${suffix}")
set(source "${root}/source assets")
set(directory_build "${root}/directory-build")
set(package_build "${root}/package-build")
file(MAKE_DIRECTORY "${source}/sub")
string(REPEAT "a" 160 large)
file(WRITE "${source}/a.bin" "${large}")
file(WRITE "${source}/sub/b.bin" "old")

function(run_ok)
    execute_process(COMMAND ${ARGV}
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 120)
    if(NOT result STREQUAL "0")
        message(FATAL_ERROR "Command failed: ${ARGV}\n${result}\n${output}\n${error}")
    endif()
endfunction()

function(run_fail)
    execute_process(COMMAND ${ARGV}
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 120)
    if(result STREQUAL "0" OR NOT result MATCHES "^[0-9]+$")
        message(FATAL_ERROR "Expected nonzero process exit: ${ARGV}\n${result}\n${output}\n${error}")
    endif()
endfunction()

set(generator_args -G "${GENERATOR}")
if(DEFINED PLATFORM AND NOT PLATFORM STREQUAL "")
    list(APPEND generator_args -A "${PLATFORM}")
endif()
if(DEFINED TOOLSET AND NOT TOOLSET STREQUAL "")
    list(APPEND generator_args -T "${TOOLSET}")
endif()
set(compiler_args "-DCMAKE_CXX_COMPILER=${COMPILER}")
if(DEFINED MAKE_PROGRAM AND NOT MAKE_PROGRAM STREQUAL "")
    list(APPEND compiler_args "-DCMAKE_MAKE_PROGRAM=${MAKE_PROGRAM}")
endif()
function(configure_game build mode part_size)
    run_ok("${CMAKE_COMMAND}" -S "${FIXTURE}" -B "${build}"
        ${generator_args} ${compiler_args}
        "-DCMAKE_BUILD_TYPE=${CONFIG}"
        "-DPACKER=${PACKER}" "-DASSET_MODULE=${MODULE}"
        "-DASSET_SOURCE=${source}" "-DASSET_MODE=${mode}" "-DPART_BYTES=${part_size}")
endfunction()

function(build_game build)
    run_ok("${CMAKE_COMMAND}" --build "${build}" --config "${CONFIG}" --target ExternalGame)
endfunction()

# Directory mode uses original assets, not copied assets; a changed source
# file is read on the next run without rebuilding the executable.
configure_game("${directory_build}" DIRECTORY 64)
build_game("${directory_build}")
set(directory_runtime "${directory_build}/runtime/${CONFIG}")
set(directory_exe "${directory_runtime}/ExternalGame${EXE_SUFFIX}")
if(EXISTS "${directory_runtime}/assets")
    message(FATAL_ERROR "Directory mode unexpectedly copied assets")
endif()
file(WRITE "${source}/a.bin" "changed")
run_ok("${directory_exe}" DIRECTORY "${source}")

# Package mode: build just the game target, not ALL or a separate pack target.
file(WRITE "${source}/a.bin" "${large}")
configure_game("${package_build}" PACKAGE 64)
build_game("${package_build}")
set(runtime "${package_build}/runtime/${CONFIG}")
set(package "${runtime}/assets")
set(exe "${runtime}/ExternalGame${EXE_SUFFIX}")
run_ok("${exe}" PACKAGE "${runtime}")
run_ok("${PACKER}" verify "${package}")
run_ok("${COMPARE}" --compare "${source}" "${package}")
if(NOT EXISTS "${package}/assets.2" OR EXISTS "${package}/a.bin" OR EXISTS "${package}/sub")
    message(FATAL_ERROR "Expected split package only, with no loose assets")
endif()
file(GLOB parts "${package}/assets.*")
foreach(part IN LISTS parts)
    if(NOT part MATCHES "[.]pak$")
        file(SIZE "${part}" size)
        if(size GREATER 64)
            message(FATAL_ERROR "Part size exceeds configured limit: ${part}")
        endif()
    endif()
endforeach()

# Same-size edit with no C++ modification must reach the next build.
string(REPEAT "z" 160 changed)
file(WRITE "${source}/a.bin" "${changed}")
build_game("${package_build}")
run_ok("${COMPARE}" --compare "${source}" "${package}")

# Addition, modification and deletion without any C++ rebuild.
file(REMOVE "${source}/a.bin" "${source}/sub/b.bin")
file(WRITE "${source}/new.bin" "new")
build_game("${package_build}")
run_ok("${COMPARE}" --compare "${source}" "${package}")
run_ok("${PACKER}" verify "${package}")
if(EXISTS "${package}/assets.2")
    message(FATAL_ERROR "Stale part remained after repack")
endif()

# Build fails if packaging fails, and does not delete unrelated files or
# overwrite the previous index. No executable rebuild is required.
file(SHA256 "${package}/assets.pak" previous_hash)
file(WRITE "${package}/unrelated.txt" "keep me")
run_fail("${CMAKE_COMMAND}" --build "${package_build}" --config "${CONFIG}" --target ExternalGame)
file(READ "${package}/unrelated.txt" preserved)
file(SHA256 "${package}/assets.pak" current_hash)
if(NOT preserved STREQUAL "keep me" OR NOT previous_hash STREQUAL current_hash)
    message(FATAL_ERROR "Previous package was not preserved after failed build")
endif()
file(REMOVE "${package}/unrelated.txt")
run_ok("${PACKER}" verify "${package}")

# Invalid mode and part-size settings fail at configure time.
run_fail("${CMAKE_COMMAND}" -S "${FIXTURE}" -B "${package_build}" "-DASSET_MODE=INVALID")
run_fail("${CMAKE_COMMAND}" -S "${FIXTURE}" -B "${package_build}"
    "-DASSET_MODE=PACKAGE" "-DPART_BYTES=32")

file(REMOVE_RECURSE "${root}")
