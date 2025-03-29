cmake_minimum_required(VERSION 3.5)
project(SGCL_Tests LANGUAGES CXX)

enable_testing()

file(GLOB TEST_SOURCES "*.cpp")
add_executable(tests ${TEST_SOURCES} types.h)
target_link_libraries(tests gtest gtest_main sgcl)
target_include_directories(tests PRIVATE ${CMAKE_SOURCE_DIR})
add_test(NAME tests COMMAND tests)
