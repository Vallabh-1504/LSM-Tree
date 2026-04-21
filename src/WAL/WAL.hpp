#ifndef WAL_HPP
#define WAL_HPP

#include <string>
#include <fstream>
#include <vector>
#include <utility>
#include <cstdint> // for fix-width integer types (unint32_t)

namespace LSM {

class WAL{
private:
    std::string log_path;
    std::ofstream out_stream;

    // pragma directive for compiler to tell- 'do not add padding bytes to this struct'
    // as we want it to be exactly 10 bytes on disk
    #pragma pack(push, 1)
    struct LogHeader{
        uint32_t crc32; // for integrity of key and value
        uint16_t key_len; // how many bytes to read for key
        uint32_t val_len; // how many bytes to read for value
    };
    #pragma pack(pop)

    // helper for CRC32 data integrity
    uint32_t calculateCRC32(const std::string &data) const;

public:
    WAL(const std::string &path);
    ~WAL();

    // Append to log
    void append(const std::string& key, const std::string& value);

    // Read log file from disk to rebuild MemTable on startup
    std::vector<std::pair<std::string, std::string>> recover();
    
    // Clears the log file
    void clear();
};

}

#endif