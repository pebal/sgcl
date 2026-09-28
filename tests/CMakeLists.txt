# One test program per module, each over the module's directory, so that a
# module is built and run on its own: cmake --build <dir> --target
# tests_async && ctest --test-dir <dir> -R async. main.cpp (the listener
# that clears the stack between tests) and types.h are shared. The target
# `tests` builds them all.
add_custom_target(tests)

# The program `TARGET` of a module's sources (less those given after it)
function(sgcl_test_program TARGET MODULE)
    file(GLOB SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/${MODULE}/*.cpp")
    list(REMOVE_ITEM SOURCES ${ARGN})
    add_executable(${TARGET} main.cpp types.h ${SOURCES})
    target_link_libraries(${TARGET} gtest sgcl)
    target_include_directories(${TARGET} PRIVATE ${CMAKE_SOURCE_DIR})
    add_test(NAME ${TARGET} COMMAND ${TARGET})
    add_dependencies(tests ${TARGET})
endfunction()

function(sgcl_test_module MODULE)
    get_filename_component(NAME ${MODULE} NAME)
    sgcl_test_program(tests_${NAME} ${MODULE} ${ARGN})
endfunction()

# static_init*.cpp create a managed object in a global's initializer, before
# main: a program of its own, so that the object living for the whole run
# does not disturb the live-object counts the suite asserts.
sgcl_test_module(core
    "${CMAKE_CURRENT_SOURCE_DIR}/core/static_init.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/static_init_factory.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/teardown.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/limit_deadlock.cpp")
sgcl_test_module(containers)   # core's containers (sgcl/core), a program of their own
sgcl_test_module(immutable)
sgcl_test_module(concurrent)
sgcl_test_module(async "${CMAKE_CURRENT_SOURCE_DIR}/async/env_settings.cpp")   # env_settings.cpp: a program of its own, below
sgcl_test_module(io)
sgcl_test_module(txt)
sgcl_test_module(math)
sgcl_test_module(hash)
# hash and crypto have a road on the processor's own instructions and one in
# plain C++, chosen at compile time: the _portable programs are the same
# sources built for the second road, so that every vector is checked on both
sgcl_test_program(tests_hash_portable hash)
target_compile_definitions(tests_hash_portable PRIVATE SGCL_HASH_PORTABLE)
sgcl_test_module(compress)
target_link_libraries(tests_compress z bz2)   # zlib and libbz2 of the system: oracles of the tests only
if(SGCL_HAVE_LIBLZMA)   # liblzma, the LZMA tests' oracle (../CMakeLists.txt)
    target_include_directories(tests_compress PRIVATE ${SGCL_XZ_ROOT}/include)
    target_link_libraries(tests_compress ${SGCL_LIBLZMA})
    target_compile_definitions(tests_compress PRIVATE SGCL_TEST_LIBLZMA=1)
endif()
if(SGCL_HAVE_OPENSSL)   # the crypto tests hold the module against OpenSSL (../CMakeLists.txt)
    sgcl_test_module(crypto)
    sgcl_test_program(tests_crypto_portable crypto)
    target_compile_definitions(tests_crypto_portable PRIVATE SGCL_CRYPTO_PORTABLE)
    foreach(T tests_crypto tests_crypto_portable)
        target_include_directories(${T} PRIVATE ${SGCL_OPENSSL_ROOT}/include)
        target_link_libraries(${T} ${SGCL_LIBCRYPTO})
    endforeach()
endif()
sgcl_test_module(time)
sgcl_test_module(encoding)
sgcl_test_module(net)
sgcl_test_module(net/http)

add_executable(static_init_test core/static_init.cpp core/static_init_factory.cpp core/static_init.h)
target_link_libraries(static_init_test gtest sgcl)
target_include_directories(static_init_test PRIVATE ${CMAKE_SOURCE_DIR})
add_test(NAME static_init_test COMMAND static_init_test)
add_dependencies(tests static_init_test)

# teardown.cpp: the library used from a static destructor, after the main
# thread's Thread is gone (its object is the process, not a test case): once
# with a global unique_ptr, once with a thread_local destroyed after the Thread.
add_executable(teardown_test core/teardown.cpp)
target_link_libraries(teardown_test sgcl)
target_include_directories(teardown_test PRIVATE ${CMAKE_SOURCE_DIR})
add_test(NAME teardown_test COMMAND teardown_test)
add_dependencies(tests teardown_test)

add_executable(teardown_thread_local_test core/teardown.cpp)
target_compile_definitions(teardown_thread_local_test PRIVATE TEARDOWN_THREAD_LOCAL=1)
target_link_libraries(teardown_thread_local_test sgcl)
target_include_directories(teardown_thread_local_test PRIVATE ${CMAKE_SOURCE_DIR})
add_test(NAME teardown_thread_local_test COMMAND teardown_thread_local_test)
add_dependencies(tests teardown_thread_local_test)

# limit_deadlock.cpp: a destructor allocating at the memory ceiling on the
# sweep waits for its own cycle; the symptom is a hang, hence the timeout.
add_executable(limit_deadlock_test core/limit_deadlock.cpp)
target_link_libraries(limit_deadlock_test sgcl)
target_include_directories(limit_deadlock_test PRIVATE ${CMAKE_SOURCE_DIR})
add_test(NAME limit_deadlock_test COMMAND limit_deadlock_test)
set_tests_properties(limit_deadlock_test PROPERTIES TIMEOUT 30)
add_dependencies(tests limit_deadlock_test)

# env_settings.cpp: the settings of the environment (SGCL_WORKERS,
# SGCL_WORKER_SPIN_US, SGCL_BLOCKING_THREADS, SGCL_MEMORY_LIMIT), each read
# once where it is first needed, so each case is a process of its own with
# the variables set before it starts
add_executable(env_settings_test async/env_settings.cpp)
target_link_libraries(env_settings_test sgcl)
target_include_directories(env_settings_test PRIVATE ${CMAKE_SOURCE_DIR})
add_dependencies(tests env_settings_test)
add_test(NAME env_settings_workers COMMAND env_settings_test workers)
set_tests_properties(env_settings_workers PROPERTIES ENVIRONMENT "SGCL_WORKERS=3;SGCL_WORKER_SPIN_US=5;SGCL_BLOCKING_THREADS=7" TIMEOUT 60)
add_test(NAME env_settings_explicit COMMAND env_settings_test explicit)
set_tests_properties(env_settings_explicit PROPERTIES ENVIRONMENT "SGCL_WORKERS=3;SGCL_WORKER_SPIN_US=5;SGCL_BLOCKING_THREADS=7" TIMEOUT 60)
add_test(NAME env_settings_memory COMMAND env_settings_test memory 67108864)
set_tests_properties(env_settings_memory PROPERTIES ENVIRONMENT "SGCL_MEMORY_LIMIT=64M" TIMEOUT 60)
add_test(NAME env_settings_percent COMMAND env_settings_test percent 50)
set_tests_properties(env_settings_percent PROPERTIES ENVIRONMENT "SGCL_MEMORY_LIMIT=50%" TIMEOUT 60)
add_test(NAME env_settings_invalid COMMAND env_settings_test invalid)
set_tests_properties(env_settings_invalid PROPERTIES ENVIRONMENT "SGCL_WORKERS=abc;SGCL_MEMORY_LIMIT=12Q" TIMEOUT 60)
