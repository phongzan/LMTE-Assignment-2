if(NOT DEFINED FLUX OR NOT DEFINED SOURCE OR NOT DEFINED EXPECTED)
    message(FATAL_ERROR "FLUX, SOURCE and EXPECTED are required")
endif()

set(command_args ${ARGS} "${SOURCE}")
if(DEFINED INPUT AND NOT INPUT STREQUAL "")
    execute_process(
        COMMAND "${FLUX}" ${command_args}
        INPUT_FILE "${INPUT}"
        OUTPUT_VARIABLE actual_stdout
        ERROR_VARIABLE actual_stderr
        RESULT_VARIABLE actual_exit
    )
else()
    execute_process(
        COMMAND "${FLUX}" ${command_args}
        OUTPUT_VARIABLE actual_stdout
        ERROR_VARIABLE actual_stderr
        RESULT_VARIABLE actual_exit
    )
endif()

file(READ "${EXPECTED}" expected_text)
string(REPLACE "\r\n" "\n" actual_stdout "${actual_stdout}")
string(REPLACE "\r\n" "\n" actual_stderr "${actual_stderr}")
string(REPLACE "\r\n" "\n" expected_text "${expected_text}")

if(DEFINED EXPECT_FAILURE AND EXPECT_FAILURE)
    if(actual_exit EQUAL 0)
        message(FATAL_ERROR "Expected failure but Flux exited successfully")
    endif()
    set(actual_text "${actual_stderr}")
else()
    if(NOT actual_exit EQUAL 0)
        message(FATAL_ERROR "Flux exited with ${actual_exit}:\n${actual_stderr}")
    endif()
    set(actual_text "${actual_stdout}")
endif()

if(NOT actual_text STREQUAL expected_text)
    message(FATAL_ERROR
        "Golden output mismatch\n--- expected ---\n${expected_text}"
        "--- actual ---\n${actual_text}")
endif()
