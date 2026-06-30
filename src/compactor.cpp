#include "../include/lsm.h"
#include <iostream>
#include <map>
#include <vector>
#include <fstream>
#include <filesystem>
#include <algorithm>

// Compaction flattens Level 0 tables by merge-sorting overlapping keys into Level 1
void LSMTree::compact() {
    // Basic Size-Tiered / Leveled Compaction Trigger (Level 0 -> Level 1)
    if (levels[0].size() < 4) return;

    std::cout << "[Compactor] Initiating Level 0 compaction to Level 1..." << std::endl;

    // Merge map to sort and deduplicate keys across Level 0 tables (newest entries overwrite oldest)
    std::map<std::string, std::pair<std::string, bool>> merged_data;

    // Sort order: Read oldest SSTables first, newer ones last, so newer updates override
    for (const auto& sstable : levels[0]) {
        std::ifstream file(sstable.filepath, std::ios::binary);
        if (!file.is_open()) continue;

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

            merged_data[k] = {v, tombstone == 1};
        }
        file.close();
    }

    // Write merged contents to a new Level 1 SSTable
    std::string new_sstable_path = db_dir + "/sstable_l1_" + std::to_string(sstable_counter++) + ".sst";
    std::ofstream out(new_sstable_path, std::ios::binary);
    if (!out.is_open()) return;

    uint32_t num_elements = merged_data.size();
    out.write(reinterpret_cast<const char*>(&num_elements), sizeof(num_elements));

    SSTable new_sstable;
    new_sstable.filepath = new_sstable_path;
    new_sstable.level = 1;
    new_sstable.min_key = merged_data.begin()->first;
    new_sstable.max_key = merged_data.rbegin()->first;
    new_sstable.filter = BloomFilter(std::max(1000, (int)merged_data.size() * 10));

    for (const auto& pair : merged_data) {
        const std::string& k = pair.first;
        const std::string& v = pair.second.first;
        bool tombstone = pair.second.second;

        uint32_t k_len = k.size();
        uint32_t v_len = v.size();
        uint8_t ts = tombstone ? 1 : 0;

        out.write(reinterpret_cast<const char*>(&k_len), sizeof(k_len));
        out.write(k.data(), k_len);
        out.write(reinterpret_cast<const char*>(&v_len), sizeof(v_len));
        out.write(v.data(), v_len);
        out.write(reinterpret_cast<const char*>(&ts), sizeof(ts));

        new_sstable.filter.insert(k);
    }
    out.close();
    new_sstable.size = std::filesystem::file_size(new_sstable_path);

    // Clean up disk files of compacted Level 0 tables
    for (const auto& sstable : levels[0]) {
        std::filesystem::remove(sstable.filepath);
    }
    levels[0].clear();

    // Register new Level 1 table
    levels[1].push_back(new_sstable);
    std::cout << "[Compactor] Level 1 compaction completed successfully. New table: " << new_sstable_path << std::endl;
}
