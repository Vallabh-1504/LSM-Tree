cmake_minimum_required(VERSION 3.10)
project(LSM_Tree_engine LANGUAGES CXX)

# Require C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Define the executable and add source files
add_executable(lsm_engine
    src/main.cpp
    src/KVStore.cpp
    src/SkipList.cpp
    src/SSTable.cpp
    src/WAL.cpp
    src/BloomFilter.cpp

    # vendored dependency
    vendor/MurmurHash3/MurmurHash3.cpp
)

# Specify where to find header files
target_include_directories(lsm_engine PRIVATE 
    include
    vendor/MurmurHash3)

# Add a custom target to run the executable after building
add_custom_target(run
    COMMAND lsm_engine
    DEPENDS lsm_engine
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
