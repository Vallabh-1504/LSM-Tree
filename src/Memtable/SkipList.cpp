#include "SkipList.hpp"
#include <iostream>
#include <random>

namespace LSM {

SkipList::SkipList(int max_level, float probability) : max_level(max_level), probability(probability), current_level(0){
    head = new SkipListNode("", "", max_level);
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
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(0.0f, 1.0f);

    int lvl = 0;
    while(dis(gen) < probability && lvl < max_level - 1){
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
        return current->value;
    }

    return std::nullopt;
}

void SkipList::put(const std::string &key, const std::string &value){
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
    SkipListNode* new_node = new SkipListNode(key, value, new_level + 1);

    for(int i = 0; i <= new_level; i++){
        new_node->forward[i] = update[i]->forward[i];
        update[i]->forward[i] = new_node;
    }
}

void SkipList::remove(const std::string& key){
    std::vector<SkipListNode*> update(max_level, nullptr);
    SkipListNode* current = head;

    for(int i = current_level; i >= 0; i--){
        while(current->forward[i] != nullptr && current->forward[i]->key < key){
            current = current->forward[i];
        }
        update[i] = current;
    }

    current = current->forward[0];

    // If the node exists, update pointers to bypass it, then delete it.
    if(current != nullptr && current->key == key){
        for(int i = 0; i <= current_level; i++){
            // If at level i, the previous node's forward pointer doesn't point to current, it means we've updated all pointers for levels the node existed in. Break early
            if(update[i]->forward[i] != current){
                break;
            }
            update[i]->forward[i] = current->forward[i];
        }

        delete current;

        // Shrink current_level if the deleted node was the only one at the highest levels
        while(current_level > 0 && head->forward[current_level] == nullptr){
            current_level--;
        }
    }
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

std::vector<std::pair<std::string, std::string>> SkipList::flushAll() const {
    std::vector<std::pair<std::string, std::string>> result;
    // Level 0 is a standard linked list and contains all elements in soted order
    SkipListNode* current = head->forward[0]; 
    while (current != nullptr) {
        result.emplace_back(current->key, current->value);
        current = current->forward[0];
    }
    return result;
}

} // namespace LSM