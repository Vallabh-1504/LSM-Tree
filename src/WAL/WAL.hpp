#ifndef WAL_HPP
#define WAL_HPP

#include <string>
#include <fstream>
#include <vector>
#include <utility>

namespace LSM {

class WAL{
private:
    std::string log_path;
    std::ofstream out_stream;

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