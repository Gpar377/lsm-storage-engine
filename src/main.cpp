#include "../include/lsm.h"
#include <iostream>
#include <filesystem>
#include <cassert>
#include <chrono>

void cleanup_database_dir(const std::string& dir) {
    if (std::filesystem::exists(dir)) {
        std::filesystem::remove_all(dir);
    }
    std::filesystem::create_directories(dir);
}

int main() {
    std::string db_dir = "./data_test";
    cleanup_database_dir(db_dir);

    std::cout << "============================================" << std::endl;
    std::cout << "Starting LSM-Tree Storage Engine Verification" << std::endl;
    std::cout << "============================================" << std::endl;

    // 1. Initializing DB with MemTable threshold limit of 10 writes to force flushes quickly
    std::cout << "[Step 1] Initializing LSMTree..." << std::endl;
    auto db = std::make_unique<LSMTree>(db_dir, 10);

    // 2. Insert values (This will cross the threshold and create SSTables)
    std::cout << "[Step 2] Executing bulk insertion..." << std::endl;
    for (int i = 0; i < 45; i++) {
        db->put("key_" + std::to_string(i), "val_" + std::to_string(i));
    }
    std::cout << "-> Successfully inserted 45 keys." << std::endl;

    // Verify compaction was triggered (since 45 keys / 10 limit -> 4 flushes -> compaction triggered)
    auto current_levels = db->get_levels();
    std::cout << "-> Current SSTables at Level 0: " << current_levels[0].size() << std::endl;
    std::cout << "-> Current SSTables at Level 1: " << current_levels[1].size() << std::endl;
    assert(current_levels[1].size() > 0 && "Compaction should have consolidated L0 tables to L1!");

    // 3. Verify Reads
    std::cout << "[Step 3] Verifying read requests..." << std::endl;
    std::string test_val;
    bool found = db->get("key_23", test_val);
    assert(found && "Key 23 should exist!");
    assert(test_val == "val_23" && "Value should match val_23!");
    std::cout << "-> Read verification successful (key_23 -> " << test_val << ")." << std::endl;

    // 4. Verification of Tombstone/Delete operation
    std::cout << "[Step 4] Testing Deletes..." << std::endl;
    db->del("key_23");
    found = db->get("key_23", test_val);
    assert(!found && "Key 23 should be deleted via tombstone record!");
    std::cout << "-> Delete verification successful (key_23 correctly reported missing)." << std::endl;

    // 5. Crash Recovery Test (Verify WAL recovery)
    std::cout << "[Step 5] Simulating database crash..." << std::endl;
    // We write a value and delete the db instance without calling flush to memory.
    db->put("crash_key", "crash_safe_data");
    
    // Simulate crash by resetting pointer without graceful flush/shutdown
    db.reset(); 
    std::cout << "-> Database crashed. Simulating recovery boot..." << std::endl;

    // Reopen DB and check if crash_key exists from the WAL
    db = std::make_unique<LSMTree>(db_dir, 10);
    found = db->get("crash_key", test_val);
    assert(found && "WAL recovery failed to restore crash_key!");
    assert(test_val == "crash_safe_data" && "WAL recovery restored corrupted value!");
    std::cout << "-> WAL Recovery verification successful! (crash_key -> " << test_val << ")." << std::endl;

    std::cout << "\n============================================" << std::endl;
    std::cout << "ALL VERIFICATIONS COMPLETED SUCCESSFULLY! 🎉" << std::endl;
    std::cout << "============================================" << std::endl;

    return 0;
}
