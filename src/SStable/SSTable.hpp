#include <string>
#include <vector>
#include <utility>
#include <optional>
#include <cstdint>

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

    #pragma pack(push, 1)
    struct RecordHeader{
        uint16_t key_len;
        uint32_t val_len;
    };
    #pragma pack(pop)

};

} // namespace LSM