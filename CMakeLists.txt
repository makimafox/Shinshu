cmake_minimum_required(VERSION 3.25)

project(NeuralMemorizationCompression VERSION 0.1.0 LANGUAGES CXX)

option(SHINSHU_BUILD_TESTS "Enable tests" ON)
option(SHINSHU_BUILD_BENCHMARKS "Enable benchmarks" OFF)
option(SHINSHU_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(cmake/CompilerWarnings.cmake)
include(cmake/Sanitizers.cmake)

add_library(shinshu_core
    src/core/version.cpp
    src/cli/arguments.cpp
)
add_library(shinshu::core ALIAS shinshu_core)
target_include_directories(shinshu_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_definitions(shinshu_core PUBLIC SHINSHU_VERSION="${PROJECT_VERSION}")
shinshu_set_warnings(shinshu_core)
shinshu_enable_sanitizers(shinshu_core)

# These interface targets establish real public component boundaries. Each
# exports its headers and can acquire implementation sources as research lands.
foreach(component IN ITEMS entropy format neural)
    add_library(shinshu_${component} INTERFACE)
    add_library(shinshu::${component} ALIAS shinshu_${component})
    target_include_directories(shinshu_${component} INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )
endforeach()
target_link_libraries(shinshu_entropy INTERFACE shinshu_core)
target_link_libraries(shinshu_format INTERFACE shinshu_core)
target_link_libraries(shinshu_neural INTERFACE shinshu_core)

add_executable(shinshu_cli src/cli/main.cpp)
set_target_properties(shinshu_cli PROPERTIES OUTPUT_NAME shinshu)
target_link_libraries(shinshu_cli PRIVATE shinshu::core)
shinshu_set_warnings(shinshu_cli)
shinshu_enable_sanitizers(shinshu_cli)

if(SHINSHU_BUILD_TESTS)
    include(CTest)
    enable_testing()

    # CLI contract tests stay dependency-free; GoogleTest is available through
    # vcpkg for future unit-level suites and is used when present.
    add_test(NAME cli_help COMMAND shinshu_cli --help)
    add_test(NAME cli_version COMMAND shinshu_cli --version)
    add_test(NAME cli_rejects_unknown_option COMMAND shinshu_cli --unknown)
    set_tests_properties(cli_rejects_unknown_option PROPERTIES WILL_FAIL TRUE)
    add_test(NAME cli_reports_unimplemented_command COMMAND shinshu_cli compress input.bin -o output.shinshu)
    set_tests_properties(cli_reports_unimplemented_command PROPERTIES WILL_FAIL TRUE)

    find_package(GTest 1.14 CONFIG QUIET)
    if(GTest_FOUND)
        add_executable(shinshu_tests tests/unit/api_tests.cpp)
        target_link_libraries(shinshu_tests PRIVATE shinshu::core GTest::gtest_main)
        shinshu_set_warnings(shinshu_tests)
        shinshu_enable_sanitizers(shinshu_tests)
        include(GoogleTest)
        gtest_discover_tests(shinshu_tests)
    else()
        message(STATUS "GoogleTest not found; dependency-free CLI contract tests remain enabled")
    endif()
endif()

if(SHINSHU_BUILD_BENCHMARKS)
    message(FATAL_ERROR "SHINSHU_BUILD_BENCHMARKS is enabled, but no benchmarks are implemented yet")
endif()
