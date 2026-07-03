#include <filesystem>
#include <string>
#include <vector>
#include <iostream>
#include <algorithm>
#include <chrono>

#include "KVStore.hpp"

namespace LSM {

KVStore::KVStore(const std::string &directory) : db_directory(directory){
    // 1. ensure directory exists
    if(!std::filesystem::exists(db_directory)){
        std::filesystem::create_directories(db_directory);
    }

    // 2. Scan for existing SSTables and load them
    std::vector<std::string> sstable_paths;
    for (const auto& entry : std::filesystem::directory_iterator(db_directory)) {
        if (entry.is_regular_file() && entry.path().extension() == ".sst") {
            sstable_paths.push_back(entry.path().string());
        }
    }
    // Sort paths alphabetically to load them in creation order (assuming timestamp-based names)
    std::sort(sstable_paths.begin(), sstable_paths.end());
    for (const auto& path : sstable_paths) {
        sstables_.emplace_back(path);
    }
    if (!sstable_paths.empty()) {
        std::cout << "Loaded " << sstable_paths.size() << " SSTables from disk.\n";
    }


    // 3. initial WAL and Memtable
    std::string wal_path = db_directory + "/LSM.wal";
    wal = std::make_unique<WAL>(wal_path);
    memtable = std::make_unique<SkipList>();

    // 4. rebuild in-memory state from disk Log
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

    // 3. Check if memtable is full and needs to be flushed
    if (memtable->size() >= memtable_threshold_) {
        flushMemtable();
    }
}

std::optional<std::string> KVStore::get(const std::string &key) const {
    // 1. Check in memtable
    auto result = memtable->get(key);
    if(result.has_value()){
        return result;
    }

    // 2. Go in SSTables from newest to oldest
    for(auto it = sstables_.rbegin(); it != sstables_.rend(); it++){
        result = it->search(key);
        if(result.has_value()){
            return result;
        }
    }

    return std::nullopt;
}

void KVStore::remove(const std::string &key){
    // removal is a special write- tombstone
    wal->append(key, "", RecordType::DELETE);

    // "remove" from RAM by inserting a tombstone
    memtable->remove(key);
}

void KVStore::flushMemtable() {
    if (memtable->size() == 0) {
        return; // Nothing to flush
    }

    // 0. Sync the WAL to ensure all records are on disk before we rely on them for recovery.
    wal->sync();

    // 1. Create a new SSTable file path
    // Using a timestamp ensures unique, chronologically sortable filenames.
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    std::string sstable_path = db_directory + "/sstable_" + std::to_string(timestamp) + ".sst";

    // 2. Write memtable data to the new SSTable
    SSTable new_sstable(sstable_path);
    new_sstable.write(memtable->flushAll());
    sstables_.push_back(std::move(new_sstable));

    // 3. Atomically clear the WAL and reset the memtable.
    // This is the critical step to prevent the WAL from growing forever.
    wal->clear();
    memtable = std::make_unique<SkipList>();
}

} // namespace LSM