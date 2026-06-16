#include "SSTable.hpp"

#include <iostream>
#include <fstream>

namespace LSM {

SSTable::SSTable(const std::string &path) : file_path(path){}

void SSTable::write(const std::vector<std::pair<std::string, std::string>> &data){
    // open in binary and trunc ensoures we start in a new file
    std::ofstream out(file_path, std::ios::binary | std::ios::trunc);
    if(!out.is_open()){
        std::cerr << "failed to create SSTable: " << file_path << "\n";
        return;
    }

    for(const auto &pair : data){
        RecordHeader header;
        header.key_len = static_cast<uint16_t>(pair.first.size());
        header.val_len = static_cast<uint32_t>(pair.second.size());

        out.write(reinterpret_cast<const char*>(&header), sizeof(RecordHeader));
        out.write(pair.first.data(), pair.first.size());
        out.write(pair.second.data(), pair.second.size());
    }

    out.close(); // This file is now immutable by convention
}

std::optional<std::string> SSTable::search(const std::string &target_key) const {
    std::ifstream in(file_path, std::ios::binary);
    if(!in.is_open()){
        return std::nullopt;
    }

    RecordHeader header;
    // linear search (will improve later with block indexing)
    while(in.read(reinterpret_cast<char*>(&header), sizeof(RecordHeader))){
        std::string current_key(header.key_len, '\0');
        in.read(&current_key[0], header.key_len);

        // if current key matched, return the value
        if(current_key == target_key){
            std::string value(header.val_len, '\0');
            in.read(&value[0], header.val_len);
            return value;
        }

        // as file is sorted, if current key is greater than target key, stop early
        if(current_key > target_key){
            break;
        }

        // if its not the target, skip the value of bytes to save I/O
        in.seekg(header.val_len, std::ios::cur);
    }

    return std::nullopt;
}

} // namespace LSM