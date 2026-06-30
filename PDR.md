# LSM-Tree Storage Engine
A high-performance log-structured merge-tree (LSM-Tree) storage engine implemented in C++. This project replicates the storage architectures used in modern databases like RocksDB and LevelDB, optimized for write-heavy workloads through append-only writes, in-memory caching, and asynchronous compaction.

## Proposed Git Repo Name
`lsm-storage-engine`

## Architecture & Scope
*   **Write-Ahead Log (WAL):** Sequential append-only file on disk to guarantee durability. All writes are committed to the WAL before updating the active memory table.
*   **MemTable (In-Memory Buffer):** An active, sorted structure (implemented as a concurrent SkipList or Red-Black Tree) holding writes. Once size limit is reached, it becomes read-only and is flushed to disk.
*   **SSTables (Sorted String Tables):** Immutable files on disk containing key-value pairs sorted by key. Includes:
    *   **Index Block:** Stored at the end of the SSTable for binary search lookup.
    *   **Bloom Filters:** Bit arrays stored in the index block to quickly reject queries for non-existent keys, eliminating disk reads.
*   **Compaction Engine:** Background threads performing compaction (Size-Tiered or Leveled compaction). Merges overlapping SSTables, removes duplicate keys, and purges tombstoned (deleted) keys.
*   **Read Path:** Search hierarchy: MemTable -> Read-Only MemTables -> SSTables Level 0 -> SSTables Level 1+ (accelerated by Bloom Filters).

## Target Milestones
1. WAL and MemTable (SkipList) implementation with basic write/read logic.
2. SSTable file layout format (Data blocks, Index blocks, Bloom Filters) and flush routine.
3. Tombstone handling and Read Path searching.
4. Background compaction thread scheduler merging Level 0 files to Level 1.
