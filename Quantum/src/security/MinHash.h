#pragma once

#include "security/BloomFilter.h" // For MurmurHash3
#include <vector>
#include <string>
#include <cstdint>
#include <algorithm>
#include <set>

namespace quantum {

class MinHash {
public:
    explicit MinHash(int numHashes = 128);
    
    std::vector<uint64_t> computeSketch(const std::string& axTree);
    float computeSimilarity(const std::vector<uint64_t>& a, const std::vector<uint64_t>& b);

private:
    int numHashes_;
    std::vector<uint32_t> hashSeeds_;
    
    std::vector<std::string> generateShingles(const std::string& text);
};

} // namespace quantum
