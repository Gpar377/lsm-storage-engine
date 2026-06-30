#include "../include/lsm.h"
#include <iostream>
#include <cstring>
#include <filesystem>

void WAL::append(const std::string& key, const std::string& value, bool is_tombstone) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!file.is_open()) return;

    uint32_t key_len = key.size();
    uint32_t val_len = value.size();
    uint8_t tombstone = is_tombstone ? 1 : 0;

    file.write(reinterpret_cast<const char*>(&key_len), sizeof(key_len));
    file.write(key.data(), key_len);
    file.write(reinterpret_cast<const char*>(&val_len), sizeof(val_len));
    file.write(value.data(), val_len);
    file.write(reinterpret_cast<const char*>(&tombstone), sizeof(tombstone));
    file.flush();
}

bool WAL::recover(SkipList<std::string, std::string>& memtable) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!std::filesystem::exists(filepath)) return false;

    std::ifstream in(filepath, std::ios::in | std::ios::binary);
    if (!in.is_open()) return false;

    while (in.peek() != EOF) {
        uint32_t key_len = 0;
        if (!in.read(reinterpret_cast<char*>(&key_len), sizeof(key_len))) break;

        std::string key(key_len, '\0');
        if (!in.read(&key[0], key_len)) break;

        uint32_t val_len = 0;
        if (!in.read(reinterpret_cast<char*>(&val_len), sizeof(val_len))) break;

        std::string value(val_len, '\0');
        if (!in.read(&value[0], val_len)) break;

        uint8_t tombstone = 0;
        if (!in.read(reinterpret_cast<char*>(&tombstone), sizeof(tombstone))) break;

        memtable.insert(key, value, tombstone == 1);
    }

    in.close();
    return true;
}

void WAL::clear() {
    std::lock_guard<std::mutex> lock(mutex);
    if (file.is_open()) file.close();
    
    // Truncate the file to 0 size
    file.open(filepath, std::ios::out | std::ios::trunc | std::ios::binary);
}
