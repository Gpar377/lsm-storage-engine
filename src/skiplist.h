#ifndef SKIPLIST_H
#define SKIPLIST_H

#include <iostream>
#include <vector>
#include <string>
#include <shared_mutex>
#include <random>
#include <memory>

template <typename Key, typename Value>
class SkipList {
private:
    struct Node {
        Key key;
        Value value;
        bool is_tombstone; // To support deletions
        std::vector<std::shared_ptr<Node>> forward;

        Node(Key k, Value v, int level, bool tombstone = false) 
            : key(k), value(v), is_tombstone(tombstone), forward(level + 1, nullptr) {}
    };

    int max_level;
    float p;
    int level;
    std::shared_ptr<Node> header;
    
    // Thread safety locks
    mutable std::shared_mutex mutex;

    // Random level generator
    int random_level() {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_real_distribution<float> dis(0.0, 1.0);
        
        int lvl = 0;
        while (dis(gen) < p && lvl < max_level) {
            lvl++;
        }
        return lvl;
    }

public:
    SkipList(int max_lvl = 16, float prob = 0.5) 
        : max_level(max_lvl), p(prob), level(0) {
        // Initialize header node with default key and value
        header = std::make_shared<Node>(Key(), Value(), max_level);
    }

    // Insert key-value pair or update if exists
    void insert(const Key& key, const Value& value, bool is_tombstone = false) {
        std::unique_lock<std::shared_mutex> lock(mutex);
        
        std::vector<std::shared_ptr<Node>> update(max_level + 1, nullptr);
        auto current = header;

        for (int i = level; i >= 0; i--) {
            while (current->forward[i] && current->forward[i]->key < key) {
                current = current->forward[i];
            }
            update[i] = current;
        }

        current = current->forward[0];

        // Key already exists, update value and tombstone flag
        if (current && current->key == key) {
            current->value = value;
            current->is_tombstone = is_tombstone;
            return;
        }

        // Key does not exist, insert new node
        int r_level = random_level();
        if (r_level > level) {
            for (int i = level + 1; i <= r_level; i++) {
                update[i] = header;
            }
            level = r_level;
        }

        auto new_node = std::make_shared<Node>(key, value, r_level, is_tombstone);
        for (int i = 0; i <= r_level; i++) {
            new_node->forward[i] = update[i]->forward[i];
            update[i]->forward[i] = new_node;
        }
    }

    // Retrieve value corresponding to key. Returns false if not found or deleted (tombstone)
    bool search(const Key& key, Value& value, bool& is_tombstone) const {
        std::shared_lock<std::shared_mutex> lock(mutex);
        
        auto current = header;
        for (int i = level; i >= 0; i--) {
            while (current->forward[i] && current->forward[i]->key < key) {
                current = current->forward[i];
            }
        }
        current = current->forward[0];

        if (current && current->key == key) {
            value = current->value;
            is_tombstone = current->is_tombstone;
            return true;
        }
        return false;
    }

    // Get all sorted key-value pairs inside the SkipList (for SSTable flushing)
    std::vector<std::pair<Key, std::pair<Value, bool>>> get_all() const {
        std::shared_lock<std::shared_mutex> lock(mutex);
        
        std::vector<std::pair<Key, std::pair<Value, bool>>> items;
        auto current = header->forward[0];
        while (current) {
            items.push_back({current->key, {current->value, current->is_tombstone}});
            current = current->forward[0];
        }
        return items;
    }

    void clear() {
        std::unique_lock<std::shared_mutex> lock(mutex);
        for (int i = 0; i <= max_level; i++) {
            header->forward[i] = nullptr;
        }
        level = 0;
    }
};

#endif // SKIPLIST_H
