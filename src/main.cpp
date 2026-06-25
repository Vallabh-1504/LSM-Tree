#include "SkipList.hpp"
#include "SSTable.hpp"
#include <iostream>
#include <chrono>

// Helper macro/function for profiling
template <typename Func>
void measureTime(const std::string& keyName, Func&& searchFunc) {
    auto start = std::chrono::high_resolution_clock::now();
    
    auto val = searchFunc();
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "Search '" << keyName << "': " 
              << (val ? "Found" : "Miss") 
              << " | Time: " << duration.count() << " us\n";
}

int main() {
    LSM::SkipList memtable;
    
    // Generate enough data to force the SSTable to create multiple 4KB blocks.
    // inserting 2000 keys with a 50-byte exceeding 4096 bytes.
    std::cout << "Generating data...\n";
    for(int i = 0; i < 2000; i++){
        char key_buf[32];
        snprintf(key_buf, sizeof(key_buf), "key_%04d", i);
        
        std::string value(50, 'A' + (i % 26)); // e.g., "AAAA..." or "BBBB..."
        memtable.put(key_buf, value);
    }

    auto sorted_data = memtable.flushAll();

    LSM::SSTable sstable("./data_v2.sst");
    sstable.write(sorted_data);
    std::cout << "Flushed to Bloon-Filter Block-Indexed SSTable.\n";

    // Cache warmup, dummy search
    measureTime("warmup_key", [&](){ return sstable.search("warmup_key"); }); 

    // Testing extreme bounds and middle of the data
    std::cout << "\nTesting Block Indexing\n";
    
    measureTime("key_0000", [&]() { return sstable.search("key_0000"); }); // Should be in the very first block
    measureTime("key_1050", [&]() { return sstable.search("key_1050"); }); // Should be deep in a middle 
    measureTime("key_1999", [&]() { return sstable.search("key_1999"); }); // Should be in the final block
    measureTime("key_9999", [&]() { return sstable.search("key_9999"); }); // Should fail immediately via index

    return 0;
}