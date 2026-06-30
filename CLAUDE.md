# Claude Code Guidelines - LSM Storage Engine

## Project Overview
This repository contains a high-performance, concurrent LSM-tree storage engine in C++ incorporating MemTables, WALs, SSTables, Bloom Filters, and Compaction.

## Technology Stack
*   **C++17** (Standard GCC/Clang compilers)
*   **Build Tools:** CMake
*   **Concurrency:** Standard C++ threading (`std::thread`, `std::mutex`, `std::shared_mutex`)

## Coding Standards & Conventions
*   Ensure thread safety for writes and reads: use read-write locks (`std::shared_mutex`) on MemTable lists.
*   Enforce RAII for file handles. Ensure files are properly synced (`fsync`) when writing to the WAL to prevent data loss.
*   Optimize memory layouts in SSTables to allow memory-mapping (`mmap`) of indexes and bloom filters.
*   Use precise serialization formats for key-value entry schemas.

## Workflow Rules & Commands
*   **Compile Code:** `cmake -B build && cmake --build build`
*   **Run Test Suite:** `./build/tests/lsm_test` (includes concurrent read-write verification)
*   **Run Benchmark:** `./build/benchmarks/benchmark_perf`
