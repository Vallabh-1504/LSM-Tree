cmake_minimum_required(VERSION 3.10)
project(LSM_Tree_engine LANGUAGES CXX)

# Require C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Define the executable and add source files
add_executable(lsm_engine
    src/main.cpp
    src/KVStore/KVStore.cpp
    src/Memtable/SkipList.cpp
    src/SStable/SSTable.cpp
    src/WAL/WAL.cpp
    src/BloomFilter/BloomFilter.cpp
)

# Specify where to find header files
target_include_directories(lsm_engine PRIVATE
    src/KVStore
    src/Memtable
    src/SStable
    src/WAL
    src/BloomFilter
)

# Add a custom target to run the executable after building
add_custom_target(run
    COMMAND lsm_engine
    DEPENDS lsm_engine
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
