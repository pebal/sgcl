cmake_minimum_required(VERSION 3.16)
project(SGCL LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ccache for the builds of this tree (tests, benchmarks), when it is
# installed and this is the top-level project: a program that adds sgcl as
# a subdirectory keeps its own compiler. Worktrees share the cache when
# ccache's base_dir is the tree's parent directory (ccache --set-config).
option(SGCL_CCACHE "Compile this tree through ccache when it is installed" ON)
if(SGCL_CCACHE AND CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR AND NOT CMAKE_CXX_COMPILER_LAUNCHER)
    find_program(SGCL_CCACHE_PROGRAM ccache)
    if(SGCL_CCACHE_PROGRAM)
        set(CMAKE_CXX_COMPILER_LAUNCHER "${SGCL_CCACHE_PROGRAM}")
    endif()
endif()

option(SGCL_BUILD_TESTS "Build unit and stress tests (requires external/googletest)" ON)
option(SGCL_BUILD_BENCHMARKS "Build benchmarks (always optimized)" ON)
set(SGCL_SANITIZER "" CACHE STRING "Sanitizer for tests: address, thread or empty")
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

# The system frameworks codec::heif calls on Apple's systems (ImageIO for
# HEIF and AVIF, CoreGraphics, CoreFoundation, vImage in Accelerate), and
# Security, whose trust settings crypto::x509 reads the system's roots from
# (sgcl/crypto/detail/apple_security.h), and CoreServices, whose FSEvents
# io::watch reads the changes of the file system with
# (sgcl/io/detail/apple_fsevents.h), linked with the library. A program that makes no call still lists them (loaded
# from the shared cache at start); -Wl,-dead_strip_dylibs drops them.
# Their headers are never included: sgcl/codec/detail/apple_imageio.h,
# sgcl/crypto/detail/apple_security.h and sgcl/io/detail/apple_fsevents.h
# declare what they call (the rule for platform headers).
if(APPLE)
    target_link_libraries(sgcl INTERFACE
        "-framework ImageIO"
        "-framework CoreGraphics"
        "-framework CoreFoundation"
        "-framework Accelerate"
        "-framework Security"
        "-framework CoreServices"
    )
endif()

# Release builds of a program that links sgcl: no exported symbols and no
# local symbol table. The library is header-only, so every inline function
# and template it instantiates would otherwise be a weak exported symbol of
# the program, with its mangled name in the string table and a binding for
# dyld (the async example program: 1.43 MB, 788 KB with these flags, the
# code itself unchanged). The flags apply to the whole program that links
# sgcl, not to sgcl alone: turn the option off for a shared library whose
# own API must stay exported, and for a process in which more than one image
# (the program and a .dylib/.so) uses sgcl, since hidden symbols give each
# image a collector of its own (docs/sgcl/core/config.md).
option(SGCL_HIDDEN_VISIBILITY "Release: hide the symbols of programs linking sgcl and drop their local symbol table" ON)
if(SGCL_HIDDEN_VISIBILITY AND NOT MSVC)
    target_compile_options(sgcl INTERFACE
        $<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:CXX>>:-fvisibility=hidden -fvisibility-inlines-hidden>
    )
    target_link_options(sgcl INTERFACE
        $<$<CONFIG:Release>:-Wl,-x>
    )
endif()

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

# Sanitizer flags applied to everything built in this tree (tests, benchmarks,
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
    message(STATUS "SGCL: no OpenSSL 3 at SGCL_OPENSSL_ROOT (${SGCL_OPENSSL_ROOT}): tests_crypto runs the vectors alone, without the oracle")
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

# libyaml: the other side of bench_yaml (Go's standard library has no YAML),
# linked to it alone; where it is not found the benchmark measures the library only
if(APPLE)
    set(SGCL_LIBYAML_DEFAULT "/opt/homebrew/opt/libyaml")
else()
    set(SGCL_LIBYAML_DEFAULT "/usr")
endif()
set(SGCL_LIBYAML_ROOT "${SGCL_LIBYAML_DEFAULT}" CACHE PATH "A libyaml installation, bench_yaml's reference")
find_library(SGCL_LIBYAML NAMES yaml HINTS "${SGCL_LIBYAML_ROOT}/lib" "${SGCL_LIBYAML_ROOT}/lib64" NO_DEFAULT_PATH)
if(SGCL_LIBYAML AND EXISTS "${SGCL_LIBYAML_ROOT}/include/yaml.h")
    set(SGCL_HAVE_LIBYAML TRUE)
else()
    set(SGCL_HAVE_LIBYAML FALSE)
endif()

# BLAKE3's C library, its authors': the reference the BLAKE3 tests and
# bench_crypto hold the module to (Go has no BLAKE3), linked to them alone.
# Homebrew's blake3 on macOS, the system's elsewhere; where it is not found
# the tests run the published vectors alone and the benchmark has no
# reference side.
if(APPLE)
    set(SGCL_BLAKE3_DEFAULT "/opt/homebrew/opt/blake3")
else()
    set(SGCL_BLAKE3_DEFAULT "/usr")
endif()
set(SGCL_BLAKE3_ROOT "${SGCL_BLAKE3_DEFAULT}" CACHE PATH "A BLAKE3 C library installation, the BLAKE3 tests' and benchmark's reference")
find_library(SGCL_LIBBLAKE3 NAMES blake3 HINTS "${SGCL_BLAKE3_ROOT}/lib" "${SGCL_BLAKE3_ROOT}/lib64" NO_DEFAULT_PATH)
if(SGCL_LIBBLAKE3 AND EXISTS "${SGCL_BLAKE3_ROOT}/include/blake3.h")
    set(SGCL_HAVE_BLAKE3 TRUE)
else()
    set(SGCL_HAVE_BLAKE3 FALSE)
    message(STATUS "SGCL: no BLAKE3 C library at SGCL_BLAKE3_ROOT (${SGCL_BLAKE3_ROOT}): the BLAKE3 tests run the published vectors alone")
endif()

# xxHash, LZ4, zstd and brotli (Homebrew's on macOS, the system's
# elsewhere): the oracles of the tests of XXH32/XXH64 and of the compress
# formats of the same names, and the other side of their benchmarks, linked
# to those alone. SGCL_HAVE_<NAME> says which were found; a test or a
# benchmark case that needs a missing one is skipped.
if(APPLE)
    set(SGCL_REFERENCE_DEFAULT "/opt/homebrew/opt")
else()
    set(SGCL_REFERENCE_DEFAULT "")
endif()
set(SGCL_REFERENCE_ROOT "${SGCL_REFERENCE_DEFAULT}" CACHE PATH "Where xxhash, lz4, zstd and brotli are installed, one directory each (empty: the system's)")
function(sgcl_find_reference NAME HEADER)
    if(SGCL_REFERENCE_ROOT)
        set(ROOT "${SGCL_REFERENCE_ROOT}/${NAME}")
    else()
        set(ROOT "/usr")
    endif()
    set(LIBS "")
    foreach(L ${ARGN})
        find_library(SGCL_LIB_${L} NAMES ${L} HINTS "${ROOT}/lib" "${ROOT}/lib64" NO_DEFAULT_PATH)
        if(NOT SGCL_LIB_${L})
            set(SGCL_HAVE_${NAME} FALSE PARENT_SCOPE)
            message(STATUS "SGCL: no ${NAME} at ${ROOT}: the tests and benchmarks against it are skipped")
            return()
        endif()
        list(APPEND LIBS ${SGCL_LIB_${L}})
    endforeach()
    if(NOT EXISTS "${ROOT}/include/${HEADER}")
        set(SGCL_HAVE_${NAME} FALSE PARENT_SCOPE)
        message(STATUS "SGCL: no ${NAME} at ${ROOT}: the tests and benchmarks against it are skipped")
        return()
    endif()
    set(SGCL_HAVE_${NAME} TRUE PARENT_SCOPE)
    set(SGCL_${NAME}_INCLUDE "${ROOT}/include" PARENT_SCOPE)
    set(SGCL_${NAME}_LIBS ${LIBS} PARENT_SCOPE)
endfunction()
sgcl_find_reference(xxhash xxhash.h xxhash)
sgcl_find_reference(lz4 lz4frame.h lz4)
sgcl_find_reference(zstd zstd.h zstd)
sgcl_find_reference(brotli brotli/encode.h brotlienc brotlidec brotlicommon)
# Attaches the references found to a test or a benchmark program:
# SGCL_TEST_<NAME>=1 (or SGCL_BENCH_<NAME>=1) where one was found
function(sgcl_link_references TARGET PREFIX)
    foreach(NAME ${ARGN})
        if(SGCL_HAVE_${NAME})
            string(TOUPPER ${NAME} UPPER)
            target_include_directories(${TARGET} PRIVATE ${SGCL_${NAME}_INCLUDE})
            target_link_libraries(${TARGET} ${SGCL_${NAME}_LIBS})
            target_compile_definitions(${TARGET} PRIVATE ${PREFIX}_${UPPER}=1)
        endif()
    endforeach()
endfunction()

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
