# Real WAV fixtures; intentionally small parts for boundary-crossing reads.
if(NOT DEFINED PACKER OR NOT DEFINED AUDIO OR NOT DEFINED SOURCE OR NOT DEFINED OUTPUT_ROOT)
    message(FATAL_ERROR "Missing AudioAssetTests arguments")
endif()
string(RANDOM LENGTH 16 ALPHABET 0123456789abcdef suffix)
set(output "${OUTPUT_ROOT}/audio-asset-test-${suffix}")
execute_process(
    COMMAND "${PACKER}" pack "${SOURCE}" "${output}" 65536
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output_text
    ERROR_VARIABLE error_text
)
if(NOT result STREQUAL "0")
    message(FATAL_ERROR "Audio fixture pack failed: ${result}\n${output_text}\n${error_text}")
endif()
execute_process(
    COMMAND "${AUDIO}" "${SOURCE}" "${output}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output_text
    ERROR_VARIABLE error_text
    TIMEOUT 60
)
if(NOT result STREQUAL "0")
    message(FATAL_ERROR "Audio asset test failed: ${result}\n${output_text}\n${error_text}")
endif()
file(REMOVE_RECURSE "${output}")
