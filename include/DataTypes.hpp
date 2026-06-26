#ifndef DATATYPES_HPP
#define DATATYPES_HPP

#include <string>

namespace LSM {

// Struct to hold data flushed from Memtable to SSTable
struct FlushedEntry {
    std::string key;
    std::string value;
    bool is_tombstone;
};

} // namespace LSM

#endif // DATATYPES_HPP