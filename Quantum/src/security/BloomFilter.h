#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <cmath>
#include <algorithm>

namespace quantum {

// MurmurHash3_x64_128 implementation (Simplified for local use)
inline void MurmurHash3_x64_128(const void* key, const int len, const uint32_t seed, void* out) {
    const uint8_t* data = (const uint8_t*)key;
    const int nblocks = len / 16;
    uint64_t h1 = seed;
    uint64_t h2 = seed;
    const uint64_t c1 = 0x87c37b91114253d5ULL;
    const uint64_t c2 = 0x4cf5ad432745937fULL;

    const uint64_t* blocks = (const uint64_t*)(data);
    for (int i = 0; i < nblocks; i++) {
        uint64_t k1 = blocks[i * 2];
        uint64_t k2 = blocks[i * 2 + 1];
        k1 *= c1; k1 = (k1 << 31) | (k1 >> 33); k1 *= c2; h1 ^= k1;
        h1 = (h1 << 27) | (h1 >> 37); h1 = h1 * 5 + 0x52dce729;
        k2 *= c2; k2 = (k2 << 33) | (k2 >> 31); k2 *= c1; h2 ^= k2;
        h2 = (h2 << 31) | (h2 >> 33); h2 = h2 * 5 + 0x38495ab5;
    }

    const uint8_t* tail = (const uint8_t*)(data + nblocks * 16);
    uint64_t k1 = 0;
    uint64_t k2 = 0;
    switch (len & 15) {
    case 15: k2 ^= (uint64_t)tail[14] << 48;
    case 14: k2 ^= (uint64_t)tail[13] << 40;
    case 13: k2 ^= (uint64_t)tail[12] << 32;
    case 12: k2 ^= (uint64_t)tail[11] << 24;
    case 11: k2 ^= (uint64_t)tail[10] << 16;
    case 10: k2 ^= (uint64_t)tail[9] << 8;
    case  9: k2 ^= (uint64_t)tail[8] << 0;
             k2 *= c2; k2 = (k2 << 33) | (k2 >> 31); k2 *= c1; h2 ^= k2;
    case  8: k1 ^= (uint64_t)tail[7] << 56;
    case  7: k1 ^= (uint64_t)tail[6] << 48;
    case  6: k1 ^= (uint64_t)tail[5] << 40;
    case  5: k1 ^= (uint64_t)tail[4] << 32;
    case  4: k1 ^= (uint64_t)tail[3] << 24;
    case  3: k1 ^= (uint64_t)tail[2] << 16;
    case  2: k1 ^= (uint64_t)tail[1] << 8;
    case  1: k1 ^= (uint64_t)tail[0] << 0;
             k1 *= c1; k1 = (k1 << 31) | (k1 >> 33); k1 *= c2; h1 ^= k1;
    };

    h1 ^= len; h2 ^= len;
    h1 += h2; h2 += h1;
    h1 ^= h1 >> 33; h1 *= 0xff51afd7ed558ccdULL; h1 ^= h1 >> 33; h1 *= 0xc4ceb9fe1a85ec53ULL; h1 ^= h1 >> 33;
    h2 ^= h2 >> 33; h2 *= 0xff51afd7ed558ccdULL; h2 ^= h2 >> 33; h2 *= 0xc4ceb9fe1a85ec53ULL; h2 ^= h2 >> 33;
    h1 += h2; h2 += h1;
    ((uint64_t*)out)[0] = h1;
    ((uint64_t*)out)[1] = h2;
}

class BloomFilter {
public:
    BloomFilter(size_t expectedItems, double falsePositiveRate) {
        // Calculate optimal size (m) and number of hashes (k)
        size_ = static_cast<size_t>(-1.0 * expectedItems * std::log(falsePositiveRate) / (std::log(2) * std::log(2)));
        numHashes_ = static_cast<int>(std::ceil((size_ / expectedItems) * std::log(2)));
        bits_.assign((size_ + 63) / 64, 0);
    }

    void add(const std::string& item) {
        uint64_t hashes[2];
        MurmurHash3_x64_128(item.data(), (int)item.size(), 0, hashes);
        
        for (int i = 0; i < numHashes_; ++i) {
            // Kirsch-Mitzenmacher optimization: Simulate 'k' hashes using only 2
            size_t bitPos = (hashes[0] + i * hashes[1]) % size_;
            bits_[bitPos / 64] |= (1ULL << (bitPos % 64));
        }
    }

    bool contains(const std::string& item) const {
        uint64_t hashes[2];
        MurmurHash3_x64_128(item.data(), (int)item.size(), 0, hashes);
        
        for (int i = 0; i < numHashes_; ++i) {
            size_t bitPos = (hashes[0] + i * hashes[1]) % size_;
            if (!(bits_[bitPos / 64] & (1ULL << (bitPos % 64)))) {
                return false; // Definitely not in the set
            }
        }
        return true; // Probably in the set
    }

    size_t bitCount() const { return size_; }
    int hashCount() const { return numHashes_; }

private:
    size_t size_;
    int numHashes_;
    std::vector<uint64_t> bits_;
};

} // namespace quantum
