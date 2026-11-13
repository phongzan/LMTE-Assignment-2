cmake_minimum_required(VERSION 3.16)

project(flux LANGUAGES CXX)

# Use one portable language level for the interpreter and all test executables.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Core front-end and runtime components are shared by the CLI and unit tests.
add_library(flux_runtime
    src/Environment.cpp
    src/Interpreter.cpp
    src/Lexer.cpp
    src/Operators.cpp
    src/Parser.cpp
    src/SemanticAnalyzer.cpp
    src/Token.cpp
    src/Value.cpp
)

target_include_directories(flux_runtime PUBLIC include)

if(MSVC)
    target_compile_options(flux_runtime PRIVATE /W4 /permissive-)
else()
    target_compile_options(flux_runtime PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Command-line Flux executable.
add_executable(flux main.cpp)
target_link_libraries(flux PRIVATE flux_runtime)
if(MSVC)
    # Flux intentionally supports a deep but bounded recursion test.
    target_link_options(flux PRIVATE /STACK:33554432)
endif()

include(CTest)
if(BUILD_TESTING)
    # Keep front-end and runtime suites separate so failures identify the stage.
    add_executable(runtime_tests tests/runtime_tests.cpp)
    target_link_libraries(runtime_tests PRIVATE flux_runtime)
    add_test(NAME runtime_tests COMMAND runtime_tests)

    add_executable(frontend_tests tests/frontend_tests.cpp)
    target_link_libraries(frontend_tests PRIVATE flux_runtime)
    add_test(NAME frontend_tests COMMAND frontend_tests)

    add_executable(semantic_tests tests/semantic_tests.cpp)
    target_link_libraries(semantic_tests PRIVATE flux_runtime)
    add_test(NAME semantic_tests COMMAND semantic_tests)

    add_executable(interpreter_tests tests/interpreter_tests.cpp)
    target_link_libraries(interpreter_tests PRIVATE flux_runtime)
    if(MSVC)
        target_link_options(interpreter_tests PRIVATE /STACK:33554432)
    endif()
    add_test(NAME interpreter_tests COMMAND interpreter_tests)

    function(add_flux_golden_test name source expected expect_failure)
        add_test(
            NAME ${name}
            COMMAND ${CMAKE_COMMAND}
                -DFLUX=$<TARGET_FILE:flux>
                -DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/${source}
                -DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/${expected}
                -DEXPECT_FAILURE=${expect_failure}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_golden.cmake
        )
    endfunction()

    add_flux_golden_test(system_p01_literals
        tests/pass/p01_literals.flx tests/pass/p01_literals.expected FALSE)
    add_flux_golden_test(system_p20_stock_ledger
        tests/pass/p20_stock_ledger.flx tests/pass/p20_stock_ledger.expected FALSE)
    add_flux_golden_test(system_e01_invalid_character
        tests/fail/e01_invalid_character.flx tests/fail/e01_invalid_character.expected TRUE)
    add_flux_golden_test(system_e18_division_by_zero
        tests/fail/e18_division_by_zero.flx tests/fail/e18_division_by_zero.expected TRUE)
    add_test(
        NAME system_p22_memory
        COMMAND ${CMAKE_COMMAND}
            -DFLUX=$<TARGET_FILE:flux>
            -DARGS=--mem
            -DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/tests/pass/p22_memory.flx
            -DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/tests/pass/p22_memory.expected
            -DEXPECT_FAILURE=FALSE
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_golden.cmake
    )
endif()
