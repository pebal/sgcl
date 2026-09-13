file(GLOB TEST_SOURCES "*.cpp")
# static_init*.cpp create a managed object in a global's initializer, before
# main: a program of its own, so that the object living for the whole run
# does not disturb the live-object counts the suite asserts.
list(REMOVE_ITEM TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/static_init.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/static_init_factory.cpp")
add_executable(tests ${TEST_SOURCES} types.h)
target_link_libraries(tests gtest sgcl)
target_include_directories(tests PRIVATE ${CMAKE_SOURCE_DIR})
add_test(NAME tests COMMAND tests)

add_executable(static_init_test static_init.cpp static_init_factory.cpp static_init.h)
target_link_libraries(static_init_test gtest sgcl)
target_include_directories(static_init_test PRIVATE ${CMAKE_SOURCE_DIR})
add_test(NAME static_init_test COMMAND static_init_test)
