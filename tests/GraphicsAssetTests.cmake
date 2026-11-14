# Pack the real graphics fixtures, then exercise public Load overloads.
if(NOT DEFINED PACKER OR NOT DEFINED GRAPHICS OR NOT DEFINED SOURCE OR NOT DEFINED OUTPUT_ROOT)
    message(FATAL_ERROR "Missing GraphicsAssetTests arguments")
endif()
string(RANDOM LENGTH 16 ALPHABET 0123456789abcdef suffix)
set(output "${OUTPUT_ROOT}/graphics-asset-test-${suffix}")
execute_process(
    COMMAND "${PACKER}" pack "${SOURCE}" "${output}" 65536
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output_text
    ERROR_VARIABLE error_text
)
if(NOT result STREQUAL "0")
    message(FATAL_ERROR "Graphics fixture pack failed: ${result}\n${output_text}\n${error_text}")
endif()
execute_process(
    COMMAND "${GRAPHICS}" "${SOURCE}" "${output}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output_text
    ERROR_VARIABLE error_text
)
if(NOT result STREQUAL "0")
    message(FATAL_ERROR "Graphics asset test failed: ${result}\n${output_text}\n${error_text}")
endif()
file(REMOVE_RECURSE "${output}")
