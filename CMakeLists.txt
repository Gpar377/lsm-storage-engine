cmake_minimum_required(VERSION 3.12)
project(LSMStorageEngine CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add Include Directories
include_directories(include)

# Source Files
set(SOURCES
    src/wal.cpp
    src/lsm.cpp
    src/compactor.cpp
    src/main.cpp
)

# Executable Target
add_executable(lsm_engine ${SOURCES})
