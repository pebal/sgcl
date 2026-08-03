cmake_minimum_required(VERSION 3.5)
project(SGCL_Tests LANGUAGES CXX)

enable_testing()

file(GLOB TEST_SOURCES "*.cpp")

# static_init.cpp/static_init_factory.cpp reproduce a tracked object being
# constructed during C++ static/dynamic initialization (before main()). This
# exercises whole-program/process startup behavior and deliberately keeps a
# tracked object alive for the process's entire lifetime, so it is built as
# its own standalone binary rather than folded into the shared `tests`
# executable below, which asserts exact collector::get_live_object_count()
# baselines throughout and must not share collector/thread state with it.
list(REMOVE_ITEM TEST_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/static_init.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/static_init_factory.cpp"
)

add_executable(tests ${TEST_SOURCES} types.h)
target_link_libraries(tests gtest gtest_main sgcl)
target_include_directories(tests PRIVATE ${CMAKE_SOURCE_DIR})
add_test(NAME tests COMMAND tests)

add_executable(static_init_test
    static_init.cpp
    static_init_factory.cpp
    static_init.h
)
target_link_libraries(static_init_test gtest gtest_main sgcl)
target_include_directories(static_init_test PRIVATE ${CMAKE_SOURCE_DIR})
add_test(NAME static_init_test COMMAND static_init_test)
