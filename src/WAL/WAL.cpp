#include "WAL.hpp"

#include <iostream>

namespace LSM {

WAL::WAL(const std::string &path) : log_path(path){
    // Create file in binary mode (binary mode stops OS from modifying line endings)
    out_stream.open(log_path, std::ios::app | std::ios::binary);
    if(!out_stream.is_open()){
        std::cerr << "Failed to open WAL file: " << log_path << "\n";
    }
}

WAL::~WAL(){
    if(out_stream.is_open()){
        out_stream.close();
    }
}

uint32_t WAL::calculateCRC32(const std::string &data) const{
    uint32_t crc = 0xFFFFFFFF;
    for(char c : data){
        crc ^= static_cast<uint8_t>(c);
        for(int i = 0; i < 8; i++){
            crc = (crc >> 1) ^ (0xEd88320 & (-(crc & 1)));
        }
    }
    return crc;
}

void WAL::append(const std::string& key, const std::string& value){
    LogHeader header;
    header.key_len = static_cast<uint16_t>(key.size());
    header.val_len = static_cast<uint32_t>(value.size());

    // checksum over combined key and value string
    header.crc32 = calculateCRC32(key + value);

    // reinterpret_cast to treat struct as a rwa array of bytes
    out_stream.write(reinterpret_cast<const char*>(&header), sizeof(LogHeader));
    out_stream.write(key.data(), key.size());
    out_stream.write(value.data(), value.size());

    out_stream.flush();
}

std::vector<std::pair<std::string, std::string>> WAL::recover(){
    std::vector<std::pair<std::string, std::string>> entries;
    std::ifstream in_stream(log_path, std::ios::binary);
    
    if(!in_stream.is_open()){
        return entries; // Return empty if no log exists yet
    }

    LogHeader header;

    // read exactly sizeof(LogHeader) bytes, if failed, we reached end
    while(in_stream.read(reinterpret_cast<char*>(&header), sizeof(LogHeader))){

        // Allocate exact memory for string based on header length
        std::string key(header.key_len, '\0');
        std::string value(header.val_len, '\0');

        // read the exact number of bytes
        in_stream.read(&key[0], header.key_len);
        in_stream.read(&value[0], header.val_len);

        // verify data integrity
        uint32_t computed_crc = calculateCRC32(key + value);
        if(computed_crc != header.crc32){
            std::cerr << "WAL Data corruption detected! Stopping reocvery at this point\n";
            break;
        }

        entries.emplace_back(key, value);
    }

    return entries;
}

void WAL::clear(){
    out_stream.close();

    // open with std::ios::trunc to wipe the file contents
    out_stream.open(log_path, std::ios::out | std::ios::trunc | std::ios::binary);
    out_stream.close();

    // reopen in append mode to match the original state
    out_stream.open(log_path, std::ios::app | std::ios::binary); 

    
}

} // namespace LSM
