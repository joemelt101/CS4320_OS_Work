cmake_minimum_required(VERSION 2.8)
project(libback_store)

find_library(bitmap_lib bitmap)

# Locate GTest
find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

set(CMAKE_CXX_FLAGS "-std=c++0x -Wall -Wextra -Wshadow -Werror -g -D_XOPEN_SOURCE=500")
set(CMAKE_C_FLAGS "-std=c99 -Wall -Wextra -Wshadow -Werror -g -D_XOPEN_SOURCE=500")

#Todo: At end, remove src/back_store.c and grab from public library .so object
add_executable(project_test test/tests.cpp)
add_library(back_store SHARED src/back_store.c)
target_link_libraries(project_test ${bitmap_lib} ${GTEST_LIBRARIES} pthread)

enable_testing()
add_test(NAME    project_test
         COMMAND project_test)

