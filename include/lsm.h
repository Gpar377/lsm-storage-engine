#ifndef LSM_H
#define LSM_H

#include <string>
#include <vector>
#include <fstream>
#include <mutex>
#include <shared_mutex>
#include <memory>
#include "skiplist.h"

// Bloom Filter implementation for fast key checks before SSTable reads
class BloomFilter {
private:
    std::vector<bool> bits;
    int num_hashes;

    size_t hash(const std::string& key, int seed) const {
        size_t h = 0x811c9dc5;
        for (char c : key) {
            h = (h ^ c ^ seed) * 0x01000193;
        }
        return h;
    }

public:
    BloomFilter(int size = 10000, int hashes = 4) 
        : bits(size, false), num_hashes(hashes) {}

    void insert(const std::string& key) {
        for (int i = 0; i < num_hashes; i++) {
            bits[hash(key, i) % bits.size()] = true;
        }
    }

    bool contains(const std::string& key) const {
        for (int i = 0; i < num_hashes; i++) {
            if (!bits[hash(key, i) % bits.size()]) return false;
        }
        return true;
    }
};

// Write-Ahead Log (WAL) to guarantee durability on system crashes
class WAL {
private:
    std::string filepath;
    std::ofstream file;
    std::mutex mutex;

public:
    WAL(const std::string& path) : filepath(path) {
        file.open(filepath, std::ios::out | std::ios::app | std::ios::binary);
    }

    ~WAL() {
        if (file.is_open()) file.close();
    }

    void append(const std::string& key, const std::string& value, bool is_tombstone);
    bool recover(SkipList<std::string, std::string>& memtable);
    void clear();
};

// SSTable metadata representation
struct SSTable {
    std::string filepath;
    int level;
    std::string min_key;
    std::string max_key;
    size_t size;
    BloomFilter filter;

    bool contains(const std::string& key) const {
        if (key < min_key || key > max_key) return false;
        return filter.contains(key);
    }
};

// Core LSM-Tree coordinator engine
class LSMTree {
private:
    std::string db_dir;
    size_t max_memtable_size; // Max keys in MemTable before flushing to SSTable
    
    // Active Memory structure
    SkipList<std::string, std::string> memtable;
    std::unique_ptr<WAL> wal;
    
    // Immutable Disk levels
    std::vector<std::vector<SSTable>> levels; // Index 0 = Level 0, Index 1 = Level 1...
    
    mutable std::shared_mutex db_mutex;
    int sstable_counter;

    void flush_memtable();
    void compact();

public:
    LSMTree(const std::string& dir, size_t max_mem_size = 1000) 
        : db_dir(dir), max_memtable_size(max_mem_size), sstable_counter(0) {
        levels.resize(4); // Supports Level 0 to Level 3
        wal = std::make_unique<WAL>(db_dir + "/wal.log");
        wal->recover(memtable);
    }

    ~LSMTree() {
        // Safe shutdown does not flush to verify WAL recovery capabilities on next boot
    }

    void put(const std::string& key, const std::string& value);
    bool get(const std::string& key, std::string& value);
    void del(const std::string& key);
    
    // Internal helper exports
    std::vector<std::vector<SSTable>> get_levels() const { return levels; }
};

#endif // LSM_H
