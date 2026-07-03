#include <iostream>
#include <random>

#include "SkipList.hpp"

namespace LSM {

SkipList::SkipList(int max_level, float probability) : max_level(max_level), probability(probability), current_level(0), rng_(std::random_device{}()){
    head = new SkipListNode("", "", max_level, false);
}

SkipList::~SkipList(){
    // Traverse at 0th level to ensure we delete all nodes
    SkipListNode* current = head;
    while(current != nullptr){
        SkipListNode* next = current->forward[0];
        delete current;
        current = next;
    }
}
int SkipList::randomLevel(){
    // Generate a perfect random
    std::uniform_real_distribution<float> dis(0.0f, 1.0f);

    int lvl = 0;
    while(dis(rng_) < probability && lvl < max_level - 1){
        lvl++;
    }
    return lvl;
}

std::optional<std::string> SkipList::get(const std::string &key) const{
    SkipListNode* current = head;

    // Start from highest active level and move right, then drop down
    for(int i = current_level; i >= 0; i--){
        while(current->forward[i] != nullptr && current->forward[i]->key < key){
            current = current->forward[i];
        }
    }

    // Drop to level 0 and advance to target node
    current = current->forward[0];

    if(current != nullptr && current->key == key){
        if(current->is_tombstone){
            return std::nullopt;
        }
        return current->value; // Not a tombstone, return value
    }

    return std::nullopt;
}

void SkipList::put(const std::string &key, const std::string &value, bool is_tombstone){
    // Keep track of nodes whose forward pointers need to be updated
    std::vector<SkipListNode*> update(max_level, nullptr);
    SkipListNode* current = head;

    for(int i = current_level; i >= 0; i--){
        while(current->forward[i] != nullptr && current->forward[i]->key < key){
            current = current->forward[i];
        }
        update[i] = current;
    }

    current = current->forward[0];

    // Case-1: key already exists, overwrite
    if(current != nullptr && current->key == key){
        current->value = value;
        current->is_tombstone = is_tombstone;
        // Not a new element, so size_ does not change
        return;
    }

    // Case-2: key does not exits. insert a new node
    int new_level = randomLevel();

    // If new node level is higher than current maximum level of list, initialize update array for new levels to point to the head node
    if(new_level > current_level){
        for(int i = current_level + 1; i <= new_level; i++){
            update[i] = head;
        }
        current_level = new_level;
    }

    // create new node
    SkipListNode* new_node = new SkipListNode(key, value, new_level + 1, is_tombstone);

    for(int i = 0; i <= new_level; i++){
        new_node->forward[i] = update[i]->forward[i];
        update[i]->forward[i] = new_node;
    }
    size_++;
}

void SkipList::remove(const std::string& key){
    // Instead of deleting, we insert a tombstone node.
    // The value for a tombstone is irrelevant, so we use an empty string.
    put(key, "", true);
}

void SkipList::print() const {
    std::cout << "\n--- Skip List Contents --\n";

    for(int i = current_level; i >= 0; i--){
        SkipListNode* current = head->forward[i];
        std::cout << "Level " << i << ": ";

        while(current != nullptr){
            std::cout << "[" << current->key << ":" << current->value << "] ";
            current = current->forward[i];
        }
        std::cout << "\n";
    }
    std::cout << "------------------------\n";
}

std::vector<FlushedEntry> SkipList::flushAll() const {
    std::vector<FlushedEntry> result;
    // Level 0 is a standard linked list and contains all elements in soted order
    SkipListNode* current = head->forward[0]; 
    while (current != nullptr) {
        result.push_back({current->key, current->value, current->is_tombstone});
        current = current->forward[0];
    }
    return result;
}

} // namespace LSM