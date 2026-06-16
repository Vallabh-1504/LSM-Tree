#ifndef SKIPLIST_HPP
#define SKIPLIST_HPP

#include <string>
#include <vector>
#include <optional>

namespace LSM{

struct SkipListNode{
    std::string key;
    std::string value;

    // an Array of forward pointers.
    std::vector<SkipListNode*> forward;

    SkipListNode(const std::string &k, const std::string &v, int level) : key(k), value(v), forward(level, nullptr){}
};

class SkipList{
private:
    int max_level;
    float probability;
    int current_level;
    SkipListNode* head;

    // generate random level for a new node
    int randomLevel();

public:
    SkipList(int max_level = 12, float probability = 0.25);
    ~SkipList();

    void put(const std::string &key, const std::string &value);

    std::optional<std::string> get(const std::string &key) const;

    // will make a tombstone but implementing this for now
    void remove(const std::string &key);

    // debugging
    void print() const;

    // Extracts all elements in sorted order, required by SSTable
    std::vector<std::pair<std::string, std::string>> flushAll() const;

};

} // namespace LSM

#endif