#include "security/MinHash.h"
#include <set>
#include <sstream>
#include <algorithm>
#include <random>

namespace quantum {

MinHash::MinHash(int numHashes) : numHashes_(numHashes) {
    // Generate fixed random seeds for K hash functions (to ensure consistent DNA across sessions)
    std::mt19937 gen(42);
    for (int i = 0; i < numHashes; ++i) {
        hashSeeds_.push_back(gen());
    }
}

std::vector<uint64_t> MinHash::computeSketch(const std::string& axTree) {
    auto shingles = generateShingles(axTree);
    std::vector<uint64_t> sketch(numHashes_, std::numeric_limits<uint64_t>::max());

    if (shingles.empty()) return sketch;

    for (const auto& shingle : shingles) {
        uint64_t sHash = std::hash<std::string>{}(shingle);
        for (int i = 0; i < numHashes_; ++i) {
            // Use Kirsch-Mitzenmacher like trick for multi-hash
            uint64_t combinedHash = sHash ^ (hashSeeds_[i] + 0x9e3779b9 + (sHash << 6) + (sHash >> 2));
            if (combinedHash < sketch[i]) {
                sketch[i] = combinedHash;
            }
        }
    }

    return sketch;
}

float MinHash::computeSimilarity(const std::vector<uint64_t>& a, const std::vector<uint64_t>& b) {
    if (a.size() != b.size() || a.empty()) return 0.0f;

    int matches = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i] == b[i]) matches++;
    }

    return static_cast<float>(matches) / a.size();
}

std::vector<std::string> MinHash::generateShingles(const std::string& text) {
    std::vector<std::string> shingles;
    // Simple 3-word shingling for AXTree structure detection
    std::stringstream ss(text);
    std::string word;
    std::vector<std::string> words;
    while (ss >> word) {
        words.push_back(word);
    }

    if (words.size() < 3) return words;

    for (size_t i = 0; i < words.size() - 2; ++i) {
        shingles.push_back(words[i] + " " + words[i+1] + " " + words[i+2]);
    }

    return shingles;
}

} // namespace quantum
