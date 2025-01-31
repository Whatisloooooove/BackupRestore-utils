cmake_minimum_required(VERSION 3.10)
project(MyBackupAndRestore)

set(CMAKE_CXX_STANDARD 17)

find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_executable(my_backup my_backup/my_backup.cpp)
add_executable(my_restore my_restore/my_restore.cpp)

add_executable(run_tests tests.cpp)
target_link_libraries(run_tests GTest::gtest GTest::gtest_main pthread) 

enable_testing()
add_test(NAME AllTests COMMAND run_tests)

install(TARGETS my_backup my_restore run_tests
        RUNTIME DESTINATION bin)