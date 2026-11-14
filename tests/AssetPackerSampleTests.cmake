# Exercise the real CLI against the sample PNG/JPG/TTF/WAV assets.
if(NOT DEFINED PACKER OR NOT DEFINED SOURCE OR NOT DEFINED OUTPUT_ROOT OR NOT DEFINED COMPARE OR NOT DEFINED READER)
    message(FATAL_ERROR "Missing AssetPackerSampleTests arguments")
endif()
string(RANDOM LENGTH 16 ALPHABET 0123456789abcdef suffix)
set(output "${OUTPUT_ROOT}/asset-packer-sample-${suffix}")
execute_process(
    COMMAND "${PACKER}" pack "${SOURCE}" "${output}" 65536
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output_text
    ERROR_VARIABLE error_text
)
if(NOT result STREQUAL "0")
    message(FATAL_ERROR "Sample pack failed: ${result}\n${output_text}\n${error_text}")
endif()
if(NOT EXISTS "${output}/assets.pak" OR NOT EXISTS "${output}/assets.1" OR
   NOT EXISTS "${output}/assets.2")
    message(FATAL_ERROR "Sample package files missing")
endif()
file(GLOB parts "${output}/assets.*")
foreach(part IN LISTS parts)
    if(NOT part MATCHES "[.]pak$")
        file(SIZE "${part}" size)
        if(size GREATER 65536)
            message(FATAL_ERROR "Part exceeds configured size: ${part}")
        endif()
    endif()
endforeach()
execute_process(
    COMMAND "${PACKER}" verify "${output}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output_text
    ERROR_VARIABLE error_text
)
if(NOT result STREQUAL "0")
    message(FATAL_ERROR "Sample verify failed: ${result}\n${output_text}\n${error_text}")
endif()
execute_process(
    COMMAND "${COMPARE}" --compare "${SOURCE}" "${output}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output_text
    ERROR_VARIABLE error_text
)
if(NOT result STREQUAL "0")
    message(FATAL_ERROR "Sample byte comparison failed: ${result}\n${output_text}\n${error_text}")
endif()
execute_process(
    COMMAND "${READER}" --compare "${SOURCE}" "${output}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output_text
    ERROR_VARIABLE error_text
)
if(NOT result STREQUAL "0")
    message(FATAL_ERROR "Sample public reader comparison failed: ${result}\n${output_text}\n${error_text}")
endif()
file(REMOVE_RECURSE "${output}")
