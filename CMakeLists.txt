cmake_minimum_required(VERSION 3.5) 
project(SGCL LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE SGCL_SOURCES "sgcl/*.h")
add_library(sgcl INTERFACE
    ${SGCL_SOURCES}
)

target_include_directories(sgcl INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/sgcl>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/sgcl/detail>
    $<INSTALL_INTERFACE:include/sgcl>
    $<INSTALL_INTERFACE:include/sgcl/detail>
)

install(DIRECTORY sgcl/
    DESTINATION include/sgcl
    FILES_MATCHING PATTERN "*.h"
)

install(DIRECTORY sgcl/detail/
    DESTINATION include/sgcl/detail
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

add_subdirectory(tests)
add_subdirectory(examples)
add_subdirectory(external/googletest)
