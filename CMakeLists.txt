cmake_minimum_required(VERSION 3.20)
project(replicator CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(-Wall -Wextra -Wpedantic -O3)

add_library(replicator_core
    src/bf.cpp
    src/soup.cpp
)
target_include_directories(replicator_core PUBLIC src)

add_executable(replicator src/main.cpp)
target_link_libraries(replicator PRIVATE replicator_core)

enable_testing()
include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
    URL_HASH SHA256=7b42b4d6ed48810c5362c265a17faebe90dc2373c885e5216439d37927f02926
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(unit_tests
    tests/test_bf.cpp
    tests/test_soup.cpp
)
target_link_libraries(unit_tests PRIVATE replicator_core GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(unit_tests)
