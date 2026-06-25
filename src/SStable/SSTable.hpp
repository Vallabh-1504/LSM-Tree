#include <string>
#include <vector>
#include <utility>
#include <optional>
#include <cstdint>
#include <cstring>

namespace LSM {

class SSTable {
public:
    SSTable(const std::string &path);
    ~SSTable() = default;

    // write sorted data to immutable file
    void write(const std::vector<std::pair<std::string, std::string>> &data);

    // read the file to find a specific key
    std::optional<std::string> search(const std::string &target_key) const;

private:
    std::string file_path;


    static constexpr size_t BLOCK_SIZE = 4096; // 4KB Blocks
    static constexpr uint64_t MAGIC_NUMBER = 0xDEADBEEFCAFEBABE; // File signature

    #pragma pack(push, 1)
    struct RecordHeader{
        uint16_t key_len;
        uint32_t val_len;
    };

    struct Footer{
        uint64_t index_offset;
        uint64_t magic_number;
    };
    #pragma pack(pop)

    // Helper to search within a single 4KB block
    std::optional<std::string> searchInBlock(const std::string& block_data, const std::string& target_key) const;

};

} // namespace LSM