# One test program per module, each over the module's directory, so that a
# module is built and run on its own: cmake --build <dir> --target
# tests_async && ctest --test-dir <dir> -R async; tests_Sgcl covers the
# second interface. main.cpp (the listener
# that clears the stack between tests) and types.h are shared. The target
# `tests` builds them all.
add_custom_target(tests)

function(sgcl_test_module MODULE)
    set(TARGET tests_${MODULE})
    file(GLOB SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/${MODULE}/*.cpp")
    list(REMOVE_ITEM SOURCES ${ARGN})
    add_executable(${TARGET} main.cpp types.h ${SOURCES})
    target_link_libraries(${TARGET} gtest sgcl)
    target_include_directories(${TARGET} PRIVATE ${CMAKE_SOURCE_DIR})
    add_test(NAME ${TARGET} COMMAND ${TARGET})
    add_dependencies(tests ${TARGET})
endfunction()

# static_init*.cpp create a managed object in a global's initializer, before
# main: a program of its own, so that the object living for the whole run
# does not disturb the live-object counts the suite asserts.
sgcl_test_module(core
    "${CMAKE_CURRENT_SOURCE_DIR}/core/static_init.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/static_init_factory.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/teardown.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/limit_deadlock.cpp")
sgcl_test_module(containers)
sgcl_test_module(concurrent)
sgcl_test_module(async)
sgcl_test_module(Sgcl)

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
