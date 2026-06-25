#include <vector>
#include <string>
#include <cstdint>

namespace LSM{

class BloomFilter {
private:
    std::vector<bool> bits_;
    uint8_t num_hashes_;

    uint64_t hash(const std::string &key, uint8_t seed) const;

public:
    // bits_per_key: Higher for fewer false positives (usually 10)
    explicit BloomFilter(size_t num_elements, int bits_per_key = 10);
    
    void add(const std::string& key);
    bool possiblyExists(const std::string& key) const;
    
    // Serializing the filter so it can be stored inside the SSTable file
    std::vector<uint8_t> serialize() const;
    void deserialize(const std::vector<uint8_t>& data);
};

} // Namespace LSM