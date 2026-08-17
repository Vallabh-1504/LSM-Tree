#ifndef BLOOMFILTER_HPP
#define BLOOMFILTER_HPP

#include <vector>
#include <string>
#include <cstdint>

namespace LSM{

class BloomFilter {
private:
    std::vector<uint8_t> bits_;
    uint8_t num_hashes_;

    // DEPRECATED: manual FNV-1a hash is untimized which is bringing anomaly in benchmark numbers, because of manual unoptimized loop
    uint64_t hash(const std::string &key, uint8_t seed) const;

    // replacing hash with getHashes- using murmurHash3 by importing third-party vendor
    void getHashes(const std::string &key, uint64_t &h1, uint64_t &h2) const; 

public:
    BloomFilter() : num_hashes_(0) {}

    // bits_per_key: Higher for fewer false positives (usually 10)
    explicit BloomFilter(size_t num_elements, int bits_per_key = 10);
    
    void add(const std::string& key);
    bool possiblyExists(const std::string& key) const;
    
    // Serializing the filter so it can be stored inside the SSTable file
    std::vector<uint8_t> serialize() const;
    void deserialize(const std::vector<uint8_t>& data);
};

} // Namespace LSM

#endif