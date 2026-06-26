#include <iostream>
#include <fstream>

#include "SSTable.hpp"
#include "BloomFilter.hpp"

namespace LSM {

SSTable::SSTable(const std::string &path) : file_path(path){}

void SSTable::write(const std::vector<FlushedEntry> &data){
    // open in binary and trunc ensoures we start in a new file
    std::ofstream out(file_path, std::ios::binary | std::ios::trunc);
    if(!out.is_open()){
        std::cerr << "failed to create SSTable: " << file_path << "\n";
        return;
    }

    // initialize bloom filter for this specific SSTable
    LSM::BloomFilter filter(data.size());

    std::string current_block;
    std::vector<std::pair<std::string, uint64_t>> index_entries;
    uint64_t current_block_offset = 0;

    // 1. BUild data blocks an populate Bloom Filter
    for(size_t i = 0; i < data.size(); i++){
        const auto &entry = data[i];

        // Add every key to Bloom Filter as it passes by
        filter.add(entry.key);

        RecordHeader header;
        header.record_type = entry.is_tombstone ? 1 : 0;
        header.key_len = static_cast<uint16_t>(entry.key.size());
        header.val_len = static_cast<uint32_t>(entry.value.size());

        std::string record(reinterpret_cast<const char*>(&header), sizeof(RecordHeader));
        record += entry.key;
        record += entry.value;

        // If adding this record exceeds 4KB, flush the current block to disk
        if(!current_block.empty() && current_block.size() + record.size() > BLOCK_SIZE){
            out.write(current_block.data(), current_block.size());

            // The key of the LAST element in this block is used for the index
            // We use i - 1 because i is the current element that triggered the flush
            index_entries.emplace_back(data[i - 1].key, current_block_offset);

            current_block_offset += current_block.size();
            current_block.clear();
        }

        current_block += record;
    }

    // Flush the final, partially filled data block   
    if (!current_block.empty()) {
        out.write(current_block.data(), current_block.size());

        index_entries.emplace_back(data.back().key, current_block_offset);
    }

    // 2. Write the Meta Block (Bloom Filter)
    uint64_t meta_offset = out.tellp();
    std::vector<uint8_t> serialized_filter = filter.serialize();

    // We write the size of the serialized filter first so to know how many bytes to read later
    uint32_t filter_size = static_cast<uint32_t>(serialized_filter.size());
    out.write(reinterpret_cast<const char*>(&filter_size), sizeof(uint32_t));
    out.write(reinterpret_cast<const char*>(serialized_filter.data()), serialized_filter.size());

    // 3. Record where the Index Block starts and build
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
    footer.meta_offest = meta_offset; // Recording bloom filter location
    out.write(reinterpret_cast<const char*>(&footer), sizeof(Footer));

    out.close(); // This file is now immutable by convention
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

    if(footer.magic_number != MAGIC_NUMBER){
        return std::nullopt;
    }

    // 2. GATEKEEPER: reading bloom filter first, if it say no, exit from here
    in.seekg(footer.meta_offest, std::ios::beg);
    uint32_t filter_size;
    in.read(reinterpret_cast<char*>(&filter_size), sizeof(uint32_t));

    std::vector<uint8_t> filter_data(filter_size);
    in.read(reinterpret_cast<char*>(filter_data.data()), filter_size);

    BloomFilter filter(0); // Create empty filter
    filter.deserialize(filter_data); // Load state from disk

    if(!filter.possiblyExists(target_key)){
        // std::cout << "[Bloom Filter]: Key '" << target_key << "' definitely NOT in file. Skipping disk search.\n";
        return std::nullopt;
    }

    // 3. Read the Index Block
    uint64_t index_size = (std::streamoff)footer.magic_number; // We calculate size by offsets
    in.seekg(0, std::ios::end);
    uint64_t file_size = in.tellg();
    index_size = file_size - footer.index_offset - sizeof(Footer);

    in.seekg(footer.index_offset, std::ios::beg);
    std::string index_data(index_size, '\0');
    in.read(&index_data[0], index_size);

    // 3. Search the Index Block in RAM to find the correct Data Block offset
    size_t idx_offset = 0;
    uint64_t target_block_offset = 0;
    bool block_found = false;

    while(idx_offset < index_size){
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

    if(!block_found){
        return std::nullopt;
    }

    // 4. Determine block size and read ONLY that specific Data Block from disk
    in.seekg(target_block_offset, std::ios::beg);
    
    // we read up to BLOCK_SIZE + maximum potential spillover, or up to the index offset. But, generally we would store exact block sizes
    uint64_t bytes_to_read = footer.index_offset - target_block_offset;
    if(bytes_to_read > BLOCK_SIZE * 2){
        bytes_to_read = BLOCK_SIZE * 2;
    } 

    std::string block_data(bytes_to_read, '\0');
    in.read(&block_data[0], bytes_to_read);

    // 5. Search the specific block in RAM
    return searchInBlock(block_data, target_key);
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
            if (header.record_type == 1) { // It's a tombstone
                return std::nullopt;
            }
            // It's a regular value
            return block_data.substr(offset, header.val_len);
        }

        if(current_key > target_key){
            break; // Data is sorted, we overshot
        }

        offset += header.val_len; // Skip the value to check the next key
    }
    return std::nullopt;
}

} // namespace LSM