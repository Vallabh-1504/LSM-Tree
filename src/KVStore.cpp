#include <filesystem>
#include <iostream>

#include "KVStore.hpp"

namespace LSM {

KVStore::KVStore(const std::string &directory) : db_directory(directory){
    // 1. ensure directory exists
    if(!std::filesystem::exists(db_directory)){
        std::filesystem::create_directories(db_directory);
    }

    // 2. initial WAL and Memtable
    std::string wal_path = db_directory + "/LSM.wal";
    wal = std::make_unique<WAL>(wal_path);
    memtable = std::make_unique<SkipList>();

    // 3. rebuild in-memory state from disk Log
    recoverFromWAL();
}

void KVStore::recoverFromWAL(){
    auto entries = wal->recover();
    int count = 0;

    for(const auto &entry : entries){
        if (entry.type == RecordType::PUT) {
            // insert directly into the Memtable without calling the KVStore::put()
            // as we do not want to write back to WAL Again
            memtable->put(entry.key, entry.value, false);
        } else if (entry.type == RecordType::DELETE) {
            memtable->put(entry.key, "", true); // Insert tombstone
        }
        count++;
    }

    if(count > 0){
        std::cout << "successfully recovered " << count << " keys from WAL\n"; 
    }
}

void KVStore::put(const std::string &key, const std::string &value){
    // 1. append to disk
    wal->append(key, value, RecordType::PUT);

    // 2. make available in RAM from fast reads
    memtable->put(key, value, false);
}

std::optional<std::string> KVStore::get(const std::string &key) const {
    return memtable->get(key);
}

void KVStore::remove(const std::string &key){
    // removal is a special write- tombstone
    wal->append(key, "", RecordType::DELETE);

    // "remove" from RAM by inserting a tombstone
    memtable->remove(key);
}

} // namespace LSM