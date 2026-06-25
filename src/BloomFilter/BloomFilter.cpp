#include "BloomFilter.hpp"
#include <cmath>

namespace LSM{

BloomFilter::BloomFilter(size_t num_elements, int bits_per_key) {
    size_t num_bits = num_elements * bits_per_key;
    if (num_bits < 64) num_bits = 64; // Minimum size
    bits_.resize(num_bits, false);
    
    // Formula for optimal number of hashes: (m/n) * ln(2)
    num_hashes_ = static_cast<uint8_t>(std::round((static_cast<float>(num_bits) / num_elements) * 0.693));
}

uint64_t BloomFilter::hash(const std::string& key, uint8_t seed) const {
    // A simple FNV-1a hash variation using the seed to create multiple "different" functions
    uint64_t h = 0xcbf29ce484222325 ^ seed;
    for (char c : key) {
        h = (h ^ static_cast<uint8_t>(c)) * 0x100000001b3;
    }
    return h;
}

void BloomFilter::add(const std::string& key) {
    for (uint8_t i = 0; i < num_hashes_; i++) {
        uint64_t h = hash(key, i);
        bits_[h % bits_.size()] = true;
    }
}

bool BloomFilter::possiblyExists(const std::string& key) const {
    for (uint8_t i = 0; i < num_hashes_; i++) {
        uint64_t h = hash(key, i);
        if (!bits_[h % bits_.size()]) {
            return false; // Guaranteed not to exist
        }
    }
    return true; // Might exist
}

std::vector<uint8_t> BloomFilter::serialize() const {
    // We need 1 byte to store the number of hashes, plus enough bytes to hold all our bits
    size_t num_bytes = (bits_.size() + 7) / 8; // Ceiling division
    std::vector<uint8_t> buffer(1 + num_bytes, 0);

    // Store the number of hashes in the very first byte
    buffer[0] = num_hashes_;

    // Pack the boolean bits into actual 8-bit bytes
    for (size_t i = 0; i < bits_.size(); i++) {
        if (bits_[i]) {
            buffer[1 + (i / 8)] |= (1 << (i % 8)); // Bitwise OR to flip the specific bit ON
        }
    }
    return buffer;
}

void BloomFilter::deserialize(const std::vector<uint8_t>& data) {
    if (data.empty()) return;

    // Read the number of hashes
    num_hashes_ = data[0];

    // Unpack the bytes back into the std::vector<bool>
    size_t num_bits = (data.size() - 1) * 8;
    bits_.resize(num_bits, false);

    for (size_t i = 0; i < num_bits; i++) {
        bits_[i] = (data[1 + (i / 8)] & (1 << (i % 8))) != 0;
    }
}

}; // Namespace LSm