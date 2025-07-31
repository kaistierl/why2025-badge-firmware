cmake_minimum_required(VERSION 3.16)
project(host_tests)

# Set C standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

message(STATUS "Host test compiler: ${CMAKE_C_COMPILER}")
message(STATUS "Host test compiler ID: ${CMAKE_C_COMPILER_ID}")
message(STATUS "Host test compiler version: ${CMAKE_C_COMPILER_VERSION}")

# Enable testing
enable_testing()

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../compute_badgevms
    ${CMAKE_CURRENT_SOURCE_DIR}/../compute_badgevms/thirdparty
)

add_executable(logical_names_test
    ${CMAKE_CURRENT_SOURCE_DIR}/../compute_badgevms/logical_names.c
)

target_compile_definitions(logical_names_test PRIVATE RUN_TEST)

target_compile_options(logical_names_test PRIVATE
    -Wall
    -Wextra
    -Werror
)

add_test(NAME logical_names_test COMMAND logical_names_test)

add_custom_target(run_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure --verbose
    DEPENDS logical_names_test
    COMMENT "Running all host tests"
)
