#include <cmath>

#include "BloomFilter.hpp"
#include "MurmurHash3.h"

namespace LSM{

BloomFilter::BloomFilter(size_t num_elements, int bits_per_key) {
    // add 0 guard for undefined zero division behavior below
    if (num_elements == 0) {
        bits_.resize(8, 0); // 64 bits
        num_hashes_ = 0;
        return;
    } 

    size_t num_bits = num_elements * bits_per_key;
    if (num_bits < 64){
        num_bits = 64; // Minimum size
    }
    // Round up to the nearest byte
    bits_.resize((num_bits + 7) / 8, 0);
    
    // Formula for optimal number of hashes: (m/n) * ln(2)
    num_hashes_ = static_cast<uint8_t>(
        std::round((static_cast<float>(bits_.size() * 8) / num_elements) * 0.693));
}

uint64_t BloomFilter::hash(const std::string& key, uint8_t seed) const {
    // A simple FNV-1a hash variation using the seed to create multiple "different" functions
    uint64_t h = 0xcbf29ce484222325 ^ seed;
    for (char c : key) {
        h = (h ^ static_cast<uint8_t>(c)) * 0x100000001b3;
    }
    return h;
}

// Wrapper around the external API of murmurhash
void BloomFilter::getHashes(const std::string& key, uint64_t& h1, uint64_t& h2) const {
    // using MurmurHash3_x64_128 which outputs 128 bits (16 bytes)
    uint64_t hash_out[2]; 
    
    // Arguments: key data, key length, seed (0), output array
    MurmurHash3_x64_128(key.data(), key.length(), 0, hash_out);
    
    h1 = hash_out[0];
    h2 = hash_out[1];
}

void BloomFilter::add(const std::string& key) {
    uint64_t h1, h2;
    getHashes(key, h1, h2); // Hash string exactly once
    size_t num_bits = bits_.size() * 8;

    for(uint8_t i = 0; i < num_hashes_; i++){
        // Kirsch-Mitzenmacher optimization
        // h_i(x) = (h_1(x) + i x h_2(x)) (mod m)
        uint64_t combined_hash = h1 + (i * h2);
        size_t bit_pos = combined_hash % num_bits;
        bits_[bit_pos / 8] |= (1 << (bit_pos % 8));
    }
}

bool BloomFilter::possiblyExists(const std::string& key) const {
    uint64_t h1, h2;
    getHashes(key, h1, h2);
    size_t num_bits = bits_.size() * 8;

    for(uint64_t i = 0; i < num_hashes_; i++){
        uint64_t combined_hash = h1 + (i * h2);
        size_t bit_pos = combined_hash % num_bits;
        if ((bits_[bit_pos / 8] & (1 << (bit_pos % 8))) == 0) {
            return false;
        }
    }

    return true; // Might exist
}

std::vector<uint8_t> BloomFilter::serialize() const {
    // With std::vector<uint8_t>, serialization is much simpler.
    // We just need to prepend the num_hashes_ byte.
    std::vector<uint8_t> buffer(1 + bits_.size());

    // Store the number of hashes in the very first byte
    buffer[0] = num_hashes_;
    std::copy(bits_.begin(), bits_.end(), buffer.begin() + 1);
    return buffer;
}

void BloomFilter::deserialize(const std::vector<uint8_t>& data) {
    if (data.empty()) return;

    // Read the number of hashes
    num_hashes_ = data[0];

    // The rest of the data is our bit vector.
    bits_.assign(data.begin() + 1, data.end());
}

}; // Namespace LSm