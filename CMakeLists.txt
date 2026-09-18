cmake_minimum_required(VERSION 3.16)
project(SGCL LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(SGCL_BUILD_TESTS "Build unit and stress tests (requires external/googletest)" ON)
option(SGCL_BUILD_EXAMPLES "Build examples" ON)
option(SGCL_BUILD_BENCHMARKS "Build benchmarks (always optimized)" ON)
set(SGCL_SANITIZER "" CACHE STRING "Sanitizer for tests/examples: address, thread or empty")
set_property(CACHE SGCL_SANITIZER PROPERTY STRINGS "" address thread)

file(GLOB_RECURSE SGCL_SOURCES "sgcl/*.h")
add_library(sgcl INTERFACE
    ${SGCL_SOURCES}
)

# The include path is the tree's root: "sgcl/sgcl.h". Not the
# sgcl directory itself, whose headers are named after their std
# counterparts (string.h among them) and would shadow the C headers.
target_include_directories(sgcl INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

# One subdirectory per module (core, containers, concurrent, async), each
# with its own detail/, and Sgcl/ for the second interface over them; the
# includes inside are relative to the including file, so the headers work
# from any include path and a quoted "sgcl/..." never resolves to Sgcl/ on
# a case-insensitive filesystem.
install(DIRECTORY sgcl/
    DESTINATION include/sgcl
    FILES_MATCHING PATTERN "*.h"
)

install(TARGETS sgcl
    EXPORT sgclTargets
    INCLUDES DESTINATION include
)

install(EXPORT sgclTargets
    FILE sgclTargets.cmake
    NAMESPACE SGCL::
    DESTINATION lib/cmake/SGCL
)

# Sanitizer flags applied to everything built in this tree (tests, examples,
# googletest), never exported with the interface library.
if(SGCL_SANITIZER)
    if(MSVC)
        add_compile_options(/fsanitize=${SGCL_SANITIZER})
    else()
        add_compile_options(-fsanitize=${SGCL_SANITIZER} -fno-omit-frame-pointer -g)
        add_link_options(-fsanitize=${SGCL_SANITIZER})
        if(SGCL_SANITIZER STREQUAL "address" AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            # locals must stay on the real stack: the collector scans it for roots
            add_compile_options(-fsanitize-address-use-after-return=never)
        endif()
    endif()
    message(STATUS "SGCL: building with -fsanitize=${SGCL_SANITIZER}")
endif()

if(SGCL_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(SGCL_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

if(SGCL_BUILD_TESTS)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/googletest/CMakeLists.txt")
        set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
        set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
        add_subdirectory(external/googletest)
        enable_testing()
        add_subdirectory(tests)
    else()
        message(WARNING "SGCL: external/googletest is missing; run "
            "'git submodule update --init' to build the tests (SGCL_BUILD_TESTS=OFF to silence)")
    endif()
endif()
