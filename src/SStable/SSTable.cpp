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

    std::string current_block;
    std::vector<std::pair<std::string, uint64_t>> index_entries;
    uint64_t current_block_offset = 0;

    // BUild data blocks
    for (size_t i = 0; i < data.size(); i++) {
        const auto& pair = data[i];

        RecordHeader header;
        header.key_len = static_cast<uint16_t>(pair.first.size());
        header.val_len = static_cast<uint32_t>(pair.second.size());

        std::string record(reinterpret_cast<const char*>(&header), sizeof(RecordHeader));
        record += pair.first;
        record += pair.second;

        // If adding this record exceeds 4KB, flush the current block to disk
        if(!current_block.empty() && current_block.size() + record.size() > BLOCK_SIZE){
            out.write(current_block.data(), current_block.size());

            // The key of the LAST element in this block is used for the index
            // We use i - 1 because i is the current element that triggered the flush
            index_entries.emplace_back(data[i - 1].first, current_block_offset);

            current_block_offset += current_block.size();
            current_block.clear();
        }

        current_block += record;
    }

    // Flush the final, partially filled data block   
    if (!current_block.empty()) {
        out.write(current_block.data(), current_block.size());
        index_entries.emplace_back(data.back().first, current_block_offset);
    }

    // Record where the Index Block starts
    uint64_t index_offset = out.tellp();

    // Build and Write the Index Block
    for (size_t i = 0; i < index_entries.size(); i++) {
        const auto& entry = index_entries[i];
        uint16_t key_len = static_cast<uint16_t>(entry.first.size());
        
        out.write(reinterpret_cast<const char*>(&key_len), sizeof(uint16_t));
        out.write(entry.first.data(), entry.first.size());
        out.write(reinterpret_cast<const char*>(&entry.second), sizeof(uint64_t));
    }

    // Write the Footer
    Footer footer;
    footer.index_offset = index_offset;
    footer.magic_number = MAGIC_NUMBER;
    out.write(reinterpret_cast<const char*>(&footer), sizeof(Footer));

    out.close(); // This file is now immutable by convention
}

std::optional<std::string> SSTable::searchInBlock(const std::string& block_data, const std::string& target_key) const {
    size_t offset = 0;
    while(offset < block_data.size()) {
        if(offset + sizeof(RecordHeader) > block_data.size()) break;

        RecordHeader header;
        std::memcpy(&header, block_data.data() + offset, sizeof(RecordHeader));
        offset += sizeof(RecordHeader);

        std::string current_key = block_data.substr(offset, header.key_len);
        offset += header.key_len;

        if(current_key == target_key){
            return block_data.substr(offset, header.val_len);
        }

        if(current_key > target_key){
            break; // Data is sorted, we overshot
        }

        offset += header.val_len; // Skip the value to check the next key
    }
    return std::nullopt;
}

std::optional<std::string> SSTable::search(const std::string &target_key) const {
    std::ifstream in(file_path, std::ios::binary);
    if(!in.is_open()){
        return std::nullopt;
    }

    // 1. Read the Footer
    in.seekg(-static_cast<int>(sizeof(Footer)), std::ios::end);
    Footer footer;
    in.read(reinterpret_cast<char*>(&footer), sizeof(Footer));

    // 2. Read the Index Block
    uint64_t file_size = in.tellg();
    uint64_t index_size = file_size - footer.index_offset - sizeof(Footer);

    in.seekg(footer.index_offset, std::ios::beg);
    std::string index_data(index_size, '\0');
    in.read(&index_data[0], index_size);

    // 3. Search the Index Block in RAM to find the correct Data Block offset
    size_t idx_offset = 0;
    uint64_t target_block_offset = 0;
    bool block_found = false;

    while (idx_offset < index_size) {
        uint16_t key_len;
        std::memcpy(&key_len, index_data.data() + idx_offset, sizeof(uint16_t));
        idx_offset += sizeof(uint16_t);

        std::string index_key = index_data.substr(idx_offset, key_len);
        idx_offset += key_len;

        uint64_t block_offset;
        std::memcpy(&block_offset, index_data.data() + idx_offset, sizeof(uint64_t));
        idx_offset += sizeof(uint64_t);

        // If the target_key is less than or equal to the last key in this block,
        // then the target MUST be in this block (if it exists at all).
        if (target_key <= index_key) {
            target_block_offset = block_offset;
            block_found = true;
            break;
        }
    }

    if (!block_found) return std::nullopt;

    // 4. Determine block size and read ONLY that specific Data Block from disk
    in.seekg(target_block_offset, std::ios::beg);
    
    // In a production engine, we would store exact block sizes. For this implementation,
    // we read up to BLOCK_SIZE + maximum potential spillover, or up to the index offset.
    uint64_t bytes_to_read = footer.index_offset - target_block_offset;
    if (bytes_to_read > BLOCK_SIZE * 2) bytes_to_read = BLOCK_SIZE * 2; 

    std::string block_data(bytes_to_read, '\0');
    in.read(&block_data[0], bytes_to_read);

    // 5. Search the specific block in RAM
    return searchInBlock(block_data, target_key);
}

} // namespace LSM