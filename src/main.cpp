#include "./WAL/WAL.hpp"
#include <iostream>

int main(){
    std::string filepath = "test.wal";

    // write phase
    {
        LSM::WAL wal(filepath);

        wal.clear();

        wal.append("user:101", "{\"name\":\"Vallabh\", \"age\":\"22\"}");
        wal.append("user:101", "{\"name\":\"Saloni\", \"age\":\"26\"}");
        std::cout << "Data appended to binary WAL\n";
    }

    // reocvery phase
    LSM::WAL wal(filepath);
    auto recovered_data = wal.recover();
    std::cout << "\n Recovered " << recovered_data.size() << " entries\n";
    for(const auto &pair : recovered_data){
        std::cout << "Key: " << pair.first << " | value: " << pair.second << "\n";
    }
    return 0;
}