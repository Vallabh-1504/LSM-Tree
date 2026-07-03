#ifndef KVSTORE_HPP
#define KVSTORE_HPP

#include <string>
#include <memory>
#include <optional>

#include "SkipList.hpp"
#include "WAL.hpp"
#include "SSTable.hpp"

namespace LSM {

class KVStore {
public:
    KVStore(const std::string &directory);
    ~KVStore() = default;

    KVStore(const KVStore&) = delete;
    KVStore& operator=(const KVStore&) = delete;

    // Core API exposed to user
    void put(const std::string &key, const std::string &value);
    std::optional<std::string> get(const std::string &key) const;
    void remove(const std::string &key);

private:
    std::string db_directory;
    std::unique_ptr<WAL> wal;
    std::unique_ptr<SkipList> memtable;
    std::vector<SSTable> sstables_; // one entry per flushed file    
    size_t memtable_threshold_ = 4096; // Flush after this many items

    // helper to restore on startup
    void recoverFromWAL();

    // helper to flush memtable to disk
    void flushMemtable();

};

}  // namespace LSM

#endif