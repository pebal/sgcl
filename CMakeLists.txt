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

# One subdirectory per module (core, containers, concurrent, async, io),
# each with its own detail/; the includes inside are relative to the
# including file, so the headers work from any include path.
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

# OpenSSL 3's libcrypto: the oracle the crypto tests and benchmarks hold the
# module against, linked to them alone, never to the library. Homebrew's
# openssl@3 on macOS, the system's elsewhere; where it is not found the
# crypto tests are not built and the benchmarks have no openssl side.
if(APPLE)
    set(SGCL_OPENSSL_DEFAULT "/opt/homebrew/opt/openssl@3")
else()
    set(SGCL_OPENSSL_DEFAULT "/usr")
endif()
set(SGCL_OPENSSL_ROOT "${SGCL_OPENSSL_DEFAULT}" CACHE PATH "An OpenSSL 3 installation, the crypto tests' and benchmarks' oracle")
find_library(SGCL_LIBCRYPTO NAMES crypto HINTS "${SGCL_OPENSSL_ROOT}/lib" "${SGCL_OPENSSL_ROOT}/lib64" NO_DEFAULT_PATH)
if(SGCL_LIBCRYPTO AND EXISTS "${SGCL_OPENSSL_ROOT}/include/openssl/evp.h")
    set(SGCL_HAVE_OPENSSL TRUE)
else()
    set(SGCL_HAVE_OPENSSL FALSE)
    message(STATUS "SGCL: no OpenSSL 3 at SGCL_OPENSSL_ROOT (${SGCL_OPENSSL_ROOT}): tests_crypto not built")
endif()

# liblzma (xz's): the oracle of the LZMA tests and the other side of
# bench_lzma, linked to them alone. Homebrew's xz on macOS, the system's
# elsewhere; where it is not found the tests that need it are skipped and
# the benchmark measures the library only.
if(APPLE)
    set(SGCL_XZ_DEFAULT "/opt/homebrew/opt/xz")
else()
    set(SGCL_XZ_DEFAULT "/usr")
endif()
set(SGCL_XZ_ROOT "${SGCL_XZ_DEFAULT}" CACHE PATH "An xz (liblzma) installation, the LZMA tests' and benchmark's oracle")
find_library(SGCL_LIBLZMA NAMES lzma HINTS "${SGCL_XZ_ROOT}/lib" "${SGCL_XZ_ROOT}/lib64" NO_DEFAULT_PATH)
if(SGCL_LIBLZMA AND EXISTS "${SGCL_XZ_ROOT}/include/lzma.h")
    set(SGCL_HAVE_LIBLZMA TRUE)
else()
    set(SGCL_HAVE_LIBLZMA FALSE)
    message(STATUS "SGCL: no liblzma at SGCL_XZ_ROOT (${SGCL_XZ_ROOT}): the LZMA tests against it are skipped")
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
