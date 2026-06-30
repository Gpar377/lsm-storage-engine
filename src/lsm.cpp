#include "../include/lsm.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <algorithm>

void LSMTree::put(const std::string& key, const std::string& value) {
    std::unique_lock<std::shared_mutex> lock(db_mutex);
    
    // 1. Write to WAL for durability
    wal->append(key, value, false);
    
    // 2. Insert into MemTable
    memtable.insert(key, value, false);
    
    // 3. Flush if size exceeds threshold
    // Using a simple count check; in production this would be memory-footprint based
    static size_t count = 0;
    if (++count >= max_memtable_size) {
        flush_memtable();
        count = 0;
    }
}

void LSMTree::del(const std::string& key) {
    std::unique_lock<std::shared_mutex> lock(db_mutex);
    
    // Log tombstone to WAL and MemTable
    wal->append(key, "", true);
    memtable.insert(key, "", true);
}

bool LSMTree::get(const std::string& key, std::string& value) {
    std::shared_lock<std::shared_mutex> lock(db_mutex);
    
    // 1. Search MemTable first
    bool is_tombstone = false;
    if (memtable.search(key, value, is_tombstone)) {
        if (is_tombstone) return false; // Deleted key
        return true;
    }

    // 2. Search Disk SSTables (Level 0 first, then deeper levels)
    for (int lvl = 0; lvl < levels.size(); lvl++) {
        // Iterate backward to search the newest SSTables first in case of overlapping keys
        for (auto it = levels[lvl].rbegin(); it != levels[lvl].rend(); ++it) {
            const auto& sstable = *it;
            if (!sstable.contains(key)) continue;

            // Search file contents
            std::ifstream file(sstable.filepath, std::ios::binary);
            if (!file.is_open()) continue;

            // Simple binary file structure:
            // [Num Elements (uint32_t)]
            // Repeats: [Key Len][Key][Val Len][Val][IsTombstone]
            uint32_t num_elements = 0;
            file.read(reinterpret_cast<char*>(&num_elements), sizeof(num_elements));

            for (uint32_t i = 0; i < num_elements; i++) {
                uint32_t k_len = 0;
                file.read(reinterpret_cast<char*>(&k_len), sizeof(k_len));
                std::string k(k_len, '\0');
                file.read(&k[0], k_len);

                uint32_t v_len = 0;
                file.read(reinterpret_cast<char*>(&v_len), sizeof(v_len));
                std::string v(v_len, '\0');
                file.read(&v[0], v_len);

                uint8_t tombstone = 0;
                file.read(reinterpret_cast<char*>(&tombstone), sizeof(tombstone));

                if (k == key) {
                    if (tombstone == 1) {
                        return false; // Deleted key
                    }
                    value = v;
                    return true;
                }
            }
        }
    }
    return false;
}

void LSMTree::flush_memtable() {
    auto items = memtable.get_all();
    if (items.empty()) return;

    std::string sstable_path = db_dir + "/sstable_" + std::to_string(sstable_counter++) + ".sst";
    std::ofstream file(sstable_path, std::ios::binary);
    if (!file.is_open()) return;

    // Write number of items
    uint32_t num_elements = items.size();
    file.write(reinterpret_cast<const char*>(&num_elements), sizeof(num_elements));

    SSTable sstable;
    sstable.filepath = sstable_path;
    sstable.level = 0;
    sstable.min_key = items.front().first;
    sstable.max_key = items.back().first;
    sstable.size = 0; // Filled below
    
    // Configure Bloom Filter size proportional to items
    sstable.filter = BloomFilter(std::max(1000, (int)items.size() * 10));

    for (const auto& item : items) {
        const std::string& k = item.first;
        const std::string& v = item.second.first;
        bool tombstone = item.second.second;

        uint32_t k_len = k.size();
        uint32_t v_len = v.size();
        uint8_t ts = tombstone ? 1 : 0;

        file.write(reinterpret_cast<const char*>(&k_len), sizeof(k_len));
        file.write(k.data(), k_len);
        file.write(reinterpret_cast<const char*>(&v_len), sizeof(v_len));
        file.write(v.data(), v_len);
        file.write(reinterpret_cast<const char*>(&ts), sizeof(ts));

        sstable.filter.insert(k);
    }
    
    file.close();
    sstable.size = std::filesystem::file_size(sstable_path);
    
    // Add to Level 0
    levels[0].push_back(sstable);
    
    // Reset memtable memory and clear WAL logs
    memtable.clear();
    wal->clear();

    // Trigger compaction check
    if (levels[0].size() >= 4) {
        compact();
    }
}
