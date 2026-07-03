#ifndef SSTABLE_HPP
#define SSTABLE_HPP

#include <string>
#include <vector>
#include <utility>
#include <optional>
#include <cstdint>
#include <cstring>

#include "DataTypes.hpp" // For FlushedEntry
#include "BloomFilter.hpp"

namespace LSM {

class SSTable {
public:
    SSTable(const std::string &path);
    ~SSTable() = default;

    // write sorted data to immutable file
    void write(const std::vector<FlushedEntry> &data);

    // read the file to find a specific key
    std::optional<std::string> search(const std::string &target_key) const;

private:
    std::string file_path;
    
    BloomFilter bloom_filter_; // make bloom filter stateful so it lives whole life of SSTable and not reinitialized on every search- which was previous behaviour

    static constexpr size_t BLOCK_SIZE = 4096; // 4KB Blocks
    static constexpr uint64_t MAGIC_NUMBER = 0xDEADBEEFCAFEBABE; // File signature

    #pragma pack(push, 1)
    struct RecordHeader{
        uint8_t record_type; // 0 for PUT, 1 for DELETE (tombstone)
        uint16_t key_len;
        uint32_t val_len;
    };

    // footer now needs to track where bloom filter is stored
    struct Footer{
        uint64_t index_offset;
        uint64_t magic_number;
        uint64_t meta_offset; // offset for bloom filter
    };
    #pragma pack(pop)

    // Helper to search within a single 4KB block
    std::optional<std::string> searchInBlock(const std::string& block_data, const std::string& target_key) const;

};

} // namespace LSM

#endif