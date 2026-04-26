cmake_minimum_required(VERSION 3.20)
project(replicator CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    message(STATUS "ccache enabled: ${CCACHE_PROGRAM}")
endif()

add_compile_options(-Wall -Wextra -Wpedantic -O3 -march=native)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_library(replicator_core
    src/bf.cpp
    src/soup.cpp
)
target_include_directories(replicator_core PUBLIC src)
target_link_libraries(replicator_core PUBLIC Threads::Threads)

add_executable(replicator src/main.cpp)
target_link_libraries(replicator PRIVATE replicator_core)

# OpenMP enables libstdc++'s __gnu_parallel::sort for compute_stats at large pop.
find_package(OpenMP)
if(OpenMP_CXX_FOUND)
    target_compile_definitions(replicator PRIVATE _GLIBCXX_PARALLEL)
    target_link_libraries(replicator PRIVATE OpenMP::OpenMP_CXX)
endif()

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
    tests/test_bf_compare.cpp
    tests/test_soup.cpp
)
target_link_libraries(unit_tests PRIVATE replicator_core GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(unit_tests)
