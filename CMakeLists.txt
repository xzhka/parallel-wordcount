cmake_minimum_required(VERSION 3.16)
project(parallel-wordcount LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)  # timing a Debug build is meaningless
endif()

find_package(Threads REQUIRED)

foreach(prog word_count_solo word_count_parallel function_ratio)
  add_executable(${prog} ${prog}/Source.cpp)
  target_link_libraries(${prog} PRIVATE Threads::Threads)
  if(MSVC)
    target_compile_options(${prog} PRIVATE /W4)
  else()
    target_compile_options(${prog} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endforeach()
