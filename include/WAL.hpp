#ifndef WAL_HPP
#define WAL_HPP

#include <string>
#include <fstream>
#include <vector>
#include <utility>
#include <cstdint> // for fix-width integer types (unint32_t)

namespace LSM {

enum class RecordType : uint8_t {
    PUT = 0,
    DELETE = 1,
};

struct WALEntry {
    std::string key;
    std::string value;
    RecordType type;
};

class WAL{
private:
    std::string log_path;
    std::ofstream out_stream;

    #pragma pack(push, 1)
    struct LogHeader{
        uint32_t crc32; // for integrity of key and value
        uint16_t key_len; // how many bytes to read for key
        uint32_t val_len; // how many bytes to read for value
        RecordType type;
    };
    #pragma pack(pop)

    // helper for CRC32 data integrity
    uint32_t calculateCRC32(const std::string &data) const;

public:
    WAL(const std::string &path);
    ~WAL();

    // Append to log
    void append(const std::string& key, const std::string& value, RecordType type);

    // Read log file from disk to rebuild MemTable on startup
    std::vector<WALEntry> recover();
    
    // Clears the log file
    void clear();
    
    // Sync the WAL file with Memtable periodically (Batch Writing)
    void sync() { out_stream.flush(); }
};

}

#endif