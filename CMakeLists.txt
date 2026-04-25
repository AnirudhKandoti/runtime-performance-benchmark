cmake_minimum_required(VERSION 3.16)
project(runtime_performance_benchmark LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_executable(runtime_benchmark src/main.cpp)

target_compile_options(runtime_benchmark PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-O2 -Wall -Wextra -pedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/O2 /W4>
)
