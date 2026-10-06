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
        target_compile_options(${TARGET} PRIVATE -O3)
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

# bench_hash compares XXH32, XXH64 and XXH3 with libxxhash when it is found
# (SGCL_REFERENCE_ROOT, ../CMakeLists.txt)
sgcl_link_references(bench_hash SGCL_BENCH xxhash)

# bench_compress compares with the system's zlib
target_link_libraries(bench_compress z)

# bench_lz4 compares with liblz4 when it is found (SGCL_REFERENCE_ROOT, ../CMakeLists.txt);
# bench_snappy with liblz4's fast level, for orientation (no Snappy library is a reference here)
sgcl_link_references(bench_lz4 SGCL_BENCH lz4)
sgcl_link_references(bench_snappy SGCL_BENCH lz4)

# bench_zstd compares with libzstd when it is found (SGCL_REFERENCE_ROOT, ../CMakeLists.txt)
sgcl_link_references(bench_zstd SGCL_BENCH zstd)

# bench_brotli compares with libbrotli when it is found (SGCL_REFERENCE_ROOT, ../CMakeLists.txt)
sgcl_link_references(bench_brotli SGCL_BENCH brotli)

# bench_bzip2 compares with the system's libbz2
target_link_libraries(bench_bzip2 bz2)

# bench_lzma compares with liblzma when it is found (SGCL_XZ_ROOT, ../CMakeLists.txt)
if(SGCL_HAVE_LIBLZMA)
    target_include_directories(bench_lzma PRIVATE ${SGCL_XZ_ROOT}/include)
    target_link_libraries(bench_lzma ${SGCL_LIBLZMA})
    target_compile_definitions(bench_lzma PRIVATE SGCL_BENCH_LIBLZMA=1)
endif()

# bench_yaml compares with libyaml when it is found (SGCL_LIBYAML_ROOT, ../CMakeLists.txt)
if(SGCL_HAVE_LIBYAML)
    target_include_directories(bench_yaml PRIVATE ${SGCL_LIBYAML_ROOT}/include)
    target_link_libraries(bench_yaml ${SGCL_LIBYAML})
    target_compile_definitions(bench_yaml PRIVATE SGCL_BENCH_LIBYAML=1)
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
# bench_crypto's blake3 has the authors' C library as its reference side when it is found (../CMakeLists.txt)
if(SGCL_HAVE_BLAKE3)
    target_include_directories(bench_crypto PRIVATE ${SGCL_BLAKE3_ROOT}/include)
    target_link_libraries(bench_crypto ${SGCL_LIBBLAKE3})
    target_compile_definitions(bench_crypto PRIVATE SGCL_BENCH_BLAKE3=1)
endif()

# bench_codec compares the codec module with libpng, libjpeg-turbo, libwebp and
# giflib when all four are found (Homebrew's by default; SGCL_CODEC_LIBS_ROOT
# for another prefix of the four); without them it has the module's side alone
if(APPLE)
    set(SGCL_CODEC_LIBS_DEFAULT "/opt/homebrew/opt")
else()
    set(SGCL_CODEC_LIBS_DEFAULT "/usr")
endif()
set(SGCL_CODEC_LIBS_ROOT "${SGCL_CODEC_LIBS_DEFAULT}" CACHE PATH "Where libpng, jpeg-turbo, webp and giflib are installed (Homebrew's opt, or a common prefix)")
set(SGCL_CODEC_LIBS_FOUND TRUE)
set(SGCL_CODEC_LIB_DIRS "")
foreach(PAIR "libpng:png16:png.h" "jpeg-turbo:turbojpeg:turbojpeg.h" "webp:webp:webp/decode.h" "giflib:gif:gif_lib.h")
    string(REPLACE ":" ";" P ${PAIR})
    list(GET P 0 PKG)
    list(GET P 1 LIB)
    list(GET P 2 HEADER)
    unset(SGCL_CODEC_LIB CACHE)
    find_library(SGCL_CODEC_LIB NAMES ${LIB} HINTS "${SGCL_CODEC_LIBS_ROOT}/${PKG}/lib" "${SGCL_CODEC_LIBS_ROOT}/lib" NO_DEFAULT_PATH)
    if(SGCL_CODEC_LIB AND (EXISTS "${SGCL_CODEC_LIBS_ROOT}/${PKG}/include/${HEADER}" OR EXISTS "${SGCL_CODEC_LIBS_ROOT}/include/${HEADER}"))
        target_link_libraries(bench_codec ${SGCL_CODEC_LIB})
        target_include_directories(bench_codec PRIVATE "${SGCL_CODEC_LIBS_ROOT}/${PKG}/include" "${SGCL_CODEC_LIBS_ROOT}/include")
    else()
        set(SGCL_CODEC_LIBS_FOUND FALSE)
    endif()
endforeach()
unset(SGCL_CODEC_LIB CACHE)
if(SGCL_CODEC_LIBS_FOUND)
    target_compile_definitions(bench_codec PRIVATE SGCL_BENCH_CODEC_LIBS=1)
else()
    message(STATUS "SGCL: libpng, jpeg-turbo, webp or giflib not found at SGCL_CODEC_LIBS_ROOT (${SGCL_CODEC_LIBS_ROOT}): bench_codec has the module's side alone")
endif()

# libtiff for bench_codec's TIFF cases, when found (Homebrew's)
find_library(SGCL_LIBTIFF NAMES tiff HINTS "${SGCL_CODEC_LIBS_ROOT}/libtiff/lib" "${SGCL_CODEC_LIBS_ROOT}/lib" NO_DEFAULT_PATH)
if(SGCL_LIBTIFF AND (EXISTS "${SGCL_CODEC_LIBS_ROOT}/libtiff/include/tiffio.h" OR EXISTS "${SGCL_CODEC_LIBS_ROOT}/include/tiffio.h"))
    target_link_libraries(bench_codec ${SGCL_LIBTIFF})
    target_include_directories(bench_codec PRIVATE "${SGCL_CODEC_LIBS_ROOT}/libtiff/include" "${SGCL_CODEC_LIBS_ROOT}/include")
    target_compile_definitions(bench_codec PRIVATE SGCL_BENCH_LIBTIFF=1)
endif()

# CoreImage for bench_codec's QR case (the system's QR generator)
if(APPLE)
    target_link_libraries(bench_codec "-framework CoreImage" "-framework Foundation")
endif()

# libjxl for bench_codec's JPEG XL case, when found (Homebrew's jpeg-xl)
find_library(SGCL_LIBJXL NAMES jxl HINTS "${SGCL_CODEC_LIBS_ROOT}/jpeg-xl/lib" NO_DEFAULT_PATH)
find_library(SGCL_LIBJXL_THREADS NAMES jxl_threads HINTS "${SGCL_CODEC_LIBS_ROOT}/jpeg-xl/lib" NO_DEFAULT_PATH)
if(SGCL_LIBJXL AND SGCL_LIBJXL_THREADS AND EXISTS "${SGCL_CODEC_LIBS_ROOT}/jpeg-xl/include/jxl/decode.h")
    target_link_libraries(bench_codec ${SGCL_LIBJXL} ${SGCL_LIBJXL_THREADS})
    target_include_directories(bench_codec PRIVATE "${SGCL_CODEC_LIBS_ROOT}/jpeg-xl/include")
    target_compile_definitions(bench_codec PRIVATE SGCL_BENCH_LIBJXL=1)
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
    target_compile_options(bench_http_hello PRIVATE -O3)
endif()
set_target_properties(bench_http_hello PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/benchmarks)

# The reverse proxy load test's proxy (net/http_load/proxy.cpp, run_proxy.sh
# there): http::reverse_proxy in front of bench_http_hello
add_executable(bench_http_proxy ${CMAKE_CURRENT_SOURCE_DIR}/net/http_load/proxy.cpp)
target_link_libraries(bench_http_proxy sgcl)
target_include_directories(bench_http_proxy PRIVATE ${CMAKE_SOURCE_DIR})
target_compile_definitions(bench_http_proxy PRIVATE NDEBUG)
if(MSVC)
    target_compile_options(bench_http_proxy PRIVATE /O2)
else()
    target_compile_options(bench_http_proxy PRIVATE -O3)
endif()
set_target_properties(bench_http_proxy PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/benchmarks)

# Its client's side (net/http_load/postbench.cpp): the module's client
# POSTing a body, its requests per second and its CPU per request
add_executable(bench_http_post ${CMAKE_CURRENT_SOURCE_DIR}/net/http_load/postbench.cpp)
target_link_libraries(bench_http_post sgcl)
target_include_directories(bench_http_post PRIVATE ${CMAKE_SOURCE_DIR})
target_compile_definitions(bench_http_post PRIVATE NDEBUG)
if(MSVC)
    target_compile_options(bench_http_post PRIVATE /O2)
else()
    target_compile_options(bench_http_post PRIVATE -O3)
endif()
set_target_properties(bench_http_post PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/benchmarks)

# A gzip stream written straight to a connection (net/gzip_stream; go/main.go
# is Go's side and the load for both)
add_executable(bench_gzip_stream ${CMAKE_CURRENT_SOURCE_DIR}/net/gzip_stream/server.cpp)
target_link_libraries(bench_gzip_stream sgcl)
target_include_directories(bench_gzip_stream PRIVATE ${CMAKE_SOURCE_DIR})
target_compile_definitions(bench_gzip_stream PRIVATE NDEBUG)
if(MSVC)
    target_compile_options(bench_gzip_stream PRIVATE /O2)
else()
    target_compile_options(bench_gzip_stream PRIVATE -O3)
endif()
set_target_properties(bench_gzip_stream PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/benchmarks)
