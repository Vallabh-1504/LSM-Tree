cmake_minimum_required(VERSION 3.10)
project(LSM_Tree_engine LANGUAGES CXX)

# Require C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Define Build Options (Default OFF)
option(BENCHMARKS "Build the Google Benchmark executable" OFF)

# Make the Core Project source files a library
add_library(lsm_core
    src/KVStore.cpp
    src/SkipList.cpp
    src/SSTable.cpp
    src/WAL.cpp
    src/BloomFilter.cpp

    # vendor dependency
    vendor/MurmurHash3/MurmurHash3.cpp
)

# Specify where to find header files
target_include_directories(lsm_core PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/vendor/MurmurHash3
)

# Main Executable Target
add_executable(lsm_engine src/main.cpp)
target_link_libraries(lsm_engine PRIVATE lsm_core)

# Add a custom target to run the executable after building
add_custom_target(run
    COMMAND lsm_engine
    DEPENDS lsm_engine
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# Optional Benchmark Target 
if(BENCHMARKS)
    message(STATUS "Configuring Benchmarks")

    # for GBench package in system
    # find_package(benchmark REQUIRED)

    # for GBench using FetchContent
    include(FetchContent)
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "Disable benchmark testing" FORCE)
    FetchContent_Declare(
        googlebenchmark
        URL https://github.com/google/benchmark/archive/refs/tags/v1.8.3.zip
    )
    FetchContent_MakeAvailable(googlebenchmark)


    add_executable(lsm_benchmarks benchmarks/Benchmark.cpp)

    # Force compiler to optimize for RELEASE build
    target_compile_options(lsm_benchmarks PRIVATE -O3)
    target_compile_definitions(lsm_benchmarks PRIVATE NDEBUG)

    target_link_libraries(lsm_benchmarks PRIVATE
        lsm_core
        benchmark::benchmark
        pthread
    )  
endif()
