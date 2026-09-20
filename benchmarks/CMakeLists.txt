# Benchmarks are always built optimized, whatever the configuration: numbers
# from a debug build mean nothing. Each binary holds every variant of one
# benchmark (SGCL, shared_ptr, unique_ptr, raw) and takes the variant as its
# first argument; see run.sh for the matrix. The sources lie in one
# directory per module (core, containers, im — the immutable family, concurrent, async), the binaries all in
# <build>/benchmarks, where the scripts look for them.
file(GLOB BENCHMARK_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*/*.cpp")

foreach(SRC ${BENCHMARK_SOURCES})
    get_filename_component(NAME ${SRC} NAME_WE)
    set(TARGET bench_${NAME})
    add_executable(${TARGET} ${SRC})
    target_link_libraries(${TARGET} sgcl)
    target_include_directories(${TARGET} PRIVATE ${CMAKE_SOURCE_DIR})
    target_compile_definitions(${TARGET} PRIVATE NDEBUG)
    if(MSVC)
        target_compile_options(${TARGET} PRIVATE /O2)
    else()
        target_compile_options(${TARGET} PRIVATE -O2)
    endif()
    set_target_properties(${TARGET} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/benchmarks)
endforeach()

# bench_im compares the immutable containers with immer when its headers
# are given: -DSGCL_IMMER_INCLUDE=<the root of an immer checkout>
set(SGCL_IMMER_INCLUDE "" CACHE PATH "The root of an immer checkout, for bench_im's immer variants")
if(SGCL_IMMER_INCLUDE)
    target_include_directories(bench_im PRIVATE ${SGCL_IMMER_INCLUDE})
endif()
