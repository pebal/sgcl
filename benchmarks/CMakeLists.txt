# Benchmarks are always built optimized, whatever the configuration: numbers
# from a debug build mean nothing. Each binary holds every variant of one
# benchmark (SGCL, shared_ptr, unique_ptr, raw) and takes the variant as its
# first argument; see run.sh for the matrix. The sources lie in one
# directory per module (core, containers — core's containers, immutable, concurrent, async...), the binaries all in
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

# bench_immutable compares the immutable containers with immer when its headers
# are given: -DSGCL_IMMER_INCLUDE=<the root of an immer checkout>
set(SGCL_IMMER_INCLUDE "" CACHE PATH "The root of an immer checkout, for bench_immutable's immer variants")
if(SGCL_IMMER_INCLUDE)
    target_include_directories(bench_immutable PRIVATE ${SGCL_IMMER_INCLUDE})
endif()

# bench_heap_io_net counts the objects a call leaves to the collector from the
# collector's own log of each cycle (heap/heap_parse.py sums it)
target_compile_definitions(bench_heap_io_net PRIVATE SGCL_LOG_PRINT_LEVEL=2)

# bench_compress compares with the system's zlib
target_link_libraries(bench_compress z)

# bench_lzma compares with liblzma when it is found (SGCL_XZ_ROOT, ../CMakeLists.txt)
if(SGCL_HAVE_LIBLZMA)
    target_include_directories(bench_lzma PRIVATE ${SGCL_XZ_ROOT}/include)
    target_link_libraries(bench_lzma ${SGCL_LIBLZMA})
    target_compile_definitions(bench_lzma PRIVATE SGCL_BENCH_LIBLZMA=1)
endif()

# bench_crypto, bench_curve25519, bench_ecc, bench_mlkem, bench_rsa and bench_x509 compare the crypto module with OpenSSL
# when libcrypto is found (SGCL_OPENSSL_ROOT, ../CMakeLists.txt)
if(SGCL_HAVE_OPENSSL)
    foreach(T bench_crypto bench_curve25519 bench_ecc bench_mlkem bench_rsa bench_x509)
        target_include_directories(${T} PRIVATE ${SGCL_OPENSSL_ROOT}/include)
        target_link_libraries(${T} ${SGCL_LIBCRYPTO})
        target_compile_definitions(${T} PRIVATE SGCL_BENCH_OPENSSL=1)
    endforeach()
endif()

# bench_heap_probe and bench_heap_probe2 are not timings: they count the
# managed objects and pages an operation allocates, off the collector's cycle
# log, which this level of logging prints (heap/parse.py reads it)
foreach(T bench_heap_probe bench_heap_probe2)
    target_compile_definitions(${T} PRIVATE SGCL_LOG_PRINT_LEVEL=2)
endforeach()

# The HTTP load test's server (net/http_load, run.sh there): one binary, the
# number of workers from the environment (SGCL_WORKERS=4 bench_http_hello)
add_executable(bench_http_hello ${CMAKE_CURRENT_SOURCE_DIR}/net/http_load/server.cpp)
target_link_libraries(bench_http_hello sgcl)
target_include_directories(bench_http_hello PRIVATE ${CMAKE_SOURCE_DIR})
target_compile_definitions(bench_http_hello PRIVATE NDEBUG)
if(MSVC)
    target_compile_options(bench_http_hello PRIVATE /O2)
else()
    target_compile_options(bench_http_hello PRIVATE -O2)
endif()
set_target_properties(bench_http_hello PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/benchmarks)
