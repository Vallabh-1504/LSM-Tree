#include "SkipList.hpp"
#include "SSTable.hpp"
#include <iostream>


int main() {
    // 1. Simulate an Active MemTable filling up
    LSM::SkipList memtable;
    memtable.put("apple", "A red fruit");
    memtable.put("zebra", "An animal with stripes");
    memtable.put("banana", "A yellow fruit");
    memtable.put("mango", "King of fruits");

    // 2. The MemTable is full. Extract sorted data.
    auto sorted_data = memtable.flushAll();
    
    // 3. Write it to an Immutable SSTable on disk
    LSM::SSTable sstable("./data_001.sst");
    sstable.write(sorted_data);
    std::cout << "Successfully flushed MemTable to data_001.sst\n";

    // 4. Test searching the disk file directly (without RAM)
    std::cout << "\n--- Searching SSTable ---\n";
    
    auto val1 = sstable.search("banana");
    std::cout << "Search 'banana': " << (val1 ? val1.value() : "Not Found") << "\n";

    auto val2 = sstable.search("zebra");
    std::cout << "Search 'zebra': " << (val2 ? val2.value() : "Not Found") << "\n";

    auto val3 = sstable.search("grape");
    std::cout << "Search 'grape': " << (val3 ? val3.value() : "Not Found") << "\n";

    return 0;
}