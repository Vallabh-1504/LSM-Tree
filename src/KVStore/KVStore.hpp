#ifndef KVSTORE_HPP
#define KVSTORE_HPP

#include <string>
#include <memory>
#include <optional>
#include "../Memtable/SkipList.hpp"
#include "../WAL/WAL.hpp"

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

    // helper to restore on startup
    void recoverFromWAL();

};

}  // namespace LSM

#endif